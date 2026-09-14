import unreal,json
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
w=next(w for w in unreal.WidgetLibrary.get_all_widgets_of_class(world,unreal.HSRPreBattleCandidateWidget,False) if w.is_in_viewport())
size=unreal.WidgetLayoutLibrary.get_viewport_size(world)
file=unreal.MCPythonHelper.capture_play_viewport('PREBATTLE_'+str(int(size.x))+'x'+str(int(size.y)))
w.get_editor_property('Button_Confirm').on_clicked.broadcast()
print(json.dumps({'success':True,'capture':file,'confirm_clicked':True}))
