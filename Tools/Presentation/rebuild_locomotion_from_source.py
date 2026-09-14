"""Retarget original mannequin locomotion, not an already-retargeted VRM graph."""
import json
import warnings
from pathlib import Path
import unreal
from UnrealMCPython.blueprint_actions import ue_connect_blueprint_pins

warnings.filterwarnings('ignore', category=RuntimeWarning)
lib = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
source = unreal.load_asset('/Game/Assets/Mannequins/Meshes/SKM_Quinn_Simple')
source_bp = '/Game/Assets/Mannequins/Anims/Unarmed/ABP_Unarmed'
report = []
for name in ['Huohua', 'Remiel', 'EvernightMoon', 'Verina']:
    definition = unreal.load_asset('/Game/Data/VerticalSlice/Characters/DA_Character_' + name)
    target = definition.get_editor_property('character_mesh')
    rtg = unreal.load_asset('/Game/Presentation/Animation/RTG_Combat_' + name)
    ctl = unreal.IKRetargeterController.get_controller(rtg)
    if ctl.get_num_retarget_ops() != 6:
        ctl.remove_all_ops()
        ctl.add_default_ops()
    ctl.auto_map_chains(unreal.AutoMapChainType.FUZZY, True)
    lib.save_loaded_asset(rtg)
    prefix = 'FINAL_' + name + '_'
    dst_folder = '/Game/Presentation/Animation/Locomotion/' + name
    bp_path = dst_folder + '/' + prefix + 'ABP_Unarmed'
    bp = unreal.load_asset(bp_path) if lib.does_asset_exist(bp_path) else None
    if not bp:
        unreal.IKRetargetBatchOperation.duplicate_and_retarget([lib.find_asset_data(source_bp)], source, target, rtg, '', '', prefix, '', True)
        # The batch result contains sequences, not necessarily the generated AnimBlueprint.
        paths = [p for p in lib.list_assets('/Game', False, False) if p.rsplit('/',1)[-1].startswith(prefix)]
        for path in paths:
            asset = unreal.load_asset(path)
            lib.save_loaded_asset(asset)
        for path in paths:
            asset = unreal.load_asset(path)
            dst = dst_folder + '/' + asset.get_name()
            if not lib.rename_asset(path, dst): raise RuntimeError('Cannot move ' + path)
        bp = unreal.load_asset(bp_path)
    if not bp: raise RuntimeError('No generated ABP: ' + name)
    graph = json.loads(unreal.MCPythonHelper.get_blueprint_graph_info(bp, 'AnimGraph'))
    slot = next(n['node_name'] for n in graph['nodes'] if n['node_class'] == 'AnimGraphNode_Slot')
    root = next(n['node_name'] for n in graph['nodes'] if n['node_class'] == 'AnimGraphNode_Root')
    result = json.loads(ue_connect_blueprint_pins(bp_path, 'AnimGraph', slot, 'Pose', root, 'Result'))
    if not result.get('success'): raise RuntimeError(str(result))
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    definition.set_editor_property('animation_class', bp.generated_class())
    lib.save_loaded_asset(definition)
    lib.save_directory(dst_folder, False, True)
    report.append({'character': name, 'abp': bp_path})
Path(unreal.Paths.project_dir(), 'Saved/Presentation/locomotion_rebuild.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
print(json.dumps({'success': True, 'characters': len(report)}))
