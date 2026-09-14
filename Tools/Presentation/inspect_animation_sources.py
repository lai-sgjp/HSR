import json
from pathlib import Path
import unreal

lib = unreal.EditorAssetLibrary
report = {'sequences': [], 'rigs': [], 'navigation_api': []}
for folder in ['/Game/Assets/Mannequins', '/Game/Assets/mmd/Character', '/Game/Presentation/Animation', '/Game/Characters']:
    for path in lib.list_assets(folder):
        if any(word in path for word in ['MM_Attack', 'HitReact_Front_Med_01', 'Death_Front_01']):
            asset = unreal.load_asset(path)
            if isinstance(asset, unreal.AnimSequence):
                report['sequences'].append({'path': path, 'skeleton': asset.get_editor_property('skeleton').get_path_name(), 'length': asset.get_play_length()})
        if '/IK_' in path or '/RTG_' in path:
            report['rigs'].append(path)
report['navigation_api'] = [x.get_path_name() for x in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors() if 'Nav' in x.get_class().get_name()]
Path(unreal.Paths.project_dir(), 'Saved/Presentation/animation_sources.json').write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
print(json.dumps({'success': True, 'sequences': len(report['sequences']), 'navigation_api': report['navigation_api']}))
