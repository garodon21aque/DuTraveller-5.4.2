#include "DTAnimationRouter.h"
#include "RPGActionComponent.h"
#include "RPGCharacter.h"
#include "RPGPlayerCharacter.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Animation/Skeleton.h"
#include "Components/SkeletalMeshComponent.h"
#include "AbilitySystemBlueprintLibrary.h"

UDTAnimationRouter::UDTAnimationRouter() { PrimaryComponentTick.bCanEverTick = false; }

void UDTAnimationRouter::BeginPlay()
{
	Super::BeginPlay();
	if (ARPGCharacter* Character = Cast<ARPGCharacter>(GetOwner()))
		Character->ActionComp->OnAbilityCommitted.AddUniqueDynamic(this, &ThisClass::AbilityCommitted);
}

void UDTAnimationRouter::EndPlay(const EEndPlayReason::Type Reason)
{
	if (BoundAnimInstance) BoundAnimInstance->OnMontageEnded.RemoveAll(this);
	if (ARPGCharacter* Character = Cast<ARPGCharacter>(GetOwner()))
		Character->ActionComp->OnAbilityCommitted.RemoveAll(this);
	bComboWindowOpen = false;
	Super::EndPlay(Reason);
}

bool UDTAnimationRouter::IsTraversalOrExternalMontageActive() const
{
	const ACharacter* Character = Cast<ACharacter>(GetOwner());
	const UAnimInstance* Anim = Character && Character->GetMesh() ? Character->GetMesh()->GetAnimInstance() : nullptr;
	return Anim && Anim->IsAnyMontagePlaying() &&
		(!ActiveCombatMontage || Anim->GetCurrentActiveMontage() != ActiveCombatMontage);
}

bool UDTAnimationRouter::CanPlayCombatAnimation(const FDTCombatAnimation& Animation) const
{
	const ACharacter* Character = Cast<ACharacter>(GetOwner());
	UAnimInstance* Anim = Character && Character->GetMesh() ? Character->GetMesh()->GetAnimInstance() : nullptr;
	if (!Anim || !Animation.Montage || IsTraversalOrExternalMontageActive() ||
		!FMath::IsFinite(Animation.PlayRate) || Animation.PlayRate <= 0.f) return false;
	// AI decisions must not replace a hit reaction on their next decision tick.
	if (HitReaction.Montage && ActiveCombatMontage==HitReaction.Montage &&
		Animation.Montage!=HitReaction.Montage && Anim->Montage_IsPlaying(HitReaction.Montage)) return false;
	// A mismatched old RPG skeleton must be retargeted before entering DT's graph.
	if (!Anim->CurrentSkeleton || Anim->CurrentSkeleton != Animation.Montage->GetSkeleton()) return false;
	if (!Animation.StartSection.IsNone() && Animation.Montage->GetSectionIndex(Animation.StartSection) == INDEX_NONE) return false;
	TSet<FName> Slots;
	bool bHasSequence = false;
	for (const FSlotAnimationTrack& Track : Animation.Montage->SlotAnimTracks)
	{
		if (Anim->CurrentSkeleton->GetSlotGroupName(Track.SlotName) != Animation.Montage->GetGroupName()) return false;
		if (Slots.Contains(Track.SlotName))
		{
			UE_LOG(LogRPGCombat, Error, TEXT("[Animation] Duplicate montage slot %s in %s"), *Track.SlotName.ToString(), *Animation.Montage->GetPathName());
			return false;
		}
		Slots.Add(Track.SlotName);
		for (const FAnimSegment& Segment : Track.AnimTrack.AnimSegments)
		{
			const UAnimSequenceBase* Sequence = Segment.GetAnimReference();
			if (!Sequence || Sequence->GetSkeleton() != Animation.Montage->GetSkeleton()) return false;
			bHasSequence = true;
		}
	}
	return !Slots.IsEmpty() && bHasSequence;
}

