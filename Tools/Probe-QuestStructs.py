import unreal

for path in ("/Game/Blueprints/Quest/FQuestDetails",
             "/Game/Blueprints/Quest/FQuestDetails.FQuestDetails",
             "/Game/Blueprints/Quest/FObjectiveDetails.FObjectiveDetails",
             "/Game/Blueprints/Quest/FStageDetails.FStageDetails"):
    o = unreal.load_object(None, path)
    unreal.log("{0} -> {1} class={2}".format(
        path, o, o.get_class().get_name() if o else None))

# What does the redirector mechanism produce?
import unreal as u
bp = u.load_asset("/Game/UI/QuestSystem/WBP_Objectives")
names = u.BlueprintEditorLibrary.list_member_variable_names(bp)
unreal.log("WBP_Objectives vars: {0}".format([n for n in names if "/" not in n]))
