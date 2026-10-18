"""PIE integration matrix, using the real character-page navigation and roster.

Does not save, equip, change party slots or mutate character definitions.
Run after start_capture_pie.py through editor_bridge.py. Results are written as
steps finish, so a failure is reviewable even if a later step cannot run.
"""
import json
import re
import time
from pathlib import Path
import unreal

root = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
out = root / 'Saved/CharacterPreview'
out.mkdir(parents=True, exist_ok=True)
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
assert world, 'Start PIE first'
pc = unreal.GameplayStatics.get_player_controller(world, 0)
pawn = unreal.GameplayStatics.get_player_pawn(world, 0)
report = {'started': time.time(), 'world': world.get_path_name(), 'steps': [],
          'pawn_before': pawn.get_path_name(), 'transform_before': str(pawn.get_actor_transform()),
          'viewport': str(unreal.WidgetLayoutLibrary.get_viewport_size(world))}
state = {'index': 0, 'began': time.monotonic(), 'round': 0}
handle = [None]
initial = {}

def value_text(value):
    # Unreal StructBase repr includes the wrapper's memory address, which is not data.
    return re.sub(r'\(0x[0-9A-Fa-f]+\)', '(address)', str(value))

def shell():
    return next((w for w in unreal.WidgetLibrary.get_all_widgets_of_class(world, unreal.HSRCharacterShellWidget, False)
                 if w.is_visible() and w.get_character_preview()), None)

def ready():
    w = shell()
    session = w.get_character_preview().get_preview_session() if w else None
    return session if session and session.get_state() == unreal.HSRCharacterPreviewState.READY else None

def record(name, passed, **facts):
    report['steps'].append({'name': name, 'pass': bool(passed), **facts})
    (out / 'pie_matrix.json').write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')

def capture(name, rt=True):
    path = unreal.MCPythonHelper.capture_play_viewport('PREVIEW_' + name)
    if rt and ready():
        unreal.RenderingLibrary.export_render_target(world, ready().get_render_target(), str(out), name + '.png')
    return path

def wait_ready():
    session = ready()
    if not session:
        assert time.monotonic() - state['began'] < 45, 'Preview did not become Ready'
    return session

def step_open():
    pc.request_close_frontend_to_root()
    pc.request_open_character_detail_screen()
    return True

def step_initial():
    session = wait_ready()
    if not session or session.get_capture_count() < 2: return False
    snapshot = shell().get_current_snapshot()
    initial.update(id=snapshot.selected_character_id, stats=value_text(snapshot.character_detail), equipment=value_text(snapshot.equipment_detail))
    state['ids'] = [e.character_id for e in snapshot.character_entries if e.is_available]
    stage = session.get_stage()
    state['capture_before'] = session.get_capture_count()
    record('real UI -> Ready', True, id=str(session.get_character_id()), mesh=stage.get_preview_mesh().get_skinned_asset().get_path_name(),
           paused=unreal.GameplayStatics.is_game_paused(world), capture=capture('initial'),
           rt=[session.get_render_target().size_x, session.get_render_target().size_y])
    return True

def step_idle():
    if time.monotonic() - state['began'] < 1.2: return False
    session = wait_ready()
    record('capture advances while world paused', session.get_capture_count() > state['capture_before'],
           before=state['capture_before'], after=session.get_capture_count(), capture=capture('idle_later'))
    stage = session.get_stage()
    state['yaw'] = stage.get_preview_yaw()
    state['distance'] = stage.get_camera_distance()
    session.rotate(180.0)
    session.zoom(2.0)
    return True

def step_controls():
    session = wait_ready()
    stage = session.get_stage()
    record('rotation and zoom controller', stage.get_preview_yaw() != state['yaw'] and stage.get_camera_distance() < state['distance'],
           yaw=stage.get_preview_yaw(), distance=stage.get_camera_distance(), capture=capture('rotate_zoom'))
    session.reset_view()
    record('reset controller', abs(stage.get_preview_yaw()-state['yaw']) < .01 and abs(stage.get_camera_distance()-state['distance']) < .01)
    state['character_index'] = 0
    return True

