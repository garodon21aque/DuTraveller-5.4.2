import unreal
lib=unreal.BlueprintEditorLibrary
bp=unreal.load_asset('/Game/UI/InventorySystem/WBP_PlayerStat')
print('vars:',lib.list_member_variable_names(bp))
gc=bp.generated_class()
for p in ['health','mana','level']:
    try:
        v=gc.get_default_object().get_editor_property(p); print(p,'= OK',v)
    except Exception as e:
        print(p,'= MISSING')
