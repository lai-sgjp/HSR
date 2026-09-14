"""Authorize a narrow on-disk fallback only for editor-proven deletion failures."""
import json,unreal
from pathlib import Path
root=Path(unreal.Paths.project_dir()).resolve()
m=json.loads((root/'Saved/Presentation/approved_cleanup_backup.json').read_text(encoding='utf-8'))
result=json.loads((root/'Saved/Presentation/cleanup_editor_result.json').read_text(encoding='utf-8'))
assert result['complete'],'Finish the normal editor pass first'
registry=unreal.AssetRegistryHelpers.get_asset_registry()
options=unreal.AssetRegistryDependencyOptions(include_soft_package_references=True,include_hard_package_references=True,include_soft_management_references=True,include_hard_management_references=True)
rows={('/Game/'+Path(r['path']).relative_to('Content').with_suffix('').as_posix()):r for r in m['files'] if Path(r['path']).suffix in {'.umap','.uasset'}}
remaining={p:r for p,r in rows.items() if (root/r['path']).exists()}
failures={r['package'] for r in result['failed']}
for p in remaining:
    assert p in failures,p
    assert p.startswith(('/Game/__ExternalActors__/ThirdPerson/Maps/ThirdPersonMap/','/Game/__ExternalObjects__/ThirdPerson/Maps/ThirdPersonMap/','/Game/Audio/Mix/')) or p.endswith('_BuiltData'),p
    refs=[str(x) for x in registry.get_referencers(p,options) if str(x).startswith('/Game/') and str(x) not in remaining]
    assert not refs,(p,refs)
report={'reference_checked_in_editor':True,'reason':'Unresolvable orphan external objects or reproducible editor audio deletion assertion','files':list(remaining.values())}
(root/'Saved/Presentation/cleanup_fallback_approved.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
print(json.dumps({'success':True,'fallback_files':len(remaining),'outside_references':0,'dirty_content':[p.get_path_name() for p in unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()]}))
