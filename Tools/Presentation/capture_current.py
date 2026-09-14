import json
from pathlib import Path
import unreal
editor=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
out=Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))/'Saved/Presentation/current.png'
unreal.AutomationLibrary.take_high_res_screenshot(1280,720,str(out))
print(json.dumps({'success':True,'view':str(editor.get_level_viewport_camera_info()),'file':str(out)}))
