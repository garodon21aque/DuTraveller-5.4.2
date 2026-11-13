import unreal
for p in ['/Game/Blueprints/Actors/Interactables/Items/BP_MasterPickupItem',
          '/Game/Blueprints/Actors/Interactables/Items/BP_Pickup']:
    try:
        a = unreal.load_asset(p)
        print('LOADED', p, type(a), a.get_class().get_name() if a else None)
    except Exception as e:
        print('FAIL', p, repr(e))
unreal.SystemLibrary.quit_editor()
