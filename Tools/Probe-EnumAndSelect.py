import unreal
en = unreal.load_asset('/Game/Blueprints/Quest/EQuestTypes')
print('enum:', en, type(en))
try:
    names = en.get_editor_property('names')
    print('names:', names)
except Exception as e:
    print('names err:', e)
try:
    for i in range(en.get_editor_property('num_enums') if hasattr(en,'num_enums') else 8):
        print(i, en.get_name_string_by_index(i), '|', en.get_name_by_index(i))
except Exception as e:
    print('idx err:', e)
# Dump WBP_QuestLog Select nodes specifically
unreal.DTStructRepairLibrary.dump_blueprint_graph('/Game/UI/QuestSystem/WBP_QuestLog.WBP_QuestLog')
