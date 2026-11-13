"""Bounded, no-save real PIE test of panel ownership, context, cameras and initial progression."""
import json
from pathlib import Path
import time
import traceback
import unreal

unreal.EditorPythonScripting.set_keep_python_script_alive(True)
editor = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
worlds = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
started = time.monotonic()
phase = "wait"
next_time = started
index = 0
callback = None
report = {"checks": [], "panels": [], "headless": "-nullrhi" in unreal.SystemLibrary.get_command_line().lower(),
          "limits": ["No camera pixel or visual layout validation", "No physical keyboard/gamepad test", "No visual combat pose, blend or combo timing validation"]}
panel_names = ["Inventory", "Crafting", "Skills", "Talents", "Quests", "Party", "Guild", "Attributes", "Settings", "Pause"]

def check(name, ok, details=None):
    report["checks"].append(dict(name=name, passed=bool(ok), details=details))
    unreal.log("DT_UI_CHECK " + json.dumps(report["checks"][-1], default=str))

def finish(reason):
    report["reason"] = reason
    report["passed"] = all(row["passed"] for row in report["checks"]) and reason == "complete"
    output = Path(unreal.Paths.project_saved_dir()) / "Automation" / "UnifiedPlayerUIRuntime.json"
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(report, indent=2, default=str), encoding="utf-8")
    if callback is not None:
        unreal.unregister_slate_post_tick_callback(callback)
    editor.editor_request_end_play()
    unreal.log("DT_UI_RUNTIME " + json.dumps(report, default=str))
    unreal.SystemLibrary.quit_editor()

