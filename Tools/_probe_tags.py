import unreal
import json
from pathlib import Path

names = ["State.Dodging", "State.Invulnerable", "Attributes.Primary.Strength", "Event.Animation.SpellRelease"]
out = {}
try:
    mgr = unreal.GameplayTagsLibrary
except Exception as e:
    mgr = str(e)
for n in names:
    try:
        tag = unreal.GameplayTag(n)
        out[n] = {"requested": str(tag), "valid": bool(tag.is_valid_gameplay_tag()) if hasattr(tag, "is_valid_gameplay_tag") else str(tag)}
    except Exception as e:
        out[n] = "ERR:" + str(e)
try:
    lib = unreal.GameplayTagsLibrary
    for n in names:
        try:
            tag = lib.request_gameplay_tag(n)
            out[n + "_lib"] = str(tag)
        except Exception as e:
            out[n + "_lib"] = "ERR:" + str(e)
except Exception as e:
    out["lib"] = "ERR:" + str(e)

p = Path(unreal.Paths.project_saved_dir()) / "Automation" / "TagProbe.json"
p.parent.mkdir(parents=True, exist_ok=True)
p.write_text(json.dumps(out, indent=2, default=str))
unreal.log("TAGPROBE " + json.dumps(out, default=str))
unreal.SystemLibrary.quit_editor()
