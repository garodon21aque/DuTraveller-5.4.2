#pragma once
#include "Kismet/BlueprintFunctionLibrary.h"
#include "DTAnimationMigrationLibrary.generated.h"

/** Editor-only recovery from Epic's installed UE4 mannequin; never fabricates a bind pose. */
UCLASS()
class DUTRAVELLER_API UDTAnimationMigrationLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintCallable, Category="Migration Diagnostics") static FString InspectLegacyMannequin();
	/** Copies an explicit dependency map only into absent Mannequin_UE4 packages. */
	UFUNCTION(BlueprintCallable, Category="Migration Repair") static bool CopyLegacyMannequin(const TMap<FString, FString>& Packages);
	/** Remaps references only within the explicitly recovered project packages. Never saves source assets. */
	UFUNCTION(BlueprintCallable, Category="Migration Repair") static bool FinalizeLegacyMannequin(const TMap<FString, FString>& Packages);
};
