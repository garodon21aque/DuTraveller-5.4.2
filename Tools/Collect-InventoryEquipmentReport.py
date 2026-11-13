import json
from pathlib import Path

root=Path(__file__).resolve().parents[1]
saved=root/"Saved/Automation"
read=lambda path:json.loads(path.read_text(encoding="utf-8-sig"))
pie=read(saved/"InventoryEquipmentPIE.json")
testfile=saved/"InventoryEquipmentRegression/index.json"
tests=read(testfile) if testfile.exists() else {}
report={
 "project":str(root/"DT.uproject"),
 "implemented":[
  "Drag owned gear onto the character preview or equipment panel to equip to its authored slot.",
  "Drop onto a specific equipment slot with strict slot validation; rejected drops cannot accidentally unequip.",
  "Drag equipped items back into the bag to unequip; right-click supports equip/use and slot unequip.",
  "Equipment icons, bag filtering, item details and the nested GAS attribute menu follow the shared character's equipment state.",
  "Equipment bonuses replace the active GAS equipment effect; permanent attribute bases remain separate.",
  "Existing drag-operation/payload Blueprint types and source-widget references remain compatible with legacy drop confirmation.",
  "Client equipment lookups resolve replicated item identities against current inventory instances, including late-arriving inventory updates."
 ],
 "blueprint_apply":read(saved/"InventoryEquipmentApply.json"),
 "pie_validation":pie,
 "automation":{key:tests.get(key) for key in ("succeeded","succeededWithWarnings","failed","notRun","totalDuration")},
 "build":"DTEditor Win64 Development succeeded with -DisablePlugin=Fab; the updated engine FabToolset installation otherwise triggers a build-rule error. No project plugin settings were changed.",
 "modified_source_files":[str(root/"Source/DuTraveller"/name) for name in (
  "DTInventoryDragDrop.h","DTInventoryDragDrop.cpp","DTEquipmentWidget.h","DTEquipmentWidget.cpp",
  "DTPlayerInventoryWidget.h","DTPlayerInventoryWidget.cpp","Equipment_ItemObject.h","Equipment_ItemObject.cpp",
  "ItemObject.h","ItemObject.cpp","RPGCharacter.cpp","RPG_ItemWidget.h","RPG_ItemWidget.cpp","RPG_ItemToolTip.cpp",
  "Tests/RestorationFoundationTests.cpp")],
 "saved_widget_assets":[str(root/"Content/UI/InventorySystem"/name) for name in (
  "WBP_EquipmentSlot.uasset","WBP_Equipment.uasset","WBP_PlayerInventory.uasset","WBP_ItemObject.uasset",
  "WBP_InvItemTooltip.uasset","DragDrop/BP_ItemDragDropOperation.uasset","DragDrop/BP_EquipmentDragDropOperation.uasset")],
 "backups":str(root/"Saved/Backups/InventoryEquipment"),
 "legacy_reference":str(Path("D:/GameJams/Github/DuTraveller")),
 "legacy_recovery_limit":"The old BP_BaseEquipment_ItemObject_Sword references /Game/Blueprints/InventorySystem/Items/BP_BaseEquipment_ItemObject, which is absent from that D: copy. Its original inherited balance data could not be recovered. Existing C: item AttributeBonuses were preserved; test-helmet bonuses were transient and removed after PIE.",
 "validation_limits":[
  "PIE checks invoke the actual native widget mouse/drag/drop handlers against laid-out rendered widgets; a manual mouse gesture and visual layout pass were not performed.",
  "Network UI replication and the complete world-drop confirmation interaction were not exercised in this PIE run.",
  "NPC mesh/animation repairs described in the user's separate status update were left untouched."
 ]
}
output=saved/"InventoryEquipmentReport.json"
output.write_text(json.dumps(report,indent=2),encoding="utf-8")
print(str(output))
