#include "Modules/ModuleManager.h"
#include "DTMeshyEditorLibrary.h"
#include "ToolMenus.h"

#define LOCTEXT_NAMESPACE "FDuTravellerEditorModule"

class FDuTravellerEditorModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		if (!IsRunningGame())
		{
			UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateLambda([]()
			{
				UToolMenu* Menu = UToolMenus::Get()->ExtendMenu("LevelEditor.MainMenu.Tools");
				FToolMenuSection& Section = Menu->AddSection("DuTraveller", LOCTEXT("DT", "DuTraveller"));
				FToolMenuEntry Entry = FToolMenuEntry::InitMenuEntry(
					FName("DuTravellerCharacterRetargeter"),
					LOCTEXT("RetargeterLabel", "DuTraveller Character Retargeter (Meshy)"),
					LOCTEXT("RetargeterTip", "Open EUW_DuTraveller_CharacterRetargeter - Meshy import, IK retargeting and enemy generation"),
					FSlateIcon(),
					FUIAction(FExecuteAction::CreateLambda([]()
					{
						UDTMeshyEditorLibrary::SpawnRetargeterEUW();
					})));
				Section.AddEntry(Entry);
			}));
		}
	}

	virtual void ShutdownModule() override {}
};

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FDuTravellerEditorModule, DuTravellerEditor)
