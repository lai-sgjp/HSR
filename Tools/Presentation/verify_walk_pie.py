"""Walk stairs and ramp with actual CharacterMovement; teleport only between independent starts."""
import unreal,json,math
from pathlib import Path
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
pc=unreal.GameplayStatics.get_player_controller(world,0);pc.request_close_frontend_to_root()
pawn=unreal.GameplayStatics.get_player_pawn(world,0);pc.reset_ignore_move_input()
routes=[('west_stairs',[(-38,30,110),(-38,73,610),(18,85,610)]),('east_stairs',[(38,30,110),(38,73,610)]),('ramp',[(50,-4,110),(50,72,610)])]
state={'route':0,'point':1,'started':False,'time':0,'results':[],'samples':[],'last_sample':0};handle=[None]
def tick(delta):
 if state['route']>=len(routes):
  Path(unreal.Paths.project_dir(),'Saved/Presentation/walking_validation.json').write_text(json.dumps(state,indent=2),encoding='utf-8');unreal.unregister_slate_post_tick_callback(handle[0]);return
 name,points=routes[state['route']]
 if not state['started']:
  x,y,z=points[0];pawn.set_actor_location(unreal.Vector(x*100,y*100,z),False,True);state['started']=True;state['time']=0;state['point']=1;state['samples']=[]
 state['time']+=delta
 p=pawn.get_actor_location();x,y,z=points[state['point']];dx=x*100-p.x;dy=y*100-p.y;distance=math.hypot(dx,dy)
 if state['time']-state['last_sample']>1:
  state['samples'].append([p.x,p.y,p.z]);state['last_sample']=state['time']
 if distance<75:
  state['point']+=1
  if state['point']>=len(points):
   state['results'].append({'route':name,'passed':abs(p.z-z)<40,'seconds':state['time'],'end':[p.x,p.y,p.z],'samples':state['samples']});state['route']+=1;state['started']=False;state['last_sample']=0;return
 elif state['time']>90:
  state['results'].append({'route':name,'passed':False,'reason':'walk timeout','end':[p.x,p.y,p.z],'samples':state['samples']});state['route']+=1;state['started']=False;state['last_sample']=0;return
 if distance>0:pawn.add_movement_input(unreal.Vector(dx/distance,dy/distance,0),1,False)
handle[0]=unreal.register_slate_post_tick_callback(tick)
print(json.dumps({'success':True,'started':'CharacterMovement routes'}))
