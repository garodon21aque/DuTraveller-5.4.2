#include "RPGPlayerCharacter.h"
#include "RPGPlayerController.h"
#include "RPGActionComponent.h"
#include "RPGProjectile.h"
#include "RPGAttributeSet.h"
#include "AbilitySystemComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SphereComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "GameFramework/RootMotionSource.h"
#include "Kismet/GameplayStatics.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimInstance.h"
#include "EngineUtils.h"
#include "Net/UnrealNetwork.h"
#include "NativeGameplayTags.h"

// Native registration keeps the tags resolvable everywhere config import order varies.
UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Dodging, "State.Dodging", "Authoritative directional dodge state");
UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Invulnerable, "State.Invulnerable", "Character cannot receive combat damage while this state is active");

ARPGPlayerCharacter::ARPGPlayerCharacter()
{
	FactionId = FName("Player");
	PrimaryActorTick.bCanEverTick = true;
}

void ARPGPlayerCharacter::BeginPlay()
{
	Super::BeginPlay();
	if (GetNetMode()!=NM_DedicatedServer)
	{
		// Player poses and authored combat notifies must keep advancing even
		// when a weapon, camera transition or preview capture changes visibility.
		GetMesh()->VisibilityBasedAnimTickOption=EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
		GetMesh()->bEnableUpdateRateOptimizations=false;
	}
	OnCharacterDeath.AddUniqueDynamic(this, &ThisClass::AvoidanceDeath);
}

void ARPGPlayerCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ARPGPlayerCharacter, bIsDodging);
}

bool ARPGPlayerCharacter::RequestDodge(FVector Direction)
{
	if (Direction.ContainsNaN() || bIsDead || bIsStunned) return false;
	if (const ARPGPlayerController* PC = Cast<ARPGPlayerController>(GetController()))
		if (PC->ShouldBlockGameplayInput() || PC->IsMoveInputIgnored() || PC->IsPaused()) return false;
	Direction.Z = 0;
	if (Direction.IsNearlyZero()) Direction = GetPendingMovementInputVector().GetSafeNormal2D();
	if (Direction.IsNearlyZero()) Direction = GetCharacterMovement()->GetCurrentAcceleration().GetSafeNormal2D();
	if (Direction.IsNearlyZero()) Direction = -FRotationMatrix(FRotator(0,GetControlRotation().Yaw,0)).GetUnitAxis(EAxis::X);
	Direction = Direction.GetSafeNormal2D();
	if (HasAuthority()) return StartDodge(Direction);
	ServerDodge(Direction);
	return true;
}

void ARPGPlayerCharacter::ServerDodge_Implementation(FVector_NetQuantizeNormal Direction) { StartDodge(Direction); }
void ARPGPlayerCharacter::ServerCancelDodge_Implementation() { FinishDodge(); }

