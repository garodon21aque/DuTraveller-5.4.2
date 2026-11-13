#include "DTAnimationMigrationLibrary.h"
#if WITH_EDITOR
#include "Animation/Skeleton.h"
#include "Engine/SkeletalMesh.h"
#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Serialization/ArchiveReplaceObjectRef.h"

namespace
{
bool LoadLegacySource(USkeleton*& Skeleton, USkeletalMesh*& Mesh)
{
	static bool bRegistered = false;
	if (!bRegistered)
	{
		FPackageName::RegisterMountPoint(TEXT("/AnimToTexture/"), FPaths::Combine(FPaths::EnginePluginsDir(), TEXT("Experimental/AnimToTexture/Content/")));
		bRegistered = true;
	}
	Skeleton = LoadObject<USkeleton>(nullptr, TEXT("/AnimToTexture/Characters/Mannequin/Meshes/SK_Mannequin.SK_Mannequin"));
	Mesh = LoadObject<USkeletalMesh>(nullptr, TEXT("/AnimToTexture/Characters/Mannequin/Meshes/SKM_Mannequin.SKM_Mannequin"));
	if (!Skeleton || !Mesh || Mesh->GetSkeleton() != Skeleton) return false;
	const FReferenceSkeleton& Bones = Skeleton->GetReferenceSkeleton();
	const int32 Pelvis = Bones.FindBoneIndex(TEXT("pelvis"));
	if (Bones.GetNum() < 50 || Bones.GetNum() > 100 || Pelvis == INDEX_NONE || Bones.FindBoneIndex(TEXT("root")) != 0 ||
		Bones.FindBoneIndex(TEXT("spine_03")) == INDEX_NONE || Bones.FindBoneIndex(TEXT("spine_05")) != INDEX_NONE ||
		Bones.FindBoneIndex(TEXT("ik_foot_root")) == INDEX_NONE) return false;
	const float Height = Bones.GetRefBonePose()[Pelvis].GetTranslation().Z;
	return FMath::IsFinite(Height) && Height > 40.f && Height < 150.f;
}
}
#endif

FString UDTAnimationMigrationLibrary::InspectLegacyMannequin()
{
#if WITH_EDITOR
	USkeleton* Skeleton = nullptr; USkeletalMesh* Mesh = nullptr;
	if (!LoadLegacySource(Skeleton, Mesh)) return TEXT("FAILED: source is unavailable or not the expected UE4 reference hierarchy and bind pose");
	const FReferenceSkeleton& Bones = Skeleton->GetReferenceSkeleton();
	return FString::Printf(TEXT("OK bones=%d pelvis=%s skeleton=%s mesh=%s"), Bones.GetNum(),
		*Bones.GetRefBonePose()[Bones.FindBoneIndex(TEXT("pelvis"))].GetTranslation().ToString(), *Skeleton->GetPathName(), *Mesh->GetPathName());
#else
	return TEXT("Editor only");
#endif
}

bool UDTAnimationMigrationLibrary::CopyLegacyMannequin(const TMap<FString, FString>& Packages)
{
#if WITH_EDITOR
	USkeleton* Skeleton = nullptr; USkeletalMesh* Mesh = nullptr;
	if (!LoadLegacySource(Skeleton, Mesh) || Packages.IsEmpty() || Packages.Num() > 64) return false;
	const FString* SkeletonDestination = Packages.Find(TEXT("/AnimToTexture/Characters/Mannequin/Meshes/SK_Mannequin"));
	const FString* MeshDestination = Packages.Find(TEXT("/AnimToTexture/Characters/Mannequin/Meshes/SKM_Mannequin"));
	if (!SkeletonDestination || *SkeletonDestination != TEXT("/Game/Characters/Mannequin_UE4/Meshes/SK_Mannequin_Skeleton") ||
		!MeshDestination || *MeshDestination != TEXT("/Game/Characters/Mannequin_UE4/Meshes/SK_Mannequin")) return false;
	for (const auto& Pair : Packages)
	{
		if (!Pair.Key.StartsWith(TEXT("/AnimToTexture/Characters/Mannequin/")) || !Pair.Value.StartsWith(TEXT("/Game/Characters/Mannequin_UE4/")) ||
			!FPackageName::IsValidLongPackageName(Pair.Key) || !FPackageName::IsValidLongPackageName(Pair.Value) ||
			!FPackageName::DoesPackageExist(Pair.Key) || FPackageName::DoesPackageExist(Pair.Value)) return false;
	}
	// Unreal's copy implementation remaps cross-package and soft references.
	// Neither dirty source packages nor overlapping destination packages are saved.
	return FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools")).Get().AdvancedCopyPackages(Packages, false, false) && FinalizeLegacyMannequin(Packages);
#else
	return false;
#endif
}

bool UDTAnimationMigrationLibrary::FinalizeLegacyMannequin(const TMap<FString, FString>& Packages)
{
#if WITH_EDITOR
	USkeleton* SourceSkeleton = nullptr; USkeletalMesh* SourceMesh = nullptr;
	if (!LoadLegacySource(SourceSkeleton, SourceMesh) || Packages.IsEmpty() || Packages.Num() > 64) return false;
	TMap<UObject*, UObject*> Replacements;
	TMap<FSoftObjectPath, FSoftObjectPath> SoftReplacements;
	TArray<UPackage*> TargetPackages;
	for (const auto& Pair : Packages)
	{
		if (!Pair.Key.StartsWith(TEXT("/AnimToTexture/Characters/Mannequin/")) || !Pair.Value.StartsWith(TEXT("/Game/Characters/Mannequin_UE4/"))) return false;
		const FString SourcePath = Pair.Key + TEXT(".") + FPackageName::GetShortName(Pair.Key);
		const FString TargetPath = Pair.Value + TEXT(".") + FPackageName::GetShortName(Pair.Value);
		UObject* Source = LoadObject<UObject>(nullptr, *SourcePath);
		UObject* Target = LoadObject<UObject>(nullptr, *TargetPath);
		if (!Source || !Target || Source->GetClass() != Target->GetClass()) return false;
		Replacements.Add(Source, Target);
		SoftReplacements.Add(FSoftObjectPath(Source), FSoftObjectPath(Target));
		TargetPackages.AddUnique(Target->GetOutermost());
	}
	for (const auto& Pair : Replacements)
	{
		FArchiveReplaceObjectRef<UObject> FixReferences(Pair.Value, Replacements, EArchiveReplaceObjectFlags::IgnoreOuterRef | EArchiveReplaceObjectFlags::IgnoreArchetypeRef);
		Pair.Value->MarkPackageDirty();
	}
	FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools")).Get().RenameReferencingSoftObjectPaths(TargetPackages, SoftReplacements);
	USkeleton* TargetSkeleton = Cast<USkeleton>(Replacements.FindRef(SourceSkeleton));
	USkeletalMesh* TargetMesh = Cast<USkeletalMesh>(Replacements.FindRef(SourceMesh));
	if (!TargetSkeleton || !TargetMesh) return false;
	TargetMesh->SetSkeleton(TargetSkeleton);
	TargetSkeleton->SetPreviewMesh(TargetMesh);
	return TargetMesh->GetSkeleton() == TargetSkeleton;
#else
	return false;
#endif
}
