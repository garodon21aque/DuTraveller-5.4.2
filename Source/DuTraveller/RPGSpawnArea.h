#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameFramework/Actor.h"
#include "RPGSpawnArea.generated.h"

class UBoxComponent;

/** Replicated identity links an encounter actor to its location and optional quest. */
UCLASS(ClassGroup=(Spawning), BlueprintType, meta=(BlueprintSpawnableComponent))
class DUTRAVELLER_API URPGSpawnContextComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	URPGSpawnContextComponent();
	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const override;
	UPROPERTY(BlueprintReadOnly, Replicated) FGuid SpawnId;
	UPROPERTY(BlueprintReadOnly, Replicated) FGuid QuestInstanceId;
	UPROPERTY(BlueprintReadOnly, Replicated) FName LocationId;
	/** Authoritative interactions can report Interacted, Harvested, etc. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly) bool ReportQuestEvent(FName Event, int32 Amount = 1);
private:
	bool bDeathReported = false;
	UFUNCTION() void HealthChanged(float Health);
};

USTRUCT(BlueprintType)
struct FRPGWeightedSpawn
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite) TSubclassOf<AActor> ActorClass;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="1")) int32 Weight = 1;
};

USTRUCT(BlueprintType)
struct FRPGSpawnRecord
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly, SaveGame) TSubclassOf<AActor> ActorClass;
	UPROPERTY(BlueprintReadOnly, SaveGame) FTransform Transform;
	UPROPERTY(BlueprintReadOnly, SaveGame) FGuid SpawnId;
	UPROPERTY(BlueprintReadOnly, SaveGame) FGuid QuestInstanceId;
};

USTRUCT(BlueprintType)
struct FRPGSpawnAreaSnapshot
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly, SaveGame) int32 SchemaVersion = 1;
	UPROPERTY(BlueprintReadOnly, SaveGame) FName AreaId;
	UPROPERTY(BlueprintReadOnly, SaveGame) FName LocationId;
	UPROPERTY(BlueprintReadOnly, SaveGame) TArray<FRPGSpawnRecord> Actors;
	UPROPERTY(BlueprintReadOnly, SaveGame) float RemainingCooldown = 0.f;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FRPGSpawnStateChanged);

UCLASS(BlueprintType, Blueprintable)
class DUTRAVELLER_API ARPGSpawnArea : public AActor
{
	GENERATED_BODY()
public:
	ARPGSpawnArea();
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const override;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UBoxComponent> Area;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Replicated) FName AreaId;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Replicated) FName LocationId;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FRPGWeightedSpawn> SpawnPool;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="1", ClampMax="256")) int32 MaximumAlive = 5;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0")) float SpawnInterval = 5.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="1", ClampMax="32")) int32 PlacementAttempts = 8;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FVector PlacementHalfExtent = FVector(40, 40, 90);
	UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bRequireGround = true;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0", ClampMax="89")) float MaximumGroundSlope = 45.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bAutoSpawn = true;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bCleanupOnEndPlay = true;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 RandomSeed = 12345;
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_SpawnState) TArray<TObjectPtr<AActor>> AliveActors;
	UPROPERTY(BlueprintAssignable) FRPGSpawnStateChanged OnSpawnStateChanged;
	UFUNCTION(BlueprintPure) bool ContainsPosition(FVector Position) const;
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly) AActor* TrySpawnOne(FGuid QuestInstanceId);
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly) void ClearSpawnedActors();
	UFUNCTION(BlueprintPure) FRPGSpawnAreaSnapshot CaptureSpawnState() const;
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly) bool RestoreSpawnState(const FRPGSpawnAreaSnapshot& Snapshot);
	UFUNCTION(BlueprintImplementableEvent) void SpawnedActorReady(AActor* Actor, FGuid QuestInstanceId);
protected:
	bool IsPlacementClear(const FVector& Position) const;
private:
	FRandomStream Random;
	FTimerHandle SpawnTimer;
	double NextSpawnTime = 0;
	bool bMutating = false;
	bool bHasSpawned = false;
	bool bRandomInitialized = false;
	AActor* SpawnTracked(TSubclassOf<AActor> Class, const FTransform& Transform, FGuid SpawnId, FGuid QuestId);
	void Pulse();
	UFUNCTION() void SpawnDestroyed(AActor* Actor);
	UFUNCTION() void OnRep_SpawnState();
};
