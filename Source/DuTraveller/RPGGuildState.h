#pragma once

#include "GameFramework/Actor.h"
#include "InventoryComponent.h"
#include "RPGGuildState.generated.h"

class ARPGBuilding;
class ARPGPlayerState;
class URPGBuildingDefinition;

UENUM(BlueprintType) enum class ERPGGuildRole : uint8 { Leader, Officer, Member };
UENUM(BlueprintType, meta=(Bitflags, UseEnumValuesAsMaskValuesInEditor="true"))
enum class ERPGGuildPermission : uint8 { None=0, Deposit=1, Withdraw=2, PlaceBuilding=4, Harvest=8, ManageMembers=16 };
ENUM_CLASS_FLAGS(ERPGGuildPermission)

USTRUCT(BlueprintType)
struct FRPGGuildMember
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly, SaveGame) TObjectPtr<ARPGPlayerState> Player;
	UPROPERTY(BlueprintReadOnly, SaveGame) ERPGGuildRole Role = ERPGGuildRole::Member;
	UPROPERTY(BlueprintReadOnly, SaveGame, meta=(Bitmask, BitmaskEnum="/Script/DuTraveller.ERPGGuildPermission")) int32 Permissions = static_cast<int32>(ERPGGuildPermission::Deposit | ERPGGuildPermission::Harvest);
};

USTRUCT(BlueprintType)
struct FRPGGuildBankStack
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly, SaveGame) TSubclassOf<UItemObject> ItemClass;
	UPROPERTY(BlueprintReadOnly, SaveGame) int32 Quantity = 0;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FRPGGuildChanged);

/** Server-owned session guild, shared bank and settlement registry. */
UCLASS(BlueprintType, Blueprintable)
class DUTRAVELLER_API ARPGGuildState : public AActor
{
	GENERATED_BODY()
public:
	ARPGGuildState();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const override;
	UPROPERTY(BlueprintReadOnly, Replicated, SaveGame) FGuid GuildId;
	UPROPERTY(BlueprintReadOnly, Replicated, SaveGame) FGuid SettlementId;
	UPROPERTY(BlueprintReadOnly, Replicated, SaveGame) FName GuildName;
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_Guild, SaveGame) TArray<FRPGGuildMember> Members;
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_Guild, SaveGame) TArray<FRPGGuildBankStack> Bank;
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_Guild, SaveGame) TArray<TObjectPtr<ARPGBuilding>> Buildings;
	UPROPERTY(BlueprintReadOnly, Replicated, SaveGame) int32 Reputation = 0;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta=(ClampMin="1")) int32 BankStackCapacity = 100;
	UPROPERTY(BlueprintAssignable) FRPGGuildChanged OnGuildChanged;
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly) bool InitializeGuild(FName Name, ARPGPlayerState* Leader);
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly) bool AddMember(ARPGPlayerState* Requester, ARPGPlayerState* NewMember);
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly) bool Deposit(ARPGPlayerState* Requester, TSubclassOf<UItemObject> ItemClass, int32 Quantity);
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly) bool Withdraw(ARPGPlayerState* Requester, TSubclassOf<UItemObject> ItemClass, int32 Quantity);
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly) ARPGBuilding* PlaceBuilding(ARPGPlayerState* Requester, URPGBuildingDefinition* Definition, FTransform Transform);
	UFUNCTION(BlueprintPure) bool HasPermission(ARPGPlayerState* Player, ERPGGuildPermission Permission) const;
	UFUNCTION(BlueprintPure) int32 CountBankItem(TSubclassOf<UItemObject> ItemClass) const;
	static bool CanPlayerUseBuilding(ARPGPlayerState* Player, const FGuid& BuildingGuildId);
private:
	bool bMutating = false;
	bool RemoveBankCosts(const TArray<FRPGInventoryAmount>& Costs);
	UFUNCTION() void OnRep_Guild();
};
