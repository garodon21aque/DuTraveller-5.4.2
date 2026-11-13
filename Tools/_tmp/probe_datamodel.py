import unreal
seq=unreal.load_asset("/Game/DTIntegration/Animation/Avoidance/DT_Stand_To_Roll_inplace_mixamo_com_UEFN")
unreal.log("SEQ attrs: "+str([m for m in dir(seq) if 'data' in m.lower() or 'model' in m.lower() or 'controller' in m.lower()]))
for cls in ('AnimationDataModel','AnimationDataController','AnimationDataLibrary','AnimationDataModelLibrary'):
    c=getattr(unreal,cls,None)
    if c: unreal.log(cls+": "+str([m for m in dir(c) if not m.startswith('_')]))
unreal.SystemLibrary.quit_editor()
