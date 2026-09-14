import unreal,json
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
cat=unreal.load_asset('/Game/Data/VerticalSlice/Items/DA_InventoryCatalog_VerticalSlice')
entries=list(cat.get_editor_property('entries'))
for index,entry in enumerate(entries):
 item=str(entry.get_editor_property('item_id'))
 key=next((k for k in ['Head','Hands','Body','Feet','PlanarSphere','LinkRope'] if item.endswith(k)),'Weapon' if 'EchoConductor' in item else 'Material')
 entry.set_editor_property('icon',unreal.load_asset('/Game/Presentation/UI/T_Item_'+key));entries[index]=entry
cat.set_editor_property('entries',entries);unreal.EditorAssetLibrary.save_loaded_asset(cat)
for w in unreal.WidgetLibrary.get_all_widgets_of_class(world,unreal.HSRInventoryModuleWidget,False):
 if not w.get_entry_count():continue
 w.get_editor_property('NavBox').set_visibility(unreal.SlateVisibility.VISIBLE)
 w.initialize_for_inventory(cat);w.select_entry_by_index(0)
unreal.SystemLibrary.execute_console_command(world,'Shot showui')
print(json.dumps({'success':True,'icons':[str(e.get_editor_property('icon')) for e in cat.get_editor_property('entries')]}))
