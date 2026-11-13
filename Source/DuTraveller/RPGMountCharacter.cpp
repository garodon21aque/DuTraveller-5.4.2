#include "RPGMountCharacter.h"
#include "RPGCharacter.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/PlayerController.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputAction.h"
#include "InputActionValue.h"
#include "InputMappingContext.h"
#include "RPGCombatTypes.h"
#include "Net/UnrealNetwork.h"

ARPGMountCharacter::ARPGMountCharacter()
{
	NPCRole = ERPGNPCRole::Mount;
	Saddle = CreateDefaultSubobject<USceneComponent>(TEXT("Saddle"));
	Saddle->SetupAttachment(RootComponent);
	DetachPoint = CreateDefaultSubobject<USceneComponent>(TEXT("DetachPoint"));
	DetachPoint->SetupAttachment(RootComponent);
	DetachPoint->SetRelativeLocation(FVector(-150.f, 0.f, 0.f));
	MountedRiderMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("MountedRiderMesh"));
	MountedRiderMesh->SetupAttachment(Saddle);
	MountedRiderMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	MountedRiderMesh->SetVisibility(false);
}

void ARPGMountCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ARPGMountCharacter, MountedRider);
}

bool ARPGMountCharacter::TryMount(ARPGCharacter* Rider)
{
	if (!HasAuthority() || !IsValid(Rider) || Rider->bIsDead || IsValid(MountedRider)) return false;
	AController* RiderController = Rider->GetController();
	if (!RiderController) return false;

	MountedRider = Rider;
	if (UCharacterMovementComponent* Move = GetCharacterMovement())
	{
		PreMountWalkSpeed = Move->MaxWalkSpeed;
		Move->MaxWalkSpeed *= MountedSpeedMultiplier;
	}
	if (USkeletalMeshComponent* RiderMesh = Rider->GetMesh()) RiderMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Rider->SetActorEnableCollision(false);
	Rider->SetActorHiddenInGame(true);
	MountedRiderMesh->SetVisibility(true);
	RiderController->Possess(this);
	UE_LOG(LogRPGCombat, Log, TEXT("[Mount] %s mounted %s"), *Rider->GetName(), *GetName());
	return true;
}

bool ARPGMountCharacter::TryDismount()
{
	if (!HasAuthority() || !IsValid(MountedRider)) return false;
	ARPGCharacter* Rider = MountedRider;
	AController* RiderController = GetController();
	if (!RiderController) return false;

	if (UCharacterMovementComponent* Move = GetCharacterMovement())
		Move->MaxWalkSpeed = PreMountWalkSpeed > 0.f ? PreMountWalkSpeed : Move->MaxWalkSpeed;
	MountedRiderMesh->SetVisibility(false);
	MountedRider = nullptr;

	const FTransform Detach = DetachPoint ? DetachPoint->GetComponentTransform() : GetActorTransform();
	Rider->SetActorTransform(Detach, false, nullptr, ETeleportType::TeleportPhysics);
	Rider->SetActorHiddenInGame(false);
	Rider->SetActorEnableCollision(true);
	if (USkeletalMeshComponent* RiderMesh = Rider->GetMesh()) RiderMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	RiderController->Possess(Rider);
	Rider->FlushNetDormancy();
	UE_LOG(LogRPGCombat, Log, TEXT("[Mount] %s dismounted %s"), *Rider->GetName(), *GetName());
	return true;
}

void ARPGMountCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);
	if (!MountedMappingContext.IsNull())
	{
		if (APlayerController* PC = Cast<APlayerController>(NewController))
		{
			if (UEnhancedInputLocalPlayerSubsystem* Subsystem =
				ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
			{
				Subsystem->AddMappingContext(MountedMappingContext.LoadSynchronous(), 1);
				bMountedContextAdded = true;
			}
		}
	}
}

void ARPGMountCharacter::UnPossessed()
{
	if (bMountedContextAdded && !MountedMappingContext.IsNull())
	{
		if (APlayerController* PC = Cast<APlayerController>(Controller))
		{
			if (UEnhancedInputLocalPlayerSubsystem* Subsystem =
				ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
			{
				Subsystem->RemoveMappingContext(MountedMappingContext.LoadSynchronous());
			}
		}
		bMountedContextAdded = false;
	}
	Super::UnPossessed();
}

void ARPGMountCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
	UEnhancedInputComponent* Input = Cast<UEnhancedInputComponent>(PlayerInputComponent);
	if (!Input) return;
	if (UInputAction* Move = MoveInputAction.LoadSynchronous())
		Input->BindAction(Move, ETriggerEvent::Triggered, this, &ARPGMountCharacter::MountMoveInput);
	if (UInputAction* Look = LookInputAction.LoadSynchronous())
		Input->BindAction(Look, ETriggerEvent::Triggered, this, &ARPGMountCharacter::MountLookInput);
	if (UInputAction* JumpAction = JumpInputAction.LoadSynchronous())
	{
		Input->BindAction(JumpAction, ETriggerEvent::Started, this, &ARPGMountCharacter::Jump);
		Input->BindAction(JumpAction, ETriggerEvent::Completed, this, &ARPGMountCharacter::StopJumping);
	}
	if (UInputAction* Dismount = DismountInputAction.LoadSynchronous())
		Input->BindAction(Dismount, ETriggerEvent::Started, this, &ARPGMountCharacter::MountDismountInput);
}

void ARPGMountCharacter::MountMoveInput(const FInputActionValue& Value)
{
	const FVector2D Axis = Value.Get<FVector2D>();
	if (!FMath::IsNearlyZero(Axis.Y)) AddMovementInput(GetActorForwardVector(), Axis.Y);
	if (!FMath::IsNearlyZero(Axis.X)) AddMovementInput(GetActorRightVector(), Axis.X);
}

void ARPGMountCharacter::MountLookInput(const FInputActionValue& Value)
{
	const FVector2D Axis = Value.Get<FVector2D>();
	AddControllerYawInput(Axis.X);
	AddControllerPitchInput(Axis.Y);
}

void ARPGMountCharacter::MountDismountInput(const FInputActionValue& Value)
{
	if (HasAuthority()) TryDismount();
	else ServerDismount();
}

void ARPGMountCharacter::ServerDismount_Implementation()
{
	TryDismount();
}
