import unreal

qg = unreal.load_asset("/Game/Blueprints/Quest/BP_QuestGiver")
names = unreal.BlueprintEditorLibrary.list_member_variable_names(qg)
unreal.log("vars: {0}".format([n for n in names if "/" not in n]))
for n in names:
    if "/" not in n:
        t = unreal.BlueprintEditorLibrary.get_member_variable_type(qg, n)
        unreal.log("  {0}: {1}".format(n, t.export_text()))
