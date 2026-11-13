#include "RPGActionComponent.h"
#include "RPGPlayerCharacter.h"
#include "RPGCharacter.h"
#include "RPGPlayerState.h"
#include "RPGSkillComponent.h"
#include "RPGAttributeSet.h"
#include "RPGAbilitySystemComponent.h"
#include "RPGProjectile.h"
#include "DTAnimationRouter.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimInstance.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Controller.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"
#include "RPGSkillExecution.h"
#include "GameFramework/CharacterMovementComponent.h"

URPGActionComponent::URPGActionComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	SetComponentTickEnabled(false);
	SetIsReplicatedByDefault(true);
	MeleeTraceSockets = {FName("weapon_r"), FName("hand_r")};
}

void URPGActionComponent::BeginPlay()
{
	Super::BeginPlay();
	for (URPGAction* Action : Actions)
	{
		if (!IsValid(Action)) continue;
		Action->OwningComponent = this;
		Action->OnActionStateChanged.AddUniqueDynamic(this, &URPGActionComponent::ForwardActionChanged);
		Action->OnActionStopped.AddUniqueDynamic(this, &URPGActionComponent::ForwardActionStopped);
	}
}

void URPGActionComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(URPGActionComponent, CommittedAction);
}

FGameplayTag URPGActionComponent::CooldownTagForAbility(const FName AbilityId)
{
	FString Suffix = AbilityId.ToString();
	Suffix.RemoveFromStart(TEXT("Abilities."));
	if (Suffix.IsEmpty()) return FGameplayTag();
	return FGameplayTag::RequestGameplayTag(FName(TEXT("Cooldown.") + Suffix), false);
}

float URPGActionComponent::GetRemainingCooldown(const FName AbilityId) const
{
	ARPGCharacter* Source = Cast<ARPGCharacter>(GetOwner());
	const UAbilitySystemComponent* ASC = Source ? Source->GetAbilitySystemComponent() : nullptr;
	if (!ASC) return 0.f;
	// The applied cooldown effect is the authoritative cooldown state; this
	// handle path also covers abilities whose id has no registered tag.
	if (const FActiveGameplayEffectHandle* Handle = CooldownHandles.Find(AbilityId);
		Handle && Handle->IsValid())
		if (const FActiveGameplayEffect* Effect = ASC->GetActiveGameplayEffect(*Handle))
			return FMath::Max(0.f, Effect->GetTimeRemaining(ASC->GetWorld()->GetTimeSeconds()));
	const FGameplayTag Tag = CooldownTagForAbility(AbilityId);
	if (!Tag.IsValid()) return 0.f;
	FGameplayTagContainer Tags;
	Tags.AddTag(Tag);
	const TArray<float> Times = ASC->GetActiveEffectsTimeRemaining(
		FGameplayEffectQuery::MakeQuery_MatchAnyOwningTags(Tags));
	float Longest = 0.f;
	for (const float T : Times) Longest = FMath::Max(Longest, T);
	return Longest;
}

void URPGActionComponent::RegisterCooldownHandle(const FName AbilityId, const FActiveGameplayEffectHandle Handle)
{
	if (!AbilityId.IsNone() && Handle.IsValid()) CooldownHandles.Add(AbilityId, Handle);
}

