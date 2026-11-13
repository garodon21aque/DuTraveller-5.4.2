#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "DTMeshyEditorLibrary.generated.h"

class USkeletalMesh;
class UEditorUtilityWidget;

/**
 * Editor helpers backing the Meshy character retargeting pipeline.
 * Python sockets cannot be authored directly (USkeletalMeshSocket names are
 * read-only reflected properties), so they are created here.
 */
UCLASS()
class DUTRAVELLEREDITOR_API UDTMeshyEditorLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:
	/** Adds a named skeletal-mesh socket bound to BoneName. No-op when it exists. */
	UFUNCTION(BlueprintCallable, Category="DT|Meshy")
	static bool AddSkeletalSocket(USkeletalMesh* Mesh, FName SocketName, FName BoneName,
	                              FTransform RelativeTransform);

	/** Ordered reference-skeleton bone names (parents first). */
	UFUNCTION(BlueprintCallable, Category="DT|Meshy")
	static TArray<FName> EnumerateBoneNames(USkeletalMesh* Mesh);

	/** True when the file path exists on disk. Used to detect report files. */
	UFUNCTION(BlueprintCallable, Category="DT|Meshy")
	static bool FileExists(const FString& Path);

	UFUNCTION(BlueprintCallable, Category="DT|Meshy")
	static FString ReadTextFile(const FString& Path);

	/** Absolute path to Saved/Automation/MeshyRetargetUI.json. */
	UFUNCTION(BlueprintCallable, Category="DT|Meshy")
	static FString RetargeterReportPath();

	/** Spawns the EUW_DuTraveller_CharacterRetargeter editor tab when the asset exists. */
	UFUNCTION(BlueprintCallable, Category="DT|Meshy")
	static UEditorUtilityWidget* SpawnRetargeterEUW();

	/**
	 * Rewrites every animation-asset reference inside an Animation Blueprint's
	 * graphs (sequence/blend-space players) using an explicit name map.
	 * Key/value entries are full package paths of source -> retargeted assets.
	 * Returns the number of references remapped.
	 */
	UFUNCTION(BlueprintCallable, Category="DT|Meshy")
	static int32 RemapAnimBlueprintAnimations(const FString& AnimBlueprintPath,
	                                          const TMap<FString, FString>& SourceToTarget);

	/** Runtime actor spawn for PIE validation scripts (Python has no spawn API in 5.8). */
	UFUNCTION(BlueprintCallable, Category="DT|Meshy")
	static AActor* SpawnActorInWorld(UObject* WorldObject, UClass* ActorClass, FVector Location);
};
