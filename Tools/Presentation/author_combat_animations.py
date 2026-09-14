"""Retarget the existing mannequin attack, hit reaction and defeat onto each playable skeleton."""
import json
import warnings
from pathlib import Path
import unreal

warnings.filterwarnings('ignore', category=RuntimeWarning)
lib = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
source = unreal.load_asset('/Game/Assets/Mannequins/Meshes/SKM_Quinn_Simple')
source_rig = unreal.load_asset('/Game/Assets/Mannequins/Meshes/IK_SKM_Quinn_Simple')
sources = {
    'attack_animation': '/Game/Assets/Mannequins/Anims/Unarmed/Attack/MM_Attack_01',
    'skill_animation': '/Game/Assets/Mannequins/Anims/Unarmed/Attack/MM_ChargedAttack',
    'hit_animation': '/Game/Assets/Mannequins/Anims/Rifle/HitReact/MM_HitReact_Front_Med_01',
}
report = []
for name in ['Huohua', 'Remiel', 'EvernightMoon', 'Verina']:
    definition = unreal.load_asset('/Game/Data/VerticalSlice/Characters/DA_Character_' + name)
    target = definition.get_editor_property('character_mesh')
    folder = target.get_path_name().split('/Meshes/')[0] + '/Rigs'
    target_rig_path = next(p for p in lib.list_assets(folder) if '_Mannequin.' in p and '/IK_' in p and '/IK_UEFN_' not in p)
    rtg_path = '/Game/Presentation/Animation/RTG_Combat_' + name
    rtg = unreal.load_asset(rtg_path) if lib.does_asset_exist(rtg_path) else tools.create_asset('RTG_Combat_' + name, '/Game/Presentation/Animation', unreal.IKRetargeter, unreal.IKRetargetFactory())
    ctl = unreal.IKRetargeterController.get_controller(rtg)
    ctl.set_ik_rig(unreal.RetargetSourceOrTarget.SOURCE, source_rig)
    ctl.set_ik_rig(unreal.RetargetSourceOrTarget.TARGET, unreal.load_asset(target_rig_path))
    if ctl.get_num_retarget_ops() != 6:
        ctl.remove_all_ops()
        ctl.add_default_ops()
    ctl.auto_map_chains(unreal.AutoMapChainType.FUZZY, True)
    lib.save_loaded_asset(rtg)
    for field, src in sources.items():
        stem = 'LIVE_' + name + '_' + src.rsplit('/', 1)[-1]
        dst = '/Game/Presentation/Animation/Combat/' + stem
        asset = unreal.load_asset(dst) if lib.does_asset_exist(dst) else None
        if not asset:
            generated = unreal.IKRetargetBatchOperation.duplicate_and_retarget([lib.find_asset_data(src)], source, target, rtg, '', '', 'LIVE_' + name + '_', '', False)
            if len(generated) != 1:
                raise RuntimeError('Expected one animation: ' + field)
            asset = generated[0].get_asset()
            lib.save_loaded_asset(asset)
            if not lib.rename_asset(asset.get_path_name(), dst):
                raise RuntimeError('Cannot relocate ' + stem)
        if asset.get_editor_property('skeleton') != target.get_editor_property('skeleton'):
            raise RuntimeError('Wrong skeleton: ' + dst)
        definition.set_editor_property(field, asset)
        report.append({'character': name, 'field': field, 'source': src, 'asset': dst})
    lib.save_loaded_asset(definition)
Path(unreal.Paths.project_dir(), 'Saved/Presentation/combat_animation_sources.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
print(json.dumps({'success': True, 'animations': len(report)}))
exec(Path(unreal.Paths.project_dir(), 'Tools/Presentation/author_complete_defeat.py').read_text(encoding='utf-8'))
exec(Path(unreal.Paths.project_dir(), 'Tools/Presentation/author_cast_animations.py').read_text(encoding='utf-8'))
