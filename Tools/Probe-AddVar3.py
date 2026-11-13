import unreal,sys
lib=unreal.BlueprintEditorLibrary
bp=unreal.load_asset('/Game/Blueprints/Quest/TT_TestBP2')
gc=bp.generated_class()
cdo=gc.get_default_object()
print('RESULT TestName attr:',getattr(cdo,'test_name','ABSENT'))
print('RESULT attrs with test:',[a for a in dir(cdo) if 'test' in a.lower()])
sys.stdout.flush()
unreal.SystemLibrary.quit_editor()
