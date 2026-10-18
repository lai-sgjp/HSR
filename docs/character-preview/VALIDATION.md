# Character Preview：验收与交付记录

日期：2026-10-18（Asia/Shanghai）。结论：**预览功能及本机 Development Cook/Package、包内运行验证完成；最终视觉验收仍有缺陷项。** 用户已明确批准四个本地模型仅进入本机 Development 验证包，未授权上传或分发。

## A. 完成结论

真实 `Map_ObservationCar` → 原角色界面 → 四角色实时 3D → 待机、鼠标左拖、滚轮、重置、返回/关闭已经接通并实际验证。预览不生成 Character/Pawn/ASC，不改变角色、装备、队伍或 v9 存档协议。Editor 和非 Editor Development 游戏目标均已编译通过；游戏目标编译不等于包内运行通过。

三地图 Cook/Package 已成功，实际包内 69 项原生 API/渲染/生命周期检查全部通过，另已执行前台 OS 鼠标操作。仍待最终用户人工接受；用户指出火花脚部不自然，作为紧接此交付的修复项。部分旧 UI 字形切换后偶发缺片，详见视觉限制。没有发布或上传资产/包，没有推送远端。

## B. 实际架构

详见 [ARCHITECTURE.md](ARCHITECTURE.md) 和 [面试讲解](../../CHARACTER_PREVIEW_INTERVIEW.md)。原 `CharacterShellViewModel` 的选择 ID → Widget-owned Session → 只读 Profile/Definition → StreamableManager → 轻量 Stage → ShowOnly SceneCapture2D → transient BGRA8 RT → Opaque UI Material → UMG。

Stage 在同一 World 中，通过捕获专见 Mesh、ShowOnly 和灯光通道 2 隔离，不依赖离屏位置。当前实际探索地图的非预览 Primitive 未占通道 2（`Saved/CharacterPreview/lighting_channel_audit.json`）。这不是所有未来地图、透明材质和其他 SceneCapture 的全局隔离承诺；扩展时需重新审计。

Session 的 UPROPERTY 引用维持 Stage、RT、加载期定义；组件持有 Mesh/Idle；World/Profile 为弱引用。Generation、取消 handle、弱异步委托、关闭移除 ticker，以及 WorldCleanup 共同防止过期加载继续安装资源。动画用实时时间无 Notify 采样，适配已有菜单的 World 暂停。

## C. 文件变更

精确路径见 [FILES.md](FILES.md)。改动范围只有：原角色定义展示字段、原角色壳接入、3 个预览类、专项测试、原 WBP、原 4 个角色 DA、1 个 UI Material、QA/接线脚本和文档。

没有持久化 RenderTarget `.uasset`：RT 是运行时创建并在会话内复用的 transient UObject。没有新地图、新角色账本、新 Subsystem、第三方插件、Runtime editor-only 模块依赖或存档协议修改。原始角色 Mesh/动画、已有地图未改动。

开始时已有的 8 个 VRM4U DLL 被备份并排除。其 SHA-256 与开始时一致，见 `Saved/CharacterPreview/baseline-plugin-hashes.json`；该证据检查不等于将 DLL 作为本任务提交。

## D. 验收证据

### 构建、资产与回归

| 项目 | 结果 | 方法与证据 |
|---|---|---|
| UE/工具链 | Pass | UE5.6.1、MSVC14.51.36252、SDK10.0.22621.0；实际构建日志。MSVC 有引擎的 non-preferred-version 警告，不阻止构建。 |
| HSREditor Development | Pass | `Saved/CharacterPreview/build-editor-final.log`，Result: Succeeded。 |
| HSR Development Game | Pass | `Saved/CharacterPreview/build-game-final.log`，Result: Succeeded，无 UnrealEd/AdvancedPreviewScene 依赖。 |
| 资产接线、compile/save、重新启动 Editor/PIE 加载 | Pass | `asset_wiring.json`，原资产备份位于 `BeforeAssets/`，最终截图来自重启后的 Editor。 |
| 预览/角色壳/导航/装备/队伍/存档相关 Automation | Pass | 47/47，`automation-final.json`，完整日志 `Saved/Logs/CharacterPreviewEditorR3.log`。 |
| 首次 Capture 顺序最终复验 | Pass | 4/4 预览专项，`automation-preview-delivery.json`；是同一组测试的复验，不重复累计测试数量。 |
| 战斗结算、GAS 治疗、存档 Envelope 回归 | Pass | 8/8，`core-regression.json`，完整日志 `Saved/Logs/CharacterPreviewEditorFinal.log`。合计 55 个不同测试通过，来自两个相关验证轮次。 |
| 三地图 Cook/Package | Pass（仅本机） | UAT BuildCookRun：build/cook/stage/pak/iostore/archive，1237 packages，ExitCode=0；`Saved/CharacterPreview/package-development-final.log`。 |
| 包内非 Editor 角色预览 | Pass（技术检查） | 运行 `Saved/CharacterPreview/Package/Windows/HSR/Binaries/Win64/HSR.exe`；原生 69/69 检查、四角色 RT/UI PNG、20 次开关，`PackagedEvidence/probe-report.json`。 |

