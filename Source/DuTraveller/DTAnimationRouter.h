#pragma once

#include "Components/ActorComponent.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "GameplayTagContainer.h"
#include "DTAnimationRouter.generated.h"

class UAnimMontage;
class UAnimInstance;
class URPGCombatAbilityDefinition;

USTRUCT(BlueprintType)
struct FDTCombatAnimation
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite) TObjectPtr<UAnimMontage> Montage;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FName StartSection;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0.01")) float PlayRate = 1.f;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FDTAnimationEvent, FGameplayTag, EventTag, bool, bWindowOpen);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FDTCombatAnimationRequested, URPGCombatAbilityDefinition*, Ability, AActor*, Target);

/** Shared presentation entry point for players and AI; the current AnimBP owns locomotion. */
UCLASS(ClassGroup=(DT), BlueprintType, meta=(BlueprintSpawnableComponent))
class DUTRAVELLER_API UDTAnimationRouter : public UActorComponent
{
	GENERATED_BODY()
public:
	UDTAnimationRouter();
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	/** Map ability identity to a montage already retargeted to this character's skeleton. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="DT|Animation") TMap<FName, FDTCombatAnimation> CombatAnimations;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="DT|Animation") FDTCombatAnimation HitReaction;
	UPROPERTY(BlueprintReadOnly, Category="DT|Animation") bool bComboWindowOpen = false;
	UPROPERTY(BlueprintReadOnly, Category="DT|Animation") TObjectPtr<UAnimMontage> ActiveCombatMontage;
	UPROPERTY(BlueprintAssignable, Category="DT|Animation") FDTAnimationEvent OnAnimationEvent;
	UPROPERTY(BlueprintAssignable, Category="DT|Animation") FDTCombatAnimationRequested OnCombatAnimationRequested;
	UFUNCTION(BlueprintCallable, Category="DT|Animation") bool PlayCombatAnimation(FDTCombatAnimation Animation);
	bool CanPlayCombatAnimation(const FDTCombatAnimation& Animation) const;
	UFUNCTION(BlueprintCallable, Category="DT|Animation") void RouteAnimationEvent(FGameplayTag EventTag, bool bWindowOpen);
	UFUNCTION(BlueprintCallable, Category="DT|Animation") void SetComboWindow(bool bOpen);
	/** Forwards authored melee collision windows to the authoritative action component. */
	UFUNCTION(BlueprintCallable, Category="DT|Animation") void SetMeleeWindow(bool bOpen);
	UFUNCTION(BlueprintPure, Category="DT|Animation") bool IsTraversalOrExternalMontageActive() const;
	UFUNCTION(BlueprintCallable, Category="DT|Animation") void PlayHitReaction();
	/** Stops only this router's active combat montage (interruption, death, unequip). */
	UFUNCTION(BlueprintCallable, Category="DT|Animation") void StopActiveMontage(float BlendOut = 0.15f);
	UFUNCTION(BlueprintPure, Category="DT|Animation") UAnimMontage* FindCombatMontage(FName AbilityId) const;
	UFUNCTION(BlueprintPure, Category="DT|Animation") bool FindCombatAnimation(FName AbilityId, FDTCombatAnimation& OutAnimation) const;
	/** Authored absolute trigger time of the first UDTAnimNotifyEvent carrying this tag, or -1. */
	static float FindNotifyTriggerTime(const UAnimMontage* Montage, const FGameplayTag& EventTag);
	/** Authored melee window notify-state spans as absolute [start,end] montage times. */
	static void FindMeleeWindows(const UAnimMontage* Montage, TArray<FVector2D>& OutWindows);
private:
	UPROPERTY(Transient) TObjectPtr<UAnimInstance> BoundAnimInstance;
	UFUNCTION() void AbilityCommitted(URPGCombatAbilityDefinition* Ability, AActor* Target);
	UFUNCTION() void MontageEnded(UAnimMontage* Montage, bool bInterrupted);
};

/** Routes an authored montage event to the same animation component and GAS avatar. */
UCLASS(meta=(DisplayName="DT Animation Event"))
class DUTRAVELLER_API UDTAnimNotifyEvent : public UAnimNotify
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, Category="DT|Animation") FGameplayTag EventTag;
	virtual void Notify(USkeletalMeshComponent* Mesh, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& Reference) override;
};

UCLASS(meta=(DisplayName="DT Combo Window"))
class DUTRAVELLER_API UDTAnimNotifyComboWindow : public UAnimNotifyState
{
	GENERATED_BODY()
public:
	virtual void NotifyBegin(USkeletalMeshComponent* Mesh, UAnimSequenceBase* Animation, float Duration, const FAnimNotifyEventReference& Reference) override;
	virtual void NotifyEnd(USkeletalMeshComponent* Mesh, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& Reference) override;
};

/** Authored window during which the equipped weapon's swept collision can damage targets. */
UCLASS(meta=(DisplayName="DT Melee Window"))
class DUTRAVELLER_API UDTAnimNotifyMeleeWindow : public UAnimNotifyState
{
	GENERATED_BODY()
public:
	virtual void NotifyBegin(USkeletalMeshComponent* Mesh, UAnimSequenceBase* Animation, float Duration, const FAnimNotifyEventReference& Reference) override;
	virtual void NotifyEnd(USkeletalMeshComponent* Mesh, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& Reference) override;
};
