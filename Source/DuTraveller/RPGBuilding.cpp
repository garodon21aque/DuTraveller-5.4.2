#include "RPGBuilding.h"

#include "Net/UnrealNetwork.h"
#include "RPGCharacter.h"
#include "RPGGuildState.h"
#include "RPGPlayerState.h"

bool URPGBuildingDefinition::IsConfigured() const
{
	if (DefinitionId.IsNone() || !BuildingClass || MaximumLevel < 1 || !FMath::IsFinite(InteractionRange) || InteractionRange <= 0.f) return false;
	for (const FRPGInventoryAmount& Cost : ConstructionCosts) if (!Cost.ItemClass || Cost.Quantity <= 0) return false;
	return true;
}

ARPGBuilding::ARPGBuilding() { bReplicates = true; SetReplicateMovement(true); }
void ARPGBuilding::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ARPGBuilding, InstanceId); DOREPLIFETIME(ARPGBuilding, GuildId); DOREPLIFETIME(ARPGBuilding, SettlementId);
	DOREPLIFETIME(ARPGBuilding, Definition); DOREPLIFETIME(ARPGBuilding, Level); DOREPLIFETIME(ARPGBuilding, State);
}
void ARPGBuilding::InitializeBuilding(const FGuid& InGuildId, const FGuid& InSettlementId, URPGBuildingDefinition* InDefinition)
{
	if (!HasAuthority() || !InGuildId.IsValid() || !InSettlementId.IsValid() || !IsValid(InDefinition) || !InDefinition->IsConfigured()) return;
	InstanceId = FGuid::NewGuid(); GuildId = InGuildId; SettlementId = InSettlementId; Definition = InDefinition; Level = 1; State = ERPGBuildingState::Active; ForceNetUpdate();
}

ARPGFarmBuilding::ARPGFarmBuilding() { PrimaryActorTick.bCanEverTick = true; }
void ARPGFarmBuilding::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps); DOREPLIFETIME(ARPGFarmBuilding, SeedItemClass); DOREPLIFETIME(ARPGFarmBuilding, HarvestItemClass);
	DOREPLIFETIME(ARPGFarmBuilding, HarvestQuantity); DOREPLIFETIME(ARPGFarmBuilding, FarmState); DOREPLIFETIME(ARPGFarmBuilding, ReadyServerTime);
}
void ARPGFarmBuilding::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (HasAuthority() && FarmState == ERPGFarmState::Growing && GetWorld()->GetTimeSeconds() >= ReadyServerTime) { FarmState = ERPGFarmState::Ready; OnRep_Farm(); ForceNetUpdate(); }
}
void ARPGFarmBuilding::OnRep_Farm() { FarmStateChanged(); }
bool ARPGFarmBuilding::Plant(ARPGPlayerState* Requester)
{
	ARPGCharacter* Pawn = Requester ? Requester->GetPawn<ARPGCharacter>() : nullptr;
	if (!HasAuthority() || FarmState != ERPGFarmState::Empty || !Pawn || !SeedItemClass || !Definition ||
		FVector::DistSquared(Pawn->GetActorLocation(), GetActorLocation()) > FMath::Square(Definition->InteractionRange) ||
		!ARPGGuildState::CanPlayerUseBuilding(Requester, GuildId)) return false;
	FRPGInventoryAmount Seed; Seed.ItemClass = SeedItemClass; Seed.Quantity = 1;
	if (!Pawn->InventoryComp->TryExchangeItems({Seed}, {})) return false;
	FarmState = ERPGFarmState::Growing; ReadyServerTime = GetWorld()->GetTimeSeconds() + FMath::Max(0.1f, GrowthDuration); OnRep_Farm(); ForceNetUpdate(); return true;
}
bool ARPGFarmBuilding::Harvest(ARPGPlayerState* Requester)
{
	ARPGCharacter* Pawn = Requester ? Requester->GetPawn<ARPGCharacter>() : nullptr;
	if (!HasAuthority() || FarmState != ERPGFarmState::Ready || !Pawn || !HarvestItemClass || HarvestQuantity <= 0 || !Definition ||
		FVector::DistSquared(Pawn->GetActorLocation(), GetActorLocation()) > FMath::Square(Definition->InteractionRange) ||
		!ARPGGuildState::CanPlayerUseBuilding(Requester, GuildId)) return false;
	FRPGInventoryAmount Output; Output.ItemClass = HarvestItemClass; Output.Quantity = HarvestQuantity;
	if (!Pawn->InventoryComp->TryExchangeItems({}, {Output})) return false;
	FarmState = ERPGFarmState::Empty; ReadyServerTime = 0.f; OnRep_Farm(); ForceNetUpdate(); return true;
}
