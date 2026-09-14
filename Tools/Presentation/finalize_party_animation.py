import unreal,json
lib=unreal.EditorAssetLibrary
out=[]
for path in lib.list_assets('/Game',recursive=False,include_folder=False):
 name=path.split('/')[-1].split('.')[0]
 if not name.startswith('PR_'):continue
 asset=unreal.load_asset(path)
 if not isinstance(asset,(unreal.AnimationAsset,unreal.AnimBlueprint)):continue
 lib.save_loaded_asset(asset)
 lib.rename_asset(path,'/Game/Presentation/Animation/'+name)
 out.append(name)
for name in ['Remiel','EvernightMoon']:
 bp=unreal.load_asset('/Game/Presentation/Animation/PR_ABP_'+name+'_Unarmed')
 if not bp:raise RuntimeError('Missing retargeted AnimBP '+name)
 unreal.BlueprintEditorLibrary.compile_blueprint(bp);lib.save_loaded_asset(bp)
 definition=unreal.load_asset('/Game/Data/VerticalSlice/Characters/DA_Character_'+name)
 definition.set_editor_property('animation_class',bp.generated_class());lib.save_loaded_asset(definition)
lib.save_directory('/Game/Presentation/Animation',False,True)
print(json.dumps({'success':True,'moved':len(out)}))
