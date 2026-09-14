import json
from pathlib import Path
import unreal
levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
levels.load_level('/Game/Maps/VerticalSlice/Map_NewEriduSixthStreetMetro')
editor=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
editor.set_level_viewport_camera_info(unreal.Vector(-2100,-4000,550),unreal.Rotator(pitch=-5,yaw=60,roll=0))
out=Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))/'Saved/Presentation/city.png'
unreal.AutomationLibrary.take_high_res_screenshot(1280,720,str(out))
print(json.dumps({'success':True,'scheduled':str(out)}))
