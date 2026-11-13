#include "RPGCharacter.h"

#include "Equipment_ItemObject.h"
#include "InventoryComponent.h"
#include "RPGActionComponent.h"
#include "DTAnimationRouter.h"
#include "AttributeComponent.h"
#include "RPGCraftingComponent.h"
#include "RPGQuest.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "RPGAbilitySystemComponent.h"
#include "RPGAttributeSet.h"
#include "RPGPlayerState.h"
#include "RPGPickupItem.h"
#include "RPGVendor.h"
#include "RPGAICharacter.h"
#include "GameplayEffectAggregator.h"
#include "Net/UnrealNetwork.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/StaticMeshComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "RPGSkillExecution.h"
#include "RPGSkillComponent.h"
#include "RPGGameModeBase.h"
#include "RPGPlayerController.h"
#include "RPGPlayerCharacter.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "NiagaraComponent.h"
#include "RPG_DamageTextComponent.h"

ARPGCharacter::ARPGCharacter()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	bReplicateUsingRegisteredSubObjectList = true;
	FallbackAbilitySystem = CreateDefaultSubobject<URPGAbilitySystemComponent>(TEXT("FallbackAbilitySystem"));
	FallbackAbilitySystem->SetIsReplicated(true);
	// AI/NPC pawns have no owning client, so Minimal or Mixed would leave enemy
	// attributes and cooldown tags invisible to every remote player. Full sends
	// the fallback component's effect state to all connections.
	FallbackAbilitySystem->SetReplicationMode(EGameplayEffectReplicationMode::Full);
	FallbackAttributes = CreateDefaultSubobject<URPGAttributeSet>(TEXT("FallbackAttributes"));

	InventoryComp = CreateDefaultSubobject<UInventoryComponent>(TEXT("InventoryComp"));
	AttributeComp = CreateDefaultSubobject<URPGAttributeComponent>(TEXT("AttributeComp"));
	ActionComp = CreateDefaultSubobject<URPGActionComponent>(TEXT("ActionComp"));
	AnimationRouter = CreateDefaultSubobject<UDTAnimationRouter>(TEXT("DT_AnimationRouter"));
	CraftingComp = CreateDefaultSubobject<URPGCraftingComponent>(TEXT("CraftingComp"));
	QuestComp = CreateDefaultSubobject<URPGQuestComponent>(TEXT("QuestComp"));
	static ConstructorHelpers::FClassFinder<ARPGPickupItem> MasterPickup(
		TEXT("/Game/Blueprints/Actors/Interactables/Items/BP_MasterPickupItem"));
	PickupClass = MasterPickup.Succeeded() ? TSubclassOf<ARPGPickupItem>(MasterPickup.Class)
		: TSubclassOf<ARPGPickupItem>(ARPGPickupItem::StaticClass());

	FrontCapture = CreateDefaultSubobject<USceneCaptureComponent2D>(TEXT("FrontCapture"));
	FrontCapture->SetupAttachment(GetMesh());
	FrontCapture->bCaptureEveryFrame = false;
	FrontCapture->bCaptureOnMovement = false;

	FaceCapture = CreateDefaultSubobject<USceneCaptureComponent2D>(TEXT("FaceCapture"));
	FaceCapture->SetupAttachment(GetMesh());
	FaceCapture->bCaptureEveryFrame = false;
	FaceCapture->bCaptureOnMovement = false;
	MinimapCapture = CreateDefaultSubobject<USceneCaptureComponent2D>(TEXT("DT_MinimapCapture"));
	MinimapCapture->SetupAttachment(GetRootComponent());
	MinimapCapture->bCaptureEveryFrame = false;
	MinimapCapture->bCaptureOnMovement = false;
}

void ARPGCharacter::InitializePlayerCaptures()
{
	if (!IsLocallyControlled() || GetNetMode() == NM_DedicatedServer) return;
	if (!PlayerPreviewTarget) PlayerPreviewTarget = LoadObject<UTextureRenderTarget2D>(nullptr, TEXT("/Game/UI/2D/Textures/RT_PlayerPreview.RT_PlayerPreview"));
	if (!FacePreviewTarget) FacePreviewTarget = LoadObject<UTextureRenderTarget2D>(nullptr, TEXT("/Game/UI/2D/Textures/RT_Face.RT_Face"));
	if (!MinimapTarget) MinimapTarget = LoadObject<UTextureRenderTarget2D>(nullptr, TEXT("/Game/UI/2D/Textures/RT_Minimap.RT_Minimap"));
	FrontCapture->AttachToComponent(GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);
	FrontCapture->SetRelativeLocation(PreviewCameraOffset);
	FrontCapture->SetRelativeRotation((-PreviewCameraOffset).Rotation());
	FrontCapture->TextureTarget = PlayerPreviewTarget;
	FrontCapture->PrimitiveRenderMode = ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList;
	FrontCapture->ShowOnlyActorComponents(this, true);
	FaceCapture->AttachToComponent(GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);
	FaceCapture->SetRelativeLocation(FaceCameraOffset);
	FaceCapture->SetRelativeRotation(FRotator(0.f, 180.f, 0.f));
	FaceCapture->TextureTarget = FacePreviewTarget;
	FaceCapture->PrimitiveRenderMode = ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList;
	FaceCapture->ShowOnlyActorComponents(this, true);
	MinimapCapture->SetRelativeLocation(FVector(0.f, 0.f, MinimapHeight));
	MinimapCapture->SetUsingAbsoluteRotation(true);
	MinimapCapture->SetWorldRotation(FRotator(-90.f, 0.f, 0.f));
	MinimapCapture->ProjectionType = ECameraProjectionMode::Orthographic;
	MinimapCapture->OrthoWidth = MinimapWidth;
	MinimapCapture->TextureTarget = MinimapTarget;
	MinimapCapture->bCaptureEveryFrame = bEnableMinimap && MinimapTarget;
	MinimapCapture->SetVisibility(bEnableMinimap);
}

