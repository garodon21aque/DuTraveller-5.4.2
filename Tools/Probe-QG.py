import unreal
lib=unreal.BlueprintEditorLibrary
bp=unreal.load_asset('/Game/Blueprints/Quest/BP_QuestGiver')
print('vars:',lib.list_member_variable_names(bp))
gc=bp.generated_class()
for p in ['DataTable','Data Table','QuestID','CurrentObjectiveProgress']:
    try:
        gc.get_default_object().get_editor_property(p); print(p,'OK')
    except Exception:
        print(p,'MISSING')
