import unreal,json
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
rows=[]
for w in unreal.WidgetLibrary.get_all_widgets_of_class(world,unreal.HSRInventoryModuleWidget,False):
 if not w.get_entry_count():continue
 row={'path':w.get_path_name(),'catalog':str(w.get_editor_property('catalog')),'entry':str(w.get_entry(0)),'widgets':[]}
 for name in ['NavBox','TXT_Title','BTN_Back','BTN_Close','BTN_NextCharacter','IMG_DetailIcon']:
  try:x=w.get_editor_property(name)
  except Exception:x=None
  row['widgets'].append({'name':name,'object':str(x),'visible':str(x.get_visibility()) if x else None,'parent':str(x.get_parent()) if x else None})
 rows.append(row)
cat=unreal.load_asset('/Game/Data/VerticalSlice/Items/DA_InventoryCatalog_VerticalSlice')
print(json.dumps({'live':rows,'authored':str(cat.get_editor_property('entries'))}))
