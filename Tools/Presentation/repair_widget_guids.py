import unreal,json
count=0
for path in unreal.EditorAssetLibrary.list_assets('/Game/UI'):
 bp=unreal.load_asset(path)
 if not isinstance(bp,unreal.WidgetBlueprint):continue
 info=json.loads(unreal.MCPythonHelper.umg_get_widget_info(bp))
 for w in info['widgets']:
  # The helper now supplies UWidgetBlueprint's required persistent variable GUID.
  conflicts=bp.get_name()=='WBP_AttributeDebug' and w['name'].lower() in ['text_health','text_maxhealth','text_energy','text_maxenergy','text_speed']
  unreal.MCPythonHelper.umg_set_widget_is_variable(bp,w['name'],not conflicts)
 unreal.BlueprintEditorLibrary.compile_blueprint(bp);unreal.EditorAssetLibrary.save_loaded_asset(bp);count+=1
print(json.dumps({'success':True,'widget_blueprints':count}))
