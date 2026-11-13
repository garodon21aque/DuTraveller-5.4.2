#pragma once

#include "RPGNPCCharacter.h"
#include "RPGAICharacter.generated.h"

/** Legacy AI-controlled character base; parent of the missing BP_RPGAICharacter Blueprint class. */
UCLASS(BlueprintType, Blueprintable)
class DUTRAVELLER_API ARPGAICharacter : public ARPGNPCCharacter
{
	GENERATED_BODY()

public:
	ARPGAICharacter();
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	UPROPERTY(Transient, BlueprintReadOnly, Category="AI|UI") TObjectPtr<class URPGWorldUserWidget> EnemyHealthWidget;
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="AI|Rewards") TSubclassOf<class ARPGPickupItem> LootPickupClass;
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="AI|Rewards") TSubclassOf<class UItemObject> LootItemClass;
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="AI|Rewards", meta=(ClampMin="1")) int32 LootQuantity = 1;
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, BlueprintAuthorityOnly, Category="AI|Rewards") void SpawnDeathLoot();
	UPROPERTY(EditDefaultsOnly, Category="AI|Rewards", meta=(ClampMin="0.1")) float CorpseLifetime = 5.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI")
	FName EnemyClass;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AI|Rewards", meta=(ClampMin="0"))
	float ExperienceReward = 10.f;
private:
	bool bDeathLootSpawned = false;
	UFUNCTION() void HandleEnemyDeath(ARPGCharacter* Character, AActor* Killer);
};