def tick(delta):
    global phase, next_time, index
    try:
        now = time.monotonic()
        if now - started > 180:
            finish("timeout " + phase)
            return
        world = worlds.get_game_world()
        if not world or now < next_time:
            return
        controller = unreal.GameplayStatics.get_player_controller(world, 0)
        pawn = controller.get_controlled_pawn() if controller else None
        if phase == "wait":
            if not pawn or now - started < 8:
                return
            state = controller.get_editor_property("player_state")
            check("Unified controller", isinstance(controller, unreal.RPGPlayerController))
            check("One root exists", controller.get_editor_property("ui_root") is not None)
            hud = controller.get_hud()
            check("World HUD shares controller root", isinstance(hud, unreal.DT_HUD) and hud.get_player_ui_root() == controller.get_editor_property("ui_root"))
            check("5 initial attribute points", state.get_editor_property("attribute_points") == 5, state.get_editor_property("attribute_points"))
            check("3 initial skill points", state.get_editor_property("spell_points") == 3, state.get_editor_property("spell_points"))
            skills = state.get_editor_property("skills")
            check("Player skill catalogue table assigned", skills.get_editor_property("ability_catalogue") is not None)
            check("Skill definitions resolve from catalogue", len(skills.get_available_definitions()) >= 4)
            group = unreal.load_asset("/Game/Data/Quests/DA_QuestGroup_LocationDiscovery")
            check("Location quest catalogue table assigned", group.get_editor_property("quest_catalogue") is not None)
            check("Quest group resolves table entries", len(group.get_quest_entries()) >= 2)
            legacy_table = unreal.load_asset("/Game/Blueprints/Quest/QuestData")
            legacy_struct = unreal.load_object(None, "/Game/Blueprints/Quest/FQuestDetails.FQuestDetails")
            check("Legacy quest table matches Blueprint row struct", unreal.DataTableFunctionLibrary.get_data_table_row_struct(legacy_table) == legacy_struct)
            legacy_rows = json.loads(unreal.DataTableFunctionLibrary.export_data_table_to_json_string(legacy_table))
            check("Legacy quest rows have stages and objectives", len(legacy_rows) >= 2 and all(row.get("Stages") and row["Stages"][0].get("Objectives") for row in legacy_rows))
            crafting = pawn.get_editor_property("crafting_comp")
            check("Crafting catalogue table assigned", crafting.get_editor_property("recipe_catalogue") is not None)
            check("Crafting recipe instances load at startup", len(crafting.get_recipes()) >= 1)
            check("Starter skills learned", len(skills.get_editor_property("learned_abilities")) >= 2)
            check("Starter skills equipped", len(skills.get_editor_property("hotbar")) >= 2)
            presentation = skills.get_ability_presentation()
            check("Player skill icons assigned", sum(1 for info in presentation if info.get_editor_property("icon")) >= 4)
            controller.initialize_player_ui()
            controller.initialize_player_ui()
            widget_api = getattr(unreal, "WidgetBlueprintLibrary", getattr(unreal, "WidgetLibrary", None))
            if widget_api:
                screen_widgets = [item for item in widget_api.get_all_widgets_of_class(world, unreal.UserWidget, False)
                                  if item.is_in_viewport() and item.get_owning_player() == controller]
                check("Only unified root attached to player screen", len(screen_widgets) == 1 and screen_widgets[0] == controller.get_editor_property("ui_root"),
                      [item.get_class().get_path_name() for item in screen_widgets])
            check("Repeated UI initialization does not grant points again", state.get_editor_property("attribute_points") == 5 and state.get_editor_property("spell_points") == 3)
            strength = unreal.GameplayTag()
            strength.import_text('(TagName="Attributes.Primary.Strength")')
            before = state.get_attribute_value("Strength", True)
            state.request_upgrade_attribute(strength)
            check("Attribute allocation spends one point", state.get_editor_property("attribute_points") == 4)
            check("Attribute allocation changes GAS base", state.get_attribute_value("Strength", True) > before)
            starter = skills.get_editor_property("hotbar")[0].get_editor_property("definition")
            rank = skills.get_learned_rank(starter.get_editor_property("ability_id"))
            skills.request_learn_ability(starter)
            check("Skill allocation spends one point", state.get_editor_property("spell_points") == 2)
            check("Skill allocation increases learned rank", skills.get_learned_rank(starter.get_editor_property("ability_id")) == rank + 1)
            animation = pawn.get_editor_property("mesh").get_anim_instance()
            animation_class = animation.get_class().get_path_name() if animation else None
            check("DT Motion Matching AnimInstance retained", animation_class == "/Game/Blueprints/Animations/ABP_SandboxCharacter.ABP_SandboxCharacter_C", animation_class)
            check("Shared animation router", pawn.get_editor_property("animation_router") is not None)
            recovered_mesh = unreal.load_asset("/Game/Characters/Mannequin_UE4/Meshes/SK_Mannequin")
            recovered_skeleton = unreal.load_asset("/Game/Characters/Mannequin_UE4/Meshes/SK_Mannequin_Skeleton")
            check("Recovered UE4 preview mesh skeleton persists", recovered_mesh.get_editor_property("skeleton") == recovered_skeleton)
            material_paths = [slot.get_editor_property("material_interface").get_path_name()
                              for slot in recovered_mesh.get_editor_property("materials") if slot.get_editor_property("material_interface")]
            check("Recovered UE4 preview materials use project packages", len(material_paths) >= 2 and all(path.startswith("/Game/Characters/Mannequin_UE4/") for path in material_paths), material_paths)
            for capture in ("front_capture", "face_capture", "minimap_capture"):
                component = pawn.get_editor_property(capture)
                check(capture + " render target", component.get_editor_property("texture_target") is not None)
            pawn.get_editor_property("mesh").set_editor_property("visibility_based_anim_tick_option", unreal.VisibilityBasedAnimTickOption.ALWAYS_TICK_POSE_AND_REFRESH_BONES)
            candidate = unreal.load_asset("/Game/DTIntegration/Animation/Combat/DT_Sword_And_Shield_Slash_mixamo_com_Montage_UEFN")
            profile = unreal.DTCombatAnimation()
            profile.set_editor_property("montage", candidate)
            router = pawn.get_editor_property("animation_router")
            check("Retargeted combat montage starts through shared router", router.play_combat_animation(profile))
            check("Router owns the active combat montage", router.get_editor_property("active_combat_montage") == candidate)
            phase = "open"
        elif phase == "open":
            if index >= len(panel_names):
                controller.close_ui_panels()
                check("Closed panels restore cursor", not controller.get_editor_property("show_mouse_cursor"))
                finish("complete")
                return
            name = panel_names[index]
            ok = controller.open_ui_panel(name)
            check(name + " opens", ok)
            phase = "inspect"
            next_time = now + 0.8
        elif phase == "inspect":
            name = panel_names[index]
            panels = controller.get_editor_property("panels")
            widget = panels.get(unreal.Name(name))
            check(name + " owned by player", widget is not None and widget.get_owning_player() == controller)
            check(name + " attached to root layer", widget is not None and widget.get_parent() is not None)
            if widget and name in ("Inventory", "Crafting"):
                prop = "Inventory" if name == "Inventory" else "CraftingComp"
                check(name + " context assigned", widget.get_editor_property(prop) is not None)
            if widget and name == "Crafting":
                check("Crafting menu reads table-backed recipes", len(widget.get_available_recipes()) >= 1)
            if widget and name == "Inventory":
                check("Inventory preview image attached", isinstance(widget, unreal.DTPlayerInventoryWidget) and widget.get_editor_property("character_preview") is not None)
            if widget and name == "Quests":
                check("Quest journal reads PlayerState", isinstance(widget, unreal.DTQuestJournalWidget))
                check("Quest journal attached to retained layout", widget.get_editor_property("journal_entries") is not None)
                check("Transient location quest offer generated", unreal.DTStructRepairLibrary.run_foundation_client_test_action(pawn, 10))
                widget.refresh_journal()
                check("Nearby quest offers shown in journal", widget.get_editor_property("nearby_offer_count") >= 1)
                quests = controller.get_editor_property("player_state").get_editor_property("dynamic_quests")
                for source in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.RPGRandomQuestSpawner):
                    offers = source.get_editor_property("offers")
                    if offers:
                        quests.request_accept_offer(source, offers[0].get_editor_property("offer_id"))
                        break
                widget.refresh_journal()
                check("Accepted quest appears in journal", widget.get_editor_property("owned_quest_count") >= 1)
            report["panels"].append({"name": name, "class": widget.get_class().get_path_name() if widget else None})
            controller.close_ui_panels()
            index += 1
            phase = "open"
            next_time = now + 0.2
    except Exception:
        report["exception"] = traceback.format_exc()
        finish("exception")

unreal.EditorLoadingAndSavingUtils.load_map("/Game/Levels/Tutorial")
callback = unreal.register_slate_post_tick_callback(tick)
editor.editor_request_begin_play()
