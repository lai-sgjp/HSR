"""Write the exact include/exclude review from the current allowlist; no Git mutation."""
import json,csv
from pathlib import Path
root=Path(__file__).resolve().parents[2]
d=json.loads((root/'Saved/Presentation/git_backup_audit.json').read_text(encoding='utf-8'))
folder=root/'docs/git-backup-review';folder.mkdir(exist_ok=True)
with (folder/'paths.csv').open('w',encoding='utf-8-sig',newline='') as f:
    writer=csv.DictWriter(f,fieldnames=['path','bytes','git','decision','reason']);writer.writeheader();writer.writerows(d['files'])
included=[r for r in d['files'] if r['decision']=='include']
text=f'''# 清理前 Git 备份审核

目标：`{d['remote']}`，分支 `{d['branch']}`。审核时已有待推送提交：{len(d['ahead'])}。

本次按明确路径纳入 {len(included)} 个文件，约 {sum(r['bytes'] for r in included)/1048576:.2f} MiB；排除 {len(d['files'])-len(included)} 个当前改动文件。逐文件清单见 [paths.csv](paths.csv)。此清单之外，Git 忽略文件仍遵守原忽略规则，不强制加入。

纳入此前尚未提交的正式场景与 UI 改造，以及本轮修复、原生插件辅助源码、自制场景源文件、CC0 动作源与文档。既有改动属于本次清理前备份范围，不冒称全部由本轮创作。

排除：未核实公开再分发依据的 MMD 衍生动画/肖像、8 个本地 VRM4U 二进制、Blender 恢复副本。角色定义引用这些本地资源，因此此 Git 提交不等于完整美术备份，也不能仅凭公开克隆直接还原所有角色。

已执行源码/配置 diff 检查、凭证样式扫描、构建与相关 Automation、三地图清理前 Cook。扫描未发现新增的所列凭证格式；这不代替原资源授权证明。实际提交哈希与推送验证将在交付消息中列明。

未执行任何清理删除。确认具体删除清单后，先在项目外建立本地归档与 SHA-256 清单并校验；Git 忽略资源不由此次 push 覆盖。
'''
(folder/'README.md').write_text(text,encoding='utf-8')
print(folder.as_posix())
