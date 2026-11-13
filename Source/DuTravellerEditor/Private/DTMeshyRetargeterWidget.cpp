#include "DTMeshyRetargeterWidget.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/CheckBox.h"
#include "Components/ComboBoxString.h"
#include "Components/EditableTextBox.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/MultiLineEditableTextBox.h"
#include "Components/ScrollBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "DTMeshyEditorLibrary.h"
#include "IPythonScriptPlugin.h"
#include "Misc/Paths.h"

namespace
{
FText Lbl(const TCHAR* T) { return FText::FromString(T); }

UTextBlock* MakeText(UWidgetTree* Tree, UVerticalBox* Parent, const TCHAR* Text, bool bBold = false)
{
	UTextBlock* T = NewObject<UTextBlock>(Tree);
	T->SetText(Lbl(Text));
	Parent->AddChild(T);
	return T;
}

UHorizontalBox* MakeRow(UWidgetTree* Tree, UVerticalBox* Parent, const TCHAR* Label)
{
	UHorizontalBox* Row = NewObject<UHorizontalBox>(Tree);
	UTextBlock* T = NewObject<UTextBlock>(Tree);
	T->SetText(Lbl(Label));
	UHorizontalBoxSlot* LS = Row->AddChildToHorizontalBox(T);
	LS->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
	LS->SetPadding(FMargin(0, 0, 8, 0));
	Parent->AddChild(Row);
	return Row;
}

UComboBoxString* MakeCombo(UWidgetTree* Tree, UHorizontalBox* Row)
{
	UComboBoxString* C = NewObject<UComboBoxString>(Tree);
	UHorizontalBoxSlot* S = Row->AddChildToHorizontalBox(C);
	S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	S->SetPadding(FMargin(0, 2, 0, 2));
	return C;
}

template <typename BindFn>
UButton* MakeButton(UWidgetTree* Tree, UVerticalBox* Parent, const TCHAR* Label, BindFn&& Bind)
{
	UButton* B = NewObject<UButton>(Tree);
	UTextBlock* T = NewObject<UTextBlock>(Tree);
	T->SetText(Lbl(Label));
	B->AddChild(T);
	Bind(B);
	UVerticalBoxSlot* S = Parent->AddChildToVerticalBox(B);
	S->SetPadding(FMargin(0, 3, 0, 0));
	return B;
}
} // namespace

void UDuTravellerCharacterRetargeterWidget::NativeConstruct()
{
	Super::NativeConstruct();
	BuildLayout();
	RefreshAssetLists();
	RefreshOutputFromReport();
}

