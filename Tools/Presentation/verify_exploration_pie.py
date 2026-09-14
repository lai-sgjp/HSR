"""Public interaction and UI intents; QA saves use their own slot and never replace user saves."""
import unreal,json
from pathlib import Path
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
pc=unreal.GameplayStatics.get_player_controller(world,0);pawn=unreal.GameplayStatics.get_player_pawn(world,0)
root=Path(unreal.Paths.project_dir(),'Saved/Presentation')
request=json.loads((root/'exploration_request.json').read_text(encoding='utf-8-sig'))
report={'world':world.get_path_name(),'steps':[]}
def interact(label):
 pc.request_close_frontend_to_root()
 target=next(a for a in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.Actor) if a.get_actor_label()==label)
 p=target.get_actor_location();p.x-=120;p.z+=110;pawn.set_actor_location(p,False,True)
 result=pawn.get_component_by_class(unreal.HSRInteractionComponent).try_interact()
 report['steps'].append({'interact':label,'result':str(result)})
def module(name,cls):
 pc.request_close_frontend_to_root();pc.request_open_pause_screen()
 shell=next(w for w in unreal.WidgetLibrary.get_all_widgets_of_class(world,unreal.HSRFrontendShellWidget,False) if w.is_in_viewport())
 shell.request_open_module(getattr(unreal.HSRFrontendModule,name))
 return next(w for w in unreal.WidgetLibrary.get_all_widgets_of_class(world,cls,False) if w.is_visible())
for label in request.get('interactions',[]):interact(label)
if request.get('quest'):
 report['quest']=str(module('QUEST',unreal.HSRQuestWidget).get_current_snapshot())
if request.get('inventory'):
 w=module('INVENTORY',unreal.HSRInventoryModuleWidget);report['inventory']=str(w.get_current_snapshot());report['entry_count']=w.get_entry_count()
if request.get('party'):
 w=module('PARTY',unreal.HSRPartyWidget)
 report['party_initial']=str(w.get_current_snapshot())
 report['clear']=str(w.clear_candidate_slot(3));report['cancel']=str(w.cancel_candidate())
 report['party_after_cancel']=str(w.get_current_snapshot())
 report['swap']=str(w.swap_candidate_slots(0,1));report['confirm']=str(w.confirm_candidate())
 report['party_after_confirm']=str(w.get_current_snapshot())
 report['swap_back']=str(w.swap_candidate_slots(0,1));report['restore_party']=str(w.confirm_candidate())
if request.get('save'):
 w=module('SAVE',unreal.HSRSaveWidget);slot=request['save']
 report['save']=str(w.request_save(slot));report['save_result']=str(w.get_current_result())
 report['summary']=str(w.get_slot_summary(slot))
if request.get('load'):
 w=module('SAVE',unreal.HSRSaveWidget);report['load']=str(w.request_load(request['load']))
if request.get('travel'):interact(request['travel'])
(root/request.get('report','exploration_validation.json')).write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
print(json.dumps({'success':True,'report':request.get('report','exploration_validation.json'),'steps':report['steps'],'save':report.get('save'),'load':report.get('load')},ensure_ascii=False))
