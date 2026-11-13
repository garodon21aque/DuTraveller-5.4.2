#pragma once

#include "CoreMinimal.h"
#include "EditorUtilityWidget.h"
#include "DTMeshyRetargeterWidget.generated.h"

class UComboBoxString;
class UCheckBox;
class UTextBlock;
class UMultiLineEditableTextBox;
class UEditableTextBox;

/**
 * Native Editor Utility Widget for the DuTraveller Meshy retargeting pipeline.
 * The whole UMG layout is built in code inside NativeConstruct so the asset
 * EUW_DuTraveller_CharacterRetargeter stays a thin shell parented to this class.
 * Actions delegate to Tools/MeshyRetargetCore.py through IPythonScriptPlugin and
 * results are read back from Saved/Automation/MeshyRetargetUI.json.
 */
UCLASS(BlueprintType)
class DUTRAVELLEREDITOR_API UDuTravellerCharacterRetargeterWidget : public UEditorUtilityWidget
{
	GENERATED_BODY()
public:
	virtual void NativeConstruct() override;

	UFUNCTION() void OnImportFbx();
	UFUNCTION() void OnGenerateRetargeter();
	UFUNCTION() void OnRetargetLibrary();
	UFUNCTION() void OnAdaptAnimBP();
	UFUNCTION() void OnCreateEnemyBP();
	UFUNCTION() void OnValidate();
	UFUNCTION() void OnPreview();

protected:
	UPROPERTY() TObjectPtr<UComboBoxString> SourceRigCombo;
	UPROPERTY() TObjectPtr<UComboBoxString> TargetMeshCombo;
	UPROPERTY() TObjectPtr<UComboBoxString> AnimSourceCombo;
	UPROPERTY() TObjectPtr<UComboBoxString> DirectionCombo;
	UPROPERTY() TObjectPtr<UCheckBox> AutoChainsCheck;
	UPROPERTY() TObjectPtr<UCheckBox> AutoAlignCheck;
	UPROPERTY() TObjectPtr<UComboBoxString> RootMotionCombo;
	UPROPERTY() TObjectPtr<UEditableTextBox> FbxPathEdit;
	UPROPERTY() TObjectPtr<UMultiLineEditableTextBox> OutputLog;
	UPROPERTY() TObjectPtr<UTextBlock> StatusText;

private:
	void BuildLayout();
	void RefreshAssetLists();
	void RunPythonAction(const FString& Action);
	void RefreshOutputFromReport();
	FString SelectedTargetMesh() const;
	FString SelectedSourceKey() const;
	FString JsonArgs(const FString& Action) const;
};
