// Fill out your copyright notice in the Description page of Project Settings.


#include "DT_HUD.h"
#include "DuTraveller/RPGPlayerController.h"
#include "DuTraveller/DTUIRoot.h"

UDTUIRoot* ADT_HUD::GetPlayerUIRoot() const
{
	const ARPGPlayerController* Controller = Cast<ARPGPlayerController>(GetOwningPlayerController());
	return Controller ? Controller->UIRoot.Get() : nullptr;
}

