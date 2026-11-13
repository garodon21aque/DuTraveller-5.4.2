#include "RPGSpawnArea.h"
#include "Components/BoxComponent.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Net/UnrealNetwork.h"
#include "RPGCharacter.h"
#include "RPGPlayerState.h"
#include "RPGDynamicQuestComponent.h"
#include "AttributeComponent.h"
#include "EngineUtils.h"

URPGSpawnContextComponent::URPGSpawnContextComponent() { SetIsReplicatedByDefault(true); }
void URPGSpawnContextComponent::BeginPlay()
{
	Super::BeginPlay();
	if (GetOwner() && GetOwner()->HasAuthority())
		if (ARPGCharacter* Character = Cast<ARPGCharacter>(GetOwner()))
			Character->AttributeComp->OnHealthChanged.AddUniqueDynamic(this, &URPGSpawnContextComponent::HealthChanged);
}
void URPGSpawnContextComponent::HealthChanged(const float Health)
{
	if (!bDeathReported && FMath::IsFinite(Health) && Health <= 0.f && GetOwner() && GetOwner()->HasAuthority())
	{ bDeathReported = true; ReportQuestEvent(FName("Defeated"), 1); }
}
bool URPGSpawnContextComponent::ReportQuestEvent(const FName Event, const int32 Amount)
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || !QuestInstanceId.IsValid() || !SpawnId.IsValid() || Event.IsNone() || Amount < 1) return false;
	for (TActorIterator<ARPGPlayerState> It(GetWorld()); It; ++It)
		if (It->DynamicQuests->Records.ContainsByPredicate([&](const FRPGDynamicQuestRecord& Record) { return Record.InstanceId == QuestInstanceId; }))
			return It->DynamicQuests->RecordObjectiveEvent(Event, GetOwner(), Amount);
	return false;
}
void URPGSpawnContextComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(URPGSpawnContextComponent, SpawnId);
	DOREPLIFETIME(URPGSpawnContextComponent, QuestInstanceId);
	DOREPLIFETIME(URPGSpawnContextComponent, LocationId);
}

ARPGSpawnArea::ARPGSpawnArea()
{
	bReplicates = true;
	Area = CreateDefaultSubobject<UBoxComponent>(TEXT("SpawnArea"));
	SetRootComponent(Area);
	Area->SetBoxExtent(FVector(500, 500, 250));
	Area->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void ARPGSpawnArea::BeginPlay()
{
	Super::BeginPlay();
	Random.Initialize(RandomSeed);
	bRandomInitialized = true;
	if (HasAuthority()) GetWorldTimerManager().SetTimer(SpawnTimer, this, &ARPGSpawnArea::Pulse, 0.5f, true);
}

void ARPGSpawnArea::EndPlay(const EEndPlayReason::Type Reason)
{
	GetWorldTimerManager().ClearTimer(SpawnTimer);
	if (HasAuthority() && bCleanupOnEndPlay) ClearSpawnedActors();
	Super::EndPlay(Reason);
}

void ARPGSpawnArea::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ARPGSpawnArea, AreaId);
	DOREPLIFETIME(ARPGSpawnArea, LocationId);
	DOREPLIFETIME(ARPGSpawnArea, AliveActors);
}

bool ARPGSpawnArea::ContainsPosition(const FVector Position) const
{
	if (Position.ContainsNaN() || !Area || Area->GetComponentTransform().ContainsNaN()) return false;
	const FVector Scale = Area->GetComponentScale();
	if (Scale.GetMin() <= UE_SMALL_NUMBER || Scale.ContainsNaN()) return false;
	const FVector Local = Area->GetComponentTransform().InverseTransformPosition(Position);
	const FVector Extent = Area->GetUnscaledBoxExtent();
	return !Local.ContainsNaN() && FMath::Abs(Local.X) <= Extent.X && FMath::Abs(Local.Y) <= Extent.Y && FMath::Abs(Local.Z) <= Extent.Z;
}

bool ARPGSpawnArea::IsPlacementClear(const FVector& Position) const
{
	if (!ContainsPosition(Position) || PlacementHalfExtent.ContainsNaN() || PlacementHalfExtent.GetMin() <= 0) return false;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(RPGSpawnPlacement), false, this);
	return !GetWorld()->OverlapBlockingTestByChannel(Position, FQuat::Identity, ECC_Pawn,
		FCollisionShape::MakeBox(PlacementHalfExtent), Params);
}

