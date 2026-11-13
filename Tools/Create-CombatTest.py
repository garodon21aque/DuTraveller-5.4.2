import json
import shutil
from pathlib import Path
import unreal
root=Path(unreal.Paths.project_dir())
tools=unreal.AssetToolsHelpers.get_asset_tools()
repair=unreal.DTStructRepairLibrary
report={"passed":False,"assets":[],"errors":[]}
def backup(path):
    file=root/("Content/"+path[len("/Game/"):]+".uasset")
    if file.exists():
        target=root/"Saved/Backups/CombatEncounter"/file.relative_to(root)
        target.parent.mkdir(parents=True,exist_ok=True)
        if not target.exists(): shutil.copy2(file,target)
def bp(path,parent):
    existing=unreal.load_asset(path)
    if existing:
        backup(path)
        unreal.BlueprintEditorLibrary.compile_blueprint(existing)
        return existing
    factory=unreal.BlueprintFactory(); factory.set_editor_property("parent_class",parent)
    created=tools.create_asset(path.rsplit("/",1)[1],path.rsplit("/",1)[0],unreal.Blueprint,factory)
    unreal.BlueprintEditorLibrary.compile_blueprint(created)
    return created
def save(asset):
    if not unreal.EditorAssetLibrary.save_loaded_asset(asset,False): raise RuntimeError("Save failed "+asset.get_path_name())
    report["assets"].append(asset.get_path_name())
def clone(source,path):
    asset=unreal.load_asset(path)
    if asset: backup(path); return asset
    return unreal.EditorAssetLibrary.duplicate_asset(source,path)
def entry(path):
    row=unreal.DTCombatAnimation(); row.set_editor_property("montage",unreal.load_asset(path)); return row
