#include "RPGGuildState.h"

#include "EngineUtils.h"
#include "Net/UnrealNetwork.h"
#include "RPGBuilding.h"
#include "RPGCharacter.h"
#include "RPGPlayerState.h"

ARPGGuildState::ARPGGuildState() { bReplicates = true; bAlwaysRelevant = true; }
void ARPGGuildState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps); DOREPLIFETIME(ARPGGuildState, GuildId); DOREPLIFETIME(ARPGGuildState, SettlementId);
	DOREPLIFETIME(ARPGGuildState, GuildName); DOREPLIFETIME(ARPGGuildState, Members); DOREPLIFETIME(ARPGGuildState, Bank);
	DOREPLIFETIME(ARPGGuildState, Buildings); DOREPLIFETIME(ARPGGuildState, Reputation);
}
void ARPGGuildState::OnRep_Guild() { OnGuildChanged.Broadcast(); }
bool ARPGGuildState::InitializeGuild(const FName Name, ARPGPlayerState* Leader)
{
	if (!HasAuthority() || GuildId.IsValid() || Name.IsNone() || !IsValid(Leader) || Leader->GuildState) return false;
	GuildId = FGuid::NewGuid(); SettlementId = FGuid::NewGuid(); GuildName = Name;
	FRPGGuildMember Entry; Entry.Player = Leader; Entry.Role = ERPGGuildRole::Leader; Entry.Permissions = 0xFF; Members.Add(Entry); Leader->GuildState = this;
	OnRep_Guild(); Leader->ForceNetUpdate(); ForceNetUpdate(); return true;
}
bool ARPGGuildState::HasPermission(ARPGPlayerState* Player, const ERPGGuildPermission Permission) const
{
	const FRPGGuildMember* Member = Members.FindByPredicate([&](const FRPGGuildMember& Entry){ return Entry.Player == Player; });
	return Member && (Member->Permissions & static_cast<int32>(Permission)) != 0;
}
bool ARPGGuildState::AddMember(ARPGPlayerState* Requester, ARPGPlayerState* NewMember)
{
	if (!HasAuthority() || !HasPermission(Requester, ERPGGuildPermission::ManageMembers) || !IsValid(NewMember) || NewMember->GuildState || Members.ContainsByPredicate([&](const FRPGGuildMember& E){return E.Player==NewMember;})) return false;
	FRPGGuildMember Entry; Entry.Player = NewMember; Members.Add(Entry); NewMember->GuildState = this; OnRep_Guild(); NewMember->ForceNetUpdate(); ForceNetUpdate(); return true;
}
int32 ARPGGuildState::CountBankItem(const TSubclassOf<UItemObject> ItemClass) const
{
	const FRPGGuildBankStack* Stack = Bank.FindByPredicate([&](const FRPGGuildBankStack& E){return E.ItemClass==ItemClass;}); return Stack ? Stack->Quantity : 0;
}
bool ARPGGuildState::Deposit(ARPGPlayerState* Requester, const TSubclassOf<UItemObject> ItemClass, const int32 Quantity)
{
	ARPGCharacter* Pawn = Requester ? Requester->GetPawn<ARPGCharacter>() : nullptr;
	if (!HasAuthority() || bMutating || !Pawn || !HasPermission(Requester, ERPGGuildPermission::Deposit) || !ItemClass || Quantity <= 0) return false;
	FRPGGuildBankStack* Existing = Bank.FindByPredicate([&](const FRPGGuildBankStack& E){return E.ItemClass==ItemClass;});
	if (!Existing && Bank.Num() >= BankStackCapacity) return false;
	if (Existing && Existing->Quantity > MAX_int32-Quantity) return false;
	FRPGInventoryAmount Amount; Amount.ItemClass=ItemClass; Amount.Quantity=Quantity; TGuardValue<bool> Guard(bMutating,true);
	if (!Pawn->InventoryComp->TryExchangeItems({Amount}, {})) return false;
	if (!Existing) { FRPGGuildBankStack New; New.ItemClass=ItemClass; Bank.Add(New); Existing=&Bank.Last(); }
	Existing->Quantity += Quantity; OnRep_Guild(); ForceNetUpdate(); return true;
}
bool ARPGGuildState::Withdraw(ARPGPlayerState* Requester, const TSubclassOf<UItemObject> ItemClass, const int32 Quantity)
{
	ARPGCharacter* Pawn = Requester ? Requester->GetPawn<ARPGCharacter>() : nullptr;
	FRPGGuildBankStack* Existing = Bank.FindByPredicate([&](const FRPGGuildBankStack& E){return E.ItemClass==ItemClass;});
	if (!HasAuthority() || bMutating || !Pawn || !HasPermission(Requester, ERPGGuildPermission::Withdraw) || !Existing || Quantity<=0 || Existing->Quantity<Quantity) return false;
	FRPGInventoryAmount Amount; Amount.ItemClass=ItemClass; Amount.Quantity=Quantity; TGuardValue<bool> Guard(bMutating,true);
	if (!Pawn->InventoryComp->TryExchangeItems({}, {Amount})) return false;
	Existing->Quantity-=Quantity; if(Existing->Quantity==0) Bank.RemoveAll([&](const FRPGGuildBankStack& E){return E.ItemClass==ItemClass;}); OnRep_Guild(); ForceNetUpdate(); return true;
}
bool ARPGGuildState::RemoveBankCosts(const TArray<FRPGInventoryAmount>& Costs)
{
	for(const FRPGInventoryAmount& Cost:Costs) if(!Cost.ItemClass || Cost.Quantity<=0 || CountBankItem(Cost.ItemClass)<Cost.Quantity) return false;
	for(const FRPGInventoryAmount& Cost:Costs){ for(FRPGGuildBankStack& Stack:Bank) if(Stack.ItemClass==Cost.ItemClass){Stack.Quantity-=Cost.Quantity;break;} }
	Bank.RemoveAll([](const FRPGGuildBankStack& E){return E.Quantity<=0;}); return true;
}
ARPGBuilding* ARPGGuildState::PlaceBuilding(ARPGPlayerState* Requester, URPGBuildingDefinition* Definition, const FTransform Transform)
{
	ARPGCharacter* Pawn=Requester?Requester->GetPawn<ARPGCharacter>():nullptr;
	if(!HasAuthority()||bMutating||!Pawn||!HasPermission(Requester,ERPGGuildPermission::PlaceBuilding)||!Definition||!Definition->IsConfigured()||Transform.ContainsNaN()||Transform.GetScale3D().ContainsNaN()||FVector::DistSquared(Pawn->GetActorLocation(),Transform.GetLocation())>FMath::Square(Definition->InteractionRange)) return nullptr;
	for(const FRPGInventoryAmount& Cost:Definition->ConstructionCosts) if(CountBankItem(Cost.ItemClass)<Cost.Quantity) return nullptr;
	TGuardValue<bool> Guard(bMutating,true); FActorSpawnParameters Params; Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::DontSpawnIfColliding;
	ARPGBuilding* Building=GetWorld()->SpawnActorDeferred<ARPGBuilding>(Definition->BuildingClass,Transform,this,nullptr,ESpawnActorCollisionHandlingMethod::DontSpawnIfColliding);
	if(!Building) return nullptr; Building->InitializeBuilding(GuildId,SettlementId,Definition); Building->FinishSpawning(Transform);
	if(!RemoveBankCosts(Definition->ConstructionCosts)){Building->Destroy();return nullptr;} Buildings.Add(Building); OnRep_Guild(); ForceNetUpdate(); return Building;
}
bool ARPGGuildState::CanPlayerUseBuilding(ARPGPlayerState* Player,const FGuid& BuildingGuildId)
{
	if(!IsValid(Player)||!BuildingGuildId.IsValid()) return false;
	for(TActorIterator<ARPGGuildState> It(Player->GetWorld());It;++It) if(It->GuildId==BuildingGuildId) return It->HasPermission(Player,ERPGGuildPermission::Harvest);
	return false;
}
