import unreal
lib = unreal.DTStructRepairLibrary
for path in [
    "/Game/Art/3D/Player/Animations/Anims/AnimBP_RPGCharacter.AnimBP_RPGCharacter",
    "/Game/Art/3D/Player/Animations/Anims/AnimBP_RPGAICharacter.AnimBP_RPGAICharacter",
    "/Game/Art/3D/Player/Animations/Anims/AnimBP_Lion.AnimBP_Lion",
    "/Game/Art/3D/Player/Animations/Anims/AnimBP_Wolf.AnimBP_Wolf",
]:
    try:
        lib.refresh_blueprint_nodes(path)
    except Exception as e:
        unreal.log("R4b refresh threw for %s" % path)
    try:
        saved = lib.save_asset_package(path)
        unreal.log("R4b saved=%d %s" % (1 if saved else 0, path))
    except Exception as e:
        unreal.log("R4b save threw for %s" % path)
unreal.log("R4b done")