void UDuTravellerCharacterRetargeterWidget::BuildLayout()
{
	if (!WidgetTree)
	{
		return;
	}
	UScrollBox* Scroll = NewObject<UScrollBox>(WidgetTree);
	UVerticalBox* Root = NewObject<UVerticalBox>(Scroll);
	Scroll->AddChild(Root);
	WidgetTree->RootWidget = Scroll;

	MakeText(WidgetTree, Root, TEXT("DuTraveller — Meshy Character Retargeter"), true);
	MakeText(WidgetTree, Root, TEXT("Imports rigged Meshy FBX characters, builds IK Rigs + Retargeters,\nretargets the enemy library and generates a working enemy Blueprint."));

	FbxPathEdit = NewObject<UEditableTextBox>(WidgetTree);
	{
		UHorizontalBox* Row = MakeRow(WidgetTree, Root, TEXT("FBX Path"));
		UHorizontalBoxSlot* S = Row->AddChildToHorizontalBox(FbxPathEdit);
		S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		FbxPathEdit->SetHintText(Lbl(TEXT("Absolute path to rigged Meshy .fbx (optional)")));
	}

	{
		UHorizontalBox* Row = MakeRow(WidgetTree, Root, TEXT("Source Rig / Skeleton"));
		SourceRigCombo = MakeCombo(WidgetTree, Row);
	}
	{
		UHorizontalBox* Row = MakeRow(WidgetTree, Root, TEXT("Target Meshy Mesh"));
		TargetMeshCombo = MakeCombo(WidgetTree, Row);
	}
	{
		UHorizontalBox* Row = MakeRow(WidgetTree, Root, TEXT("Animation Source"));
		AnimSourceCombo = MakeCombo(WidgetTree, Row);
		AnimSourceCombo->AddOption(TEXT("Existing Enemy (UE4 Mannequin)"));
		AnimSourceCombo->AddOption(TEXT("UE5 Manny/Quinn"));
		AnimSourceCombo->AddOption(TEXT("UEFN FN_Mannequin"));
		AnimSourceCombo->SetSelectedIndex(0);
	}
	{
		UHorizontalBox* Row = MakeRow(WidgetTree, Root, TEXT("Retarget Direction"));
		DirectionCombo = MakeCombo(WidgetTree, Row);
		DirectionCombo->AddOption(TEXT("Source -> Meshy"));
		DirectionCombo->AddOption(TEXT("Meshy -> Source"));
		DirectionCombo->SetSelectedIndex(0);
	}
	{
		UHorizontalBox* Row = MakeRow(WidgetTree, Root, TEXT("Options"));
		AutoChainsCheck = NewObject<UCheckBox>(WidgetTree);
		AutoChainsCheck->SetIsChecked(true);
		UTextBlock* T1 = NewObject<UTextBlock>(WidgetTree); T1->SetText(Lbl(TEXT(" Auto Chains  ")));
		AutoAlignCheck = NewObject<UCheckBox>(WidgetTree);
		AutoAlignCheck->SetIsChecked(true);
		UTextBlock* T2 = NewObject<UTextBlock>(WidgetTree); T2->SetText(Lbl(TEXT(" Auto Align  ")));
		RootMotionCombo = NewObject<UComboBoxString>(WidgetTree);
		RootMotionCombo->AddOption(TEXT("RootMotion: Generate"));
		RootMotionCombo->AddOption(TEXT("RootMotion: None"));
		RootMotionCombo->AddOption(TEXT("RootMotion: Maintain"));
		RootMotionCombo->SetSelectedIndex(0);
		Row->AddChildToHorizontalBox(AutoChainsCheck);
		Row->AddChildToHorizontalBox(T1);
		Row->AddChildToHorizontalBox(AutoAlignCheck);
		Row->AddChildToHorizontalBox(T2);
		UHorizontalBoxSlot* RMS = Row->AddChildToHorizontalBox(RootMotionCombo);
		RMS->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	}

	MakeButton(WidgetTree, Root, TEXT("Import FBX (rigged)"),
	           [&](UButton* B){ B->OnClicked.AddDynamic(this, &UDuTravellerCharacterRetargeterWidget::OnImportFbx); });
	MakeButton(WidgetTree, Root, TEXT("Generate IK Rig + Retargeter"),
	           [&](UButton* B){ B->OnClicked.AddDynamic(this, &UDuTravellerCharacterRetargeterWidget::OnGenerateRetargeter); });
	MakeButton(WidgetTree, Root, TEXT("Retarget Selected Animations / Library"),
	           [&](UButton* B){ B->OnClicked.AddDynamic(this, &UDuTravellerCharacterRetargeterWidget::OnRetargetLibrary); });
	MakeButton(WidgetTree, Root, TEXT("Duplicate && Adapt Enemy AnimBP"),
	           [&](UButton* B){ B->OnClicked.AddDynamic(this, &UDuTravellerCharacterRetargeterWidget::OnAdaptAnimBP); });
	MakeButton(WidgetTree, Root, TEXT("Create Enemy Blueprint Child"),
	           [&](UButton* B){ B->OnClicked.AddDynamic(this, &UDuTravellerCharacterRetargeterWidget::OnCreateEnemyBP); });
	MakeButton(WidgetTree, Root, TEXT("Validate Character"),
	           [&](UButton* B){ B->OnClicked.AddDynamic(this, &UDuTravellerCharacterRetargeterWidget::OnValidate); });
	MakeButton(WidgetTree, Root, TEXT("Preview Retarget (open retargeter asset)"),
	           [&](UButton* B){ B->OnClicked.AddDynamic(this, &UDuTravellerCharacterRetargeterWidget::OnPreview); });

	StatusText = NewObject<UTextBlock>(WidgetTree);
	StatusText->SetText(Lbl(TEXT("Ready.")));
	Root->AddChild(StatusText);

	OutputLog = NewObject<UMultiLineEditableTextBox>(WidgetTree);
	OutputLog->SetIsReadOnly(true);
	UVerticalBoxSlot* OS = Root->AddChildToVerticalBox(OutputLog);
	OS->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
}

void UDuTravellerCharacterRetargeterWidget::RefreshAssetLists()
{
	FAssetRegistryModule& ARM = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	IAssetRegistry& AR = ARM.Get();
	TArray<FAssetData> Meshes;
	AR.GetAssetsByClass(USkeletalMesh::StaticClass()->GetClassPathName(), Meshes, true);
	auto InImportDir = [](const FString& P)
	{
		return P.Contains(TEXT("/Game/DuTraveller/Characters/Imported/Meshy"))
			|| P.Contains(TEXT("/Game/MeshyImports"))
			|| P.Contains(TEXT("/Game/Characters/"));
	};
	TargetMeshCombo->ClearOptions();
	SourceRigCombo->ClearOptions();
	for (const FAssetData& AD : Meshes)
	{
		const FString P = AD.PackageName.ToString();
		if (InImportDir(P))
		{
			TargetMeshCombo->AddOption(P);
		}
	}
	TArray<FAssetData> Rigs;
	AR.GetAssetsByClass(UObject::StaticClass()->GetClassPathName(), Rigs, true);
	for (const FAssetData& AD : Rigs)
	{
		if (AD.AssetClassPath.GetAssetName() == TEXT("IKRigDefinition"))
		{
			SourceRigCombo->AddOption(AD.PackageName.ToString());
		}
	}
}

