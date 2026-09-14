"""Load only the newest save created by this task, preserving user slots."""
import unreal,json
from pathlib import Path
folder=Path(unreal.Paths.project_dir(),'Saved/SaveGames')
slot=max(folder.glob('PlayableFixQA_*.sav'),key=lambda p:p.stat().st_mtime).stem
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
pc=unreal.GameplayStatics.get_player_controller(world,0)
pc.request_close_frontend_to_root();pc.request_open_pause_screen()
frontend=next(w for w in unreal.WidgetLibrary.get_all_widgets_of_class(world,unreal.HSRFrontendShellWidget,False) if w.is_in_viewport())
frontend.request_open_module(unreal.HSRFrontendModule.SAVE)
save=next(w for w in unreal.WidgetLibrary.get_all_widgets_of_class(world,unreal.HSRSaveWidget,False) if w.is_visible())
print(json.dumps({'slot':slot,'load':str(save.request_load(slot))}))
