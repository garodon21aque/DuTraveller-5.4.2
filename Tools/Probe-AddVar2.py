import unreal
lib=unreal.BlueprintEditorLibrary
f=unreal.AssetToolsHelpers.get_asset_tools()
fac=unreal.BlueprintFactory()
fac.set_editor_property('parent_class',unreal.Actor)
bp=f.create_asset('TT_TestBP2','/Game/Blueprints/Quest',unreal.Blueprint,fac)
t=unreal.EdGraphPinType(); t.import_text('(PinCategory="name",PinSubCategory="",PinSubCategoryObject=None,PinSubCategoryMemberReference=(MemberParent=None,MemberName="",MemberGuid=00000000000000000000000000000000),PinValueType=(TerminalCategory="",TerminalSubCategory="",TerminalSubCategoryObject=None,bTerminalIsConst=False,bTerminalIsWeakPointer=False,bTerminalIsUObjectWrapper=False),ContainerType=None,bIsReference=False,bIsConst=False,bIsWeakPointer=False,bIsUObjectWrapper=False,bSerializeAsSinglePrecisionFloat=False)')
print('add:',lib.add_member_variable(bp,'TestName',t))
# inspect NewVariables desc
try:
    nv=bp.get_editor_property('new_variables')
    print('new_variables:',[(v.get_editor_property('var_name'),v.get_editor_property('var_guid')) for v in nv])
except Exception as e:
    print('nv err:',e)
lib.compile_blueprint(bp)
unreal.EditorAssetLibrary.save_loaded_asset(bp, only_if_is_dirty=False)
bp2=unreal.load_asset('/Game/Blueprints/Quest/TT_TestBP2')
gc=bp2.generated_class()
try:
    gc.get_default_object().get_editor_property('TestName'); print('prop OK')
except Exception as e: print('prop missing post-save')
# does the pin type import hold?
try:
    t2=lib.get_member_variable_type(bp2,'TestName')
    print('vartype:',t2.export_text()[:120])
except Exception as e: print('vt err',e)
unreal.SystemLibrary.quit_editor()
