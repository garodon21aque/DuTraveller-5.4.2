#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "DTStructRepairLibrary.generated.h"

class ARPGCharacter;
class ARPGPlayerState;
class URPGOverlayWidgetController;

/** Transient observer used by bounded editor progression checks; never saved. */
UCLASS(Transient, BlueprintType)
class DUTRAVELLER_API UDTProgressionEventProbe : public UObject
{
	GENERATED_BODY()
public:
	UPROPERTY(BlueprintReadOnly, Category="Diagnostics") TArray<int32> PlayerStateLevels;
	UPROPERTY(BlueprintReadOnly, Category="Diagnostics") TArray<int32> OverlayLevels;
	UPROPERTY(BlueprintReadOnly, Category="Diagnostics") TArray<int32> InitialLevels;
	UFUNCTION() void RecordPlayerLevel(int32 Level) { PlayerStateLevels.Add(Level); }
	UFUNCTION() void RecordOverlayLevel(int32 Level, bool bLevelUp) { if (bLevelUp) OverlayLevels.Add(Level); else InitialLevels.Add(Level); }
};

/**
 * Editor-only asset reconstruction helpers for missing user-defined
 * structs/enums. Serialized graph pins reference these asset paths directly;
 * core redirects cannot fabricate the missing packages, so the assets are
 * regenerated with fields matching the serialized evidence.
 */
