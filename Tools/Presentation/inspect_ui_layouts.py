import unreal,json
from pathlib import Path
out={}
for file in Path(unreal.Paths.project_dir(),'Content/UI').rglob('*.uasset'):
 path='/Game/UI/'+file.relative_to(Path(unreal.Paths.project_dir(),'Content/UI')).with_suffix('').as_posix()
 bp=unreal.load_asset(path)
 if not isinstance(bp,unreal.WidgetBlueprint):continue
 out[path]=json.loads(unreal.MCPythonHelper.umg_get_widget_info(bp))
Path(unreal.Paths.project_dir(),'Saved/Presentation/ui_layouts.json').write_text(json.dumps(out,ensure_ascii=False,indent=2),encoding='utf-8')
print(json.dumps({'success':True,'count':len(out)}))
