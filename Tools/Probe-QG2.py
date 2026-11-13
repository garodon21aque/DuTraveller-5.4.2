import unreal
lib=unreal.BlueprintEditorLibrary
bp=unreal.load_asset('/Game/Blueprints/Quest/BP_QuestGiver')
print('status pre:',bp.get_editor_property('status'))
r=lib.compile_blueprint(bp)
print('compile returned:',r)
print('status post:',bp.get_editor_property('status'))
gc=bp.generated_class()
for p in ['DataTable','QuestID','CurrentObjectiveProgress']:
    try:
        gc.get_default_object().get_editor_property(p); print(p,'OK')
    except Exception:
        print(p,'MISSING')
unreal.EditorAssetLibrary.save_loaded_asset(bp, only_if_is_dirty=False)
print('saved')
