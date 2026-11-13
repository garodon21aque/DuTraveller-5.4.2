import unreal

tools = unreal.AssetToolsHelpers.get_asset_tools()
ability_path = "/Game/Data/Abilities/DA_AI_BasicAttack"

ability = unreal.load_asset(ability_path)
if not ability:
    factory = unreal.DataAssetFactory()
    ability_class = unreal.load_class(None, "/Script/DuTraveller.RPGCombatAbilityDefinition")
    factory.set_editor_property("data_asset_class", ability_class)
    ability = tools.create_asset("DA_AI_BasicAttack", "/Game/Data/Abilities", ability_class, factory)
if not ability:
    raise RuntimeError("Unable to create AI attack definition")

ability.set_editor_property("ability_id", "AI.BasicAttack")
ability.set_editor_property("display_name", unreal.Text("Basic Attack"))
ability.set_editor_property("description", unreal.Text("Server-authoritative close-range enemy attack."))
ability.set_editor_property("maximum_rank", 1)
ability.set_editor_property("base_damage_by_rank", [12.0])
ability.set_editor_property("attack_power_scale", 1.0)
ability.set_editor_property("ability_power_scale", 0.0)
ability.set_editor_property("stamina_cost", 5.0)
ability.set_editor_property("range", 225.0)
ability.set_editor_property("cooldown", 1.25)
unreal.EditorAssetLibrary.save_loaded_asset(ability)

controller_bp = unreal.load_asset("/Game/Blueprints/AI/BP_RPGAIController")
if not controller_bp:
    raise RuntimeError("Missing BP_RPGAIController")
controller_cdo = unreal.get_default_object(controller_bp.generated_class())
controller_cdo.set_editor_property("attack_ability", ability)
controller_cdo.set_editor_property("detection_range", 1500.0)
controller_cdo.set_editor_property("lose_target_range", 2200.0)
controller_cdo.set_editor_property("decision_interval", 0.25)
unreal.BlueprintEditorLibrary.compile_blueprint(controller_bp)
unreal.EditorAssetLibrary.save_loaded_asset(controller_bp)

controller_class = controller_bp.generated_class()
for path in ["/Game/Blueprints/AI/BP_RPGAICharacter", "/Game/Blueprints/AI/BP_AIRangedCharacter"]:
    bp = unreal.load_asset(path)
    if not bp:
        continue
    cdo = unreal.get_default_object(bp.generated_class())
    cdo.set_editor_property("ai_controller_class", controller_class)
    cdo.set_editor_property("auto_possess_ai", unreal.AutoPossessAI.PLACED_IN_WORLD_OR_SPAWNED)
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    unreal.EditorAssetLibrary.save_loaded_asset(bp)
    unreal.log("AI_COMBAT_CONFIGURED %s STATUS=%s" % (path, bp.status))

unreal.log("AI_COMBAT_ABILITY %s CONTROLLER_STATUS=%s" % (ability_path, controller_bp.status))

