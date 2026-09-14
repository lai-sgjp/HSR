import unreal,json
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
w=next(x for x in unreal.WidgetLibrary.get_all_widgets_of_class(world,unreal.HSRBattleCommandWidget,False) if x.is_in_viewport())
print(json.dumps({'success':w.confirm_battle_result()}))
