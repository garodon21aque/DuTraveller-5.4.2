#pragma once

#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "ActiveGameplayEffectHandle.h"
#include "RPGAction.h"
#include "RPGCombatTypes.h"
#include "RPGActionComponent.generated.h"

class ARPGCharacter;
class UAnimMontage;
class UAbilitySystemComponent;

USTRUCT(BlueprintType)
struct FRPGReplicatedActionState
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly) FName AbilityId;
	UPROPERTY(BlueprintReadOnly) TObjectPtr<URPGCombatAbilityDefinition> Definition;
	UPROPERTY(BlueprintReadOnly) TObjectPtr<AActor> Target;
	UPROPERTY(BlueprintReadOnly) float ServerStartTime = 0.f;
	UPROPERTY(BlueprintReadOnly) int32 Sequence = 0;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FRPGAbilityCommittedSignature, URPGCombatAbilityDefinition*, Ability, AActor*, Target);
DECLARE_MULTICAST_DELEGATE_TwoParams(FRPGAbilityExecutionFinishedNative, FName, bool);

/** Container for the legacy action/effect system consumed by world effect widgets and trainer UI. */
UCLASS(ClassGroup = (Actions), BlueprintType, Blueprintable, meta = (BlueprintSpawnableComponent))
class DUTRAVELLER_API URPGActionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	URPGActionComponent();
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(EditAnywhere, Instanced, BlueprintReadOnly, Category = "Actions")
	TArray<TObjectPtr<URPGAction>> Actions;

	UPROPERTY(BlueprintAssignable, Category = "Actions")
	FOnActionStateChangedSignature OnActionStateChanged;

	UPROPERTY(BlueprintAssignable, Category = "Actions")
	FOnActionStoppedSignature OnActionStopped;

	/** Compact public presentation state. Individual action UObject instances are not replicated. */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_CommittedAction, Category="Actions")
	FRPGReplicatedActionState CommittedAction;

	UPROPERTY(BlueprintAssignable, Category="Actions")
	FRPGAbilityCommittedSignature OnAbilityCommitted;

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Actions")
	bool TryActivateAbility(URPGCombatAbilityDefinition* Ability, int32 Rank, ARPGCharacter* Target);
	/** GAS-owned entry point: validation/execution stay here, while the active GameplayAbility owns cost/cooldown. */
	bool TryActivateAbilityFromGameplayAbility(URPGCombatAbilityDefinition* Ability, int32 Rank, ARPGCharacter* Target);
	bool CanActivateAbility(const URPGCombatAbilityDefinition* Ability, int32 Rank, ARPGCharacter* Target,
		FString& OutFailure, bool bIgnoreCostAndCooldown = false, bool bAllowClientPrediction = false);
	FRPGAbilityExecutionFinishedNative OnAbilityExecutionFinishedNative;

	/** Authority entry point for UDTAnimNotifyEvent montage events. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Actions")
	void HandleAnimationEvent(FGameplayTag EventTag);

	/** Authority entry point for the authored melee window notify state; drives the swept trace. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Actions")
	void SetMeleeWindowOpen(bool bOpen);

	/** Receives authoritative overlap hits from the equipped weapon mesh. */
	void HandleWeaponCollision(AActor* OtherActor);
	void ApplyControlLock(float Duration);

	/** Authority entry point invoked by the animation router when the active montage finishes. */
	void NotifyMontageFinished(const UAnimMontage* Montage, bool bInterrupted);

	/** Cancels any unexecuted release (interruption, unequip, death). */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Actions")
	void CancelPendingRelease(FName Reason);
	void ResetAfterRespawn();

	/** Socket names swept while the melee window is open (e.g. weapon root + tip). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Combat|Trace")
	TArray<FName> MeleeTraceSockets;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Combat|Trace", meta=(ClampMin="1"))
	float MeleeTraceRadius = 20.f;

	/** Fallback release position (fraction of montage length) when no authored notify exists. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Combat|Trace", meta=(ClampMin="0.05", ClampMax="0.95"))
	float FallbackReleaseFraction = 0.6f;

	/** Shared damage mitigation pipeline used by releases, melee hits and projectiles. */
	static float ComputeMitigatedDamage(ARPGCharacter* Source, ARPGCharacter* Target,
	                                    const URPGCombatAbilityDefinition* Ability, float BaseDamage,
	                                    bool* bOutCriticalHit = nullptr, bool* bOutBlockedHit = nullptr);

	/** Per-ability cooldown tag derived from the definition id: Abilities.Fire.FireBolt -> Cooldown.Fire.FireBolt. */
	UFUNCTION(BlueprintPure, Category="Actions")
	static FGameplayTag CooldownTagForAbility(FName AbilityId);

	UFUNCTION(BlueprintPure, Category="Actions")
	float GetRemainingCooldown(FName AbilityId) const;
	/** Exposes a GAS-committed cooldown GE to legacy ID-based UI and validation. */
	void RegisterCooldownHandle(FName AbilityId, FActiveGameplayEffectHandle Handle);
	/** Reads the actual pending montage's next melee window, not a distance-only warning. */
	UFUNCTION(BlueprintPure, Category="Combat|Threat") bool GetTimeUntilMeleeThreat(const ARPGCharacter* Target, float& Seconds) const;