void ARPGCharacter::Initialize(UObject* PC)
{
	InitializeAbilitySystem();
}

void ARPGCharacter::BeginPlay()
{
	Super::BeginPlay();
	InventoryComp->OnInventoryUpdated.AddUniqueDynamic(this, &ARPGCharacter::OnRep_Equipment);
	InitializeAbilitySystem();
	InitializeStartingEquipment();
}

void ARPGCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);
	InitializeAbilitySystem();
}

void ARPGCharacter::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();
	InitializeAbilitySystem();
}

void ARPGCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ARPGCharacter, ReplicatedEquipment);
	DOREPLIFETIME(ARPGCharacter, FactionId);
	DOREPLIFETIME(ARPGCharacter, bIsDead);
}

bool ARPGCharacter::ReceiveCombatDamage(const float Damage, AActor* DamageCauser, const FName AbilityId,
                                        const bool bCriticalHit, const bool bBlockedHit,
                                        const TSubclassOf<UGameplayEffect> DamageEffectOverride)
{
	UAbilitySystemComponent* ASC = GetAbilitySystemComponent();
	URPGAttributeSet* Stats = GetRPGAttributes();
	if (!HasAuthority() || bIsDead || !ASC || !Stats || !FMath::IsFinite(Damage) || Damage <= 0.f) return false;
	if (const ARPGPlayerCharacter* Player=Cast<ARPGPlayerCharacter>(this))
		if (Player->IsDodgeInvulnerable()) return false;
	// The single combat-damage funnel honours the shared invulnerability state;
	// every caller (abilities, projectiles, sweeps, executions) routes here.
	const FGameplayTag Invulnerable = FGameplayTag::RequestGameplayTag(FName("State.Invulnerable"), false);
	if (Invulnerable.IsValid() && ASC->HasMatchingGameplayTag(Invulnerable)) return false;
	const float HealthBeforeDamage = Stats->GetHealth();
	// Damage enters through GAS so modifiers, shields and attribute hooks stay
	// authoritative. Per-ability definitions may substitute their own damage GE.
	const UGameplayEffect* DamageEffect = DamageEffectOverride.Get()
		? DamageEffectOverride->GetDefaultObject<UGameplayEffect>()
		: GetDefault<URPGCombatDamageEffect>();
	FGameplayEffectContextHandle Context = ASC->MakeEffectContext();
	Context.AddSourceObject(DamageCauser);
	if (DamageCauser) Context.AddInstigator(DamageCauser, DamageCauser);
	FGameplayEffectSpec Spec(DamageEffect, Context, 1.f);
	Spec.SetSetByCallerMagnitude(FName("Damage"), -Damage);
	PendingDamageCauser = DamageCauser;
	const bool bApplied = ASC->ApplyGameplayEffectSpecToSelf(Spec).WasSuccessfullyApplied();
	PendingDamageCauser.Reset();
	if (!bApplied) return false;
	const float NewHealth = FMath::Max(0.f, Stats->GetHealth());
	if (ARPGCharacter* Source = Cast<ARPGCharacter>(DamageCauser))
		if (Source != this)
			if (ARPGPlayerState* State = Source->GetPlayerState<ARPGPlayerState>())
				if (State->Skills) URPGSkillExecution::RestoreResources(Source,
					FMath::Min(Damage, HealthBeforeDamage) * 0.03f * State->Skills->GetLearnedRank(TEXT("Abilities.Passive.LifeSiphon")),
					FMath::Min(Damage, HealthBeforeDamage) * 0.02f * State->Skills->GetLearnedRank(TEXT("Abilities.Passive.ManaSiphon")));
	if (AttributeComp)
	{
		AttributeComp->Health = NewHealth;
		AttributeComp->MaxHealth = Stats->GetMaxHealth();
		AttributeComp->OnHealthChanged.Broadcast(NewHealth);
	}
	if (NewHealth > 0.f) MulticastHitReaction();
	// Resolved, authoritative damage only: rejected specs never reach this point.
	const float Height = GetCapsuleComponent() ? GetCapsuleComponent()->GetScaledCapsuleHalfHeight() + 30.f : 90.f;
	MulticastDamageText(Damage, bBlockedHit, bCriticalHit, GetActorLocation() + FVector(0, 0, Height));
	if (NewHealth <= 0.f) HandleDeath(DamageCauser);
	return true;
}

