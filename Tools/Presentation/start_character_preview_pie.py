"""Start this task's fixed 1280x720 gameplay session, independent of older capture requests."""
import json
import unreal
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assert not unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world(), 'Stop the previous PIE session first'
levels.load_level('/Game/Maps/VerticalSlice/Map_ObservationCar')
assert unreal.MCPythonHelper.begin_presentation_pie(1280, 720)
print(json.dumps({'success': True, 'width': 1280, 'height': 720}))