private:
	struct FPendingCombatRelease
	{
		TObjectPtr<const URPGCombatAbilityDefinition> Definition;
		TWeakObjectPtr<ARPGCharacter> Target;
		TObjectPtr<const UAnimMontage> Montage;
		float BaseDamage = 0.f;
		int32 Rank = 1;
		bool bReleased = false;
		bool bWindowHitApplied = false;
		explicit operator bool() const { return Definition != nullptr; }
	};

	UPROPERTY(Transient) TMap<FName, TObjectPtr<URPGAction>> AbilityActions;
	/** Server-authoritative map from ability id to its active cooldown effect; also the activation gate. */
	TMap<FName, FActiveGameplayEffectHandle> CooldownHandles;

	FPendingCombatRelease PendingRelease;
	TSet<TWeakObjectPtr<AActor>> PendingHitActors;
	TMap<FName, FVector> PrevSocketPositions;
	bool bMeleeWindowOpen = false;
	FTimerHandle ReleaseTimer;
	FTimerHandle ControlLockTimer;
	float PreLockWalkSpeed = 0.f;
	bool bControlLocked = false;
	void RestoreControl();
	TArray<FTimerHandle> WindowTimers;

	void BeginPendingRelease(const URPGCombatAbilityDefinition* Ability, int32 Rank, ARPGCharacter* Target, float BaseDamage);
	void ScheduleReleaseTiming(const URPGCombatAbilityDefinition* Ability, const UAnimMontage* Montage, float PlayRate);
	void ExecutePendingRelease();
	bool ApplyAbilityDamage(ARPGCharacter* Target, const URPGCombatAbilityDefinition* Ability, float BaseDamage, const FName SourceLabel);
	void SpawnProjectile(const URPGCombatAbilityDefinition* Ability, ARPGCharacter* Source, ARPGCharacter* Target, float BaseDamage, int32 Rank);
	bool IsValidMeleeTarget(ARPGCharacter* Source, ARPGCharacter* Target, const URPGCombatAbilityDefinition* Ability) const;
	void SweepMelee(ARPGCharacter* Source);
	bool TryActivateAbilityInternal(URPGCombatAbilityDefinition* Ability, int32 Rank, ARPGCharacter* Target);
	void BroadcastAbilityExecutionFinished(FName AbilityId, bool bCancelled);
	void ClearPendingState(bool bCancelled = false);

	UFUNCTION() void OnRep_CommittedAction();
	UFUNCTION() void ForwardActionChanged(URPGAction* Action);
	UFUNCTION() void ForwardActionStopped(URPGAction* Action);
};
