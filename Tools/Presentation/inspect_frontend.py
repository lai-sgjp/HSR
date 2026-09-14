import unreal,json
from pathlib import Path
out={}
for path in ['/Game/UI/P17/Frontend/WBP_FrontendModuleRoot_P17','/Game/UI/P17/Frontend/WBP_FrontendShell_P17']:
 out[path]=json.loads(unreal.MCPythonHelper.umg_get_widget_info(unreal.load_asset(path)))
Path(unreal.Paths.project_dir(),'Saved/Presentation/frontend_layouts.json').write_text(json.dumps(out,ensure_ascii=False,indent=2),encoding='utf-8')
print(json.dumps(out))
