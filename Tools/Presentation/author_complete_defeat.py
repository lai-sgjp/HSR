"""Retarget the CC0 complete Death01 clip; keep its final collapsed pose."""
import unreal,json
lib=unreal.EditorAssetLibrary
tools=unreal.AssetToolsHelpers.get_asset_tools()
folder='/Game/Presentation/Animation/CC0'
source=unreal.load_asset(folder+'/QuaterniusDeath')
rig_path=folder+'/IK_Quaternius'
rig=unreal.load_asset(rig_path) if lib.does_asset_exist(rig_path) else tools.create_asset('IK_Quaternius',folder,unreal.IKRigDefinition,unreal.IKRigDefinitionFactory())
c=unreal.IKRigController.get_controller(rig)
c.set_skeletal_mesh(source)
c.set_retarget_root('DEF-hips')
chains={'Root':('root','root'),'Spine':('DEF-spine.001','DEF-spine.003'),'Neck':('DEF-neck','DEF-neck'),'Head':('DEF-head','DEF-head')}
for side,suffix in [('Left','L'),('Right','R')]:
    chains[side+'Leg']=('DEF-thigh.'+suffix,'DEF-foot.'+suffix)
    chains[side+'Clavicle']=('DEF-shoulder.'+suffix,'DEF-shoulder.'+suffix)
    chains[side+'Arm']=('DEF-upper_arm.'+suffix,'DEF-hand.'+suffix)
    for label,bone in [('Thumb','thumb'),('Index','f_index'),('Middle','f_middle'),('Ring','f_ring'),('Pinky','f_pinky')]:
        chains[side+label]=('DEF-'+bone+'.01.'+suffix,'DEF-'+bone+'.03.'+suffix)
existing={str(ch.chain_name) for ch in c.get_retarget_chains()}
for name,(start,end) in chains.items():
    if name not in existing: c.add_retarget_chain(name,start,end,'')
lib.save_loaded_asset(rig)
report=[]
for name in ['Huohua','Remiel','EvernightMoon','Verina']:
    definition=unreal.load_asset('/Game/Data/VerticalSlice/Characters/DA_Character_'+name)
    target=definition.get_editor_property('character_mesh')
    rigs=target.get_path_name().split('/Meshes/')[0]+'/Rigs'
    target_rig=unreal.load_asset(next(p for p in lib.list_assets(rigs) if '_Mannequin.' in p and '/IK_' in p and '/IK_UEFN_' not in p))
    rtg_path='/Game/Presentation/Animation/RTG_CompleteDefeat_'+name
    rtg=unreal.load_asset(rtg_path) if lib.does_asset_exist(rtg_path) else tools.create_asset('RTG_CompleteDefeat_'+name,'/Game/Presentation/Animation',unreal.IKRetargeter,unreal.IKRetargetFactory())
    ctl=unreal.IKRetargeterController.get_controller(rtg)
    ctl.set_ik_rig(unreal.RetargetSourceOrTarget.SOURCE,rig)
    ctl.set_ik_rig(unreal.RetargetSourceOrTarget.TARGET,target_rig)
    if ctl.get_num_retarget_ops()!=6:
        ctl.remove_all_ops();ctl.add_default_ops()
    ctl.auto_map_chains(unreal.AutoMapChainType.FUZZY,True)
    # A falling body must follow FK; standing foot IK must not pin it upright.
    ctl.set_retarget_op_enabled(2,False)
    ctl.set_retarget_op_enabled(3,False)
    lib.save_loaded_asset(rtg)
    dst='/Game/Presentation/Animation/Combat/FULL_'+name+'_QuaterniusDeath_Anim'
    if lib.does_asset_exist(dst):
        clip=unreal.load_asset(dst)
    else:
        result=unreal.IKRetargetBatchOperation.duplicate_and_retarget([lib.find_asset_data(folder+'/QuaterniusDeath_Anim')],source,target,rtg,'','','FULL_'+name+'_','',False)
        if len(result)!=1: raise RuntimeError('Retarget failed '+name)
        clip=result[0].get_asset()
        lib.save_loaded_asset(clip)
        if not lib.rename_asset(clip.get_path_name(),dst): raise RuntimeError('Relocate failed '+name)
    definition.set_editor_property('defeat_animation',clip)
    lib.save_loaded_asset(definition)
    report.append({'character':name,'clip':dst,'seconds':clip.get_play_length()})
unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True,True)
print(json.dumps({'success':True,'defeat':report}))