void ARPGCharacter::HandleDeath(AActor* Killer)
{
	if (!HasAuthority() || bIsDead) return;
	bIsDead = true;
	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponent())
	{
		ASC->SetLooseGameplayTagCount(FGameplayTag::RequestGameplayTag(TEXT("State.Dead"), false), 1);
		ASC->CancelAllAbilities();
	}
	if (ActionComp) ActionComp->CancelPendingRelease(TEXT("death"));
	if (AnimationRouter) AnimationRouter->StopActiveMontage();
	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponent())
		if (ResourceRegenEffect.IsValid()) ASC->RemoveActiveGameplayEffect(ResourceRegenEffect);
	ResourceRegenEffect.Invalidate();
	ApplyDeathPose();
	OnCharacterDeath.Broadcast(this, Killer);
	DeathPresentation(Killer);
	if (const ARPGAICharacter* DefeatedAI = Cast<ARPGAICharacter>(this))
		if (ARPGCharacter* KillerCharacter = Cast<ARPGCharacter>(Killer))
			if (ARPGPlayerState* KillerState = KillerCharacter->GetPlayerState<ARPGPlayerState>())
				KillerState->GrantExperience(DefeatedAI->ExperienceReward, true);
	if (IsPlayerControlled())
		if (ARPGGameModeBase* Mode = GetWorld()->GetAuthGameMode<ARPGGameModeBase>()) Mode->HandlePlayerDeath(this);
	ForceNetUpdate();
}

void ARPGCharacter::ApplyDeathPose()
{
	GetCharacterMovement()->StopMovementImmediately();
	GetCharacterMovement()->DisableMovement();
	SetWeaponCollisionEnabled(false);
	RefreshEquipmentMeshes();
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	if (bDeathPoseApplied) return;
	bDeathPoseApplied = true;
	SetActorTickEnabled(false);
	LivingMeshTransform = GetMesh()->GetRelativeTransform();
	LivingMeshCollisionProfile = GetMesh()->GetCollisionProfileName();
	if (GetMesh()->GetPhysicsAsset())
	{
		GetMesh()->SetCollisionProfileName(TEXT("Ragdoll"));
		GetMesh()->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		GetMesh()->SetEnableGravity(true);
		GetMesh()->SetAllBodiesSimulatePhysics(true);
		GetMesh()->SetSimulatePhysics(true);
		GetMesh()->SetAllBodiesPhysicsBlendWeight(1.f);
		GetMesh()->WakeAllRigidBodies();
		// A perfectly balanced idle physics pose can remain standing. Initiate
		// the death fall instead of relying on incidental contact to tip it over.
		GetMesh()->AddImpulse(GetActorForwardVector() * 150.f + GetActorRightVector() * 75.f, TEXT("pelvis"), true);
	}
}

void ARPGCharacter::RestoreLivingPose()
{
	if (bDeathPoseApplied)
	{
		GetMesh()->SetSimulatePhysics(false);
		GetMesh()->SetAllBodiesSimulatePhysics(false);
		GetMesh()->SetAllBodiesPhysicsBlendWeight(0.f);
		GetMesh()->AttachToComponent(GetCapsuleComponent(), FAttachmentTransformRules::KeepRelativeTransform);
		GetMesh()->SetRelativeTransform(LivingMeshTransform);
		GetMesh()->SetCollisionProfileName(LivingMeshCollisionProfile);
		GetMesh()->InitAnim(true);
		bDeathPoseApplied = false;
	}
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	GetCharacterMovement()->SetMovementMode(MOVE_Walking);
	SetActorTickEnabled(PrimaryActorTick.bCanEverTick);
}