bool URPGActionComponent::CanActivateAbility(const URPGCombatAbilityDefinition* Ability, const int32 Rank,
	ARPGCharacter* Target, FString& OutFailure, const bool bIgnoreCostAndCooldown, const bool bAllowClientPrediction)
{
	ARPGCharacter* Source = Cast<ARPGCharacter>(GetOwner());
	UAbilitySystemComponent* SourceASC = Source ? Source->GetAbilitySystemComponent() : nullptr;
	const URPGAttributeSet* SourceStats = Source ? Source->GetRPGAttributes() : nullptr;
	const TCHAR* Reject = nullptr;
	if (!Source) Reject = TEXT("no source character");
	else if (!Source->HasAuthority() && !bAllowClientPrediction) Reject = TEXT("not authority");
	else if (Source->bIsDead) Reject = TEXT("source dead");
	else if (Source->bIsStunned) Reject = TEXT("source controlled");
	else if (const ARPGPlayerCharacter* Player=Cast<ARPGPlayerCharacter>(Source); Player && Player->bIsDodging) Reject = TEXT("source dodging");
	else if (!Ability || !Ability->IsConfigured()) Reject = TEXT("ability not configured");
	else if (Rank < 1 || Rank > Ability->MaximumRank) Reject = TEXT("invalid rank");
	else if (!SourceASC || !SourceStats) Reject = TEXT("no ASC/attributes");
	else if (!bIgnoreCostAndCooldown && GetRemainingCooldown(Ability->AbilityId) > 0.f) Reject = TEXT("cooldown");
	else if (PendingRelease) Reject = TEXT("ability already in flight");
	if (Reject)
	{
		OutFailure = Reject;
		return false;
	}
	if (Ability->bRequiresTarget || Target)
	{
		const TCHAR* TargetReject = nullptr;
		if (!IsValid(Target) || Target == Source || Target->bIsDead || !Target->GetRPGAttributes()) TargetReject = TEXT("invalid target");
		else if (!Ability->bAllowFriendlyFire && !Source->FactionId.IsNone() && Source->FactionId == Target->FactionId) TargetReject = TEXT("friendly target");
		else if (FVector::DistSquared(Source->GetActorLocation(), Target->GetActorLocation()) > FMath::Square(Ability->Range)) TargetReject = TEXT("out of range");
		else
		{
			FHitResult Hit;
			FCollisionQueryParams Params(SCENE_QUERY_STAT(RPGCombatLOS), false, Source);
			Params.AddIgnoredActor(Source);
			if (GetWorld()->LineTraceSingleByChannel(Hit, Source->GetActorLocation() + FVector(0,0,50),
				Target->GetActorLocation() + FVector(0,0,50), ECC_Visibility, Params) && Hit.GetActor() != Target)
				TargetReject = TEXT("line of sight blocked");
		}
		if (TargetReject)
		{
			OutFailure = TargetReject;
			return false;
		}
	}
	if (!bIgnoreCostAndCooldown && (SourceStats->GetMana() < Ability->ManaCost || SourceStats->GetStamina() < Ability->StaminaCost))
	{
		OutFailure = TEXT("insufficient resources");
		return false;
	}
	const float BaseDamage = Ability->GetBaseDamage(Rank) + SourceStats->GetAttackPower() * Ability->AttackPowerScale +
		SourceStats->GetAbilityPower() * Ability->AbilityPowerScale;
	if (!FMath::IsFinite(BaseDamage) || BaseDamage < 0.f)
	{
		OutFailure = TEXT("non-finite damage");
		return false;
	}
	if (Ability->ActionClass)
	{
		URPGAction* Action = AbilityActions.FindRef(Ability->AbilityId);
		if (!Action || Action->GetClass() != Ability->ActionClass)
		{
			Action = NewObject<URPGAction>(this, Ability->ActionClass);
			Action->OwningComponent = this;
			Action->Actionname = Ability->AbilityId;
			Action->OnActionStateChanged.AddUniqueDynamic(this, &ThisClass::ForwardActionChanged);
			Action->OnActionStopped.AddUniqueDynamic(this, &ThisClass::ForwardActionStopped);
			AbilityActions.Add(Ability->AbilityId, Action);
		}
		Action->TargetActor = Target;
		Action->SkillLevel = Rank;
		if (!Action->CanStart(Source))
		{
			OutFailure = TEXT("action CanStart failed");
			return false;
		}
	}
	const bool bDeferred = Ability->ReleaseEventTag.IsValid() || Ability->ProjectileClass || Ability->Kind == ERPGAbilityKind::Melee;
	if (bDeferred && Source->GetMesh() && Source->GetMesh()->GetSkeletalMeshAsset() && GetNetMode() != NM_DedicatedServer)
	{
		FDTCombatAnimation Animation;
		if (!Source->AnimationRouter || !Source->AnimationRouter->FindCombatAnimation(Ability->AbilityId, Animation) ||
			!Source->AnimationRouter->CanPlayCombatAnimation(Animation))
		{
			OutFailure = TEXT("combat montage unavailable or invalid");
			return false;
		}
	}
	OutFailure.Reset();
	return true;
}

bool URPGActionComponent::TryActivateAbility(URPGCombatAbilityDefinition* Ability, const int32 Rank, ARPGCharacter* Target)
{
	ARPGCharacter* Source = Cast<ARPGCharacter>(GetOwner());
	URPGAbilitySystemComponent* ASC = Source
		? Cast<URPGAbilitySystemComponent>(Source->GetAbilitySystemComponent()) : nullptr;
	if (!Source || !Source->HasAuthority() || !ASC)
	{
		UE_LOG(LogRPGCombat, Log, TEXT("[GAS] %s rejected: no authoritative RPG ASC"), *GetNameSafe(Ability));
		return false;
	}
	return ASC->ActivateAbilityDefinition(Ability, Rank, Target);
}

bool URPGActionComponent::TryActivateAbilityFromGameplayAbility(URPGCombatAbilityDefinition* Ability, const int32 Rank,
	ARPGCharacter* Target)
{
	return TryActivateAbilityInternal(Ability, Rank, Target);
}

