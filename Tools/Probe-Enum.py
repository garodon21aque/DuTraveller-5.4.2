import unreal
en = unreal.load_asset('/Game/Blueprints/Quest/EQuestTypes')
try:
    for i in range(8):
        print(i, en.get_display_name_text_by_index(i), '|', en.get_name_string_by_index(i), '|', en.get_name_by_index(i))
except Exception as e:
    print('err', e)