bool ARPGCharacter::RespawnAt(const FTransform& SpawnTransform)
{
	UAbilitySystemComponent* ASC = GetAbilitySystemComponent();
	const URPGAttributeSet* Stats = GetRPGAttributes();
	if (!HasAuthority() || !bIsDead || !ASC || !Stats || SpawnTransform.ContainsNaN()) return false;
	if (ActionComp) ActionComp->ResetAfterRespawn();
	RestoreLivingPose();
	SetActorTransform(SpawnTransform, false, nullptr, ETeleportType::TeleportPhysics);
	bIsDead = false;
	ASC->SetLooseGameplayTagCount(FGameplayTag::RequestGameplayTag(TEXT("State.Dead"), false), 0);
	ASC->SetLooseGameplayTagCount(FGameplayTag::RequestGameplayTag(TEXT("State.Stunned"), false), 0);
	RefreshEquipmentMeshes();
	ASC->SetNumericAttributeBase(URPGAttributeSet::GetHealthAttribute(), Stats->GetMaxHealth());
	ASC->SetNumericAttributeBase(URPGAttributeSet::GetManaAttribute(), Stats->GetMaxMana());
	ASC->SetNumericAttributeBase(URPGAttributeSet::GetStaminaAttribute(), Stats->GetMaxStamina());
	FGameplayEffectSpec RegenSpec(GetDefault<URPGResourceRegenEffect>(), ASC->MakeEffectContext(), 1.f);
	ResourceRegenEffect = ASC->ApplyGameplayEffectSpecToSelf(RegenSpec);
	if (ARPGPlayerController* PC = Cast<ARPGPlayerController>(GetController())) PC->ClientFinishRespawn();
	ForceNetUpdate();
	return true;
}

UNiagaraComponent* ARPGCharacter::SpawnBoundedNiagara(UNiagaraSystem* Effect, const FVector& Location,
                                                      const float LifeSeconds)
{
	if (!Effect || GetNetMode() == NM_DedicatedServer) return nullptr;
	UNiagaraComponent* FX = UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, Effect, Location,
		FRotator::ZeroRotator, FVector::OneVector, true, true);
	if (!FX || LifeSeconds <= 0.f) return FX;
	// Some retained effects loop forever; AutoDestroy alone never completes
	// those systems. Bound temporary spell presentation explicitly.
	TWeakObjectPtr<UNiagaraComponent> WeakFX = FX;
	FTimerHandle Handle;
	GetWorld()->GetTimerManager().SetTimer(Handle, FTimerDelegate::CreateLambda([WeakFX]()
	{
		if (UNiagaraComponent* Component = WeakFX.Get())
		{
			Component->DeactivateImmediate();
			if (UNiagaraComponent* Remaining = WeakFX.Get()) Remaining->DestroyComponent();
		}
	}), FMath::Max(0.05f, LifeSeconds), false);
	return FX;
}

void ARPGCharacter::AssignBeamEndpoints(UNiagaraComponent* FX, const FVector& Start, const FVector& End)
{
	if (!FX) return;
	static const FName StartNames[] = {TEXT("BeamStart"), TEXT("Beam Start"), TEXT("User.BeamStart"),
		TEXT("BeamStartPos"), TEXT("StartPoint"), TEXT("Start Position"), TEXT("SourcePosition")};
	static const FName EndNames[] = {TEXT("BeamEnd"), TEXT("Beam End"), TEXT("User.BeamEnd"),
		TEXT("BeamEndPos"), TEXT("EndPoint"), TEXT("End Position"), TEXT("TargetPosition")};
	for (const FName& Name : StartNames) FX->SetVectorParameter(Name, Start);
	for (const FName& Name : EndNames) FX->SetVectorParameter(Name, End);
}

void ARPGCharacter::MulticastSkillFX_Implementation(UNiagaraSystem* Effect, const FVector Location)
{
	SpawnBoundedNiagara(Effect, Location, 0.f);
}

void ARPGCharacter::MulticastSkillFXTimed_Implementation(UNiagaraSystem* Effect, const FVector Location, const float LifeSeconds)
{
	SpawnBoundedNiagara(Effect, Location, LifeSeconds);
}

void ARPGCharacter::MulticastChainFX_Implementation(UNiagaraSystem* Effect, const TArray<FVector_NetQuantize>& Points,
                                                  const float LifeSeconds)
{
	if (!Effect || GetNetMode() == NM_DedicatedServer || Points.Num() < 2) return;
	for (int32 Index = 0; Index + 1 < Points.Num(); ++Index)
		if (UNiagaraComponent* FX = SpawnBoundedNiagara(Effect, Points[Index], LifeSeconds))
			AssignBeamEndpoints(FX, Points[Index], Points[Index + 1]);
}

void ARPGCharacter::MulticastDamageText_Implementation(const float Damage, const bool bBlockedHit,
                                                       const bool bCriticalHit, const FVector_NetQuantize Location)
{
	if (GetNetMode() == NM_DedicatedServer || Damage <= 0.f) return;
	UClass* Class = DamageTextComponentClass ? DamageTextComponentClass.Get()
		: LoadClass<URPG_DamageTextComponent>(nullptr, TEXT("/Game/UI/World/BP_DamageTextComponent.BP_DamageTextComponent_C"));
	if (!Class) return;
	URPG_DamageTextComponent* Popup = NewObject<URPG_DamageTextComponent>(this, Class);
	Popup->SetupAttachment(GetRootComponent());
	Popup->RegisterComponent();
	Popup->SetWorldLocation(Location);
	Popup->ShowDamageText(Damage, bBlockedHit, bCriticalHit);
}

