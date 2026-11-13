#include "DTMeshyEditorLibrary.h"

#include "Animation/Skeleton.h"
#include "EditorUtilitySubsystem.h"
#include "EditorUtilityWidgetBlueprint.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/SkeletalMeshSocket.h"
#include "Animation/AnimNodeBase.h"
#include "Animation/AnimationAsset.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Kismet2/BlueprintEditorUtils.h"

bool UDTMeshyEditorLibrary::AddSkeletalSocket(USkeletalMesh* Mesh, FName SocketName, FName BoneName,
                                              FTransform RelativeTransform)
{
	if (!Mesh || SocketName.IsNone() || BoneName.IsNone() || Mesh->FindSocket(SocketName))
	{
		return false;
	}
	USkeletalMeshSocket* Socket = NewObject<USkeletalMeshSocket>(Mesh);
	if (!Socket)
	{
		return false;
	}
	Socket->SocketName = SocketName;
	Socket->BoneName = BoneName;
	Socket->RelativeLocation = RelativeTransform.GetLocation();
	Socket->RelativeRotation = RelativeTransform.GetRotation().Rotator();
	Socket->RelativeScale = RelativeTransform.GetScale3D();
	Mesh->AddSocket(Socket);
	Mesh->MarkPackageDirty();
	return true;
}

TArray<FName> UDTMeshyEditorLibrary::EnumerateBoneNames(USkeletalMesh* Mesh)
{
	TArray<FName> Out;
	if (!Mesh)
	{
		return Out;
	}
	const FReferenceSkeleton& Ref = Mesh->GetRefSkeleton();
	const int32 Num = Ref.GetNum();
	Out.Reserve(Num);
	for (int32 i = 0; i < Num; ++i)
	{
		Out.Add(Ref.GetBoneName(i));
	}
	return Out;
}

bool UDTMeshyEditorLibrary::FileExists(const FString& Path)
{
	return FPaths::FileExists(Path);
}

FString UDTMeshyEditorLibrary::ReadTextFile(const FString& Path)
{
	FString Contents;
	FFileHelper::LoadFileToString(Contents, *Path);
	return Contents;
}

FString UDTMeshyEditorLibrary::RetargeterReportPath()
{
	return FPaths::ProjectSavedDir() / TEXT("Automation/MeshyRetargetUI.json");
}

UEditorUtilityWidget* UDTMeshyEditorLibrary::SpawnRetargeterEUW()
{
	const FString AssetPath = TEXT("/Game/DuTraveller/Editor/Retargeting/EUW_DuTraveller_CharacterRetargeter");
	UEditorUtilityWidgetBlueprint* BP = LoadObject<UEditorUtilityWidgetBlueprint>(nullptr, *AssetPath);
	if (!BP)
	{
		return nullptr;
	}
	UEditorUtilitySubsystem* Sub = GEditor ? GEditor->GetEditorSubsystem<UEditorUtilitySubsystem>() : nullptr;
	FName TabId;
	return Sub ? Sub->SpawnAndRegisterTabAndGetID(BP, TabId) : nullptr;
}

int32 UDTMeshyEditorLibrary::RemapAnimBlueprintAnimations(const FString& AnimBlueprintPath,
                                                        const TMap<FString, FString>& SourceToTarget)
{
	UBlueprint* BP = LoadObject<UBlueprint>(nullptr, *AnimBlueprintPath);
	if (!BP)
	{
		return 0;
	}
	int32 Remapped = 0;
	TArray<UEdGraph*> Graphs;
	BP->GetAllGraphs(Graphs);
	for (UEdGraph* Graph : Graphs)
	{
		for (UEdGraphNode* Node : Graph->Nodes)
		{
			// Anim graph nodes embed the anim node as a USTRUCT field (e.g.
			// 'Node' of type UAnimNode_SequencePlayer). Swap any UAnimationAsset
			// object property found inside those embedded structs via pure
			// reflection — no AnimGraph module dependency needed.
			for (TFieldIterator<FStructProperty> SP(Node->GetClass()); SP; ++SP)
			{
				void* StructVal = SP->ContainerPtrToValuePtr<void>(Node);
				for (TFieldIterator<FObjectProperty> OP(SP->Struct); OP; ++OP)
				{
					UObject* Stored = OP->GetObjectPropertyValue_InContainer(StructVal);
					UAnimationAsset* Current = Cast<UAnimationAsset>(Stored);
					if (!Current)
					{
						continue;
					}
					FString Cur = Current->GetPathName();
					const int32 Dot = Cur.Find(TEXT("."), ESearchCase::IgnoreCase,
					                           ESearchDir::FromStart);
					if (Dot != INDEX_NONE)
					{
						Cur = Cur.Left(Dot);
					}
					const FString* Mapped = SourceToTarget.Find(Cur);
					if (!Mapped)
					{
						Mapped = SourceToTarget.Find(Current->GetPathName());
					}
					UAnimationAsset* Replacement = Mapped
						? Cast<UAnimationAsset>(LoadObject<UObject>(nullptr, **Mapped))
						: nullptr;
					if (!Replacement)
					{
						continue;
					}
					OP->SetObjectPropertyValue_InContainer(StructVal, Replacement);
					++Remapped;
				}
			}
		}
	}
	if (Remapped > 0)
	{
		BP->Modify();
		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
	}
	return Remapped;
}

AActor* UDTMeshyEditorLibrary::SpawnActorInWorld(UObject* WorldObject, UClass* ActorClass, FVector Location)
{
	UWorld* W = Cast<UWorld>(WorldObject);
	return (W && ActorClass) ? W->SpawnActor<AActor>(ActorClass, Location, FRotator::ZeroRotator) : nullptr;
}
