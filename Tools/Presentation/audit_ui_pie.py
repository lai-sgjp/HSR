"""Capture each real module after opening it through the frontend route authority."""
import unreal,json
from pathlib import Path
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
pc=unreal.GameplayStatics.get_player_controller(world,0)
size=unreal.WidgetLayoutLibrary.get_viewport_size(world);suffix='_'+str(int(size.x))+'x'+str(int(size.y))
folder=Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))/'Saved/Presentation/UIAudit';folder.mkdir(exist_ok=True)
names=['CHARACTER','INVENTORY','PARTY','QUEST','MAP','SAVE','CHALLENGE']
state={'t':0,'index':0,'phase':0,'results':[]};handle=[None]
def tick(delta):
 state['t']+=delta
 if state['t']<1.2:return
 state['t']=0
 if state['index']>=len(names):
  (folder/('routes'+suffix+'.json')).write_text(json.dumps(state['results'],ensure_ascii=False,indent=2),encoding='utf-8')
  unreal.unregister_slate_post_tick_callback(handle[0]);return
 name=names[state['index']]
 if state['phase']==0:
  pc.request_close_frontend_to_root();pc.request_open_pause_screen()
  shell=next(w for w in unreal.WidgetLibrary.get_all_widgets_of_class(world,unreal.HSRFrontendShellWidget,False) if w.is_in_viewport())
  opened=shell.request_open_module(getattr(unreal.HSRFrontendModule,name))
  state['results'].append({'module':name,'open':opened});state['phase']=1
 else:
  state['results'][-1]['capture']=unreal.MCPythonHelper.capture_play_viewport(name+suffix)
  state['results'][-1]['widgets']=[{'class':w.get_class().get_name(),'visibility':str(w.get_visibility())} for w in unreal.WidgetLibrary.get_all_widgets_of_class(world,unreal.UserWidget,False) if w.is_visible()]
  state['index']+=1;state['phase']=0
handle[0]=unreal.register_slate_post_tick_callback(tick)
print(json.dumps({'success':True,'started':names}))
