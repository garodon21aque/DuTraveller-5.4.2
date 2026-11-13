#pragma once

#include "RPGCharacter.h"
#include "DTAnimationRouter.h"
#include "RPGPlayerCharacter.generated.h"

/** Player branch of the unified DuTraveller character hierarchy. */
UCLASS(BlueprintType, Blueprintable)
class DUTRAVELLER_API ARPGPlayerCharacter : public ARPGCharacter
{
	GENERATED_BODY()

public:
	ARPGPlayerCharacter();
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual void FaceRotation(FRotator NewControlRotation, float DeltaTime) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Combat|Dodge") bool bEnableDodge = true;
	/** Full-body, in-place roll; the movement component supplies swept displacement. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Combat|Dodge") FDTCombatAnimation DodgeAnimation;
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Combat|Dodge", meta=(ClampMin="0.2")) float DodgeDuration = 1.f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Combat|Dodge", meta=(ClampMin="0")) float DodgeDistance = 500.f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Combat|Dodge", meta=(ClampMin="0")) float DodgeStaminaCost = 15.f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Combat|Dodge", meta=(ClampMin="0")) float DodgeCooldown = .25f;
	/** When enabled, the character cannot receive combat damage for the entire dodge animation. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Combat|Dodge") bool bDodgeInvulnerability = true;
	UPROPERTY(ReplicatedUsing=OnRep_Dodging, BlueprintReadOnly, Category="Combat|Dodge") bool bIsDodging = false;
	UFUNCTION(BlueprintCallable, Category="Combat|Dodge") bool RequestDodge(FVector WorldDirection);
	UFUNCTION(BlueprintPure, Category="Combat|Dodge") bool IsDodgeInvulnerable() const;
	UFUNCTION(BlueprintCallable, Category="Combat|Avoidance") void CancelAvoidance();
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Combat|Focus") bool bEnableThreatFocus = true;
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Combat|Focus", meta=(ClampMin="0.05",ClampMax="1")) float FocusTimeScale = .25f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Combat|Focus", meta=(ClampMin="0.05")) float ThreatLeadSeconds = .35f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Combat|Focus", meta=(ClampMin="0.05")) float FocusMaxRealSeconds = .65f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Combat|Focus", meta=(ClampMin="0")) float FocusCooldownRealSeconds = 1.f;
	UPROPERTY(BlueprintReadOnly, Category="Combat|Focus") bool bFocusHeld = false;
	UPROPERTY(BlueprintReadOnly, Category="Combat|Focus") bool bThreatFocusActive = false;
	UPROPERTY(BlueprintReadOnly, Category="Combat|Focus") TObjectPtr<AActor> IncomingThreat;
	UPROPERTY(BlueprintReadOnly, Category="Combat|Focus") float IncomingHitSeconds = -1.f;
	UFUNCTION(BlueprintCallable, Category="Combat|Focus") void SetFocusHeld(bool bHeld);
	UFUNCTION(BlueprintPure, Category="Combat|Focus") bool FindIncomingThreat(AActor*& Threat, float& TimeToHit) const;
	UFUNCTION(BlueprintImplementableEvent, Category="Combat|Focus") void ThreatFocusChanged(bool bActive, AActor* Threat);
private:
	UFUNCTION(Server, Reliable) void ServerDodge(FVector_NetQuantizeNormal Direction);
	UFUNCTION(Server, Reliable) void ServerCancelDodge();
	UFUNCTION(NetMulticast, Reliable) void MulticastDodge(FVector_NetQuantizeNormal Direction, float Duration);
	UFUNCTION() void OnRep_Dodging();
	UFUNCTION() void AvoidanceDeath(ARPGCharacter* Victim, AActor* Killer);
	bool StartDodge(FVector Direction);
	void FinishDodge();
	void StopThreatFocus();
	void UpdateThreatFocus();
	uint16 DodgeRootMotionId = 0;
	float DodgeElapsedSeconds = 0, ActiveDodgeDuration = 0;
	float SavedRootMotionTranslationScale = 1.f;
	bool bDodgeUsesAuthoredRootMotion = false;
	double NextDodgeReal = 0;
	double FocusEndsReal = 0, NextFocusReal = 0;
	bool bSavedOrientToMovement = false, bSavedDesiredRotation = false;
	float PreviousWorldDilation = 1.f, PreviousPawnDilation = 1.f, PreviousControllerDilation = 1.f;
	float AppliedFocusScale = 1.f;
	FVector DodgeHeading = FVector::ZeroVector;
	TWeakObjectPtr<AController> FocusController;
};
