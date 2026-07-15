# HSR 低级模型执行任务模板

> 本文件是高级模型生成低级模型任务卡时的详细素材真源。高级模型完成全局上下文读取与模板填充后，必须把最终契约写入 `tasks/active-task.md`；低级模型只读取活动任务卡，不直接读取本模板或全局文档。

## 使用方法

- 模板 A：纯 Markdown 规划、整理和记录。
- 模板 B：1–5 个文件范围的小型 C++ 功能。
- 模板 C：已有明确错误现象或日志证据的 Debug/修复。
- 每轮只放入一个可独立编译、测试和验收的小任务。
- `【允许修改的文件】` 是硬边界；未列入的文件默认禁止修改。
- 如果任务同时包含大型功能、重构、资源导入和 Debug，必须先拆分。

---

## 模板 A：纯文档任务

```text
# HSR 低级模型执行任务模板 — A：纯文档任务

你正在维护 HSR 项目的 Markdown 文档。本轮只处理文档，不实现 UE 工程或游戏功能。

## 任务参数

- 任务名称：【本轮任务名称】
- 任务背景：【任务背景】
- 目标 Phase：【目标 Phase】
- 允许修改的文件：【允许修改的文件】
- 禁止修改的文件：【禁止修改的文件】
- 预期结果：【预期结果】
- 测试方式：【测试方式】
- 是否涉及 GAS：【是否涉及 GAS】
- 是否涉及 UI：【是否涉及 UI】
- 是否涉及 SaveGame：【是否涉及 SaveGame】
- 是否涉及 DataAsset：【是否涉及 DataAsset】
- 是否涉及 Editor 手动操作：【是否涉及 Editor 手动操作】
- 额外限制：【额外限制】

## 1. 任务目标

只完成【本轮任务名称】。

成功标准：

1. 文档内容与【目标 Phase】和现有架构一致。
2. 保留所有历史记录。
3. 不把规划完成误标为 Gameplay 已实现。
4. 不创建或修改任何 UE 工程、代码、Config、资产或构建产物。
5. 最终结果满足【预期结果】。

## 2. 上下文读取要求

低级模型只读取 `tasks/active-task.md` 作为上下文入口。高级模型必须在生成活动任务卡前读取 agents、todo、worklog、README、learning journal、当前 Phase 和相关专项文档，并把本任务必需的规则、事实和证据写入卡片。

执行时只可读取和修改【允许修改的文件】中列出的 Markdown，以便保留现有内容并完成最小编辑。不得自行读取其他全局文档补充隐含需求；信息不足时停止并请求高级模型补全活动任务卡。

如果【额外限制】禁止运行命令，则不得调用 shell、PowerShell、cmd、Git、构建工具或 Editor。若环境没有非终端文件读取能力，停止并说明限制，不得绕过。

## 3. 修改范围

修改前先向用户说明：

1. 本轮目标。
2. 将创建哪些 Markdown。
3. 将修改哪些 Markdown。
4. 每个文件的用途。
5. 哪些历史内容必须保留。
6. 明确说明不会修改哪些内容。

只允许修改：

【允许修改的文件】

禁止修改：

【禁止修改的文件】

未列入允许清单的文件默认禁止修改。已存在文档必须追加或局部合并，不得整文件覆盖；新文件只有确认不存在时才能创建。

## 4. 禁止事项

- 不创建或修改 `.uproject`、Source、Config、Content、Plugins。
- 不创建 C++、Blueprint、UMG、DataAsset 或 GameplayEffect 资产。
- 不启用或禁用插件。
- 不导入模型、动画、音频、VFX、字体或图标。
- 不创建 Binaries、Intermediate、Saved 或其他构建产物。
- 不实现游戏功能。
- 不修改大量无关文档。
- 不删除、覆盖或重命名文件。
- 不覆盖 worklog 历史。
- 不自动执行 git add、commit、push。
- 不执行 git reset 或 git clean。
- 不对用户已有内容做无关格式化。

## 5. 执行步骤

1. 读取 `tasks/active-task.md`，确认执行模型、授权、允许文件和验收标准；随后只读取允许修改清单中的目标 Markdown。
2. 对比【预期结果】与当前文档。
3. 列出已有内容、缺失内容、冲突描述和必须保留的历史。
4. 说明修改文件和非修改范围。
5. 使用受控文件编辑工具进行最小修改。
6. 为详细规划指定唯一真源。
7. 其他文档只保存摘要、状态、门禁和真源链接。
8. 更新 worklog。
9. 任务状态真实变化时更新 todo_plan。
10. 产生新知识时更新 learning-journal。
11. GAS 新知识更新 gas-notes。
12. 重新读取修改后的文件进行一致性检查。

## 6. UE 反射检查

纯文档任务不创建反射代码。如果文档规划反射类型，检查描述是否包含：

- UCLASS / USTRUCT / UENUM / UINTERFACE。
- GENERATED_BODY。
- UPROPERTY / UFUNCTION。
- `*.generated.h` 为最后 include。
- Blueprint 暴露最小化。

不得在纯文档任务中实际生成这些类型。

## 7. GAS 检查

当【是否涉及 GAS】为“是”时，文档必须检查：

- ASC 放置、Owner Actor、Avatar Actor。
- AttributeSet、GameplayAbility、GameplayEffect 类型和 Tags。
- Cost、Cooldown、Attribute Delegate。
- GAS 与 TurnSystem 边界。
- 世界秒数与回合持续时间的区别。
- DataAsset、Runtime、SaveGame 边界。
- 当前推迟的高级 GAS 内容。

高级模型生成活动任务卡前必须读取 GAS 学习路线，并把与本任务有关的阶段限制写入卡片；低级模型不得自行创造冲突阶段。

## 8. GC 检查

纯文档任务不创建 UObject 引用。若规划引用，必须说明：

- 强、弱或软引用。
- UPROPERTY/TObjectPtr。
- 跨地图禁止保存 Actor、ASC、Widget 和 Active GE Handle。
- Delegate、Timer 和异步回调生命周期。

## 9. Tick 检查

纯文档任务不得新增 Tick。若规划 Tick，必须说明事件方案为何不适用、启停条件、频率、对象数量和性能验证。

## 10. UI 检查

当【是否涉及 UI】为“是”时，必须明确：

- UI 只读取状态和提交命令。
- UI 不直接扣血、发奖励、推进回合、修改任务或写 SaveGame。
- Attribute/Tags 使用 Delegate/Event。
- 不使用 Widget Tick。
- Widget 绑定和解绑成对设计。

## 11. SaveGame / DataAsset 检查

当相关占位符为“是”时，必须区分：

- DataAsset：只读静态定义。
- Runtime State：当前会话状态。
- SaveGame：跨会话持久状态。

禁止规划保存 Actor、Component、Widget、ASC、AttributeSet、Active GE Handle、Ability Instance/Spec Handle、TurnQueue 和其他 World 生命周期对象。

## 12. 文档更新要求

- `worklog.md`：追加目标、检查、决策、修改、未验证和下一步。
- `todo_plan.md`：只更新真实状态。
- `learning-journal.md`：只记录可复习知识。
- `docs/gas-notes.md`：仅记录 GAS 新知识或真实 Debug 结论。
- `README.md`：仅在入口、运行方式、当前状态或公开功能变化时更新。
- `.agents/agents.md`：只加入长期稳定规则。

## 13. 编译步骤

纯文档任务不编译。最终明确写：“本轮为纯文档任务，未执行 UBT、UHT、编译、Editor 或 PIE。”

## 14. 测试步骤

按【测试方式】验证：Markdown 标题、内部链接、中文、术语/Phase 状态、历史保留、未误标 Gameplay 完成、未产生 UE 工程或构建文件。不得运行【额外限制】禁止的命令。

## 15. 最终回复格式

### 本轮完成
- 创建的文件：
- 修改的文件：
- 关键内容：

### 保留内容
- 保留的历史：
- 未触碰的系统：

### 验证结果
- 已验证：
- 未验证：
- 未运行的命令/构建：

### Editor 手动操作
- 本轮是否需要：【是否涉及 Editor 手动操作】
- 如不需要，写“无”。

### 下一步建议
只建议一个与【目标 Phase】相邻的小任务，不自动执行。
```

