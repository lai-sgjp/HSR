"""Exercise public player/UI/interaction entry points in the current PIE session."""
import unreal,json
from pathlib import Path
root=Path(unreal.Paths.project_dir())
request=json.loads((root/'Saved/Presentation/pie_request.json').read_text(encoding='utf-8-sig'))
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
pc=unreal.GameplayStatics.get_player_controller(world,0)
pawn=unreal.GameplayStatics.get_player_pawn(world,0)
action=request['action'];result={}
if action=='interact':
 target=next(a for a in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.Actor) if a.get_actor_label()==request['label'])
 p=target.get_actor_location();p.x-=120;p.z+=110
 pawn.set_actor_location(p,False,True)
 component=pawn.get_component_by_class(unreal.HSRInteractionComponent)
 result['candidate']=str(component.get_current_candidate())
 result['interaction']=str(component.try_interact())
elif action=='inventory':pc.request_open_inventory_screen()
elif action=='pause':pc.request_open_pause_screen()
elif action=='character':pc.request_open_character_detail_screen()
elif action=='back':pc.request_back_screen()
elif action=='close':pc.request_close_frontend_to_root()
elif action=='move':
 pawn.set_actor_location(unreal.Vector(*request['location']),False,True)
elif action=='camera':pc.set_control_rotation(unreal.Rotator(pitch=request.get('pitch',-10),yaw=request.get('yaw',0)))
elif action=='apply_chest_collision':
 for chest in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.HSRRewardChest):
  chest.get_editor_property('collision_component').set_collision_profile_name('OverlapAllDynamic')
  chest.get_editor_property('collision_component').set_sphere_radius(180)
result.update(success=True,action=action)
print(json.dumps(result,ensure_ascii=False))
