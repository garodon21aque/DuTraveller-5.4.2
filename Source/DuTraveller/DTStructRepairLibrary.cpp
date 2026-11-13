#include "DTStructRepairLibrary.h"
#include "RPGCharacter.h"
#include "RPGAttributeSet.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "GameFramework/PlayerController.h"
#include "Engine/LocalPlayer.h"
#include "EnhancedInputSubsystems.h"
#include "InputAction.h"
#include "InputActionValue.h"
#include "InputKeyEventArgs.h"
#include "RPGNPCCharacter.h"
#include "Engine/World.h"
#include "NiagaraComponent.h"
#include "RPGProjectile.h"
#include "UObject/UObjectIterator.h"
#include "NiagaraSystem.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Framework/Application/SlateApplication.h"
#include "Components/Widget.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "RPGPlayerController.h"
#include "RPGPlayerCharacter.h"
#include "RPGPlayerState.h"
#include "RPGSkillComponent.h"
#include "RPGAbilitySystemComponent.h"
#include "RPGActionComponent.h"
#include "RPGOverlayWidgetController.h"
#include "Layout/WidgetPath.h"
#include "Framework/Application/SlateUser.h"
#include "Slate/SObjectWidget.h"

UDTProgressionEventProbe* UDTStructRepairLibrary::ObserveProgression(ARPGPlayerState* PlayerState, URPGOverlayWidgetController* Overlay)
{
#if WITH_EDITOR
	if (!PlayerState || !Overlay) return nullptr;
	UDTProgressionEventProbe* Probe=NewObject<UDTProgressionEventProbe>(GetTransientPackage());
	PlayerState->OnPlayerLevelChangedDelegate.AddDynamic(Probe,&UDTProgressionEventProbe::RecordPlayerLevel);
	Overlay->OnPlayerLevelChangedDelegate.AddDynamic(Probe,&UDTProgressionEventProbe::RecordOverlayLevel);
	return Probe;
#else
	return nullptr;
#endif
}

TMap<FString,float> UDTStructRepairLibrary::MeasureRuntimeGroundClearance(AActor* Actor)
{
	TMap<FString,float> Result;
#if WITH_EDITOR
	if (!IsValid(Actor) || !Actor->GetWorld() || Actor->GetWorld()->WorldType != EWorldType::PIE) return Result;
	FVector Center, Extent; Actor->GetActorBounds(false, Center, Extent);
	FCollisionQueryParams Query(SCENE_QUERY_STAT(DTGroundMeasurement), false, Actor);
	if (Actor->GetOwner()) Query.AddIgnoredActor(Actor->GetOwner());
	FHitResult Hit;
	if (Actor->GetWorld()->LineTraceSingleByObjectType(Hit,Center+FVector(0,0,200),Center-FVector(0,0,2000),FCollisionObjectQueryParams(ECC_WorldStatic),Query))
	{
		Result.Add(TEXT("ground"),Hit.ImpactPoint.Z);
		Result.Add(TEXT("bottom"),Center.Z-Extent.Z);
		Result.Add(TEXT("clearance"),Center.Z-Extent.Z-Hit.ImpactPoint.Z);
	}
#endif
	return Result;
}

TArray<UWidget*> UDTStructRepairLibrary::GetRuntimeUIWidgets(UUserWidget* Root)
{
	TArray<UWidget*> Result;
#if WITH_EDITOR
	if (!Root || !Root->GetWorld() || Root->GetWorld()->WorldType != EWorldType::PIE || !Root->WidgetTree) return Result;
	Root->WidgetTree->GetAllWidgets(Result);
	const TArray<UWidget*> Children = Result;
	for (UWidget* Child : Children)
		if (UUserWidget* Nested = Cast<UUserWidget>(Child)) Result.Append(GetRuntimeUIWidgets(Nested));
#endif
	return Result;
}

bool UDTStructRepairLibrary::SendRuntimeUIKey(APlayerController* Controller, FKey Key)
{
#if WITH_EDITOR
	if (!Controller || !Controller->GetWorld() || Controller->GetWorld()->WorldType != EWorldType::PIE || !FSlateApplication::IsInitialized()) return false;
	FSlateApplication& Slate = FSlateApplication::Get();
	const FKeyEvent Event(Key, FModifierKeysState(), 0, false, 0, 0);
	ARPGPlayerController* PC = Cast<ARPGPlayerController>(Controller);
	UUserWidget* Panel = PC ? PC->Panels.FindRef(PC->ActivePanel) : nullptr;
	FWidgetPath Path;
	if (!Panel || !Slate.GeneratePathToWidgetUnchecked(Panel->TakeWidget(), Path)) return false;
	// Route within this PIE widget path. Global keyboard preprocessing also
	// executes editor shortcuts (Escape ends PIE), which is outside this probe.
	for (int32 Index=0; Index<Path.Widgets.Num(); ++Index)
		if (Path.Widgets[Index].Widget->OnPreviewKeyDown(Path.Widgets[Index].Geometry, Event).IsEventHandled()) return true;
	for (int32 Index=Path.Widgets.Num()-1; Index>=0; --Index)
		if (Path.Widgets[Index].Widget->OnKeyDown(Path.Widgets[Index].Geometry, Event).IsEventHandled()) return true;
	return false;
#else
	return false;
#endif
}

bool UDTStructRepairLibrary::ClickRuntimeUIWidget(UWidget* Widget, FKey Button, FVector2D NormalizedPosition)
{
#if WITH_EDITOR
	if (!Widget || !Widget->GetWorld() || Widget->GetWorld()->WorldType != EWorldType::PIE || !FSlateApplication::IsInitialized()) return false;
	const FGeometry& Geometry = Widget->GetCachedGeometry();
	const FVector2D Position = Geometry.LocalToAbsolute(Geometry.GetLocalSize() * NormalizedPosition);
	FSlateApplication& Slate = FSlateApplication::Get();
	FWidgetPath Path;
	if (!Slate.GeneratePathToWidgetUnchecked(Widget->TakeWidget(), Path)) return false;
	// Generated geometry paths have no hit-test pointer entries. Slate's
	// routing policies require one optional virtual position per path widget.
	TArray<FWidgetAndPointer> PointerWidgets;
	for (int32 Index=0; Index<Path.Widgets.Num(); ++Index)
		PointerWidgets.Emplace(Path.Widgets[Index], TOptional<FVirtualPointerPosition>());
	Path = FWidgetPath(MakeArrayView(PointerWidgets));
	// A virtual Slate user supports pointer capture in unattended PIE without
	// stealing the desktop user's cursor or requiring the editor to be active.
	const TSharedRef<FSlateVirtualUserHandle> User = Slate.FindOrCreateVirtualUser(0);
	// Use the screen hit path so covering widgets cannot be bypassed by this probe.
	// Resolve for the local player's real Slate user. The virtual routing user
	// is outside SPlayerLayer's hit-test user and cannot discover its children.
	Path = Slate.LocateWindowUnderMouse(Position, {Path.GetWindow()}, false, 0);
	if (!Path.IsValid()) return false;
	UE_LOG(LogTemp, Display, TEXT("DTUIHIT requested=%s position=%s size=%s"), *Widget->GetName(), *Position.ToString(), *Geometry.GetLocalSize().ToString());
	for (int32 Index=0; Index<Path.Widgets.Num(); ++Index)
	{
		UE_LOG(LogTemp, Display, TEXT("DTUIHIT %s"), *Path.Widgets[Index].Widget->ToString());
		if (Path.Widgets[Index].Widget->GetType()==FName(TEXT("SObjectWidget")))
			UE_LOG(LogTemp, Display, TEXT("DTUIHIT object=%s"), *GetPathNameSafe(StaticCastSharedRef<SObjectWidget>(Path.Widgets[Index].Widget)->GetWidgetObject()));
	}
	// Resolve the screen hit exactly as a mouse would. A generated path to the
	// requested button bypasses overlays and can incorrectly report success.
	Path = Slate.LocateWindowUnderMouse(Position, {Path.GetWindow()}, false, User->GetUserIndex());
	if (!Path.IsValid()) return false;
	const FVector2D Previous = Slate.GetCursorPos();
	const FPointerEvent Move(User->GetUserIndex(), 0, Position, Previous, TSet<FKey>(), FKey(), 0, FModifierKeysState());
	Slate.RoutePointerMoveEvent(Path, Move, true);
	const FPointerEvent Down(User->GetUserIndex(), 0, Position, Position, TSet<FKey>{Button}, Button, 0, FModifierKeysState());
	const bool bHandled = Slate.RoutePointerDownEvent(Path, Down).IsEventHandled();
	const FPointerEvent Up(User->GetUserIndex(), 0, Position, Position, TSet<FKey>(), Button, 0, FModifierKeysState());
	Slate.RoutePointerUpEvent(Path, Up);
	return bHandled;
#else
	return false;
#endif
}

AActor* UDTStructRepairLibrary::SpawnRuntimeImpactProbe(ARPGCharacter* Source, int32 Index)
{
#if WITH_EDITOR
	if (!Source || !Source->HasAuthority() || !Source->GetWorld() || Source->GetWorld()->WorldType != EWorldType::PIE || Index < 0 || Index >= 20) return nullptr;
	UClass* Class = LoadClass<ARPGProjectile>(nullptr, TEXT("/Game/Blueprints/Gameplay/Projectiles/BP_Projectile_FireBolt.BP_Projectile_FireBolt_C"));
	if (!Class) return nullptr;
	const FTransform Transform(FRotator::ZeroRotator, Source->GetActorLocation() + FVector((Index%5)*80.f,600.f+(Index/5)*80.f,250.f));
	ARPGProjectile* Projectile = Source->GetWorld()->SpawnActorDeferred<ARPGProjectile>(Class,Transform,Source,Source,ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!Projectile) return nullptr;
	Projectile->SetFlags(RF_Transient);
	Projectile->ImpactFX = LoadObject<UNiagaraSystem>(nullptr, Index%2 ? TEXT("/Game/Art/Niagara/Spells/FIre/FireBolt.FireBolt") : TEXT("/Game/Art/Niagara/Explode/Fire_Burst.Fire_Burst"));
	Projectile->ImpactFXLifetime = .05f;
	UGameplayStatics::FinishSpawningActor(Projectile,Transform);
	Projectile->Movement->Velocity = FVector(0,0,-1400);
	return Projectile;
#else
	return nullptr;
#endif
}

AActor* UDTStructRepairLibrary::SpawnRuntimeActor(ARPGCharacter* Source, const FString& ClassPath, const FVector& Offset)
{
#if WITH_EDITOR
	if (!Source || !Source->GetWorld() || Source->GetWorld()->WorldType != EWorldType::PIE) return nullptr;
	UClass* Class = LoadClass<AActor>(nullptr, *ClassPath);
	if (!Class) return nullptr;
	const FTransform Transform(Source->GetActorRotation(), Source->GetActorLocation() + Offset);
	AActor* Actor = Source->GetWorld()->SpawnActorDeferred<AActor>(Class, Transform, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn);
	if (!Actor) return nullptr;
	Actor->SetFlags(RF_Transient);
	UGameplayStatics::FinishSpawningActor(Actor, Transform);
	return Actor;
#else
	return nullptr;
#endif
}

TMap<FString,int32> UDTStructRepairLibrary::CountRuntimeProjectileFX(ARPGCharacter* Character)
{
	TMap<FString,int32> Counts;
	Counts.Add(TEXT("projectiles"),0); Counts.Add(TEXT("hidden_projectiles"),0); Counts.Add(TEXT("active_niagara"),0);
	if (!Character || !Character->GetWorld()->IsGameWorld()) return Counts;
	for (TObjectIterator<ARPGProjectile> It; It; ++It)
		if (IsValid(*It) && It->GetWorld()==Character->GetWorld())
		{
			++Counts[TEXT("projectiles")];
			if (It->IsHidden()) ++Counts[TEXT("hidden_projectiles")];
		}
	for (TObjectIterator<UNiagaraComponent> It; It; ++It)
		if (IsValid(*It) && It->GetWorld()==Character->GetWorld() && It->IsActive()) ++Counts[TEXT("active_niagara")];
	return Counts;
}

ARPGCharacter* UDTStructRepairLibrary::SpawnRuntimeCombatTarget(ARPGCharacter* Source, const float Distance)
{
	if (!Source || !Source->HasAuthority() || !Source->GetWorld()->IsGameWorld()) return nullptr;
	FActorSpawnParameters Params;
	Params.ObjectFlags |= RF_Transient;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ARPGNPCCharacter* Target = Source->GetWorld()->SpawnActor<ARPGNPCCharacter>(
		Source->GetActorLocation() + Source->GetActorForwardVector() * Distance, FRotator::ZeroRotator, Params);
	if (Target)
	{
		Target->FactionId = FName("Enemy");
		Target->GetCharacterMovement()->DisableMovement();
	}
	return Target;
}

bool UDTStructRepairLibrary::InjectRuntimeInput(APlayerController* Controller, const FString& ActionName, const FVector Value)
{
	if (Controller && !ActionName.StartsWith(TEXT("IA_")))
	{
		FInputKeyEventArgs Event;
		Event.Key = FKey(*ActionName);
		Event.Event = Value.IsNearlyZero() ? IE_Released : IE_Pressed;
		return Event.Key.IsValid() && Controller->InputKey(Event);
	}
	ULocalPlayer* Player = Controller ? Controller->GetLocalPlayer() : nullptr;
	UEnhancedInputLocalPlayerSubsystem* Input = Player ? Player->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>() : nullptr;
	const FString Path = TEXT("/Game/Input/") + ActionName + TEXT(".") + ActionName;
	const UInputAction* Action = LoadObject<UInputAction>(nullptr, *Path);
	if (!Input || !Action) return false;
	FInputActionValue Data;
	switch (Action->ValueType)
	{
		case EInputActionValueType::Boolean: Data = FInputActionValue(!Value.IsNearlyZero()); break;
		case EInputActionValueType::Axis1D: Data = FInputActionValue(static_cast<float>(Value.X)); break;
		case EInputActionValueType::Axis2D: Data = FInputActionValue(FVector2D(Value.X, Value.Y)); break;
		default: Data = FInputActionValue(Value); break;
	}
	Input->InjectInputForAction(Action, Data, {}, {});
	return true;
}

float UDTStructRepairLibrary::ReadRuntimeAttribute(AActor* Actor, const FName AttributeName)
{
	UAbilitySystemComponent* ASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Actor);
	FProperty* Property = FindFProperty<FProperty>(URPGAttributeSet::StaticClass(), AttributeName);
	return ASC && Property ? ASC->GetNumericAttribute(FGameplayAttribute(Property)) : -1.f;
}

