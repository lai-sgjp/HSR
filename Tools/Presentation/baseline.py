import json
from pathlib import Path
import unreal

root = Path(unreal.Paths.project_dir()) / 'Saved/Presentation/baseline'
root.mkdir(parents=True, exist_ok=True)
maps = ['Map_ObservationCar', 'Map_NewEriduSixthStreetMetro', 'Map_HertaSupportSection']
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
report = {'maps': {}, 'widgets': {}}
for name in maps:
    levels.load_level('/Game/Maps/VerticalSlice/' + name)
    entries = []
    for actor in actors.get_all_level_actors():
        location = actor.get_actor_location()
        entries.append({'name': actor.get_actor_label(), 'class': actor.get_class().get_path_name(), 'location': [location.x, location.y, location.z]})
    report['maps'][name] = entries
for path in unreal.EditorAssetLibrary.list_assets('/Game/UI', recursive=True, include_folder=False):
    asset = unreal.load_asset(path)
    if isinstance(asset, unreal.WidgetBlueprint):
        report['widgets'][path] = json.loads(unreal.MCPythonHelper.umg_get_widget_info(asset))
(root / 'inventory.json').write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
print(json.dumps({'success': True, 'report': str(root / 'inventory.json'), 'maps': {k:len(v) for k,v in report['maps'].items()}, 'widgets':len(report['widgets'])}))
