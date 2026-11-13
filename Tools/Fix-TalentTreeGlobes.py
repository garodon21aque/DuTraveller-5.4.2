import json, traceback
import unreal
MAPPING = {
 "Fire_Skill3":"Abilities.Fire.MeteorStrike","Fire_Skill2":"Abilities.Fire.FireBlast","Fire_Skill1":"Abilities.Fire.FireBolt",
 "Water_Skill3":"Abilities.Water.WaterStream","Water_Skill2":"Abilities.Water.WaterHeal","Water_Skill1":"Abilities.Water.WaterBolt",
 "Earth_Skill3":"Abilities.Light.HolyShield","Earth_Skill2":"Abilities.Light.LightHeal","Earth_Skill1":"Abilities.Light.LightBolt",
 "Electric_Skill3":"Abilities.Electric.ElectricBlink","Electric_Skill2":"Abilities.Electric.ChainLightning","Electric_Skill1":"Abilities.Electric.ElectricBolt",
 "Ice_Skill3":"Abilities.Ice.IceStorm","Ice_Skill2":"Abilities.Ice.IceWall","Ice_Skill1":"Abilities.Ice.IceShards",
 "Light_Skill3":"Abilities.Earth.Root","Light_Skill2":"Abilities.Earth.EarthQuake","Light_Skill1":"Abilities.Earth.EarthWall",
 "Shadow_Skill3":"Abilities.Shadow.ShadowFear","Shadow_Skill2":"Abilities.Shadow.ShadowShipon","Shadow_Skill1":"Abilities.Shadow.ShadowBolt",
 "Air_Skill2":"Abilities.Air.PushBack","Air_Skill1":"Abilities.Air.AirBolt",
}
PATH = "/Game/UI/SpellMenu/WBP_MageTalentTree"
res = {}
try:
    lib = unreal.DTStructRepairLibrary
    for name, tag in MAPPING.items():
        res[name] = bool(lib.set_designer_widget_gameplay_tag(PATH, name, "AbilityTag", tag))
    unreal.log("TALENT_FIX " + json.dumps(res))
except Exception:
    unreal.log_error(traceback.format_exc())
finally:
    unreal.SystemLibrary.quit_editor()
