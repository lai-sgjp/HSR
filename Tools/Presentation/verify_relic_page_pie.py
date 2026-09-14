"""Exercise character -> relic page public intents with a QA inventory."""
import unreal,json
from pathlib import Path
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
pc=unreal.GameplayStatics.get_player_controller(world,0)
pc.request_close_frontend_to_root();pc.request_open_character_detail_screen()
shell=next(w for w in unreal.WidgetLibrary.get_all_widgets_of_class(world,unreal.HSRCharacterShellWidget,False) if w.is_visible())
shell.select_tab(unreal.HSRCharacterShellTab.RELICS)
w=next(w for w in unreal.WidgetLibrary.get_all_widgets_of_class(world,unreal.HSRRelicEquipmentWidget,False) if w.is_visible())
report={'steps':[]}
def snapshot():
    s=w.get_current_snapshot()
    return s[0] if isinstance(s,tuple) else s
def step(name,value): report['steps'].append({'step':name,'result':str(value),'snapshot':str(w.get_current_snapshot())})
step('select planar sphere',w.select_slot(unreal.HSRRelicSlot.PLANAR_SPHERE))
s=snapshot()
if not s.candidates: raise RuntimeError('QA save must contain a planar sphere before this test')
step('preview candidate',w.select_candidate(s.candidates[0].instance_id))
before=str(w.get_current_snapshot())
step('cancel comparison',w.back())
step('preview candidate again',w.select_candidate(snapshot().candidates[0].instance_id))
step('equip sphere',w.commit_selected_movement())
step('open enhancement',w.open_enhancement())
step('cancel enhancement',w.back())
step('reopen enhancement',w.open_enhancement())
step('enhance level 2',w.commit_enhancement(2))
step('duplicate enhancement',w.commit_enhancement(2))
step('back to slot',w.back())
step('unequip sphere',w.unequip_selected_slot())
step('duplicate unequip',w.unequip_selected_slot())
Path(unreal.Paths.project_dir(),'Saved/Presentation/relic_page_validation.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
print(json.dumps({'success':True,'steps':[{k:v for k,v in r.items() if k!='snapshot'} for r in report['steps']]},ensure_ascii=False))