def step_characters():
    index = state['character_index']
    if index >= len(state['ids']): return True
    desired = state['ids'][index]
    session = ready()
    if state.get('requested_id') != desired:
        state['character_capture_start'] = session.get_capture_count() if session else 0
        state['character_requested_at'] = time.monotonic()
        state['requested_id'] = desired
        shell().select_character(desired)
        return False
    if not session or session.get_character_id() != desired: return False
    if session.get_capture_count() < state['character_capture_start'] + 2 or time.monotonic() - state['character_requested_at'] < .75: return False
    record('roster character ' + str(desired), True, mesh=session.get_stage().get_preview_mesh().get_skinned_asset().get_path_name(),
           capture=capture('character_' + str(index)))
    state['character_index'] += 1
    return False

def step_race_request():
    for i in range(24): shell().select_character(state['ids'][i % len(state['ids'])])
    state['last_id'] = state['ids'][23 % len(state['ids'])]
    return True

def step_race_result():
    session = wait_ready()
    if not session: return False
    record('rapid switch latest request wins', session.get_character_id() == state['last_id'],
           selected=str(state['last_id']), rendered=str(session.get_character_id()))
    session.request_character('Automation.Preview.InvalidID')
    record('invalid ID -> Error', session.get_state() == unreal.HSRCharacterPreviewState.ERROR,
           error=str(session.get_error_text()))
    session.request_character(initial['id'])
    state['closed_session'] = session
    state['closed_count'] = session.get_capture_count()
    pc.request_close_frontend_to_root()
    return True

def step_loading_close():
    session = state['closed_session']
    stages = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.HSRCharacterPreviewStage)
    record('close during Loading: no late stage/capture', session.get_state() == unreal.HSRCharacterPreviewState.CLOSED
           and session.get_capture_count() == state['closed_count'] and len(stages) == 0,
           captures=session.get_capture_count(), stage_count=len(stages))
    pc.request_open_character_detail_screen()
    return True

def step_cycles():
    session = wait_ready()
    if not session or session.get_capture_count() < 1: return False
    stages = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.HSRCharacterPreviewStage)
    assert len(stages) == 1, 'More than one active stage'
    closed = session
    count = session.get_capture_count()
    pc.request_close_frontend_to_root()
    assert closed.get_state() == unreal.HSRCharacterPreviewState.CLOSED
    assert closed.get_capture_count() == count
    state['round'] += 1
    record('Ready open/close ' + str(state['round']), True, active_stages_before_close=len(stages))
    pc.request_open_character_detail_screen()
    return state['round'] >= 20

def step_business():
    session = wait_ready()
    if not session: return False
    shell().select_character(initial['id'])
    snapshot = shell().get_current_snapshot()
    actual_stats = value_text(snapshot.character_detail)
    actual_equipment = value_text(snapshot.equipment_detail)
    record('character and equipment snapshots unchanged', actual_stats == initial['stats']
           and actual_equipment == initial['equipment'], before_stats=initial['stats'], after_stats=actual_stats,
           before_equipment=initial['equipment'], after_equipment=actual_equipment)
    record('world pawn and transform unchanged', unreal.GameplayStatics.get_player_pawn(world, 0) == pawn and str(pawn.get_actor_transform()) == report['transform_before'])
    report['finished'] = time.time()
    capture('final')
    pc.request_close_frontend_to_root()
    record('final closed: zero stages', len(unreal.GameplayStatics.get_all_actors_of_class(world, unreal.HSRCharacterPreviewStage)) == 0)
    return True

steps = [step_open, step_initial, step_idle, step_controls, step_characters, step_race_request,
         step_race_result, step_loading_close, step_cycles, step_business]

def tick(delta):
    if time.monotonic() - state.get('last_tick', 0) < .35: return
    state['last_tick'] = time.monotonic()
    try:
        if steps[state['index']]():
            state['index'] += 1
            state['began'] = time.monotonic()
        if state['index'] == len(steps):
            unreal.unregister_slate_post_tick_callback(handle[0])
            print('CHARACTER_PREVIEW_MATRIX_COMPLETE')
    except Exception as error:
        record('exception at ' + steps[state['index']].__name__, False, error=str(error))
        unreal.unregister_slate_post_tick_callback(handle[0])
        unreal.log_error('CHARACTER_PREVIEW_MATRIX_FAILED: ' + str(error))

handle[0] = unreal.register_slate_post_tick_callback(tick)
print(json.dumps({'success': True, 'queued': True, 'report': str(out / 'pie_matrix.json')}))
