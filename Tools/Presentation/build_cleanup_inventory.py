"""Read-only filesystem + exported AssetRegistry inventory; never delete assets.

Roots include executable source/config references and the three formal maps.
Authoring scripts and tests are separate retention roots; read-only inventory
tools do not make everything they inspect a gameplay dependency.
"""
import csv,json,re,subprocess,hashlib
from pathlib import Path
from collections import defaultdict,Counter

root=Path(__file__).resolve().parents[2]
graph=json.loads((root/'Saved/Presentation/asset_dependency_graph.json').read_text(encoding='utf-8'))
out=root/'docs/asset-cleanup-review';out.mkdir(exist_ok=True)
def git(*args):
    return subprocess.check_output(['git','-c','core.quotePath=false',*args],cwd=root,stderr=subprocess.DEVNULL).decode('utf-8').splitlines()
tracked=set(git('ls-files','Content'))
ignored=set(git('ls-files','--others','--ignored','--exclude-standard','Content'))
runtime=defaultdict(list);support=defaultdict(list)
for folder in ['Source','Config','Tools']:
    for path in (root/folder).rglob('*'):
        if not path.is_file() or path.suffix.lower() not in {'.cpp','.h','.ini','.py','.json'}: continue
        relative=path.relative_to(root).as_posix()
        if folder=='Tools' and path.name.startswith(('inspect_','audit_','capture_','build_cleanup_inventory','build_cleanup_report')): continue
        destination=support if folder=='Tools' or '/Tests/' in relative else runtime
        source=path.read_text(encoding='utf-8-sig',errors='replace')
        # Cook inclusion is a packaging constraint, not proof an asset is used.
        if folder=='Config':
            for line in source.splitlines():
                if 'DirectoriesToAlwaysCook' in line:
                    for prefix in re.findall(r'/Game/[A-Za-z0-9_/]+',line):
                        for package in graph:
                            if package.startswith(prefix+'/'): support[package].append(relative+' (cook-only inclusion; packaging must be reviewed)')
            source='\n'.join(line for line in source.splitlines() if 'DirectoriesToAlwaysCook' not in line)
        for token in re.findall(r'/Game/[A-Za-z0-9_\u4e00-\u9fff/.-]+',source):
            token=token.rstrip('./').split('.')[0]
            if token in graph: destination[token].append(relative)
            else:
                # Folder loads and dynamic suffix construction retain the entire matching prefix.
                for package in graph:
                    if package.startswith(token+'/'): destination[package].append(relative+' (dynamic folder)')
for name in ['Map_ObservationCar','Map_NewEriduSixthStreetMetro','Map_HertaSupportSection']:
    runtime['/Game/Maps/VerticalSlice/'+name].append('formal playable map')
def closure(roots):
    evidence={p:'; '.join(v[:4]) for p,v in roots.items()};todo=list(roots)
    while todo:
        parent=todo.pop()
        for child in graph.get(parent,{}).get('dependencies',[]):
            if child not in evidence:
                evidence[child]='dependency of '+parent;todo.append(child)
    return evidence
live=closure(runtime);aux=closure(support)
rows=[]
for path in sorted((root/'Content').rglob('*')):
    if not path.is_file():continue
    relative=path.relative_to(root).as_posix()
    package='/Game/'+path.relative_to(root/'Content').with_suffix('').as_posix()
    entry=graph.get(package)
    if package in live: verdict='仍被使用';risk='保留';evidence=live[package]
    elif package in aux: verdict='待核实';risk='中';evidence='test / authoring dependency: '+aux[package]
    elif not entry: verdict='待核实';risk='高';evidence='no AssetRegistry package record / sidecar; manual verification required'
    else: verdict='建议删除';risk='中';evidence='outside conservative runtime, tests and authoring dependency closure'
    state='tracked' if relative in tracked else 'ignored' if relative in ignored else 'untracked'
    group='/'.join(relative.split('/')[:3])
    refs=entry.get('referencers',[]) if entry else []
    kind=entry.get('class','unknown') if entry else 'unknown'
    purpose=('地图 / 场景' if kind=='World' else '角色 / 动作 / 重定向' if any(s in kind for s in ['Anim','Skeleton','Skeletal','IKRig','IKRetarget']) else '材质 / 纹理' if any(s in kind for s in ['Material','Texture']) else '模型 / 几何' if kind=='StaticMesh' else '蓝图 / 数据 / 其他资源')
    digest=hashlib.sha256(path.read_bytes()).hexdigest() if verdict=='建议删除' else ''
    rows.append({'verdict':verdict,'group':group,'path':relative,'bytes':path.stat().st_size,'class':kind,'purpose':purpose,'sha256':digest,
        'evidence':evidence,'referencers':'; '.join(refs),'git':state,'backup':'verified pushed commit + local archive' if state=='tracked' else 'external local archive + SHA-256 manifest required',
        'risk':risk,'approved':'NO'})
with (out/'files.csv').open('w',encoding='utf-8-sig',newline='') as f:
    writer=csv.DictWriter(f,fieldnames=list(rows[0]));writer.writeheader();writer.writerows(rows)
groups=defaultdict(lambda:Counter())
for row in rows:
    c=groups[(row['verdict'],row['group'])];c['count']+=1;c['bytes']+=row['bytes'];c[row['git']]+=1
backup_commit=git('rev-parse','HEAD')[0]
summary=['# 资产删除候选清单（尚未授权删除）','',f'生成清单时本地提交：`{backup_commit}`。Git 状态按本次生成时重新读取；推送是否一致以交付中的远端核验为准。',
    '', '此表是保守依赖审计，不表示已经删除或完成外部备份。逐文件路径、引用者、大小、Git 状态和备份要求见 [files.csv](files.csv)。',
    '', '依赖包含硬引用、软引用和 Asset Manager 管理引用；源码和配置动态目录保守保留。测试/制作脚本独立列为待核实。未登记文件不自动判删。',
    '', '只有明确确认的 files.csv 具体行才能进入 Editor 引用感知删除；候选中存在互相引用时应按依赖组处理，删除前再次核对当前引用。',
    '', '|结论|目录|文件数|MiB|tracked / ignored / untracked|','|---|---|---:|---:|---|']
for (verdict,group),c in sorted(groups.items()):
    summary.append(f"|{verdict}|{group}|{c['count']}|{c['bytes']/1048576:.2f}|{c['tracked']} / {c['ignored']} / {c['untracked']}|")
summary+=['','项目外备份尚未执行；待确认后对非 Git 文件建立独立归档及逐文件 SHA-256 清单，校验完成后才删除。缓存、插件、源码、私人文件均不在清理范围。',
    '', '四人模型、FINAL 移动动画、LIVE 战斗动画、FULL 倒地和 CAST 施法动画、骨架、重定向器及其依赖保留。旧地图或资源包只因名字陈旧不会被判删。',
    '', '候选行附 SHA-256，确认只适用于该内容版本；后续发生变化的文件需重新核对。']
(out/'README.md').write_text('\n'.join(summary)+'\n',encoding='utf-8')
(out/'summary.json').write_text(json.dumps({'inventory_commit':backup_commit,'files':len(rows),'verdicts':dict(Counter(r['verdict'] for r in rows)),'registry_packages':len(graph),'deletion_authorized':False,'external_backup_completed':False},ensure_ascii=False,indent=2),encoding='utf-8')
print((out/'README.md').as_posix());print(Counter(r['verdict'] for r in rows))
