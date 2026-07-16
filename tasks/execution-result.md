# TASK-P1-005 执行结果

## Segment A：动画资产需求清单

### 最低合格资产

- 一个可提交的 Humanoid Skeletal Mesh + Skeleton；或含 Skin/Weights/Bones 的原始 FBX
- 至少包含：Idle Loop、Walk/Locomotion Loop、Jump（含空中状态）
- 动画优先全部 In-Place（原地播放），不使用 Root Motion
- 同一 Skeleton 优先；异骨架需提供 Source Skeleton、Reference Pose 与 Retarget 证据
- 主骨链可识别：root → pelvis(hips) → spine → head → arms → legs
- 每个文件需提供：文件名、格式、用途、来源、作者、许可证、能否提交 Git

### 推荐完整集

- Humanoid Skeletal Mesh + Skeleton + PhysicsAsset
- 五段动画：Idle / WalkRun / JumpStart / FallLoop / Land
- A Pose 或 T Pose；需 Retarget 时提供源/目标 Reference Pose
- 材质/贴图可选；不要求 Montage、战斗动画或 VFX

### 拒绝条件

- 商业游戏提取物、来源/作者不明、许可证证据不足
- 禁止商用/修改/公开展示/Git 再分发的资产
- 仅 Static Mesh 无 Skin/骨骼，或无可用 Idle/Locomotion/Air 动画
- `.uasset` 缺必要依赖；异骨架动画缺 Source Skeleton/Reference Pose

### 接受格式与依赖

- UE `.uasset`：注明 UE 版本、Content 相对路径、依赖的 Skeleton/Mesh/PhysicsAsset
- FBX：说明含 Mesh/Skin/Bones/Animation、多 Take、导出版本、单位/Up Axis/Forward Axis/Scale
- 材质/贴图单列

### 许可证证据要求

- 来源、作者、许可证名称/原文
- 商用/修改/再分发/公开展示/署名要求逐项确认
- 购买/下载凭证或许可证截图（敏感信息可遮挡）

### Segment A 交付

Implementation Agent 已提供完整可操作清单。用户可按 Segment B 模板回传候选资产。