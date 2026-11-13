#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "RPGAction.generated.h"

class URPGAction;
class URPGActionComponent;
class UTexture2D;

/** Skill category retained on RPGAction (referenced by the player-skill HUD widget). */
UENUM(BlueprintType)
enum class ESkillClass : uint8
{
	ESC_None UMETA(DisplayName = "None"),
	ESC_Melee UMETA(DisplayName = "Melee"),
	ESC_Spell UMETA(DisplayName = "Spell"),
	ESC_Passive UMETA(DisplayName = "Passive"),
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnActionStateChangedSignature, URPGAction*, Action);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnActionStoppedSignature, URPGAction*, Action);

/** Base legacy action object; concrete behavior lives in Blueprint children. */
UCLASS(BlueprintType, Blueprintable, EditInlineNew, DefaultToInstanced)
class DUTRAVELLER_API URPGAction : public UObject
{
	GENERATED_BODY()

public:
	virtual UWorld* GetWorld() const override;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Action") FName Actionname;
	UPROPERTY(BlueprintReadOnly, Category="Action") TObjectPtr<AActor> TargetActor;
	UPROPERTY(BlueprintReadOnly, Category="Action") bool bIsRunning = false;
	UFUNCTION(BlueprintPure, Category="Action") bool IsRunning() const { return bIsRunning; }
	UFUNCTION(BlueprintNativeEvent, Category="Action") bool CanStart(AActor* Instigator);
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category="Action") void StartAction(AActor* Instigator);
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category="Action") void StopAction(AActor* Instigator);
	UPROPERTY(BlueprintAssignable, Category = "Action")
	FOnActionStateChangedSignature OnActionStateChanged;

	UPROPERTY(BlueprintAssignable, Category = "Action")
	FOnActionStoppedSignature OnActionStopped;

	UPROPERTY(BlueprintReadOnly, Category = "Action")
	TObjectPtr<URPGActionComponent> OwningComponent;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action")
	TObjectPtr<UTexture2D> Icon;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action")
	TObjectPtr<UTexture2D> Thumbnail;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action")
	float Duration = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action")
	ESkillClass SkillClass = ESkillClass::ESC_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action")
	FText SkillDisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action")
	FText SkillDescription;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action")
	int32 SkillLevel = 0;

	UFUNCTION(BlueprintPure, Category = "Action")
	URPGActionComponent* GetOwningComponent() const { return OwningComponent; }
};

/** Timed action effect retained by world effect-slot widgets (duration, icon, stop notification). */
UCLASS(BlueprintType, Blueprintable, EditInlineNew, DefaultToInstanced)
class DUTRAVELLER_API URPGActionEffect : public URPGAction
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "Effect")
	float GetTimeRemaining() const;

private:
	float StartTime = 0.f;
	bool bStarted = false;
};
