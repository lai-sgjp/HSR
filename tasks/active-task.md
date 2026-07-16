# TASK-P1-005：动画资产接入与 Phase 1 最终回归

## Role Lock / 角色锁定

当前执行者仅担任 `Implementation Agent / 低级执行模型`。不得自行切换人格、扩大范围、搜索或下载资源、导入资产、修改源码或启动 Editor。需要扩权时必须停止并回报 Coordinator。

## 当前 Phase

- Phase 1 进行中；P1-001～P1-004 已归档。
- P1-004 已形成可玩的探索闭环并通过最终 Reviewer。
- 用户选择使用自己已有且已授权的角色/动画资产；当前尚未提供候选文件与许可证证据。

## 唯一目标

在合法、可提交且技术兼容的用户自有/已授权 Humanoid 资产上完成最小 AnimBP，并对 Phase 1 探索闭环执行最终回归。资产未通过来源、许可证和兼容性门禁前，不得复制或导入到 `Content/`。

## 工作包分段与交接

1. **Segment A — Implementation Agent 资产需求清单**：先向用户给出可操作的中文清单，并把同样内容写入 `tasks/execution-result.md`，创建 Implementation 角色 commit。
2. **Segment B — 用户提供候选与授权证据**：用户只回传本地候选文件清单、截图和元数据；仓库外文件只报告路径与元数据，不复制、不提交。
3. **Segment C — Implementation/Coordinator 只读兼容性评估**：根据真实候选精确判断 Skeleton、动画、格式、重定向和许可证，并形成导入方案。此段仍不得导入。
4. **Segment D — 用户 UE Editor 导入、AnimBP、绑定与用户资产 commit**：必须先由 Coordinator 根据候选结果另行精确授权目标路径、资产清单和依赖；“用户说已有资产”不等于许可导入。
5. **Segment E — 最终回归**：验证动画表现、移动/输入/控制模式、连续 PIE、重新 Possess、帧率和失败路径。
6. **验收与归档**：Reviewer 或用户明确验收 → Coordinator 归档。不同人格与资产作者必须独立交接、独立 commit；Phase 结束仍需 P1-006 Teacher 流程。

## 当前仅授权 Segment A

### 首次响应门禁

Implementation Agent 首次只能读取本文件并完整复述：任务目标、A～E 分段、当前允许文件、禁止项、资产最低要求、证据要求与后续接收条件。必须精确以：

`等待用户确认执行 TASK-P1-005-A。`

结束并停止。用户另发明确确认前，不得继续调用工具或写文件。

### Segment A 允许读取/修改

- 读取：`tasks/active-task.md`
- 修改：`tasks/execution-result.md`

其余文件全部禁止读取或修改。Segment A 完成时，只能提交 `tasks/execution-result.md`，commit message：

`低级执行模型+Implementation Agent+TASK-P1-005-A/Phase 1+提供动画资产需求清单`

### Segment A 必须交付给用户并写入执行报告的中文清单

#### 最低合格

- 1 个可提交的 Humanoid Skeletal Mesh + Skeleton；也接受一个包含 Skin/Weights/Bones 的原始 FBX Mesh。
- 动画至少包含：Idle 循环、Locomotion 循环（Walk 或 Run 至少一个）、空中/Jump 动画至少一个。
- 动画优先全部为 In-Place；Root Motion 不进入本阶段。
- 动画应使用同一 Skeleton；若不同，必须明确可 Retarget，并提供 Source Skeleton 与 Reference Pose 信息。
- 骨骼至少能识别 `root / pelvis(hips) / spine / head / arms / legs` 等人体主链；单位、朝向和 Scale 若非 UE 常规设置需明确说明。
- 对每个文件提供文件名、格式、用途、来源、作者、许可证以及能否提交仓库。

#### 推荐完整

- 1 个 Humanoid Skeletal Mesh、Skeleton、必要的 PhysicsAsset 与直接依赖。
- 5 段基础动画：Idle Loop、Walk 或 Run Loop、JumpStart、FallLoop、Land。
- A Pose 或 T Pose Reference Pose；若需要 Retarget，同时提供源/目标 Reference Pose 截图或说明。
- 材质和贴图可提供但不是门禁；不要求 Montage、攻击/战斗动画或 VFX。