---

## 模板 B：小型 C++ 功能任务

```text
# HSR 低级模型执行任务模板 — B：小型 C++ 功能任务

你正在为 HSR 实现一个小型、可独立编译和测试的 UE5.6 C++ 功能。每轮只能处理一个任务，不得扩展范围。

## 任务参数

- 任务名称：【本轮任务名称】
- 任务背景：【任务背景】
- 目标 Phase：【目标 Phase】
- 允许修改的文件：【允许修改的文件】
- 禁止修改的文件：【禁止修改的文件】
- 预期结果：【预期结果】
- 测试方式：【测试方式】
- 是否涉及 GAS：【是否涉及 GAS】
- 是否涉及 UI：【是否涉及 UI】
- 是否涉及 SaveGame：【是否涉及 SaveGame】
- 是否涉及 DataAsset：【是否涉及 DataAsset】
- 是否涉及 Editor 手动操作：【是否涉及 Editor 手动操作】
- 额外限制：【额外限制】

## 1. 任务目标

只实现【本轮任务名称】，达到【预期结果】。

本轮必须保持：

- 小改动。
- 少量文件。
- 单一职责。
- 可编译。
- 可测试。
- 初学者可以理解。
- 不实现下一 Phase 的功能。

如果现有工程尚未达到【目标 Phase】的前置条件，停止并报告，不能跳过门禁。

## 2. 上下文读取要求

低级模型只读取 `tasks/active-task.md` 作为上下文入口，并确认执行模型匹配、用户已授权、卡片无占位符且允许文件为精确路径。

实现时只读取和修改【允许修改的文件】列出的目标 `.h/.cpp` 及卡片明确列出的必要 Build/Config 文件。高级模型必须在生成活动任务卡前读取 agents、todo、worklog、Phase、专项设计和实际工程证据，并把必要规则、数据流、API 证据与资产命名写入卡片。

若判断任务必须读取或修改清单外文件，立即停止并请求高级模型补卡和用户扩权；不得自行搜索全仓库扩大上下文。

## 3. 修改范围

修改前先输出：

### 本轮目标

用 1–3 句话说明【本轮任务名称】和成功标准。

### 文件修改清单

逐个列出：

- 文件路径。
- 新建或修改。
- 文件职责。
- 计划修改内容。

### 明确不修改

必须列出：

- 【禁止修改的文件】
- 下一 Phase 系统。
- 无关 Gameplay 系统。
- 无关 Config、插件和资产。
- 不执行的重构。

只能修改：

【允许修改的文件】

出现额外必需文件时，先停止并向用户说明原因，不能自行扩展范围。

## 4. 禁止事项

- 不做无关重构。
- 不批量重命名、移动或格式化文件。
- 不删除文件。
- 不覆盖用户已有代码和文档历史。
- 不执行 `git reset`、`git clean`。
- 不自动执行 `git add`、`commit`、`push`。
- 不导入大量资源。
- 不修改大量 Config。
- 不启用无关插件。
- 不把核心逻辑写进蓝图。
- 不把所有功能塞入 Character、GameMode 或一个 Subsystem。
- 不提前实现下一 Phase。
- 不猜测 UE5.6 API。
- 不通过删除 Binaries、Intermediate、Saved 掩盖真实错误。
- 不因代码较短而省略编译和测试。

## 5. 执行步骤

1. 读取上下文和目标文件。
2. 确认前置 Phase 已完成。
3. 说明目标、文件清单和非修改范围。
4. 画出本任务最小数据流：
   - 谁发起；
   - 谁验证；
   - 谁修改状态；
   - 谁接收结果。
5. 只创建实现当前目标所需的最少类型。
6. 先完成 C++ 核心逻辑。
7. 蓝图只负责配置、派生、动画、UI 和资源绑定。
8. 加入必要日志和失败路径。
9. 检查反射、GC、Tick、GAS、UI 和 Save/Data 边界。
10. 执行编译。
11. 执行专项测试。
12. 修复本轮产生的错误。
13. 更新文档。
14. 给出 Editor 手动配置清单。
15. 最终汇报已验证和未验证内容。

## 6. UE 反射检查

涉及反射时逐项检查：

- `UCLASS` / `USTRUCT` / `UENUM` / `UINTERFACE` 是否必要且正确。
- `GENERATED_BODY` 是否存在。
- `*.generated.h` 是否为最后一个 include。
- UObject 引用是否使用 `UPROPERTY`/`TObjectPtr`。
- Blueprint 暴露是否使用最小权限。
- 玩家文本是否使用 `FText`。
- 稳定 ID 是否使用 `FName`、GameplayTag 或明确 ID 类型。
- Enum/Struct/Property 是否有安全默认值。
- `UFUNCTION` 是否真的需要 `BlueprintCallable`、`BlueprintPure` 或 Blueprint Event。
- 构造函数、CDO、`BeginPlay` 和运行时初始化是否混淆。

在最终回复中给出反射检查结论。

## 7. GAS 检查

当【是否涉及 GAS】为“是”时：

1. 读取 GAS 学习路线中对应 Stage。
2. 说明本任务属于哪个 GAS Stage。
3. 检查：
   - ASC 在哪里；
   - Owner Actor；
   - Avatar Actor；
   - Actor Info 初始化时机；
   - AttributeSet；
   - Ability 是否已经授予；
   - GameplayEffect 类型；
   - Tags 的生产者/消费者；
   - Cost / Cooldown；
   - Action 成功、失败和取消；
   - UI/TurnSystem 如何接收结果。
4. 回合持续时间不得直接使用真实秒数 Duration。
5. Ability 不得直接推进 TurnManager。
6. UI 不得直接 Apply GE。
7. SaveGame 不得保存 GAS Runtime Handle。
8. 单机阶段不擅自加入 RepNotify、RPC、Prediction 或复制模式。
9. GAS API 不确定时检查 UE5.6 实际头文件或官方文档；无法确认时停止并要求用户提供报错，禁止编造。

## 8. GC 检查

涉及 UObject、Actor、Component、Widget、DataAsset 时检查：

- 强引用是否使用 `UPROPERTY`/`TObjectPtr`。
- 观察性引用是否适合 `TWeakObjectPtr`。
- 大型或按需资产是否适合 `TSoftObjectPtr`/`TSoftClassPtr`。
- Delegate 是否需要解绑。
- Timer、Lambda、异步回调是否可能访问销毁对象。
- UObject 的 Outer 是否正确。
- 跨地图对象是否错误保存 World Actor 指针。
- `IsValid` 是否被错误当成生命周期设计替代品。

最终明确列出发现和处理的 GC 风险。

## 9. Tick 检查

默认不新增 Tick。

如果必须使用 Tick，修改前说明：

- 为什么 Delegate、事件、Timer、Animation Notify 或状态切换不适用。
- Tick 对象数量。
- 每帧工作量。
- 启停条件。
- 是否可以降低频率。
- 性能验证方式。

没有充分理由不得开启 Tick。

## 10. UI 检查

当【是否涉及 UI】为“是”时：

- UI 只读取 ViewState、Attribute、Tags 和领域事件。
- UI 只提交 Command。
- UI 不直接修改 Attribute、Turn、Reward、Quest、Inventory 或 SaveGame。
- 不使用 Widget Tick。
- Delegate 必须成对绑定/解绑。
- 地图旅行后不得持有旧 ASC、Actor 或 Subsystem World 引用。
- 蓝图只处理布局、动画和绑定。

## 11. SaveGame / DataAsset 检查

当相关占位符为“是”时：

- DataAsset：静态、只读定义。
- Runtime State：当前会话可变状态。
- SaveGame：跨会话 DTO。
- 保存稳定 ID，不保存显示名称作为主键。
- 不保存 Actor、Component、Widget、ASC、AttributeSet、GE Handle、Ability Spec Handle、TurnQueue。
- 加载后根据 Definition 和 Save DTO 重建 Runtime/GAS。
- Schema 必须有版本或兼容入口。
- 修改字段时更新 `docs/data-asset-guidelines.md` 或 `docs/save-system-design.md`。

## 12. 文档更新要求

代码修改完成后：

- 必须追加 `worklog.md`。
- 进度真实变化时更新 `todo_plan.md`。
- 产生新知识、坑或验证方法时更新 `learning-journal.md`。
- GAS 新知识或 Debug 结论更新 `docs/gas-notes.md`。
- 架构变化更新对应设计文档。
- README 只在功能、运行方式或公开状态变化时更新。
- 不把未验证任务勾选完成。

## 13. 编译步骤

根据当前项目和【测试方式】给出并执行最小相关构建。

默认：

1. 构建 `HSREditor Development Win64`。
2. 先定位第一处真实 UHT/UBT/C++/Link 错误。
3. 修复本轮引入的错误。
4. 不通过删除缓存进行盲目修复。
5. 若构建因环境或权限无法执行，记录完整原因，不声称通过。

执行前说明命令目的；遵守【额外限制】。

## 14. 测试步骤

至少包含：

- 正常路径。
- 一个失败路径。
- 空引用或无效输入。
- 重复调用或幂等性。
- 对象销毁/地图旅行相关路径（如适用）。
- GAS 激活失败/Cost/Tag 路径（如适用）。
- PIE 或自动化测试。
- Editor 手动验证步骤。

按【测试方式】执行。不能执行的测试必须列为“未验证”。

## 15. 最终回复格式

### 本轮目标

- 目标：
- 完成标准：

### 修改内容

- 文件：
- 核心类/类型：
- 数据流：

### UE Editor 手动操作

按顺序列出用户必须完成的资产、蓝图、Project Settings 或地图配置。若无，写“无”。

### 编译与测试

- 已执行：
- 结果：
- 未执行：
- 原因：

### 专项检查

- 反射：
- GAS：
- GC：
- Tick：
- UI：
- SaveGame/DataAsset：

### 文档更新

- worklog：
- todo：
- learning journal：
- GAS/设计文档：

### 未验证内容

逐项列出，禁止用“应该可以”代替验证。

### 下一步建议

只建议一个紧邻的小任务，不自动执行。
```

