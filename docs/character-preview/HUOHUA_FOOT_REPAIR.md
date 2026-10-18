# 火花脚部变形修复（2026-10-18）

## 结论与范围

用户在预览交付期间反馈「火花的脚骨骼不自然」，要求先完成当前工作再修复。本修复在预览实现及本机包验证提交 `6b1491b` 后进行。

已确认并修复普通脚趾骨链未跟随 D 变形骨链造成的靴子拉扯；保留原 Mesh、Skeleton、绑定姿势、蒙皮、D 链动作、动画时长/帧率、Root Motion 与业务数据。仅修正火花当前定义/AnimBP 可达动画中的八条普通腿/趾轨道。用户对自然程度的最终主观验收仍待实际体验，不把骨链数值一致等同于正式动画美术验收。

## 根因证据

- VRM4U 的 `VRMUtil::GetReplacedPMXBone`（`Plugins/VRM4U/Source/VRM4U/Private/VrmUtil.cpp`）将足/膝/踝/趾映射到 D/EX 链。现有 retarget 动作确实驱动 D/EX；普通腿链保持旧姿势。
- UE 原生只读导出绑定 Mesh 至 `Saved/HuohuaFeet/huohua-bind.fbx`，现有 Blender 3.6.23 读取其权重：左右普通 `つま先` 各有 **212 个受权顶点**、权重和约 120.74；EX 趾仅各 13 个顶点、权重和约 1.64。故只让 D/EX 运动不能带动整个靴子前掌。
- 修复前 idle 采样的两条脚踝链最大分离 **21.3935 cm**。这解释了腿/靴筒跟随 D 链、前掌仍受另一条停滞链影响的拉扯。原始 `pose-before.json`、`skin-weights.json` 与 before 近景保留在 Saved。
- 没有用关闭 PostProcess 作为未经证实的归因；其 VRM Constraint/SpringBone 并不是已确认的 PMX grant 修复器。

## 实际修改

`Tools/Presentation/fix_huohua_foot_chains.py` 通过原 `DA_Character_Huohua` 的 AnimationClass、PreviewIdle、Attack/Skill/Hit/Defeat 软引用，递归读取当前 AnimBP/BlendSpace 依赖，找到 **24 个当前 AnimSequence**。不扫描并改写全部历史 LIVE/PR 动画，也不处理其他角色。

每个动作先检查两条腿对应绑定轴、前三节的绑定位置一致。用 UE `AnimPose` Source 在原帧率采样；把 D 大腿、膝、踝的局部平移/旋转/缩放烘焙到普通对应链；普通脚趾只同步 EX 的旋转，**保留其自己的平移枢轴**，避免把不同骨长的 EX 位置强塞到原脚趾。之后用 `AnimationDataController.set_bone_track_keys` 写入八条轨道。

全程不重新导入网格，不改权重，不改变 Skeleton，不新增运行时 Tick 或对所有角色实施骨骼补偿。副作用只存在于已确认错误的离线动画数据中。当前 24 个动作有 23 个修改了八条骨轨道；完整倒地动作原本一致，没有烘焙轨道。新进程验证时，工具旧版在跳过分支调用 save_loaded_asset(True)，使加载后带脏标记的倒地动画重新序列化了一次；已移除该调用，且最终独立进程重复执行24个文件哈希全部不变。该额外重保存单列于文件清单，不伪称它从未改写。

第一次副本与 SHA-256 在 `Saved/HuohuaFeet/BeforeAssets`。原始角色资产仍按仓库规则在本地忽略，不进入 Git 或远端；源码交付包含可重复执行的修复工具。未来重新 retarget 这些动作后，应重新运行修复和验证工具。

## 验证

| 项目 | 结果 | 证据 |
|---|---|---|
| 24 动作 Source 与 Compressed、所有关键帧与半帧 | Pass | 5,428 个 pose 评估，包含脚趾局部旋转和保留自身枢轴检查（枢轴偏移 0 cm）；全新无窗口 Editor 读取保存资源复验通过，`fresh-process-verification.log`、`compressed-verification.json`。最大压缩脚踝分离 0.0041871 cm，未压缩修复误差小于 0.00005 cm。阈值为链一致性诊断，不是人体姿态标准。 |
| 非目标骨动作 | Pass（采样范围） | 首轮修复对每个动作 5 个相位比较全部非目标骨的局部位置/旋转；最大值及每文件哈希见 `repair-first-run.json`。最初试修 idle/attack 分别保留原资产副本，不伪称这两项也来自同一首轮日志。 |
| Mesh/Skeleton/DA 原样 | Pass | `repair-current-animations.json` 的 protected_unchanged。 |
| 重复运行 | Pass | 第 2 次 0 项需要修正，24 个动作文件 SHA-256 全部不变；`idempotence-final.json`、`final-idempotence.log`（最终工具）。 |
| 实际 Preview 正/侧/背面 | Pass（工程观察） | `Saved/Presentation/Captures/HUOHUA_FEET_after_front/side/back.png`，已逐张查看，原前掌拉扯消失。 |
| 大世界实际 Pawn/AnimBP | Pass（待机采样） | `pie-world.json`，原 Sparxie mesh + FINAL_Huohua_ABP_Unarmed；左右脚踝链偏差均 0 cm。没有声称已逐个实机播放全部战斗动作。 |
| 本机重新 Cook/Package 和包内复验 | Pass（仅本机） | `Saved/HuohuaFeet/package-development-final.log` 与最终 `package-delivery.log` BUILD SUCCESSFUL；首轮25项重 Cook，最终同步当前资产，最终70/70原生检查含 `huohua_weighted_foot_chains_coherent`；`package-report.json`，四角色及20次开关重新通过。 |

