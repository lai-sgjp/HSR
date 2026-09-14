"""Collect local verification artifacts without modifying gameplay assets or saves."""
import csv,html,json,re,statistics,subprocess
from pathlib import Path
root=Path(__file__).resolve().parents[2]
out=root/'Saved/Presentation';perf=out/'Performance'
package=out/'Package/Windows/HSR/Saved'
route=json.loads((package/'Presentation/Performance/packaged_route.json').read_text(encoding='utf-8-sig'))
(perf/'packaged_route.json').write_text(json.dumps(route,indent=2),encoding='utf-8')
samples=json.loads((perf/'adapter_memory.json').read_text())['samples']
csvfile=max((package/'Profiling/CSV').glob('*.csv'),key=lambda p:p.stat().st_mtime)
with csvfile.open(encoding='utf-8-sig') as file:rows=list(csv.DictReader(file))
timings={}
for key in ['FrameTime','GameThreadTime','RenderThreadTime','GPUTime']:
 values=[]
 for row in rows:
  try:values.append(float(row[key]))
  except (ValueError,TypeError,KeyError):pass
 timings[key]=statistics.mean(values)
summary={**route,'csv_mean_ms':timings,'peak_adapter_memory_mib':max(x['used_mib'] for x in samples),'gpu':samples[0]['gpu'],
 'window':json.loads((perf/'window_dimensions.json').read_text()),
 'limitations':['Hidden standalone window, rendered at 1920x1080; no visible-window presentation-latency measurement.',
 'GPU memory is whole-adapter usage, including other applications.',
 'First route includes newly encountered PSO compilation; log records a 100 ms creation wait.',
 'No human first-play pacing measurement.']}
(perf/'summary.json').write_text(json.dumps(summary,indent=2),encoding='utf-8')
paths=subprocess.run(['git','-c','core.quotepath=false','status','--porcelain','-uall'],cwd=root,capture_output=True,text=True,encoding='utf-8',check=True).stdout.splitlines()
included=[];excluded=[]
for row in paths:
 name=row[3:]
 if name.startswith('Plugins/VRM4U/Binaries/') or name.endswith('.blend1'):excluded.append(row)
 else:included.append(row)
(out/'change_manifest.json').write_text(json.dumps({'included':included,'excluded_preexisting_or_backup':excluded,'committed':False,'pushed':False},ensure_ascii=False,indent=2),encoding='utf-8')
log=(out/'automation_final_74.log').read_text(encoding='utf-8-sig',errors='replace')
passed=len(re.findall(r'Test Completed\. Result=\{Success\}',log));failed=len(re.findall(r'Test Completed\. Result=\{Fail\}',log))
captures=sorted((out/'Captures').glob('*.png'))
views=sorted((out/'MapViews').glob('*.png'))
cards=[]
preferred=[p for p in captures if '2560x1440' in p.name or p.stem=='DIALOGUE' or p.stem.startswith('PACKAGED_')]
for p in views+preferred:
 cards.append(f'<figure><a href="{html.escape(p.as_uri())}"><img loading="lazy" src="{html.escape(p.as_uri())}" alt="{html.escape(p.stem)}"></a><figcaption>{html.escape(p.stem)}</figcaption></figure>')
page='''<!doctype html><html lang="zh-CN"><meta charset="utf-8"><title>HSR 重做 · 交付与验证</title><style>body{margin:32px;background:#101b29;color:#e7edf4;font:17px/1.7 system-ui}h1{color:#ddbf79}a{color:#80d4de}.grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(470px,1fr));gap:20px}figure{margin:0;padding:12px;background:#203246;border:1px solid #3e5266;border-radius:8px}img{width:100%;height:auto}p{max-width:1000px}code{color:#d7bd7b}</style><h1>地图、UI 与可玩体验重做</h1><p>三张正式地图、模块化模型、既有功能 UI 与装备/遗器交互修复。点击图片查看原始截图；截图来自 UE 场景或 PIE 视口。</p>'''
page+=f'<p>相关 Automation：{passed} 通过 / {failed} 失败。Windows Development BuildCookRun 成功。独立包隐藏窗口 1920×1080 固定路线：{route["average_fps"]:.1f} FPS，P95 {route["p95_frame_ms"]:.2f} ms；GPU 真实渲染平均 {timings["GPUTime"]:.2f} ms。</p>'
page+='<p>边界：首次玩家 10–15 分钟节奏未实测；当前为模块化风格化美术，精细度低于商业参考。显存为整张显卡用量，隐藏窗口测量不包含可见窗口呈现延迟。未提交、未推送；原有 VRM4U DLL 修改单独排除。</p>'
page+=f'<p><a href="{(root/"docs/presentation-rebuild-execution.md").as_uri()}">实现与验证记录</a> · <a href="{(root/"ArtSource/Presentation/README.md").as_uri()}">模型源文件说明</a> · <a href="{(out/"change_manifest.json").as_uri()}">精确修改清单</a> · <a href="{(perf/"summary.json").as_uri()}">性能记录</a></p><div class="grid">'+''.join(cards)+'</div></html>'
(out/'delivery.html').write_text(page,encoding='utf-8')
print(json.dumps({'tests_passed':passed,'tests_failed':failed,'average_fps':route['average_fps'],'screenshots':len(cards),'included_files':len(included),'excluded_files':len(excluded)}))