bool URPGActionComponent::TryActivateAbilityInternal(URPGCombatAbilityDefinition* Ability, const int32 Rank,
	ARPGCharacter* Target)
{
	ARPGCharacter* Source = Cast<ARPGCharacter>(GetOwner());
	URPGAttributeSet* SourceStats = Source ? Source->GetRPGAttributes() : nullptr;
	FString Failure;
	if (!CanActivateAbility(Ability, Rank, Target, Failure, true))
	{
		UE_LOG(LogRPGCombat, Log, TEXT("[GAS] %s rejected: %s"), *GetNameSafe(Ability), *Failure);
		return false;
	}
	URPGAction* Action = nullptr;
	if (Ability->ActionClass)
	{
		Action = AbilityActions.FindRef(Ability->AbilityId);
		if (!ensure(Action)) return false;
	}
	const float BaseDamage = Ability->GetBaseDamage(Rank) + SourceStats->GetAttackPower() * Ability->AttackPowerScale +
		SourceStats->GetAbilityPower() * Ability->AbilityPowerScale;
	// Deferred release path: authored montage event / melee window drives damage.
	const bool bDeferred = Ability->ReleaseEventTag.IsValid() || Ability->ProjectileClass ||
		Ability->Kind == ERPGAbilityKind::Melee;
	if (!bDeferred && Target && BaseDamage > 0.f)
	{
		// Instant hits are not part of a shared melee window; they must not join
		// PendingHitActors or every repeat activation against that target would
		// be rejected as a phantom duplicate.
		bool bCrit = false, bBlocked = false;
		const float Damage = ComputeMitigatedDamage(Source, Target, Ability, BaseDamage, &bCrit, &bBlocked);
		if (!FMath::IsFinite(Damage) || Damage <= 0.f || !Target->ReceiveCombatDamage(Damage, Source, Ability->AbilityId, bCrit, bBlocked, Ability->DamageEffect)) return false;
	}

	CommittedAction.AbilityId = Ability->AbilityId;
	CommittedAction.Definition = Ability;
	CommittedAction.Target = Target;
	CommittedAction.ServerStartTime = GetWorld()->GetTimeSeconds();
	++CommittedAction.Sequence;
	if (Action) Action->StartAction(Source);
	if (bDeferred) BeginPendingRelease(Ability, Rank, Target, BaseDamage);
	else if (Ability->Execution != ERPGSkillExecution::Damage)
		URPGSkillExecution::Execute(Source, Ability, Target, BaseDamage, Rank);
	OnAbilityCommitted.Broadcast(Ability, Target);
	if (!bDeferred) BroadcastAbilityExecutionFinished(Ability->AbilityId, false);
	GetOwner()->ForceNetUpdate();
	UE_LOG(LogRPGCombat, Log, TEXT("[GAS] %s activated rank=%d target=%s deferred=%d"),
	       *Ability->AbilityId.ToString(), Rank, *GetNameSafe(Target), bDeferred ? 1 : 0);
	return true;
}

void URPGActionComponent::BeginPendingRelease(const URPGCombatAbilityDefinition* Ability, const int32 Rank,
                                              ARPGCharacter* Target, const float BaseDamage)
{
	PendingRelease.Definition = Ability;
	PendingRelease.Rank = Rank;
	PendingRelease.Target = Target;
	PendingRelease.BaseDamage = BaseDamage;
	PendingRelease.bReleased = false;
	PendingRelease.bWindowHitApplied = false;
	PendingHitActors.Reset();

	ARPGCharacter* Source = Cast<ARPGCharacter>(GetOwner());
	const UAnimMontage* Montage = Source && Source->AnimationRouter
		? Source->AnimationRouter->FindCombatMontage(Ability->AbilityId) : nullptr;
	PendingRelease.Montage = Montage;
	float PlayRate = 1.f;
	if (Montage && Source->AnimationRouter)
	{
		FDTCombatAnimation Entry;
		if (Source->AnimationRouter->FindCombatAnimation(Ability->AbilityId, Entry) && Entry.PlayRate > KINDA_SMALL_NUMBER)
			PlayRate = Entry.PlayRate;
	}
	if (!Montage)
	{
		// No montage authored: release immediately so incomplete content stays usable.
		UE_LOG(LogRPGCombat, Warning, TEXT("[Animation] %s has no mapped montage; releasing immediately"), *Ability->AbilityId.ToString());
		ExecutePendingRelease();
		return;
	}
	ScheduleReleaseTiming(Ability, Montage, PlayRate);
}

