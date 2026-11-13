#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "ItemObject.h"
#include "AbilitySystemInterface.h"
#include "GameplayEffectTypes.h"
#include "RPGCharacter.generated.h"

class UInventoryComponent;
class URPGAttributeComponent;
class URPGActionComponent;
class URPGCraftingComponent;
class URPGQuestComponent;
class UEquipment_ItemObject;
class USceneCaptureComponent2D;
class UAbilitySystemComponent;
class UGameplayEffect;
class URPGAttributeSet;
class ARPGPickupItem;
class ARPGVendor;
class UDTAnimationRouter;
class UTextureRenderTarget2D;

USTRUCT(BlueprintType)
struct FRPGEquippedItem
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly) EEquipableSlot Slot = EEquipableSlot::EIS_None;
	UPROPERTY(BlueprintReadOnly) FGuid ItemId;
	UPROPERTY(BlueprintReadOnly) TSubclassOf<UEquipment_ItemObject> ItemClass;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnEquipedItemsChangedSignature);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FRPGHealthChangedSignature, float, NewValue);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FRPGCharacterDeathSignature, ARPGCharacter*, Character, AActor*, Killer);

/**
 * Legacy DuTraveller character base reconstructed from serialized Blueprint
 * contracts. Owns the shared inventory/equipment/preview-capture API surface
 * that inventory, equipment, crafting, and drop widgets call through
 * `As RPGCharacter` / `As BP Player Character` casts.
 */