AActor* ARPGSpawnArea::SpawnTracked(TSubclassOf<AActor> Class, const FTransform& Transform, FGuid SpawnId, FGuid QuestId)
{
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::DontSpawnIfColliding;
	AActor* Actor = GetWorld()->SpawnActor<AActor>(Class, Transform, Params);
	if (!IsValid(Actor) || Actor->IsActorBeingDestroyed()) return nullptr;
	Actor->SetReplicates(true);
	URPGSpawnContextComponent* Context = NewObject<URPGSpawnContextComponent>(Actor);
	Context->SpawnId = SpawnId;
	Context->QuestInstanceId = QuestId;
	Context->LocationId = LocationId;
	Actor->AddInstanceComponent(Context);
	Context->RegisterComponent();
	Actor->OnDestroyed.AddDynamic(this, &ARPGSpawnArea::SpawnDestroyed);
	AliveActors.Add(Actor);
	return Actor;
}

AActor* ARPGSpawnArea::TrySpawnOne(const FGuid QuestInstanceId)
{
	if (!HasAuthority() || bMutating || AreaId.IsNone() || LocationId.IsNone() || MaximumAlive < 1 || MaximumAlive > 256 ||
		!FMath::IsFinite(SpawnInterval) || SpawnInterval < 0 || PlacementAttempts < 1 || PlacementAttempts > 32 ||
		!FMath::IsFinite(MaximumGroundSlope) || MaximumGroundSlope < 0 || MaximumGroundSlope >= 90 ||
		AliveActors.Num() >= MaximumAlive || GetWorld()->GetTimeSeconds() < NextSpawnTime) return nullptr;
	int64 Weight = 0;
	for (const auto& Entry : SpawnPool)
	{
		if (!Entry.ActorClass || Entry.ActorClass->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated) || Entry.Weight <= 0) return nullptr;
		Weight += Entry.Weight;
	}
	if (Weight <= 0 || Weight > MAX_int32) return nullptr;
	TGuardValue<bool> Guard(bMutating, true);
	if (!bRandomInitialized) { Random.Initialize(RandomSeed); bRandomInitialized = true; }
	int32 Pick = Random.RandRange(1, static_cast<int32>(Weight));
	TSubclassOf<AActor> Class;
	for (const auto& Entry : SpawnPool) { Pick -= Entry.Weight; if (Pick <= 0) { Class = Entry.ActorClass; break; } }
	const FVector Extent = Area->GetUnscaledBoxExtent();
	for (int32 Attempt = 0; Attempt < PlacementAttempts; ++Attempt)
	{
		FVector Position = Area->GetComponentTransform().TransformPosition(FVector(
			Random.FRandRange(-Extent.X, Extent.X), Random.FRandRange(-Extent.Y, Extent.Y), Random.FRandRange(-Extent.Z, Extent.Z)));
		if (bRequireGround)
		{
			FHitResult Ground;
			FCollisionQueryParams Params(SCENE_QUERY_STAT(RPGSpawnGround), false, this);
			const float Height = Area->Bounds.BoxExtent.Z;
			const FVector Start(Position.X, Position.Y, Area->Bounds.Origin.Z + Height);
			const FVector End(Position.X, Position.Y, Area->Bounds.Origin.Z - Height);
			if (!GetWorld()->LineTraceSingleByObjectType(Ground, Start, End, FCollisionObjectQueryParams(ECC_WorldStatic), Params) ||
				Ground.ImpactNormal.Z < FMath::Cos(FMath::DegreesToRadians(MaximumGroundSlope))) continue;
			Position = Ground.ImpactPoint + FVector(0, 0, PlacementHalfExtent.Z + 2.f);
		}
		if (!IsPlacementClear(Position)) continue;
		AActor* Spawned = SpawnTracked(Class, FTransform(FRotator(0, GetActorRotation().Yaw, 0), Position), FGuid::NewGuid(), QuestInstanceId);
		if (!Spawned) continue;
		bHasSpawned = true;
		NextSpawnTime = GetWorld()->GetTimeSeconds() + FMath::Max(0.f, SpawnInterval);
		OnRep_SpawnState();
		ForceNetUpdate();
		SpawnedActorReady(Spawned, QuestInstanceId);
		return Spawned;
	}
	// Failed placements also back off, preventing an obstructed area from hot-looping.
	NextSpawnTime = GetWorld()->GetTimeSeconds() + FMath::Max(0.5f, SpawnInterval);
	return nullptr;
}

