# TASK-P1-005：动画资产接入与 Phase 1 最终回归

## 单卡工作包规则

本文件是 `TASK-P1-005` 从资产需求到最终归档的唯一任务卡，持续有效直到 Segment F 归档完成。A～F 是同一工作包内的阶段，不创建 `TASK-P1-005-A/B/C/D/E/F` 新任务卡，也不因段间确认反复执行 CC-SWITCH。

- 段间只更新本卡的状态/交接表与 `tasks/execution-result.md`；用户确认可直接在聊天中完成。
- 角色锁按当前 Segment 生效。不同人格可以在同一张长任务卡内继续工作，但 Implementation Agent 不得冒充 Coordinator、Reviewer、Teacher 或用户/资产作者。
- 不同人格与资产作者仍需独立交接、独立 Git commit。只有聊天证据且没有仓库产物的用户 Segment B 不要求 Git commit。
- 未满足当前段退出条件时，不得进入下一段。发现缺证据时停留在本卡等待补充，不另建卡。
- Segment C 通过最终导入 manifest 激活 Segment D；Coordinator 只需在本卡和累计执行报告中记录批准并创建同卡内文档 commit，无需新任务卡。

## 唯一目标与完成定义

在来源合法、可提交且技术兼容的用户资产上完成最小探索 AnimBP，并对 Phase 1 可玩闭环执行最终回归。只有 A～F 的真实证据、独立提交与归档都完成后，P1-005 才算完成；之后只交接 P1-006 Teacher/阶段收尾，不直接进入 Phase 2。

## 全流程状态与交接表

| Segment | 当前状态 | 进入条件 | 执行角色 | 退出产物 | Commit 作者 |
|---|---|---|---|---|---|
| A 资产需求清单 | `READY / 等待首次复述` | 本卡已由 Coordinator 提交 | Implementation Agent | 中文清单写入累计报告 | Implementation Agent |
| B 用户候选回传 | `LOCKED` | A commit 与交接完成 | 用户 | 聊天中的模板、路径/截图/metadata/许可材料 | 仅聊天时无需 commit |
| C 兼容性与许可证评估 | `LOCKED` | 候选及证据足以评估 | Implementation Agent 或用户授权的高级模型 | 逐文件矩阵、最终 manifest、风险 | 对应评估人格 |
| D Editor 导入与 AnimBP | `LOCKED` | C manifest 已批准并由 Coordinator 记录 | 用户 / UE Editor 执行者 | 导入资产、AnimBP、绑定、重开证据 | 用户资产作者 |
| E 最终回归 | `LOCKED` | D commit 完成且工作树范围清楚 | 用户 / 验证者 | Editor/PIE/Build/日志证据 | 有报告改动者按真实人格提交 |
| F 验收与归档 | `LOCKED` | E 证据齐全 | Reviewer 或用户 → Coordinator | 验收结论、完整历史归档 | Reviewer（如有）与 Coordinator |

## Segment A：Implementation Agent 提供资产需求清单

### 首次响应门禁

Implementation Agent 首次只能读取本文件，完整复述任务目标、A～F 流程、当前允许文件、禁止项、最低/推荐资产、证据要求和后续接收条件，并精确以：

`等待用户确认执行 TASK-P1-005-A。`

结束。用户另发明确确认前不得继续调用工具或写文件。这只是同一卡首次启动门禁；后续段不创建新卡。

### 允许读写与提交

- 读取：`tasks/active-task.md`
- 修改：`tasks/execution-result.md` 的 Segment A，必须保留其他段模板和已有历史
- commit message：`低级执行模型+Implementation Agent+TASK-P1-005-A/Phase 1+提供动画资产需求清单`
- commit 只能包含 `tasks/execution-result.md`

### 必须交付的资产清单

最低合格：