### P0 必须项

| 验收项 | 结果 | 实际方法与观察 |
|---|---|---|
| 从真实 UI 入口打开 | Pass | `RequestOpenCharacterDetailScreen` 走现有 UIManager/CharacterShell；无测试页面替代生产入口。 |
| 已有真实角色模型显示 | Pass | 4 个原 DA/mesh/idle，截图 `Saved/Presentation/Captures/PREVIEW_character_0.png`～`3.png`。 |
| 实时 3D、不是静态图片 | Pass | 暂停 World 时持续捕获；固定相机的两张原始 RT 图有 24,048 像素变化，见 `idle-frame-difference.json`，另有实际旋转截图。 |
| 不错误捕获大世界 | Pass | 截图为独立黑色背景，仅 ShowOnly Mesh；专项测试检查过滤列表。 |
| 不干扰真实角色/战斗数据 | Pass（所测范围） | 角色/装备完整值快照（移除 Python wrapper 地址后）不变，Pawn 身份与 Transform 不变；无 ASC 的 Stage 检查及业务回归通过。 |
| 关闭后停止捕获 | Pass | Ready 开关 20 轮、Loading 关闭、真实 Esc/X、地图重载均检查 Closed/零 Stage；关闭性能样本捕获数为 0。 |
| 现有核心功能回归 | Pass（自动化范围） | 47+8 项相关回归通过；不是整个游戏所有内容的人工全流程验收。 |

### P1 必须项

| 验收项 | 结果 | 方法与观察 |
|---|---|---|
| 4 个不同已有角色切换 | Pass | 真实 roster 和 ID → DA → 独立 Mesh；不要求对应 Pawn 存在。 |
| 鼠标左拖/滚轮/重置 | Pass | 最终构建 7 项 OS 鼠标/键盘断言全部通过，`mouse_matrix.json`；角度和距离实际改变，重置恢复。 |
| 待机与静态回退 | Pass | 实时 Idle 图像变化；移除 idle 后只首次捕获，交互后才增加捕获；不同 Skeleton 回退静态。 |
| 快速切换最新请求胜出 | Pass | 同一帧提交 24 次真实 roster 选择，最终 ID 正确；另有真实 StreamableManager 取消 Automation。没有声称人为控制了磁盘 A/B 完成顺序。 |
| 关闭/重开 | Pass | 20 轮等待真实 Ready 后关闭；单一活跃 Stage，结束为零。 |

### 生命周期与异常

`Saved/CharacterPreview/pie_matrix.json`：34 项全部通过（1280×720）。`edges.json`：7 项全部通过。`mouse_matrix.json`：7 项全部通过。

| 条件 | 结果 | 证据范围 |
|---|---|---|
| Ready 开关 20 次 | Pass | 每轮实际加载至 Ready，不能与仅错误状态开关 Automation 混同。 |
| Loading 中关闭/重新打开 | Pass | 旧会话 Closed、捕获计数不增、无过期 Stage；重新打开正常。 |
| 连续进入/退出 PIE | Pass | 本轮多次正常开始/结束与 Editor 重启，日志分别保留；未崩溃。 |
| Loading 中重载同地图 | Pass | 原会话 Closed，新 World 零 Stage；`edges.json`。 |
| 不存在的 ID/空 Mesh | Pass | 明确 Error，不永久 Loading；资源恢复后可 Ready。 |
| 无动画/不匹配 Skeleton | Pass | 静态展示、无重复捕获，无错误动画播放。 |
| RT 宽高比与复用 | Pass | 800×200 请求得到同一个 RT 的 1024×256，随后恢复；几何 Automation 覆盖 1:4、2:3、1:1、16:9、4:1。 |
| 分辨率 | Pass（已测） | 1280×720 最终完整矩阵；2560×1440 首轮截图/矩阵。首轮 Python 快照比较含 wrapper 地址，单独保留失败记录 `pie_matrix-r1-2560.json`，修正后 1280 矩阵全过；不抹去历史失败。 |
| 长时资源泄漏 | Not Run | 20 轮零残留 Actor/捕获不是数小时 UObject/显存泄漏专项证明。 |

### 本机 Development 包补充验证

实际执行文件的 SHA-256 为 `PackagedEvidence/exe-sha256.json`。独立 `-userdir` 位于本任务 Saved，不读取或写入用户日常存档。原生命令 `HSR.CharacterPreviewProbe` 仅非 Shipping 编译存在，未自动运行；通过 `-ExecCmds=HSR.CharacterPreviewProbe` 显式启动。

69/69 断言：实际角色页入口、四个 definition 对应 Mesh、每次单一 Stage、RT 真正读回 PNG、原生带 UI 截图、旋转/缩放/重置 API、重复请求无重复加载、暂停捕获增长、最新请求胜出、Loading 中关闭、20 轮真实 Ready/关闭、无延迟复活、Pawn/Transform/角色 ID/Revision/Level/XP 不变。原始 UTF-16 报告及 CSV/图片位于 `Saved/CharacterPreview/PackagedProbeUser/Saved/CharacterPreviewProbe`；UTF-8 报告副本为 `Saved/CharacterPreview/PackagedEvidence/probe-report.json`。完整日志 `PackagedEvidence/probe-runtime.log`。它是包内真实 UI/session 调用，不冒充物理鼠标或完整属性/装备快照比较。

