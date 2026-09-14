import unreal,json
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
widget=next(w for w in unreal.WidgetLibrary.get_all_widgets_of_class(world,unreal.HSRBattleCommandWidget,False) if w.is_in_viewport())
print(json.dumps({'success':widget.confirm_battle_result()}))
