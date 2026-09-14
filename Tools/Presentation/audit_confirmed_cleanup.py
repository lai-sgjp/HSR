"""Recheck current AssetRegistry references against the frozen approved set."""
import unreal,json
from pathlib import Path
root=Path(unreal.Paths.project_dir()).resolve()
m=json.loads((root/'Saved/Presentation/approved_cleanup_backup.json').read_text(encoding='utf-8'))
assert m['verified'] and m['count']==2093
registry=unreal.AssetRegistryHelpers.get_asset_registry()
registry.search_all_assets(True)
options=unreal.AssetRegistryDependencyOptions(include_soft_package_references=True,include_hard_package_references=True,include_searchable_names=False,include_soft_management_references=True,include_hard_management_references=True)
packages={'/Game/'+Path(r['path']).relative_to('Content').with_suffix('').as_posix():r for r in m['files'] if Path(r['path']).suffix.lower() in {'.uasset','.umap'}}
blocked={}
for p in packages:
    refs=[str(x) for x in registry.get_referencers(p,options) if str(x).startswith('/Game/') and str(x) not in packages]
    if refs:blocked[p]=refs
while True:
    more={p:[str(x) for x in registry.get_referencers(p,options) if str(x) in blocked] for p in packages if p not in blocked}
    more={p:r for p,r in more.items() if r}
    if not more:break
    blocked.update(more)
report={'approved_count':len(packages),'blocked_by_retained_references':blocked,'eligible':[p for p in packages if p not in blocked]}
(root/'Saved/Presentation/cleanup_reference_preflight.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
print(json.dumps({'success':True,'approved':len(packages),'eligible':len(report['eligible']),'blocked':len(blocked)}))