void ARPGSpawnArea::Pulse() { if (bAutoSpawn) TrySpawnOne(FGuid()); }
void ARPGSpawnArea::OnRep_SpawnState() { OnSpawnStateChanged.Broadcast(); }
void ARPGSpawnArea::SpawnDestroyed(AActor* Actor)
{
	if (!HasAuthority()) return;
	AliveActors.Remove(Actor);
	OnRep_SpawnState();
	ForceNetUpdate();
}

void ARPGSpawnArea::ClearSpawnedActors()
{
	if (!HasAuthority() || bMutating) return;
	TGuardValue<bool> Guard(bMutating, true);
	const auto Actors = AliveActors;
	AliveActors.Reset();
	for (AActor* Actor : Actors) if (IsValid(Actor)) { Actor->OnDestroyed.RemoveDynamic(this, &ARPGSpawnArea::SpawnDestroyed); Actor->Destroy(); }
	OnRep_SpawnState();
	ForceNetUpdate();
}

FRPGSpawnAreaSnapshot ARPGSpawnArea::CaptureSpawnState() const
{
	FRPGSpawnAreaSnapshot Snapshot;
	Snapshot.AreaId = AreaId; Snapshot.LocationId = LocationId;
	Snapshot.RemainingCooldown = FMath::Max(0.f, static_cast<float>(NextSpawnTime - GetWorld()->GetTimeSeconds()));
	for (AActor* Actor : AliveActors)
	{
		if (!IsValid(Actor)) continue;
		const auto* Context = Actor->FindComponentByClass<URPGSpawnContextComponent>();
		if (!Context) continue;
		FRPGSpawnRecord Record;
		Record.ActorClass = Actor->GetClass(); Record.Transform = Actor->GetActorTransform();
		Record.SpawnId = Context->SpawnId; Record.QuestInstanceId = Context->QuestInstanceId;
		Snapshot.Actors.Add(Record);
	}
	return Snapshot;
}

bool ARPGSpawnArea::RestoreSpawnState(const FRPGSpawnAreaSnapshot& Snapshot)
{
	if (!HasAuthority() || bMutating || bHasSpawned || !AliveActors.IsEmpty() || Snapshot.SchemaVersion != 1 ||
		AreaId.IsNone() || Snapshot.AreaId != AreaId || Snapshot.LocationId != LocationId || MaximumAlive < 1 || MaximumAlive > 256 ||
		Snapshot.Actors.Num() > MaximumAlive || !FMath::IsFinite(Snapshot.RemainingCooldown) || Snapshot.RemainingCooldown < 0) return false;
	TSet<FGuid> Ids;
	for (const auto& Record : Snapshot.Actors)
	{
		if (!Record.SpawnId.IsValid() || Ids.Contains(Record.SpawnId) || !Record.Transform.IsValid() ||
			!Record.Transform.GetScale3D().Equals(FVector::OneVector) || !IsPlacementClear(Record.Transform.GetLocation()) ||
			!SpawnPool.ContainsByPredicate([&](const FRPGWeightedSpawn& Entry) { return Entry.ActorClass == Record.ActorClass && Entry.Weight > 0; })) return false;
		Ids.Add(Record.SpawnId);
	}
	TGuardValue<bool> Guard(bMutating, true);
	for (const auto& Record : Snapshot.Actors)
	{
		if (!IsPlacementClear(Record.Transform.GetLocation()) || !SpawnTracked(Record.ActorClass, Record.Transform, Record.SpawnId, Record.QuestInstanceId))
		{
			const auto Actors = AliveActors;
			AliveActors.Reset();
			for (AActor* Actor : Actors) if (IsValid(Actor)) { Actor->OnDestroyed.RemoveDynamic(this, &ARPGSpawnArea::SpawnDestroyed); Actor->Destroy(); }
			return false;
		}
	}
	bHasSpawned = true;
	NextSpawnTime = GetWorld()->GetTimeSeconds() + Snapshot.RemainingCooldown;
	OnRep_SpawnState(); ForceNetUpdate();
	return true;
}
