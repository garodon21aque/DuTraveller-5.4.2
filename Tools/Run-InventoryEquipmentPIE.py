"""Exercise real inventory widget drag/drop handlers in rendered Tutorial PIE."""
import json
import time
import traceback
from pathlib import Path
import unreal

unreal.EditorPythonScripting.set_keep_python_script_alive(True)
root=Path(unreal.Paths.project_dir())
output=root/"Saved/Automation/InventoryEquipmentPIE.json"
report={"passed":False,"steps":[],"checks":[],"errors":[]}
started=time.monotonic()
worlds=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
editor=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
probe=unreal.DTStructRepairLibrary
controller=pawn=panel=sword=first=second=None
phase=-1
next_time=0
callback=None
baseline={}

def save(): output.write_text(json.dumps(report,indent=2,default=str),encoding="utf-8")
def check(name,ok): report["checks"].append({"name":name,"passed":bool(ok)}); save()
def value(name): return probe.read_runtime_attribute(pawn,name)
def handles_inventory(widget):
    for name in ("bHandleInventoryInput","handle_inventory_input","b_handle_inventory_input"):
        try: return bool(widget.get_editor_property(name))
        except Exception: pass
    return False
def close():
    if callback is not None: unreal.unregister_slate_post_tick_callback(callback)
    save()
    unreal.EditorPythonScripting.set_keep_python_script_alive(False)

def snapshot(label,handled=None):
    world=worlds.get_game_world()
    entries=[]
    grid=panel.get_editor_property("InventoryWB")
    for widget in grid.get_all_children():
        if not isinstance(widget,unreal.RPG_ItemWidget): continue
        item=widget.get_editor_property("item")
        if not item or item.get_quantity()<=0 or not handles_inventory(widget): continue
        row={"name":str(item.get_editor_property("item_display_name")),"equipped":item.is_equipped() if isinstance(item,unreal.Equipment_ItemObject) else False}
        try:
            tooltip=widget.get_editor_property("tool_tip_widget")
            row["tooltip_details"]=str(tooltip.get_editor_property("details_text")) if tooltip else None
            row["tooltip_description"]=str(tooltip.get_editor_property("DescriptionTxT").get_editor_property("text")) if tooltip else None
        except Exception as exc: row["tooltip_probe_error"]=str(exc)
        entries.append(row)
    slots=[]
    for widget in unreal.WidgetLibrary.get_all_widgets_of_class(world,unreal.DTEquipmentSlotWidget,False):
        gear=widget.get_editor_property("equipped_item")
        slots.append({"widget":widget.get_name(),"slot":str(widget.get_editor_property("equipment_slot")),"item":str(gear.get_editor_property("item_display_name")) if gear else None})
    row={"label":label,"handled":handled,"Strength":value("Strength"),"AttackPower":value("AttackPower"),"MaxMana":value("MaxMana"),"Armor":value("Armor"),"bag_items":entries,"equipment_slots":slots}
    report["steps"].append(row)
    unreal.log("INVENTORY_PIE "+json.dumps(row,default=str))
    save()

def drop(item,target): return unreal.DTInventoryDragDrop.drive_inventory_drop(controller,item,target)

