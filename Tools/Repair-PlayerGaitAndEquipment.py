import shutil
from pathlib import Path
import unreal

root=Path(unreal.Paths.project_dir())
for name in ("Blueprints/Characters/BP_PlayerCharacter", "Blueprints/Framework/BP_PlayerState", "Data/Abilities/Player/DA_Player_BasicSlash"):
    src=root/("Content/"+name+".uasset")
    backup=root/"Saved/Backups/PlayerGaitAndEquipment"/src.relative_to(root)
    backup.parent.mkdir(parents=True,exist_ok=True)
    if not backup.exists(): shutil.copy2(src,backup)
if not unreal.DTStructRepairLibrary.repair_player_sprint_gait(): raise RuntimeError("Sprint gait repair failed")
state=unreal.load_asset("/Game/Blueprints/Framework/BP_PlayerState")
skills=unreal.get_default_object(state.generated_class()).get_editor_property("skills")
slots=list(skills.get_editor_property("starter_hotbar"))
fire=unreal.load_asset("/Game/Data/Abilities/Player/DA_Player_FireBolt")
fire_entry=next((s for s in slots if s.get_editor_property("definition")==fire),None)
if fire_entry is None: raise RuntimeError("Fireball missing from starter skills")
oldslot=fire_entry.get_editor_property("slot")
for slot in slots:
    if slot is not fire_entry and slot.get_editor_property("slot")==2: slot.set_editor_property("slot",oldslot)
fire_entry.set_editor_property("slot",2)
skills.set_editor_property("starter_hotbar",slots)
if not unreal.DTStructRepairLibrary.force_compile_and_save("/Game/Blueprints/Framework/BP_PlayerState"): raise RuntimeError("Starter hotbar save failed")
for script in ("Apply-HotbarEquipment.py","Audit-MeleeAndGait.py"):
    path=root/"Tools"/script
    exec(compile(path.read_text(encoding="utf-8"),str(path),"exec"),{"__file__":str(path)})
