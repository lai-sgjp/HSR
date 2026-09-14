"""Revalidate the exact remaining approval set after unstable legacy asset deletion."""
import json,unreal
from pathlib import Path
root=Path(unreal.Paths.project_dir()).resolve()
m=json.loads((root/'Saved/Presentation/approved_cleanup_backup.json').read_text(encoding='utf-8'))
registry=unreal.AssetRegistryHelpers.get_asset_registry()
options=unreal.AssetRegistryDependencyOptions(include_soft_package_references=True,include_hard_package_references=True,include_soft_management_references=True,include_hard_management_references=True)
rows={('/Game/'+Path(r['path']).relative_to('Content').with_suffix('').as_posix()):r for r in m['files'] if Path(r['path']).suffix in {'.umap','.uasset'}}
remaining={p:r for p,r in rows.items() if (root/r['path']).exists()}
outside={}
for p in remaining:
    refs=[str(x) for x in registry.get_referencers(p,options) if str(x).startswith('/Game/') and str(x) not in rows]
    if refs:outside[p]=refs
assert not outside,outside
report={'reference_checked_in_editor':True,'reason':'Native force-delete has crashed and stalls repeatedly on legacy resources; exact remaining approved package recovery','files':list(remaining.values())}
(root/'Saved/Presentation/cleanup_fallback_approved.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
print(json.dumps({'success':True,'remaining_packages':len(remaining),'outside_references':outside,'dirty_content_count':len(unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages())}))
