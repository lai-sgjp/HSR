import json
from pathlib import Path
import unreal

world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
pc = unreal.GameplayStatics.get_player_controller(world, 0)
shell = next((w for w in unreal.WidgetLibrary.get_all_widgets_of_class(world, unreal.HSRCharacterShellWidget, False)
              if w.is_visible() and w.get_character_preview()), None)
if not shell:
    pc.request_open_character_detail_screen()
    print(json.dumps({'success': True, 'opened': True, 'repeat_for_geometry': True}))
else:
    preview = shell.get_character_preview()
    session = preview.get_preview_session()
    stage = session.get_stage()
    report = {'success': True, 'state': str(session.get_state()), 'character': str(session.get_character_id()),
              'capture_count': session.get_capture_count(),
              'yaw': stage.get_preview_yaw() if stage else None,
              'distance': stage.get_camera_distance() if stage else None,
              'snapshot': str(shell.get_current_snapshot().selected_character_id)}
    Path(unreal.Paths.project_dir(), 'Saved/CharacterPreview/input_state.json').write_text(json.dumps(report, indent=2), encoding='utf8')
    unreal.MCPythonHelper.capture_play_viewport('PREVIEW_MOUSE_STATE')
    print(json.dumps(report))
