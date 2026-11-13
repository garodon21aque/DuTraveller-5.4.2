#include "RPGSkillExecution.h"
#include "RPGCharacter.h"
#include "RPGCombatTypes.h"
#include "RPGActionComponent.h"
#include "RPGAttributeSet.h"
#include "AbilitySystemComponent.h"
#include "EngineUtils.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/StaticMesh.h"
#include "Components/StaticMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"

void URPGSkillExecution::RestoreResources(ARPGCharacter* Character, const float Health, const float Mana)
{
	if (!Character || !Character->HasAuthority() || Character->bIsDead) return;
	UAbilitySystemComponent* ASC = Character->GetAbilitySystemComponent();
	if (!ASC) return;
	FGameplayEffectSpec Spec(GetDefault<URPGConsumableResourceEffect>(), ASC->MakeEffectContext(), 1.f);
	Spec.SetSetByCallerMagnitude(FName("Health"), FMath::Max(0.f, Health));
	Spec.SetSetByCallerMagnitude(FName("Mana"), FMath::Max(0.f, Mana));
	Spec.SetSetByCallerMagnitude(FName("Stamina"), 0.f);
	ASC->ApplyGameplayEffectSpecToSelf(Spec);
}

void URPGSkillExecution::Execute(ARPGCharacter* Source, const URPGCombatAbilityDefinition* D,
	ARPGCharacter* Target, const float Damage, const int32 Rank)
{
	if (!Source || !Source->HasAuthority() || !D || Source->bIsDead) return;
	UWorld* World = Source->GetWorld();
	const FVector Origin = Source->GetActorLocation();
	const FVector Forward = Source->GetBaseAimRotation().Vector();
	FVector AimPoint = IsValid(Target) ? Target->GetActorLocation() : Origin + Forward * D->Range;
	FCollisionQueryParams Query(SCENE_QUERY_STAT(SkillAim), false, Source);
	FHitResult Hit;
	if (!Target && World->LineTraceSingleByChannel(Hit, Origin + FVector(0,0,50), AimPoint, ECC_Visibility, Query)) AimPoint = Hit.ImpactPoint;
	const float Magnitude = D->EffectMagnitude * Rank;
	const float FXLifetime = FMath::Max(1.f, D->EffectDuration);
	// Chain visual endpoints are only known after target selection; spawn below.
	if (D->Execution == ERPGSkillExecution::Heal)
	{
		RestoreResources(Source, Magnitude, 0.f);
		return;
	}
	if (D->Execution == ERPGSkillExecution::Shield)
	{
		UAbilitySystemComponent* ASC = Source->GetAbilitySystemComponent();
		if (!ASC) return;
		FGameplayEffectSpec Spec(GetDefault<URPGSkillArmorEffect>(), ASC->MakeEffectContext(), Rank);
		Spec.SetSetByCallerMagnitude(FName("Duration"), D->EffectDuration);
		Spec.SetSetByCallerMagnitude(FName("Armor"), Magnitude);
		ASC->ApplyGameplayEffectSpecToSelf(Spec);
		return;
	}
	if (D->Execution == ERPGSkillExecution::Blink)
	{
		FVector End = Origin + Forward.GetSafeNormal2D() * D->Range;
		const UCapsuleComponent* Capsule = Source->GetCapsuleComponent();
		if (World->SweepSingleByChannel(Hit, Origin, End, FQuat::Identity, ECC_Pawn,
			FCollisionShape::MakeCapsule(Capsule->GetScaledCapsuleRadius(), Capsule->GetScaledCapsuleHalfHeight()), Query))
			End = Hit.Location - Forward.GetSafeNormal2D() * 10.f;
		Source->TeleportTo(End, Source->GetActorRotation(), false, false);
		return;
	}
	if (D->Execution == ERPGSkillExecution::Wall)
	{
		FActorSpawnParameters Params; Params.Owner = Source;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		AStaticMeshActor* Wall = World->SpawnActor<AStaticMeshActor>(AimPoint + FVector(0,0,100), FRotator(0,Source->GetActorRotation().Yaw,0), Params);
		if (Wall)
		{
			Wall->SetReplicates(true);
			UStaticMeshComponent* Mesh = Wall->GetStaticMeshComponent();
			Mesh->SetMobility(EComponentMobility::Movable);
			Mesh->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")));
			Mesh->SetWorldScale3D(FVector(0.4f, D->EffectRadius / 50.f, 2.f));
			Mesh->SetCollisionProfileName(TEXT("BlockAll"));
			Wall->SetLifeSpan(D->EffectDuration);
		}
		return;
	}
	const bool Area = D->Execution == ERPGSkillExecution::Area;
	TArray<ARPGCharacter*> Candidates;
	for (TActorIterator<ARPGCharacter> It(World); It; ++It)
	{
		ARPGCharacter* Other = *It;
		if (Other == Source || Other->bIsDead || (!D->bAllowFriendlyFire && !Source->FactionId.IsNone() && Source->FactionId == Other->FactionId)) continue;
		const FVector Offset = Other->GetActorLocation() - Origin;
		if (Area ? FVector::DistSquared(Other->GetActorLocation(), AimPoint) > FMath::Square(D->EffectRadius) : Offset.SizeSquared() > FMath::Square(D->Range)) continue;
		if (!Area && D->Execution != ERPGSkillExecution::Chain && D->Execution != ERPGSkillExecution::Cone && Target && Other != Target) continue;
		if (!Area && (!Target || D->Execution == ERPGSkillExecution::Cone) && FVector::DotProduct(Forward.GetSafeNormal2D(), Offset.GetSafeNormal2D()) < 0.65f) continue;
		FHitResult Sight;
		if (World->LineTraceSingleByChannel(Sight, Origin + FVector(0,0,50), Other->GetActorLocation() + FVector(0,0,50), ECC_Visibility, Query) && Sight.GetActor() != Other) continue;
		Candidates.Add(Other);
	}
	Candidates.Sort([Origin](const ARPGCharacter& A, const ARPGCharacter& B) { return FVector::DistSquared(Origin,A.GetActorLocation()) < FVector::DistSquared(Origin,B.GetActorLocation()); });
	if (D->Execution == ERPGSkillExecution::Chain && Candidates.Num() > 3) Candidates.SetNum(3);
	if (D->ReleaseFX)
	{
		if (D->Execution == ERPGSkillExecution::Chain)
		{
			// Beam endpoints resolve here, not to world origin: the visual chains
			// source -> nearest -> next -> next over actual valid targets.
			TArray<FVector_NetQuantize> Points;
			Points.Add(Origin + FVector(0, 0, 60.f));
			for (const ARPGCharacter* Other : Candidates) Points.Add(Other->GetActorLocation() + FVector(0, 0, 50.f));
			if (Points.Num() > 1) Source->MulticastChainFX(D->ReleaseFX, Points, FXLifetime);
		}
		else Source->MulticastSkillFXTimed(D->ReleaseFX, D->Execution == ERPGSkillExecution::Area ? AimPoint : Origin, FXLifetime);
	}
	for (ARPGCharacter* Other : Candidates)
	{
		bool bCrit = false, bBlocked = false;
		const float Applied = URPGActionComponent::ComputeMitigatedDamage(Source, Other, D, Damage, &bCrit, &bBlocked);
		if (Applied > 0.f) Other->ReceiveCombatDamage(Applied, Source, D->AbilityId, bCrit, bBlocked, D->DamageEffect);
		if (D->Execution == ERPGSkillExecution::Drain) RestoreResources(Source, Applied * 0.5f, 0.f);
		if (D->Execution == ERPGSkillExecution::Root && Other->ActionComp) Other->ActionComp->ApplyControlLock(D->EffectDuration);
		if (D->Execution == ERPGSkillExecution::Fear || D->AbilityId == FName("Abilities.Air.PushBack"))
			Other->LaunchCharacter((Other->GetActorLocation() - Origin).GetSafeNormal2D() * Magnitude + FVector(0,0,120), true, true);
	}
}
