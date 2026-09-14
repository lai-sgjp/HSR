import unreal, json
from pathlib import Path
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
pc = unreal.GameplayStatics.get_player_controller(world,0)
pc.request_close_frontend_to_root()
pawn = unreal.GameplayStatics.get_player_pawn(world,0)
chest = next(a for a in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.HSRRewardChest) if a.get_actor_label() == 'GAME_WangXiaYiTong')
p = chest.get_actor_location(); p.x -= 120; p.z += 110
pawn.set_actor_location(p,False,True)
result = str(pawn.get_component_by_class(unreal.HSRInteractionComponent).try_interact())
elapsed = [0.]; handle=[None]
def capture(dt):
    elapsed[0] += dt
    if elapsed[0] < .5: return
    unreal.unregister_slate_post_tick_callback(handle[0])
    size = unreal.WidgetLayoutLibrary.get_viewport_size(world)
    widgets = unreal.WidgetLibrary.get_all_widgets_of_class(world,unreal.HSRRewardSummaryWidget,False)
    report = {'interaction': result,'viewport':[size.x,size.y], 'notifications':[str(w.get_reward_notification_text()) for w in widgets],
        'capture':unreal.MCPythonHelper.capture_play_viewport('reward_'+str(int(size.x))+'x'+str(int(size.y)))}
    Path(unreal.Paths.project_dir(),'Saved/Presentation/reward_'+str(int(size.x))+'.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
handle[0]=unreal.register_slate_post_tick_callback(capture)
print(json.dumps({'success':True,'interaction':result}))
