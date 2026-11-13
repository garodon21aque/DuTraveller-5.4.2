#include "Consumable_ItemObject.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "RPGAttributeSet.h"

bool UConsumable_ItemObject::ApplyUseEffect_Implementation(AActor* User)
{
	if (!IsValid(User) || !User->HasAuthority() || !UseEffect || !FMath::IsFinite(EffectLevel) || EffectLevel <= 0.f) return false;
	UAbilitySystemComponent* ASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(User);
	if (!ASC) return false;
	const UGameplayEffect* Effect = UseEffect->GetDefaultObject<UGameplayEffect>();
	FGameplayEffectContextHandle Context = ASC->MakeEffectContext();
	Context.AddSourceObject(this);
	FGameplayEffectSpec Spec(Effect, Context, EffectLevel);
	if (UseEffect->IsChildOf(URPGConsumableResourceEffect::StaticClass()))
	{
		Spec.SetSetByCallerMagnitude(FName("Health"), 0.f);
		Spec.SetSetByCallerMagnitude(FName("Mana"), 0.f);
		Spec.SetSetByCallerMagnitude(FName("Stamina"), 0.f);
		bool bHasRestoration = false;
		for (const auto& Magnitude : SetByCallerMagnitudes)
		{
			if ((Magnitude.Key != FName("Health") && Magnitude.Key != FName("Mana") && Magnitude.Key != FName("Stamina")) ||
				!FMath::IsFinite(Magnitude.Value) || Magnitude.Value < 0.f) return false;
			bHasRestoration |= Magnitude.Value > 0.f;
		}
		if (!bHasRestoration) return false;
	}
	for (const auto& Magnitude : SetByCallerMagnitudes)
	{
		if (!FMath::IsFinite(Magnitude.Value)) return false;
		Spec.SetSetByCallerMagnitude(Magnitude.Key, Magnitude.Value);
	}
	const FActiveGameplayEffectHandle Applied = ASC->ApplyGameplayEffectSpecToSelf(Spec);
	return Applied.WasSuccessfullyApplied();
}
