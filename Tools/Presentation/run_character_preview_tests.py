import json
import unreal

world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
filters = ('HSR.UI.CharacterPreview+HSR.UI.CharacterShell+HSR.UI.Frontend+'
           'HSR.Party+HSR.Equipment+HSR.Save.Version+HSR.Save.Validation')
unreal.SystemLibrary.execute_console_command(world, 'Automation RunTests ' + filters)
print(json.dumps({'success': True, 'queued_filters': filters}))
