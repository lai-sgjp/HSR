import unreal,json
from pathlib import Path
wld=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
pc=unreal.GameplayStatics.get_player_controller(wld,0)
size=unreal.WidgetLayoutLibrary.get_viewport_size(wld);suffix='_'+str(int(size.x))+'x'+str(int(size.y))
pc.request_close_frontend_to_root();pc.request_open_character_detail_screen()
state={'time':0,'index':0,'phase':0,'results':[]};handle=[None]
tabs=['DETAIL','WEAPON','TRACES','RELICS','INFORMATION']
folder=Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))/'Saved/Presentation/UIAudit'
def tick(delta):
 state['time']+=delta
 if state['time']<1:return
 state['time']=0
 w=next((x for x in unreal.WidgetLibrary.get_all_widgets_of_class(wld,unreal.HSRCharacterShellWidget,False) if x.is_visible()),None)
 if not w:return
 if state['index']>=len(tabs):
  (folder/('character_tabs'+suffix+'.json')).write_text(json.dumps(state['results'],ensure_ascii=False,indent=2),encoding='utf-8');unreal.unregister_slate_post_tick_callback(handle[0]);return
 name=tabs[state['index']]
 if state['phase']==0:
  result=w.select_tab(getattr(unreal.HSRCharacterShellTab,name))
  state['results'].append({'tab':name,'result':str(result),'buttons':{n:w.get_editor_property(n).get_is_enabled() for n in ['DetailTabButton','WeaponTabButton','TracesTabButton','RelicsTabButton','InformationTabButton']}});state['phase']=1
 else:
  state['results'][-1]['capture']=unreal.MCPythonHelper.capture_play_viewport('CHARACTER_'+name+suffix);state['index']+=1;state['phase']=0
handle[0]=unreal.register_slate_post_tick_callback(tick)
print(json.dumps({'success':True,'started':True}))