修复前后真实近景（动画相位可能不同，不用于逐像素相位比较）：

![修复前](../../Saved/Presentation/Captures/HUOHUA_FEET_before_front.png)

![修复后](../../Saved/Presentation/Captures/HUOHUA_FEET_after_front.png)

现有 UI 字形偶发缺片仍为预览报告中的独立已知问题，本修复未声称解决该问题。原预览性能数字采自脚部修复前的 PIE，保持其原始证据轮次；本修复没有新增运行时求解成本。

包内最终截图：`Saved/HuohuaFeet/DeliveryUser/Saved/CharacterPreviewProbe/character_01_ui.png`；真实鼠标正/侧/返回大世界截图在 `Saved/CharacterPreview/PackagedEvidence/foot_fixed_packaged_*.png`。最终 exe SHA-256 为 `Saved/HuohuaFeet/exe-sha256.json`，最终独立测试存档目录为 `Saved/HuohuaFeet/DeliveryUser`。前两次构建遇到 Editor 退出收尾期的 Live Coding mutex，保留失败日志；实际 mutex 释放后构建成功，未删除生成目录或修改全局设置。

## 复现

停止 PIE，在已启动的项目 Editor 运行：

```powershell
python Tools/Presentation/character_preview_bridge.py Tools/Presentation/fix_huohua_foot_chains.py
python Tools/Presentation/character_preview_bridge.py Tools/Presentation/verify_huohua_foot_chains.py
```

检查结果 JSON 后重新 PIE，Tab → 角色 → 火花 → 左拖观察脚尖/靴底；Esc/X 返回大世界。不要仅凭脚本返回 queued 就判为通过。

## 本地已修改的动画资产

下列 23 项均是已有本地忽略资源；未加入 Git。没有生成新的一套角色动画目录。

- `Content/Presentation/Animation/Combat/LIVE_Huohua_MM_Attack_01.uasset`
- `Content/Presentation/Animation/Combat/LIVE_Huohua_MM_ChargedAttack.uasset`
- `Content/Presentation/Animation/Combat/LIVE_Huohua_MM_HitReact_Front_Med_01.uasset`
- `Content/Presentation/Animation/Locomotion/Huohua/FINAL_Huohua_MF_Unarmed_Jog_Bwd.uasset`
- `Content/Presentation/Animation/Locomotion/Huohua/FINAL_Huohua_MF_Unarmed_Jog_Bwd_Left.uasset`
- `Content/Presentation/Animation/Locomotion/Huohua/FINAL_Huohua_MF_Unarmed_Jog_Bwd_Right.uasset`
- `Content/Presentation/Animation/Locomotion/Huohua/FINAL_Huohua_MF_Unarmed_Jog_Fwd.uasset`
- `Content/Presentation/Animation/Locomotion/Huohua/FINAL_Huohua_MF_Unarmed_Jog_Fwd_Left.uasset`
- `Content/Presentation/Animation/Locomotion/Huohua/FINAL_Huohua_MF_Unarmed_Jog_Fwd_Right.uasset`
- `Content/Presentation/Animation/Locomotion/Huohua/FINAL_Huohua_MF_Unarmed_Jog_Left.uasset`
- `Content/Presentation/Animation/Locomotion/Huohua/FINAL_Huohua_MF_Unarmed_Jog_Right.uasset`
- `Content/Presentation/Animation/Locomotion/Huohua/FINAL_Huohua_MF_Unarmed_Walk_Bwd.uasset`
- `Content/Presentation/Animation/Locomotion/Huohua/FINAL_Huohua_MF_Unarmed_Walk_Bwd_Left.uasset`
- `Content/Presentation/Animation/Locomotion/Huohua/FINAL_Huohua_MF_Unarmed_Walk_Bwd_Right.uasset`
- `Content/Presentation/Animation/Locomotion/Huohua/FINAL_Huohua_MF_Unarmed_Walk_Fwd.uasset`
- `Content/Presentation/Animation/Locomotion/Huohua/FINAL_Huohua_MF_Unarmed_Walk_Fwd_Left.uasset`
- `Content/Presentation/Animation/Locomotion/Huohua/FINAL_Huohua_MF_Unarmed_Walk_Fwd_Right.uasset`
- `Content/Presentation/Animation/Locomotion/Huohua/FINAL_Huohua_MF_Unarmed_Walk_Left.uasset`
- `Content/Presentation/Animation/Locomotion/Huohua/FINAL_Huohua_MF_Unarmed_Walk_Right.uasset`
- `Content/Presentation/Animation/Locomotion/Huohua/FINAL_Huohua_MM_Fall_Loop.uasset`
- `Content/Presentation/Animation/Locomotion/Huohua/FINAL_Huohua_MM_Idle.uasset`
- `Content/Presentation/Animation/Locomotion/Huohua/FINAL_Huohua_MM_Jump.uasset`
- `Content/Presentation/Animation/Locomotion/Huohua/FINAL_Huohua_MM_Land.uasset`

另有一次已一致资产的序列化重保存：`Content/Presentation/Animation/Combat/FULL_Huohua_QuaterniusDeath_Anim.uasset`。没有脚链烘焙；前后 SHA-256 见 `Saved/HuohuaFeet/consistent-resave.json`。工具已避免此行为，后续跳过不触发 Save。
