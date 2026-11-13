import unreal
lib=unreal.BlueprintEditorLibrary
f=unreal.AssetToolsHelpers.get_asset_tools()
fac=unreal.BlueprintFactory()
fac.set_editor_property('parent_class',unreal.Actor.static_class() if hasattr(unreal.Actor,'static_class') else unreal.Actor)
bp=f.create_asset('TT_TestBP','/Game/Blueprints/Quest',unreal.Blueprint,fac)
print('bp:',bp)
t=unreal.EdGraphPinType(); t.import_text('(PinCategory="name",PinSubCategory="",PinSubCategoryObject=None,PinSubCategoryMemberReference=(MemberParent=None,MemberName="",MemberGuid=00000000000000000000000000000000),PinValueType=(TerminalCategory="",TerminalSubCategory="",TerminalSubCategoryObject=None,bTerminalIsConst=False,bTerminalIsWeakPointer=False,bTerminalIsUObjectWrapper=False),ContainerType=None,bIsReference=False,bIsConst=False,bIsWeakPointer=False,bIsUObjectWrapper=False,bSerializeAsSinglePrecisionFloat=False)')
print('add:',lib.add_member_variable(bp,'TestName',t))
print('vars:',[v for v in lib.list_member_variable_names(bp)])
r=lib.compile_blueprint(bp)
print('compile:',r)
gc=bp.generated_class()
try:
    gc.get_default_object().get_editor_property('TestName'); print('prop OK')
except Exception as e: print('prop missing')
unreal.SystemLibrary.quit_editor()