void ARPGCharacter::MulticastHitReaction_Implementation()
{
	if (GetNetMode() != NM_DedicatedServer && AnimationRouter) AnimationRouter->PlayHitReaction();
}

void ARPGCharacter::OnRep_Death()
{
	if (!bIsDead) { RestoreLivingPose(); RefreshEquipmentMeshes(); return; }
	ApplyDeathPose();
	OnCharacterDeath.Broadcast(this, nullptr);
	DeathPresentation(nullptr);
}

UAbilitySystemComponent* ARPGCharacter::GetAbilitySystemComponent() const
{
	const ARPGPlayerState* PS = GetPlayerState<ARPGPlayerState>();
	return PS ? PS->AbilitySystem.Get() : FallbackAbilitySystem.Get();
}

URPGAttributeSet* ARPGCharacter::GetRPGAttributes() const
{
	const ARPGPlayerState* PS = GetPlayerState<ARPGPlayerState>();
	return PS ? PS->Attributes.Get() : FallbackAttributes.Get();
}

void ARPGCharacter::InitializeAbilitySystem()
{
	UAbilitySystemComponent* ASC = GetAbilitySystemComponent();
	if (!ASC) return;
	AActor* OwnerActor = GetPlayerState<ARPGPlayerState>() ? static_cast<AActor*>(GetPlayerState()) : this;
	ASC->InitAbilityActorInfo(OwnerActor, this);
	if (HasAuthority())
		if (ARPGPlayerState* PS = GetPlayerState<ARPGPlayerState>())
			if (PS->Skills) PS->Skills->RefreshGrantedAbilities();
	const FGameplayAttribute Observed[] = {URPGAttributeSet::GetHealthAttribute(), URPGAttributeSet::GetMaxHealthAttribute(),
		URPGAttributeSet::GetManaAttribute(), URPGAttributeSet::GetMaxManaAttribute(),
		URPGAttributeSet::GetStaminaAttribute(), URPGAttributeSet::GetMaxStaminaAttribute()};
	if (BoundAbilitySystem.Get() != ASC)
	{
		if (UAbilitySystemComponent* OldASC = BoundAbilitySystem.Get())
		{
			for (const FGameplayAttribute& Attribute : Observed) OldASC->GetGameplayAttributeValueChangeDelegate(Attribute).RemoveAll(this);
			if (HasAuthority())
			{
				if (EquipmentEffect.IsValid()) OldASC->RemoveActiveGameplayEffect(EquipmentEffect);
				if (ResourceRegenEffect.IsValid()) OldASC->RemoveActiveGameplayEffect(ResourceRegenEffect);
				if (DerivedAttributeEffect.IsValid()) OldASC->RemoveActiveGameplayEffect(DerivedAttributeEffect);
			}
		}
		EquipmentEffect.Invalidate();
		ResourceRegenEffect.Invalidate();
		DerivedAttributeEffect.Invalidate();
		BoundAbilitySystem = ASC;
		for (const FGameplayAttribute& Attribute : Observed)
			ASC->GetGameplayAttributeValueChangeDelegate(Attribute).AddUObject(this, &ARPGCharacter::RefreshLegacyAttributes);
		if (HasAuthority())
		{
			// Rebinding (new pawn on an existing PlayerState ASC) must not stack
			// second copies of the permanent effects.
			for (const FActiveGameplayEffectHandle& Existing : ASC->GetActiveEffects(FGameplayEffectQuery()))
				if (const FActiveGameplayEffect* Effect = ASC->GetActiveGameplayEffect(Existing))
					if (Effect->Spec.Def && (Effect->Spec.Def->IsA<URPGDerivedAttributeEffect>() || Effect->Spec.Def->IsA<URPGResourceRegenEffect>()
						|| Effect->Spec.Def->IsA<URPGEquipmentStatEffect>()))
						ASC->RemoveActiveGameplayEffect(Existing);
			FGameplayEffectSpec DerivedSpec(GetDefault<URPGDerivedAttributeEffect>(), ASC->MakeEffectContext(), 1.f);
			DerivedAttributeEffect = ASC->ApplyGameplayEffectSpecToSelf(DerivedSpec);
			ApplyEquipmentState(EquippedItems);
			FGameplayEffectSpec RegenSpec(GetDefault<URPGResourceRegenEffect>(), ASC->MakeEffectContext(), 1.f);
			ResourceRegenEffect = ASC->ApplyGameplayEffectSpecToSelf(RegenSpec);
		}
	}
	RefreshLegacyAttributes(FOnAttributeChangeData());
}

