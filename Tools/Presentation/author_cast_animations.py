"""Import CC0 casting actions and bind ranged/healing characters to them."""
from pathlib import Path
import unreal,json
lib=unreal.EditorAssetLibrary;tools=unreal.AssetToolsHelpers.get_asset_tools()
folder='/Game/Presentation/Animation/CC0'
source=unreal.load_asset(folder+'/QuaterniusDeath')
unreal.SystemLibrary.execute_console_command(None,'Interchange.FeatureFlags.Import.FBX 0')
for stem in ['QuaterniusCast','QuaterniusHeal']:
    if lib.does_asset_exist(folder+'/'+stem): continue
    t=unreal.AssetImportTask()
    t.filename=str(Path(unreal.Paths.project_dir(),'ArtSource/Presentation/Quaternius',stem+'.fbx').resolve())
    t.destination_path=folder;t.destination_name=stem;t.automated=True;t.save=True
    t.factory=unreal.FbxFactory()
    o=unreal.FbxImportUI();o.import_mesh=False;o.import_animations=True
    o.skeleton=source.get_editor_property('skeleton')
    o.mesh_type_to_import=unreal.FBXImportType.FBXIT_ANIMATION
    o.automated_import_should_detect_type=False;t.options=o
    tools.import_asset_tasks([t])
for name in ['Remiel','Verina']:
    definition=unreal.load_asset('/Game/Data/VerticalSlice/Characters/DA_Character_'+name)
    target=definition.get_editor_property('character_mesh')
    rtg=unreal.load_asset('/Game/Presentation/Animation/RTG_CompleteDefeat_'+name)
    definition.set_editor_property('melee_basic_attack',False)
    for field,stem in [('attack_animation','QuaterniusCast'),('skill_animation','QuaterniusHeal' if name=='Verina' else 'QuaterniusCast')]:
        dst='/Game/Presentation/Animation/Combat/CAST_'+name+'_'+stem
        if lib.does_asset_exist(dst): clip=unreal.load_asset(dst)
        else:
            result=unreal.IKRetargetBatchOperation.duplicate_and_retarget([lib.find_asset_data(folder+'/'+stem)],source,target,rtg,'','','CAST_'+name+'_','',False)
            if len(result)!=1: raise RuntimeError('Cannot retarget '+stem)
            clip=result[0].get_asset();lib.save_loaded_asset(clip)
            if not lib.rename_asset(clip.get_path_name(),dst): raise RuntimeError('Cannot relocate '+dst)
        definition.set_editor_property(field,clip)
    lib.save_loaded_asset(definition)
unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True,True)
print(json.dumps({'success':True,'ranged':'Remiel / Verina','healing':'Verina'}))
