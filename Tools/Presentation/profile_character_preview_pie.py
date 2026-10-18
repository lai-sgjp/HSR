"""Record separate engine CSV captures; PIE/Editor overhead is part of these measurements."""
import json
import time
from pathlib import Path
import unreal

root = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
out = root / 'Saved/CharacterPreview'
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
assert world
pc = unreal.GameplayStatics.get_player_controller(world, 0)
scenarios = ['closed_running', 'paused_menu', 'open_idle', 'rotating', 'switching', 'closed_after']
state = {'index': 0, 'began': time.monotonic(), 'phase': 'setup', 'last_switch': 0., 'switch_count': 0}
report = {'environment': 'UE5.6.1 Editor PIE, DX12, existing ObservationCar map; same fixed camera; warmup3s/sample8s each',
          'viewport': str(unreal.WidgetLayoutLibrary.get_viewport_size(world)), 'scenarios': []}
handle = [None]

def shell():
    return next((w for w in unreal.WidgetLibrary.get_all_widgets_of_class(world, unreal.HSRCharacterShellWidget, False)
                 if w.is_visible() and w.get_character_preview()), None)

def session():
    w = shell()
    return w.get_character_preview().get_preview_session() if w else None

def command(text):
    unreal.SystemLibrary.execute_console_command(world, text)

def save():
    (out / 'profile_scenarios.json').write_text(json.dumps(report, indent=2), encoding='utf8')

def tick(delta):
    try:
        name = scenarios[state['index']]
        now = time.monotonic()
        if state['phase'] == 'setup':
            pc.request_close_frontend_to_root()
            if name == 'paused_menu': pc.request_open_pause_screen()
            elif name in ['open_idle', 'rotating', 'switching']: pc.request_open_character_detail_screen()
            state.update(phase='warmup', began=now)
            return
        if state['phase'] == 'warmup':
            if now - state['began'] < 3: return
            s = session()
            if name in ['open_idle', 'rotating', 'switching'] and (not s or s.get_state() != unreal.HSRCharacterPreviewState.READY): return
            if shell(): state['ids'] = [e.character_id for e in shell().get_current_snapshot().character_entries if e.is_available]
            command('CSVProfile STARTFILE=CharacterPreview_' + name + '.csv')
            command('CSVProfile START')
            state.update(phase='measure', began=now, first_capture=s.get_capture_count() if s else 0)
            return
        if state['phase'] == 'measure':
            s = session()
            if name == 'rotating' and s: s.rotate(60. * delta)
            if name == 'switching' and shell() and now - state['last_switch'] > .4:
                shell().select_character(state['ids'][state['switch_count'] % len(state['ids'])])
                state['last_switch'] = now
                state['switch_count'] += 1
            if now - state['began'] < 8: return
            command('CSVProfile STOP')
            report['scenarios'].append({'name': name, 'seconds': now-state['began'],
                'captures_delta': s.get_capture_count()-state['first_capture'] if s else 0,
                'paused': unreal.GameplayStatics.is_game_paused(world),
                'csv_name': 'CharacterPreview_' + name + '.csv'})
            save()
            state.update(phase='flush', began=now)
            return
        if state['phase'] == 'flush' and now - state['began'] > 2:
            state['index'] += 1
            if state['index'] == len(scenarios):
                pc.request_close_frontend_to_root()
                unreal.unregister_slate_post_tick_callback(handle[0])
                print('CHARACTER_PREVIEW_PROFILE_COMPLETE')
            else: state['phase'] = 'setup'
    except Exception as error:
        command('CSVProfile STOP')
        report['error'] = str(error)
        save()
        unreal.unregister_slate_post_tick_callback(handle[0])
        unreal.log_error('CHARACTER_PREVIEW_PROFILE_FAILED ' + str(error))

handle[0] = unreal.register_slate_post_tick_callback(tick)
print(json.dumps({'success': True, 'queued': True, 'scenario_count': len(scenarios)}))