void ARPGCharacter::RefreshLegacyAttributes(const FOnAttributeChangeData& Data)
{
	const URPGAttributeSet* Stats = GetRPGAttributes();
	if (!Stats || !AttributeComp) return;
	AttributeComp->Health = Stats->GetHealth(); AttributeComp->MaxHealth = Stats->GetMaxHealth();
	AttributeComp->Mana = Stats->GetMana(); AttributeComp->MaxMana = Stats->GetMaxMana();
	AttributeComp->Stamina = Stats->GetStamina(); AttributeComp->MaxStamina = Stats->GetMaxStamina();
	AttributeComp->OnHealthChanged.Broadcast(AttributeComp->Health);
	AttributeComp->OnMaxHealthChanged.Broadcast(AttributeComp->MaxHealth);
	AttributeComp->OnManaChanged.Broadcast(AttributeComp->Mana);
	AttributeComp->OnMaxManaChanged.Broadcast(AttributeComp->MaxMana);
	OnHealthChanged.Broadcast(AttributeComp->Health); OnMaxHealthChanged.Broadcast(AttributeComp->MaxHealth);
	if (HasAuthority() && Stats->GetHealth() <= 0.f) HandleDeath(PendingDamageCauser.Get());
}

void ARPGCharacter::UseItem(UItemObject* Item)
{
	if (InventoryComp)
	{
		InventoryComp->UseItem(Item);
	}
}

void ARPGCharacter::DropItem(UItemObject* Item, const int32 Quantity)
{
	if (HasAuthority()) TryDropItem(Item, Quantity);
	else ServerDropItem(Item, Quantity);
}

bool ARPGCharacter::TryDropItem(UItemObject* Item, const int32 Quantity)
{
	if (!HasAuthority() || !InventoryComp || !IsValid(Item) || InventoryComp->FindItem(Item) != Item ||
		Quantity <= 0 || Item->GetQuantity() < Quantity || IsItemEquipped(Item) || !PickupClass) return false;
	const FVector Location = GetActorLocation() + GetActorForwardVector() * DropForwardDistance;
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	ARPGPickupItem* Pickup = GetWorld()->SpawnActor<ARPGPickupItem>(PickupClass, Location, GetActorRotation(), Params);
	if (!Pickup || !Pickup->InitializePickup(Item->GetClass(), Quantity))
	{
		if (Pickup) Pickup->Destroy();
		return false;
	}
	if (!InventoryComp->ConsumeItem(Item, Quantity))
	{
		Pickup->Destroy();
		return false;
	}
	return true;
}

void ARPGCharacter::ServerDropItem_Implementation(UItemObject* Item, const int32 Quantity) { TryDropItem(Item, Quantity); }

bool ARPGCharacter::CollectPickup(ARPGPickupItem* Pickup)
{
	if (HasAuthority()) return IsValid(Pickup) && Pickup->TryCollect(this);
	ServerCollectPickup(Pickup);
	return false;
}

void ARPGCharacter::ServerCollectPickup_Implementation(ARPGPickupItem* Pickup)
{
	if (IsValid(Pickup)) Pickup->TryCollect(this);
}

void ARPGCharacter::PurchaseFromVendor(ARPGVendor* Vendor, const FName OfferId, const int32 PurchaseCount)
{
	if (HasAuthority())
	{
		if (IsValid(Vendor)) Vendor->TryPurchase(this, OfferId, PurchaseCount);
	}
	else ServerPurchaseFromVendor(Vendor, OfferId, PurchaseCount);
}

void ARPGCharacter::ServerPurchaseFromVendor_Implementation(ARPGVendor* Vendor, const FName OfferId, const int32 PurchaseCount)
{
	if (IsValid(Vendor)) Vendor->TryPurchase(this, OfferId, PurchaseCount);
}

void ARPGCharacter::FrontCaptureOn()
{
	if (FrontCapture)
	{
		FrontCapture->Activate();
		FrontCapture->bCaptureEveryFrame = true;
	}
}

void ARPGCharacter::FrontCaptureOff()
{
	if (FrontCapture)
	{
		FrontCapture->Deactivate();
		FrontCapture->bCaptureEveryFrame = false;
	}
}

void ARPGCharacter::FaceCaptureOn()
{
	if (FaceCapture)
	{
		FaceCapture->Activate();
		FaceCapture->bCaptureEveryFrame = true;
	}
}

void ARPGCharacter::FaceCaptureOff()
{
	if (FaceCapture)
	{
		FaceCapture->Deactivate();
		FaceCapture->bCaptureEveryFrame = false;
	}
}

TMap<EEquipableSlot, UEquipment_ItemObject*> ARPGCharacter::GetEquippedItems() const
{
	TMap<EEquipableSlot, UEquipment_ItemObject*> Result;
	if (!HasAuthority())
	{
		// Equipment identities can arrive before their owner-only inventory instances.
		for (const FRPGEquippedItem& Entry : ReplicatedEquipment)
			if (UEquipment_ItemObject* Item = Cast<UEquipment_ItemObject>(InventoryComp->FindItemById(Entry.ItemId))) Result.Add(Entry.Slot, Item);
		return Result;
	}
	for (const TPair<EEquipableSlot, TObjectPtr<UEquipment_ItemObject>>& Pair : EquippedItems)
	{
		Result.Add(Pair.Key, Pair.Value.Get());
	}
	return Result;
}

