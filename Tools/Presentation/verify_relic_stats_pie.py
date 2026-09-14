"""Exercise real relic widget transactions, comparing the live character sheet and ASC mirror."""
import unreal,json
from pathlib import Path
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
pc=unreal.GameplayStatics.get_player_controller(world,0)
pc.request_open_character_detail_screen()
shell=next(w for w in unreal.WidgetLibrary.get_all_widgets_of_class(world,unreal.HSRCharacterShellWidget,False) if w.is_visible())
report=[]
def stats():
    detail=shell.get_current_snapshot().character_detail
    vm=unreal.GameplayStatics.get_player_pawn(world,0).get_attribute_view_model()
    return {'id':str(detail.character_id),'hp':detail.derived_stats.max_health,'attack':detail.derived_stats.attack,'defense':detail.derived_stats.defense,'revision':detail.equipment_revision,
        'pawn_hp':vm.health,'pawn_max_hp':vm.max_health}
for name in ['Huohua','Remiel']:
    shell.select_character('Demo.Character.'+name)
    shell.select_tab(unreal.HSRCharacterShellTab.RELICS)
    relic=next(w for w in unreal.WidgetLibrary.get_all_widgets_of_class(world,unreal.HSRRelicEquipmentWidget,False) if w.is_visible())
    for slot in [unreal.HSRRelicSlot.HEAD,unreal.HSRRelicSlot.HANDS,unreal.HSRRelicSlot.BODY,unreal.HSRRelicSlot.FEET,unreal.HSRRelicSlot.PLANAR_SPHERE,unreal.HSRRelicSlot.LINK_ROPE]:
        relic.select_slot(slot)
        candidates=relic.get_current_snapshot().candidates
        if not candidates: continue
        before=stats()
        relic.select_candidate(candidates[0].instance_id)
        result=str(relic.commit_selected_movement())
        equipped=stats()
        relic.open_enhancement()
        options=[o for o in relic.get_current_snapshot().enhancement_options if o.available and o.affordable]
        enhancement=str(relic.commit_enhancement(options[0].target_level)) if options else 'no affordable option'
        enhanced=stats()
        relic.select_slot(slot)
        unequip=str(relic.unequip_selected_slot())
        report.append({'character':name,'slot':str(slot),'candidate':str(candidates[0].instance),'before':before,'equip':result,'equipped':equipped,
            'enhance':enhancement,'enhanced':enhanced,'unequip':unequip,'after':stats()})
        break
Path(unreal.Paths.project_dir(),'Saved/Presentation/relic_stats_pie.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
print(json.dumps({'success':bool(report),'cases':report},ensure_ascii=False))
