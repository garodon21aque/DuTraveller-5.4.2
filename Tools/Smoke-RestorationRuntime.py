"""Bounded PIE inspection of existing maps; never saves project assets."""
import json
import hashlib
from pathlib import Path
import time
import traceback
import unreal

unreal.EditorPythonScripting.set_keep_python_script_alive(True)

started = time.monotonic()
phase_started = started
map_name = "MainMenu" if "-RestorationMap=MainMenu" in unreal.SystemLibrary.get_command_line() else "Tutorial"
phase = "menu_wait" if map_name == "MainMenu" else "tutorial_wait"
callback = None
test_pawn = None
movement_start = None
input_subsystem = None
move_action = None
move_value = None
editor = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
worlds = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)


def log(kind, **data):
    unreal.log("RESTORE_RUNTIME " + json.dumps(dict(kind=kind, **data), default=str))


def finish(reason):
    log("finished", reason=reason)
    if callback is not None:
        unreal.unregister_slate_post_tick_callback(callback)
    editor.editor_request_end_play()
    unreal.SystemLibrary.quit_editor()


def tick(delta):
    global phase, phase_started, test_pawn, movement_start, input_subsystem, move_action, move_value
    try:
        now = time.monotonic()
        if now - started > 150:
            finish("timeout: " + phase)
            return
        world = worlds.get_game_world()
        if phase == "menu_wait" and world and now - phase_started > 3:
            widgets = unreal.WidgetLibrary.get_all_widgets_of_class(world, unreal.UserWidget, False)
            controller = unreal.GameplayStatics.get_player_controller(world, 0)
            log("menu", world=world.get_path_name(),
                top_level_widgets=[w.get_class().get_path_name() for w in widgets if w.is_in_viewport()],
                mouse_cursor=controller.get_editor_property("show_mouse_cursor") if controller else None)
            finish("menu inspection complete")
        elif phase == "tutorial_wait" and world and now - phase_started > 5:
            controller = unreal.GameplayStatics.get_player_controller(world, 0)
            pawn = controller.get_controlled_pawn() if controller else None
            log("tutorial", world=world.get_path_name(),
                controller=controller.get_class().get_path_name() if controller else None,
                pawn=pawn.get_class().get_path_name() if pawn else None,
                components=[c.get_class().get_path_name() for c in pawn.get_components_by_class(unreal.ActorComponent)] if pawn else [])
            widgets = unreal.WidgetLibrary.get_all_widgets_of_class(world, unreal.UserWidget, True)
            log("tutorial_ui", top_level_widgets=[w.get_class().get_path_name() for w in widgets])
            if pawn:
                movement = pawn.get_editor_property("character_movement")
                log("movement_state", max_speed=movement.get_editor_property("max_walk_speed"),
                    movement_mode=str(movement.get_editor_property("movement_mode")),
                    paused=unreal.GameplayStatics.is_game_paused(world),
                    input_ignored=controller.is_move_input_ignored())
                mesh = pawn.get_editor_property("mesh")
                log("animation_state", tick_policy=str(mesh.get_editor_property("visibility_based_anim_tick_option")),
                    anim_instance=str(mesh.get_anim_instance()))
                if "-nullrhi" in unreal.SystemLibrary.get_command_line().lower():
                    # Root-motion characters need their animation evaluated even
                    # when a headless test never renders the mesh. PIE-only change.
                    mesh.set_editor_property("visibility_based_anim_tick_option",
                                             unreal.VisibilityBasedAnimTickOption.ALWAYS_TICK_POSE_AND_REFRESH_BONES)
                    log("headless_override", animation_tick="AlwaysTickPoseAndRefreshBones", saved=False)
                test_pawn = pawn
                movement_start = pawn.get_actor_location()
                phase = "movement"
                phase_started = now
            else:
                finish("blocked: no possessed pawn")
        elif phase == "movement" and test_pawn:
            if now - phase_started < 1:
                test_pawn.add_movement_input(unreal.Vector(1, 0, 0), 1, False)
            else:
                end = test_pawn.get_actor_location()
                log("programmatic_movement", start=str(movement_start), end=str(end),
                    horizontal_distance=((end.x - movement_start.x) ** 2 + (end.y - movement_start.y) ** 2) ** 0.5,
                    note="Direct movement input; physical keyboard and mouse unverified")
                if not hasattr(unreal, "SubsystemBlueprintLibrary"):
                    log("action_movement_skipped",
                        reason="UE 5.8 Python does not expose SubsystemBlueprintLibrary",
                        note="Physical keyboard and gamepad input remain manual Editor checks")
                    finish("inspection complete; enhanced-input injection unavailable")
                    return
                getter_name = next(n for n in dir(unreal.SubsystemBlueprintLibrary)
                                   if n.startswith("get_local_player") and n.endswith("from_player_controller"))
                input_subsystem = getattr(unreal.SubsystemBlueprintLibrary, getter_name)(
                    unreal.GameplayStatics.get_player_controller(world, 0), unreal.EnhancedInputLocalPlayerSubsystem)
                move_action = unreal.load_asset("/Game/Input/IA_Move")
                move_value = unreal.EnhancedInputLibrary.make_input_action_value_of_type(
                    0, 1, 0, move_action.get_editor_property("value_type"))
                if not input_subsystem:
                    finish("blocked: missing enhanced input subsystem")
                    return
                movement_start = end
                phase_started = now
                phase = "action_movement"
        elif phase == "action_movement" and test_pawn:
            if now - phase_started < 1:
                input_subsystem.inject_input_for_action(move_action, move_value, [], [])
            else:
                end = test_pawn.get_actor_location()
                log("action_movement", start=str(movement_start), end=str(end),
                    horizontal_distance=((end.x - movement_start.x) ** 2 + (end.y - movement_start.y) ** 2) ** 0.5,
                    note="Existing IA_Move action injected; physical keyboard and mouse unverified")
                finish("inspection complete")
    except Exception:
        log("exception", traceback=traceback.format_exc())
        finish("exception")


try:
    # Exercise the save refusal against an already-failing Blueprint. Loading
    # and checking status does not compile, edit, or save it.
    guard_asset = unreal.load_asset("/Game/UI/InventorySystem/WBP_PlayerStat")
    guard_status = str(guard_asset.get_editor_property("status"))
    if "ERROR" in guard_status.upper():
        guard_file = Path(unreal.Paths.project_content_dir()) / "UI/InventorySystem/WBP_PlayerStat.uasset"
        before = hashlib.sha256(guard_file.read_bytes()).hexdigest()
        saved = unreal.DTStructRepairLibrary.save_asset_package(guard_asset.get_path_name())
        after = hashlib.sha256(guard_file.read_bytes()).hexdigest()
        log("save_guard", status=guard_status, refused=not saved, unchanged=before == after)
        if saved or before != after:
            raise RuntimeError("Failed Blueprint save guard did not preserve the asset")
    else:
        log("save_guard_skipped", reason="Asset is not currently in error status", status=guard_status)
    # One map per process: avoid loading or switching editor worlds from a
    # Slate callback while World Partition initialization can pump more ticks.
    current_world = worlds.get_editor_world()
    if not current_world or current_world.get_name() != map_name:
        unreal.EditorLoadingAndSavingUtils.load_map("/Game/Levels/" + map_name)
    callback = unreal.register_slate_post_tick_callback(tick)
    editor.editor_request_begin_play()
    phase_started = time.monotonic()
    log("started")
except Exception:
    log("exception", traceback=traceback.format_exc())
    finish("startup exception")