bool UDTAnimationRouter::PlayCombatAnimation(FDTCombatAnimation Animation)
{
	if (!CanPlayCombatAnimation(Animation)) return false;
	const ACharacter* Character = Cast<ACharacter>(GetOwner());
	UAnimInstance* Anim = Character->GetMesh()->GetAnimInstance();
	// The authoritative committed-action replication confirms a locally
	// predicted montage. Do not restart the same presentation on confirmation.
	if (ActiveCombatMontage == Animation.Montage && Anim->Montage_IsPlaying(Animation.Montage)) return true;
	if (BoundAnimInstance != Anim)
	{
		if (BoundAnimInstance) BoundAnimInstance->OnMontageEnded.RemoveAll(this);
		BoundAnimInstance = Anim;
		Anim->OnMontageEnded.AddUniqueDynamic(this, &ThisClass::MontageEnded);
	}
	bComboWindowOpen = false;
	// Stop only this router's preceding combat montage, never the traversal group.
	if (ActiveCombatMontage) Anim->Montage_Stop(0.1f, ActiveCombatMontage);
	const float Length = Anim->Montage_Play(Animation.Montage, Animation.PlayRate, EMontagePlayReturnType::MontageLength, 0.f, false);
	if (Length <= 0.f) return false;
	ActiveCombatMontage = Animation.Montage;
	if (!Animation.StartSection.IsNone()) Anim->Montage_JumpToSection(Animation.StartSection, Animation.Montage);
	return true;
}

void UDTAnimationRouter::AbilityCommitted(URPGCombatAbilityDefinition* Ability, AActor* Target)
{
	if (!Ability) return;
	OnCombatAnimationRequested.Broadcast(Ability, Target);
	if (GetNetMode() == NM_DedicatedServer) return;
	if (const FDTCombatAnimation* Animation = CombatAnimations.Find(Ability->AbilityId))
	{
		const bool bStarted = PlayCombatAnimation(*Animation);
		if (!bStarted && GetOwner()->HasAuthority())
			if (ARPGCharacter* Character = Cast<ARPGCharacter>(GetOwner())) Character->ActionComp->CancelPendingRelease(TEXT("montage playback failed"));
		UE_LOG(LogRPGCombat, Log, TEXT("[Animation] Casting montage %s for ability=%s"),
		       bStarted ? TEXT("started") : TEXT("FAILED"), *Ability->AbilityId.ToString());
	}
	else
	{
		UE_LOG(LogRPGCombat, Warning, TEXT("[Animation] No combat montage mapped for ability=%s"), *Ability->AbilityId.ToString());
	}
}

void UDTAnimationRouter::PlayHitReaction()
{
	if (ARPGPlayerCharacter* Player=Cast<ARPGPlayerCharacter>(GetOwner())) if (Player->bIsDodging) Player->CancelAvoidance();
	if (!CanPlayCombatAnimation(HitReaction)) return;
	if (ARPGCharacter* Character=Cast<ARPGCharacter>(GetOwner()))
		if (Character->ActionComp) Character->ActionComp->CancelPendingRelease(TEXT("hit reaction"));
	PlayCombatAnimation(HitReaction);
}

void UDTAnimationRouter::StopActiveMontage(const float BlendOut)
{
	if (BoundAnimInstance && ActiveCombatMontage) BoundAnimInstance->Montage_Stop(BlendOut, ActiveCombatMontage);
}

void UDTAnimationRouter::MontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
	// A hit reaction can replace the active montage before the old cast finishes
	// blending out. Its end still has to clear that cast's pending release.
	if (ARPGCharacter* Character = Cast<ARPGCharacter>(GetOwner()))
		if (Character->ActionComp && GetOwner()->HasAuthority())
			Character->ActionComp->NotifyMontageFinished(Montage, bInterrupted);
	if (Montage == ActiveCombatMontage)
	{
		ActiveCombatMontage = nullptr;
		bComboWindowOpen = false;
	}
}

void UDTAnimationRouter::RouteAnimationEvent(FGameplayTag EventTag, bool bWindowOpen)
{
	if (!EventTag.IsValid() || !ActiveCombatMontage || !BoundAnimInstance ||
		!BoundAnimInstance->Montage_IsActive(ActiveCombatMontage)) return;
	OnAnimationEvent.Broadcast(EventTag, bWindowOpen);
	// Cosmetic client notifies never directly mutate authoritative gameplay.
	if (GetOwner()->HasAuthority())
	{
		if (ARPGCharacter* Character = Cast<ARPGCharacter>(GetOwner()))
			if (Character->ActionComp) Character->ActionComp->HandleAnimationEvent(EventTag);
		FGameplayEventData Payload;
		Payload.EventTag = EventTag;
		Payload.Instigator = GetOwner();
		Payload.Target = GetOwner();
		Payload.EventMagnitude = bWindowOpen ? 1.f : 0.f;
		UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(GetOwner(), EventTag, Payload);
	}
}

