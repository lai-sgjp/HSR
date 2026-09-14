import json
from pathlib import Path
import unreal

patrol_time = [0.]
patrol_handle = [None]
patrol_samples = []
def patrol_tick(dt):
    patrol_time[0] += dt
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
    for actor in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.HSREnemyCharacter):
        p = actor.get_actor_location()
        patrol_samples.append({'time':patrol_time[0],'actor':actor.get_name(),'location':[p.x,p.y,p.z], 'speed':actor.get_velocity().length(),'controller':str(actor.get_controller())})
    if patrol_time[0] > 12:
        unreal.unregister_slate_post_tick_callback(patrol_handle[0])
        Path(unreal.Paths.project_dir(),'Saved/Presentation/patrol_motion.json').write_text(json.dumps(patrol_samples,indent=2),encoding='utf-8')
patrol_handle[0] = unreal.register_slate_post_tick_callback(patrol_tick)
print(json.dumps({'success':True,'sampling_seconds':12}))
