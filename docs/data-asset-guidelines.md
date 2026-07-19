# Data Asset Guidelines

## 原则

- 静态定义、运行时状态和存档快照相互分离。
- 所有角色、敌人、技能、物品和奖励使用稳定 ID。
- 显示名称使用 `FText`，不得作为主键。
- 不复制商业游戏的专有命名、数值表或成长曲线。

## 四层数据与生命周期边界

| 层 | 作用 | 生命周期 | 可以保存 | 禁止保存 |
|---|---|---|---|---|
| `DataAsset` / `PrimaryDataAsset` | 静态定义，描述“是什么” | 资产生命周期，可被多个 World 引用 | 稳定 ID、静态参数、软资源/地图引用 | 当前 Actor、当前 HP、临时请求、Widget、ASC、GE Handle |
| `Runtime Request` | 本次操作的纯值快照，描述“这一次做什么” | 当前运行会话；需由 `GameInstanceSubsystem` 等跨 World 容器托管才可跨 `OpenLevel` | ID、枚举、`FGuid`、软地图路径、`FTransform` | Actor/Pawn/Controller/ASC/Widget/UObject 实例指针 |
| `ReturnContext` | 本次旅行的回程纯值，描述“回来哪里” | 当前运行会话；由跨 World Runtime 服务托管 | 原地图路径、返回 Transform、Request ID、旅行匹配信息 | 旧 World Actor、旧 ASC、旧 Widget、GE Handle |
| `SaveGame` | 跨游戏会话持久快照，描述“下次启动恢复什么” | 磁盘/存档会话 | 可迁移的 Profile、进度、ID、等级、背包等 DTO | 临时 Pending、ReturnContext、Actor、ASC、Widget、GE Handle |

`OpenLevel` 后旧 World 的 Actor 实例会销毁；纯值结构体不会自动存活，必须由 `GameInstance`/`GameInstanceSubsystem` 持有。新 World 应依据稳定 ID 和定义重新解析/创建运行实例。

## 类型选择

### PrimaryDataAsset

用于具有独立身份、资源引用或需要 Asset Manager 管理的定义，例如角色、敌人、技能、物品和奖励定义。

### DataTable

用于大量结构相同、便于批量编辑和导入的扁平数据。不要在 Row 中建立难以追踪的硬引用网络。

### CurveTable

用于等级成长、经验需求和可视化数值曲线。

### Gameplay Tags

用于可组合的类别和状态语义，例如 Ability、State、Status、Damage 和 Element。Tag 不替代所有结构化字段。

## 引用规则

- 必须常驻的小型核心定义可以使用明确硬引用。
- 大型或按需资源优先考虑软引用。
- 第三方资产必须与原创资产目录隔离。
- 数据定义不得直接保存临时 World Actor 作为持久真源。

## Schema 变更清单

1. 字段是否有安全默认值。
2. 现有 DataAsset/DataTable 是否仍可加载。
3. 稳定 ID 是否保持不变。
4. SaveGame 是否需要版本迁移。
5. UI 和战斗逻辑是否存在硬编码依赖。
6. README、战斗设计和本文档是否需要同步。

## 第三方资产记录字段

- 资源名称与稳定 ID。
- 来源 URL、作者和获取日期。
- 许可证原文或许可证链接。
- 是否允许商用、修改和再分发。
- 署名方式。
- 项目内位置和是否进入公开构建。
