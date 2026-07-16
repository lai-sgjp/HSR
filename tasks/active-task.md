# TASK-P1-005：动画资产接入与 Phase 1 最终回归

## 单卡规则与当前结论

本文件仍是 P1-005 唯一任务卡。Reviewer `b741391` 结论为 `REVISE`；补证与追认继续在本卡完成，不创建分段新卡，不进入 P1-006，不 push。

当前交接：`Coordinator → 用户`。

## A～F 真实状态

| Segment | 状态 | 真实证据/缺口 |
|---|---|---|
| A 资产需求清单 | `完成` | Implementation `649e125` |
| B 用户候选回传 | `候选已收到 / 许可证证据不足` | 缺官方许可/FAQ、来源与下载凭证 |
| C 兼容性与许可证评估 | `REVISE` | `0c85794` 存在许可证过度结论；公开 Git 再分发权未验证 |
| D Editor 导入与 AnimBP | `已发生 / 待追认与引用验证` | `a539b6d`、`abca679`；manifest、路径移动和 GameMode 修改未事前批准 |
| E 最终回归 | `部分用户报告通过 / 待补证` | 缺 FPS、同会话 Re-Possess、无 Mesh/AnimClass、正式构建证据 |
| F 验收与归档 | `REVISE` | `3d94b74` 仅为汇总草稿；Reviewer `b741391` 未放行 |

## Reviewer REVISE 补证段（不新建任务卡）

### 用户需回传许可证与仓库信息

- Adobe/Mixamo 官方许可证或 FAQ 原文、截图，或来源 URL 信息。
- 下载账户/下载凭证；敏感字段可遮挡。
- 当前仓库是 private 还是 public。
- 是否计划公开仓库并保留这些原始或可提取 `.uasset` 的 Git 历史。

成品使用许可不得自动推导公开 Git 再分发权。若不能证明公开再分发，可选但本卡不擅自执行：保持资产/历史仅私有；经用户另行授权用安全 Git 策略移出公开历史；替换为明确允许再分发的资产。

### Coordinator 待追认 manifest

资产已经进入 `a539b6d` / `abca679`，不得写成尚未提交：

- 删除：`Content/Blueprints/Character/BP_HSRExplorationCharacter.uasset`
- 新增 BP：`Content/Blueprints/Character/Player/BP_HSRExplorationCharacter.uasset`
- 修改：`Content/Blueprints/Framework/BP_HSRGameMode.uasset`
- 角色：`Content/Characters/Player/AS_Kachujin_G_Rosales_Anim.uasset`、`PA_Kachujin_G_Rosales_PhysicsAsset.uasset`、`SKM_Kachujin_G_Rosales.uasset`、`SK_Kachujin_G_Rosales_Skeleton.uasset`
- 动画：`Content/Characters/Player/Anim/ABP_HSRExploration.uasset`、`Breathing_Idle.uasset`、`Jogging.uasset`、`Jump_Up.uasset`、`Jump_Loop.uasset`、`Jump_Down.uasset`
- 材质：`Content/Characters/Player/Material/kachujin_MAT.uasset`、`kachujin_MAT_.uasset`
- 贴图：`Content/Characters/Player/Texture/Kachujin_diffuse.uasset`、`Kachujin_diffuse_body.uasset`、`Kachujin_normal.uasset`、`Kachujin_specular.uasset`

用户重开 Editor 后必须确认：GameMode Default Pawn 指向新 BP；新 BP 的 Mesh/AnimBP 有效；地图 Spawn/Possess/动画正常；旧路径无缺失引用，redirector 状态明确。之后 Coordinator 才能事后追认。

### 补测与构建清单

1. 30/60 FPS（或 30/120）下移动、跳跃和动画一致。
2. 同一 PIE `UnPossess → Re-Possess` 后输入/动画恢复，Action 单次触发，Pawn/Context/Binding/HUD 不重复。
3. 临时清空 Mesh/AnimClass 后 Gameplay 仍工作；随后恢复且不提交临时改动。
4. 最终 Development Editor 构建：完整命令、target/config/platform、时间、退出码、UHT/Compile/Link 或 up-to-date 状态、日志位置/摘要。
5. Output Log 的 Error、Ensure、Accessed None、Skeleton/Animation/Binding 警告结果。

## 边界与下一交接

- `Config/DefaultEditor.ini` 保持未跟踪，不改、不暂存、不提交；归档前用户决定保留本地或另行授权处理。
- P1-005 与 todo 保持未完成；禁止源码、资产、Config 修改及 push。
- 用户一次性回传上述证据后，Coordinator 核对并决定是否交 Reviewer 复审。
