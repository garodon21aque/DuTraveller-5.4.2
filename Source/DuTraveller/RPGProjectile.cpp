#include "RPGProjectile.h"

#include "GameplayEffect.h"
#include "Components/SphereComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "Net/UnrealNetwork.h"
#include "RPGActionComponent.h"
#include "RPGCharacter.h"
#include "RPGCombatTypes.h"
#include "TimerManager.h"
#include "Particles/ParticleSystemComponent.h"

ARPGProjectile::ARPGProjectile()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	SetReplicateMovement(true);
	InitialLifeSpan = MaxLifeSeconds;
	SetNetUpdateFrequency(30.f);

	Collision = CreateDefaultSubobject<USphereComponent>(TEXT("Collision"));
	SetRootComponent(Collision);
	Collision->InitSphereRadius(18.f);
	Collision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Collision->SetCollisionObjectType(ECC_WorldDynamic);
	Collision->SetCollisionResponseToAllChannels(ECR_Ignore);
	Collision->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
	Collision->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
	Collision->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Block);
	Collision->SetGenerateOverlapEvents(false);
	Collision->SetNotifyRigidBodyCollision(true);
	Collision->OnComponentHit.AddUniqueDynamic(this, &ThisClass::OnProjectileHit);

	Movement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("Movement"));
	Movement->InitialSpeed = LaunchSpeed;
	Movement->MaxSpeed = LaunchSpeed;
	Movement->bRotationFollowsVelocity = true;
	Movement->bShouldBounce = false;
	Movement->ProjectileGravityScale = 0.f;
	Movement->OnProjectileStop.AddUniqueDynamic(this, &ThisClass::OnProjectileStopped);
}

void ARPGProjectile::BeginPlay()
{
	Super::BeginPlay();
	SetLifeSpan(FMath::Max(0.1f, MaxLifeSeconds));
	if (TrailFX && GetNetMode() != NM_DedicatedServer)
	{
		UNiagaraComponent* Trail = NewObject<UNiagaraComponent>(this, TEXT("TrailFX"));
		Trail->SetupAttachment(Collision);
		Trail->SetAsset(TrailFX);
		Trail->RegisterComponent();
	}
}

void ARPGProjectile::InitializeProjectile(ARPGCharacter* Source, const URPGCombatAbilityDefinition* Definition,
                                          const float InDamage, const int32 InRank)
{
	SourceCharacter = Source;
	Ability = Definition;
	Damage = InDamage;
	Rank = InRank;
	AbilityId = Definition ? Definition->AbilityId : NAME_None;
	const float Speed = Definition && Definition->ProjectileSpeed > 0.f ? Definition->ProjectileSpeed : LaunchSpeed;
	Movement->InitialSpeed = Speed;
	Movement->MaxSpeed = Speed;
	Movement->Velocity = GetActorForwardVector() * Speed;
	SetLifeSpan(FMath::Max(0.1f, MaxLifeSeconds));
	if (Source)
	{
		Collision->IgnoreActorWhenMoving(Source, true);
		SetOwner(Source);
	}
	UE_LOG(LogRPGCombat, Log, TEXT("[Combat] Server projectile spawned owner=%s class=%s speed=%.0f"),
	       *GetNameSafe(Source), *GetName(), Speed);
}

void ARPGProjectile::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ARPGProjectile, bImpacted);
	DOREPLIFETIME(ARPGProjectile, ImpactLocation);
}

bool ARPGProjectile::IsLiveThreat() const { return !bImpacted && !IsActorBeingDestroyed() && Movement && Movement->IsActive() && !Movement->Velocity.IsNearlyZero(); }
ARPGCharacter* ARPGProjectile::GetThreatSource() const { return SourceCharacter.IsValid() ? SourceCharacter.Get() : Cast<ARPGCharacter>(GetOwner()); }

