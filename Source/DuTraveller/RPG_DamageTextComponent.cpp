#include "RPG_DamageTextComponent.h"

#include "Blueprint/UserWidget.h"
#include "Engine/World.h"
#include "UObject/UnrealType.h"

void URPG_DamageTextComponent::ShowDamageText(const float Damage, const bool bBlockedHit, const bool bCriticalHit)
{
	if (UUserWidget* PopupWidget = GetUserWidgetObject())
	{
		static const FName Candidates[] = {TEXT("Update DamageText"), TEXT("UpdateDamageText"), TEXT("Update_DamageText")};
		UFunction* Update = nullptr;
		for (const FName& Name : Candidates)
			if (UFunction* Fn = PopupWidget->FindFunction(Name)) { Update = Fn; break; }
		if (Update)
		{
			uint8* Params = static_cast<uint8*>(FMemory::Malloc(Update->ParmsSize));
			FMemory::Memzero(Params, Update->ParmsSize);
			for (TFieldIterator<FProperty> It(Update); It; ++It)
			{
				const FString Name = It->GetName();
				if (FNumericProperty* Num = CastField<FNumericProperty>(*It); Num && Name == TEXT("Damage"))
					Num->SetFloatingPointPropertyValue(It->ContainerPtrToValuePtr<void>(Params), Damage);
				else if (FBoolProperty* Bool = CastField<FBoolProperty>(*It))
					if (Name == TEXT("BlockedHit") || Name == TEXT("bBlockedHit") || Name == TEXT("Blocked"))
						Bool->SetPropertyValue(It->ContainerPtrToValuePtr<void>(Params), bBlockedHit);
					else if (Name == TEXT("CriticalHit") || Name == TEXT("bCriticalHit") || Name == TEXT("Critical"))
						Bool->SetPropertyValue(It->ContainerPtrToValuePtr<void>(Params), bCriticalHit);
			}
			PopupWidget->ProcessEvent(Update, Params);
			FMemory::Free(Params);
		}
	}
	if (UWorld* World = GetWorld())
		World->GetTimerManager().SetTimer(DestroyTimer,
			FTimerDelegate::CreateWeakLambda(this, [this]() { DestroyComponent(); }),
			FMath::Max(0.1f, PopupLifetime), false);
}
