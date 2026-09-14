import unreal,json
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
w=next(x for x in unreal.WidgetLibrary.get_all_widgets_of_class(world,unreal.HSRBattleCommandWidget,False) if x.is_in_viewport())
size=unreal.WidgetLayoutLibrary.get_viewport_size(world)
name='RESULT_' if w.get_current_view_state().result_view_state.visible else 'BATTLE_'
file=unreal.MCPythonHelper.capture_play_viewport(name+str(int(size.x))+'x'+str(int(size.y)))
print(json.dumps({'success':True,'capture':file,'state':str(w.get_current_view_state())}))
