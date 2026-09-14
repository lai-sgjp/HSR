"""Reuse the existing locomotion AnimBP on the remaining two character skeletons."""
import unreal,json
tools=unreal.AssetToolsHelpers.get_asset_tools();lib=unreal.EditorAssetLibrary
source=unreal.load_asset('/Game/Assets/mmd/Character/Velina/Meshes/SK_维琳娜6')
source_rig=unreal.load_asset('/Game/Assets/mmd/Character/Velina/Rigs/IK_维琳娜6_VrmHumanoid')
source_bp='/Game/Assets/mmd/Character/Velina/Animations/ABP_Velina_Unarmed'
report=[]
for name,folder,stem in [('Remiel','Ramiel','蕾米埃尔·黑'),('EvernightMoon','Evernight','星穹铁道—长夜月2')]:
 target=unreal.load_asset('/Game/Assets/mmd/Character/'+folder+'/Meshes/SK_'+stem)
 rig=unreal.load_asset('/Game/Assets/mmd/Character/'+folder+'/Rigs/IK_'+stem+'_VrmHumanoid')
 asset_path='/Game/Presentation/Animation/RTG_'+name
 retargeter=unreal.load_asset(asset_path)
 if not retargeter:retargeter=tools.create_asset('RTG_'+name,'/Game/Presentation/Animation',unreal.IKRetargeter,unreal.IKRetargetFactory())
 controller=unreal.IKRetargeterController.get_controller(retargeter)
 controller.set_ik_rig(unreal.RetargetSourceOrTarget.SOURCE,source_rig)
 controller.set_ik_rig(unreal.RetargetSourceOrTarget.TARGET,rig)
 controller.auto_map_chains(unreal.AutoMapChainType.EXACT,True)
 lib.save_loaded_asset(retargeter)
 generated=unreal.IKRetargetBatchOperation.duplicate_and_retarget([lib.find_asset_data(source_bp)],source,target,retargeter,'Velina',name,'PR_','',True)
 for data in generated:
  asset=data.get_asset();lib.save_loaded_asset(asset)
  if isinstance(asset,unreal.AnimBlueprint):
   destination='/Game/Presentation/Animation/'+asset.get_name()
   if asset.get_path_name().split('.')[0]!=destination:lib.rename_asset(asset.get_path_name(),destination)
   definition=unreal.load_asset('/Game/Data/VerticalSlice/Characters/DA_Character_'+name)
   definition.set_editor_property('animation_class',asset.generated_class());lib.save_loaded_asset(definition)
 report.append({'character':name,'assets':len(generated)})
print(json.dumps({'success':True,'retargeted':report}))