TMap<FString, FString> UDTStructRepairLibrary::InspectRuntimePlayer(APlayerController* Controller)
{
	TMap<FString, FString> Out;
	ARPGCharacter* Pawn = Controller ? Cast<ARPGCharacter>(Controller->GetPawn()) : nullptr;
	if (!Pawn) { Out.Add(TEXT("error"), TEXT("No unified player pawn")); return Out; }
	Out.Add(TEXT("pawn"), Pawn->GetClass()->GetPathName());
	Out.Add(TEXT("controller"), Controller->GetClass()->GetPathName());
	Out.Add(TEXT("tick_enabled"), Pawn->IsActorTickEnabled() ? TEXT("true") : TEXT("false"));
	Out.Add(TEXT("can_ever_tick"), Pawn->PrimaryActorTick.bCanEverTick ? TEXT("true") : TEXT("false"));
	USkeletalMeshComponent* Mesh = Pawn->GetMesh();
	UAnimInstance* Anim = Mesh ? Mesh->GetAnimInstance() : nullptr;
	Out.Add(TEXT("runtime_anim_instance"), Anim ? Anim->GetClass()->GetPathName() : TEXT("none"));
	if (Mesh)
	{
		Out.Add(TEXT("mesh"), GetPathNameSafe(Mesh->GetSkeletalMeshAsset()));
		Out.Add(TEXT("mesh_tick_enabled"), Mesh->IsComponentTickEnabled() ? TEXT("true") : TEXT("false"));
		Out.Add(TEXT("pause_anims"), Mesh->bPauseAnims ? TEXT("true") : TEXT("false"));
		Out.Add(TEXT("no_skeleton_update"), Mesh->bNoSkeletonUpdate ? TEXT("true") : TEXT("false"));
		Out.Add(TEXT("anim_tick_mode"), FString::FromInt(static_cast<int32>(Mesh->VisibilityBasedAnimTickOption)));
		Out.Add(TEXT("bone_count"), FString::FromInt(Mesh->GetNumBones()));
	}
	const UCharacterMovementComponent* Movement = Pawn->GetCharacterMovement();
	Out.Add(TEXT("speed"), FString::SanitizeFloat(Pawn->GetVelocity().Size2D()));
	Out.Add(TEXT("acceleration"), Movement->GetCurrentAcceleration().ToString());
	Out.Add(TEXT("falling"), Movement->IsFalling() ? TEXT("true") : TEXT("false"));
	Out.Add(TEXT("max_walk_speed"), FString::SanitizeFloat(Movement->MaxWalkSpeed));
	if (UAbilitySystemComponent* ASC = Pawn->GetAbilitySystemComponent())
	{
		Out.Add(TEXT("asc"), ASC->GetPathName());
		Out.Add(TEXT("asc_owner"), GetPathNameSafe(ASC->GetOwnerActor()));
		Out.Add(TEXT("asc_avatar"), GetPathNameSafe(ASC->GetAvatarActor()));
		Out.Add(TEXT("granted_gas_abilities"), FString::FromInt(ASC->GetActivatableAbilities().Num()));
		Out.Add(TEXT("active_effects"), FString::FromInt(ASC->GetActiveEffects(FGameplayEffectQuery()).Num()));
		FGameplayTagContainer Tags; ASC->GetOwnedGameplayTags(Tags);
		Out.Add(TEXT("gas_tags"), Tags.ToString());
		for (const FName Name : {FName("Mana"), FName("MaxMana"), FName("ManaRegeneration"), FName("Health"), FName("Stamina")})
			Out.Add(Name.ToString(), FString::SanitizeFloat(ReadRuntimeAttribute(Pawn, Name)));
	}
	if (Anim)
	{
		Out.Add(TEXT("current_montage"), GetPathNameSafe(Anim->GetCurrentActiveMontage()));
		if (UAnimMontage* Montage = Anim->GetCurrentActiveMontage())
			Out.Add(TEXT("montage_position"), FString::SanitizeFloat(Anim->Montage_GetPosition(Montage)));
		for (TFieldIterator<FProperty> It(Anim->GetClass()); It; ++It)
		{
			const FString Name = It->GetName();
			if (!Name.Contains(TEXT("Speed")) && !Name.Contains(TEXT("Velocity")) && !Name.Contains(TEXT("Acceleration")) &&
				!Name.Contains(TEXT("Direction")) && !Name.Contains(TEXT("Gait")) && !Name.Contains(TEXT("Falling")) &&
				!Name.Contains(TEXT("Movement")) && !Name.Contains(TEXT("Stance"))) continue;
			FString Value; It->ExportText_InContainer(0, Value, Anim, Anim, Anim, PPF_None);
			if (Value.Len() < 600) Out.Add(TEXT("anim.") + Name, Value);
		}
	}
	return Out;
}

#if WITH_EDITOR
#include "EdGraph/EdGraphPin.h"
#include "EdGraphSchema_K2.h"
#include "Engine/UserDefinedEnum.h"
#include "StructUtils/UserDefinedStruct.h"
#include "Kismet2/EnumEditorUtils.h"
#include "Kismet2/StructureEditorUtils.h"
#include "UserDefinedStructure/UserDefinedStructEditorData.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "K2Node_CallFunction.h"
#include "K2Node_CallParentFunction.h"
#include "K2Node_Event.h"
#include "K2Node_CustomEvent.h"
#include "K2Node_IfThenElse.h"
#include "K2Node_Select.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_FunctionResult.h"
#include "K2Node_VariableGet.h"
#include "K2Node_VariableSet.h"
#include "K2Node_Variable.h"
#include "K2Node_EditablePinBase.h"
#include "K2Node_DynamicCast.h"
#include "EdGraph/EdGraph.h"
#include "Engine/Blueprint.h"
#include "Misc/PackageName.h"
#include "UObject/SavePackage.h"
#include "Animation/AnimBlueprint.h"
#include "Animation/Skeleton.h"
#include "AnimGraphNode_Base.h"
#include "AnimGraphNode_Slot.h"
#include "AnimGraphNode_LinkedAnimLayer.h"
#include "AnimGraphNode_LinkedAnimGraph.h"
#include "AnimGraphNode_StateMachine.h"
#include "AnimGraphNode_LayeredBoneBlend.h"
#include "AnimGraphNode_SaveCachedPose.h"
#include "AnimGraphNode_UseCachedPose.h"
#include "AnimGraphNode_Root.h"
#include "ReferenceSkeleton.h"
#include "Settings/LevelEditorPlaySettings.h"
#include "RPGCharacter.h"
#include "InventoryComponent.h"
#include "Equipment_ItemObject.h"
#include "Food_ItemObject.h"
#include "Gear_ItemObject.h"
#include "RPGAttributeSet.h"
#include "RPG_AbilitySystemLibrary.h"
#include "RPGAttributeMenuWidgetController.h"
#include "RPG_ItemToolTip.h"
#include "RPG_ItemWidget.h"
#include "GameFramework/PlayerController.h"
#include "UnrealEngine.h"
#include "RPGRandomQuestSpawner.h"
#include "RPGDynamicQuestComponent.h"
#include "RPGPlayerState.h"
#include "RPGPlayerController.h"
#include "RPGPartyComponent.h"
#include "RPGPickupItem.h"
#include "RPGAICharacter.h"
#include "EngineUtils.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "DTAnimationRouter.h"
#include "RPGCombatTypes.h"
#include "RPGProjectile.h"
#include "WidgetBlueprint.h"
#include "Blueprint/WidgetTree.h"
#include "Components/ScrollBox.h"
#endif

TArray<FString> UDTStructRepairLibrary::InspectSkillWidgets(const FString& ObjectPath, const bool bEnableScrolling)
{
	TArray<FString> Result;
#if WITH_EDITOR
	UWidgetBlueprint* BP = LoadObject<UWidgetBlueprint>(nullptr, *ObjectPath);
	if (!BP || !BP->WidgetTree) return Result;
	TArray<UWidget*> Widgets; BP->WidgetTree->GetAllWidgets(Widgets);
	bool Changed = false;
	for (UWidget* Widget : Widgets)
	{
		FString Line = Widget->GetName() + TEXT(" class=") + Widget->GetClass()->GetName();
		for (TFieldIterator<FProperty> It(Widget->GetClass()); It; ++It)
		{
			const FString Name = It->GetName();
			if (!Name.Contains(TEXT("Tag")) && !Name.Contains(TEXT("Ability")) && !Name.Contains(TEXT("Input"))) continue;
			FString Value; It->ExportText_InContainer(0, Value, Widget, Widget, Widget, PPF_None);
			if (Value.Len() < 300) Line += TEXT(" ") + Name + TEXT("=") + Value;
		}
		if (UScrollBox* Scroll = Cast<UScrollBox>(Widget))
		{
			Line += FString::Printf(TEXT(" orientation=%d"), static_cast<int32>(Scroll->GetOrientation()));
			if (bEnableScrolling)
			{
				Scroll->SetConsumeMouseWheel(EConsumeMouseWheel::Always);
				Scroll->SetScrollBarVisibility(ESlateVisibility::Visible);
				Scroll->SetAnimateWheelScrolling(true);
				Changed = true;
			}
		}
		Result.Add(Line);
	}
	if (Changed) { FBlueprintEditorUtils::MarkBlueprintAsModified(BP); ForceCompileAndSave(ObjectPath); }
#endif
	return Result;
}

bool UDTStructRepairLibrary::SetDesignerWidgetGameplayTag(const FString& ObjectPath, const FString& WidgetName,
                                                          const FString& PropertyName, const FString& TagName)
{
#if WITH_EDITOR
	UWidgetBlueprint* BP = LoadObject<UWidgetBlueprint>(nullptr, *ObjectPath);
	if (!BP || !BP->WidgetTree) return false;
	const FGameplayTag NewTag = FGameplayTag::RequestGameplayTag(FName(*TagName), false);
	if (!NewTag.IsValid()) return false;
	TArray<UWidget*> Widgets;
	BP->WidgetTree->GetAllWidgets(Widgets);
	for (UWidget* Widget : Widgets)
	{
		if (!Widget || Widget->GetName() != WidgetName) continue;
		FStructProperty* Prop = nullptr;
		for (TFieldIterator<FStructProperty> It(Widget->GetClass()); It; ++It)
			if (It->Struct == TBaseStructure<FGameplayTag>::Get() &&
			    It->GetName().Equals(PropertyName, ESearchCase::IgnoreCase)) { Prop = *It; break; }
		if (!Prop) return false;
		FGameplayTag* Tag = Prop->ContainerPtrToValuePtr<FGameplayTag>(Widget);
		if (!Tag) return false;
		if (*Tag == NewTag) return true;
		Widget->Modify();
		*Tag = NewTag;
		// Widget-tree defaults do not affect compile status; a package dirty
		// flag is enough for the guarded save below.
		if (UPackage* Package = BP->GetOutermost()) Package->SetDirtyFlag(true);
		return SaveAssetPackage(ObjectPath);
	}
#endif
	return false;
}

bool UDTStructRepairLibrary::RepairHUDLevelUp(const FString& ObjectPath)
{
#if WITH_EDITOR
	UBlueprint* BP=LoadObject<UBlueprint>(nullptr,*ObjectPath);
	if (!BP) return false;
	for (UEdGraph* Graph:BP->UbergraphPages) for (UEdGraphNode* Node:Graph->Nodes)
	{
		UK2Node_CustomEvent* Event=Cast<UK2Node_CustomEvent>(Node);
		UEdGraphPin* IsLevelUp=Event ? Event->FindPin(TEXT("bLevelUp")) : nullptr;
		UEdGraphPin* Then=Event ? Event->FindPin(UEdGraphSchema_K2::PN_Then) : nullptr;
		if (!IsLevelUp || !Event->FindPin(TEXT("NewValue")) || !Then || Then->LinkedTo.Num()!=1) continue;
		if (Then->LinkedTo[0]->GetOwningNode()->GetFName()==TEXT("DT_GenuineLevelUpOnly")) return true;
		UEdGraphPin* PopupExecute=Then->LinkedTo[0];
		UEdGraphPin* ClassPin=PopupExecute->GetOwningNode()->FindPin(TEXT("Class"));
		if (!ClassPin || !GetPathNameSafe(ClassPin->DefaultObject).Contains(TEXT("WBP_LevelUpNotification"))) continue;
		UK2Node_IfThenElse* Branch=NewObject<UK2Node_IfThenElse>(Graph,TEXT("DT_GenuineLevelUpOnly"));
		Graph->AddNode(Branch,false,false); Branch->CreateNewGuid(); Branch->PostPlacedNewNode(); Branch->AllocateDefaultPins();
		Branch->NodePosX=Event->NodePosX+300; Branch->NodePosY=Event->NodePosY;
		Branch->NodeComment=TEXT("Only genuine level gains create a notification. Initial HUD binding and ordinary XP/stat refreshes must not show Level Up."); Branch->bCommentBubbleVisible=true;
		Then->BreakAllPinLinks();
		const UEdGraphSchema* Schema=Graph->GetSchema();
		bool bOK=Schema->TryCreateConnection(Then,Branch->GetExecPin());
		bOK &= Schema->TryCreateConnection(IsLevelUp,Branch->GetConditionPin());
		bOK &= Schema->TryCreateConnection(Branch->GetThenPin(),PopupExecute);
		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
		return bOK;
	}
	return false;
#else
	return false;
#endif
}

bool UDTStructRepairLibrary::ExposeControllerMenuFlow(const FString& ObjectPath)
{
#if WITH_EDITOR
	UBlueprint* BP=LoadObject<UBlueprint>(nullptr,*ObjectPath);
	if (!BP || !BP->ParentClass->IsChildOf(ARPGPlayerController::StaticClass()) || BP->UbergraphPages.IsEmpty()) return false;
	const FName Function=GET_FUNCTION_NAME_CHECKED(ARPGPlayerController,ApplyUIInputMode);
	for (UEdGraph* Graph:BP->UbergraphPages) for (UEdGraphNode* Node:Graph->Nodes)
		if (const UK2Node_Event* Event=Cast<UK2Node_Event>(Node))
			if (Event->EventReference.GetMemberName()==Function) return true;
	UEdGraph* Graph=BP->UbergraphPages[0];
	int32 Y=600;
	UK2Node_Event* Event=FKismetEditorUtilities::AddDefaultEventNode(BP,Graph,Function,ARPGPlayerController::StaticClass(),Y);
	if (!Event) return false;
	Event->NodeComment=TEXT("Menu input: extend this event to customize focus/cursor behavior. Keep the parent call for gameplay locks and cleanup. Widget classes are editable in Class Defaults > DT > UI and UI.");
	Event->bCommentBubbleVisible=true;
	UK2Node_CallParentFunction* Parent=NewObject<UK2Node_CallParentFunction>(Graph);
	Parent->FunctionReference.SetExternalMember(Function,ARPGPlayerController::StaticClass());
	Graph->AddNode(Parent,false,false); Parent->CreateNewGuid(); Parent->PostPlacedNewNode(); Parent->AllocateDefaultPins();
	Parent->NodePosX=Event->NodePosX+480; Parent->NodePosY=Event->NodePosY;
	const bool bConnected=Graph->GetSchema()->TryCreateConnection(Event->FindPin(UEdGraphSchema_K2::PN_Then),Parent->FindPin(UEdGraphSchema_K2::PN_Execute));
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
	return bConnected;
#else
	return false;
#endif
}

