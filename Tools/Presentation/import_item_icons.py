import unreal,json
from pathlib import Path
root=Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
unreal.SystemLibrary.execute_console_command(None,'Interchange.FeatureFlags.Import.PNG 0')
textures={}
for source in (root/'ArtSource/Presentation/ItemIcons').glob('*.png'):
 task=unreal.AssetImportTask();task.set_editor_property('filename',str(source));task.set_editor_property('destination_path','/Game/Presentation/UI');task.set_editor_property('destination_name','T_Item_'+source.stem)
 task.set_editor_property('automated',True);task.set_editor_property('replace_existing',True);task.set_editor_property('save',True)
 task.set_editor_property('factory',unreal.TextureFactory())
 unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
 tex=unreal.load_asset('/Game/Presentation/UI/T_Item_'+source.stem)
 tex.set_editor_property('compression_settings',unreal.TextureCompressionSettings.TC_EDITOR_ICON)
 tex.set_editor_property('lod_group',unreal.TextureGroup.TEXTUREGROUP_UI)
 unreal.EditorAssetLibrary.save_loaded_asset(tex);textures[source.stem]=tex
catalog=unreal.load_asset('/Game/Data/VerticalSlice/Items/DA_InventoryCatalog_VerticalSlice')
catalog.modify()
entries=catalog.get_editor_property('entries')
for index,entry in enumerate(entries):
 item=str(entry.get_editor_property('item_id'))
 key=next((k for k in ['Head','Hands','Body','Feet','PlanarSphere','LinkRope'] if item.endswith(k)),'Weapon' if 'EchoConductor' in item else 'Material')
 entry.set_editor_property('icon',textures[key])
 entries[index]=entry
catalog.set_editor_property('entries',entries);unreal.EditorAssetLibrary.save_loaded_asset(catalog,only_if_is_dirty=False)
bp=unreal.load_asset('/Game/UI/P17/Inventory/WBP_Inventory_P17')
h=unreal.MCPythonHelper
if not h.umg_find_widget(bp,'IMG_DetailIcon'):
 h.umg_add_widget(bp,'Image','IMG_DetailIcon','RightVBox');h.umg_set_widget_is_variable(bp,'IMG_DetailIcon',True)
icon=h.umg_find_widget(bp,'IMG_DetailIcon');icon.set_desired_size_override(unreal.Vector2D(128,128))
brush=icon.get_editor_property('brush');size=brush.get_editor_property('image_size');size.set_editor_property('x',128);size.set_editor_property('y',128);brush.set_editor_property('image_size',size);icon.set_brush(brush)
icon.get_editor_property('slot').set_horizontal_alignment(unreal.HorizontalAlignment.H_ALIGN_LEFT)
unreal.BlueprintEditorLibrary.compile_blueprint(bp);unreal.EditorAssetLibrary.save_loaded_asset(bp)
print(json.dumps({'success':True,'icons':len(textures),'entries':len(entries)}))
