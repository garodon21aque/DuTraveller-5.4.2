#include "RPGAICharacter.h"
#include "RPGAIController.h"
#include "RPGPickupItem.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/World.h"
#include "RPGWorldUserWidget.h"
#include "Kismet/GameplayStatics.h"
#include "RPGAttributeSet.h"

ARPGAICharacter::ARPGAICharacter()
{
	NPCRole = ERPGNPCRole::Enemy;
	FactionId = FName("Enemy");
	AIControllerClass = ARPGAIController::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
	bUseControllerRotationYaw = false;
	GetCharacterMovement()->bUseControllerDesiredRotation = true;
	GetCharacterMovement()->bOrientRotationToMovement = false;
}

void ARPGAICharacter::BeginPlay()
{
	Super::BeginPlay();
	OnCharacterDeath.AddUniqueDynamic(this, &ThisClass::HandleEnemyDeath);
	if (GetNetMode() != NM_DedicatedServer)
		if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
		{
			UClass* Class = LoadClass<URPGWorldUserWidget>(nullptr, TEXT("/Game/UI/World/WBP_EnemyHealthWidget.WBP_EnemyHealthWidget_C"));
			EnemyHealthWidget = Class ? CreateWidget<URPGWorldUserWidget>(PC, Class) : nullptr;
			if (EnemyHealthWidget)
			{
				EnemyHealthWidget->InitializeCharacterOverlay(this, true);
				EnemyHealthWidget->AddToPlayerScreen(5);
				OnMaxHealthChanged.Broadcast(GetRPGAttributes()->GetMaxHealth());
				OnHealthChanged.Broadcast(GetRPGAttributes()->GetHealth());
			}
		}
}

void ARPGAICharacter::EndPlay(const EEndPlayReason::Type Reason)
{
	if (EnemyHealthWidget) EnemyHealthWidget->RemoveFromParent();
	Super::EndPlay(Reason);
}

void ARPGAICharacter::HandleEnemyDeath(ARPGCharacter* Character, AActor* Killer)
{
	if (!HasAuthority()) return;
	if (EnemyHealthWidget) EnemyHealthWidget->RemoveFromParent();
	if (AAIController* AI = Cast<AAIController>(GetController())) { AI->StopMovement(); AI->ClearFocus(EAIFocusPriority::Gameplay); }
	SpawnDeathLoot();
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SetLifeSpan(FMath::Max(0.1f, CorpseLifetime));
}

void ARPGAICharacter::SpawnDeathLoot_Implementation()
{
	if (!HasAuthority() || !bIsDead || bDeathLootSpawned) return;
	bDeathLootSpawned = true;
	if (LootItemClass && LootQuantity > 0)
	{
		FActorSpawnParameters Params;
		Params.Owner = this;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		UClass* Class = LootPickupClass ? LootPickupClass.Get() : ARPGPickupItem::StaticClass();
		if (ARPGPickupItem* Loot = GetWorld()->SpawnActor<ARPGPickupItem>(Class, GetActorLocation(), FRotator::ZeroRotator, Params))
		{
			if (!Loot->InitializePickup(LootItemClass, LootQuantity)) Loot->Destroy();
		}
	}
}