void URPGActionComponent::ScheduleReleaseTiming(const URPGCombatAbilityDefinition* Ability, const UAnimMontage* Montage,
                                                const float PlayRate)
{
	FTimerManager& Timers = GetWorld()->GetTimerManager();
	bool bScheduled = false;
	const float Rate = FMath::Max(KINDA_SMALL_NUMBER, PlayRate);
	const ARPGCharacter* Source = Cast<ARPGCharacter>(GetOwner());
	if (GetNetMode() != NM_DedicatedServer && Source && Source->GetMesh() && Source->GetMesh()->GetSkeletalMeshAsset())
	{
		// Rendered authority must receive the actual montage notify. A timer used
		// to spawn projectiles even when Montage_Play had failed completely.
		UE_LOG(LogRPGCombat, Log, TEXT("[Animation] %s awaiting authored montage notify"), *Ability->AbilityId.ToString());
		return;
	}

	// Authoritative copy of the authored release event; the real notify also reaches
	// ExecutePendingRelease on listening hosts and is deduplicated by bReleased.
	if (Ability->ReleaseEventTag.IsValid())
	{
		const float EventTime = UDTAnimationRouter::FindNotifyTriggerTime(Montage, Ability->ReleaseEventTag);
		if (EventTime >= 0.f)
		{
			Timers.SetTimer(ReleaseTimer, this, &URPGActionComponent::ExecutePendingRelease,
			                EventTime / Rate, false);
			bScheduled = true;
		}
	}

	// Authoritative melee window from the authored notify-state spans.
	TArray<FVector2D> Windows;
	UDTAnimationRouter::FindMeleeWindows(Montage, Windows);
	for (const FVector2D& Window : Windows)
	{
		FTimerHandle Open, Close;
		Timers.SetTimer(Open, FTimerDelegate::CreateUObject(this, &URPGActionComponent::SetMeleeWindowOpen, true),
		                Window.X / Rate, false);
		Timers.SetTimer(Close, FTimerDelegate::CreateUObject(this, &URPGActionComponent::SetMeleeWindowOpen, false),
		                Window.Y / Rate, false);
		WindowTimers.Add(Open);
		WindowTimers.Add(Close);
		bScheduled = true;
	}
	// A committed melee target also releases at the first window's midpoint.
	// The swept trace may have already applied this target (deduplicated by
	// PendingHitActors); the release covers meshless/sweepless authority.
	if (Ability->Kind == ERPGAbilityKind::Melee && Windows.Num() > 0 && PendingRelease.Target.IsValid())
	{
		Timers.SetTimer(ReleaseTimer, this, &URPGActionComponent::ExecutePendingRelease,
		                (Windows[0].X + (Windows[0].Y - Windows[0].X) * 0.5f) / Rate, false);
	}

	if (!bScheduled)
	{
		// Montage exists but carries no matching notify: backstop release keeps the
		// committed ability deterministic while flagging missing content.
		UE_LOG(LogRPGCombat, Warning, TEXT("[Animation] %s montage %s has no matching release/window notify; using fallback timing"),
		       *PendingRelease.Definition->AbilityId.ToString(), *Montage->GetName());
		Timers.SetTimer(ReleaseTimer, this, &URPGActionComponent::ExecutePendingRelease,
		                Montage->GetPlayLength() * FallbackReleaseFraction / Rate, false);
	}
}

void URPGActionComponent::ExecutePendingRelease()
{
	if (!PendingRelease || PendingRelease.bReleased) return;
	PendingRelease.bReleased = true;
	ARPGCharacter* Source = Cast<ARPGCharacter>(GetOwner());
	const URPGCombatAbilityDefinition* Ability = PendingRelease.Definition;
	if (!IsValid(Source) || Source->bIsDead)
	{
		CancelPendingRelease(TEXT("source invalid at release"));
		return;
	}
	UE_LOG(LogRPGCombat, Log, TEXT("[GAS] Release event received ability=%s"), *Ability->AbilityId.ToString());
	if (Ability->ProjectileClass)
	{
		SpawnProjectile(Ability, Source, PendingRelease.Target.Get(), PendingRelease.BaseDamage, PendingRelease.Rank);
	}
	else
	{
		if (Ability->Execution == ERPGSkillExecution::Damage)
		{
			if (ARPGCharacter* Target = PendingRelease.Target.Get()) ApplyAbilityDamage(Target, Ability, PendingRelease.BaseDamage, TEXT("release"));
		}
		else URPGSkillExecution::Execute(Source, Ability, PendingRelease.Target.Get(), PendingRelease.BaseDamage, PendingRelease.Rank);
	}
	// Keep pending state alive while a melee window is still scheduled/open; the
	// montage-finished callback performs the final cleanup.
	const bool bWindowActive = bMeleeWindowOpen || WindowTimers.ContainsByPredicate(
		[this](const FTimerHandle& H) { return GetWorld()->GetTimerManager().IsTimerActive(H); });
	if (!bWindowActive) ClearPendingState();
}