bool UDTStructRepairLibrary::RepairUnifiedWidgetContext(const FString& ObjectPath)
{
#if WITH_EDITOR
	UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *ObjectPath);
	if (!Blueprint) return false;
	if (Blueprint->GetName() == TEXT("WBP_PlayerStat"))
	{
		AddMemberVariableExact(ObjectPath, TEXT("Health"), TEXT("real"), TEXT(""));
		AddMemberVariableExact(ObjectPath, TEXT("Mana"), TEXT("real"), TEXT(""));
		AddMemberVariableExact(ObjectPath, TEXT("Level"), TEXT("int"), TEXT(""));
		SetUserPinType(ObjectPath, TEXT("K2Node_CustomEvent_0"), TEXT("PC"), TEXT("object"), TEXT("/Script/DuTraveller.RPGCharacter"));
		RebindVariableRefParents(ObjectPath);
	}
	for (FBPVariableDescription& Variable : Blueprint->NewVariables)
	{
		UClass* Target = nullptr;
		if (Variable.VarName == TEXT("PController") || Variable.VarName == TEXT("PlayerController")) Target = ARPGPlayerController::StaticClass();
		if (Variable.VarName == TEXT("PC") || Variable.VarName == TEXT("Player")) Target = ARPGCharacter::StaticClass();
		if (Variable.VarName == TEXT("Inventory") || Variable.VarName == TEXT("InventoryComp")) Target = UInventoryComponent::StaticClass();
		if (Blueprint->GetName() == TEXT("WBP_EnemyHealthWidget") && Variable.VarName == TEXT("BP_EnemyBase")) Target = ARPGAICharacter::StaticClass();
		if (Target) { Variable.VarType.PinCategory = UEdGraphSchema_K2::PC_Object; Variable.VarType.PinSubCategoryObject = Target; }
	}
	TArray<UEdGraph*> Graphs;
	Blueprint->GetAllGraphs(Graphs);
	if (Blueprint->GetName() == TEXT("WBP_ItemBpTooltip"))
	{
		// Recipe previews already receive Recipe on spawn. Their rarity must
		// use that definition, not an unrelated inventory-slot widget.
		for (UEdGraph* Graph : Graphs)
		{
			UEdGraphPin* Recipe = nullptr;
			for (UEdGraphNode* Node : Graph->Nodes)
				if (UK2Node_VariableGet* Get = Cast<UK2Node_VariableGet>(Node))
					if (Get->GetVarName() == TEXT("Recipe")) Recipe = Get->FindPin(TEXT("Recipe"));
			if (!Recipe) continue;
			for (UEdGraphNode* Node : Graph->Nodes)
				if (UK2Node_VariableGet* Get = Cast<UK2Node_VariableGet>(Node))
					if (Get->GetVarName() == TEXT("Rarity"))
						if (UEdGraphPin* Target = Get->FindPin(UEdGraphSchema_K2::PN_Self))
						{
							Target->BreakAllPinLinks(); Recipe->MakeLinkTo(Target);
						}
		}
	}
	if (Blueprint->GetName() == TEXT("WBP_QuestLog"))
	{
		// The owning controller manages input for reusable panels, including teardown.
		TArray<UEdGraphNode*> InputNodes;
		for (UEdGraph* Graph : Graphs) for (UEdGraphNode* Node : Graph->Nodes)
		{
			if (UK2Node_CallFunction* Call = Cast<UK2Node_CallFunction>(Node))
				if (Call->FunctionReference.GetMemberName().ToString().StartsWith(TEXT("SetInputMode_"))) InputNodes.Add(Node);
			if (UK2Node_VariableSet* Variable = Cast<UK2Node_VariableSet>(Node))
				if (Variable->GetVarName() == TEXT("bShowMouseCursor")) InputNodes.Add(Node);
		}
		for (UEdGraphNode* Node : InputNodes)
		{
			UEdGraphPin* In = Node->FindPin(UEdGraphSchema_K2::PN_Execute);
			UEdGraphPin* Out = Node->FindPin(UEdGraphSchema_K2::PN_Then);
			if (In && Out)
			{
				const TArray<UEdGraphPin*> Before = In->LinkedTo, After = Out->LinkedTo;
				In->BreakAllPinLinks(); Out->BreakAllPinLinks();
				for (UEdGraphPin* A : Before) for (UEdGraphPin* B : After) A->MakeLinkTo(B);
			}
			FBlueprintEditorUtils::RemoveNode(Blueprint, Node, true);
		}
	}
	if (Blueprint->GetName() == TEXT("WBP_DropWidget"))
	{
		// Inventory mutations already broadcast OnInventoryUpdated. The old
		// controller-child refresh path is both redundant and incompatible.
		TArray<UEdGraphNode*> Obsolete;
		for (UEdGraph* Graph : Graphs) for (UEdGraphNode* Node : Graph->Nodes)
		{
			if (UK2Node_CallFunction* Call = Cast<UK2Node_CallFunction>(Node))
				if (Call->FunctionReference.GetMemberName() == TEXT("Initialise Inventory")) Obsolete.Add(Node);
			if (UK2Node_Variable* Variable = Cast<UK2Node_Variable>(Node))
				if (Variable->GetVarName() == TEXT("InventoryWidget")) Obsolete.Add(Node);
		}
		for (UEdGraphNode* Node : Obsolete) FBlueprintEditorUtils::RemoveNode(Blueprint, Node, true);
	}
	for (UEdGraph* Graph : Graphs) for (UEdGraphNode* Node : Graph->Nodes)
	{
		if (Blueprint->GetName() == TEXT("WBP_PlayerStat"))
			if (UK2Node_CallFunction* Call = Cast<UK2Node_CallFunction>(Node))
				if (Call->FunctionReference.GetMemberName() == TEXT("Initialize")) Call->FunctionReference.SetSelfMember(TEXT("Initialize"));
		if (UK2Node_DynamicCast* CastNode = Cast<UK2Node_DynamicCast>(Node))
		{
			UClass* Target = nullptr;
			for (UEdGraphPin* Pin : CastNode->Pins)
			{
				const FString Name = Pin->PinName.ToString().Replace(TEXT(" "), TEXT(""));
				if (Name.Contains(TEXT("AsBPRPGPlayerController"))) Target = ARPGPlayerController::StaticClass();
				else if (Blueprint->GetName() == TEXT("WBP_EnemyHealthWidget") && Name.Contains(TEXT("AsBPRPGAICharacter"))) Target = ARPGAICharacter::StaticClass();
				else if (Name.Contains(TEXT("AsBPRPGCharacter")) || Name.Contains(TEXT("AsBPPlayerCharacter"))) Target = ARPGCharacter::StaticClass();
			}
			if (Target)
			{
				TArray<UEdGraphPin*> ResultLinks;
				if (UEdGraphPin* Result = CastNode->GetCastResultPin())
				{
					ResultLinks = Result->LinkedTo;
					Result->BreakAllPinLinks();
				}
				CastNode->TargetType = Target;
				CastNode->ReconstructNode();
				if (UEdGraphPin* Result = CastNode->GetCastResultPin())
					for (UEdGraphPin* Linked : ResultLinks) if (Linked) Result->MakeLinkTo(Linked);
			}
		}
	}
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
	RefreshBlueprintNodes(ObjectPath);
	return true;
#else
	return false;
#endif
}

bool UDTStructRepairLibrary::PrepareFoundationTestInventory(ARPGCharacter* Character)
{
#if WITH_EDITOR
	if (!IsValid(Character) || !Character->HasAuthority() || Character->GetWorld()->WorldType != EWorldType::PIE ||
		!Character->InventoryComp->Items.IsEmpty()) return false;
	UInventoryComponent* Inventory = Character->InventoryComp;
	Inventory->TryAddItemFromClass(UGear_ItemObject::StaticClass(), 1);
	Inventory->TryAddItemFromClass(UFood_ItemObject::StaticClass(), 1);
	UGear_ItemObject* Gear = Cast<UGear_ItemObject>(Inventory->GetItem(0));
	UFood_ItemObject* Food = Cast<UFood_ItemObject>(Inventory->GetItem(1));
	if (!Gear || !Food) return false;
	Gear->Slot = EEquipableSlot::EIS_Helmet;
	Gear->AttributeBonuses.Add(FName("Strength"), 5.f);
	Gear->AttributeBonuses.Add(FName("MaxHealth"), 50.f);
	Food->UseEffect = URPGConsumableResourceEffect::StaticClass();
	Food->SetByCallerMagnitudes.Add(FName("Health"), 25.f);
	return true;
#else
	return false;
#endif
}

bool UDTStructRepairLibrary::RunFoundationClientTestAction(ARPGCharacter* Character, const int32 Stage)
{
#if WITH_EDITOR
	if (!IsValid(Character) || Character->GetWorld()->WorldType != EWorldType::PIE) return false;
	if (Stage == 15 && Character->HasAuthority())
	{
		ARPGPlayerState* PS = Character->GetPlayerState<ARPGPlayerState>();
		URPGCombatAbilityDefinition* Fire = LoadObject<URPGCombatAbilityDefinition>(nullptr,
			TEXT("/Game/Data/Abilities/Player/DA_Player_FireBolt.DA_Player_FireBolt"));
		if (!PS || !PS->Skills || !PS->AbilitySystem || !Fire || !Fire->IsConfigured()) return false;
		PS->AbilitySystem->SetNumericAttributeBase(URPGAttributeSet::GetMaxManaAttribute(), 100.f);
		PS->AbilitySystem->SetNumericAttributeBase(URPGAttributeSet::GetManaAttribute(), 100.f);
		PS->Skills->LearnedAbilities.RemoveAll([Fire](const FRPGLearnedAbility& Entry) { return Entry.Definition == Fire; });
		FRPGLearnedAbility Learned; Learned.Definition = Fire; Learned.Rank = 1; PS->Skills->LearnedAbilities.Add(Learned);
		PS->Skills->Hotbar.RemoveAll([Fire](const FRPGHotbarSlot& Entry) { return Entry.Slot == 0 || Entry.Definition == Fire; });
		FRPGHotbarSlot Slot; Slot.Slot = 0; Slot.Definition = Fire; PS->Skills->Hotbar.Add(Slot);
		PS->Skills->RefreshGrantedAbilities();
		ARPGCharacter* PredictionTarget = SpawnRuntimeCombatTarget(Character, 200.f);
		if (!PredictionTarget) return false;
		PredictionTarget->SetReplicates(true);
		PredictionTarget->ForceNetUpdate();
		PS->ForceNetUpdate();
		return Cast<URPGAbilitySystemComponent>(PS->AbilitySystem.Get())->HasGrantedAbility(Fire->AbilityId);
	}
	if (Stage == 17 && Character->HasAuthority())
	{
		// Create an intentional one-frame authority/client disagreement for the
		// prediction rollback test. The owning client still sees an enemy when it
		// dispatches, while the server validates the same actor as friendly.
		ARPGCharacter* Target = Character->ActionComp
			? Cast<ARPGCharacter>(Character->ActionComp->CommittedAction.Target.Get()) : nullptr;
		if (!Target || Target->bIsDead) return false;
		if (UAbilitySystemComponent* ASC = Character->GetAbilitySystemComponent())
			ASC->SetNumericAttributeBase(URPGAttributeSet::GetManaRegenerationAttribute(), 0.f);
		Target->FactionId = Character->FactionId;
		return true;
	}
	if (Stage == 10 && Character->HasAuthority())
	{
		auto* Group = LoadObject<URPGQuestGroupDefinition>(nullptr, TEXT("/Game/Data/Quests/DA_QuestGroup_LocationDiscovery.DA_QuestGroup_LocationDiscovery"));
		UClass* TargetClass = LoadClass<AActor>(nullptr, TEXT("/Game/Blueprints/Gameplay/Spawning/BP_MasterQuestTarget.BP_MasterQuestTarget_C"));
		if (!Group || !TargetClass) return false;
		auto* Spawner = Character->GetWorld()->SpawnActor<ARPGRandomQuestSpawner>(Character->GetActorLocation(), FRotator::ZeroRotator);
		if (!Spawner) return false;
		Spawner->AreaId = FName("NetworkQuestTest"); Spawner->LocationId = FName("Location.Ruins");
		Spawner->bRequireGround = false; Spawner->SpawnInterval = 0; Spawner->MaximumOffers = 1;
		FRPGWeightedSpawn Entry; Entry.ActorClass = TargetClass; Spawner->SpawnPool.Add(Entry);
		Spawner->QuestGroups.Add(Group);
		return Spawner->GenerateOffer();
	}
	if (Stage == 11 && Character->HasAuthority())
	{
		UClass* AIClass = LoadClass<ARPGAICharacter>(nullptr, TEXT("/Game/Blueprints/AI/BP_RPGAICharacter.BP_RPGAICharacter_C"));
		if (!AIClass) return false;
		FActorSpawnParameters Params; Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		ARPGAICharacter* AI = Character->GetWorld()->SpawnActor<ARPGAICharacter>(AIClass,
			Character->GetActorLocation() + Character->GetActorForwardVector() * 150.f, Character->GetActorRotation(), Params);
		if (!AI) return false;
		AI->SpawnDefaultController();
		return AI->GetController() != nullptr;
	}
	if (Character->HasAuthority()) return false;
	FScopedConditionalWorldSwitcher WorldScope(Character->GetWorld());
	// Python's FEditorScriptExecutionGuard forces AActor::GetFunctionCallspace
	// to Local. A gameplay request must instead use normal network routing.
	TGuardValue<bool> ScriptGuard(GAllowActorScriptExecutionInEditor, false);
	if (Stage == 0)
	{
		URPGAttributeMenuWidgetController* Controller = URPG_AbilitySystemLibrary::GetAttributeWidgetController(Character);
		UEquipment_ItemObject* Gear = Cast<UEquipment_ItemObject>(Character->InventoryComp->GetItem(0));
		if (!Controller || !Gear) return false;
		Controller->UpgradeAttribute(FGameplayTag::RequestGameplayTag(FName("Attributes.Primary.Strength")));
		Character->EquipItem(Gear, EEquipableSlot::EIS_Helmet);
		return true;
	}
	if (Stage == 1)
	{
		UItemObject* Food = Character->InventoryComp->GetItem(1);
		if (!Food || !Food->IsA<UFood_ItemObject>()) return false;
		Character->InventoryComp->UseItem(Food);
		return true;
	}
	if (Stage == 2)
	{
		Character->UnequipItem(EEquipableSlot::EIS_Helmet);
		return true;
	}
	if (Stage == 16)
	{
		ARPGPlayerState* PS = Character->GetPlayerState<ARPGPlayerState>();
		if (!PS || !PS->Skills) return false;
		ARPGCharacter* Target = nullptr;
		for (TActorIterator<ARPGCharacter> It(Character->GetWorld()); It; ++It)
			if (*It != Character && !It->bIsDead && It->FactionId == FName("Enemy")) { Target = *It; break; }
		if (!Target) return false;
		PS->Skills->RequestActivateHotbar(0, Target);
		return true;
	}
	if (Stage == 13)
	{
		ARPGPlayerState* PS = Character->GetPlayerState<ARPGPlayerState>();
		if (!PS || !PS->Party) return false;
		PS->Party->AcceptInvite();
		return true;
	}
	if (Stage == 3)
	{
		ARPGPlayerState* PS = Character->GetPlayerState<ARPGPlayerState>();
		if (!PS) return false;
		for (TActorIterator<ARPGRandomQuestSpawner> It(Character->GetWorld()); It; ++It)
			if (!It->Offers.IsEmpty() && It->ContainsPosition(Character->GetActorLocation()))
			{ PS->DynamicQuests->RequestAcceptOffer(*It, It->Offers[0].OfferId); return true; }
		return false;
	}
	if (Stage == 4)
	{
		ARPGPlayerState* PS = Character->GetPlayerState<ARPGPlayerState>();
		if (!PS || PS->DynamicQuests->Records.IsEmpty()) return false;
		PS->DynamicQuests->RequestClaimReward(PS->DynamicQuests->Records[0].InstanceId);
		return true;
	}
	if (Stage == 5)
	{
		APlayerController* PC = Cast<APlayerController>(Character->GetController());
		if (!PC && Character->GetWorld())
		{
			PC = Character->GetWorld()->GetFirstPlayerController();
		}
		UItemObject* Item = Character->InventoryComp->GetItem(0);
		UE_LOG(LogTemp, Verbose,
		       TEXT("ITEM_TOOLTIP_PIE precheck authority=%d controller=%d local=%d item=%d world=%s"),
		       Character->HasAuthority(), PC != nullptr, PC && PC->IsLocalController(), Item != nullptr,
		       Character->GetWorld() ? *Character->GetWorld()->GetName() : TEXT("<none>"));
		if (!PC || !PC->IsLocalController() || !Item) return false;
		URPG_ItemWidget* Widget = CreateWidget<URPG_ItemWidget>(PC, URPG_ItemWidget::StaticClass());
		if (!Widget)
		{
			UE_LOG(LogTemp, Verbose, TEXT("ITEM_TOOLTIP_PIE widget creation failed"));
			return false;
		}
		Widget->ItemTooltip = URPG_ItemToolTip::StaticClass();
		Widget->SetItem(Item);
		URPG_ItemToolTip* Tooltip = Cast<URPG_ItemToolTip>(Widget->GetToolTip());
		const bool bValid = Tooltip && Tooltip->Item == Item &&
			Tooltip->Quantity == Item->GetQuantity() && !Tooltip->ItemDisplayName.IsEmpty();
		UE_LOG(LogTemp, Verbose,
		       TEXT("ITEM_TOOLTIP_PIE widget=%d tooltip=%d same_item=%d quantity=%d expected=%d name=%s valid=%d"),
		       Widget != nullptr, Tooltip != nullptr, Tooltip && Tooltip->Item == Item,
		       Tooltip ? Tooltip->Quantity : -1, Item->GetQuantity(),
		       Tooltip ? *Tooltip->ItemDisplayName.ToString() : TEXT("<none>"), bValid);
		return bValid;
	}
	if (Stage == 6)
	{
		UItemObject* Item = Character->InventoryComp->GetItem(0);
		if (!Item) return false;
		Character->DropItem(Item, 1);
		return true;
	}
	if (Stage == 7)
	{
		for (TActorIterator<ARPGPickupItem> It(Character->GetWorld()); It; ++It)
		{
			Character->CollectPickup(*It);
			return true;
		}
		return false;
	}
	if (Stage == 14)
	{
		// Client dodge request exercising the real ServerDodge RPC path.
		ARPGPlayerCharacter* Hero = Cast<ARPGPlayerCharacter>(Character);
		return Hero && Hero->RequestDodge(FVector::ForwardVector);
	}
#endif
	return false;
}

