#include "DuTraveller.h"
#include "Modules/ModuleManager.h"
#include "HAL/IConsoleManager.h"

namespace
{
// The retained DT AnimBP reads this sample selector every frame. Register it
// independently of the old sample debug widget so the unified GameMode works.
TAutoConsoleVariable<int32> CVarDTMMDatabaseLOD(
	TEXT("DDCvar.MMDatabaseLOD"), 0,
	TEXT("DT motion matching database density selector used by ABP_SandboxCharacter."), ECVF_Default);
}

IMPLEMENT_GAME_MODULE(FDefaultGameModuleImpl, DuTraveller);
