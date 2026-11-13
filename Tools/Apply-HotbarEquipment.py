"""Author editable starter gear and connect the existing retargeted slash.
Existing assets are backed up before this script; no old library assets are saved.
"""
import json
from pathlib import Path
import unreal

tools = unreal.AssetToolsHelpers.get_asset_tools()
report = {'assets': [], 'passed': False, 'limits': ['Attachment pose requires rendered verification', 'Other legacy actions still need migration']}

def save_bp(path):
    if not unreal.DTStructRepairLibrary.force_compile_and_save(path):
        raise RuntimeError('Compile/save failed: ' + path)
    report['assets'].append(path)

def make_bp(path, parent):
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        return unreal.load_asset(path), False
    factory = unreal.BlueprintFactory()
    factory.set_editor_property('parent_class', parent)
    folder, name = path.rsplit('/', 1)
    return tools.create_asset(name, folder, unreal.Blueprint, factory), True

try:
    path = '/Game/Data/Items/BP_DT_StarterSword'
    bp, created = make_bp(path, unreal.Gear_ItemObject)
    if created:
        mesh = unreal.load_asset('/Game/Art/3D/Weapons/Sword')
        if not isinstance(mesh, unreal.StaticMesh):
            raise RuntimeError('Expected original Sword art to be a static mesh')
        cdo = unreal.get_default_object(bp.generated_class())
        for key, value in dict(item_display_name=unreal.Text('Starter sword'),
                               item_description=unreal.Text('A basic sword. Use it to equip or unequip.'),
                               use_action_text=unreal.Text('Equip / Unequip'),
                               slot=unreal.EquipableSlot.EIS_PRIMARY_WEAPON,
                               thumbnail=unreal.load_asset('/Game/UI/2D/Weapons/Sword'),
                               equipped_mesh=mesh, attach_socket='hand_r',
                               attribute_bonuses={'AttackPower': 5.0}).items():
            cdo.set_editor_property(key, value)
        save_bp(path)
    action_path = '/Game/Data/Abilities/Actions/BP_DT_BasicSlashAction'
    action, created = make_bp(action_path, unreal.RPGAction)
    if created:
        unreal.get_default_object(action.generated_class()).set_editor_property('actionname', 'Abilities.Melee.Slash')
        save_bp(action_path)
    character_path = '/Game/Blueprints/Characters/BP_PlayerCharacter'
    character = unreal.get_default_object(unreal.load_asset(character_path).generated_class())
    starting = list(character.get_editor_property('starting_equipment'))
    if bp.generated_class() not in starting:
        starting.append(bp.generated_class())
        character.set_editor_property('starting_equipment', starting)
    ability = unreal.load_asset('/Game/Data/Abilities/Player/DA_Player_BasicSlash')
    ability.set_editor_property('requires_target', False)
    ability.set_editor_property('action_class', action.generated_class())
    router = character.get_editor_property('animation_router')
    mapping = dict(router.get_editor_property('combat_animations'))
    key = ability.get_editor_property('ability_id')
    if key not in mapping:
        entry = unreal.DTCombatAnimation()
        entry.set_editor_property('montage', unreal.load_asset('/Game/DTIntegration/Animation/Combat/DT_Sword_And_Shield_Slash_mixamo_com_Montage_UEFN'))
        mapping[key] = entry
        router.set_editor_property('combat_animations', mapping)
    save_bp(character_path)
    if not unreal.EditorAssetLibrary.save_loaded_asset(ability, False):
        raise RuntimeError('Could not save slash definition')
    report['assets'].append(ability.get_path_name())
    report['passed'] = True
finally:
    (Path(unreal.Paths.project_saved_dir()) / 'Automation/HotbarEquipmentImplementation.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