UCLASS()
class DUTRAVELLER_API UDTStructRepairLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category="Migration Diagnostics") static UDTProgressionEventProbe* ObserveProgression(ARPGPlayerState* PlayerState, URPGOverlayWidgetController* Overlay);
	UFUNCTION(BlueprintCallable, Category="Migration Repair") static bool RepairHUDLevelUp(const FString& ObjectPath);
	/** Adds a visible, parent-preserving Blueprint override for menu input customization. */
	UFUNCTION(BlueprintCallable, Category="Migration Repair") static bool ExposeControllerMenuFlow(const FString& ObjectPath);
	/** Bounded PIE validation through Slate's actual input routing; never changes assets. */
	UFUNCTION(BlueprintCallable, Category="Migration Diagnostics") static bool SendRuntimeUIKey(class APlayerController* Controller, FKey Key);
	UFUNCTION(BlueprintCallable, Category="Migration Diagnostics") static bool ClickRuntimeUIWidget(class UWidget* Widget, FKey Button, FVector2D NormalizedPosition);
	UFUNCTION(BlueprintCallable, Category="Migration Diagnostics") static TArray<class UWidget*> GetRuntimeUIWidgets(class UUserWidget* Root);
	UFUNCTION(BlueprintCallable, Category="Migration Diagnostics") static TMap<FString,float> MeasureRuntimeGroundClearance(class AActor* Actor);
	UFUNCTION(BlueprintCallable, Category="Migration Diagnostics") static class AActor* SpawnRuntimeImpactProbe(ARPGCharacter* Source, int32 Index);
	UFUNCTION(BlueprintCallable, Category="Migration Diagnostics") static class AActor* SpawnRuntimeActor(ARPGCharacter* Source, const FString& ClassPath, const FVector& Offset);
	UFUNCTION(BlueprintCallable, Category="Migration Repair") static bool RepairCombatUpperBodyBlend(const FString& ObjectPath);
	UFUNCTION(BlueprintCallable, Category="Migration Diagnostics") static TMap<FString,int32> CountRuntimeProjectileFX(ARPGCharacter* Character);
	/** Retypes legacy local-player widget members/casts to shared native bases. Never saves. */
	UFUNCTION(BlueprintCallable, Category="Migration Repair")
	static bool RepairUnifiedWidgetContext(const FString& ObjectPath);
	/** In-memory two-player listen-server settings for bounded editor tests. Never saves config. */
	UFUNCTION(BlueprintCallable, Category = "Migration Diagnostics")
	static void SetMultiplayerTestSettings(bool bEnabled);
	/** Dispatch a test request with normal PIE RPC routing instead of Python's editor-local override. */
	UFUNCTION(BlueprintCallable, Category = "Migration Diagnostics")
	static bool RunFoundationClientTestAction(ARPGCharacter* Character, int32 Stage);
	UFUNCTION(BlueprintPure, Category = "Migration Diagnostics")
	static bool HasGrantedGameplayAbility(ARPGCharacter* Character, FName AbilityId);
	UFUNCTION(BlueprintPure, Category = "Migration Diagnostics")
	static bool IsGameplayAbilityActive(ARPGCharacter* Character, FName AbilityId);
	/** Configure server-owned transient test items without editing class defaults or assets. */
	UFUNCTION(BlueprintCallable, Category = "Migration Diagnostics")
	static bool PrepareFoundationTestInventory(ARPGCharacter* Character);

	/**
	 * Creates (or updates) a UUserDefinedStruct asset at ObjectPath.
	 * FieldSpecs entries use the form:
	 *   "Name|Category|SubCategoryObjectPath|ContainerType|SerializedVarName"
	 * Category: int, real, bool, text, name, string, object, class, struct, enum.
	 * SubCategoryObjectPath: object path of the referenced class/struct/enum.
	 * ContainerType: None, Array, Set, Map (Map requires a value spec appended
	 * as |ValueCategory|ValueObjectPath).
	 * SerializedVarName (optional): exact historical VarName FName recovered
	 * from referencing assets so serialized member references resolve.
	 */
	UFUNCTION(BlueprintCallable, Category = "Migration Repair")
	static bool CreateUserDefinedStructAsset(const FString& ObjectPath,
	                                         const TArray<FString>& FieldSpecs);

	/** Creates (or updates) a UUserDefinedEnum asset with the given display names. */
	UFUNCTION(BlueprintCallable, Category = "Migration Repair")
	static bool CreateUserDefinedEnumAsset(const FString& ObjectPath,
	                                       const TArray<FString>& EntryNames);

	/**
	 * Renames the enum entries on an existing UUserDefinedEnum so serialized
	 * enum-literal pins resolve by entry name (e.g. "EQuestTypes::Renown").
	 * SetEnumeratorDisplayName only touches the DisplayNameMap, so this sets
	 * the actual Names array while preserving existing display names.
	 */
	UFUNCTION(BlueprintCallable, Category = "Migration Repair")
	static bool SetUserDefinedEnumEntryNames(const FString& ObjectPath,
	                                         const TArray<FString>& EntryNames);

	/**
	 * Forces FBlueprintEditorUtils::RegenerateBlueprintClass + a compile on the
	 * Blueprint at ObjectPath. The Python add_member_variable/compile path does
	 * not produce FProperties on the generated class in this build, so this
	 * rebuilds the class layout directly from NewVariables.
	 */
	UFUNCTION(BlueprintCallable, Category = "Migration Repair")
	static bool RegenerateBlueprintClass(const FString& ObjectPath);

	/** Diagnostic: logs every call-function/variable node pin + link for the asset. */
	UFUNCTION(BlueprintCallable, Category = "Migration Repair")
	static bool DumpBlueprintGraph(const FString& ObjectPath);

	/**
	 * Reconstructs every K2 node in the Blueprint's graphs. Used for stale
	 * serialized pins the compiler asks to refresh (Select enum pins,
	 * outdated function signatures) after contracts have been restored.
	 */
	UFUNCTION(BlueprintCallable, Category = "Migration Repair")
	static bool RefreshBlueprintNodes(const FString& ObjectPath);

	/** Logs each NewVariables entry's name/guid and whether a generated FProperty exists. */
	UFUNCTION(BlueprintCallable, Category = "Migration Repair")
	static bool ProbeVarState(const FString& ObjectPath);

	/**
	 * Creates a Blueprint-declared function on the Blueprint at ObjectPath.
	 * PinSpecs entries: "in|Name|Category|SubObjectPath" for inputs and
	 * "out|Name|Category|SubObjectPath" for outputs (a "ReturnValue" out pin
	 * produces a return value). Used to recreate serialized BP function
	 * signatures that carried space-named params (e.g. 'Array Element').
	 */
	UFUNCTION(BlueprintCallable, Category = "Migration Repair")
	static bool CreateBPFunction(const FString& ObjectPath,
	                             const FString& FuncName,
	                             const TArray<FString>& PinSpecs);

	/** Rebinds external member references whose parent class fails to resolve,
	 *  inferring the owner class from the pin wired into 'self'. */
	UFUNCTION(BlueprintCallable, Category = "Migration Repair")
	static bool RebindVariableRefParents(const FString& ObjectPath);

	/** Replaces matching call nodes with a current UFunction while preserving context and return links. */
	UFUNCTION(BlueprintCallable, Category = "Migration Repair")
	static bool ReplaceBlueprintCallFunction(const FString& ObjectPath,
	                                        const FString& OldFunctionName,
	                                        const FString& OwnerClassPath,
	                                        const FString& NewFunctionName);

	/** Manually connects two pins by node/pin name (serialized links that were
	 *  dropped by earlier failed reconstructions). */
	UFUNCTION(BlueprintCallable, Category = "Migration Repair")
	static bool ConnectBlueprintPins(const FString& ObjectPath,
	                                 const FString& SrcNodeName,
	                                 const FString& SrcPinName,
	                                 const FString& DstNodeName,
	                                 const FString& DstPinName);

	/** Marks the Blueprint structurally modified, compiles it (produces
	 *  UFunctions for new function graphs), and saves only after successful compilation. */
	UFUNCTION(BlueprintCallable, Category = "Migration Repair")
	static bool ForceCompileAndSave(const FString& ObjectPath);

	/** Saves the package for the asset at ObjectPath. Blueprints must have a
	 *  successful compile status; failed or uncompiled assets are never saved. */
	UFUNCTION(BlueprintCallable, Category = "Migration Repair")
	static bool SaveAssetPackage(const FString& ObjectPath);

	/**
	 * Creates (or updates) a USkeleton asset at ObjectPath with the supplied
	 * bone hierarchy. BoneSpecs entries are "BoneName|ParentName"; an empty
	 * ParentName marks the root bone. Used to stand in for Skeleton assets the
	 * serialized AnimBlueprints reference but which were never committed.
	 */
	UFUNCTION(BlueprintCallable, Category = "Migration Repair")
	static bool CreateSkeletonAsset(const FString& ObjectPath,
	                                const TArray<FString>& BoneSpecs);

	/**
	 * Assigns the Skeleton at SkeletonObjectPath to the AnimBlueprint at
	 * ObjectPath (UAnimBlueprint::TargetSkeleton) and recompiles it. Resaved
	 * AnimBlueprints dropped their dangling skeleton reference before the
	 * assets existed, so the reference must be restored explicitly.
	 */
	UFUNCTION(BlueprintCallable, Category = "Migration Repair")
	static bool AssignAnimSkeleton(const FString& ObjectPath,
	                               const FString& SkeletonObjectPath);

	/**
	 * Rewrites a user-defined pin's PinType on a UK2Node_EditablePinBase node
	 * (FunctionEntry, FunctionResult, CustomEvent) found by node name. Used
	 * when a recreated function/event pin was declared with a wrong or
	 * degraded type (e.g. BP_QuestLog 'Array Element'/'Array_Element',
	 * WBP_PlayerStat custom-event 'PC').
	 */
	UFUNCTION(BlueprintCallable, Category = "Migration Repair")
	static bool SetUserPinType(const FString& ObjectPath,
	                           const FString& NodeName,
	                           const FString& PinName,
	                           const FString& Category,
	                           const FString& SubPath);

	/**
	 * Sets the target class on a UK2Node_DynamicCast found by node name and
	 * reconstructs it, restoring casts whose serialized target class was a
	 * deleted-asset reference (e.g. 'As BP RPGCharacter').
	 */
	UFUNCTION(BlueprintCallable, Category = "Migration Repair")
	static bool SetCastTargetClass(const FString& ObjectPath,
	                               const FString& NodeName,
	                               const FString& ClassPath);

	/**
	 * Appends a Blueprint member variable to NewVariables with the exact
	 * serialized VarName (which may contain spaces, e.g. ' BP Player
	 * Character' on AnimBP_RPGCharacter). The Python add_member_variable path
	 * sanitizes names, which breaks member references that serialized the
	 * literal spaced name. Call RegenerateBlueprintClass afterwards so the
	 * generated class materializes the FProperty.
	 */
	UFUNCTION(BlueprintCallable, Category = "Migration Repair")
	static bool AddMemberVariableExact(const FString& ObjectPath,
	                                   const FString& VarName,
	                                   const FString& Category,
	                                   const FString& SubPath);

	/** Logs every notify/notify-state on the montage: name, class, trigger time and duration. */
	UFUNCTION(BlueprintCallable, Category = "Migration Repair")
	static TArray<FString> DescribeMontageNotifies(const FString& ObjectPath);

	/** Adds a UDTAnimNotifyEvent carrying EventTag at absolute montage time, then resaves the montage. */
	UFUNCTION(BlueprintCallable, Category = "Migration Repair")
	static bool AddMontageGameplayEventNotify(const FString& ObjectPath,
	                                          const FString& EventTag,
	                                          float TimeSeconds);

	/** Adds a UDTAnimNotifyMeleeWindow state spanning [Start,End] seconds, then resaves the montage. */
	UFUNCTION(BlueprintCallable, Category = "Migration Repair")
	static bool AddMontageMeleeWindow(const FString& ObjectPath,
	                                  float StartSeconds,
	                                  float EndSeconds);

	/**
	 * Configures release-event execution on a URPGCombatAbilityDefinition:
	 * release tag, projectile class and socket, then resaves the asset.
	 */
	UFUNCTION(BlueprintCallable, Category = "Migration Repair")
	static bool ConfigureAbilityRelease(const FString& ObjectPath,
	                                    const FString& EventTag,
	                                    const FString& ProjectileClassPath,
	                                    const FString& SocketName);

	/** Binds the primary track to SlotName and gives additional tracks unique names. */
	UFUNCTION(BlueprintCallable, Category = "Migration Repair")
	static bool SetMontageSlotName(const FString& ObjectPath, const FString& SlotName);

	/**
	 * Diagnostic: logs/returns every anim-graph node class in the AnimBlueprint —
	 * slot names, linked anim layers, state machines and the graph root — so
	 * montage slot compatibility and locomotion graph contents can be verified
	 * without opening the editor UI.
	 */
	UFUNCTION(BlueprintCallable, Category = "Migration Repair")
	static TArray<FString> DescribeAnimGraph(const FString& ObjectPath);
	UFUNCTION(BlueprintCallable, Category="Migration Repair")
	static TArray<FString> InspectSkillWidgets(const FString& ObjectPath, bool bEnableScrolling = false);
	/** Sets an FGameplayTag member (e.g. InputTag/AbilityTag) on a named designer widget inside a WidgetBlueprint's tree, then resaves. */
	UFUNCTION(BlueprintCallable, Category="Migration Repair")
	static bool SetDesignerWidgetGameplayTag(const FString& ObjectPath, const FString& WidgetName, const FString& PropertyName, const FString& TagName);
	UFUNCTION(BlueprintCallable, Category="Runtime Diagnostics")
	static TMap<FString, FString> InspectRuntimePlayer(class APlayerController* Controller);
	UFUNCTION(BlueprintPure, Category="Runtime Diagnostics")
	static float ReadRuntimeAttribute(class AActor* Actor, FName AttributeName);
	UFUNCTION(BlueprintCallable, Category="Migration Repair")
	static TArray<FString> RepairDuplicateMontageSlots(const FString& ObjectPath, bool bApply = false);
	UFUNCTION(BlueprintCallable, Category="Runtime Diagnostics")
	static bool InjectRuntimeInput(class APlayerController* Controller, const FString& ActionName, FVector Value);
	UFUNCTION(BlueprintCallable, Category="Migration Repair")
	static bool RestoreMissingMontageSequence(const FString& MontagePath, const FString& SequencePath);
	UFUNCTION(BlueprintCallable, Category="Runtime Diagnostics")
	static class ARPGCharacter* SpawnRuntimeCombatTarget(class ARPGCharacter* Source, float Distance);
	UFUNCTION(BlueprintCallable, Category="Migration Repair")
	static bool RepairPlayerSprintGait();
	UFUNCTION(BlueprintCallable, Category="Migration Repair")
	static bool RepairEnemyAnimReferences(const FString& ObjectPath);
};