bool ARPGPlayerCharacter::StartDodge(FVector Direction)
{
	UCharacterMovementComponent* Move = GetCharacterMovement();
	UAbilitySystemComponent* ASC = GetAbilitySystemComponent();
	const URPGAttributeSet* Stats = GetRPGAttributes();
	if (!HasAuthority() || !bEnableDodge || bIsDead || bIsStunned || bIsDodging || Direction.ContainsNaN() ||
		!Move->IsMovingOnGround() || GetWorld()->GetRealTimeSeconds()<NextDodgeReal || !ASC || !Stats ||
		Stats->GetStamina()<FMath::Max(0.f,DodgeStaminaCost) || !AnimationRouter ||
		!DodgeAnimation.Montage || !AnimationRouter->CanPlayCombatAnimation(DodgeAnimation)) return false;
	Direction = Direction.GetSafeNormal2D();
	if (Direction.IsNearlyZero()) return false;
	const float Duration = FMath::Clamp(DodgeDuration/FMath::Max(.01f,DodgeAnimation.PlayRate),.2f,2.f);
	const bool bAuthoredRootMotion=DodgeAnimation.Montage->HasRootMotion();
	const FVector AuthoredDelta=bAuthoredRootMotion ? DodgeAnimation.Montage->ExtractRootMotionFromTrackRange(0.f,DodgeAnimation.Montage->GetPlayLength(),FAnimExtractContext()).GetTranslation() : FVector::ZeroVector;
	if (bAuthoredRootMotion && (AuthoredDelta.ContainsNaN() || AuthoredDelta.Size2D()<1.f)) return false;
	ActionComp->CancelPendingRelease(TEXT("dodge"));
	AnimationRouter->StopActiveMontage(.08f);
	if (DodgeStaminaCost > 0.f)
	{
		FGameplayEffectSpec CostSpec(GetDefault<URPGAbilityCostEffect>(), ASC->MakeEffectContext(), 1.f);
		CostSpec.SetSetByCallerMagnitude(FName("Mana"), 0.f);
		CostSpec.SetSetByCallerMagnitude(FName("Stamina"), -FMath::Max(0.f, DodgeStaminaCost));
		ASC->ApplyGameplayEffectSpecToSelf(CostSpec);
	}
	bIsDodging = true;
	DodgeElapsedSeconds=0; ActiveDodgeDuration=Duration;
	DodgeHeading = Direction;
	bSavedOrientToMovement = Move->bOrientRotationToMovement; bSavedDesiredRotation = Move->bUseControllerDesiredRotation;
	Move->bOrientRotationToMovement = false; Move->bUseControllerDesiredRotation = false;
	bDodgeUsesAuthoredRootMotion=bAuthoredRootMotion;
	SavedRootMotionTranslationScale=GetAnimRootMotionTranslationScale();
	if (bAuthoredRootMotion)
	{
		// Consume the advancing root track into the collision capsule. The mesh
		// then remains over that capsule instead of returning to its old origin.
		SetAnimRootMotionTranslationScale(FMath::Max(0.f,DodgeDistance)/AuthoredDelta.Size2D());
	}
	else
	{
		TSharedPtr<FRootMotionSource_ConstantForce> Force = MakeShared<FRootMotionSource_ConstantForce>();
		Force->InstanceName = TEXT("DT_Dodge"); Force->Priority = 500;
		Force->AccumulateMode = ERootMotionAccumulateMode::Override;
		Force->Duration = Duration; Force->Force = Direction*FMath::Max(0.f,DodgeDistance)/Duration;
		Force->FinishVelocityParams.Mode = ERootMotionFinishVelocityMode::SetVelocity;
		Force->FinishVelocityParams.SetVelocity = FVector::ZeroVector;
		DodgeRootMotionId = Move->ApplyRootMotionSource(Force);
	}
	OnRep_Dodging();
	MulticastDodge(Direction,Duration);
	ForceNetUpdate();
	return true;
}

void ARPGPlayerCharacter::MulticastDodge_Implementation(FVector_NetQuantizeNormal Direction, float Duration)
{
	DodgeHeading = Direction;
	if (!HasAuthority())
	{
		bSavedOrientToMovement=GetCharacterMovement()->bOrientRotationToMovement;
		bSavedDesiredRotation=GetCharacterMovement()->bUseControllerDesiredRotation;
		GetCharacterMovement()->bOrientRotationToMovement=false; GetCharacterMovement()->bUseControllerDesiredRotation=false;
		bIsDodging=true; DodgeElapsedSeconds=0; ActiveDodgeDuration=Duration;
		bDodgeUsesAuthoredRootMotion=DodgeAnimation.Montage && DodgeAnimation.Montage->HasRootMotion();
		SavedRootMotionTranslationScale=GetAnimRootMotionTranslationScale();
		if (bDodgeUsesAuthoredRootMotion)
		{
			const float Distance=DodgeAnimation.Montage->ExtractRootMotionFromTrackRange(0.f,DodgeAnimation.Montage->GetPlayLength(),FAnimExtractContext()).GetTranslation().Size2D();
			if (Distance>1.f) SetAnimRootMotionTranslationScale(FMath::Max(0.f,DodgeDistance)/Distance);
		}
		OnRep_Dodging();
	}
	SetActorRotation(Direction.Rotation());
	if ((GetNetMode()!=NM_DedicatedServer || bDodgeUsesAuthoredRootMotion) && AnimationRouter && DodgeAnimation.Montage)
	{
		FDTCombatAnimation Animation=DodgeAnimation;
		Animation.PlayRate=Animation.Montage->GetPlayLength()/FMath::Max(.1f,Duration);
		AnimationRouter->PlayCombatAnimation(Animation);
	}
}

void ARPGPlayerCharacter::OnRep_Dodging()
{
	if (UAbilitySystemComponent* ASC=GetAbilitySystemComponent())
	{
		ASC->SetLooseGameplayTagCount(FGameplayTag::RequestGameplayTag(TEXT("State.Dodging"),false),bIsDodging?1:0);
		ASC->SetLooseGameplayTagCount(FGameplayTag::RequestGameplayTag(TEXT("State.Invulnerable"),false),bIsDodging && bDodgeInvulnerability?1:0);
	}
	if (!bIsDodging)
	{
		SetAnimRootMotionTranslationScale(SavedRootMotionTranslationScale);
		bDodgeUsesAuthoredRootMotion=false;
		GetCharacterMovement()->bOrientRotationToMovement=bSavedOrientToMovement;
		GetCharacterMovement()->bUseControllerDesiredRotation=bSavedDesiredRotation;
	}
}

