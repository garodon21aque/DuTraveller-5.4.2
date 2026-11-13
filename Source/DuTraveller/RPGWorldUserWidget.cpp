#include "RPGWorldUserWidget.h"
#include "RPGCharacter.h"
#include "RPGAttributeSet.h"
#include "AbilitySystemComponent.h"
#include "Blueprint/WidgetTree.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Components/ProgressBar.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/HorizontalBox.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/Texture2D.h"
#include "Components/SkeletalMeshComponent.h"
#include "UObject/UnrealType.h"
#include "RPGInteractionComponent.h"
#include "RPGAICharacter.h"
#include "RPGActionComponent.h"
#include "Components/SizeBox.h"
#include "Components/CanvasPanelSlot.h"

void URPGWorldUserWidget::SetWidgetController(UObject* InWidgetController)
{
	WidgetController = InWidgetController;
	WidgetControllerSet();
}

bool URPGWorldUserWidget::IsStatusIconVisible(FName Status) const
{
	const TObjectPtr<UImage>* Icon = StatusIcons.Find(Status);
	return Icon && *Icon && (*Icon)->IsVisible();
}

void URPGWorldUserWidget::InitializeCharacterOverlay(ARPGCharacter* Character, bool bFollowActor)
{
	AttachedActor = Character;
	bCharacterOverlay = true;
	bFollowCharacter = bFollowActor;
	// Character bars use a full-screen canvas above menus. They are display
	// overlays, so neither their canvas nor children may capture menu clicks.
	SetVisibility(ESlateVisibility::HitTestInvisible);
	if (Character)
	{
		OwningInteractionComponent = Character->FindComponentByClass<URPGInteractionComponent>();
		if (!OwningInteractionComponent)
		{
			OwningInteractionComponent = NewObject<URPGInteractionComponent>(Character);
			Character->AddInstanceComponent(OwningInteractionComponent);
			OwningInteractionComponent->RegisterComponent();
		}
		OwningInteractionComponent->EnemyActor = Cast<ARPGAICharacter>(Character);
		OwningInteractionComponent->ActionComp = Character->ActionComp;
	}
	EnsureStatusRow();
	if (Character && Character->GetRPGAttributes())
		for (const auto& Value : {TPair<FName,float>(TEXT("Health"), Character->GetRPGAttributes()->GetHealth()), TPair<FName,float>(TEXT("MaxHealth"), Character->GetRPGAttributes()->GetMaxHealth())})
			if (FNumericProperty* Property = FindFProperty<FNumericProperty>(GetClass(), Value.Key))
				if (Property->IsFloatingPoint()) Property->SetFloatingPointPropertyValue(Property->ContainerPtrToValuePtr<void>(this), Value.Value);
	SetWidgetController(Character);
}

void URPGWorldUserWidget::EnsureStatusRow()
{
	if (bStatusRowCreated || !WidgetTree || !WidgetTree->RootWidget) return;
	bStatusRowCreated = true;
	USizeBox* Anchor = Cast<USizeBox>(GetWidgetFromName(TEXT("ParentSizeBox")));
	if (Anchor && !Anchor->GetContent()) Anchor = nullptr;
	if (Anchor)
		if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Anchor->Slot))
		{
			CanvasSlot->SetAnchors(FAnchors(0.f,0.f));
			CanvasSlot->SetPosition(FVector2D::ZeroVector);
			CanvasSlot->SetAlignment(FVector2D(.5f,1.f));
			CanvasSlot->SetAutoSize(true);
		}
	UWidget* OriginalRoot = Anchor && Anchor->GetContent() ? Anchor->GetContent() : WidgetTree->RootWidget.Get();
	if (Anchor && Anchor->GetContent()) Anchor->RemoveChild(OriginalRoot);
	UOverlay* Root = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("DT_CharacterOverlay"));
	if (Anchor) Anchor->SetContent(Root); else WidgetTree->RootWidget = Root;
	UOverlaySlot* OriginalSlot = Root->AddChildToOverlay(OriginalRoot);
	OriginalSlot->SetHorizontalAlignment(HAlign_Fill); OriginalSlot->SetVerticalAlignment(VAlign_Fill);
	UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("DT_StatusEffects"));
	UOverlaySlot* RowSlot = Root->AddChildToOverlay(Row);
	RowSlot->SetHorizontalAlignment(HAlign_Center); RowSlot->SetVerticalAlignment(VAlign_Top);
	RowSlot->SetPadding(FMargin(0, -26.f, 0, 0));
	const TPair<FName, const TCHAR*> Icons[] = {
		{TEXT("Stunned"), TEXT("/Game/UI/2D/Skills/Effects/Stunned.Stunned")},
		{TEXT("Burning"), TEXT("/Game/UI/2D/Skills/Effects/fire.fire")},
		{TEXT("Poisoned"), TEXT("/Game/UI/2D/Skills/Effects/Poison.Poison")},
		{TEXT("Frozen"), TEXT("/Game/UI/2D/Skills/Effects/frozen-block.frozen-block")},
		{TEXT("Shielded"), TEXT("/Game/UI/2D/Skills/Effects/ShieldBreak.ShieldBreak")}
	};
	for (const auto& Pair : Icons)
	{
		UImage* Image = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), FName(*FString::Printf(TEXT("DT_Status_%s"), *Pair.Key.ToString())));
		Image->SetBrushFromTexture(LoadObject<UTexture2D>(nullptr, Pair.Value));
		FSlateBrush Brush = Image->GetBrush(); Brush.SetImageSize(FVector2D(24,24)); Image->SetBrush(Brush);
		Image->SetToolTipText(FText::FromName(Pair.Key)); Image->SetVisibility(ESlateVisibility::Collapsed);
		Row->AddChildToHorizontalBox(Image); StatusIcons.Add(Pair.Key, Image);
	}
}

