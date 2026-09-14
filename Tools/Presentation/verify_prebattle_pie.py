import unreal,json
from pathlib import Path
request_path=Path(unreal.Paths.project_dir(),'Saved/Presentation/fix_verification_request.json')
request=json.loads(request_path.read_text(encoding='utf-8-sig')) if request_path.exists() else {}
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
pc=unreal.GameplayStatics.get_player_controller(world,0)
pc.request_close_frontend_to_root();pc.request_open_pause_screen()
shell=next(w for w in unreal.WidgetLibrary.get_all_widgets_of_class(world,unreal.HSRFrontendShellWidget,False) if w.is_in_viewport())
shell.request_open_module(unreal.HSRFrontendModule.CHALLENGE)
w=next(w for w in unreal.WidgetLibrary.get_all_widgets_of_class(world,unreal.HSRChallengeDirectoryWidget,False) if w.is_visible())
rows=w.get_editor_property('EntryListContent').get_all_children()
button=rows[request.get('encounter_index',0)].get_editor_property('BTN_Select')
button.on_clicked.broadcast()
w.get_editor_property('BTN_Enter').on_clicked.broadcast()
print(json.dumps({'success':True,'selected':str(w.get_selected_encounter_id())}))
