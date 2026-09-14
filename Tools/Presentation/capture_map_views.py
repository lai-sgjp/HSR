"""Representative and overhead geometry views, separate from UI captures."""
import unreal,json
from pathlib import Path
out=Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))/'Saved/Presentation/MapViews'
out.mkdir(exist_ok=True)
views=[
 ('Map_ObservationCar','hub_overhead',(0,0,9000),(-90,0,0)),
 ('Map_ObservationCar','hub_lounge',(-1900,-200,260),(-4,20,0)),
 ('Map_ObservationCar','hub_observation',(1200,-200,250),(-3,25,0)),
 ('Map_NewEriduSixthStreetMetro','city_overhead',(0,0,34000),(-90,0,0)),
 ('Map_NewEriduSixthStreetMetro','city_plaza',(-2100,-4000,550),(-5,60,0)),
 ('Map_NewEriduSixthStreetMetro','city_terrace',(2000,8200,1000),(-12,-115,0)),
 ('Map_HertaSupportSection','arena_overhead',(0,0,9500),(-90,0,0)),
 ('Map_HertaSupportSection','arena_stands',(-2200,-2300,1000),(-12,45,0))]
request=Path(unreal.Paths.project_dir(),'Saved/Presentation/map_view_request.json')
if request.exists():
 requested=json.loads(request.read_text(encoding='utf-8-sig'))
 views=[view for view in views if view[1] in requested]
state={'i':0,'phase':0,'t':0};handle=[None]
def tick(delta):
 state['t']+=delta
 if state['t']<3:return
 state['t']=0
 if state['i']>=len(views):
  unreal.unregister_slate_post_tick_callback(handle[0]);return
 map_name,name,pos,rot=views[state['i']]
 if state['phase']==0:
  unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).load_level('/Game/Maps/VerticalSlice/'+map_name)
  unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).set_level_viewport_camera_info(unreal.Vector(*pos),unreal.Rotator(pitch=rot[0],yaw=rot[1],roll=rot[2]))
  state['phase']=1
 else:
  unreal.AutomationLibrary.take_high_res_screenshot(1920,1080,str(out/(name+'.png')))
  state['phase']=0;state['i']+=1
handle[0]=unreal.register_slate_post_tick_callback(tick)
print(json.dumps({'success':True,'scheduled':len(views)}))
