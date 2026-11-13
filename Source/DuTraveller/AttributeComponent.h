#pragma once

#include "Components/ActorComponent.h"
#include "AttributeComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FRPGAttributeValueChangedSignature, float, NewValue);

/** Lightweight stat carrier retained for legacy HUD/stat widgets that read Health/Mana/Level through AttributeComp. */
UCLASS(ClassGroup = (Attributes), BlueprintType, Blueprintable, meta = (BlueprintSpawnableComponent))
class DUTRAVELLER_API URPGAttributeComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	URPGAttributeComponent();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attributes")
	float Health = 100.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attributes")
	float MaxHealth = 100.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attributes")
	float Mana = 50.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attributes")
	float MaxMana = 50.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attributes")
	float Stamina = 100.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attributes")
	float MaxStamina = 100.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attributes")
	int32 Level = 1;

	UPROPERTY(BlueprintAssignable, Category = "Attributes")
	FRPGAttributeValueChangedSignature OnHealthChanged;

	UPROPERTY(BlueprintAssignable, Category = "Attributes")
	FRPGAttributeValueChangedSignature OnMaxHealthChanged;

	UPROPERTY(BlueprintAssignable, Category = "Attributes")
	FRPGAttributeValueChangedSignature OnManaChanged;

	UPROPERTY(BlueprintAssignable, Category = "Attributes")
	FRPGAttributeValueChangedSignature OnMaxManaChanged;
};
