// Fill out your copyright notice in the Description page of Project Settings.


#include "DTBaseCharacter.h"

// Sets default values
ADTBaseCharacter::ADTBaseCharacter()
{
 	// Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

}

// Called when the game starts or when spawned
void ADTBaseCharacter::BeginPlay()
{
	Super::BeginPlay();
	
}

// Called every frame
void ADTBaseCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

// Called to bind functionality to input
void ADTBaseCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

}

