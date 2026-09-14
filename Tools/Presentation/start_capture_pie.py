import unreal,json
from pathlib import Path
request_path=Path(unreal.Paths.project_dir(),'Saved/Presentation/capture_request.json')
request=json.loads(request_path.read_text(encoding='utf-8-sig')) if request_path.exists() else {}
levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
levels.load_level(request.get('map','/Game/Maps/VerticalSlice/Map_ObservationCar'))
started=unreal.MCPythonHelper.begin_presentation_pie(request.get('width',1920),request.get('height',1080))
print(json.dumps({'success':started,'width':request.get('width',1920),'height':request.get('height',1080)}))
