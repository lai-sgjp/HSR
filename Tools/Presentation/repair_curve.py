import unreal,json
tools=unreal.AssetToolsHelpers.get_asset_tools()
path='/Game/Data/VerticalSlice/Characters/Curve_CharacterExperience'
factory=unreal.CurveFactory();factory.set_editor_property('curve_class',unreal.CurveFloat)
curve=unreal.load_asset(path) or tools.create_asset('Curve_CharacterExperience','/Game/Data/VerticalSlice/Characters',unreal.CurveFloat,factory)
assert unreal.MCPythonHelper.set_float_curve_keys(curve,[float(n) for n in range(1,81)],[float((n-1)*n*50) for n in range(1,81)])
unreal.EditorAssetLibrary.save_loaded_asset(curve)
result={}
for name in ['EvernightMoon','Huohua','Remiel','Verina']:
 a=unreal.load_asset('/Game/Data/VerticalSlice/Characters/DA_Character_'+name)
 a.set_editor_property('cumulative_experience_curve',curve)
 unreal.EditorAssetLibrary.save_loaded_asset(a)
 state=unreal.HSRCharacterRuntimeState(character_id=a.get_editor_property('character_id'))
 result[name]=str(unreal.HSRCharacterProgressionLibrary.validate_runtime_state(a,state))
print(json.dumps({'success':True,'validation':result}))
