"""Exercise the visible inventory's public intent methods against real PIE subsystems."""
import unreal,json
from pathlib import Path
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
widgets=unreal.WidgetLibrary.get_all_widgets_of_class(world,unreal.HSRInventoryModuleWidget,False)
w=next(x for x in widgets if x.get_entry_count()>0)
result={'world':world.get_path_name(),'steps':[]}
def record(label,value):result['steps'].append({'step':label,'result':str(value),'snapshot':str(w.get_current_snapshot())})
record('select weapon',w.select_category(unreal.HSRInventoryCategory.WEAPON))
record('select first weapon',w.select_entry_by_index(0))
record('preview equip',w.preview_action(unreal.HSRInventoryAction.EQUIP))
record('confirm equip',w.confirm_action())
record('duplicate confirm',w.confirm_action())
record('select relic',w.select_category(unreal.HSRInventoryCategory.RELIC))
record('select first relic',w.select_entry_by_index(0))
record('preview enhancement',w.preview_action(unreal.HSRInventoryAction.ENHANCE,1))
record('confirm enhancement',w.confirm_action())
record('duplicate enhancement confirm',w.confirm_action())
record('preview relic equip',w.preview_action(unreal.HSRInventoryAction.EQUIP))
record('confirm relic equip',w.confirm_action())
Path(unreal.Paths.project_dir(),'Saved/Presentation/inventory_pie_validation.json').write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding='utf-8')
unreal.SystemLibrary.execute_console_command(world,'Shot showui')
print(json.dumps({'success':True,'steps':[{k:v for k,v in row.items() if k!='snapshot'} for row in result['steps']]}))
