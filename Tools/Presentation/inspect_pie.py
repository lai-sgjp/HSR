import unreal,json
from pathlib import Path
world=unreal.EditorLevelLibrary.get_game_world()
if not world:raise RuntimeError('No PIE world')
pc=unreal.GameplayStatics.get_player_controller(world,0)
pawn=unreal.GameplayStatics.get_player_pawn(world,0)
result={'world':world.get_path_name(),'controller':str(pc),'pawn':str(pawn),'position':str(pawn.get_actor_location()) if pawn else None,'widgets':[]}
for w in unreal.WidgetLibrary.get_all_widgets_of_class(world,unreal.UserWidget,False):
 row={'class':w.get_class().get_name(),'visible':str(w.get_visibility())}
 for method in ['get_current_snapshot','get_entry_count']:
  if hasattr(w,method):
   try:row[method]=str(getattr(w,method)())
   except Exception as e:row[method]=str(e)
 result['widgets'].append(row)
Path(unreal.Paths.project_dir(),'Saved/Presentation/pie_state.json').write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding='utf-8')
unreal.SystemLibrary.execute_console_command(world,'Shot showui')
print(json.dumps(result,ensure_ascii=False))