void UDTStructRepairLibrary::SetMultiplayerTestSettings(const bool bEnabled)
{
#if WITH_EDITOR
	static bool bActive = false;
	static EPlayNetMode PreviousMode = PIE_Standalone;
	static int32 PreviousClients = 1;
	static bool bPreviousSingleProcess = true;
	ULevelEditorPlaySettings* Settings = GetMutableDefault<ULevelEditorPlaySettings>();
	if (bEnabled && !bActive)
	{
		Settings->GetPlayNetMode(PreviousMode);
		Settings->GetPlayNumberOfClients(PreviousClients);
		Settings->GetRunUnderOneProcess(bPreviousSingleProcess);
		Settings->SetPlayNetMode(PIE_ListenServer);
		Settings->SetPlayNumberOfClients(2);
		Settings->SetRunUnderOneProcess(true);
		bActive = true;
	}
	else if (!bEnabled && bActive)
	{
		Settings->SetPlayNetMode(PreviousMode);
		Settings->SetPlayNumberOfClients(PreviousClients);
		Settings->SetRunUnderOneProcess(bPreviousSingleProcess);
		bActive = false;
	}
#endif
}

bool UDTStructRepairLibrary::HasGrantedGameplayAbility(ARPGCharacter* Character, const FName AbilityId)
{
	const URPGAbilitySystemComponent* ASC = Character
		? Cast<URPGAbilitySystemComponent>(Character->GetAbilitySystemComponent()) : nullptr;
	return ASC && ASC->HasGrantedAbility(AbilityId);
}

bool UDTStructRepairLibrary::IsGameplayAbilityActive(ARPGCharacter* Character, const FName AbilityId)
{
	const URPGAbilitySystemComponent* ASC = Character
		? Cast<URPGAbilitySystemComponent>(Character->GetAbilitySystemComponent()) : nullptr;
	return ASC && ASC->IsGrantedAbilityActive(AbilityId);
}

#if WITH_EDITOR
namespace DTStructRepair
{
static UPackage* GetOrCreatePackage(const FString& ObjectPath, FString& OutAssetName)
{
	const FString PackagePath = FPackageName::ObjectPathToPackageName(ObjectPath);
	OutAssetName = FPackageName::GetShortName(PackagePath);
	return CreatePackage(*PackagePath);
}

static UObject* LoadSubObject(const FString& Path)
{
	if (Path.IsEmpty())
	{
		return nullptr;
	}
	return StaticLoadObject(UObject::StaticClass(), nullptr, *Path);
}

static bool ParsePinType(const TArray<FString>& Parts, int32 Offset, FEdGraphPinType& OutPin,
                         EPinContainerType& OutContainer, FEdGraphTerminalType& OutValueType)
{
	if (Parts.Num() < Offset + 4)
	{
		return false;
	}

	const FString& Category = Parts[Offset + 1];
	const FString& SubPath = Parts[Offset + 2];
	const FString& Container = Parts[Offset + 3];

	if (Category == TEXT("int"))
	{
		OutPin.PinCategory = UEdGraphSchema_K2::PC_Int;
	}
	else if (Category == TEXT("real"))
	{
		OutPin.PinCategory = UEdGraphSchema_K2::PC_Real;
		OutPin.PinSubCategory = UEdGraphSchema_K2::PC_Float;
	}
	else if (Category == TEXT("bool"))
	{
		OutPin.PinCategory = UEdGraphSchema_K2::PC_Boolean;
	}
	else if (Category == TEXT("text"))
	{
		OutPin.PinCategory = UEdGraphSchema_K2::PC_Text;
	}
	else if (Category == TEXT("name"))
	{
		OutPin.PinCategory = UEdGraphSchema_K2::PC_Name;
	}
	else if (Category == TEXT("string"))
	{
		OutPin.PinCategory = UEdGraphSchema_K2::PC_String;
	}
	else if (Category == TEXT("object"))
	{
		OutPin.PinCategory = UEdGraphSchema_K2::PC_Object;
		OutPin.PinSubCategoryObject = LoadSubObject(SubPath);
	}
	else if (Category == TEXT("class"))
	{
		OutPin.PinCategory = UEdGraphSchema_K2::PC_Class;
		OutPin.PinSubCategoryObject = LoadSubObject(SubPath);
	}
	else if (Category == TEXT("struct"))
	{
		OutPin.PinCategory = UEdGraphSchema_K2::PC_Struct;
		OutPin.PinSubCategoryObject = LoadSubObject(SubPath);
	}
	else if (Category == TEXT("enum"))
	{
		OutPin.PinCategory = UEdGraphSchema_K2::PC_Enum;
		OutPin.PinSubCategoryObject = LoadSubObject(SubPath);
	}
	else if (Category == TEXT("byte"))
	{
		OutPin.PinCategory = UEdGraphSchema_K2::PC_Byte;
		OutPin.PinSubCategoryObject = LoadSubObject(SubPath);
	}
	else
	{
		return false;
	}

	if (Container == TEXT("Array"))
	{
		OutContainer = EPinContainerType::Array;
	}
	else if (Container == TEXT("Set"))
	{
		OutContainer = EPinContainerType::Set;
	}
	else if (Container == TEXT("Map"))
	{
		OutContainer = EPinContainerType::Map;
		if (Parts.Num() >= Offset + 6)
		{
			const FString& ValueCat = Parts[Offset + 4];
			const FString& ValueObj = Parts[Offset + 5];
			if (ValueCat == TEXT("int"))
			{
				OutValueType.TerminalCategory = UEdGraphSchema_K2::PC_Int;
			}
			else if (ValueCat == TEXT("name"))
			{
				OutValueType.TerminalCategory = UEdGraphSchema_K2::PC_Name;
			}
			else if (ValueCat == TEXT("string"))
			{
				OutValueType.TerminalCategory = UEdGraphSchema_K2::PC_String;
			}
			else if (ValueCat == TEXT("text"))
			{
				OutValueType.TerminalCategory = UEdGraphSchema_K2::PC_Text;
			}
			else if (ValueCat == TEXT("bool"))
			{
				OutValueType.TerminalCategory = UEdGraphSchema_K2::PC_Boolean;
			}
			else if (ValueCat == TEXT("real"))
			{
				OutValueType.TerminalCategory = UEdGraphSchema_K2::PC_Real;
				OutValueType.TerminalSubCategory = UEdGraphSchema_K2::PC_Float;
			}
			else if (ValueCat == TEXT("object"))
			{
				OutValueType.TerminalCategory = UEdGraphSchema_K2::PC_Object;
				OutValueType.TerminalSubCategoryObject = LoadSubObject(ValueObj);
			}
			else if (ValueCat == TEXT("struct"))
			{
				OutValueType.TerminalCategory = UEdGraphSchema_K2::PC_Struct;
				OutValueType.TerminalSubCategoryObject = LoadSubObject(ValueObj);
			}
		}
	}
	return true;
}
} // namespace DTStructRepair
#endif // WITH_EDITOR

bool UDTStructRepairLibrary::CreateUserDefinedStructAsset(const FString& ObjectPath,
                                                          const TArray<FString>& FieldSpecs)
{
#if WITH_EDITOR
	using namespace DTStructRepair;

	FString AssetName;
	UPackage* Package = GetOrCreatePackage(ObjectPath, AssetName);
	if (!Package)
	{
		return false;
	}

	UUserDefinedStruct* Struct = FindObject<UUserDefinedStruct>(Package, *AssetName);
	FGuid DefaultVarGuid;
	bool bHasDefaultVar = false;
	if (!Struct)
	{
		Struct = FStructureEditorUtils::CreateUserDefinedStruct(
			Package, *AssetName, RF_Public | RF_Standalone | RF_Transactional);
		if (Struct && FStructureEditorUtils::GetVarDesc(Struct).Num() > 0)
		{
			// CreateUserDefinedStruct seeds a placeholder bool member; remove it
			// once the real fields exist.
			DefaultVarGuid = FStructureEditorUtils::GetVarDesc(Struct)[0].VarGuid;
			bHasDefaultVar = true;
		}
	}
	if (!Struct)
	{
		return false;
	}

	for (const FString& Spec : FieldSpecs)
	{
		TArray<FString> Parts;
		Spec.ParseIntoArray(Parts, TEXT("|"), true);
		if (Parts.Num() < 4)
		{
			continue;
		}

		FEdGraphPinType PinType;
		EPinContainerType Container = EPinContainerType::None;
		FEdGraphTerminalType ValueType;
		if (!ParsePinType(Parts, 0, PinType, Container, ValueType))
		{
			continue;
		}
		PinType.ContainerType = Container;
		if (Container == EPinContainerType::Map)
		{
			PinType.PinValueType = ValueType;
		}

		if (!FStructureEditorUtils::AddVariable(Struct, PinType))
		{
			return false;
		}
		const int32 Index = FStructureEditorUtils::GetVarDesc(Struct).Num() - 1;
		if (Index >= 0)
		{
			FStructureEditorUtils::RenameVariable(Struct,
				FStructureEditorUtils::GetVarDesc(Struct)[Index].VarGuid,
				Parts[0]);
		}

		// Restore the exact historical VarName/VarGuid when supplied so
		// serialized member references (e.g. "QuestName_5_29411FF...") resolve.
		if (Parts.Num() >= 5 && !Parts[4].IsEmpty() && Index >= 0)
		{
			FStructVariableDescription& Desc =
				FStructureEditorUtils::GetVarDesc(Struct)[Index];
			Desc.VarName = *Parts[4];
			FString GuidHex;
			if (Parts[4].Split(TEXT("_"), nullptr, &GuidHex,
			                   ESearchCase::CaseSensitive, ESearchDir::FromEnd))
			{
				FGuid::Parse(GuidHex, Desc.VarGuid);
			}
		}
	}

	if (bHasDefaultVar)
	{
		FStructureEditorUtils::RemoveVariable(Struct, DefaultVarGuid);
	}

	FStructureEditorUtils::OnStructureChanged(Struct);
	FStructureEditorUtils::CompileStructure(Struct);
	FStructureEditorUtils::RecreateDefaultInstanceInEditorData(Struct);
	Struct->Status = UDSS_UpToDate;
	Package->MarkPackageDirty();
	return true;
#else
	return false;
#endif
}