bool ARPGPlayerCharacter::IsDodgeInvulnerable() const
{
	return bIsDodging && bDodgeInvulnerability;
}

void ARPGPlayerCharacter::FinishDodge()
{
	if (!bIsDodging) return;
	if (DodgeRootMotionId) GetCharacterMovement()->RemoveRootMotionSourceByID(DodgeRootMotionId);
	DodgeRootMotionId=0;
	if (AnimationRouter && AnimationRouter->ActiveCombatMontage==DodgeAnimation.Montage) AnimationRouter->StopActiveMontage(.1f);
	bIsDodging=false; NextDodgeReal=GetWorld()->GetRealTimeSeconds()+FMath::Max(0.f,DodgeCooldown);
	GetCharacterMovement()->StopMovementImmediately();
	OnRep_Dodging(); ForceNetUpdate();
}

void ARPGPlayerCharacter::CancelAvoidance()
{
	SetFocusHeld(false);
	if (HasAuthority()) { FinishDodge(); }
	else if (bIsDodging)
	{
		ServerCancelDodge();
		// The dodge tags sit on the PlayerState-owned ASC. If this pawn dies or is
		// destroyed before the bIsDodging rep arrives, State.Invulnerable must still clear.
		bIsDodging=false;
		OnRep_Dodging();
	}
}

void ARPGPlayerCharacter::AvoidanceDeath(ARPGCharacter*,AActor*) { CancelAvoidance(); }
void ARPGPlayerCharacter::EndPlay(const EEndPlayReason::Type Reason) { CancelAvoidance(); Super::EndPlay(Reason); }

void ARPGPlayerCharacter::FaceRotation(FRotator Rotation,float DeltaTime)
{
	if (!bIsDodging) Super::FaceRotation(Rotation,DeltaTime);
}

void ARPGPlayerCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (bIsDodging)
	{
		// Match montage and CharacterMovement simulation time. Wall-clock stalls
		// must not cut the clip and root-motion source off halfway through.
		DodgeElapsedSeconds+=DeltaSeconds;
		const UAnimInstance* Anim=GetMesh()->GetAnimInstance();
		const bool bMontageFinished=bDodgeUsesAuthoredRootMotion && Anim && DodgeElapsedSeconds>.05f && !Anim->Montage_IsActive(DodgeAnimation.Montage);
		const float Timeout=ActiveDodgeDuration+(bDodgeUsesAuthoredRootMotion ? .25f : 0.f);
		if (HasAuthority() && (bIsDead || bIsStunned || bMontageFinished || DodgeElapsedSeconds>=Timeout)) FinishDodge();
		else SetActorRotation(DodgeHeading.Rotation());
	}
	if (IsLocallyControlled()) UpdateThreatFocus();
}

void ARPGPlayerCharacter::SetFocusHeld(bool bHeld)
{
	bFocusHeld=bHeld;
	if (!bHeld) StopThreatFocus();
}

