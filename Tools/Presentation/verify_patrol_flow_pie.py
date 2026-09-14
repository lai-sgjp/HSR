"""Exercise perception and real collision encounter; never inject AI state."""
import unreal,json,time
from pathlib import Path
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
pc=unreal.GameplayStatics.get_player_controller(world,0)
pc.request_close_frontend_to_root()
pawn=unreal.GameplayStatics.get_player_pawn(world,0)
enemy=next(a for a in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.HSREnemyCharacter) if a.get_actor_label()=='GAME_CourtyardPatrolAI')
controller=enemy.get_controller()
start=time.monotonic();handle=[None];phase=[-1];samples=[];last=[-1.]
def tick(dt):
    t=time.monotonic()-start
    current=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
    if current!=world:
        unreal.unregister_slate_post_tick_callback(handle[0])
        samples.append({'seconds':round(t,2),'result':'contact transitioned to battle','world':str(current)})
        Path(unreal.Paths.project_dir(),'Saved/Presentation/patrol_flow_pie.json').write_text(json.dumps(samples,indent=2),encoding='utf-8')
        return
    stage=0 if t<3 else 1 if t<8 else 2 if t<21 else 3
    if stage!=phase[0]:
        phase[0]=stage
        if stage==0: pos=enemy.get_actor_location()+unreal.Vector(3000,3000,20)
        elif stage==1: pos=enemy.get_actor_location()+enemy.get_actor_forward_vector()*900+unreal.Vector(0,0,20)
        elif stage==2: pos=enemy.get_actor_location()+unreal.Vector(4500,4500,20)
        else: pos=enemy.get_actor_location()+unreal.Vector(80,0,20)
        pawn.set_actor_location(pos,False,True)
    if t-last[0]>.3:
        last[0]=t
        samples.append({'seconds':round(t,2),'stage':stage,'state':str(controller.get_current_state()),'position':str(enemy.get_actor_location()),'speed':enemy.get_velocity().length()})
    if t>23:
        unreal.unregister_slate_post_tick_callback(handle[0])
        Path(unreal.Paths.project_dir(),'Saved/Presentation/patrol_flow_pie.json').write_text(json.dumps(samples,indent=2),encoding='utf-8')
        unreal.MCPythonHelper.capture_play_viewport('patrol_contact_prebattle')
handle[0]=unreal.register_slate_post_tick_callback(tick)
print(json.dumps({'success':True,'seconds':23}))