void URPGWorldUserWidget::NativeTick(const FGeometry& Geometry, float DeltaSeconds)
{
	Super::NativeTick(Geometry, DeltaSeconds);
	if (!bCharacterOverlay) return;
	ARPGCharacter* Character = Cast<ARPGCharacter>(AttachedActor);
	if (!IsValid(Character) || Character->bIsDead) { SetVisibility(ESlateVisibility::Collapsed); return; }
	// Retained Blueprint tick graphs can set Visible; restore the display-only
	// policy after Super has dispatched their tick.
	SetVisibility(ESlateVisibility::HitTestInvisible);
	EnsureStatusRow();
	const URPGAttributeSet* Stats = Character->GetRPGAttributes();
	DisplayedHealthPercent = Stats && Stats->GetMaxHealth() > 0.f ? FMath::Clamp(Stats->GetHealth() / Stats->GetMaxHealth(), 0.f, 1.f) : 0.f;
	if (UProgressBar* Bar = Cast<UProgressBar>(GetWidgetFromName(TEXT("HealthBar")))) Bar->SetPercent(DisplayedHealthPercent);
	if (UImage* Bar = Cast<UImage>(GetWidgetFromName(TEXT("HealthBar"))))
		if (UMaterialInstanceDynamic* Material = Bar->GetDynamicMaterial()) Material->SetScalarParameterValue(TEXT("ProgressAlpha"), DisplayedHealthPercent);
	UAbilitySystemComponent* ASC = Character->GetAbilitySystemComponent();
	bool bShield = false;
	if (ASC) for (const FActiveGameplayEffectHandle Handle : ASC->GetActiveEffects(FGameplayEffectQuery()))
		if (const FActiveGameplayEffect* Effect = ASC->GetActiveGameplayEffect(Handle))
			if (Effect->Spec.Def && Effect->Spec.Def->IsA<URPGSkillArmorEffect>()) bShield = true;
	for (const auto& Pair : StatusIcons)
	{
		const FGameplayTag State = FGameplayTag::RequestGameplayTag(FName(*FString::Printf(TEXT("State.%s"), *Pair.Key.ToString())), false);
		const FGameplayTag Status = FGameplayTag::RequestGameplayTag(FName(*FString::Printf(TEXT("Status.%s"), *Pair.Key.ToString())), false);
		const bool bActive = (Pair.Key == TEXT("Stunned") && Character->bIsStunned) || (Pair.Key == TEXT("Shielded") && bShield) ||
			(ASC && ((State.IsValid() && ASC->HasMatchingGameplayTag(State)) || (Status.IsValid() && ASC->HasMatchingGameplayTag(Status))));
		Pair.Value->SetVisibility(bActive ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	USizeBox* AnchorBox = Cast<USizeBox>(GetWidgetFromName(TEXT("ParentSizeBox")));
	if (bFollowCharacter || AnchorBox)
	{
		FVector2D Position = FVector2D::ZeroVector;
		const FVector Head = Character->GetMesh()->GetSkeletalMeshAsset() ? Character->GetMesh()->GetSocketLocation(TEXT("head")) : Character->GetActorLocation()+FVector(0,0,90.f);
		const FVector Anchor = Head + FVector(0,0,bFollowCharacter ? 35.f : 70.f);
		const bool bOnScreen = GetOwningPlayer() && UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(GetOwningPlayer(), Anchor, Position, true);
		if (AnchorBox)
		{
			// Retained world widgets use a full-screen canvas and move their
			// authored ParentSizeBox. Keep it ticking while off screen.
			AnchorBox->SetRenderOpacity(bOnScreen ? 1.f : 0.f);
			if (bOnScreen) AnchorBox->SetRenderTranslation(Position);
		}
		else
		{
			SetRenderOpacity(bOnScreen ? 1.f : 0.f);
			if (bOnScreen) { SetPositionInViewport(Position, false); SetAlignmentInViewport(FVector2D(.5f,1.f)); SetDesiredSizeInViewport(FVector2D(230,65)); }
		}
	}
}
