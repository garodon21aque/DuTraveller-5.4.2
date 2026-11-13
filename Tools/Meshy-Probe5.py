import unreal
gs = [x for x in dir(unreal.GameplayStatics) if 'spawn' in x.lower() or 'actor' in x.lower()]
unreal.log("GS: " + str(gs))
w = [x for x in dir(unreal.World) if 'spawn' in x.lower() or 'actor' in x.lower()]
unreal.log("WORLD: " + str(w))
unreal.log("DONE")
unreal.SystemLibrary.quit_editor()
