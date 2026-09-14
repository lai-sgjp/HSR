import unreal,json
from pathlib import Path
w=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
rows=unreal.GameplayStatics.get_all_actors_of_class(w,unreal.HSREnemyCharacter)
report={'world':w.get_path_name(),'patrol_count':len(rows),'player':str(unreal.GameplayStatics.get_player_pawn(w,0)),'capture':unreal.MCPythonHelper.capture_play_viewport('patrol_victory_return')}
Path(unreal.Paths.project_dir(),'Saved/Presentation/patrol_return_pie.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
print(json.dumps(report))
