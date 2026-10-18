"""Fault-inject only presentation fields in memory, always restore, never save packages."""
import json
import time
from pathlib import Path
import unreal

world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
pc = unreal.GameplayStatics.get_player_controller(world, 0)
pc.request_close_frontend_to_root()
pc.request_open_character_detail_screen()
report = {'checks': [], 'scope': 'in-memory presentation fault injection; no asset saves'}
state = {'step': 0, 'began': time.monotonic()}
handle = [None]
saved = {}
path = Path(unreal.Paths.project_dir(), 'Saved/CharacterPreview/edges.json')

def shell():
    return next((w for w in unreal.WidgetLibrary.get_all_widgets_of_class(world, unreal.HSRCharacterShellWidget, False)
                 if w.is_visible() and w.get_character_preview()), None)

def record(name, passed, **facts):
    report['checks'].append({'name': name, 'pass': bool(passed), **facts})
    path.write_text(json.dumps(report, indent=2), encoding='utf8')

def restore():
    if saved:
        saved['definition'].set_editor_property('preview_idle_animation', saved['idle'])
        saved['definition'].set_editor_property('character_mesh', saved['mesh'])

def request_again(session, character_id):
    session.request_character('Automation.Preview.ForceNewRequest')
    session.request_character(character_id)

def tick(delta):
    try:
        w = shell()
        if not w: return
        session = w.get_character_preview().get_preview_session()
        if session.get_state() != unreal.HSRCharacterPreviewState.READY:
            assert time.monotonic() - state['began'] < 30, 'Edge test load timeout'
            return
        step = state['step']
        if step == 0:
            character_id = str(session.get_character_id())
            name = character_id.rsplit('.', 1)[1]
            definition = unreal.load_asset('/Game/Data/VerticalSlice/Characters/DA_Character_' + name)
            saved.update(definition=definition, idle=definition.get_editor_property('preview_idle_animation'),
                         mesh=definition.get_editor_property('character_mesh'), id=character_id)
            definition.set_editor_property('preview_idle_animation', None)
            state['before_static'] = session.get_capture_count()
            request_again(session, character_id)
        elif step == 1:
            if session.get_capture_count() <= state['before_static']: return
            unreal.SystemLibrary.execute_console_command(world, 'CSVProfile STARTFILE=CharacterPreview_static_on_demand.csv')
            unreal.SystemLibrary.execute_console_command(world, 'CSVProfile START')
            state.update(count=session.get_capture_count(), began=time.monotonic())
        elif step == 2:
            if time.monotonic() - state['began'] < 8.: return
            unreal.SystemLibrary.execute_console_command(world, 'CSVProfile STOP')
            record('no idle: static mesh Ready without repeated captures', session.get_capture_count() == state['count'],
                   captures=session.get_capture_count())
            session.rotate(100.)
        elif step == 3:
            if session.get_capture_count() <= state['count']: return
            record('static mesh refreshes after interaction', True)
            target = session.get_render_target()
            original_size = unreal.Vector2D(target.size_x, target.size_y)
            session.set_viewport_size(unreal.Vector2D(800, 200))
            record('resize reuses render target and bounds dimensions', session.get_render_target() == target
                   and target.size_x <= 1024 and target.size_y <= 1024,
                   dimensions=[target.size_x, target.size_y])
            session.set_viewport_size(original_size)
            saved['definition'].set_editor_property('character_mesh', None)
            request_again(session, saved['id'])
            record('empty mesh -> explicit Error', session.get_state() == unreal.HSRCharacterPreviewState.ERROR)
            restore()
            request_again(session, saved['id'])
        elif step == 4:
            record('restored resources recover Ready', True)
            other = 'Verina' if saved['id'] != 'Demo.Character.Verina' else 'Huohua'
            wrong_idle = unreal.load_asset('/Game/Presentation/Animation/Locomotion/' + other + '/FINAL_' + other + '_MM_Idle')
            saved['definition'].set_editor_property('preview_idle_animation', wrong_idle)
            state['before_static'] = session.get_capture_count()
            request_again(session, saved['id'])
        elif step == 5:
            if session.get_capture_count() <= state['before_static']: return
            state.update(count=session.get_capture_count(), began=time.monotonic())
        elif step == 6:
            if time.monotonic() - state['began'] < 1.2: return
            record('mismatched Skeleton falls back to static without repeated capture', session.get_capture_count() == state['count'])
            restore()
            request_again(session, saved['id'])
        elif step == 7:
            state['session'] = session
            # The normal gameplay travel teardown must invalidate a pending resource request.
            session.request_character('Demo.Character.Verina' if saved['id'] != 'Demo.Character.Verina' else 'Demo.Character.Huohua')
            unreal.GameplayStatics.open_level(world, 'Map_ObservationCar')
            state['step'] += 1
            return
        elif step == 8:
            return
        state['step'] += 1
    except Exception as error:
        restore()
        record('edge exception', False, error=str(error))
        unreal.unregister_slate_post_tick_callback(handle[0])
        unreal.log_error('CHARACTER_PREVIEW_EDGES_FAILED ' + str(error))

def tick_with_travel(delta):
    if state['step'] == 8:
        new_world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
        if new_world and new_world != world:
            record('map reload invalidates pending preview', state['session'].get_state() == unreal.HSRCharacterPreviewState.CLOSED
                   and len(unreal.GameplayStatics.get_all_actors_of_class(new_world, unreal.HSRCharacterPreviewStage)) == 0)
            unreal.unregister_slate_post_tick_callback(handle[0])
            print('CHARACTER_PREVIEW_EDGES_COMPLETE')
        return
    tick(delta)

handle[0] = unreal.register_slate_post_tick_callback(tick_with_travel)
print(json.dumps({'success': True, 'queued': True}))