bool UDTStructRepairLibrary::CreateUserDefinedEnumAsset(const FString& ObjectPath,
                                                        const TArray<FString>& EntryNames)
{
#if WITH_EDITOR
	using namespace DTStructRepair;

	FString AssetName;
	UPackage* Package = GetOrCreatePackage(ObjectPath, AssetName);
	if (!Package)
	{
		return false;
	}

	UUserDefinedEnum* Enum = FindObject<UUserDefinedEnum>(Package, *AssetName);
	if (!Enum)
	{
		Enum = Cast<UUserDefinedEnum>(
			FEnumEditorUtils::CreateUserDefinedEnum(Package, *AssetName,
			                                      RF_Public | RF_Standalone | RF_Transactional));
	}
	if (!Enum)
	{
		return false;
	}

	const int32 Existing = Enum->NumEnums() - 1; // skip _MAX
	for (int32 Index = Existing; Index < EntryNames.Num(); ++Index)
	{
		FEnumEditorUtils::AddNewEnumeratorForUserDefinedEnum(Enum);
	}
	for (int32 Index = 0; Index < EntryNames.Num(); ++Index)
	{
		FEnumEditorUtils::SetEnumeratorDisplayName(Enum, Index,
		                                           FText::FromString(EntryNames[Index]));
	}
	FEnumEditorUtils::PostEditUndo(Enum);
	Package->MarkPackageDirty();
	return true;
#else
	return false;
#endif
}

bool UDTStructRepairLibrary::SetUserDefinedEnumEntryNames(const FString& ObjectPath,
                                                          const TArray<FString>& EntryNames)
{
#if WITH_EDITOR
	UUserDefinedEnum* Enum = LoadObject<UUserDefinedEnum>(nullptr, *ObjectPath);
	if (!Enum)
	{
		return false;
	}

	TArray<TPair<FName, int64>> Names;
	for (int32 Index = 0; Index < EntryNames.Num(); ++Index)
	{
		// UserDefinedEnum entries are stored namespaced (e.g.
		// "EQuestTypes::Renown"); SetEnums keeps them verbatim.
		Names.Add(TPair<FName, int64>(
			*Enum->GenerateFullEnumName(*EntryNames[Index]), Index));
	}

	const UEnum::ECppForm Form = UEnum::ECppForm::Namespaced;
	const UEnum::EUnderlyingType Underlying = UEnum::EUnderlyingType::uint8;
	if (!Enum->SetEnums(Names, Form, Underlying, EEnumFlags::None,
	                    UEnum::EAddMaxKeyIfMissing::Yes))
	{
		return false;
	}

	FEnumEditorUtils::EnsureAllDisplayNamesExist(Enum);
	for (int32 Index = 0; Index < EntryNames.Num(); ++Index)
	{
		FEnumEditorUtils::SetEnumeratorDisplayName(Enum, Index,
		                                           FText::FromString(EntryNames[Index]));
	}
	FEnumEditorUtils::PostEditUndo(Enum);
	Enum->GetOutermost()->MarkPackageDirty();
	return true;
#else
	return false;
#endif
}

bool UDTStructRepairLibrary::RegenerateBlueprintClass(const FString& ObjectPath)
{
#if WITH_EDITOR
	UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *ObjectPath);
	if (!Blueprint)
	{
		return false;
	}

	// Only rebuild when a NewVariables entry lacks a generated FProperty; the
	// compile pass reports this as "property associated with X could not be
	// found" because the serialized class never materialized the members.
	bool bNeedsRegen = false;
	for (const FBPVariableDescription& Desc : Blueprint->NewVariables)
	{
		if (!Blueprint->GeneratedClass->FindPropertyByName(Desc.VarName))
		{
			bNeedsRegen = true;
			break;
		}
	}
	if (!bNeedsRegen)
	{
		return false;
	}

	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
	FKismetEditorUtilities::CompileBlueprint(Blueprint,
		EBlueprintCompileOptions::SkipSave);
	Blueprint->GetOutermost()->MarkPackageDirty();

	FString Names;
	for (TFieldIterator<FProperty> It(Blueprint->GeneratedClass); It; ++It)
	{
		Names += It->GetName() + TEXT(", ");
	}
	UE_LOG(LogTemp, Warning, TEXT("DTREGEN %s status=%d genvars=%d props=[%s]"),
	       *Blueprint->GetName(), (int32)Blueprint->Status,
	       Blueprint->NewVariables.Num(), *Names);
	return true;
#else
	return false;
#endif
}

bool UDTStructRepairLibrary::RefreshBlueprintNodes(const FString& ObjectPath)
{
#if WITH_EDITOR
	UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *ObjectPath);
	if (!Blueprint)
	{
		return false;
	}
	int32 Count = 0;
	TArray<UEdGraph*> Graphs;
	Blueprint->GetAllGraphs(Graphs);
	for (UEdGraph* Graph : Graphs)
	{
		TArray<UEdGraphNode*> Nodes(Graph->Nodes);
		for (UEdGraphNode* Node : Nodes)
		{
			UK2Node* K2Node = Cast<UK2Node>(Node);
			if (!K2Node || K2Node->NodeCausesStructuralBlueprintChange())
			{
				continue;
			}

			// Snapshot outgoing links keyed by pin name; reconstruction drops
			// links on pins whose names changed (e.g. enum option pins renamed
			// from NewEnumeratorN to the friendly entry name).
			TMap<FName, TArray<UEdGraphPin*>> OldLinks;
			for (UEdGraphPin* Pin : Node->Pins)
			{
				if (Pin->LinkedTo.Num() > 0)
				{
					OldLinks.Add(Pin->PinName, Pin->LinkedTo);
				}
			}

			K2Node->ReconstructNode();

			// Restore links for pins that lost them. Exact-name restores are
			// normally handled by the schema; this covers renamed option pins.
			for (UEdGraphPin* Pin : Node->Pins)
			{
				if (Pin->LinkedTo.Num() > 0)
				{
					continue;
				}
				TArray<UEdGraphPin*>* Found = OldLinks.Find(Pin->PinName);
				if (!Found)
				{
					// Enum Select option pins: map the new friendly-name pin
					// back to the old NewEnumeratorN pin for the same index.
					if (UK2Node_Select* Select = Cast<UK2Node_Select>(Node))
					{
						if (const UEnum* Enum = Select->GetEnum())
						{
							for (int32 i = 0; i < Enum->NumEnums() - 1; ++i)
							{
								const FString Friendly =
									Enum->GetDisplayNameTextByIndex(i).ToString();
								const FString EntryName =
									Enum->GetNameStringByIndex(i);
								if (Pin->PinName.ToString() == Friendly ||
								    Pin->PinName.ToString() == EntryName)
								{
									Found = OldLinks.Find(*FString::Printf(
										TEXT("NewEnumerator%d"), i));
									break;
								}
							}
						}
					}
				}
				if (Found)
				{
					for (UEdGraphPin* Target : *Found)
					{
						if (Target && !Target->IsPendingKill())
						{
							Pin->MakeLinkTo(Target);
						}
					}
				}
			}

			// Drop orphaned pins left over from an older signature/enum. Their
			// links were copied to the reconstructed pins above; leaving them
			// keeps the "in use pin no longer exists" compiler errors.
			TArray<UEdGraphPin*> Orphans;
			for (UEdGraphPin* Pin : Node->Pins)
			{
				if (Pin->bOrphanedPin)
				{
					Orphans.Add(Pin);
				}
			}
			for (UEdGraphPin* Pin : Orphans)
			{
				Pin->BreakAllPinLinks();
				Node->RemovePin(Pin);
			}
			++Count;
		}
	}
	UE_LOG(LogTemp, Warning, TEXT("DTREFRESH %s nodes=%d"),
	       *Blueprint->GetName(), Count);
	Blueprint->GetOutermost()->MarkPackageDirty();
	return true;
#else
	return false;
#endif
}

bool UDTStructRepairLibrary::DumpBlueprintGraph(const FString& ObjectPath)
{
#if WITH_EDITOR
	UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *ObjectPath);
	if (!Blueprint)
	{
		return false;
	}
	TArray<UEdGraph*> Graphs;
	Blueprint->GetAllGraphs(Graphs);
	for (UEdGraph* Graph : Graphs)
	{
		for (UEdGraphNode* Node : Graph->Nodes)
		{
			if (UK2Node_CallFunction* Call = Cast<UK2Node_CallFunction>(Node))
			{
				FString Desc = FString::Printf(
					TEXT("DTGRAPH %s node=%s func=%s funcparent=%s pins="),
					*Blueprint->GetName(), *Node->GetName(),
					*Call->FunctionReference.GetMemberName().ToString(),
					Call->FunctionReference.GetMemberParentClass()
						? *Call->FunctionReference.GetMemberParentClass()->GetName()
						: TEXT("NULL"));
				for (UEdGraphPin* Pin : Node->Pins)
				{
					Desc += FString::Printf(TEXT("[%s:%s:%s links="),
						*Pin->PinName.ToString(),
						*Pin->PinType.PinCategory.ToString(),
						Pin->PinType.PinSubCategoryObject.IsValid()
							? *Pin->PinType.PinSubCategoryObject->GetName()
							: TEXT("-"));
					for (UEdGraphPin* Link : Pin->LinkedTo)
					{
						Desc += Link->GetOuter()->GetName() + TEXT(".") +
						        Link->PinName.ToString() + TEXT(";");
					}
					Desc += TEXT(" default=") + Pin->DefaultValue + TEXT(" object=") + GetPathNameSafe(Pin->DefaultObject) + TEXT("]");
				}
				UE_LOG(LogTemp, Warning, TEXT("%s"), *Desc);
			}
			else
			{
				FString Desc = FString::Printf(
					TEXT("DTNODE %s node=%s cls=%s pins="),
					*Blueprint->GetName(), *Node->GetName(),
					*Node->GetClass()->GetName());
				Desc += TEXT(" graph=") + Graph->GetName();
				if (FProperty* ActionProperty = Node->GetClass()->FindPropertyByName(TEXT("InputAction")))
				{
					FString Value; ActionProperty->ExportText_InContainer(0, Value, Node, Node, Node, PPF_None);
					Desc += TEXT(" action=") + Value;
				}
				if (UK2Node_Variable* VarNode = Cast<UK2Node_Variable>(Node))
				{
					const FMemberReference& Ref = VarNode->VariableReference;
					Desc += FString::Printf(TEXT(" varparent=%s varname=%s membername=%s selfctx=%d"),
						Ref.GetMemberParentClass()
							? *Ref.GetMemberParentClass()->GetName()
							: TEXT("NULL"),
						*VarNode->GetVarName().ToString(),
						*Ref.GetMemberName().ToString(),
						Ref.IsSelfContext() ? 1 : 0);
				}
				for (UEdGraphPin* Pin : Node->Pins)
				{
					Desc += FString::Printf(TEXT("[%s:%s:%s links="),
						*Pin->PinName.ToString(),
						*Pin->PinType.PinCategory.ToString(),
						Pin->PinType.PinSubCategoryObject.IsValid()
							? *Pin->PinType.PinSubCategoryObject->GetName()
							: TEXT("-"));
					for (UEdGraphPin* Link : Pin->LinkedTo)
					{
						Desc += Link->GetOuter()->GetName() + TEXT(".") +
						        Link->PinName.ToString() + TEXT(";");
					}
					Desc += TEXT(" default=") + Pin->DefaultValue + TEXT(" object=") + GetPathNameSafe(Pin->DefaultObject) + TEXT("]");
				}
				UE_LOG(LogTemp, Warning, TEXT("%s"), *Desc);
			}
		}
	}
	return true;
#else
	return false;
#endif
}

bool UDTStructRepairLibrary::ProbeVarState(const FString& ObjectPath)
{
#if WITH_EDITOR
	UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *ObjectPath);
	if (!Blueprint)
	{
		return false;
	}
	for (const FBPVariableDescription& Desc : Blueprint->NewVariables)
	{
		FProperty* Prop = Blueprint->GeneratedClass
			? Blueprint->GeneratedClass->FindPropertyByName(Desc.VarName)
			: nullptr;
		UE_LOG(LogTemp, Warning,
		       TEXT("DTVARGAP %s var=%s varguid=%s prop=%s"),
		       *Blueprint->GetName(), *Desc.VarName.ToString(),
		       *Desc.VarGuid.ToString(), Prop ? TEXT("YES") : TEXT("NO"));
	}
	return true;
#else
	return false;
#endif
}

#if WITH_EDITOR
namespace DTStructRepair
{
static FEdGraphPinType BuildSimplePinType(const FString& Category,
                                          const FString& SubPath)
{
	FEdGraphPinType Pin;
	UObject* SubObj = LoadSubObject(SubPath);
	if (Category == TEXT("int"))
	{
		Pin.PinCategory = UEdGraphSchema_K2::PC_Int;
	}
	else if (Category == TEXT("real"))
	{
		Pin.PinCategory = UEdGraphSchema_K2::PC_Real;
		Pin.PinSubCategory = UEdGraphSchema_K2::PC_Float;
	}
	else if (Category == TEXT("bool"))
	{
		Pin.PinCategory = UEdGraphSchema_K2::PC_Boolean;
	}
	else if (Category == TEXT("text"))
	{
		Pin.PinCategory = UEdGraphSchema_K2::PC_Text;
	}
	else if (Category == TEXT("name"))
	{
		Pin.PinCategory = UEdGraphSchema_K2::PC_Name;
	}
	else if (Category == TEXT("object"))
	{
		Pin.PinCategory = UEdGraphSchema_K2::PC_Object;
		Pin.PinSubCategoryObject = SubObj;
	}
	else if (Category == TEXT("class"))
	{
		Pin.PinCategory = UEdGraphSchema_K2::PC_Class;
		Pin.PinSubCategoryObject = SubObj;
	}
	else if (Category == TEXT("struct"))
	{
		Pin.PinCategory = UEdGraphSchema_K2::PC_Struct;
		Pin.PinSubCategoryObject = SubObj;
	}
	return Pin;
}
} // namespace DTStructRepair
#endif