void ARPGCharacter::EquipItem(UEquipment_ItemObject* Item, const EEquipableSlot Slot)
{
	if (HasAuthority()) TryEquipItem(Item, Slot);
	else ServerEquipItem(Item, Slot);
}

void ARPGCharacter::UnequipItem(const EEquipableSlot Slot)
{
	if (HasAuthority()) TryUnequipItem(Slot);
	else ServerUnequipItem(Slot);
}

void ARPGCharacter::ServerEquipItem_Implementation(UEquipment_ItemObject* Item, EEquipableSlot Slot)
{
	const bool bEquipped = TryEquipItem(Item, Slot);
	UE_LOG(LogTemp, Verbose, TEXT("Equipment request: result=%d item=%s owned=%d requestedSlot=%d itemSlot=%d bonuses=%d"),
		bEquipped, *GetNameSafe(Item), Item && Item->OwningInventory == InventoryComp,
		static_cast<int32>(Slot), Item ? static_cast<int32>(Item->Slot) : -1, Item ? Item->AttributeBonuses.Num() : 0);
}
void ARPGCharacter::ServerUnequipItem_Implementation(EEquipableSlot Slot) { TryUnequipItem(Slot); }

bool ARPGCharacter::IsItemEquipped(const UItemObject* Item) const
{
	if (!IsValid(Item)) return false;
	if (!HasAuthority()) return ReplicatedEquipment.ContainsByPredicate([Item](const FRPGEquippedItem& Entry) { return Entry.ItemId == Item->ItemId; });
	for (const auto& Pair : EquippedItems) if (Pair.Value == Item) return true;
	return false;
}

bool ARPGCharacter::TryEquipItem(UEquipment_ItemObject* Item, const EEquipableSlot Slot)
{
	if (!HasAuthority() || !IsValid(Item) || Slot == EEquipableSlot::EIS_None || Item->Slot != Slot ||
		!InventoryComp || InventoryComp->FindItem(Item) != Item || Item->OwningInventory != InventoryComp || Item->GetQuantity() <= 0) return false;
	auto Next = EquippedItems;
	Next.Add(Slot, Item);
	return ApplyEquipmentState(Next);
}

bool ARPGCharacter::TryUnequipItem(const EEquipableSlot Slot)
{
	if (!HasAuthority() || !EquippedItems.Contains(Slot)) return false;
	auto Next = EquippedItems;
	Next.Remove(Slot);
	return ApplyEquipmentState(Next);
}

bool ARPGCharacter::ApplyEquipmentState(const TMap<EEquipableSlot, TObjectPtr<UEquipment_ItemObject>>& NewEquipment)
{
	UAbilitySystemComponent* ASC = GetAbilitySystemComponent();
	if (!HasAuthority() || !ASC) return false;
	const UGameplayEffect* Definition = GetDefault<URPGEquipmentStatEffect>();
	FGameplayEffectSpec Spec(Definition, ASC->MakeEffectContext(), 1.f);
	TMap<FName, float> Totals;
	for (const FGameplayModifierInfo& Modifier : Definition->Modifiers) Totals.Add(FName(*Modifier.Attribute.GetName()), 0.f);
	for (const auto& Pair : NewEquipment)
	{
		if (!IsValid(Pair.Value)) return false;
		for (const auto& Bonus : Pair.Value->AttributeBonuses)
		{
			float* Total = Totals.Find(Bonus.Key);
			if (!Total || !FMath::IsFinite(Bonus.Value) || !FMath::IsFinite(*Total + Bonus.Value)) return false;
			*Total += Bonus.Value;
		}
	}
	for (const auto& Total : Totals) Spec.SetSetByCallerMagnitude(Total.Key, Total.Value);
	{
		// Defer evaluation until replacement is complete, avoiding a temporary
		// max-health dip when swapping two pieces of gear with the same bonus.
		FScopedAggregatorOnDirtyBatch Batch;
		const FActiveGameplayEffectHandle NewEffect = ASC->ApplyGameplayEffectSpecToSelf(Spec);
		if (!NewEffect.IsValid()) return false;
		if (EquipmentEffect.IsValid()) ASC->RemoveActiveGameplayEffect(EquipmentEffect);
		EquipmentEffect = NewEffect;
	}
	TSet<UEquipment_ItemObject*> ChangedItems;
	for (const auto& Pair : EquippedItems) ChangedItems.Add(Pair.Value);
	for (const auto& Pair : NewEquipment) ChangedItems.Add(Pair.Value);
	EquippedItems = NewEquipment;
	ReplicatedEquipment.Reset();
	for (const auto& Pair : EquippedItems)
	{
		FRPGEquippedItem Entry;
		Entry.Slot = Pair.Key; Entry.ItemId = Pair.Value->ItemId; Entry.ItemClass = Pair.Value->GetClass();
		ReplicatedEquipment.Add(Entry);
	}
	OnEquipedItemsChanged.Broadcast();
	for (UEquipment_ItemObject* Item : ChangedItems) if (IsValid(Item)) Item->OnItemModified.Broadcast();
	InventoryComp->OnInventoryUpdated.Broadcast();
	RefreshEquipmentMeshes();
	EquipmentAppearanceChanged();
	ForceNetUpdate();
	return true;
}

