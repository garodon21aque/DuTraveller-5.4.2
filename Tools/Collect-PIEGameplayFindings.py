import json
from pathlib import Path

root=Path(__file__).resolve().parents[1]
saved=root/"Saved/Automation"
read=lambda name:json.loads((saved/name).read_text(encoding="utf-8-sig"))
summary=read("LivePIEVerificationSummary.json")
tests=read("RestorationAfterPIE/index.json")
report={
 "scope":"Tutorial PIE gameplay investigation; no screen viewer or recording replay used",
 "build":"DTEditor Win64 Development succeeded",
 "runtime_evidence":summary,
 "tests":{key:tests.get(key) for key in ("succeeded","succeededWithWarnings","failed","notRun","totalDuration")},
 "test_warnings":[{"test":t.get("testDisplayName"),"entries":t.get("entries")} for t in tests["tests"] if t.get("warnings")],
 "root_causes":[
  "Cast montage contained two DefaultSlot tracks, causing Unreal to reject montage playback.",
  "Four montage-only retarget exports omitted their referenced sequences; montage segments were null.",
  "A release timer could spawn a projectile after failed montage playback, masking the animation failure.",
  "GetDesiredGait full-input sprint branch returned Run; sprint Completed/Canceled pins were unwired.",
  "Active player StartingEquipment was empty, and Sword mesh had no simple collision shape.",
  "Starter hotbar placed FireBolt on RMB rather than key 1; starter slots now assign FireBolt to slot 2 (key 1).",
  "DT motion-matching database selector was unregistered and logged a warning every animation update."
 ],
 "verified_combat":{"fireball":"Native One key event -> hotbar slot 2 -> action activation with GAS mana cost -> casting montage -> Event.Animation.SpellRelease notify -> BP_Projectile_FireBolt -> NPC health 100 to 78", "melee":"Native LeftMouseButton -> melee montage -> notify window -> sword QueryOnly overlap -> GAS damage 19 -> NPC health 78 to 59 -> collision disabled", "mana":"Infinite server regeneration effect, period 1 second, live ManaRegeneration=2; sampled mana 42,44,46,48,50 with MaxMana=50; actual HUD ProgressBar percent matched GAS throughout"},
 "architecture":"Unified RPGCharacter foundation retained. Player uses RPGPlayerCharacter/DT_PlayerCharacter; NPC has Enemy, QuestGiver and Mount branches. Runtime uses the existing RPGActionComponent and GAS attributes/effects; granted UGameplayAbility specs=0, so there is no PlayMontage ability task in this implementation.",
 "modified_source_files":[str(root/p) for p in ["Source/DuTraveller/DTAnimationRouter.h","Source/DuTraveller/DTAnimationRouter.cpp","Source/DuTraveller/RPGActionComponent.cpp","Source/DuTraveller/DTStructRepairLibrary.h","Source/DuTraveller/DTStructRepairLibrary.cpp","Source/DuTraveller/DuTraveller.cpp","Source/DuTraveller/Tests/RestorationFoundationTests.cpp","Tools/Apply-CombatSetup.py"]],
 "repair_and_validation_scripts":[str(root/"Tools"/p) for p in ["Repair-CastMontageTracks.py","Restore-MontageSequences.py","Repair-PlayerGaitAndEquipment.py","Apply-HotbarEquipment.py","Audit-MeleeAndGait.py","Run-LivePIETrace.py","Summarize-LivePIETrace.py","Collect-PIEGameplayFindings.py"]],
 "modified_assets":[str(root/"Content"/p) for p in ["Blueprints/Characters/BP_PlayerCharacter.uasset","Blueprints/Framework/BP_PlayerState.uasset","Art/3D/Weapons/Sword.uasset","Data/Items/BP_DT_StarterSword.uasset","Data/Abilities/Actions/BP_DT_BasicSlashAction.uasset","Data/Abilities/Player/DA_Player_BasicSlash.uasset"]],
 "montage_slot_repair":read("MontageDuplicateSlotRepair.json"),
 "montage_sequence_repairs":read("MontageSequenceRepair.json"),
 "equipment_configuration":read("HotbarEquipmentImplementation.json"),
 "weapon_and_gait_audit":read("MeleeAndGaitAudit.json"),
 "remaining_editor_validation":[
  "BP_QuestGiver and BP_NPC_Trainer in Tutorial had no runtime AnimInstance on the inherited character mesh. Their intended skeletal mesh/AnimBP assignments remain unverified; inspect and configure intended NPC presentation.",
  "Placed BP_RPGCharacter actor uses ABP_UEFN_Mannequin_PostProcess as its main runtime AnimInstance. It is not the possessed DT player. Confirm its intended role, skeletal mesh and compatible main locomotion AnimBP in the level.",
  "Visually review casting/melee poses, sword attachment alignment, idle and turning, jumping/landing, and traversal over actual obstacles; numeric pose/montage evaluation is verified but no screen viewer was used.",
  "Verify multiplayer montage presentation/replication and live target-lock/free-combat switching. Automated TargetLockToggle, MountPossession and hierarchy tests passed; these do not establish full PIE presentation correctness."
 ],
 "backups":str(root/"Saved/Backups")
}
for repair in report["montage_sequence_repairs"]["repairs"]:
    for name in ("montage","sequence"):
        report["modified_assets"].append(str(root/("Content/"+repair[name][len("/Game/"):]+".uasset")))
out=saved/"PIEGameplayInvestigation.json"
out.write_text(json.dumps(report,indent=2),encoding="utf-8")
print(str(out))
