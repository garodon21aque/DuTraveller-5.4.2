import unreal
seq=unreal.load_asset("/Game/DTIntegration/Animation/Avoidance/DT_Stand_To_Roll_inplace_mixamo_com_UEFN")
model=seq.data_model or seq.controller.get_model_interface()
names=model.get_bone_track_names()
unreal.log("TRACKS: "+str(names))
unreal.log("FRAMES: "+str(model.get_number_of_frames()))
for name in list(names)[:6]:
    idx=model.get_bone_track_index_by_name(name)
    track=model.get_bone_track_by_index(idx)
    data=getattr(track,"internal_track_data",track)
    unreal.log("  "+str(name)+" idx="+str(idx)+" pos="+str(len(list(data.get_positional_keys())))+" rot="+str(len(list(data.get_rotational_keys()))))
unreal.SystemLibrary.quit_editor()