void ARPGProjectile::OnProjectileHit(UPrimitiveComponent* HitComponent, AActor* OtherActor,
                                     UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
	if (bImpacted || !HasAuthority()) return;
	ARPGCharacter* Source = SourceCharacter.Get();
	if (OtherActor && OtherActor == Source) return;
	if (HasAuthority())
	{
		if (ARPGCharacter* Target = Cast<ARPGCharacter>(OtherActor))
		{
			if (IsValid(Source) && IsValid(Target) && !Target->bIsDead)
			{
				const URPGCombatAbilityDefinition* Definition = Ability.Get();
				const bool bHostile = !Definition || Definition->bAllowFriendlyFire ||
					Source->FactionId.IsNone() || Source->FactionId != Target->FactionId;
				if (bHostile)
				{
					bool bCrit = false, bBlocked = false;
					const float Mitigated = URPGActionComponent::ComputeMitigatedDamage(Source, Target, Definition, Damage, &bCrit, &bBlocked);
					UE_LOG(LogRPGCombat, Log, TEXT("[Combat] Projectile collision detected target=%s damage=%.1f"),
					       *Target->GetName(), Mitigated);
					if (Mitigated > 0.f) Target->ReceiveCombatDamage(Mitigated, Source, AbilityId, bCrit, bBlocked,
						Definition ? Definition->DamageEffect.Get() : nullptr);
				}
			}
		}
		else
		{
			UE_LOG(LogRPGCombat, Verbose, TEXT("[Combat] Projectile hit non-character %s"), *GetNameSafe(OtherActor));
		}
	}
	bImpacted = true;
	ImpactLocation = Hit.ImpactPoint;
	PlayImpactFX(Hit.ImpactPoint);
	ImpactPresentation(OtherActor, Hit.ImpactPoint);
	StopFlightPresentation();
	ForceNetUpdate();
	Movement->StopMovementImmediately();
	Collision->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	// Give clients an update containing the impact before the replicated destroy.
	SetLifeSpan(0.25f);
}

void ARPGProjectile::OnRep_Impacted()
{
	if (bImpacted)
	{
		PlayImpactFX(ImpactLocation);
		ImpactPresentation(nullptr, ImpactLocation);
		StopFlightPresentation();
	}
}

void ARPGProjectile::OnProjectileStopped(const FHitResult& Hit)
{
	OnProjectileHit(Collision, Hit.GetActor(), Hit.GetComponent(), FVector::ZeroVector, Hit);
}

void ARPGProjectile::StopFlightPresentation()
{
	Movement->StopMovementImmediately();
	Movement->Deactivate();
	Collision->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SetActorHiddenInGame(true);
	TInlineComponentArray<UNiagaraComponent*> Niagara(this);
	for (UNiagaraComponent* Component : Niagara) Component->DeactivateImmediate();
	TInlineComponentArray<UParticleSystemComponent*> Particles(this);
	for (UParticleSystemComponent* Component : Particles) Component->DeactivateSystem();
}

void ARPGProjectile::PlayImpactFX(const FVector& Location) const
{
	if (GetNetMode() == NM_DedicatedServer) return;
	if (ImpactFX)
	{
		UNiagaraComponent* FX = UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, ImpactFX, Location, FRotator::ZeroRotator, FVector::OneVector, true, true);
		// Some retained Blueprint effects loop forever; AutoDestroy alone never
		// completes those systems. Bound detached impact presentation explicitly.
		TWeakObjectPtr<UNiagaraComponent> WeakFX = FX;
		FTimerHandle Handle;
		GetWorld()->GetTimerManager().SetTimer(Handle, FTimerDelegate::CreateLambda([WeakFX]()
		{
			if (UNiagaraComponent* Component = WeakFX.Get())
			{
				Component->DeactivateImmediate();
				// Deactivation can synchronously complete and auto-destroy the
				// system. Resolve the weak handle again before touching it.
				if (UNiagaraComponent* Remaining = WeakFX.Get()) Remaining->DestroyComponent();
			}
		}), FMath::Max(0.05f, ImpactFXLifetime), false);
	}
	if (ImpactSound)
		UGameplayStatics::PlaySoundAtLocation(this, ImpactSound, Location);
}