bool ARPGPlayerCharacter::FindIncomingThreat(AActor*& Threat,float& TimeToHit) const
{
	Threat=nullptr; TimeToHit=FMath::Max(.05f,ThreatLeadSeconds);
	if (!GetWorld() || bIsDead) return false;
	const auto HasClearPath=[this](AActor* Source,const FVector& Start,const FVector& End)
	{
		FHitResult Hit; FCollisionQueryParams Query(SCENE_QUERY_STAT(DTThreatSight),false,this);
		Query.AddIgnoredActor(Source);
		return !GetWorld()->LineTraceSingleByObjectType(Hit,Start,End,FCollisionObjectQueryParams(ECC_WorldStatic),Query);
	};
	for (TActorIterator<ARPGProjectile> It(GetWorld()); It; ++It)
	{
		ARPGCharacter* Source=It->GetThreatSource();
		if (!It->IsLiveThreat() || !IsValid(Source) || Source==this || (!Source->FactionId.IsNone() && Source->FactionId==FactionId)) continue;
		const FVector Offset=It->GetActorLocation()-GetActorLocation();
		const FVector Velocity=It->Movement->Velocity-GetVelocity();
		const float SpeedSquared=Velocity.SizeSquared();
		if (SpeedSquared<1.f || FVector::DotProduct(Offset,Velocity)>=0) continue;
		const float Closest=-FVector::DotProduct(Offset,Velocity)/SpeedSquared;
		const FVector Near=Offset+Velocity*Closest;
		const float Radius=GetCapsuleComponent()->GetScaledCapsuleRadius()+It->Collision->GetScaledSphereRadius()+8.f;
		const float HalfHeight=GetCapsuleComponent()->GetScaledCapsuleHalfHeight()+It->Collision->GetScaledSphereRadius();
		const float Arrival=FMath::Max(0.f,Closest-Radius/FMath::Sqrt(SpeedSquared));
		if (Near.SizeSquared2D()>FMath::Square(Radius) || FMath::Abs(Near.Z)>HalfHeight || Arrival>TimeToHit ||
			!HasClearPath(*It,It->GetActorLocation(),GetActorLocation())) continue;
		Threat=*It; TimeToHit=Arrival;
	}
	for (TActorIterator<ARPGCharacter> It(GetWorld()); It; ++It)
	{
		if (*It==this || It->bIsDead || (!It->FactionId.IsNone() && It->FactionId==FactionId) || !It->ActionComp) continue;
		float Arrival;
		if (It->ActionComp->GetTimeUntilMeleeThreat(this,Arrival) && Arrival<=TimeToHit &&
			HasClearPath(*It,It->GetActorLocation(),GetActorLocation())) { Threat=*It; TimeToHit=Arrival; }
	}
	if (!Threat) TimeToHit=-1.f;
	return Threat!=nullptr;
}

void ARPGPlayerCharacter::UpdateThreatFocus()
{
	const ARPGPlayerController* PC=Cast<ARPGPlayerController>(GetController());
	if (bIsDead || !bEnableThreatFocus || !PC || PC->ShouldBlockGameplayInput() || PC->IsPaused()) { SetFocusHeld(false); return; }
	AActor* Threat=nullptr; float HitTime=-1;
	FindIncomingThreat(Threat,HitTime); IncomingThreat=Threat; IncomingHitSeconds=HitTime;
	const double Now=GetWorld()->GetRealTimeSeconds();
	if (bThreatFocusActive)
	{
		if (!bFocusHeld || Now>=FocusEndsReal) StopThreatFocus();
		return;
	}
	// Global bullet-time is a standalone mechanic. Never slow unrelated network players.
	if (!bFocusHeld || !Threat || Now<NextFocusReal || GetNetMode()!=NM_Standalone) return;
	PreviousWorldDilation=UGameplayStatics::GetGlobalTimeDilation(this);
	if (!FMath::IsNearlyEqual(PreviousWorldDilation,1.f,.001f)) return;
	AppliedFocusScale=FMath::Clamp(FocusTimeScale,.05f,1.f);
	PreviousPawnDilation=CustomTimeDilation; PreviousControllerDilation=PC->CustomTimeDilation;
	FocusController=GetController();
	UGameplayStatics::SetGlobalTimeDilation(this,AppliedFocusScale);
	CustomTimeDilation=PreviousPawnDilation/AppliedFocusScale;
	GetController()->CustomTimeDilation=PreviousControllerDilation/AppliedFocusScale;
	bThreatFocusActive=true; FocusEndsReal=Now+FMath::Max(.05f,FocusMaxRealSeconds);
	ThreatFocusChanged(true,Threat);
}

void ARPGPlayerCharacter::StopThreatFocus()
{
	if (!bThreatFocusActive) return;
	if (GetWorld())
	{
		if (FMath::IsNearlyEqual(UGameplayStatics::GetGlobalTimeDilation(this),AppliedFocusScale,.001f))
			UGameplayStatics::SetGlobalTimeDilation(this,PreviousWorldDilation);
		NextFocusReal=GetWorld()->GetRealTimeSeconds()+FMath::Max(0.f,FocusCooldownRealSeconds);
	}
	if (FMath::IsNearlyEqual(CustomTimeDilation,PreviousPawnDilation/AppliedFocusScale,.001f)) CustomTimeDilation=PreviousPawnDilation;
	if (AController* PC=FocusController.Get())
		if (FMath::IsNearlyEqual(PC->CustomTimeDilation,PreviousControllerDilation/AppliedFocusScale,.001f)) PC->CustomTimeDilation=PreviousControllerDilation;
	bThreatFocusActive=false; IncomingThreat=nullptr; IncomingHitSeconds=-1;
	FocusController.Reset();
	ThreatFocusChanged(false,nullptr);
}
