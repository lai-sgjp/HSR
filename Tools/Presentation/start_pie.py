import unreal,json
from pathlib import Path
samples=[]
handle=[None]
elapsed=[0.]
def sample(delta):
 elapsed[0]+=delta
 world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
 if world:
  pawn=unreal.GameplayStatics.get_player_pawn(world,0)
  pc=unreal.GameplayStatics.get_player_controller(world,0)
  if pawn and pc:
   samples.append({'t':elapsed[0],'position':str(pawn.get_actor_location()),'velocity':str(pawn.get_velocity())})
 if elapsed[0]>3:
  Path(unreal.Paths.project_dir(),'Saved/Presentation/spawn_samples.json').write_text(json.dumps(samples,indent=2),encoding='utf-8')
  unreal.unregister_slate_post_tick_callback(handle[0])
handle[0]=unreal.register_slate_post_tick_callback(sample)
levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
levels.load_level('/Game/Maps/VerticalSlice/Map_ObservationCar')
unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).set_level_viewport_camera_info(unreal.Vector(-2800,-800,220),unreal.Rotator(yaw=0))
levels.editor_request_begin_play()
print(json.dumps({'success':True,'requested':'PIE ObservationCar'}))
