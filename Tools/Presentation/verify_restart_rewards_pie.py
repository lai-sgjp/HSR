import unreal,json,re
from pathlib import Path
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
pc=unreal.GameplayStatics.get_player_controller(world,0);pawn=unreal.GameplayStatics.get_player_pawn(world,0)
def snapshot(module,cls):
 pc.request_close_frontend_to_root();pc.request_open_pause_screen()
 shell=next(w for w in unreal.WidgetLibrary.get_all_widgets_of_class(world,unreal.HSRFrontendShellWidget,False) if w.is_in_viewport())
 shell.request_open_module(module)
 w=next(w for w in unreal.WidgetLibrary.get_all_widgets_of_class(world,cls,False) if w.is_visible())
 return re.sub(r'0x[0-9A-Fa-f]+','ADDRESS',str(w.get_current_snapshot()))
def state():return {'inventory':snapshot(unreal.HSRFrontendModule.INVENTORY,unreal.HSRInventoryModuleWidget),'quest':snapshot(unreal.HSRFrontendModule.QUEST,unreal.HSRQuestWidget)}
before=state();results=[]
for label in ['GAME_WangXiaYiTong','GAME_Inspector','GAME_Laigushi']:
 pc.request_close_frontend_to_root()
 target=next(a for a in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.Actor) if a.get_actor_label()==label)
 p=target.get_actor_location();p.x-=120;p.z+=110;pawn.set_actor_location(p,False,True)
 results.append({'label':label,'result':str(pawn.get_component_by_class(unreal.HSRInteractionComponent).try_interact())})
after=state();report={'unchanged':before==after,'steps':results,'before':before,'after':after}
Path(unreal.Paths.project_dir(),'Saved/Presentation/restart_reward_replay.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
print(json.dumps({'success':report['unchanged'],'steps':results}))
