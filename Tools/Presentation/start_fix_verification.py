import unreal, json
from pathlib import Path
request = json.loads(Path(unreal.Paths.project_dir(), 'Saved/Presentation/fix_verification_request.json').read_text(encoding='utf-8-sig'))
unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).load_level(request.get('map', '/Game/Maps/VerticalSlice/Map_ObservationCar'))
print(json.dumps({'success': unreal.MCPythonHelper.begin_presentation_pie(request.get('width',1920), request.get('height',1080))}))
