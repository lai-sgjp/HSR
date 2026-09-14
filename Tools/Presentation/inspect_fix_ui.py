import unreal, json
from pathlib import Path
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
pc = unreal.GameplayStatics.get_player_controller(world,0)
pc.request_open_character_detail_screen()
shell = next(w for w in unreal.WidgetLibrary.get_all_widgets_of_class(world,unreal.HSRCharacterShellWidget,False) if w.is_visible())
shell.select_tab(unreal.HSRCharacterShellTab.RELICS)
relic = next(w for w in unreal.WidgetLibrary.get_all_widgets_of_class(world,unreal.HSRRelicEquipmentWidget,False) if w.is_visible())
print(json.dumps({'shell':str(shell.get_current_snapshot()), 'relic':str(relic.get_current_snapshot()),
    'chests':[a.get_actor_label() for a in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.HSRRewardChest)]},ensure_ascii=False))