bool URPGActionComponent::ApplyAbilityDamage(ARPGCharacter* Target, const URPGCombatAbilityDefinition* Ability,
                                             const float BaseDamage, const FName SourceLabel)
{
	ARPGCharacter* Source = Cast<ARPGCharacter>(GetOwner());
	if (!IsValid(Source) || !IsValid(Target) || BaseDamage <= 0.f) return false;
	if (PendingHitActors.Contains(Target)) return false;
	bool bCrit = false, bBlocked = false;
	const float Damage = ComputeMitigatedDamage(Source, Target, Ability, BaseDamage, &bCrit, &bBlocked);
	if (!FMath::IsFinite(Damage) || Damage <= 0.f) return false;
	if (!Target->ReceiveCombatDamage(Damage, Source, Ability ? Ability->AbilityId : NAME_None, bCrit, bBlocked,
		Ability ? Ability->DamageEffect.Get() : nullptr)) return false;
	PendingHitActors.Add(Target);
	UE_LOG(LogRPGCombat, Log, TEXT("[GAS] Damage Gameplay Effect applied target=%s damage=%.1f source=%s"),
	       *Target->GetName(), Damage, *SourceLabel.ToString());
	return true;
}

float URPGActionComponent::ComputeMitigatedDamage(ARPGCharacter* Source, ARPGCharacter* Target,
                                                  const URPGCombatAbilityDefinition* Ability, const float BaseDamage,
                                                  bool* bOutCriticalHit, bool* bOutBlockedHit)
{
	if (bOutCriticalHit) *bOutCriticalHit = false;
	if (bOutBlockedHit) *bOutBlockedHit = false;
	const URPGAttributeSet* SourceStats = Source ? Source->GetRPGAttributes() : nullptr;
	const URPGAttributeSet* TargetStats = Target ? Target->GetRPGAttributes() : nullptr;
	if (!TargetStats || BaseDamage <= 0.f) return 0.f;
	float Damage = BaseDamage;
	if (Ability && Ability->Kind == ERPGAbilityKind::Spell)
	{
		// Spells are resisted by Resilience and pierced by MagicPenetration.
		const float EffectiveResist = FMath::Max(0.f, TargetStats->GetResilience() - (SourceStats ? SourceStats->GetMagicPenetration() : 0.f));
		Damage *= 100.f / (100.f + EffectiveResist);
		const float CritChance = SourceStats
			? FMath::Clamp(SourceStats->GetMagicCriticalHitChance() - TargetStats->GetMagicCriticalHitResistance(), 0.f, 100.f) : 0.f;
		if (CritChance > 0.f && FMath::FRandRange(0.f, 100.f) < CritChance)
		{
			Damage *= FMath::Max(1.f, SourceStats->GetMagicCriticalHitDamage());
			if (bOutCriticalHit) *bOutCriticalHit = true;
		}
	}
	else
	{
		// Physical hits are reduced by armor and glance off blocked strikes.
		const float EffectiveArmor = FMath::Max(0.f, TargetStats->GetArmor() - (SourceStats ? SourceStats->GetArmorPenetration() : 0.f));
		Damage *= 100.f / (100.f + EffectiveArmor);
		if (FMath::FRandRange(0.f, 100.f) < FMath::Clamp(TargetStats->GetBlockChance(), 0.f, 100.f))
		{
			Damage *= .5f;
			if (bOutBlockedHit) *bOutBlockedHit = true;
		}
		const float CritChance = SourceStats
			? FMath::Clamp(SourceStats->GetCriticalHitChance() - .5f * TargetStats->GetResilience(), 0.f, 100.f) : 0.f;
		if (CritChance > 0.f && FMath::FRandRange(0.f, 100.f) < CritChance)
		{
			Damage *= FMath::Max(1.f, SourceStats->GetCriticalHitDamage());
			if (bOutCriticalHit) *bOutCriticalHit = true;
		}
	}
	return FMath::IsFinite(Damage) ? FMath::Max(0.f, Damage) : 0.f;
}

void URPGActionComponent::SpawnProjectile(const URPGCombatAbilityDefinition* Ability, ARPGCharacter* Source,
                                          ARPGCharacter* Target, const float BaseDamage, const int32 Rank)
{
	USkeletalMeshComponent* Mesh = Source->GetMesh();
	FTransform SpawnTransform = Source->GetActorTransform();
	if (Mesh && !Ability->ReleaseSocket.IsNone() && Mesh->DoesSocketExist(Ability->ReleaseSocket))
		SpawnTransform = Mesh->GetSocketTransform(Ability->ReleaseSocket);
	else
		SpawnTransform.SetLocation(Source->GetActorLocation() + Source->GetActorForwardVector() * 50.f + FVector(0,0,60.f));

	FRotator Aim = SpawnTransform.Rotator();
	if (IsValid(Target))
		Aim = (Target->GetActorLocation() + FVector(0,0,40.f) - SpawnTransform.GetLocation()).Rotation();
	else if (const AController* Controller = Source->GetController())
		Aim = Controller->GetControlRotation();
	SpawnTransform.SetRotation(Aim.Quaternion());

	FActorSpawnParameters Params;
	Params.Owner = Source;
	Params.Instigator = Source;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ARPGProjectile* Projectile = GetWorld()->SpawnActor<ARPGProjectile>(Ability->ProjectileClass, SpawnTransform, Params);
	if (!Projectile)
	{
		UE_LOG(LogRPGCombat, Warning, TEXT("[Combat] Projectile spawn failed ability=%s"), *Ability->AbilityId.ToString());
		return;
	}
	Projectile->InitializeProjectile(Source, Ability, BaseDamage, Rank);
}