bool UDTStructRepairLibrary::CreateBPFunction(const FString& ObjectPath,
                                              const FString& FuncName,
                                              const TArray<FString>& PinSpecs)
{
#if WITH_EDITOR
	UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *ObjectPath);
	if (!Blueprint)
	{
		return false;
	}
	const FName FuncFName(*FuncName);
	for (UEdGraph* G : Blueprint->FunctionGraphs)
	{
		if (G && G->GetFName() == FuncFName)
		{
			return true;
		}
	}

	UEdGraph* FuncGraph = FBlueprintEditorUtils::CreateNewGraph(
		Blueprint, FuncFName, UEdGraph::StaticClass(),
		UEdGraphSchema_K2::StaticClass());
	if (!FuncGraph)
	{
		return false;
	}
	FBlueprintEditorUtils::AddFunctionGraph(Blueprint, FuncGraph,
	                                      /*bIsUserCreated=*/true,
	                                      /*SignatureFromObject=*/
	                                      static_cast<UFunction*>(nullptr));

	UK2Node_FunctionEntry* Entry = nullptr;
	UK2Node_FunctionResult* Result = nullptr;
	for (UEdGraphNode* N : FuncGraph->Nodes)
	{
		if (!Entry)
		{
			Entry = Cast<UK2Node_FunctionEntry>(N);
		}
		if (!Result)
		{
			Result = Cast<UK2Node_FunctionResult>(N);
		}
	}
	if (!Entry)
	{
		Entry = NewObject<UK2Node_FunctionEntry>(FuncGraph);
		FuncGraph->AddNode(Entry, false, false);
		Entry->CreateNewGuid();
		Entry->PostPlacedNewNode();
		Entry->AllocateDefaultPins();
	}
	if (!Result)
	{
		Result = NewObject<UK2Node_FunctionResult>(FuncGraph);
		FuncGraph->AddNode(Result, false, false);
		Result->CreateNewGuid();
		Result->PostPlacedNewNode();
		Result->AllocateDefaultPins();
		Result->NodePosX = Entry->NodePosX + 300;
	}

	for (const FString& Spec : PinSpecs)
	{
		// Spec: "in|Name|Category|SubObjectPath" or "out|Name|Category|SubObjectPath"
		TArray<FString> Parts;
		Spec.ParseIntoArray(Parts, TEXT("|"), /*bCullEmpty=*/false);
		if (Parts.Num() < 4)
		{
			continue;
		}
		TSharedPtr<FUserPinInfo> PinInfo = MakeShared<FUserPinInfo>();
		PinInfo->PinName = FName(*Parts[1]);
		PinInfo->PinType = DTStructRepair::BuildSimplePinType(Parts[2], Parts[3]);
		if (Parts[0] == TEXT("out"))
		{
			PinInfo->DesiredPinDirection = EGPD_Input;
			Result->UserDefinedPins.Add(PinInfo);
		}
		else
		{
			PinInfo->DesiredPinDirection = EGPD_Output;
			Entry->UserDefinedPins.Add(PinInfo);
		}
	}
	Entry->ReconstructNode();
	Result->ReconstructNode();

	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
	Blueprint->GetOutermost()->MarkPackageDirty();
	UE_LOG(LogTemp, Warning, TEXT("DTFUNC %s func=%s created"),
	       *Blueprint->GetName(), *FuncName);
	return true;
#else
	return false;
#endif
}

bool UDTStructRepairLibrary::RebindVariableRefParents(const FString& ObjectPath)
{
#if WITH_EDITOR
	UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *ObjectPath);
	if (!Blueprint)
	{
		return false;
	}
	int32 Fixed = 0;
	TArray<UEdGraph*> Graphs;
	Blueprint->GetAllGraphs(Graphs);
	for (UEdGraph* Graph : Graphs)
	{
		for (UEdGraphNode* Node : Graph->Nodes)
		{
			UK2Node_Variable* VarNode = Cast<UK2Node_Variable>(Node);
			if (!VarNode)
			{
				continue;
			}
			FMemberReference& Ref = VarNode->VariableReference;
			if (Ref.IsSelfContext() || Ref.GetMemberParentClass())
			{
				continue;
			}
			// Infer the intended owner class from the pin feeding 'self'.
			UEdGraphPin* SelfPin = nullptr;
			for (UEdGraphPin* Pin : Node->Pins)
			{
				if (Pin->Direction == EGPD_Input &&
				    (Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Object ||
				     Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_SoftObject))
				{
					SelfPin = Pin;
					break;
				}
			}
			if (!SelfPin || SelfPin->LinkedTo.Num() == 0)
			{
				continue;
			}
			UObject* SourceObj =
				SelfPin->LinkedTo[0]->PinType.PinSubCategoryObject.Get();
			UClass* OwnerClass = Cast<UClass>(SourceObj);
			while (OwnerClass)
			{
				if (OwnerClass->FindPropertyByName(Ref.GetMemberName()))
				{
					Ref.SetExternalMember(Ref.GetMemberName(), OwnerClass);
					VarNode->ReconstructNode();
					UE_LOG(LogTemp, Warning,
					       TEXT("DTREBIND %s node=%s member=%s parent=%s"),
					       *Blueprint->GetName(), *Node->GetName(),
					       *Ref.GetMemberName().ToString(),
					       *OwnerClass->GetName());
					++Fixed;
					break;
				}
				OwnerClass = OwnerClass->GetSuperClass();
			}
		}
	}
	UE_LOG(LogTemp, Warning, TEXT("DTREBIND %s fixed=%d"),
	       *Blueprint->GetName(), Fixed);
	Blueprint->GetOutermost()->MarkPackageDirty();
	return true;
#else
	return false;
#endif
}

bool UDTStructRepairLibrary::ConnectBlueprintPins(const FString& ObjectPath,
                                                const FString& SrcNodeName,
                                                const FString& SrcPinName,
                                                const FString& DstNodeName,
                                                const FString& DstPinName)
{
#if WITH_EDITOR
	UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *ObjectPath);
	if (!Blueprint)
	{
		return false;
	}
	TArray<UEdGraph*> Graphs;
	Blueprint->GetAllGraphs(Graphs);
	UEdGraphPin* SrcPin = nullptr;
	UEdGraphPin* DstPin = nullptr;
	for (UEdGraph* Graph : Graphs)
	{
		for (UEdGraphNode* Node : Graph->Nodes)
		{
			if (Node->GetName() == SrcNodeName)
			{
				SrcPin = Node->FindPin(FName(*SrcPinName));
			}
			if (Node->GetName() == DstNodeName)
			{
				DstPin = Node->FindPin(FName(*DstPinName));
			}
		}
	}
	if (!SrcPin || !DstPin)
	{
		UE_LOG(LogTemp, Warning, TEXT("DTCONNECT %s pins not found (%s.%s -> %s.%s)"),
		       *Blueprint->GetName(), *SrcNodeName, *SrcPinName,
		       *DstNodeName, *DstPinName);
		return false;
	}
	SrcPin->MakeLinkTo(DstPin);
	Blueprint->GetOutermost()->MarkPackageDirty();
	UE_LOG(LogTemp, Warning, TEXT("DTCONNECT %s %s.%s -> %s.%s"),
	       *Blueprint->GetName(), *SrcNodeName, *SrcPinName,
	       *DstNodeName, *DstPinName);
	return true;
#else
	return false;
#endif
}

bool UDTStructRepairLibrary::ReplaceBlueprintCallFunction(
	const FString& ObjectPath, const FString& OldFunctionName,
	const FString& OwnerClassPath, const FString& NewFunctionName)
{
#if WITH_EDITOR
	UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *ObjectPath);
	UClass* OwnerClass = LoadObject<UClass>(nullptr, *OwnerClassPath);
	UFunction* NewFunction = OwnerClass ? OwnerClass->FindFunctionByName(*NewFunctionName) : nullptr;
	if (!Blueprint || !NewFunction)
	{
		UE_LOG(LogTemp, Warning, TEXT("DTREPLACECALL missing asset/function %s %s::%s"),
		       *ObjectPath, *OwnerClassPath, *NewFunctionName);
		return false;
	}

	int32 Replaced = 0;
	TArray<UEdGraph*> Graphs;
	Blueprint->GetAllGraphs(Graphs);
	for (UEdGraph* Graph : Graphs)
	{
		for (UEdGraphNode* Node : Graph->Nodes)
		{
			UK2Node_CallFunction* Call = Cast<UK2Node_CallFunction>(Node);
			if (!Call || Call->FunctionReference.GetMemberName().ToString() != OldFunctionName)
			{
				continue;
			}

			TArray<UEdGraphPin*> ContextLinks;
			TArray<UEdGraphPin*> ReturnLinks;
			if (UEdGraphPin* OldContext = Call->FindPin(UEdGraphSchema_K2::PN_Self))
			{
				ContextLinks = OldContext->LinkedTo;
			}
			if (UEdGraphPin* OldReturn = Call->FindPin(UEdGraphSchema_K2::PN_ReturnValue))
			{
				ReturnLinks = OldReturn->LinkedTo;
			}
			if (UEdGraphPin* OldContext = Call->FindPin(UEdGraphSchema_K2::PN_Self))
			{
				OldContext->BreakAllPinLinks();
			}
			if (UEdGraphPin* OldReturn = Call->FindPin(UEdGraphSchema_K2::PN_ReturnValue))
			{
				OldReturn->BreakAllPinLinks();
			}

			Call->SetFromFunction(NewFunction);
			Call->ReconstructNode();
			UEdGraphPin* NewContext = Call->FindPin(TEXT("WorldContextObject"));
			if (!NewContext) NewContext = Call->FindPin(UEdGraphSchema_K2::PN_Self);
			UEdGraphPin* NewReturn = Call->FindPin(UEdGraphSchema_K2::PN_ReturnValue);
			for (UEdGraphPin* Linked : ContextLinks)
			{
				if (NewContext && Linked && !Linked->IsPendingKill()) NewContext->MakeLinkTo(Linked);
			}
			for (UEdGraphPin* Linked : ReturnLinks)
			{
				if (NewReturn && Linked && !Linked->IsPendingKill()) NewReturn->MakeLinkTo(Linked);
			}
			++Replaced;
		}
	}
	if (Replaced > 0)
	{
		Blueprint->GetOutermost()->MarkPackageDirty();
	}
	UE_LOG(LogTemp, Warning, TEXT("DTREPLACECALL %s old=%s new=%s count=%d"),
	       *Blueprint->GetName(), *OldFunctionName, *NewFunctionName, Replaced);
	return Replaced > 0;
#else
	return false;
#endif
}

bool UDTStructRepairLibrary::ForceCompileAndSave(const FString& ObjectPath)
{
#if WITH_EDITOR
	UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *ObjectPath);
	if (!Blueprint)
	{
		return false;
	}
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
	FKismetEditorUtilities::CompileBlueprint(
		Blueprint, EBlueprintCompileOptions::SkipSave);
	if (Blueprint->Status != BS_UpToDate && Blueprint->Status != BS_UpToDateWithWarnings)
	{
		UE_LOG(LogTemp, Error, TEXT("DTFCOMP refused to save %s: compile status=%d"),
		       *Blueprint->GetPathName(), (int32)Blueprint->Status);
		return false;
	}
	UPackage* Package = Blueprint->GetOutermost();
	Package->MarkPackageDirty();
	FSavePackageArgs Args;
	Args.TopLevelFlags = RF_Public | RF_Standalone;
	const FString Filename =
		FPackageName::LongPackageNameToFilename(Package->GetName(),
		                                        FPackageName::GetAssetPackageExtension());
	const bool bSaved = UPackage::SavePackage(
		Package, nullptr, *Filename, Args);
	UE_LOG(LogTemp, Warning, TEXT("DTFCOMP %s status=%d saved=%d"),
	       *Blueprint->GetName(), (int32)Blueprint->Status, bSaved ? 1 : 0);
	return bSaved;
#else
	return false;
#endif
}

bool UDTStructRepairLibrary::SaveAssetPackage(const FString& ObjectPath)
{
#if WITH_EDITOR
	UObject* Asset = LoadObject<UObject>(nullptr, *ObjectPath);
	if (!Asset)
	{
		return false;
	}
	if (const UBlueprint* Blueprint = Cast<UBlueprint>(Asset))
	{
		if (Blueprint->Status != BS_UpToDate && Blueprint->Status != BS_UpToDateWithWarnings)
		{
			UE_LOG(LogTemp, Error, TEXT("DTSAVE refused to save %s: compile status=%d"),
			       *Blueprint->GetPathName(), (int32)Blueprint->Status);
			return false;
		}
	}
	UPackage* Package = Asset->GetOutermost();
	Package->MarkPackageDirty();
	FSavePackageArgs Args;
	Args.TopLevelFlags = RF_Public | RF_Standalone;
	const FString Filename =
		FPackageName::LongPackageNameToFilename(Package->GetName(),
		                                        FPackageName::GetAssetPackageExtension());
	return UPackage::SavePackage(Package, nullptr, *Filename, Args);
#else
	return false;
#endif
}

bool UDTStructRepairLibrary::CreateSkeletonAsset(const FString& ObjectPath,
                                                   const TArray<FString>& BoneSpecs)
{
#if WITH_EDITOR
	using namespace DTStructRepair;

	FString AssetName;
	UPackage* Package = GetOrCreatePackage(ObjectPath, AssetName);
	if (!Package)
	{
		return false;
	}

	USkeleton* Skeleton = FindObject<USkeleton>(Package, *AssetName);
	if (!Skeleton)
	{
		Skeleton = NewObject<USkeleton>(Package, *AssetName,
		                                RF_Public | RF_Standalone | RF_Transactional);
	}
	if (!Skeleton)
	{
		return false;
	}

	if (Skeleton->GetReferenceSkeleton().GetNum() == 0)
	{
		FReferenceSkeletonModifier Modifier(Skeleton);
		for (const FString& Spec : BoneSpecs)
		{
			TArray<FString> Parts;
			Spec.ParseIntoArray(Parts, TEXT("|"), /*bCullEmpty=*/false);
			if (Parts.Num() < 1 || Parts[0].IsEmpty())
			{
				continue;
			}
			int32 ParentIndex = INDEX_NONE;
			if (Parts.Num() > 1 && !Parts[1].IsEmpty())
			{
				ParentIndex = Modifier.FindBoneIndex(FName(*Parts[1]));
				if (ParentIndex == INDEX_NONE)
				{
					UE_LOG(LogTemp, Warning,
					       TEXT("DTSKEL %s parent %s missing for bone %s"),
					       *AssetName, *Parts[1], *Parts[0]);
					continue;
				}
			}
			Modifier.Add(FMeshBoneInfo(FName(*Parts[0]), Parts[0], ParentIndex),
			             FTransform::Identity);
		}
	}

	Package->MarkPackageDirty();
	UE_LOG(LogTemp, Warning, TEXT("DTSKEL %s bones=%d"),
	       *AssetName, Skeleton->GetReferenceSkeleton().GetNum());
	return true;
#else
	return false;
#endif
}

bool UDTStructRepairLibrary::AssignAnimSkeleton(const FString& ObjectPath,
                                                  const FString& SkeletonObjectPath)
{
#if WITH_EDITOR
	UAnimBlueprint* AnimBP = LoadObject<UAnimBlueprint>(nullptr, *ObjectPath);
	USkeleton* Skeleton = LoadObject<USkeleton>(nullptr, *SkeletonObjectPath);
	if (!AnimBP || !Skeleton)
	{
		return false;
	}

	AnimBP->TargetSkeleton = Skeleton;
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(AnimBP);
	FKismetEditorUtilities::CompileBlueprint(AnimBP,
		EBlueprintCompileOptions::SkipSave);
	AnimBP->GetOutermost()->MarkPackageDirty();
	UE_LOG(LogTemp, Warning, TEXT("DTSKELASSIGN %s skeleton=%s status=%d"),
	       *AnimBP->GetName(), *Skeleton->GetName(), (int32)AnimBP->Status);
	return true;
#else
	return false;
#endif
}

