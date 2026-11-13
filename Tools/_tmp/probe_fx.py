import unreal, json
FX = "/Game/Art/Niagara/"
paths = ["Explode/Fire_Burst","Spells/FIre/Fire_Ball","Spells/Ice/NS_IceShards","Spells/Ice/NE_Spie",
"Spells/Ice/NE_IceWall_System","Spells/ElectricBolt","Spells/Electric/ShockBeam","Casting",
"Explode/Water_Burst","Spells/Water/WaterBolt","Casting_Effect","EnergyBall",
"Spells/Shadow/Shadow_Ball","Explode/Shadow_Burst","Spells/Rasen"]
out = {}
for p in paths:
    a = unreal.load_asset(FX + p)
    out[p] = a.get_class().get_name() if a else "MISSING"
# also list systems under /Game/Art/Niagara for fallback discovery
reg = unreal.AssetRegistryHelpers.get_asset_registry()
systems = []
for data in reg.get_assets_by_path("/Game/Art/Niagara", True):
    if str(data.asset_class_path.asset_name) == "NiagaraSystem":
        systems.append(str(data.package_name))
out["__systems__"] = systems[:80]
unreal.log("FX_PROBE " + json.dumps(out))
open(unreal.Paths.project_saved_dir() + "/Automation/fx_probe.json","w").write(json.dumps(out, indent=1))