void URPGActionComponent::HandleAnimationEvent(const FGameplayTag EventTag)
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || !PendingRelease || !EventTag.IsValid()) return;
	UE_LOG(LogRPGCombat, Log, TEXT("[Animation] Release notify reached tag=%s"), *EventTag.ToString());
	if (EventTag == PendingRelease.Definition->ReleaseEventTag) ExecutePendingRelease();
}

void URPGActionComponent::SetMeleeWindowOpen(const bool bOpen)
{
	if (!GetOwner() || !GetOwner()->HasAuthority()) return;
	if (bOpen)
	{
		if (!PendingRelease || PendingRelease.Definition->Kind != ERPGAbilityKind::Melee) return;
		if (bMeleeWindowOpen) return;
		bMeleeWindowOpen = true;
		if (const ARPGCharacter* Source = Cast<ARPGCharacter>(GetOwner()))
			if (GetNetMode() != NM_DedicatedServer && Source->GetMesh() && Source->GetMesh()->GetSkeletalMeshAsset())
				PendingRelease.bReleased = true;
		PrevSocketPositions.Reset();
		if (ARPGCharacter* Source = Cast<ARPGCharacter>(GetOwner())) Source->SetWeaponCollisionEnabled(true);
		SetComponentTickEnabled(true);
		UE_LOG(LogRPGCombat, Log, TEXT("[Combat] Melee notify-window start"));
		return;
	}
	if (!bMeleeWindowOpen) return;
	bMeleeWindowOpen = false;
	if (ARPGCharacter* Source = Cast<ARPGCharacter>(GetOwner())) Source->SetWeaponCollisionEnabled(false);
	PrevSocketPositions.Reset();
	SetComponentTickEnabled(false);
	UE_LOG(LogRPGCombat, Log, TEXT("[Combat] Melee notify-window end"));
	// Dedicated servers never tick montages, so the closing of the last authored
	// window is also the final cleanup point for a released melee ability.
	const bool bWindowStillPending = WindowTimers.ContainsByPredicate(
		[this](const FTimerHandle& H) { return GetWorld()->GetTimerManager().IsTimerActive(H); });
	const ARPGCharacter* Source = Cast<ARPGCharacter>(GetOwner());
	const bool bUsesAuthorityTimers = GetNetMode() == NM_DedicatedServer || !Source ||
		!Source->GetMesh() || !Source->GetMesh()->GetSkeletalMeshAsset();
	if (bUsesAuthorityTimers && PendingRelease && PendingRelease.bReleased && !bWindowStillPending) ClearPendingState();
}

void URPGActionComponent::HandleWeaponCollision(AActor* OtherActor)
{
	if (!bMeleeWindowOpen || !GetOwner() || !GetOwner()->HasAuthority() || !PendingRelease) return;
	ARPGCharacter* Source = Cast<ARPGCharacter>(GetOwner());
	ARPGCharacter* Target = Cast<ARPGCharacter>(OtherActor);
	if (!IsValidMeleeTarget(Source, Target, PendingRelease.Definition) || PendingHitActors.Contains(Target)) return;
	UE_LOG(LogRPGCombat, Verbose, TEXT("[Combat] Equipped weapon overlap target=%s"), *Target->GetName());
	ApplyAbilityDamage(Target, PendingRelease.Definition, PendingRelease.BaseDamage, TEXT("weapon overlap"));
}

bool URPGActionComponent::IsValidMeleeTarget(ARPGCharacter* Source, ARPGCharacter* Target,
                                             const URPGCombatAbilityDefinition* Ability) const
{
	if (!IsValid(Source) || !IsValid(Target) || Target == Source || Target->bIsDead) return false;
	if (!Ability->bAllowFriendlyFire && !Source->FactionId.IsNone() && Source->FactionId == Target->FactionId) return false;
	return true;
}

