import unreal,json
from pathlib import Path
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
pc=unreal.GameplayStatics.get_player_controller(world,0)
w=next(w for w in unreal.WidgetLibrary.get_all_widgets_of_class(world,unreal.HSRPreBattleCandidateWidget,False) if w.is_in_viewport())
unreal.MCPythonHelper.capture_play_viewport('PREBATTLE')
pc.request_back_screen()
rows={'removed':not w.is_in_viewport(),'challenge_visible':any(x.is_visible() for x in unreal.WidgetLibrary.get_all_widgets_of_class(world,unreal.HSRChallengeDirectoryWidget,False))}
Path(unreal.Paths.project_dir(),'Saved/Presentation/prebattle_back_validation.json').write_text(json.dumps(rows,indent=2),encoding='utf-8')
print(json.dumps({'success':all(rows.values()),**rows}))
