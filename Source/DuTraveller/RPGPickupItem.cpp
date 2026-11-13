#include "RPGPickupItem.h"

#include "Components/StaticMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "InventoryComponent.h"
#include "ItemObject.h"
#include "Net/UnrealNetwork.h"
#include "RPGCharacter.h"
#include "RPGWorldUserWidget.h"
#include "Equipment_ItemObject.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"

ARPGPickupItem::ARPGPickupItem()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	SetReplicateMovement(true);

	UStaticMeshComponent* Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	SetRootComponent(Mesh);

	PickupPrompt = CreateDefaultSubobject<UWidgetComponent>(TEXT("PickupPrompt"));
	PickupPrompt->SetupAttachment(Mesh);
	PickupPrompt->SetRelativeLocation(FVector(0.f, 0.f, 60.f));
	PickupPrompt->SetWidgetSpace(EWidgetSpace::Screen);
	PickupPrompt->SetDrawSize(FVector2D(260.f, 110.f));
	PickupPrompt->SetPivot(FVector2D(0.5f, 1.f));
	PickupPrompt->SetVisibility(false);
	PickupPrompt->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void ARPGPickupItem::BeginPlay()
{
	Super::BeginPlay();
	if (GetNetMode() != NM_DedicatedServer && PickupPrompt && !PickupPrompt->GetWidgetClass())
		PickupPrompt->SetWidgetClass(LoadClass<URPGWorldUserWidget>(nullptr,
			TEXT("/Game/UI/World/WBP_ItemInteract.WBP_ItemInteract_C")));
	RefreshPresentationItem();
	if (HasAuthority() && bPlaceOnGround) PlaceOnGround();
	SetActorTickEnabled(GetNetMode() != NM_DedicatedServer);
}

void ARPGPickupItem::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!PickupPrompt) return;
	PromptCheckAccum += DeltaSeconds;
	if (PromptCheckAccum < 0.15f) return;
	PromptCheckAccum = 0.f;
	if (!bPromptInitialized && PickupPrompt->GetUserWidgetObject()) { PushItemToPrompt(); bPromptInitialized = true; }
	const APawn* Viewer = GetWorld() && GetWorld()->GetFirstPlayerController()
		? GetWorld()->GetFirstPlayerController()->GetPawn() : nullptr;
	const bool bNear = Viewer && FVector::DistSquared(Viewer->GetActorLocation(), GetActorLocation())
		<= FMath::Square(FMath::Min(PromptDistance, CollectionDistance));
	PickupPrompt->SetVisibility(bNear);
}

void ARPGPickupItem::PushItemToPrompt()
{
	if (URPGWorldUserWidget* Widget = Cast<URPGWorldUserWidget>(PickupPrompt->GetUserWidgetObject()))
		Widget->SetWidgetController(this);
}

void ARPGPickupItem::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ARPGPickupItem, ItemClass);
	DOREPLIFETIME(ARPGPickupItem, Quantity);
}

bool ARPGPickupItem::InitializePickup(const TSubclassOf<UItemObject> NewItemClass, const int32 NewQuantity)
{
	if (!HasAuthority() || !NewItemClass || NewQuantity <= 0) return false;
	ItemClass = NewItemClass;
	Quantity = NewQuantity;
	RefreshPresentationItem();
	if (bPlaceOnGround) PlaceOnGround();
	ForceNetUpdate();
	return true;
}

bool ARPGPickupItem::PlaceOnGround()
{
	UStaticMeshComponent* Mesh = Cast<UStaticMeshComponent>(GetRootComponent());
	if (!HasAuthority() || !Mesh || !Mesh->GetStaticMesh() || !GetWorld()) return false;
	FHitResult Ground;
	FCollisionQueryParams Query(SCENE_QUERY_STAT(RPGLootGround), false, this);
	if (GetOwner()) Query.AddIgnoredActor(GetOwner());
	const FVector Position = GetActorLocation();
	if (!GetWorld()->LineTraceSingleByObjectType(Ground, Position+FVector(0,0,200), Position-FVector(0,0,2000), FCollisionObjectQueryParams(ECC_WorldStatic), Query)) return false;
	Mesh->UpdateBounds();
	const float PivotToBottom = Position.Z - (Mesh->Bounds.Origin.Z-Mesh->Bounds.BoxExtent.Z);
	SetActorLocation(FVector(Position.X,Position.Y,Ground.ImpactPoint.Z+PivotToBottom+2.f),false,nullptr,ETeleportType::TeleportPhysics);
	ForceNetUpdate();
	return true;
}

bool ARPGPickupItem::TryCollect(ARPGCharacter* Collector)
{
	if (!HasAuthority() || !IsValid(Collector) || Collector->bIsDead || !Collector->InventoryComp || !ItemClass || Quantity <= 0 ||
		FVector::DistSquared(Collector->GetActorLocation(), GetActorLocation()) > FMath::Square(CollectionDistance)) return false;
	FHitResult Obstruction;
	FCollisionQueryParams Query(SCENE_QUERY_STAT(RPGPickupSight), false, Collector);
	if (const ARPGCharacter* DeadOwner = Cast<ARPGCharacter>(GetOwner()))
		if (DeadOwner->bIsDead) Query.AddIgnoredActor(DeadOwner);
	if (GetWorld()->LineTraceSingleByChannel(Obstruction, Collector->GetActorLocation(), GetActorLocation(), ECC_Visibility, Query) &&
		Obstruction.GetActor() != this) return false;
	const FItemAddResult Added = Collector->InventoryComp->TryAddItemFromClass(ItemClass, Quantity);
	if (Added.AmountGiven <= 0) return false;
	Quantity -= Added.AmountGiven;
	if (Quantity <= 0) Destroy();
	else
	{
		RefreshPresentationItem();
		ForceNetUpdate();
	}
	return true;
}

void ARPGPickupItem::OnRep_PickupData()
{
	RefreshPresentationItem();
}

void ARPGPickupItem::RefreshPresentationItem()
{
	if (!ItemClass || Quantity <= 0)
	{
		Item = nullptr;
		if (PickupPrompt) PickupPrompt->SetVisibility(false);
		return;
	}
	if (!Item || Item->GetClass() != ItemClass) Item = NewObject<UItemObject>(this, ItemClass);
	Item->InitializePresentationPlaceholder(Quantity);
	if (UStaticMeshComponent* Mesh = Cast<UStaticMeshComponent>(GetRootComponent()))
	{
		if (const UEquipment_ItemObject* Gear = Cast<UEquipment_ItemObject>(Item))
			if (Gear->EquippedMesh) Mesh->SetStaticMesh(Gear->EquippedMesh);
		if (!Mesh->GetStaticMesh())
		{
			Mesh->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")));
			Mesh->SetRelativeScale3D(FVector(0.2f));
		}
		Mesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		Mesh->SetCollisionObjectType(ECC_WorldDynamic);
		Mesh->SetCollisionResponseToAllChannels(ECR_Ignore);
		Mesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	}
	PushItemToPrompt();
}
