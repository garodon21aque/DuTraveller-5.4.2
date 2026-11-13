"""Summarize measured PIE evidence without treating unmeasured checks as passes."""
import json
from pathlib import Path

root=Path(__file__).resolve().parents[1]
trace=max((root/"Saved/Automation").glob("LivePIETrace*.json"),key=lambda p:p.stat().st_mtime)
events=json.loads(trace.read_text(encoding="utf-8"))
samples=[e for e in events if e["kind"]=="sample"]
report={"trace_file":str(trace),"finished":events[-1],"identity":next((e for e in events if e["kind"]=="runtime_identity"),None),"phases":{}}
report["runtime_characters"]=next((e.get("characters",[]) for e in events if e["kind"]=="runtime_characters"),[])
for phase in dict.fromkeys(e["label"] for e in samples):
    rows=[e["state"] for e in samples if e["label"]==phase]
    report["phases"][phase]={
        "samples":len(rows),
        "speed_min_max":[min(float(r["speed"]) for r in rows),max(float(r["speed"]) for r in rows)],
        "gaits":sorted(set(r.get("anim.Gait","") for r in rows)),
        "falling":sorted(set(r["falling"] for r in rows)),
        "mana":sorted(set(float(r["Mana"]) for r in rows)),
        "npc_health":sorted(set(r["test_npc_health"] for r in rows if "test_npc_health" in r)),
        "montages":sorted(set(r.get("current_montage","") for r in rows)),
        "montage_positions":[r.get("montage_position") for r in rows if r.get("montage_position")],
        "hand_poses":len(set(r["hand_r"] for r in rows)),
        "foot_poses":len(set(r["foot_l"] for r in rows)),
        "mana_widgets":[r.get("mana_widgets",[]) for r in rows],
        "projectiles":[r["projectile_details"] for r in rows if r.get("projectile_details")],
        "pawn_movement":[r.get("pawn_movement",{}) for r in rows],
    }
report["legacy_mana_matches_gas"]=bool(samples) and all(abs(float(e["state"]["Mana"])-e["state"]["hud_mirror_mana"])<0.001 for e in samples)
widgets=[(e["state"],w) for e in samples for w in e["state"].get("mana_widgets",[])]
report["live_mana_globe_matches_gas"]=bool(widgets) and all("percent" in w and abs(w["percent"]-float(s["Mana"])/float(s["MaxMana"]))<0.001 for s,w in widgets)
output=root/"Saved/Automation/LivePIEVerificationSummary.json"
output.write_text(json.dumps(report,indent=2),encoding="utf-8")
print(json.dumps({k:v for k,v in report.items() if k!="phases"},indent=2))
