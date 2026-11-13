#pragma once

#include "RPGNPCCharacter.h"
#include "RPGMountCharacter.generated.h"

class USceneComponent;
class USkeletalMeshComponent;
class UInputAction;
class UInputMappingContext;
struct FInputActionValue;

/** Rideable NPC specialization. Riding uses controller possession: the real
 *  rider pawn is hidden and parked while a cosmetic rider mesh is shown on the
 *  saddle, matching the legacy mount interaction restored from the old repo. */
UCLASS(BlueprintType, Blueprintable)
class DUTRAVELLER_API ARPGMountCharacter : public ARPGNPCCharacter
{
	GENERATED_BODY()

public:
	ARPGMountCharacter();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Scene marking where the cosmetic rider sits; reposition per mount mesh. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="NPC|Mount")
	TObjectPtr<USceneComponent> Saddle;

	/** World offset where the real rider reappears on dismount. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="NPC|Mount")
	TObjectPtr<USceneComponent> DetachPoint;

	/** Cosmetic stand-in shown on the saddle while a rider is mounted. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="NPC|Mount")
	TObjectPtr<USkeletalMeshComponent> MountedRiderMesh;

	/** Walk-speed multiplier applied to the mount while ridden. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="NPC|Mount", meta=(ClampMin="0.1"))
	float MountedSpeedMultiplier = 1.5f;

	/** Hidden, parked rider pawn currently controlling this mount (authority). */
	UPROPERTY(BlueprintReadOnly, Replicated, Category="NPC|Mount")
	TObjectPtr<ARPGCharacter> MountedRider;

	/** Authority-only; interaction Blueprints must route through the server. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="NPC|Mount")
	bool TryMount(ARPGCharacter* Rider);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="NPC|Mount")
	bool TryDismount();

	/** Optional mapping context added while mounted; the shared move/look/jump
	 *  input actions below bind through the active player mapping context. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="NPC|Mount|Input")
	TSoftObjectPtr<UInputMappingContext> MountedMappingContext;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="NPC|Mount|Input")
	TSoftObjectPtr<UInputAction> MoveInputAction = TSoftObjectPtr<UInputAction>(FSoftObjectPath("/Game/Input/IA_Move.IA_Move"));

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="NPC|Mount|Input")
	TSoftObjectPtr<UInputAction> LookInputAction = TSoftObjectPtr<UInputAction>(FSoftObjectPath("/Game/Input/IA_Look.IA_Look"));

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="NPC|Mount|Input")
	TSoftObjectPtr<UInputAction> JumpInputAction = TSoftObjectPtr<UInputAction>(FSoftObjectPath("/Game/Input/IA_Jump.IA_Jump"));

	/** Interact doubles as the dismount trigger until a dedicated action exists. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="NPC|Mount|Input")
	TSoftObjectPtr<UInputAction> DismountInputAction = TSoftObjectPtr<UInputAction>(FSoftObjectPath("/Game/Input/IA_Interact.IA_Interact"));

	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	virtual void PossessedBy(AController* NewController) override;
	virtual void UnPossessed() override;

	void MountMoveInput(const FInputActionValue& Value);
	void MountLookInput(const FInputActionValue& Value);
	void MountDismountInput(const FInputActionValue& Value);
	UFUNCTION(Server, Reliable) void ServerDismount();

private:
	float PreMountWalkSpeed = 0.f;
	bool bMountedContextAdded = false;
};