bool UDTStructRepairLibrary::SetUserPinType(const FString& ObjectPath,
                                            const FString& NodeName,
                                            const FString& PinName,
                                            const FString& Category,
                                            const FString& SubPath)
{
#if WITH_EDITOR
	UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *ObjectPath);
	if (!Blueprint)
	{
		return false;
	}
	TArray<UEdGraph*> Graphs;
	Blueprint->GetAllGraphs(Graphs);
	for (UEdGraph* Graph : Graphs)
	{
		for (UEdGraphNode* Node : Graph->Nodes)
		{
			if (Node->GetName() != NodeName)
			{
				continue;
			}
			UK2Node_EditablePinBase* EditableNode = Cast<UK2Node_EditablePinBase>(Node);
			if (!EditableNode)
			{
				continue;
			}
			for (const TSharedPtr<FUserPinInfo>& PinInfo : EditableNode->UserDefinedPins)
			{
				if (PinInfo.IsValid() && PinInfo->PinName == FName(*PinName))
				{
					PinInfo->PinType = DTStructRepair::BuildSimplePinType(Category, SubPath);
					Node->ReconstructNode();
					Blueprint->GetOutermost()->MarkPackageDirty();
					UE_LOG(LogTemp, Warning, TEXT("DTUSRPIN %s node=%s pin=%s type=%s|%s"),
					       *Blueprint->GetName(), *NodeName, *PinName,
					       *Category, *SubPath);
					return true;
				}
			}
		}
	}
	return false;
#else
	return false;
#endif
}

bool UDTStructRepairLibrary::SetCastTargetClass(const FString& ObjectPath,
                                                const FString& NodeName,
                                                const FString& ClassPath)
{
#if WITH_EDITOR
	UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *ObjectPath);
	UClass* TargetClass = Cast<UClass>(DTStructRepair::LoadSubObject(ClassPath));
	if (!Blueprint || !TargetClass)
	{
		return false;
	}
	TArray<UEdGraph*> Graphs;
	Blueprint->GetAllGraphs(Graphs);
	for (UEdGraph* Graph : Graphs)
	{
		for (UEdGraphNode* Node : Graph->Nodes)
		{
			if (Node->GetName() != NodeName)
			{
				continue;
			}
			UK2Node_DynamicCast* CastNode = Cast<UK2Node_DynamicCast>(Node);
			if (CastNode)
			{
				CastNode->TargetType = TargetClass;
				CastNode->ReconstructNode();
				Blueprint->GetOutermost()->MarkPackageDirty();
				UE_LOG(LogTemp, Warning, TEXT("DTCAST %s node=%s target=%s"),
				       *Blueprint->GetName(), *NodeName, *TargetClass->GetName());
				return true;
			}
		}
	}
	return false;
#else
	return false;
#endif
}

bool UDTStructRepairLibrary::AddMemberVariableExact(const FString& ObjectPath,
                                                    const FString& VarName,
                                                    const FString& Category,
                                                    const FString& SubPath)
{
#if WITH_EDITOR
	UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *ObjectPath);
	if (!Blueprint)
	{
		return false;
	}
	const FName VarFName(*VarName);
	for (FBPVariableDescription& Desc : Blueprint->NewVariables)
	{
		if (Desc.VarName == VarFName)
		{
			Desc.PropertyFlags |= CPF_BlueprintVisible;
			FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
			return true;
		}
	}
	FBPVariableDescription Desc;
	Desc.VarName = VarFName;
	Desc.VarGuid = FGuid::NewGuid();
	Desc.VarType = DTStructRepair::BuildSimplePinType(Category, SubPath);
	Desc.FriendlyName = VarName;
	Desc.PropertyFlags = CPF_Edit | CPF_BlueprintVisible;
	Blueprint->NewVariables.Add(Desc);
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
	Blueprint->GetOutermost()->MarkPackageDirty();
	UE_LOG(LogTemp, Warning, TEXT("DTADDVAR %s var=%s type=%s|%s"),
	       *Blueprint->GetName(), *VarName, *Category, *SubPath);
	return true;
#else
	return false;
#endif
}

TArray<FString> UDTStructRepairLibrary::DescribeMontageNotifies(const FString& ObjectPath)
{
	TArray<FString> Result;
#if WITH_EDITOR
	const UAnimMontage* Montage = LoadObject<UAnimMontage>(nullptr, *ObjectPath);
	if (!Montage)
	{
		Result.Add(FString::Printf(TEXT("LOAD_FAIL %s"), *ObjectPath));
		return Result;
	}
	Result.Add(FString::Printf(TEXT("montage=%s length=%.3f tracks=%d notifies=%d skeleton=%s"),
		*ObjectPath, Montage->GetPlayLength(), Montage->AnimNotifyTracks.Num(),
		Montage->Notifies.Num(), *GetNameSafe(Montage->GetSkeleton())));
	for (const FAnimNotifyEvent& Event : Montage->Notifies)
	{
		Result.Add(FString::Printf(TEXT("notify name=%s notify=%s state=%s time=%.3f dur=%.3f track=%d"),
			*Event.NotifyName.ToString(), *GetNameSafe(Event.Notify),
			*GetNameSafe(Event.NotifyStateClass), Event.GetTriggerTime(), Event.GetDuration(), Event.TrackIndex));
	}
	for (int32 Track = 0; Track < Montage->AnimNotifyTracks.Num(); ++Track)
		Result.Add(FString::Printf(TEXT("track %d name=%s"), Track, *Montage->AnimNotifyTracks[Track].TrackName.ToString()));
	for (const FSlotAnimationTrack& Slot : Montage->SlotAnimTracks)
		Result.Add(FString::Printf(TEXT("slottrack slot=%s"), *Slot.SlotName.ToString()));
#endif
	return Result;
}

#if WITH_EDITOR
static UAnimMontage* LoadMontageForEdit(const FString& ObjectPath)
{
	UAnimMontage* Montage = LoadObject<UAnimMontage>(nullptr, *ObjectPath);
	if (!Montage)
	{
		UE_LOG(LogTemp, Error, TEXT("DTNOTIFY cannot load montage %s"), *ObjectPath);
		return nullptr;
	}
	Montage->Modify();
	if (Montage->AnimNotifyTracks.Num() == 0)
		Montage->AnimNotifyTracks.Add(FAnimNotifyTrack(FName("1"), FLinearColor::White));
	return Montage;
}
#endif

bool UDTStructRepairLibrary::AddMontageGameplayEventNotify(const FString& ObjectPath,
                                                           const FString& EventTag, const float TimeSeconds)
{
#if WITH_EDITOR
	UAnimMontage* Montage = LoadMontageForEdit(ObjectPath);
	if (!Montage) return false;
	const FGameplayTag Tag = FGameplayTag::RequestGameplayTag(FName(*EventTag));
	if (!Tag.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("DTNOTIFY invalid tag %s"), *EventTag);
		return false;
	}
	UDTAnimNotifyEvent* Notify = NewObject<UDTAnimNotifyEvent>(Montage);
	Notify->EventTag = Tag;
	FAnimNotifyEvent& Event = Montage->Notifies.AddDefaulted_GetRef();
	Event.Notify = Notify;
	Event.NotifyName = FName(*EventTag);
	Event.TrackIndex = 0;
	Event.Link(Montage, FMath::Clamp(TimeSeconds, 0.f, Montage->GetPlayLength()));
	Montage->SortNotifies();
	Montage->MarkPackageDirty();
	Montage->PostEditChange();
	return SaveAssetPackage(ObjectPath);
#else
	return false;
#endif
}

bool UDTStructRepairLibrary::AddMontageMeleeWindow(const FString& ObjectPath,
                                                   const float StartSeconds, const float EndSeconds)
{
#if WITH_EDITOR
	UAnimMontage* Montage = LoadMontageForEdit(ObjectPath);
	if (!Montage || EndSeconds <= StartSeconds) return false;
	UDTAnimNotifyMeleeWindow* State = NewObject<UDTAnimNotifyMeleeWindow>(Montage);
	FAnimNotifyEvent& Event = Montage->Notifies.AddDefaulted_GetRef();
	Event.NotifyStateClass = State;
	Event.NotifyName = FName("DTMeleeWindow");
	Event.TrackIndex = 0;
	const float Start = FMath::Clamp(StartSeconds, 0.f, Montage->GetPlayLength());
	const float End = FMath::Clamp(EndSeconds, Start, Montage->GetPlayLength());
	Event.SetDuration(End - Start);
	Event.Link(Montage, Start);
	Event.EndLink.Link(Montage, End);
	Montage->SortNotifies();
	Montage->MarkPackageDirty();
	Montage->PostEditChange();
	return SaveAssetPackage(ObjectPath);
#else
	return false;
#endif
}

bool UDTStructRepairLibrary::ConfigureAbilityRelease(const FString& ObjectPath,
                                                     const FString& EventTag,
                                                     const FString& ProjectileClassPath,
                                                     const FString& SocketName)
{
#if WITH_EDITOR
	URPGCombatAbilityDefinition* Ability = LoadObject<URPGCombatAbilityDefinition>(nullptr, *ObjectPath);
	if (!Ability)
	{
		UE_LOG(LogTemp, Error, TEXT("DTRELEASE cannot load %s"), *ObjectPath);
		return false;
	}
	Ability->Modify();
	if (!EventTag.IsEmpty())
	{
		const FGameplayTag Tag = FGameplayTag::RequestGameplayTag(FName(*EventTag));
		if (!Tag.IsValid())
		{
			UE_LOG(LogTemp, Error, TEXT("DTRELEASE invalid tag %s"), *EventTag);
			return false;
		}
		Ability->ReleaseEventTag = Tag;
	}
	if (!ProjectileClassPath.IsEmpty())
	{
		UClass* ProjectileClass = LoadClass<ARPGProjectile>(nullptr, *ProjectileClassPath);
		if (!ProjectileClass)
		{
			UE_LOG(LogTemp, Error, TEXT("DTRELEASE cannot load class %s"), *ProjectileClassPath);
			return false;
		}
		Ability->ProjectileClass = ProjectileClass;
	}
	if (!SocketName.IsEmpty()) Ability->ReleaseSocket = FName(*SocketName);
	return SaveAssetPackage(ObjectPath);
#else
	return false;
#endif
}

bool UDTStructRepairLibrary::SetMontageSlotName(const FString& ObjectPath, const FString& SlotName)
{
#if WITH_EDITOR
	UAnimMontage* Montage = LoadObject<UAnimMontage>(nullptr, *ObjectPath);
	if (!Montage || SlotName.IsEmpty()) return false;
	Montage->Modify();
	USkeleton* Skeleton = Montage->GetSkeleton();
	const FName Group = Skeleton ? Skeleton->GetSlotGroupName(FName(*SlotName)) : NAME_None;
	for (int32 Index = 0; Index < Montage->SlotAnimTracks.Num(); ++Index)
	{
		Montage->SlotAnimTracks[Index].SlotName = Index == 0 ? FName(*SlotName) :
			FName(*FString::Printf(TEXT("%s_Layer_%d"), *SlotName, Index));
		if (Skeleton)
		{
			Skeleton->RegisterSlotNode(Montage->SlotAnimTracks[Index].SlotName);
			Skeleton->SetSlotGroupName(Montage->SlotAnimTracks[Index].SlotName, Group);
		}
	}
	if (Skeleton) { Skeleton->MarkPackageDirty(); SaveAssetPackage(Skeleton->GetPathName()); }
	Montage->MarkPackageDirty();
	Montage->PostEditChange();
	return SaveAssetPackage(ObjectPath);
#else
	return false;
#endif
}

TArray<FString> UDTStructRepairLibrary::DescribeAnimGraph(const FString& ObjectPath)
{
	TArray<FString> Result;
#if WITH_EDITOR
	const UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *ObjectPath);
	if (!Blueprint)
	{
		Result.Add(TEXT("LOAD_FAIL ") + ObjectPath);
		return Result;
	}
	if (const UAnimBlueprint* AnimBP = Cast<UAnimBlueprint>(Blueprint))
	{
		Result.Add(FString::Printf(TEXT("animbp=%s parent=%s skeleton=%s status=%d"),
			*ObjectPath,
			Blueprint->ParentClass ? *Blueprint->ParentClass->GetPathName() : TEXT("none"),
			*GetNameSafe(AnimBP->TargetSkeleton), static_cast<int32>(Blueprint->Status)));
	}
	TArray<UEdGraph*> Graphs;
	Blueprint->GetAllGraphs(Graphs);
	for (UEdGraph* Graph : Graphs)
	{
		if (!Graph) continue;
		Result.Add(FString::Printf(TEXT("graph=%s schema=%s nodes=%d"),
			*Graph->GetName(),
			Graph->Schema ? *Graph->Schema->GetClass()->GetName() : TEXT("none"),
			Graph->Nodes.Num()));
		for (UEdGraphNode* Node : Graph->Nodes)
		{
			if (!Node) continue;
			FString Line = FString::Printf(TEXT("  node=%s cls=%s title=%s"),
				*Node->GetName(), *Node->GetClass()->GetName(),
				*Node->GetNodeTitle(ENodeTitleType::ListView).ToString());
			if (const UAnimGraphNode_Slot* Slot = Cast<UAnimGraphNode_Slot>(Node))
			{
				Line += FString::Printf(TEXT(" slot_name=%s"), *Slot->Node.SlotName.ToString());
			}
			if (const UAnimGraphNode_LinkedAnimLayer* Layer = Cast<UAnimGraphNode_LinkedAnimLayer>(Node))
			{
				Line += FString::Printf(TEXT(" layer=%s"), *Layer->Node.Layer.ToString());
			}
			if (const UAnimGraphNode_LinkedAnimGraph* Linked = Cast<UAnimGraphNode_LinkedAnimGraph>(Node))
			{
				Line += FString::Printf(TEXT(" instance_class=%s"),
					Linked->Node.InstanceClass ? *Linked->Node.InstanceClass->GetPathName() : TEXT("none"));
			}
			if (const UAnimGraphNode_SaveCachedPose* Save = Cast<UAnimGraphNode_SaveCachedPose>(Node))
			{
				Line += FString::Printf(TEXT(" cache=%s"), *Save->CacheName);
			}
			if (const UAnimGraphNode_UseCachedPose* Use = Cast<UAnimGraphNode_UseCachedPose>(Node))
			{
				Line += FString::Printf(TEXT(" use_cache_node=%s"), *Use->GetNodeTitle(ENodeTitleType::ListView).ToString());
			}
			Result.Add(Line);
		}
	}
#endif
	return Result;
}

