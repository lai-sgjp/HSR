import unreal,json
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
w=next(x for x in unreal.WidgetLibrary.get_all_widgets_of_class(world,unreal.HSRDialogueOverlayWidget,False) if x.is_in_viewport())
file=unreal.MCPythonHelper.capture_play_viewport('DIALOGUE')
print(json.dumps({'success':True,'file':file,'choices':w.get_choice_count(),'choice_result':str(w.submit_choice_by_index(0))}))