void ARPGCharacter::OnRep_Equipment()
{
	if (HasAuthority()) return;
	EquippedItems.Reset();
	for (const FRPGEquippedItem& Entry : ReplicatedEquipment)
	{
		if (UEquipment_ItemObject* Item = Cast<UEquipment_ItemObject>(InventoryComp->FindItemById(Entry.ItemId)))
			EquippedItems.Add(Entry.Slot, Item);
	}
	OnEquipedItemsChanged.Broadcast();
	RefreshEquipmentMeshes();
	EquipmentAppearanceChanged();
}

void ARPGCharacter::InitializeStartingEquipment()
{
	if (!HasAuthority() || bStartingEquipmentInitialized || !InventoryComp) return;
	bStartingEquipmentInitialized = true;
	for (const TSubclassOf<UEquipment_ItemObject> ItemClass : StartingEquipment)
	{
		if (!ItemClass) continue;
		const UEquipment_ItemObject* Defaults = ItemClass->GetDefaultObject<UEquipment_ItemObject>();
		if (Defaults->Slot == EEquipableSlot::EIS_None || EquippedItems.Contains(Defaults->Slot)) continue;
		UEquipment_ItemObject* Item = Cast<UEquipment_ItemObject>(InventoryComp->FindItemByClass(ItemClass));
		if (!Item)
		{
			if (InventoryComp->TryAddItemFromClass(ItemClass, 1).AmountGiven != 1) continue;
			Item = Cast<UEquipment_ItemObject>(InventoryComp->FindItemByClass(ItemClass));
		}
		TryEquipItem(Item, Defaults->Slot);
	}
}

void ARPGCharacter::RefreshEquipmentMeshes()
{
	for (UStaticMeshComponent* Component : EquipmentMeshes) if (IsValid(Component)) Component->DestroyComponent();
	EquipmentMeshes.Reset();
	WeaponCollisionComponents.Reset();
	if (!GetMesh() || bIsDead) return;
	const bool bDedicatedServer = GetNetMode() == NM_DedicatedServer;
	for (const FRPGEquippedItem& Entry : ReplicatedEquipment)
	{
		const bool bWeapon = Entry.Slot == EEquipableSlot::EIS_PrimaryWeapon || Entry.Slot == EEquipableSlot::EIS_SecondaryWeapon;
		// Dedicated servers still need equipped-weapon geometry so authoritative
		// melee windows have real overlap volumes; cosmetic slots are skipped.
		if (bDedicatedServer && !bWeapon) continue;
		const UEquipment_ItemObject* Item = Entry.ItemClass ? Entry.ItemClass->GetDefaultObject<UEquipment_ItemObject>() : nullptr;
		if (!Item || !Item->EquippedMesh || !GetMesh()->DoesSocketExist(Item->AttachSocket)) continue;
		UStaticMeshComponent* Component = NewObject<UStaticMeshComponent>(this);
		Component->SetStaticMesh(Item->EquippedMesh);
		Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Component->SetCanEverAffectNavigation(false);
		Component->SetupAttachment(GetMesh(), Item->AttachSocket);
		Component->SetRelativeTransform(Item->AttachmentTransform);
		Component->RegisterComponent();
		if (bDedicatedServer) Component->SetVisibility(false);
		EquipmentMeshes.Add(Component);
		if (bWeapon)
		{
			Component->SetGenerateOverlapEvents(false);
			Component->SetCollisionResponseToAllChannels(ECR_Ignore);
			Component->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
			Component->IgnoreActorWhenMoving(this, true);
			Component->OnComponentBeginOverlap.AddUniqueDynamic(this, &ThisClass::OnWeaponCollisionBeginOverlap);
			WeaponCollisionComponents.Add(Component);
		}
	}
	if (IsLocallyControlled()) InitializePlayerCaptures();
}

void ARPGCharacter::SetWeaponCollisionEnabled(const bool bEnabled)
{
	if (!HasAuthority()) return;
	for (UPrimitiveComponent* Component : WeaponCollisionComponents)
	{
		if (!IsValid(Component)) continue;
		Component->SetGenerateOverlapEvents(bEnabled);
		Component->SetCollisionEnabled(bEnabled ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
	}
}

void ARPGCharacter::OnWeaponCollisionBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComponent, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (ActionComp) ActionComp->HandleWeaponCollision(OtherActor);
}