#### 拒绝条件

- 商业游戏提取物、来源不明资源或无法证明作者/来源的文件。
- 许可证仅限个人使用，或禁止提交、展示、修改/派生，而本任务需要执行对应行为。
- 无法说明商业使用、修改、再分发、署名要求，或无法提供购买/下载凭证与许可证截图/文本。
- 仅有无 Skin/骨骼的 Static Mesh，或没有任何可用于 Idle/Locomotion/Air 的动画。
- 依赖缺失且无法完整迁移的 `.uasset`，或动画既不同 Skeleton 又没有可用 Source Skeleton/Reference Pose。

#### 接受格式与依赖

- UE `.uasset`：必须连同对应 Skeleton、Skeletal Mesh、PhysicsAsset（如被引用）及所有必要直接依赖；说明创建所用 UE 版本和原始 Content 相对路径。
- 原始 FBX：说明文件是否含 Mesh、Skin、Bones、Animation，动画是单文件多 Take 还是逐文件；保留原文件名。
- 可选材质/贴图单独列出，不作为本阶段通过门禁。

#### 许可证证据

- 每项列出：来源、作者/发布者、许可证名称或原文、是否允许商用、修改、再分发、公开展示、是否要求署名。
- 提供下载/购买凭证，或许可证页面/文件截图；若有订单号或账户信息，回传前遮挡敏感字段。
- 禁止使用商业游戏提取物、来源不明、“仅个人使用且不可提交/展示”的资源。

#### 用户回填模板

```text
候选包名称：
本地路径（仓库外只报告，不复制）：
文件清单（文件名 / 格式 / 用途）：
Skeletal Mesh：
Skeleton / Source Skeleton：
Reference Pose（A/T/其他）：
骨骼主链（root/pelvis/spine/head/arms/legs）：
动画清单（Loop/In-Place/Root Motion/所属 Skeleton）：
UE 版本或 FBX 导出版本：
单位 / Up Axis / Forward Axis / Scale：
材质贴图（可选）：
来源与作者：
许可证名称/原文或链接信息：
允许商用：是/否/不明
允许修改：是/否/不明
允许再分发/提交 Git：是/否/不明
允许公开展示：是/否/不明
署名要求：
购买/下载凭证或许可证截图：
其他依赖或限制：
```

用户回传时不得先复制进 `Content/`。如果资产位于仓库外，只报告路径、文件清单、截图和元数据，不提交候选文件。

## Segment A 明确禁止

- 调用网络、搜索、下载、购买或推荐具体第三方资产。
- 写入或读取 `Content/` 候选资产；复制、移动、导入、迁移或修改任何 `.uasset`、FBX、纹理或动画。
- 修改 Source、Config、`.uproject`、Blueprint、地图、AnimBP 或任何项目文档。
- 构建、启动 Editor、运行 PIE、生成派生产物或 push。
- 在候选资产尚未评估时承诺兼容、批准许可证或预定导入路径。

## Segment A 验收标准

- [ ] 首次只读复述满足门禁，用户另行确认后才执行。
- [ ] 用户消息与 `tasks/execution-result.md` 含相同且可操作的“最低合格 / 推荐完整 / 拒绝条件”清单。
- [ ] 清单覆盖 Mesh/Skeleton、最低与推荐动画、In-Place、Retarget、Reference Pose、骨骼主链、格式/依赖和许可证。
- [ ] 提供完整用户回填模板并明确仓库外候选不得复制进 `Content/`。
- [ ] Git commit 只含 `tasks/execution-result.md`，并回传完整 hash、已验证与未验证项。

## 后续段的停止条件

- 没有许可证或依赖证据：停止，不导入。
- Candidate 需要 Retarget 但缺 Source Skeleton/Reference Pose：停止并补资料。
- 候选范围或目标 Content 路径未获 Coordinator 精确授权：停止，不导入。
- 导入产生未列出的依赖或需要源码/Config 修改：停止并申请扩权。

## 当前状态

- Coordinator → Implementation Agent（首次只读复述）。
- 尚未搜索、下载、导入、评估或修改任何动画资产。
- 尚未实施 AnimBP、构建、Editor、PIE 或 push。

