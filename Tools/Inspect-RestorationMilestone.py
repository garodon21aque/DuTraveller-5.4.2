"""Read-only wiring inspection. Never compile or save project assets."""
import json
import unreal


def log(kind, **data):
    unreal.log("RESTORE_INSPECT " + json.dumps(dict(kind=kind, **data), default=str))


def path(obj):
    return obj.get_path_name() if obj else None


try:
    for name in (
        "/Game/Blueprints/Framework/DT_GameModeBase",
        "/Game/Blueprints/Characters/BP_PlayerCharacter",
        "/Game/Blueprints/Player/BP_PlayerCharacter",
        "/Game/Blueprints/Player/BP_RPGCharacter",
        "/Game/Blueprints/Actors/Interactables/Items/BP_MasterPickupItem",
    ):
        asset = unreal.load_asset(name)
        if not asset:
            log("missing", asset=name)
            continue
        cls = asset.generated_class()
        cdo = unreal.get_default_object(cls)
        props = {}
        for prop in ("default_pawn_class", "player_controller_class", "hud_class",
                     "auto_possess_player", "item"):
            try:
                props[prop] = str(cdo.get_editor_property(prop))
            except Exception:
                pass
        log("class", asset=name, generated_class=path(cls), properties=props)
        unreal.DTStructRepairLibrary.dump_blueprint_graph(name)

    unreal.DTStructRepairLibrary.dump_blueprint_graph("/Game/UI/Menu/WBP_MainMenu")

    for map_name in ("/Game/Levels/MainMenu", "/Game/Levels/Tutorial"):
        world = unreal.EditorLoadingAndSavingUtils.load_map(map_name)
        if not world:
            log("map_failed", map=map_name)
            continue
        settings = world.get_world_settings()
        log("map", map=map_name, game_mode=path(settings.get_editor_property("default_game_mode")))
        actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
        for actor in actors:
            cls_name = actor.get_class().get_name()
            if any(word in cls_name.lower() for word in ("player", "pickup", "gamemode", "start")):
                log("actor", map=map_name, actor=path(actor), actor_class=path(actor.get_class()))
    log("complete")
except Exception:
    import traceback
    unreal.log_error("RESTORE_INSPECT " + traceback.format_exc())
finally:
    unreal.SystemLibrary.quit_editor()