- 一个可提交的 Humanoid Skeletal Mesh + Skeleton；或含 Skin/Weights/Bones 的原始 FBX Mesh。
- 至少包含 Idle Loop、Walk 或 Run Locomotion Loop、至少一个可用于空中状态的 Jump 动画。
- 动画优先全部为 In-Place；本阶段不使用 Root Motion。
- 动画应使用同一 Skeleton；若不同，必须有 Source Skeleton、Reference Pose 与可 Retarget 证据。
- 主骨链至少可识别 `root / pelvis(hips) / spine / head / arms / legs`；说明单位、轴向和 Scale。
- 每个文件提供文件名、格式、用途、来源、作者、许可证和能否提交仓库。

推荐完整：

- Humanoid Skeletal Mesh、Skeleton、必要 PhysicsAsset 与直接依赖。
- Idle Loop、Walk/Run Loop、JumpStart、FallLoop、Land 五段动画。
- A Pose 或 T Pose；需要 Retarget 时提供源/目标 Reference Pose。
- 材质/贴图可选；不要求 Montage、攻击/战斗动画或 VFX。

拒绝条件：

- 商业游戏提取物、来源/作者不明或许可证证据不足。
- 禁止所需商用、修改、公开展示或 Git 再分发/提交的资产。
- 仅有无 Skin/骨骼的 Static Mesh，或没有可用于 Idle/Locomotion/Air 的动画。
- `.uasset` 缺少必要依赖；异骨架动画缺 Source Skeleton/Reference Pose。

接受格式与依赖：

- UE `.uasset`：说明创建 UE 版本、原始 Content 相对路径，并提供被引用的 Skeleton、Skeletal Mesh、PhysicsAsset 与必要直接依赖。
- FBX：说明是否包含 Mesh/Skin/Bones/Animation、多 Take 或逐文件、导出版本、单位/Up Axis/Forward Axis/Scale。
- 材质/贴图单列，不作为通过门禁。

许可证证据：

- 来源、作者/发布者、许可证原文或名称，以及商用、修改、再分发、公开展示、署名要求。
- 下载/购买凭证或许可证截图/文本；订单号、账号等敏感字段先遮挡。