UCLASS(BlueprintType, Blueprintable)
class DUTRAVELLER_API ARPGCharacter : public ACharacter, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	ARPGCharacter();
	virtual void BeginPlay() override;
	virtual void PossessedBy(AController* NewController) override;
	virtual void OnRep_PlayerState() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	URPGAttributeSet* GetRPGAttributes() const;
	UFUNCTION(BlueprintPure, Category="Equipment") bool IsItemEquipped(const UItemObject* Item) const;
	bool TryEquipItem(UEquipment_ItemObject* Item, EEquipableSlot Slot);
	bool TryUnequipItem(EEquipableSlot Slot);

	/** Public equipment identity for appearance; private inventory instances remain owner-only. */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_Equipment, Category="Equipment")
	TArray<FRPGEquippedItem> ReplicatedEquipment;
	UFUNCTION(BlueprintImplementableEvent, Category="Equipment")
	void EquipmentAppearanceChanged();
	/** Author starting gear on the canonical character Blueprint. Granted once per new pawn. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Equipment|Starting Loadout")
	TArray<TSubclassOf<UEquipment_ItemObject>> StartingEquipment;
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Equipment|Starting Loadout") void InitializeStartingEquipment();
	UFUNCTION(BlueprintCallable, Category="Equipment|Appearance") void RefreshEquipmentMeshes();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inventory")
	TObjectPtr<UInventoryComponent> InventoryComp;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Attributes")
	TObjectPtr<URPGAttributeComponent> AttributeComp;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Actions")
	TObjectPtr<URPGActionComponent> ActionComp;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="DT|Animation")
	TObjectPtr<UDTAnimationRouter> AnimationRouter;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Crafting")
	TObjectPtr<URPGCraftingComponent> CraftingComp;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Quests")
	TObjectPtr<URPGQuestComponent> QuestComp;

	UPROPERTY(BlueprintReadWrite, Category = "State")
	bool bIsStunned = false;

	UPROPERTY(BlueprintReadWrite, Category = "State")
	bool InShockLoop = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Replicated, Category="Combat")
	FName FactionId = FName("Player");

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_Death, Category="Combat")
	bool bIsDead = false;

	UPROPERTY(BlueprintAssignable, Category="Combat")
	FRPGCharacterDeathSignature OnCharacterDeath;

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Combat")
	bool ReceiveCombatDamage(float Damage, AActor* DamageCauser, FName AbilityId, bool bCriticalHit = false, bool bBlockedHit = false,
		TSubclassOf<UGameplayEffect> DamageEffectOverride = nullptr);
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Combat") bool RespawnAt(const FTransform& SpawnTransform);
	UFUNCTION(NetMulticast, Unreliable) void MulticastSkillFX(class UNiagaraSystem* Effect, FVector Location);
	/** Same as MulticastSkillFX but bounds looping systems after LifeSeconds. */
	UFUNCTION(NetMulticast, Unreliable) void MulticastSkillFXTimed(class UNiagaraSystem* Effect, FVector Location, float LifeSeconds);
	/** Spawns one beam segment per consecutive pair of world-space points (chain lightning style). */
	UFUNCTION(NetMulticast, Unreliable) void MulticastChainFX(class UNiagaraSystem* Effect, const TArray<FVector_NetQuantize>& Points, float LifeSeconds);
	/** Floating combat text at a resolved, authoritative hit location. */
	UFUNCTION(NetMulticast, Unreliable) void MulticastDamageText(float Damage, bool bBlockedHit, bool bCriticalHit, FVector_NetQuantize Location);

	/** Optional override for the world-space damage popup component (defaults to BP_DamageTextComponent). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|DamageText")
	TSubclassOf<class URPG_DamageTextComponent> DamageTextComponentClass;

	UFUNCTION(BlueprintImplementableEvent, Category="Combat")
	void DeathPresentation(AActor* Killer);

	UPROPERTY(VisibleAnywhere, Category = "Equipment|Preview")
	TObjectPtr<USceneCaptureComponent2D> FrontCapture;

	UPROPERTY(VisibleAnywhere, Category = "Equipment|Preview")
	TObjectPtr<USceneCaptureComponent2D> FaceCapture;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="DT|Cameras") TObjectPtr<USceneCaptureComponent2D> MinimapCapture;
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="DT|Cameras") bool bEnableMinimap = true;
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="DT|Cameras") FVector PreviewCameraOffset = FVector(250.f, 0.f, 0.f);
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="DT|Cameras") FVector FaceCameraOffset = FVector(85.f, 0.f, 60.f);
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="DT|Cameras", meta=(ClampMin="100")) float MinimapHeight = 1500.f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="DT|Cameras", meta=(ClampMin="100")) float MinimapWidth = 2500.f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="DT|Cameras") TObjectPtr<UTextureRenderTarget2D> PlayerPreviewTarget;
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="DT|Cameras") TObjectPtr<UTextureRenderTarget2D> FacePreviewTarget;
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="DT|Cameras") TObjectPtr<UTextureRenderTarget2D> MinimapTarget;
	UFUNCTION(BlueprintCallable, Category="DT|Cameras") void InitializePlayerCaptures();

	UPROPERTY(BlueprintAssignable, Category = "Equipment")
	FOnEquipedItemsChangedSignature OnEquipedItemsChanged;

	UPROPERTY(BlueprintAssignable, Category = "Attributes")
	FRPGHealthChangedSignature OnHealthChanged;

	UPROPERTY(BlueprintAssignable, Category = "Attributes")
	FRPGHealthChangedSignature OnMaxHealthChanged;

	UPROPERTY(BlueprintReadWrite, Category = "Equipment")
	TMap<EEquipableSlot, TObjectPtr<UEquipment_ItemObject>> EquippedItems;

	/**
	 * Legacy contract retained by WBP_PlayerStat's serialized call node
	 * (CallFunc_Initialize_PC: self=Target typed RPGCharacter, param `PC`).
	 * Original implementation is unrecovered; parity bridge only.
	 */
	UFUNCTION(BlueprintCallable, Category = "Character")
	void Initialize(UObject* PC);

	UFUNCTION(BlueprintCallable, Category = "Inventory")
	void UseItem(UItemObject* Item);

	UFUNCTION(BlueprintCallable, Category = "Inventory")
	void DropItem(UItemObject* Item, int32 Quantity);

	UFUNCTION(BlueprintCallable, Category="Inventory")
	bool CollectPickup(ARPGPickupItem* Pickup);
	UFUNCTION(BlueprintCallable, Category="Vendor")
	void PurchaseFromVendor(ARPGVendor* Vendor, FName OfferId, int32 PurchaseCount = 1);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Inventory")
	TSubclassOf<ARPGPickupItem> PickupClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Inventory", meta=(ClampMin="0.0"))
	float DropForwardDistance = 120.f;

	UFUNCTION(BlueprintCallable, Category = "Equipment|Preview")
	void FrontCaptureOn();

	UFUNCTION(BlueprintCallable, Category = "Equipment|Preview")
	void FrontCaptureOff();

	UFUNCTION(BlueprintCallable, Category = "Equipment|Preview")
	void FaceCaptureOn();

	UFUNCTION(BlueprintCallable, Category = "Equipment|Preview")
	void FaceCaptureOff();

	UFUNCTION(BlueprintPure, Category = "Equipment")
	TMap<EEquipableSlot, UEquipment_ItemObject*> GetEquippedItems() const;

	UFUNCTION(BlueprintCallable, Category = "Equipment")
	void EquipItem(UEquipment_ItemObject* Item, EEquipableSlot Slot);

	UFUNCTION(BlueprintCallable, Category = "Equipment")
	void UnequipItem(EEquipableSlot Slot);

	/** Enables collision only on equipped weapon meshes for an authored melee notify window. */
	void SetWeaponCollisionEnabled(bool bEnabled);

