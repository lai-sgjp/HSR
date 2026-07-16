# TASK-P1-003B：创建并验证 Enhanced Input 资产

> 状态：Coordinator 已规划，正式交接给用户 `UE Editor 执行者`。本任务不由 Implementation Agent、Reviewer 或 Coordinator 代替操作资产。

## 唯一目标

在 UE Editor 中清点、补全并验证以下五个且仅以下五个资产：

- `Content/Input/IA_Move.uasset`
- `Content/Input/IA_Look.uasset`
- `Content/Input/IA_Jump.uasset`
- `Content/Input/IA_Interact.uasset`
- `Content/Input/IMC_Exploration.uasset`

P1-003A C++ 已完成并由用户人工验收，Implementation commit 为 `490440f280f82dbffd84976bad1dd2a92ee1f39d`，Coordinator 归档 commit 为 `2ae73e2`。

## 角色与产物归属

- 执行人格：用户 / UE Editor 执行者。
- 用户本人检查、保存并提交五个 `.uasset`，以保持资产作者与 Git 归属真实。
- Coordinator 只提供任务卡并在收到证据和 commit hash 后归档。
- 不需要 Implementation Agent 修改 C++，也不得由其他人格冒充用户创建资产。

## 已存在但禁止纳入

以下两个未跟踪 Blueprint 是用户提前创建的 P1-004 候选，本卡不得修改、暂存或提交：

- `Content/Blueprints/Character/BP_HSRExplorationCharacter.uasset`
- `Content/Blueprints/Input/BP_HSRPlayerController.uasset`

输入资产引用绑定与完整 PIE 验证推迟到 P1-004。若发现蓝图绑定是完成本卡不可缺少的条件，停止并向用户说明，不自行扩大范围。

## Editor 执行步骤

1. 打开 UE Editor，在 Content Browser 确认五个资产路径和名称完全正确。
2. 检查 Input Action Value Type：
   - `IA_Move`：`Axis2D (Vector2D)`。
   - `IA_Look`：`Axis2D (Vector2D)`。
   - `IA_Jump`：`Digital (bool)`。
   - `IA_Interact`：`Digital (bool)`。
3. 打开 `IMC_Exploration`，核对键鼠 Mapping：
   - Move：W/S 对应 Y 正/负；A/D 对应 X 负/正，使用合理的 Negate/Swizzle Input Axis Values Modifier。
   - Look：Mouse XY 映射到 Axis2D；Y 轴方向按当前第三人称约定配置并记录是否反转。
   - Jump：Space Bar。
   - Interact：E。
4. 保存五个资产。不要打开后保存两个 Blueprint，不要保存地图、Config 或其他资产。
5. 关闭并重新打开 Editor，确认五个资产仍存在、可打开，Value Type、Mappings 与 Modifiers 均保持。
6. 将实际观察填写到 `tasks/execution-result.md`；如有警告或失败，记录第一处真实问题，不声称 PIE 输入已验证。

## 验收证据

- 五个准确资产路径的清单或截图/文字确认。
- 四个 Input Action 的实际 Value Type。
- IMC 中每个键鼠 Mapping 和 Modifier 的实际配置。
- Editor 重开后五资产仍存在、可打开且配置保持。
- `git status --short` 证明待提交范围只选择五个 Input 资产；两个 Blueprint 和所有派生产物均未暂存。
- 用户资产 commit 完整 hash。

## Git 提交门禁

提交前执行并检查：

```powershell
git status --short
git add Content/Input/IA_Move.uasset Content/Input/IA_Look.uasset Content/Input/IA_Jump.uasset Content/Input/IA_Interact.uasset Content/Input/IMC_Exploration.uasset
git diff --cached --name-only
```

`git diff --cached --name-only` 必须恰好只有上述五个资产。然后由用户本人提交：

```powershell
git commit -m "用户+UE Editor执行者+TASK-P1-003B/Phase 1+创建Enhanced Input资产"
```

不得 push；提交后回传完整 commit hash。

## 失败边界

- 资产路径、类型或映射无法确认：停止并记录，不猜测。
- Editor 重开后资产或配置丢失：本卡失败，不提交为完成。
- 需要修改 Blueprint/C++/Config/Build.cs/地图：停止并申请扩权。
- 不运行或宣称完整 PIE、Possession、引用绑定、移动、Look、Jump、Interact 已通过。
- 不提交 `Binaries/`、`Intermediate/`、`Saved/`、DerivedDataCache 或其他派生产物。

## 当前正式交接

`Coordinator → 用户 / UE Editor 执行者`

用户完成 Editor 检查、重开验证、执行报告和仅五资产 commit 后，将完整 hash 与证据交回 Coordinator。Coordinator 才可归档 P1-003B 并进入 P1-004。
