import unreal,json
lib=unreal.EditorAssetLibrary
out=[]
for p in lib.list_assets('/Game/Presentation/Animation/CC0'):
    a=unreal.load_asset(p)
    row={'path':p,'class':a.get_class().get_name()}
    if isinstance(a,unreal.AnimSequence):
        row['length']=a.get_play_length()
        row['hip_final']=str(unreal.AnimationLibrary.get_bone_pose_for_time(a,'DEF-hips',a.get_play_length(),False))
    out.append(row)
c=unreal.IKRigController.get_controller(unreal.load_asset('/Game/Assets/Mannequins/Meshes/IK_SKM_Quinn_Simple'))
print(json.dumps({'assets':out,'chains':[str(x) for x in c.get_retarget_chains()],'api':[n for n in dir(unreal.IKRigController) if 'chain' in n or 'root' in n]}))
