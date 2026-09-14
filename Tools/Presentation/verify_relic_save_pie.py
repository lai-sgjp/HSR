import unreal,json
from pathlib import Path
from datetime import datetime
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
pc=unreal.GameplayStatics.get_player_controller(world,0)
pc.request_open_character_detail_screen()
shell=next(w for w in unreal.WidgetLibrary.get_all_widgets_of_class(world,unreal.HSRCharacterShellWidget,False) if w.is_visible())
shell.select_character('Demo.Character.Remiel');shell.select_tab(unreal.HSRCharacterShellTab.RELICS)
relic=next(w for w in unreal.WidgetLibrary.get_all_widgets_of_class(world,unreal.HSRRelicEquipmentWidget,False) if w.is_visible())
relic.select_slot(unreal.HSRRelicSlot.HEAD)
if relic.get_current_snapshot().slots[0].has_equipped: relic.unequip_selected_slot()
relic.select_candidate(relic.get_current_snapshot().candidates[0].instance_id)
equipped=str(relic.commit_selected_movement())
expected=shell.get_current_snapshot().character_detail.derived_stats.max_health
pc.request_close_frontend_to_root();pc.request_open_pause_screen()
frontend=next(w for w in unreal.WidgetLibrary.get_all_widgets_of_class(world,unreal.HSRFrontendShellWidget,False) if w.is_in_viewport())
frontend.request_open_module(unreal.HSRFrontendModule.SAVE)
save=next(w for w in unreal.WidgetLibrary.get_all_widgets_of_class(world,unreal.HSRSaveWidget,False) if w.is_visible())
slot='PlayableFixQA_'+datetime.now().strftime('%Y%m%d_%H%M%S')
saved=str(save.request_save(slot))
pc.request_close_frontend_to_root();pc.request_open_character_detail_screen()
shell=next(w for w in unreal.WidgetLibrary.get_all_widgets_of_class(world,unreal.HSRCharacterShellWidget,False) if w.is_visible())
shell.select_character('Demo.Character.Remiel');shell.select_tab(unreal.HSRCharacterShellTab.RELICS)
relic=next(w for w in unreal.WidgetLibrary.get_all_widgets_of_class(world,unreal.HSRRelicEquipmentWidget,False) if w.is_visible())
relic.select_slot(unreal.HSRRelicSlot.HEAD);removed=str(relic.unequip_selected_slot())
without=shell.get_current_snapshot().character_detail.derived_stats.max_health
pc.request_close_frontend_to_root();pc.request_open_pause_screen()
frontend=next(w for w in unreal.WidgetLibrary.get_all_widgets_of_class(world,unreal.HSRFrontendShellWidget,False) if w.is_in_viewport())
frontend.request_open_module(unreal.HSRFrontendModule.SAVE)
save=next(w for w in unreal.WidgetLibrary.get_all_widgets_of_class(world,unreal.HSRSaveWidget,False) if w.is_visible())
loaded=str(save.request_load(slot))
elapsed=[0.];handle=[None]
def verify(dt):
    elapsed[0]+=dt
    if elapsed[0]<2:return
    unreal.unregister_slate_post_tick_callback(handle[0])
    w=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
    controller=unreal.GameplayStatics.get_player_controller(w,0)
    controller.request_close_frontend_to_root();switched=controller.switch_exploration_character(1)
    controller.request_open_character_detail_screen()
    current=next(x for x in unreal.WidgetLibrary.get_all_widgets_of_class(w,unreal.HSRCharacterShellWidget,False) if x.is_visible())
    current.select_character('Demo.Character.Remiel')
    actual=current.get_current_snapshot().character_detail.derived_stats.max_health
    vm=unreal.GameplayStatics.get_player_pawn(w,0).get_attribute_view_model()
    report={'slot':slot,'equip':equipped,'save':saved,'unequip':removed,'load':loaded,'expected':expected,'without':without,'restored':actual,'switched':switched,'pawn_max_hp':vm.max_health,
        'passed':actual==expected and without<expected and vm.max_health==expected}
    Path(unreal.Paths.project_dir(),'Saved/Presentation/relic_save_pie.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
handle[0]=unreal.register_slate_post_tick_callback(verify)
print(json.dumps({'success':True,'save':saved,'load':loaded}))