try:
    anim="/Game/Art/3D/Player/Animations/Anims/AnimBP_RPGAICharacter"
    backup(anim)
    asset=unreal.load_asset(anim)
    unreal.BlueprintEditorLibrary.reparent_blueprint(asset,unreal.RPGAnimInstance)
    for name,kind in (("bIsBurning","bool"),("GroundSpeed","real")):
        repair.add_member_variable_exact(anim,name,kind,"")
    repair.regenerate_blueprint_class(anim)
    repair.repair_enemy_anim_references(anim)
    repair.assign_anim_skeleton(anim,"/Game/Characters/Mannequin_UE4/Meshes/SK_Mannequin_Skeleton")
    repair.refresh_blueprint_nodes(anim)
    repair.rebind_variable_ref_parents(anim)
    repair.refresh_blueprint_nodes(anim)
    if not repair.force_compile_and_save(anim): raise RuntimeError("Enemy AnimBP compile failed")
    repair.dump_blueprint_graph(anim)
    cast="/Game/Art/3D/Player/Animations/Anims/Retargeted/Standing_1H_Cast_Spell_01_mixamo_com_Montage"
    slash="/Game/Art/3D/Player/Animations/Anims/Retargeted/Sword_And_Shield_Slash_mixamo_com_Montage"
    for path in (cast,slash):
        backup(path); repair.repair_duplicate_montage_slots(path,True)
    repair.add_montage_gameplay_event_notify(cast,"Event.Animation.SpellRelease",1.25)
    melee=clone("/Game/Data/Abilities/DA_AI_BasicAttack","/Game/Data/Abilities/Enemies/DA_CombatTest_Slash")
    for name,value in {"ability_id":"AI.CombatTest.Slash","range":125.0,"cooldown":2.5,"base_damage_by_rank":[8.0],"attack_power_scale":0.0,"stamina_cost":0.0}.items(): melee.set_editor_property(name,value)
    save(melee)
    fire=clone("/Game/Data/Abilities/Player/DA_Player_FireBolt","/Game/Data/Abilities/Enemies/DA_CombatTest_FireBolt")
    for name,value in {"ability_id":"AI.CombatTest.FireBolt","range":1000.0,"cooldown":5.0,"base_damage_by_rank":[8.0],"mana_cost":6.0,"attack_power_scale":0.0,"ability_power_scale":0.0,"requires_target":True}.items(): fire.set_editor_property(name,value)
    save(fire)
    pickup=bp("/Game/Blueprints/Gameplay/Spawning/BP_CombatTest_Item",unreal.RPGPickupItem)
    unreal.get_default_object(pickup.generated_class()).set_editor_property("item_class",unreal.load_class(None,"/Game/Data/Items/BP_DT_StarterSword.BP_DT_StarterSword_C"))
    save(pickup)
    controller=bp("/Game/Blueprints/AI/BP_CombatTest_Controller",unreal.load_class(None,"/Game/Blueprints/AI/BP_RPGAIController.BP_RPGAIController_C"))
    config=unreal.get_default_object(controller.generated_class())
    config.set_editor_property("combat_abilities",[melee,fire]); config.set_editor_property("target_players_only",True)
    save(controller)
    enemy=bp("/Game/Blueprints/AI/BP_CombatTest_Enemy",unreal.load_class(None,"/Game/Blueprints/AI/BP_RPGAICharacter.BP_RPGAICharacter_C"))
    cdo=unreal.get_default_object(enemy.generated_class())
    cdo.set_editor_property("faction_id","Enemy")
    cdo.set_editor_property("ai_controller_class",controller.generated_class())
    cdo.set_editor_property("auto_possess_ai",unreal.AutoPossessAI.PLACED_IN_WORLD_OR_SPAWNED)
    cdo.set_editor_property("starting_equipment",[unreal.load_class(None,"/Game/Data/Items/BP_DT_StarterSword.BP_DT_StarterSword_C")])
    cdo.set_editor_property("loot_pickup_class",pickup.generated_class())
    cdo.set_editor_property("loot_item_class",unreal.load_class(None,"/Game/Data/Items/BP_DT_StarterSword.BP_DT_StarterSword_C"))
    mesh=cdo.get_editor_property("mesh")
    mesh.set_skeletal_mesh_asset(unreal.load_asset("/Game/Art/3D/Ai/Enemy/Elf_Melee_UE4"))
    mesh.set_anim_instance_class(asset.generated_class())
    mesh.set_editor_property("relative_location",unreal.Vector(0,0,-88))
    mesh.set_editor_property("relative_rotation",unreal.Rotator(pitch=0,yaw=-90,roll=0))
    mesh.set_editor_property("visibility_based_anim_tick_option",unreal.VisibilityBasedAnimTickOption.ALWAYS_TICK_POSE_AND_REFRESH_BONES)
    movement=cdo.get_editor_property("character_movement")
    movement.set_editor_property("max_walk_speed",250.0)
    movement.set_editor_property("use_controller_desired_rotation",True)
    movement.set_editor_property("orient_rotation_to_movement",False)
    router=cdo.get_editor_property("animation_router")
    router.set_editor_property("combat_animations",{"AI.CombatTest.Slash":entry(slash),"AI.CombatTest.FireBolt":entry(cast)})
    save(enemy)
    for label,spawned,extent,half,cap,interval in (
        ("Enemy",enemy.generated_class(),unreal.Vector(150,150,150),unreal.Vector(42,42,88),1,20.0),
        ("Item",pickup.generated_class(),unreal.Vector(100,100,100),unreal.Vector(18,18,18),2,10.0)):
        preset=bp("/Game/Blueprints/Gameplay/Spawning/BP_CombatTest_"+label+"Spawner",unreal.RPGSpawnArea)
        unreal.BlueprintEditorLibrary.reparent_blueprint(preset,unreal.RPGSpawnArea)
        unreal.BlueprintEditorLibrary.compile_blueprint(preset)
        config=unreal.get_default_object(preset.generated_class())
        weight=unreal.RPGWeightedSpawn(); weight.set_editor_property("actor_class",spawned)
        config.set_editor_property("spawn_pool",[weight])
        config.set_editor_property("area_id","CombatTest_"+label+"Spawner")
        config.set_editor_property("location_id","CombatTest")
        config.set_editor_property("maximum_alive",cap); config.set_editor_property("spawn_interval",interval)
        config.set_editor_property("placement_half_extent",half)
        config.get_editor_property("area").set_box_extent(extent)
        save(preset)
    mode=bp("/Game/Blueprints/Gameplay/Spawning/BP_CombatTest_GameMode",unreal.load_class(None,"/Game/Blueprints/Framework/BP_RPGGameModeBase.BP_RPGGameModeBase_C"))
    defaults=unreal.get_default_object(mode.generated_class())
    defaults.set_editor_property("default_pawn_class",unreal.load_class(None,"/Game/Blueprints/Characters/BP_PlayerCharacter.BP_PlayerCharacter_C"))
    defaults.set_editor_property("player_controller_class",unreal.load_class(None,"/Game/Blueprints/Framework/BP_PlayerController.BP_PlayerController_C"))
    defaults.set_editor_property("player_state_class",unreal.load_class(None,"/Game/Blueprints/Framework/BP_PlayerState.BP_PlayerState_C"))
    save(mode)
    editor=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    if unreal.EditorAssetLibrary.does_asset_exist("/Game/Levels/CombatTest"):
        unreal.EditorLoadingAndSavingUtils.load_map("/Game/Levels/CombatTest")
    else:
        if not editor.new_level("/Game/Levels/CombatTest"): raise RuntimeError("Could not create CombatTest level")
    actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    world.get_world_settings().set_editor_property("default_game_mode",mode.generated_class())
    existing={a.get_actor_label():a for a in actors.get_all_level_actors()}
    def place(label,cls,location):
        if label in existing: return existing[label]
        actor=actors.spawn_actor_from_class(cls,location); actor.set_actor_label(label); return actor
    floor=place("CombatTest_Floor",unreal.StaticMeshActor,unreal.Vector(0,0,-50))
    floor.static_mesh_component.set_static_mesh(unreal.load_asset("/Engine/BasicShapes/Cube"))
    floor.set_actor_scale3d(unreal.Vector(40,40,1))
    place("CombatTest_PlayerStart",unreal.PlayerStart,unreal.Vector(-600,0,100))
    light=place("CombatTest_Sun",unreal.DirectionalLight,unreal.Vector(0,0,600))
    light.set_actor_rotation(unreal.Rotator(-45,-20,0),False)
    place("CombatTest_Sky",unreal.SkyLight,unreal.Vector(0,0,500))
    for label,cls,location,extent,half,cap,interval in (
        ("CombatTest_EnemySpawner",enemy.generated_class(),unreal.Vector(300,0,90),unreal.Vector(150,150,150),unreal.Vector(42,42,88),1,20.0),
        ("CombatTest_ItemSpawner",pickup.generated_class(),unreal.Vector(-400,250,40),unreal.Vector(100,100,100),unreal.Vector(18,18,18),2,10.0)):
        preset_class=unreal.load_class(None,"/Game/Blueprints/Gameplay/Spawning/BP_"+label+".BP_"+label+"_C")
        if label in existing and existing[label].get_class()!=preset_class:
            actors.destroy_actor(existing.pop(label))
        actor=place(label,preset_class,location)
        actor.set_editor_property("area_id",label); actor.set_editor_property("location_id","CombatTest")
        weight=unreal.RPGWeightedSpawn(); weight.set_editor_property("actor_class",cls)
        actor.set_editor_property("spawn_pool",[weight]); actor.set_editor_property("maximum_alive",cap)
        actor.set_editor_property("spawn_interval",interval); actor.set_editor_property("placement_half_extent",half)
        actor.get_editor_property("area").set_box_extent(extent)
    if not editor.save_current_level(): raise RuntimeError("Level save failed")
    report["saved_spawners"]=[{"class":a.get_class().get_path_name(),"area":str(a.get_editor_property("area_id")),"cap":a.get_editor_property("maximum_alive"),"pool":str(a.get_editor_property("spawn_pool"))} for a in actors.get_all_level_actors() if isinstance(a,unreal.RPGSpawnArea)]
    if any(a["area"]=="None" or a["pool"]=="[]" for a in report["saved_spawners"]): raise RuntimeError("Spawner configuration did not survive level save")
    report["level"]="/Game/Levels/CombatTest"
    report["passed"]=True
except Exception:
    import traceback
    report["errors"].append(traceback.format_exc()); unreal.log_error(report["errors"][-1])
finally:
    (root/"Saved/Automation/CombatTestSetup.json").write_text(json.dumps(report,indent=2),encoding="utf-8")
    unreal.log("COMBAT_TEST_SETUP "+json.dumps(report))