---

## 模板 C：Debug / 修复任务

```text
# HSR 低级模型执行任务模板 — C：Debug / 修复任务

你正在诊断并修复 HSR 的一个明确错误。本轮先收集证据、定位第一处真实错误，再做最小修复。禁止无证据试错和扩大范围。

## 任务参数

- 任务名称：【本轮任务名称】
- 任务背景：【任务背景】
- 目标 Phase：【目标 Phase】
- 允许修改的文件：【允许修改的文件】
- 禁止修改的文件：【禁止修改的文件】
- 预期结果：【预期结果】
- 测试方式：【测试方式】
- 是否涉及 GAS：【是否涉及 GAS】
- 是否涉及 UI：【是否涉及 UI】
- 是否涉及 SaveGame：【是否涉及 SaveGame】
- 是否涉及 DataAsset：【是否涉及 DataAsset】
- 是否涉及 Editor 手动操作：【是否涉及 Editor 手动操作】
- 额外限制：【额外限制】

## 1. 任务目标

只诊断和修复【本轮任务名称】。

必须达到：

- 找到第一处真实错误。
- 给出证据支持的根因。
- 实施最小修复。
- 使用相同复现步骤验证。
- 不顺手修复无关警告或重构代码。

如果证据不足，先请求用户提供完整报错、调用栈、日志、复现步骤或 Editor 配置，不能猜测。

## 2. 上下文读取要求

低级模型只读取 `tasks/active-task.md` 作为上下文入口。活动任务卡必须内嵌完整错误输出、稳定复现步骤、第一处真实错误、根因假设、允许文件和原路径复验要求。

诊断与修复时只读取【允许修改的文件】以及卡片明确列为只读证据的源文件、Target/Build/Config 或日志。高级模型必须在生成任务卡前完成 agents、todo、worklog、Phase 和专项设计读取，并把必要结论写入卡片。

证据不完整或需要读取/修改清单外文件时立即停止，请求高级模型补卡或用户扩权。

不要只看最后一行错误。优先找第一处导致后续连锁错误的真实错误。

## 3. 修改范围

诊断后、修改前输出：

### 现象

- 实际行为：
- 预期行为：
- 稳定复现步骤：

### 错误分类

从以下类型中选择：

- UHT
- UBT
- C++ 编译
- 链接
- 模块/插件
- Editor/资产
- Blueprint 配置
- GAS 初始化
- Ability 激活
- GameplayEffect/Attribute
- Gameplay Tags
- GC/生命周期
- Tick/性能
- UI/Delegate
- Save/Data
- 战斗状态机
- 其他

### 关键证据

- 第一处真实错误：
- 文件/行：
- 调用栈或日志：
- 为什么它是根因而不是后续错误：

### 文件修改清单

只列出修复根因需要的文件和职责。

允许修改：

【允许修改的文件】

禁止修改：

【禁止修改的文件】

如果根因位于禁止文件或需要新增文件，停止并请求授权。

## 4. 禁止事项

- 不删除文件。
- 不执行 `git reset`、`git clean`。
- 不自动执行 `commit`/`push`。
- 不清空或删除 Binaries、Intermediate、Saved、DerivedDataCache，除非用户明确批准且证据表明必要。
- 不同时尝试多个互不相关的修复。
- 不升级引擎、插件或依赖。
- 不批量修改 Config。
- 不重构无关类。
- 不通过关闭警告、注释代码或吞掉错误伪装修复。
- 不编造 UE5.6 API。
- 不把 C++ 问题转移到蓝图核心逻辑。
- 不声称未执行的测试通过。

## 5. 执行步骤

1. 收集完整错误和复现步骤。
2. 读取项目规则、相关文档和源文件。
3. 定位第一处真实错误。
4. 建立 1–3 个有证据的原因假设。
5. 通过静态检查、日志或最小复现排除错误假设。
6. 确认根因。
7. 列出修改文件和非修改范围。
8. 实施最小修复。
9. 使用原复现步骤验证。
10. 执行最小相关编译/测试。
11. 检查修复是否引入新错误。
12. 更新 worklog 和必要学习文档。
13. 若仍失败，报告新增证据和下一条最可能检查点，不进行随机试错。

## 6. UE 反射检查

若涉及 UHT 或反射，检查：

- `UCLASS` / `USTRUCT` / `UENUM` / `UINTERFACE`。
- `GENERATED_BODY`。
- `.generated.h` 是否最后 include。
- `UPROPERTY`/`UFUNCTION` 宏位置和参数。
- 反射类型是否完整可见。
- 前向声明是否足够。
- `BlueprintCallable`/`Pure`/`Event` 签名是否合法。
- `UENUM` 是否使用支持的底层类型。
- `USTRUCT` 属性是否有安全默认值。
- `Build.cs` 是否包含定义类型所需模块。
- 循环 include 和生成头路径。

修复后优先重新运行 UHT/Editor Target Build。

## 7. GAS 检查

当【是否涉及 GAS】为“是”时，按以下顺序检查：

1. ASC 是否存在。
2. ASC 是否注册/初始化。
3. Owner Actor 和 Avatar Actor。
4. `InitAbilityActorInfo` 调用时机和次数。
5. Ability 是否已授予，Spec 是否有效。
6. Ability Tags、Required/Blocked Tags。
7. Cost 和 Cooldown。
8. Target 是否有效。
9. GameplayEffect 类型和 Modifier。
10. SetByCaller Tag/Magnitude。
11. AttributeSet 和 Meta Attribute。
12. `PreAttributeChange`/`PostGameplayEffectExecute`。
13. ActionResolved 是否发送。
14. TurnManager 是否错误推进或停滞。
15. UI 是否只是显示问题。
16. 地图旅行后是否持有旧 ASC。
17. 单机阶段是否误加入复制/预测配置。

如果 API 不确定，检查 UE5.6 实际头文件或官方文档；仍无法确认时要求用户提供完整编译错误，禁止猜测。

## 8. GC 检查

检查：

- UObject 引用是否受 `UPROPERTY`/`TObjectPtr` 保护。
- 弱观察引用是否误用裸指针。
- Delegate 是否在对象销毁后仍绑定。
- Timer/异步/Lambda 是否捕获已销毁对象。
- UObject Outer 是否错误。
- 地图旅行后是否保存旧 World Actor/ASC/Widget。
- DataAsset 是否被运行时修改。
- `IsValid` 是否只掩盖错误生命周期。

GC 修复不能简单表现为“到处加 `IsValid`”。

## 9. Tick 检查

如果问题与 Tick 有关：

- Tick 是否真的启用。
- Tick 是否应该由事件/Delegate 替代。
- 是否在 Tick 搜索 Actor、更新 UI、加载资产或执行复杂战斗。
- Tick 是否在不需要时关闭。
- 是否存在帧率相关逻辑。
- 是否需要 DeltaSeconds。
- 是否存在对象数量放大问题。

新增 Tick 作为修复前必须说明理由。

## 10. UI 检查

当【是否涉及 UI】为“是”时，检查：

- Widget 是否持有旧 Actor/ASC。
- Delegate 是否重复绑定或未解绑。
- ViewModel 是否初始化。
- UI 是否在 Tick 轮询。
- InputMode 和 Focus。
- UI 是否直接修改核心状态。
- Widget 动画是否错误充当规则完成信号。
- UI 显示值与 ASC/Runtime Snapshot 是否一致。

## 11. SaveGame / DataAsset 检查

检查：

- DataAsset 是否被当作 Runtime 修改。
- SaveGame 是否保存 UObject/Actor/ASC/Handle。
- Definition ID 是否有效。
- SaveVersion 是否匹配。
- 加载后是否重复应用 GAS Effect。
- Runtime 缓存是否在加载前清理。
- 缺失 Definition 是否有明确错误。
- 保存事务失败是否保留旧存档。
- 显示名称是否错误充当主键。

## 12. 文档更新要求

修复完成后：

- `worklog.md` 追加现象、第一处错误、证据、根因、修复、验证和遗留。
- `todo_plan.md` 只在真实进度变化时更新。
- `learning-journal.md` 记录可复用的排错知识。
- GAS 问题更新 `docs/gas-notes.md`。
- 战斗架构问题更新 battle design。
- Data/Save/UI 规范变化更新对应文档。
- 不因修复一个 Bug 修改 agents，除非形成长期稳定规则。

## 13. 编译步骤

根据错误类型执行最小验证：

- UHT 错误：构建 Editor Target，先验证 Header Tool。
- C++ 错误：构建受影响模块/Editor Target。
- 链接错误：检查声明、定义、模块依赖后重建。
- GAS/运行时错误：先编译，再执行最小 PIE 复现。
- Blueprint 配置错误：编译 C++ 后列出 Editor 手动验证。
- Save/Data 错误：执行往返测试或最小自动化测试。

记录：

- 执行内容。
- 第一处错误。
- 修复后结果。
- 未验证内容。

## 14. 测试步骤

必须使用与原问题相同的复现步骤。

至少执行：

1. 原失败路径。
2. 正常路径。
3. 一个边界或空值路径。
4. 重复调用。
5. 对象销毁或地图旅行（如相关）。
6. 重新启动 Editor 或重新加载存档（如相关）。
7. 专项自动化或 PIE 测试。

如果测试不能执行，明确说明原因和用户需要手动执行的步骤。

## 15. 最终回复格式

### 错误判断

- 类型：
- 现象：
- 第一处真实错误：

### 根因

- 根因：
- 关键证据：
- 为什么会导致当前现象：

### 修复内容

- 修改文件：
- 最小修改：
- 未修改内容：

### UE Editor 手动操作

按顺序列出；若无则写“无”。

### 编译与测试

- 执行：
- 结果：
- 原复现是否通过：
- 未验证：

### 专项检查

- 反射：
- GAS：
- GC：
- Tick：
- UI：
- Save/Data：

### 文档更新

- worklog：
- todo：
- learning journal：
- 专项文档：

### 如果仍失败

只列出下一条最可能检查点和需要用户提供的信息，不继续随机修改。

### 下一步建议

只建议与当前根因直接相关的一个小任务，不自动执行。
```

---

## 固定使用原则

- 模板 A 用于纯 Markdown 规划和记录。
- 模板 B 用于 1–5 个文件范围内的小型 C++ 功能。
- 模板 C 用于已有明确现象或错误证据的修复。
- 如果一个任务同时包含大型功能、重构、资源导入和 Debug，必须先拆分，不能直接交给低级模型。
- 每个模板中的“允许修改文件”是硬边界；发现需要扩大范围时必须停止并请求授权。
- 所有占位符必须在交付任务前替换，不得把未填写模板直接交给执行模型。
- 没有明确列入允许清单的文件默认禁止修改。
- 每轮只能执行一个能独立验收的小任务。
- 编译、测试、Editor 和文档状态必须按实际结果汇报，禁止把未验证内容描述为完成。