bool UDTStructRepairLibrary::RepairEnemyAnimReferences(const FString& ObjectPath)
{
#if WITH_EDITOR
	UBlueprint* BP = LoadObject<UBlueprint>(nullptr, *ObjectPath);
	if (!BP) return false;
	TArray<UEdGraph*> Graphs; BP->GetAllGraphs(Graphs);
	for (UEdGraph* Graph : Graphs) for (UEdGraphNode* Node : Graph->Nodes)
		if (UK2Node_Variable* Variable = Cast<UK2Node_Variable>(Node))
		{
			const FName Name = Variable->GetVarName();
			const FString CompactName = Name.ToString().Replace(TEXT(" "), TEXT(""));
			const bool bStun = CompactName == TEXT("bIsStunned") || CompactName == TEXT("IsStunned");
			if (Variable->VariableReference.IsSelfContext() || bStun)
			{
				// Stun state belongs to the shared AnimInstance and is derived from GAS,
				// not a deleted legacy character property. Clear stale serialized GUIDs.
				if (UEdGraphPin* Self = Variable->FindPin(UEdGraphSchema_K2::PN_Self)) Self->BreakAllPinLinks();
				const FName ResolvedName = bStun ? FName(TEXT("bIsStunned")) : Name;
				TArray<UEdGraphPin*> StunLinks;
				if (bStun)
					for (UEdGraphPin* Pin : Variable->Pins)
					{
						const FString PinName = Pin->PinName.ToString().Replace(TEXT(" "), TEXT(""));
						if (PinName == TEXT("IsStunned") || PinName == TEXT("bIsStunned"))
						{
							StunLinks.Append(Pin->LinkedTo);
							Pin->BreakAllPinLinks();
							Pin->PinName = ResolvedName;
							Pin->bOrphanedPin = false;
						}
					}
				Variable->VariableReference.SetSelfMember(ResolvedName, FGuid());
				Variable->ReconstructNode();
				if (bStun)
					if (UEdGraphPin* Value = Variable->FindPin(ResolvedName))
						for (UEdGraphPin* Link : StunLinks) Graph->GetSchema()->TryCreateConnection(Value, Link);
			}
		}
	// Keep the final full-body slot. The extra DefaultSlot inside the cached
	// locomotion pose evaluated the same montage twice and generated warnings.
	for (UEdGraph* Graph : Graphs)
	{
		// Early loading of the missing legacy variable can drop its direct
		// transition link before reconstruction. Restore that two-node rule.
		{
			UEdGraphPin* StunValue = nullptr;
			UEdGraphPin* Transition = nullptr;
			UEdGraphPin* NegatedInput = nullptr;
			bool bHasCall = false;
			for (UEdGraphNode* Node : Graph->Nodes)
			{
				if (UK2Node_VariableGet* Getter = Cast<UK2Node_VariableGet>(Node))
					if (Getter->GetVarName() == TEXT("bIsStunned")) StunValue = Getter->FindPin(TEXT("bIsStunned"));
				if (Node->GetClass()->GetFName() == TEXT("AnimGraphNode_TransitionResult"))
					Transition = Node->FindPin(TEXT("bCanEnterTransition"));
				if (UK2Node_CallFunction* Call = Cast<UK2Node_CallFunction>(Node))
				{
					bHasCall = true;
					if (Call->GetFunctionName() == TEXT("Not_PreBool")) NegatedInput = Call->FindPin(TEXT("A"));
				}
			}
			if (StunValue && Transition && !bHasCall && Transition->LinkedTo.IsEmpty()) Graph->GetSchema()->TryCreateConnection(StunValue, Transition);
			if (StunValue && NegatedInput && NegatedInput->LinkedTo.IsEmpty()) Graph->GetSchema()->TryCreateConnection(StunValue, NegatedInput);
		}
		TArray<UAnimGraphNode_Slot*> Redundant;
		bool bHasFinalSlot = false;
		for (UEdGraphNode* Node : Graph->Nodes)
			if (UAnimGraphNode_Slot* Slot = Cast<UAnimGraphNode_Slot>(Node))
				if (Slot->Node.SlotName == TEXT("DefaultSlot"))
				{
					bool bFinal = false;
					if (UEdGraphPin* Pose = Slot->FindPin(TEXT("Pose")))
						for (UEdGraphPin* Link : Pose->LinkedTo) bFinal |= Link->GetOwningNode()->IsA<UAnimGraphNode_Root>();
					if (bFinal) bHasFinalSlot = true; else Redundant.Add(Slot);
				}
		if (bHasFinalSlot)
			for (UAnimGraphNode_Slot* Slot : Redundant)
			{
				UEdGraphPin* Source = Slot->FindPin(TEXT("Source"));
				UEdGraphPin* Pose = Slot->FindPin(TEXT("Pose"));
				if (!Source || Source->LinkedTo.Num() != 1 || !Pose) continue;
				UEdGraphPin* Input = Source->LinkedTo[0];
				const TArray<UEdGraphPin*> Outputs = Pose->LinkedTo;
				for (UEdGraphPin* Output : Outputs) { Output->BreakAllPinLinks(); Graph->GetSchema()->TryCreateConnection(Input, Output); }
				FBlueprintEditorUtils::RemoveNode(BP, Slot, true);
			}
	}
	FBlueprintEditorUtils::MarkBlueprintAsModified(BP);
	return true;
#else
	return false;
#endif
}

bool UDTStructRepairLibrary::RepairCombatUpperBodyBlend(const FString& ObjectPath)
{
#if WITH_EDITOR
	UAnimBlueprint* BP = LoadObject<UAnimBlueprint>(nullptr, *ObjectPath);
	if (!BP || !BP->TargetSkeleton) return false;
	BP->TargetSkeleton->RegisterSlotNode(TEXT("Upperbody"));
	BP->TargetSkeleton->SetSlotGroupName(TEXT("Upperbody"), TEXT("DTCombatGroup"));
	BP->TargetSkeleton->MarkPackageDirty();
	TArray<UEdGraph*> Graphs; BP->GetAllGraphs(Graphs);
	for (UEdGraph* Candidate:Graphs) for (UEdGraphNode* Node:Candidate->Nodes)
		if (UAnimGraphNode_Slot* Slot=Cast<UAnimGraphNode_Slot>(Node))
			if (Slot->Node.SlotName==TEXT("DefaultSlot") || Slot->Node.SlotName==TEXT("Upperbody"))
				Slot->Node.bAlwaysUpdateSourcePose=true;
	UEdGraph* Graph = nullptr;
	UAnimGraphNode_Slot* FullBody = nullptr;
	for (UEdGraph* Candidate : Graphs)
	{
		for (UEdGraphNode* Node : Candidate->Nodes)
		{
			if (UAnimGraphNode_Slot* Slot = Cast<UAnimGraphNode_Slot>(Node))
				if (Slot->Node.SlotName == TEXT("Upperbody"))
				{
					// The retained NPC graph already has this blend; repair its bone mask.
					for (UEdGraphNode* Other : Candidate->Nodes)
						if (UAnimGraphNode_LayeredBoneBlend* Layer = Cast<UAnimGraphNode_LayeredBoneBlend>(Other))
						{
							Layer->Node.LayerSetup.SetNum(1);
							Layer->Node.LayerSetup[0].BranchFilters.SetNum(1);
							Layer->Node.LayerSetup[0].BranchFilters[0].BoneName = TEXT("spine_01");
							Layer->Node.LayerSetup[0].BranchFilters[0].BlendDepth = 3;
							Layer->Node.bMeshSpaceRotationBlend = true;
						}
					FBlueprintEditorUtils::MarkBlueprintAsModified(BP);
					return true;
				}
			if (UAnimGraphNode_Slot* Slot = Cast<UAnimGraphNode_Slot>(Node))
				if (Slot->Node.SlotName == TEXT("DefaultSlot")) { Graph=Candidate; FullBody=Slot; }
		}
	}
	if (!Graph || !FullBody) return false;
	UEdGraphPin* FullSource = FullBody->FindPin(TEXT("Source"));
	if (!FullSource || FullSource->LinkedTo.Num()!=1) return false;
	UEdGraphPin* Locomotion = FullSource->LinkedTo[0];
	UAnimGraphNode_SaveCachedPose* Cache = NewObject<UAnimGraphNode_SaveCachedPose>(Graph, TEXT("DT_CombatLocomotionCache"), RF_Transactional);
	Cache->CacheName = TEXT("DT Combat Locomotion");
	UAnimGraphNode_UseCachedPose* Base = NewObject<UAnimGraphNode_UseCachedPose>(Graph, TEXT("DT_CombatLocomotionBase"), RF_Transactional);
	UAnimGraphNode_UseCachedPose* UpperBase = NewObject<UAnimGraphNode_UseCachedPose>(Graph, TEXT("DT_CombatLocomotionUpper"), RF_Transactional);
	Base->SaveCachedPoseNode = Cache; UpperBase->SaveCachedPoseNode = Cache;
	UAnimGraphNode_Slot* Upper = NewObject<UAnimGraphNode_Slot>(Graph, TEXT("DT_CombatUpperbody"), RF_Transactional);
	Upper->Node.SlotName = TEXT("Upperbody");
	Upper->Node.bAlwaysUpdateSourcePose=true;
	UAnimGraphNode_LayeredBoneBlend* Layer = NewObject<UAnimGraphNode_LayeredBoneBlend>(Graph, TEXT("DT_CombatUpperbodyBlend"), RF_Transactional);
	if (Layer->Node.BlendPoses.IsEmpty()) Layer->Node.AddPose();
	Layer->Node.LayerSetup.SetNum(1); Layer->Node.LayerSetup[0].BranchFilters.SetNum(1);
	Layer->Node.LayerSetup[0].BranchFilters[0].BoneName = TEXT("spine_01");
	Layer->Node.LayerSetup[0].BranchFilters[0].BlendDepth = 3;
	Layer->Node.bMeshSpaceRotationBlend = true;
	const TArray<UEdGraphNode*> Nodes = {Cache,Base,UpperBase,Upper,Layer};
	int32 X = FullBody->NodePosX - 900;
	for (UEdGraphNode* Node : Nodes)
	{
		Graph->AddNode(Node, false, false); Node->CreateNewGuid(); Node->PostPlacedNewNode(); Node->AllocateDefaultPins();
		Node->NodePosX=X; X+=160; Node->NodePosY=FullBody->NodePosY+300;
	}
	FullSource->BreakAllPinLinks();
	const UEdGraphSchema* Schema = Graph->GetSchema();
	bool bOK = Schema->TryCreateConnection(Locomotion,Cache->FindPin(TEXT("Pose")));
	bOK &= Schema->TryCreateConnection(Base->FindPin(TEXT("Pose")),Layer->FindPin(TEXT("BasePose")));
	bOK &= Schema->TryCreateConnection(UpperBase->FindPin(TEXT("Pose")),Upper->FindPin(TEXT("Source")));
	bOK &= Schema->TryCreateConnection(Upper->FindPin(TEXT("Pose")),Layer->FindPin(TEXT("BlendPoses_0")));
	bOK &= Schema->TryCreateConnection(Layer->FindPin(TEXT("Pose")),FullSource);
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
	return bOK;
#else
	return false;
#endif
}

bool UDTStructRepairLibrary::RepairPlayerSprintGait()
{
#if WITH_EDITOR
	const FString Path = TEXT("/Game/Blueprints/Characters/BP_PlayerCharacter");
	UBlueprint* BP = LoadObject<UBlueprint>(nullptr, *Path);
	if (!BP) return false;
	TArray<UEdGraph*> Graphs; BP->GetAllGraphs(Graphs);
	for (UEdGraph* Graph : Graphs)
		if (Graph->GetFName() == TEXT("GetDesiredGait"))
			for (UEdGraphNode* Node : Graph->Nodes)
				if (Node->GetFName() == TEXT("K2Node_FunctionResult_3"))
					if (UEdGraphPin* Pin = Node->FindPin(TEXT("ReturnValue")))
						if (const UEnum* Enum = Cast<UEnum>(Pin->PinType.PinSubCategoryObject.Get()))
							for (int32 Index = 0; Index < Enum->NumEnums(); ++Index)
								if (Enum->GetDisplayNameTextByIndex(Index).ToString().Equals(TEXT("Sprint"), ESearchCase::IgnoreCase))
								{
									BP->Modify(); Node->Modify();
									Pin->DefaultValue = Enum->GetNameStringByIndex(Index);
									return ForceCompileAndSave(Path);
								}
#endif
	return false;
}

bool UDTStructRepairLibrary::RestoreMissingMontageSequence(const FString& MontagePath, const FString& SequencePath)
{
#if WITH_EDITOR
	UAnimMontage* Montage = LoadObject<UAnimMontage>(nullptr, *MontagePath);
	UAnimSequence* Sequence = LoadObject<UAnimSequence>(nullptr, *SequencePath);
	if (!Montage || !Sequence || Montage->GetSkeleton() != Sequence->GetSkeleton() ||
		Montage->SlotAnimTracks.IsEmpty() || Montage->SlotAnimTracks[0].AnimTrack.AnimSegments.Num() != 1) return false;
	FAnimSegment& Segment = Montage->SlotAnimTracks[0].AnimTrack.AnimSegments[0];
	if (Segment.GetAnimReference()) return Segment.GetAnimReference() == Sequence;
	Montage->Modify();
	Segment.SetAnimReference(Sequence);
	Montage->PostEditChange();
	Montage->MarkPackageDirty();
	return SaveAssetPackage(MontagePath);
#else
	return false;
#endif
}

TArray<FString> UDTStructRepairLibrary::RepairDuplicateMontageSlots(const FString& ObjectPath, const bool bApply)
{
	TArray<FString> Out;
#if WITH_EDITOR
	UAnimMontage* Montage = LoadObject<UAnimMontage>(nullptr, *ObjectPath);
	if (!Montage) { Out.Add(TEXT("LOAD_FAILED")); return Out; }
	TSet<FName> Names;
	bool Changed = false;
	if (bApply) Montage->Modify();
	for (int32 Index = 0; Index < Montage->SlotAnimTracks.Num(); ++Index)
	{
		FSlotAnimationTrack& Track = Montage->SlotAnimTracks[Index];
		Out.Add(FString::Printf(TEXT("track=%d slot=%s segments=%d"), Index, *Track.SlotName.ToString(), Track.AnimTrack.AnimSegments.Num()));
		for (const FAnimSegment& Segment : Track.AnimTrack.AnimSegments)
			Out.Add(FString::Printf(TEXT("  sequence=%s start=%.3f end=%.3f"), *GetPathNameSafe(Segment.GetAnimReference()), Segment.AnimStartTime, Segment.AnimEndTime));
		if (Names.Contains(Track.SlotName) && bApply)
		{
			// Preserve every sequence/track; only the primary track feeds DefaultSlot.
			// Extra tracks receive unique names so Unreal accepts the montage.
			FName Unique(*FString::Printf(TEXT("DT_UnusedLayer_%d"), Index));
			while (Names.Contains(Unique)) Unique = FName(*(Unique.ToString() + TEXT("_Extra")));
			Track.SlotName = Unique;
			Out.Add(TEXT("renamed_duplicate=") + Unique.ToString());
			Changed = true;
		}
		Names.Add(Track.SlotName);
	}
	if (Changed) { Montage->MarkPackageDirty(); Montage->PostEditChange(); Out.Add(SaveAssetPackage(ObjectPath) ? TEXT("SAVED") : TEXT("SAVE_FAILED")); }
#endif
	return Out;
}
