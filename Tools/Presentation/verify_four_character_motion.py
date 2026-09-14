"""Sample component-space bones in real PIE while switching all four slots and moving."""
import json
from pathlib import Path
import unreal

world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
if not world:
    raise RuntimeError('Start exploration PIE first')
pc = unreal.GameplayStatics.get_player_controller(world, 0)
state = {'elapsed': 0., 'slot': -1, 'frames': [], 'baseline': {}, 'result': []}
captured=set()
handle = [None]
def sample(dt):
    state['elapsed'] += dt
    slot = int(state['elapsed'] / 4.)
    if slot >= 4:
        unreal.unregister_slate_post_tick_callback(handle[0])
        for i in range(4):
            frames = [f for f in state['frames'] if f['slot'] == i]
            state['result'].append({'slot': i, 'max_bone_delta_cm': max((f['delta'] for f in frames), default=0), 'frames': len(frames)})
        Path(unreal.Paths.project_dir(), 'Saved/Presentation/four_character_motion.json').write_text(json.dumps(state, ensure_ascii=False, indent=2), encoding='utf-8')
        return
    if state['slot'] != slot:
        ok = pc.switch_exploration_character(slot)
        state['slot'] = slot
        state['baseline'] = {}
        state['result'].append({'switch': slot, 'accepted': ok})
        unreal.GameplayStatics.get_player_pawn(world, 0).set_actor_location(unreal.Vector(-2000,-500,180), False, False)
    pawn = unreal.GameplayStatics.get_player_pawn(world, 0)
    mesh = pawn.get_editor_property('mesh')
    phase = state['elapsed'] % 4
    if 1 < phase < 3.5:
        pawn.add_movement_input(unreal.Vector(1, 0, 0), 1.)
    if phase > 2 and slot not in captured:
        captured.add(slot)
        unreal.MCPythonHelper.capture_play_viewport('final_running_'+str(slot))
    delta = 0.
    for index in range(min(mesh.get_num_bones(), 85)):
        bone = str(mesh.get_bone_name(index))
        location = mesh.get_socket_transform(bone, unreal.RelativeTransformSpace.RTS_COMPONENT).translation
        v = (location.x, location.y, location.z)
        if bone in state['baseline']:
            delta = max(delta, sum((a-b)**2 for a,b in zip(v, state['baseline'][bone]))**.5)
        elif phase > .7:
            state['baseline'][bone] = v
    if len(state['frames']) == 0 or state['elapsed'] - state['frames'][-1]['time'] > .15:
        state['frames'].append({'slot': slot, 'time': state['elapsed'], 'mesh': mesh.get_editor_property('skeletal_mesh_asset').get_path_name(), 'anim': str(mesh.get_anim_instance()), 'speed': pawn.get_velocity().length(), 'delta': delta})
handle[0] = unreal.register_slate_post_tick_callback(sample)
print(json.dumps({'success': True, 'sampling_seconds': 16}))