bool URPGActionComponent::GetTimeUntilMeleeThreat(const ARPGCharacter* Target,float& Seconds) const
{
	Seconds=-1.f;
	ARPGCharacter* Source=Cast<ARPGCharacter>(GetOwner());
	if (!Source || Source->bIsDead || !IsValid(Target) || Target==Source || Target->bIsDead || !PendingRelease ||
		!PendingRelease.Montage || PendingRelease.Definition->Kind!=ERPGAbilityKind::Melee ||
		(!PendingRelease.Definition->bAllowFriendlyFire && !Source->FactionId.IsNone() && Source->FactionId==Target->FactionId)) return false;
	const FVector Offset=Target->GetActorLocation()-Source->GetActorLocation();
	if (Offset.SizeSquared()>FMath::Square(PendingRelease.Definition->Range+50.f) ||
		FVector::DotProduct(Source->GetActorForwardVector(),Offset.GetSafeNormal())<.35f) return false;
	float Rate=1.f;
	FDTCombatAnimation Entry;
	if (Source->AnimationRouter && Source->AnimationRouter->FindCombatAnimation(PendingRelease.Definition->AbilityId,Entry)) Rate=FMath::Max(.01f,Entry.PlayRate);
	float Position=(GetWorld()->GetTimeSeconds()-CommittedAction.ServerStartTime)*Rate;
	if (UAnimInstance* Anim=Source->GetMesh()->GetAnimInstance())
		if (Anim->Montage_IsActive(PendingRelease.Montage)) Position=Anim->Montage_GetPosition(PendingRelease.Montage);
	bool bAlreadyHit=false;
	for (const TWeakObjectPtr<AActor>& Hit:PendingHitActors) if (Hit.Get()==Target) { bAlreadyHit=true; break; }
	TArray<FVector2D> Windows; UDTAnimationRouter::FindMeleeWindows(PendingRelease.Montage,Windows);
	for (const FVector2D& Window:Windows)
		if (Position<Window.Y && !bAlreadyHit)
		{
			const float Time=FMath::Max(.01f,(static_cast<float>(Window.X)-Position)/Rate);
			if (Seconds<0 || Time<Seconds) Seconds=Time;
		}
	return Seconds>=0;
}

void URPGActionComponent::TickComponent(const float DeltaTime, const ELevelTick TickType,
                                        FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (!bMeleeWindowOpen || !GetOwner() || !GetOwner()->HasAuthority()) return;
	ARPGCharacter* Source = Cast<ARPGCharacter>(GetOwner());
	if (!IsValid(Source) || Source->bIsDead)
	{
		SetMeleeWindowOpen(false);
		return;
	}
	SweepMelee(Source);
}

void URPGActionComponent::SweepMelee(ARPGCharacter* Source)
{
	USkeletalMeshComponent* Mesh = Source->GetMesh();
	if (!Mesh || !PendingRelease) { SetMeleeWindowOpen(false); return; }

	TArray<FVector> Current;
	for (const FName Socket : MeleeTraceSockets)
		if (Mesh->DoesSocketExist(Socket)) Current.Add(Mesh->GetSocketLocation(Socket));
	if (Current.IsEmpty())
	{
		UE_LOG(LogRPGCombat, Warning, TEXT("[Combat] Melee window has no valid trace sockets on %s"), *Mesh->GetName());
		SetMeleeWindowOpen(false);
		return;
	}
	// Include the blade segment between the first two sockets so the whole weapon sweeps.
	if (Current.Num() >= 2) Current.Add((Current[0] + Current[1]) * 0.5f);

	TArray<FVector> Previous;
	for (FVector& Pos : Current)
	{
		FVector& Prev = PrevSocketPositions.FindOrAdd(FName(*FString::Printf(TEXT("p%d"), Previous.Num())), Pos);
		Previous.Add(Prev);
		Prev = Pos;
	}

	FCollisionQueryParams Params(SCENE_QUERY_STAT(RPGMeleeSweep), false, Source);
	Params.AddIgnoredActor(Source);
	const FCollisionShape Shape = FCollisionShape::MakeSphere(MeleeTraceRadius);
	TArray<FHitResult> Hits;
	for (int32 Index = 0; Index < Current.Num(); ++Index)
	{
		if (Previous[Index].Equals(Current[Index], 1.f)) continue;
		TArray<FHitResult> SegmentHits;
		if (GetWorld()->SweepMultiByChannel(SegmentHits, Previous[Index], Current[Index], FQuat::Identity, ECC_Pawn, Shape, Params))
			Hits.Append(SegmentHits);
	}

	for (const FHitResult& Hit : Hits)
	{
		ARPGCharacter* Target = Cast<ARPGCharacter>(Hit.GetActor());
		if (!Target || PendingHitActors.Contains(Target)) continue;
		if (!IsValidMeleeTarget(Source, Target, PendingRelease.Definition)) continue;
		UE_LOG(LogRPGCombat, Verbose, TEXT("[Combat] Melee collision detected target=%s"), *Target->GetName());
		ApplyAbilityDamage(Target, PendingRelease.Definition, PendingRelease.BaseDamage, TEXT("melee"));
	}
}

void URPGActionComponent::NotifyMontageFinished(const UAnimMontage* Montage, const bool bInterrupted)
{
	if (!PendingRelease || PendingRelease.Montage != Montage) return;
	if (bInterrupted || !PendingRelease.bReleased)
	{
		CancelPendingRelease(bInterrupted ? TEXT("montage interrupted") : TEXT("montage ended without release"));
		return;
	}
	ClearPendingState();
}