物理鼠标独立证据：PID 和绝对 exe 路径校验、前台 HWND 断言后才输入；Tab → 角色、四角色点击、左拖、滚轮、重置、Esc/X 返回大世界，动作日志 `PackagedEvidence/actions.jsonl`。最终修正后截图 `final_mouse_huohua/rotated/zoomed/reset/world.png`；四角色早轮截图 `packaged_character_*.png`。前台被用户文件选择器占用时曾安全停止；用户关闭后续测，不将那轮停止算 Pass。

视觉缺陷：切换后操作提示收窄已通过 Fill 布局修正，最终四角色截图恢复单行。部分旧界面的属性名、Tab/返回字形仍偶发缺片（最终包 `character_01_ui.png`、`02_ui.png` 可见），原因待定位，**Fail：全 UI 字形视觉质量**；上述 69 项技术断言不检测该问题。用户反馈火花脚姿不自然，**尚未通过对应主观验收**。

### 真实截图

![真实现有角色面板](../../Saved/Presentation/Captures/PREVIEW_initial.png)

![另一个已有角色](../../Saved/Presentation/Captures/PREVIEW_character_2.png)

旋转/缩放：`PREVIEW_rotate_zoom.png`；待机连续画面：`PREVIEW_initial.png`、`PREVIEW_idle_later.png`。原始 RT 在 `Saved/CharacterPreview/initial.png`、`idle_later.png`、`character_*.png`。均来自真实运行，没有用概念图替代。

## E. 性能与风险

硬件：Ryzen 5 9600X、RTX 5070；Windows 11、DX12、UE5.6.1 Editor PIE，1280×720，同一 ObservationCar 视角，后台节流仅在本次 Editor 会话关闭。每场景预热 3 秒、采样 8 秒，剔除前 60 行及末 3 行。原始 CSV 位于 `Saved/Profiling/CSV/CharacterPreview_*.csv`；统计为 `performance-summary.json`，步骤元数据为 `profile_scenarios.json`。

最终数值表见随交付保存的 [PERFORMANCE.md](PERFORMANCE.md)。它比较 Game Thread、Render Thread exclusive scopes（排除 EventWait）和 GPU，包含 Editor/Slate/Python 及原场景开销。原 CSV 的 `RenderThreadTime` 在当前 Editor 配置近零，因此保留它但不把它当成有效的完整 Render Thread 工作时长。不是独立 Capture pass 的 GPU 单价，也不是 Shipping 性能承诺。

单颜色面大小按 `宽×高×4` 估算：当前常见 544×768 约 1.59 MiB；默认 512×768 为 1.5 MiB，1024² 为 4 MiB。没有把此估算冒充总显存占用。

已知边界：Opaque 背景，无透明合成；保守 Bounds 球构图会给部分瘦长/带翼模型较多留白；只验当前 idle，大幅新动作需补 Bounds；未实现装备外观（当前装备无外观 Mesh）、换装、多人预览或动作编辑器。现有模型的公开再分发许可尚未核实。低端设备和最终包性能未验证，最终视觉质量仍待用户体验验收。

## F. 面试复述

90 秒版本及完整追问依据见 [CHARACTER_PREVIEW_INTERVIEW.md](../../CHARACTER_PREVIEW_INTERVIEW.md)。可以陈述真实渲染、交互和生命周期已在本地 PIE 验证；本机 Development 包技术验证已完成；不能陈述所有视觉问题通过人工验收或取得商业角色资产发行权。

## G. 最短人工验收

1. 直接运行 `Saved/CharacterPreview/Package/Windows/HSR.exe`（仅本机）；或打开 `HSR.uproject`，默认 `Map_ObservationCar`，Play。
2. Tab → 角色，确认当前活动角色出现；点击左侧另外一个角色。
3. 在中间预览区域按住左键左右拖动，滚轮缩放，点击「重置视角」。
4. Esc 返回菜单，X 回到大世界；确认真实 Pawn、队伍与装备未改变。

复现自动化：按架构文档先构建并启动 Editor，然后运行 `run_character_preview_tests.py`；启动 `start_character_preview_pie.py` 后依次运行 `verify_character_preview_pie.py`、外部 `verify_character_preview_mouse.py`、`verify_character_preview_edges_pie.py`。带异步 Slate callback 的脚本返回 queued 后，以结果 JSON/COMPLETE 日志为完成依据，不能把 queued 当成 Pass。

用户已批准仅本机打包验证；公开授权仍未确认。尚待最终视觉人工接受。复现包内探针：对包内 Win64/HSR.exe 添加 `-ExecCmds=HSR.CharacterPreviewProbe` 与隔离的 `-userdir`，等待日志 `completed: PASS`，然后检查报告和真实截图。
