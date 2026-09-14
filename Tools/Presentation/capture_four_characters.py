import json
from pathlib import Path
import unreal

world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
pc = unreal.GameplayStatics.get_player_controller(world, 0)
capture_elapsed = [0.]
capture_slot = [-1]
captured = set()
capture_handle = [None]
def capture_tick(dt):
    capture_elapsed[0] += dt
    slot = int(capture_elapsed[0] / 2)
    if slot >= 4:
        unreal.unregister_slate_post_tick_callback(capture_handle[0])
        return
    if capture_slot[0] != slot:
        pc.switch_exploration_character(slot)
        pawn = unreal.GameplayStatics.get_player_pawn(world, 0)
        pawn.set_actor_location(unreal.Vector(-1400,-400,180),False,True)
        pc.set_control_rotation(unreal.Rotator(pitch=-8,yaw=0))
        capture_slot[0] = slot
    if capture_elapsed[0] % 2 > 1 and slot not in captured:
        unreal.MCPythonHelper.capture_play_viewport('fixed_character_' + str(slot))
        captured.add(slot)
capture_handle[0] = unreal.register_slate_post_tick_callback(capture_tick)
print(json.dumps({'success':True,'captures':4}))