void URPGActionComponent::CancelPendingRelease(const FName Reason)
{
	if (!PendingRelease && !bMeleeWindowOpen) return;
	UE_LOG(LogRPGCombat, Log, TEXT("[GAS] Ability ended/cancelled reason=%s ability=%s"),
	       *Reason.ToString(), PendingRelease ? *PendingRelease.Definition->AbilityId.ToString() : TEXT("none"));
	ClearPendingState(true);
}

void URPGActionComponent::ResetAfterRespawn()
{
	CancelPendingRelease(TEXT("respawn"));
	GetWorld()->GetTimerManager().ClearTimer(ControlLockTimer);
	if (bControlLocked) RestoreControl();
	if (ARPGCharacter* Character = Cast<ARPGCharacter>(GetOwner())) Character->bIsStunned = false;
}

void URPGActionComponent::ClearPendingState(const bool bCancelled)
{
	const FName FinishedAbility = PendingRelease ? PendingRelease.Definition->AbilityId : NAME_None;
	PendingRelease = FPendingCombatRelease();
	PendingHitActors.Reset();
	if (GetWorld())
	{
		FTimerManager& Timers = GetWorld()->GetTimerManager();
		Timers.ClearTimer(ReleaseTimer);
		for (FTimerHandle& Handle : WindowTimers) Timers.ClearTimer(Handle);
	}
	WindowTimers.Reset();
	if (bMeleeWindowOpen)
	{
		bMeleeWindowOpen = false;
		if (ARPGCharacter* Source = Cast<ARPGCharacter>(GetOwner())) Source->SetWeaponCollisionEnabled(false);
		PrevSocketPositions.Reset();
		SetComponentTickEnabled(false);
	}
	if (!FinishedAbility.IsNone()) BroadcastAbilityExecutionFinished(FinishedAbility, bCancelled);
}

void URPGActionComponent::BroadcastAbilityExecutionFinished(const FName AbilityId, const bool bCancelled)
{
	if (AbilityId.IsNone()) return;
	// Preserve the native hook for non-GAS observers, but drive Gameplay Ability
	// lifetime through an ability task waiting on this gameplay event.
	OnAbilityExecutionFinishedNative.Broadcast(AbilityId, bCancelled);
	FGameplayEventData Payload;
	Payload.EventTag = FGameplayTag::RequestGameplayTag(TEXT("Event.Ability.ExecutionFinished"), false);
	Payload.Instigator = GetOwner();
	Payload.Target = GetOwner();
	Payload.EventMagnitude = bCancelled ? 1.f : 0.f;
	if (Payload.EventTag.IsValid())
		UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(GetOwner(), Payload.EventTag, Payload);
}

void URPGActionComponent::ApplyControlLock(const float Duration)
{
	ARPGCharacter* Character = Cast<ARPGCharacter>(GetOwner());
	if (!Character || !Character->HasAuthority() || Character->bIsDead) return;
	if (!bControlLocked) PreLockWalkSpeed = Character->GetCharacterMovement()->MaxWalkSpeed;
	bControlLocked = true;
	Character->bIsStunned = true;
	if (UAbilitySystemComponent* ASC = Character->GetAbilitySystemComponent())
	{
		ASC->SetLooseGameplayTagCount(FGameplayTag::RequestGameplayTag(TEXT("State.Stunned"), false), 1);
		ASC->CancelAllAbilities();
	}
	Character->GetCharacterMovement()->StopMovementImmediately();
	Character->GetCharacterMovement()->MaxWalkSpeed = 0.f;
	CancelPendingRelease(TEXT("control effect"));
	const float Remaining = GetWorld()->GetTimerManager().GetTimerRemaining(ControlLockTimer);
	GetWorld()->GetTimerManager().SetTimer(ControlLockTimer, this, &ThisClass::RestoreControl, FMath::Max(Duration, Remaining), false);
}

void URPGActionComponent::RestoreControl()
{
	if (ARPGCharacter* Character = Cast<ARPGCharacter>(GetOwner()))
	{
		Character->bIsStunned = false;
		if (UAbilitySystemComponent* ASC = Character->GetAbilitySystemComponent())
			ASC->SetLooseGameplayTagCount(FGameplayTag::RequestGameplayTag(TEXT("State.Stunned"), false), 0);
		Character->GetCharacterMovement()->MaxWalkSpeed = PreLockWalkSpeed;
	}
	bControlLocked = false;
}

void URPGActionComponent::OnRep_CommittedAction()
{
	OnAbilityCommitted.Broadcast(CommittedAction.Definition, CommittedAction.Target);
}

void URPGActionComponent::ForwardActionChanged(URPGAction* Action) { OnActionStateChanged.Broadcast(Action); }
void URPGActionComponent::ForwardActionStopped(URPGAction* Action) { OnActionStopped.Broadcast(Action); }