### Segment B 用户回填模板

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
许可证名称、原文或链接信息：
允许商用：是/否/不明
允许修改：是/否/不明
允许再分发/提交 Git：是/否/不明
允许公开展示：是/否/不明
署名要求：
购买/下载凭证或许可证截图：
其他依赖或限制：
```

### Segment A 退出条件

- 用户收到与报告一致的可操作清单。
- 报告完整覆盖最低/推荐/拒绝、格式依赖、Retarget/Reference Pose、In-Place/Root Motion 与许可证。
- Implementation commit 仅含累计执行报告，并回传 hash、已验证/未验证项。

## Segment B：用户候选与授权证据回传

### 进入条件与角色锁

A 已提交并交接。用户使用上方模板回传候选；仓库外候选只报告路径、截图、文件列表和 metadata，不复制到 `Content/`，不暂存、不提交。

### 允许读写与退出产物

- 用户可提供仓库外本地路径及材料；只授权后续评估者只读，不自动授权复制、导入、联网、购买或再分发。
- 只有聊天证据时用户不需要 Git commit；Coordinator 在同一卡的状态表和累计报告中记录证据来源与接收时间。
- 退出条件：候选文件、依赖、骨架/版本/Reference Pose、动画用途及许可证材料足以进入 C。
- 缺少任何关键证据时留在 Segment B 等待补资料，不创建新卡。

## Segment C：兼容性与许可证只读评估

### 进入条件与角色锁

仅在用户明确提供候选路径及只读许可后，由 Implementation Agent 或用户指定高级模型评估。可读取明确提供的仓库外路径与许可证材料；默认不上网，除非用户另行要求。

### 允许读写

- 读取：本卡、累计执行报告、用户明确提供的候选路径/截图/metadata/许可证材料，以及为判断兼容性所需的项目只读版本/现有目标骨架信息。
- 修改：`tasks/execution-result.md` 的 Segment C；不得覆盖 A/B 历史。
- 不得复制/移动/转换/导入候选，不得写 `Content/`，不得启动 Editor、构建或 PIE。

### 必须输出

- 逐文件矩阵：`Accept / Reject / Retarget / 缺依赖`。
- 每项的 Skeleton、引擎/导出版本、Reference Pose、动画用途、Loop/In-Place/Root Motion 状态与依赖。
- 许可证判断边界：可确认事项、无法确认事项、所依据材料；技术评估不冒充法律意见。
- 确定的 Segment D 导入 manifest：外部源文件/现有 `.uasset`、目标名称、目标目录、必要直接依赖、是否需要 IK Retargeter/IK Rig。
- 风险与停止条件。候选未知时不得预先批准具体外部文件或许可证。

### 提交与退出条件

- 对应人格独立提交累计报告，message：`<人格>+TASK-P1-005-C/Phase 1+评估动画资产兼容性与许可证`
- 若证据不足，结论必须是停止并留在本卡等补资料。
- 只有逐文件矩阵与最终 manifest 均明确时，Coordinator 才可在本卡状态表和累计报告记录“Segment D 已激活”，创建同卡内 Coordinator 文档 commit；无需新卡。

## Segment D：用户 UE Editor 导入、AnimBP 与绑定

### 进入条件与路径激活

仅当 Segment C 的最终 manifest 明确列出接受文件并由 Coordinator 记录批准后，本段才激活下列路径；未列入 manifest 的文件仍禁止导入：

- `Content/Characters/Player/`：建议命名 `SK_HSRExploration`、`SKEL_HSRExploration`、`PHYS_HSRExploration` 及 manifest 批准的材质/贴图依赖。
- `Content/Characters/Animation/`：`A_HSR_Idle`、`A_HSR_Locomotion`、`A_HSR_JumpStart`、`A_HSR_FallLoop`、`A_HSR_Land`、`ABP_HSRExploration`。
- 若 C 明确要求 Retarget，批准的 IK Rig/IK Retargeter 也必须位于 `Content/Characters/Animation/` 并列入 manifest。
- 可修改：`Content/Blueprints/Character/BP_HSRExplorationCharacter.uasset`。
- 禁止其他目录、未列依赖和自动顺带导入的额外资产。若出现额外依赖，停止并在同一卡请求 manifest 更新。

### UE Editor 操作

1. 按 manifest 导入 Skeletal Mesh/Skeleton；核对 Skeleton、Reference Pose、单位、轴向、Scale、材质槽和 PhysicsAsset。
2. 导入同骨架动画；若 C 判定需要 Retarget，先创建批准的 IK Rig/IK Retargeter，再将结果输出到 Animation 目录。
3. 将 Locomotion 与空中动画设为 In-Place，关闭 Root Motion；Gameplay 移动仍由 CharacterMovement 决定。
4. 创建 `ABP_HSRExploration`，Parent 为 AnimInstance，Target Skeleton 为批准骨架。
5. Event Blueprint Update Animation：通过 `TryGetPawnOwner` 安全获取 Pawn，读取 Velocity；计算 XY Size 得到 Speed，读取 CharacterMovement `IsFalling`，用 Velocity/Actor Rotation 计算 Direction。Owner/Movement 为空时安全返回。
6. AnimGraph 创建 Idle/Move/Air 最小三态；若有完整动画可建 Idle/Move/JumpStart/FallLoop/Land 五段。转换只消费 Gameplay 状态，不写回移动规则。
7. 在 `BP_HSRExplorationCharacter` 绑定 Skeletal Mesh 与 Anim Class，核对 Mesh 相对 Capsule 的位置/旋转。
8. 保存全部资产，关闭并重开 Editor，确认引用保持、无重定向器/缺依赖警告。

### 用户资产提交门禁

- 用户是 Editor 资产作者，必须独立 commit；主 Agent 只有经用户明确授权才可代办 Git，且不得冒充作者。
- 提交前运行精确 cached list 核对，只能出现 C manifest 批准的资产、角色 BP，以及 C 批准的许可证文档。
- 许可证/署名证据建议存放 `docs/asset-licenses/<具体文件>`；只有 C 批准时才纳入提交。
- message：`用户+UE Editor执行者+TASK-P1-005-D/Phase 1+导入角色动画并创建探索AnimBP`

### 退出条件

资产重开保持、AnimBP 已绑定、Git 提交范围与 manifest 一致，并回传 commit hash、cached list、许可证台账和未验证项。

## Segment E：Phase 1 最终回归

### 必测矩阵

1. Editor 重开后 Mesh、Skeleton、AnimBP、动画与角色 BP 引用保持。
2. Idle、Move、Jump/Air、Land（若提供）表现符合状态；动画只表现，不改变 CharacterMovement。
3. 临时移除 Mesh 或 AnimClass 的失败路径仍可安全运行 Gameplay，并有可定位日志/现象；测试后恢复，不提交临时改动。
4. 同一 PIE 会话 UnPossess → Re-Possess，输入与动画恢复且只触发一次。
5. 连续至少两次 PIE，无重复 Pawn、Mapping Context、Action Binding 或 HUD。
6. 至少测试 30/60 FPS，条件允许优先 30/120 FPS；移动规则不因帧率改变。
7. Exploration → UIOnly → Exploration，UIOnly 阻止探索输入，恢复后输入/动画正常。
8. HUD 单实例；Context、Binding、Pawn 均不重复。
9. 检查 Output Log 第一处真实 Error、Ensure、Accessed None、骨架/动画/绑定警告。
10. 运行最终 `HSREditor Development Win64` 构建。若无 C++ 变化可如实记录 up-to-date；不得把 up-to-date 冒充目标源码实际编译。

### 允许写入与退出

- 持久写入仅累计 `tasks/execution-result.md` 的 Segment E；临时 Editor 测试改动必须恢复。
- 验证者按真实人格提交报告；若证据仅由用户聊天回传，Coordinator 记录为用户证据，不冒充独立验证。
- 退出条件：矩阵逐项给出 PASS/FAIL/NOT VERIFIED、实际日志与未验证项。失败时停留同一卡，由 Coordinator 决定受控修订，不另建工作包。

## Segment F：验收、归档与下一交接

- Reviewer 可依据任务卡、diff、提交与用户证据独立审查；若用户明确人工验收并跳过 Reviewer，只记录用户验收，不伪造 Reviewer 产物或 commit。
- Reviewer（如有）独立提交审查报告；用户聊天证据与 Reviewer 独立运行证据必须明确区分。
- Coordinator 合并 `tasks/execution-result.md` 的 A～F 完整历史，不得覆盖早期段；归档活动卡、执行报告及审查材料，同步项目状态并创建 Coordinator commit。
- 归档后正式交接 `TASK-P1-006` Teacher/Phase 收尾；不得直接开始 Phase 2。

## 全局失败与停止条件

- 许可证、作者、再分发/Git 提交权或依赖证据不足：停止，不导入。
- 需要 Retarget 但缺 Source Skeleton/Reference Pose：停止，补资料。
- Segment C manifest 未批准或导入产生未列依赖：停止，不扩大路径。
- 需要 Source、Config、插件、新模块或其他 Content 目录：停止，由 Coordinator 在同一卡评估扩权并记录。
- 骨架不兼容、Editor 重开丢引用、AnimBP 报错或 Gameplay 回归失败：记录第一处真实错误，停留在对应 Segment。

## 非目标

- 不搜索、下载、购买或推荐具体第三方资产，除非用户另行明确要求。
- 不实现 Root Motion Gameplay、Motion Matching、Control Rig、高级 IK、战斗 Montage、攻击/受击、GAS、网络复制或正式美术打磨。
- 不修改 Source、Config、`.uproject`、GameMode、HUD、输入或地图，除非本卡内出现真实阻断并由 Coordinator 明确扩权。
- 本卡结束后不直接进入 Phase 2。

## 当前接收条件

当前为 `Coordinator → Implementation Agent（Segment A 首次只读复述）`。执行者只能按 Segment A 首次门禁复述，并以 `等待用户确认执行 TASK-P1-005-A。` 结束。
