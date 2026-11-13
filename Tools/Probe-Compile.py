import unreal
lib=unreal.BlueprintEditorLibrary
bp=unreal.load_asset('/Game/UI/InventorySystem/WBP_PlayerStat')
gc0=bp.generated_class()
print('before:', gc0)
try:
    cdo=gc0.get_default_object(); cdo.get_editor_property('health'); print('pre-prop OK')
except Exception as e:
    print('pre-prop missing')
r=lib.compile_blueprint(bp)
print('compile result:', r)
gc=bp.generated_class()
print('after gc:', gc)
try:
    cdo=gc.get_default_object(); v=cdo.get_editor_property('health'); print('post-prop OK',v)
except Exception as e:
    print('post-prop missing:',e)
print('status:', bp.get_editor_property('status') if hasattr(bp,'get_editor_property') else '?')