void UDTAnimationRouter::SetComboWindow(bool bOpen)
{
	if (!ActiveCombatMontage) return;
	bComboWindowOpen = bOpen;
	RouteAnimationEvent(FGameplayTag::RequestGameplayTag(TEXT("Event.Animation.ComboWindow"), false), bOpen);
}

void UDTAnimationRouter::SetMeleeWindow(const bool bOpen)
{
	if (!ActiveCombatMontage) return;
	if (ARPGCharacter* Character = Cast<ARPGCharacter>(GetOwner()))
		if (Character->ActionComp) Character->ActionComp->SetMeleeWindowOpen(bOpen);
}

UAnimMontage* UDTAnimationRouter::FindCombatMontage(const FName AbilityId) const
{
	const FDTCombatAnimation* Entry = CombatAnimations.Find(AbilityId);
	return Entry ? Entry->Montage.Get() : nullptr;
}

bool UDTAnimationRouter::FindCombatAnimation(const FName AbilityId, FDTCombatAnimation& OutAnimation) const
{
	if (const FDTCombatAnimation* Entry = CombatAnimations.Find(AbilityId))
	{
		OutAnimation = *Entry;
		return true;
	}
	return false;
}

float UDTAnimationRouter::FindNotifyTriggerTime(const UAnimMontage* Montage, const FGameplayTag& EventTag)
{
	if (!Montage || !EventTag.IsValid()) return -1.f;
	for (const FAnimNotifyEvent& Event : Montage->Notifies)
	{
		if (const UDTAnimNotifyEvent* Notify = Cast<UDTAnimNotifyEvent>(Event.Notify))
			if (Notify->EventTag == EventTag) return Event.GetTriggerTime();
	}
	return -1.f;
}

void UDTAnimationRouter::FindMeleeWindows(const UAnimMontage* Montage, TArray<FVector2D>& OutWindows)
{
	OutWindows.Reset();
	if (!Montage) return;
	for (const FAnimNotifyEvent& Event : Montage->Notifies)
	{
		if (Cast<UDTAnimNotifyMeleeWindow>(Event.NotifyStateClass))
			OutWindows.Add(FVector2D(Event.GetTriggerTime(), Event.GetEndTriggerTime()));
	}
}

namespace
{
UDTAnimationRouter* RouterForMesh(USkeletalMeshComponent* Mesh)
{
	const ACharacter* Owner = Mesh ? Cast<ACharacter>(Mesh->GetOwner()) : nullptr;
	return Owner && Owner->GetMesh() == Mesh ? Owner->FindComponentByClass<UDTAnimationRouter>() : nullptr;
}
}
void UDTAnimNotifyEvent::Notify(USkeletalMeshComponent* Mesh, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& Reference)
{
	Super::Notify(Mesh, Animation, Reference);
	if (UDTAnimationRouter* Router = RouterForMesh(Mesh)) Router->RouteAnimationEvent(EventTag, true);
}
void UDTAnimNotifyComboWindow::NotifyBegin(USkeletalMeshComponent* Mesh, UAnimSequenceBase* Animation, float Duration, const FAnimNotifyEventReference& Reference)
{
	Super::NotifyBegin(Mesh, Animation, Duration, Reference);
	if (UDTAnimationRouter* Router = RouterForMesh(Mesh)) Router->SetComboWindow(true);
}
void UDTAnimNotifyComboWindow::NotifyEnd(USkeletalMeshComponent* Mesh, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& Reference)
{
	Super::NotifyEnd(Mesh, Animation, Reference);
	if (UDTAnimationRouter* Router = RouterForMesh(Mesh)) Router->SetComboWindow(false);
}
void UDTAnimNotifyMeleeWindow::NotifyBegin(USkeletalMeshComponent* Mesh, UAnimSequenceBase* Animation, float Duration, const FAnimNotifyEventReference& Reference)
{
	Super::NotifyBegin(Mesh, Animation, Duration, Reference);
	if (UDTAnimationRouter* Router = RouterForMesh(Mesh)) Router->SetMeleeWindow(true);
}
void UDTAnimNotifyMeleeWindow::NotifyEnd(USkeletalMeshComponent* Mesh, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& Reference)
{
	Super::NotifyEnd(Mesh, Animation, Reference);
	if (UDTAnimationRouter* Router = RouterForMesh(Mesh)) Router->SetMeleeWindow(false);
}
