#pragma once

#include "GameplayTagContainer.h"
#include "RPG_AttributeInfo.generated.h"

/** Exact legacy attribute-info identity retained by the attribute-menu widgets. */
USTRUCT(BlueprintType)
struct DUTRAVELLER_API FRPG_SAttributeInfo
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attribute")
	FGameplayTag AttributeTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attribute")
	FText AttributeName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attribute")
	FText AttributeDescription;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attribute")
	float AttributeValue = 0.0f;
};
