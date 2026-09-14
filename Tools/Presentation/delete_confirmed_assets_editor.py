"""Editor-only deletion of the frozen, backed-up, reference-checked approval set."""
import unreal,json,hashlib
from pathlib import Path
root=Path(unreal.Paths.project_dir()).resolve()
m=json.loads((root/'Saved/Presentation/approved_cleanup_backup.json').read_text(encoding='utf-8'))
check=json.loads((root/'Saved/Presentation/cleanup_reference_preflight.json').read_text(encoding='utf-8'))
assert m['verified'] and m['count']==2093 and not check['blocked_by_retained_references']
assert not unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world(),'Stop PIE first'
for command in ['Editor.AsyncAssetCompilation 0','Editor.AsyncTextureCompilation 0','Editor.AsyncStaticMeshCompilation 0','Editor.AsyncSkeletalMeshCompilation 0','Editor.AsyncSkinnedAssetCompilation 0']:
    unreal.SystemLibrary.execute_console_command(None,command)
rows={('/Game/'+Path(r['path']).relative_to('Content').with_suffix('').as_posix()):r for r in m['files'] if Path(r['path']).suffix.lower() in {'.uasset','.umap'}}
assert set(check['eligible'])==set(rows)
for r in rows.values():
    path=(root/r['path']).resolve()
    assert path.is_relative_to(root/'Content')
    if not path.exists():continue
    assert hashlib.sha256(path.read_bytes()).hexdigest()==r['sha256'],'Changed after approval: '+r['path']
registry=unreal.AssetRegistryHelpers.get_asset_registry()
opts=unreal.AssetRegistryDependencyOptions(include_soft_package_references=True,include_hard_package_references=True,include_soft_management_references=True,include_hard_management_references=True)
# Delete referencing packages before their dependencies. Cycles are still wholly
# inside the approved set and can be force-deleted together by the editor.
already=[p for p,r in rows.items() if not (root/r['path']).exists()]
deferred={p:'Orphan external package or audio-mixer deletion unsupported by this editor session' for p in rows if (p.startswith('/Game/__ExternalActors__/') or p.startswith('/Game/__ExternalObjects__/') or p.startswith('/Game/Audio/Mix/') or p.endswith('_BuiltData')) and p not in already}
pending=set(rows)-set(already)-set(deferred);ordered=[]
while pending:
    ready=[p for p in sorted(pending) if not any(str(r) in pending and str(r)!=p for r in registry.get_referencers(p,opts))]
    if not ready:ready=[sorted(pending)[0]]
    ordered.extend(ready);pending.difference_update(ready)
state={'approved_packages':len(rows),'done':already,'failed':[{'package':p,'reason':r} for p,r in deferred.items()],'complete':False}
handle=[None];index=[0];busy=[False]
def save():
    (root/'Saved/Presentation/cleanup_editor_result.json').write_text(json.dumps(state,ensure_ascii=False,indent=2),encoding='utf-8')
def tick(dt):
    # Loading/deleting assets pumps Slate through slow-task dialogs. Re-entering
    # this callback would delete UObjects while another package is still loading.
    if busy[0]:return
    busy[0]=True
    batch=ordered[index[0]:index[0]+5];index[0]+=len(batch)
    loaded=[]
    for p in batch:
        data=registry.get_assets_by_package_name(p)
        asset=data[0].get_asset() if data else None
        if asset:loaded.append(asset)
        else:state['failed'].append({'package':p,'reason':'Editor asset load failed'})
    if loaded:unreal.EditorAssetLibrary.delete_loaded_assets(loaded)
    loaded.clear();asset=None
    unreal.SystemLibrary.collect_garbage()
    for p in batch:
        if not (root/rows[p]['path']).exists():state['done'].append(p)
        elif not any(x['package']==p for x in state['failed']):state['failed'].append({'package':p,'reason':'Editor did not remove package'})
    unreal.log(f"HSR_CLEANUP {index[0]}/{len(ordered)} removed={len(state['done'])} failed={len(state['failed'])}")
    if index[0]>=len(ordered):
        unreal.unregister_slate_post_tick_callback(handle[0]);state['complete']=True
    save()
    busy[0]=False
save();handle[0]=unreal.register_slate_post_tick_callback(tick)
print(json.dumps({'success':True,'remaining':len(ordered),'already_removed':len(already),'batch':5}))
