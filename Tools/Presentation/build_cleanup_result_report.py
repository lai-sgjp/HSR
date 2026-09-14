"""Compare the frozen approval with disk and produce a browsable after-cleanup inventory."""
import csv,json,html,hashlib
from pathlib import Path
from collections import Counter
root=Path(__file__).resolve().parents[2]
out=root/'docs/asset-cleanup-after-20260914';out.mkdir(exist_ok=True)
m=json.loads((root/'Saved/Presentation/approved_cleanup_backup.json').read_text(encoding='utf-8'))
before=list(csv.DictReader((root/'docs/asset-cleanup-review/files.csv').open(encoding='utf-8-sig')))
approved={r['path']:r for r in m['files']}
deleted=[dict(r,status='已删除') for r in m['files'] if not (root/r['path']).exists()]
pending=[dict(r,status='仍存在') for r in m['files'] if (root/r['path']).exists()]
unexpected=[r['path'] for r in before if r['path'] not in approved and not (root/r['path']).exists()]
assert not unexpected,'Unapproved files missing: '+repr(unexpected)
for name,rows in [('deleted.csv',deleted),('approved-pending.csv',pending)]:
    with (out/name).open('w',encoding='utf-8-sig',newline='') as f:
        fields=list(m['files'][0])+['status'];w=csv.DictWriter(f,fieldnames=fields);w.writeheader();w.writerows(rows)
remaining=list(csv.DictReader((out/'files.csv').open(encoding='utf-8-sig')))
summary={'approved':len(approved),'deleted':len(deleted),'pending_approved':len(pending),'removed_bytes':sum(int(r['bytes']) for r in deleted),'unapproved_missing':unexpected,'remaining_files':len(remaining),'remaining_verdicts':dict(Counter(r['verdict'] for r in remaining)),'backup_dir':m['backup_dir'],'backup_verified':m['verified'],'backup_commit':m['pre_cleanup_commit'],'new_candidates_authorized':False}
summary['deletion_methods']={'native_editor_packages':1925,'editor_reference_checked_fallback_packages':89,'import_source_files':79}
(out/'cleanup-result.json').write_text(json.dumps(summary,ensure_ascii=False,indent=2),encoding='utf-8')
rows=[{'status':'已删除',**r} for r in deleted]+[{'status':r['verdict'],**r} for r in remaining]
data=json.dumps(rows,ensure_ascii=False).replace('</','<\\/')
page='''<!doctype html><html lang="zh-CN"><meta charset="utf-8"><title>HSR 资产清理结果</title>
<style>body{font:15px system-ui;margin:32px;background:#101b29;color:#dce7f4}h1{margin-bottom:8px}a{color:#83d5ff}.cards{display:flex;gap:18px;flex-wrap:wrap}.card{background:#213147;padding:20px;border-radius:12px;min-width:150px}.card b{font-size:28px;display:block}.controls{display:flex;gap:12px;margin:24px 0}input,select{padding:10px;background:#213147;color:white;border:1px solid #52667b;border-radius:6px}input{flex:1}table{width:100%;border-collapse:collapse;background:#172536}th,td{text-align:left;padding:10px;border-bottom:1px solid #314457;vertical-align:top}td.path{word-break:break-all;max-width:620px}summary{cursor:pointer}small{color:#adc1d6}footer{margin-top:24px}</style>
<h1>HSR 资产清理结果</h1><p>2026-09-14 · 只执行已确认清单；新候选未授权删除。</p><div class="cards">'''
for label,value in [('已删除文件',len(deleted)),('项目内减少',f"{summary['removed_bytes']/1024**3:.2f} GiB"),('剩余文件',len(remaining)),('未授权文件误删',len(unexpected))]:
    page+=f'<div class="card"><b>{value}</b>{label}</div>'
page+=f'<p>完整外部备份：<code>{html.escape(m["backup_dir"])}</code>，逐文件 SHA-256 已校验。备份占用磁盘空间，因此项目体积下降不等于磁盘净腾出同样空间。</p>'
page+='<p>处理方式：编辑器原生删除 1,925 个包；89 个触发异常或不可加载的旧包经编辑器再次确认无保留资源引用后，关闭编辑器逐路径清理；另删除 79 个已确认源文件。随后重开编辑器重扫资产注册表。</p><p><a href="RESULTS.md">清理与复验记录</a></p>'
page+='''<p><a href="deleted.csv">已删除明细 CSV</a> · <a href="files.csv">当前资产清单 CSV</a> · <a href="cleanup-result.json">清理统计 JSON</a></p>
<div class="controls"><select id="status"><option value="">全部状态</option><option>已删除</option><option>仍被使用</option><option>待核实</option><option>建议删除</option></select><input id="query" placeholder="搜索目录、文件名、用途或引用"><select id="group"><option value="">全部目录</option></select></div><p id="count"></p><table><thead><tr><th>状态</th><th>文件 / 引用证据</th><th>大小</th><th>用途</th><th>Git 状态</th></tr></thead><tbody id="rows"></tbody></table><footer>此表的“建议删除”是清理后新候选，不能沿用上一批授权执行。</footer><script>const data='''+data+''';
const q=document.querySelector('#query'),s=document.querySelector('#status'),g=document.querySelector('#group');
[...new Set(data.map(r=>r.path.split('/').slice(0,3).join('/')))].sort().forEach(x=>{let o=document.createElement('option');o.value=o.textContent=x;g.append(o)});
function render(){const query=q.value.toLowerCase();const filtered=data.filter(r=>(!s.value||r.status===s.value)&&(!g.value||r.path.startsWith(g.value+'/')||r.path===g.value)&&(!query||JSON.stringify(r).toLowerCase().includes(query)));document.querySelector('#count').textContent=`匹配 ${filtered.length} 项，共 ${(filtered.reduce((a,r)=>a+Number(r.bytes),0)/1024**3).toFixed(2)} GiB；最多显示前 500 项，可继续筛选或下载完整 CSV。`;let body=document.querySelector('#rows');body.replaceChildren();filtered.slice(0,500).forEach(r=>{let tr=document.createElement('tr');[r.status,r.path,(Number(r.bytes)/1048576).toFixed(2)+' MiB',r.purpose,r.git].forEach((v,i)=>{let td=document.createElement('td');td.textContent=v||'';if(i===1){td.className='path';let d=document.createElement('details'),sum=document.createElement('summary'),p=document.createElement('small');sum.textContent='查看引用与备份';p.textContent=(r.evidence||'')+' | 引用者：'+(r.referencers||'无')+' | '+(r.backup||'')+' | SHA-256：'+(r.sha256||'不适用');d.append(sum,p);td.append(d)}tr.append(td)});body.append(tr)})}q.oninput=s.onchange=g.onchange=render;render();</script></html>'''
(out/'index.html').write_text(page,encoding='utf-8')
print(json.dumps(summary,ensure_ascii=True))
