import unreal
seq=unreal.load_asset("/Game/DTIntegration/Animation/Avoidance/DT_Stand_To_Roll_inplace_mixamo_com_UEFN")
ctrl=seq.controller
model=seq.data_model or ctrl.get_model_interface()
unreal.log("NKEYS="+str(model.get_number_of_keys()))
t=model.get_bone_track_by_index(0)
unreal.log("T0_NAME="+str(t.name)+" TREE="+str(t.bone_tree_index))
inner=t.internal_track_data
pos=list(inner.get_positional_keys()); rot=list(inner.get_rotational_keys()); sc=list(inner.get_scale_keys())
unreal.log("KEYS pos="+str(len(pos))+" rot="+str(len(rot))+" sc="+str(len(sc)))
if pos: unreal.log("POS0="+str(pos[0])+" POS_MID="+str(pos[len(pos)//2]))
try:
    p0=unreal.AnimationLibrary.get_bone_pose_for_frame(seq,"root",0,False)
    p1=unreal.AnimationLibrary.get_bone_pose_for_frame(seq,"root",model.get_number_of_frames()//2,False)
    unreal.log("POSE f0="+str(p0)+" fmid="+str(p1))
except Exception as e: unreal.log("POSE_ERR="+str(e))
unreal.SystemLibrary.quit_editor()