FString UDuTravellerCharacterRetargeterWidget::SelectedTargetMesh() const
{
	return TargetMeshCombo ? TargetMeshCombo->GetSelectedOption() : FString();
}

FString UDuTravellerCharacterRetargeterWidget::SelectedSourceKey() const
{
	if (!AnimSourceCombo)
	{
		return TEXT("A");
	}
	const FString S = AnimSourceCombo->GetSelectedOption();
	if (S.Contains(TEXT("Manny"))) return TEXT("B");
	if (S.Contains(TEXT("FN_Mannequin")) || S.Contains(TEXT("UEFN"))) return TEXT("C");
	return TEXT("A");
}

FString UDuTravellerCharacterRetargeterWidget::JsonArgs(const FString& Action) const
{
	const FString Routes = SelectedSourceKey();
	const FString Mesh = SelectedTargetMesh();
	const FString Fbx = FbxPathEdit ? FbxPathEdit->GetText().ToString() : FString();
	const bool bReverse = DirectionCombo && DirectionCombo->GetSelectedIndex() == 1;
	return FString::Printf(
		TEXT("{\"action\":\"%s\",\"mesh\":\"%s\",\"fbx\":\"%s\",\"routes\":[\"%s\"],"
		     "\"auto_chains\":%s,\"auto_align\":%s,\"root_motion\":\"%s\",\"direction\":\"%s\"}"),
		*Action, *Mesh, *Fbx, *Routes,
		(AutoChainsCheck && AutoChainsCheck->IsChecked()) ? TEXT("true") : TEXT("false"),
		(AutoAlignCheck && AutoAlignCheck->IsChecked()) ? TEXT("true") : TEXT("false"),
		RootMotionCombo && RootMotionCombo->GetSelectedIndex() == 1 ? TEXT("none")
			: (RootMotionCombo && RootMotionCombo->GetSelectedIndex() == 2 ? TEXT("maintain") : TEXT("generate")),
		bReverse ? TEXT("meshy_to_source") : TEXT("source_to_meshy"));
}

void UDuTravellerCharacterRetargeterWidget::RunPythonAction(const FString& Action)
{
	const FString ToolsDir = FPaths::ProjectDir() / TEXT("Tools");
	const FString Args = JsonArgs(Action);
	const FString Cmd = FString::Printf(
		TEXT("import sys; sys.path.insert(0, r'%s'); import MeshyRetargetCore; MeshyRetargetCore.euw_run(r'''%s''')"),
		*ToolsDir, *Args);
	if (StatusText)
	{
		StatusText->SetText(FText::Format(FText::FromString(TEXT("Running {0} ...")), FText::FromString(Action)));
	}
	IPythonScriptPlugin::Get()->ExecPythonCommand(*Cmd);
	RefreshOutputFromReport();
	RefreshAssetLists();
}

void UDuTravellerCharacterRetargeterWidget::RefreshOutputFromReport()
{
	const FString Path = UDTMeshyEditorLibrary::RetargeterReportPath();
	const FString Json = UDTMeshyEditorLibrary::ReadTextFile(Path);
	if (Json.IsEmpty())
	{
		return;
	}
	if (OutputLog)
	{
		OutputLog->SetText(FText::FromString(Json));
	}
	if (StatusText)
	{
		StatusText->SetText(FText::FromString(FString::Printf(TEXT("Last report: %s"), *Path)));
	}
}

void UDuTravellerCharacterRetargeterWidget::OnImportFbx()         { RunPythonAction(TEXT("import")); }
void UDuTravellerCharacterRetargeterWidget::OnGenerateRetargeter() { RunPythonAction(TEXT("build_rigs")); }
void UDuTravellerCharacterRetargeterWidget::OnRetargetLibrary()   { RunPythonAction(TEXT("pipeline")); }
void UDuTravellerCharacterRetargeterWidget::OnAdaptAnimBP()       { RunPythonAction(TEXT("pipeline")); }
void UDuTravellerCharacterRetargeterWidget::OnCreateEnemyBP()     { RunPythonAction(TEXT("pipeline")); }
void UDuTravellerCharacterRetargeterWidget::OnValidate()          { RunPythonAction(TEXT("validate")); }
void UDuTravellerCharacterRetargeterWidget::OnPreview()
{
	IPythonScriptPlugin::Get()->ExecPythonCommand(
		TEXT("import unreal; unreal.EditorAssetLibrary.sync_browser_to_objects([unreal.EditorAssetLibrary.find_asset_data('/Game/DuTraveller/Animations/Retargeters').package_name])"));
}
