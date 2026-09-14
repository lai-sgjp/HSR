import unreal,json
from pathlib import Path
out={}
for name in ['WBP_HSRPreBattlePanel_P17','WBP_HSRChallengeDirectory_P17']:
 bp=unreal.load_asset('/Game/UI/P17/Frontend/'+name)
 out[name]=json.loads(unreal.MCPythonHelper.get_blueprint_graph_info(bp,'EventGraph'))
Path(unreal.Paths.project_dir(),'Saved/Presentation/prebattle_graphs.json').write_text(json.dumps(out,ensure_ascii=False,indent=2),encoding='utf-8')
print(json.dumps({'success':True}))