private:
	TWeakObjectPtr<AActor> PendingDamageCauser;
	bool bDeathPoseApplied = false;
	FTransform LivingMeshTransform;
	FName LivingMeshCollisionProfile;
	void HandleDeath(AActor* Killer);
	void ApplyDeathPose();
	void RestoreLivingPose();
	class UNiagaraComponent* SpawnBoundedNiagara(class UNiagaraSystem* Effect, const FVector& Location, float LifeSeconds);
	static void AssignBeamEndpoints(class UNiagaraComponent* FX, const FVector& Start, const FVector& End);
	bool bStartingEquipmentInitialized = false;
	UPROPERTY(Transient) TArray<TObjectPtr<class UStaticMeshComponent>> EquipmentMeshes;
	UPROPERTY(Transient) TArray<TObjectPtr<class UPrimitiveComponent>> WeaponCollisionComponents;
	UPROPERTY(VisibleAnywhere, Category="GAS") TObjectPtr<UAbilitySystemComponent> FallbackAbilitySystem;
	UPROPERTY() TObjectPtr<URPGAttributeSet> FallbackAttributes;
	TWeakObjectPtr<UAbilitySystemComponent> BoundAbilitySystem;
	FActiveGameplayEffectHandle EquipmentEffect;
	FActiveGameplayEffectHandle ResourceRegenEffect;
	FActiveGameplayEffectHandle DerivedAttributeEffect;
	void InitializeAbilitySystem();
	void RefreshLegacyAttributes(const FOnAttributeChangeData& Data);
	bool ApplyEquipmentState(const TMap<EEquipableSlot, TObjectPtr<UEquipment_ItemObject>>& NewEquipment);
	UFUNCTION() void OnRep_Equipment();
	UFUNCTION(Server, Reliable) void ServerEquipItem(UEquipment_ItemObject* Item, EEquipableSlot Slot);
	UFUNCTION(Server, Reliable) void ServerUnequipItem(EEquipableSlot Slot);
	UFUNCTION(Server, Reliable) void ServerDropItem(UItemObject* Item, int32 Quantity);
	UFUNCTION(Server, Reliable) void ServerCollectPickup(ARPGPickupItem* Pickup);
	UFUNCTION(Server, Reliable) void ServerPurchaseFromVendor(ARPGVendor* Vendor, FName OfferId, int32 PurchaseCount);
	bool TryDropItem(UItemObject* Item, int32 Quantity);
	UFUNCTION() void OnRep_Death();
	UFUNCTION() void OnWeaponCollisionBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComponent, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);
	UFUNCTION(NetMulticast, Unreliable) void MulticastHitReaction();
};