def tick(delta):
    global controller,pawn,panel,sword,first,second,phase,next_time,baseline
    try:
        if time.monotonic()-started>240: raise RuntimeError("PIE trace time limit")
        world=worlds.get_game_world()
        if not world: return
        now=unreal.GameplayStatics.get_time_seconds(world)
        if controller is None:
            controller=unreal.GameplayStatics.get_player_controller(world,0)
            pawn=controller.get_controlled_pawn() if controller else None
            if not pawn: controller=None; return
            if not controller.open_ui_panel("Inventory"): raise RuntimeError("Inventory panel did not open")
            panels=controller.get_editor_property("panels")
            panel=next(v for k,v in panels.items() if str(k)=="Inventory")
            next_time=now+2
            return
        if now<next_time: return
        next_time=now+1
        phase+=1
        inv=pawn.get_editor_property("inventory_comp")
        if phase==0:
            items=list(inv.get_editor_property("items"))
            sword=next(i for i in items if isinstance(i,unreal.Equipment_ItemObject) and i.get_editor_property("slot")==unreal.EquipableSlot.EIS_PRIMARY_WEAPON and i.is_equipped())
            first=unreal.DTInventoryDragDrop.create_verification_gear(controller,unreal.Text("PIE helmet A"),unreal.EquipableSlot.EIS_HELMET,{"Strength":3.0,"MaxMana":20.0})
            second=unreal.DTInventoryDragDrop.create_verification_gear(controller,unreal.Text("PIE helmet B"),unreal.EquipableSlot.EIS_HELMET,{"Strength":7.0,"Armor":10.0})
            if not first or not second: raise RuntimeError("Test gear could not be created")
            report["initial_bag_weapons"]=[i.get_path_name() for i in items if isinstance(i,unreal.Equipment_ItemObject) and i.get_editor_property("slot")==unreal.EquipableSlot.EIS_PRIMARY_WEAPON and not i.is_equipped()]
            panel.show_all_items()
            baseline={n:value(n) for n in ("Strength","MaxMana","Armor","AttackPower")}
            snapshot("initial")
        elif phase==1:
            handled=drop(sword,"Inventory"); snapshot("sword_to_bag",handled)
            check("equipped sword drags back to bag",handled and not sword.is_equipped())
            check("sword GAS bonus removed",abs(value("AttackPower")-(baseline["AttackPower"]-5))<0.01)
        elif phase==2:
            handled=drop(sword,"Character"); snapshot("sword_to_character",handled)
            check("character preview drop equips sword",handled and sword.is_equipped())
            check("character drop restores GAS bonus",abs(value("AttackPower")-baseline["AttackPower"])<0.01)
        elif phase==3:
            handled=drop(sword,"Character"); snapshot("repeat_character_drop",handled)
            check("repeated drop does not stack GAS stats",abs(value("AttackPower")-baseline["AttackPower"])<0.01)
            drop(sword,"Inventory")
        elif phase==4:
            handled=drop(sword,"WeaponSlot"); snapshot("matching_weapon_slot",handled)
            check("matching equipment slot accepts sword",handled and sword.is_equipped())
        elif phase==5:
            handled=drop(first,"WeaponSlot"); snapshot("wrong_equipment_slot",handled)
            check("wrong equipment slot leaves gear/stats unchanged",not first.is_equipped() and abs(value("Strength")-baseline["Strength"])<0.01)
        elif phase==6:
            handled=drop(first,"HelmetSlot"); snapshot("helmet_A_equipped",handled)
            check("helmet applies GAS primary and maximum bonuses",handled and first.is_equipped() and abs(value("Strength")-baseline["Strength"]-3)<0.01 and abs(value("MaxMana")-baseline["MaxMana"]-20)<0.01)
        elif phase==7:
            handled=drop(second,"HelmetSlot"); snapshot("helmet_B_replaces_A",handled)
            check("same-slot swap replaces bonuses",handled and second.is_equipped() and not first.is_equipped() and abs(value("Strength")-baseline["Strength"]-7)<0.01 and abs(value("Armor")-baseline["Armor"]-10)<0.01 and abs(value("MaxMana")-baseline["MaxMana"])<0.01)
        elif phase==8:
            handled=drop(second,"Inventory"); snapshot("helmet_returned_to_bag",handled)
            check("unequip restores original GAS attributes",handled and not second.is_equipped() and all(abs(value(n)-baseline[n])<0.01 for n in ("Strength","MaxMana","Armor")))
            panel.show_equipment()
        elif phase==9:
            snapshot("equipment_category")
            names=[r["name"] for r in report["steps"][-1]["bag_items"]]
            check("equipment tab shows both unequipped helmets",all(n in names for n in ("PIE helmet A","PIE helmet B")) and all("Sword" not in n.title() for n in names))
            descriptions=[r.get("tooltip_description","") for r in report["steps"][-1]["bag_items"]]
            check("rendered tooltip text contains actual GAS bonuses",all("Strength" in text for text in descriptions) and any("Max Mana" in text for text in descriptions))
            panel.show_all_items(); drop(sword,"Inventory"); panel.show_weapons()
        elif phase==10:
            snapshot("weapons_category")
            check("weapons category contains weapons and excludes helmets",bool(report["steps"][-1]["bag_items"]) and all("Sword" in r["name"].title() for r in report["steps"][-1]["bag_items"]))
            panel.show_all_items(); drop(sword,"Character")
            inv.remove_item(first); inv.remove_item(second)
            controller.close_ui_panels()
            report["passed"]=all(c["passed"] for c in report["checks"])
            close()
    except Exception:
        report["errors"].append(traceback.format_exc()); unreal.log_error(report["errors"][-1]); close()

try:
    world=worlds.get_editor_world()
    if not world or world.get_name()!="Tutorial": unreal.EditorLoadingAndSavingUtils.load_map("/Game/Levels/Tutorial")
    callback=unreal.register_slate_post_tick_callback(tick)
    editor.editor_request_begin_play()
except Exception:
    report["errors"].append(traceback.format_exc()); close()
