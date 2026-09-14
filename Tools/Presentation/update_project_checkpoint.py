from pathlib import Path
root=Path(__file__).resolve().parents[2]
intro='''## 2026-09-12 地图、UI 与可玩体验重做

当前用户授权代理制作三张正式地图、Blender/FBX、UMG 与必要代码修复，替代下方历史“用户手工创建”的分工。三张地图与内容、背包、武器/遗器装备、强化、编队、战前准备和战斗表现已实现。

修复任务存档规范化恢复、跨重启重复遭遇奖励、较早存档回滚、部分队员死亡后回合停滞。编辑器/Windows Development 构建、最终打包通过；74 项相关 Automation 全通过。七个模块和五个角色页签已进行 720p/1080p/1440p 检查。

独立包隐藏窗口实测 1920×1080：60 秒固定路线平均 327.6 FPS，P95 3.89 ms，8 个路径点；CSV GPU 平均约 2.35 ms。整卡显存峰值 2124 MiB，包含系统与其他应用。该测试不含可见窗口呈现延迟，首次路线存在新 PSO 编译等待。

边界：首次玩家 10–15 分钟节奏尚未实测；风格化模块美术精细度低于商业参考。没有提交或推送，原有八个 VRM4U DLL 修改单独保留。

当前合同与记录：`docs/presentation-rebuild-execution.md`；交付索引：`Saved/Presentation/delivery.html`。

以下为历史记录。

'''
for name in ['PROJECT_STATE.md','todo_plan.md','worklog.md']:
 p=root/name;text=p.read_text(encoding='utf-8-sig')
 # Keep historical sections instead of rewriting their past outcomes.
 first,rest=text.split('\n',1)
 if '## 2026-09-12 地图、UI 与可玩体验重做' not in text:text=first+'\n\n'+intro+rest.lstrip('\n')
 if name=='PROJECT_STATE.md':text=text.replace('> 最后更新：2026-08-11','> 历史记录日期：2026-08-11')
 p.write_text(text,encoding='utf-8')
p=root/'tasks/active-task.md';text=p.read_text(encoding='utf-8-sig')
start=text.index('Execution contract and evidence:');end=text.index('\n\n---',start)
text=text[:start]+'''Execution contract and evidence: `docs/presentation-rebuild-execution.md`. Latest editor build and Windows Development package passed. All 74 related Automation tests passed on the final native version (10:17:31 UTC). Inventory/relic/party/pre-battle, both quest orders, single/multiple/boss battles, real save reload and restart reward replay have passed their recorded checks. 720p/1080p/1440p routes are captured. Standalone hidden-window 1080p fixed route measured 327.6 FPS / P95 3.89 ms; whole-adapter memory peak 2124 MiB. Final artifact collation is underway. First-play human pacing and visible-window presentation latency remain unmeasured.'''+text[end:]
p.write_text(text,encoding='utf-8')
print('Updated project checkpoints without removing historical records.')
