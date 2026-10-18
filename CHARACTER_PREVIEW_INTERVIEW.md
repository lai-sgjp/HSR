# HSR 角色 3D 预览：面试复述

依据本仓库当前 `UHSRCharacterPreviewWidget`、`UHSRCharacterPreviewSession`、`AHSRCharacterPreviewStage` 真实实现编写。设计细节见 [架构与使用文档](docs/character-preview/ARCHITECTURE.md)，测试证据以 [VALIDATION.md](docs/character-preview/VALIDATION.md) 为准。本文的“实现”指源码和资源接线，不自动代表 PIE、打包、视觉和性能验收全部通过。

## 30 秒介绍

我把实时 3D 预览接进项目已有的角色面板。角色列表和当前选择继续来自原来的 Profile/Party ViewModel，预览只接收角色 ID，异步读取定义里的 Mesh 和专用待机动画。一个没有 ASC、碰撞和 AI 的轻量 Actor 承载模型，SceneCapture2D 只渲染这个模型到 RenderTarget，再由 UI 材质显示在 UMG 中。拖动和滚轮只改变展示变换。请求 generation 防止快速切换串图，关闭或世界清理会取消请求、停止捕获并销毁展示对象。

## 90 秒讲解

这个功能复用了现有角色界面和数据权威。角色面板的 ViewModel 从 Profile 子系统生成可选角色列表，默认选中队伍活动角色，Widget 将选中 ID 交给自己拥有的预览 session。session 验证档案和定义，通过 StreamableManager 异步加载 Mesh 与专用 idle；每次选择递增请求 generation，回调还检查弱对象、状态和 World，因此旧加载不会覆盖新选择，关闭后也不会再生成 Actor。

展示采用同 World 的轻量 stage，不生成真实战斗 Character，所以不会初始化 GAS、装备投影或 AI。Mesh 设置为仅 SceneCapture 可见，Capture 的 ShowOnly 列表只包含该 Mesh；专用光照使用独立 lighting channel，不靠移到远处来隐藏。模型按真实 Bounds 居中，再结合包围球、FOV 和控件宽高比计算适配距离。左拖旋转展示 pivot，滚轮改变有限范围内的相机距离，重置恢复默认构图。

渲染链是 SceneCapture2D → 复用的 BGRA8 RenderTarget → Opaque UI 材质 → UMG Image，使用独立深色背景，不宣称透明合成。现有角色菜单会暂停 World，所以我用 Core ticker 的实时时间手动采样 idle，并禁止动画 Notify；约每 1/30 秒最多捕获一次，静态模型只在 dirty 时捕获。关闭时失效化请求、移除 ticker 和委托、停止 Capture、销毁 stage 并释放 RT 引用。编译、资产保存、运行、打包和性能都有独立证据边界，实际验收以本次测试记录为准。

## 1. UI 点击到 RenderTarget 的真实调用链

```text
Tab / 暂停菜单「角色」
  → AHSRPlayerController::RequestOpenCharacterDetailScreen
  → UHSRUIManagerSubsystem::OpenFrontendModule(Character)
  → OpenCharacterDetailInternal
  → WBP_CharacterShell_P17 / UHSRCharacterShellWidget::NativeConstruct
  → 原 CharacterShell ViewModel 构建并广播快照
  → UHSRCharacterShellWidget::HandleShellChanged
  → UHSRCharacterPreviewWidget::SetCharacter(SelectedCharacterId)
  → UHSRCharacterPreviewSession::RequestCharacter
  → Profile::GetProfileSnapshot + GetDefinition
  → StreamableManager::RequestAsyncLoad
  → CompleteLoad(Generation)
  → Spawn AHSRCharacterPreviewStage / Configure
  → TickPreview → UpdateAnimation → Capture
  → SceneCaptureComponent2D::CaptureScene
  → TextureRenderTarget2D
  → PreviewTexture 参数 / UI MID / UMG Image
```

角色切换仍由原 `UHSRCharacterShellViewModel::SelectCharacter` 校验并发布快照，没有新的角色列表数据源。预览要求 ID 对应已有档案，不要求世界中存在该角色 Pawn。

## 2. 为什么选择 SceneCapture2D？

它能在运行时用自己的视角和过滤规则再次渲染场景，并把结果写入 GPU RenderTarget，UMG 可以将其作为纹理呈现。模型动画、旋转和相机距离都是真实 3D 状态；不是把预先截图切换成另一张图。现有 Engine/UMG/Slate Runtime 依赖就能实现，不引入编辑器展示模块。

代价是额外可见性判断、蒙皮/绘制和渲染 pass，因此必须控制更新频率、分辨率及后处理。当前关闭自动捕获，只保留 session 调度的手动 `CaptureScene()`，避免手动与自动重复渲染。

## 3. 为什么不用主游戏摄像机？

主相机属于真实探索体验，画面包含场景和实际 Pawn，其位置、曝光、输入及生命周期受玩法控制。角色页可能选择一个未在世界生成的其他角色，并需要独立构图与旋转。预览因此拥有自己的 Capture 和纯展示 Mesh，而不移动主相机、不改真实 Pawn 朝向。

## 4. 为什么不复制真实战斗 Character？

本项目 `AHSRCharacterBase::SetProjectedCharacterId` 会把基础属性写入 ASC 并投影装备；真实 Character 还有游戏框架、GAS、交互或移动职责。仅为显示模型实例化这条业务链会产生不必要的副作用。

当前 stage 是轻量 AActor，仅有 SkeletalMesh、pivot、Capture 和灯光，关闭碰撞/复制/导航影响/物理，没有 ASC 或 AI，使用同一个只读定义解析资源，业务状态保持在原子系统。

## 5. 同 World stage 和独立 PreviewScene 如何取舍？

当前采用同 World stage，主要原因是运行时集成简单、与地图和组件生命周期一致，不需要另一个 World 的初始化、时间推进与销毁，也不会引入 UnrealEd/AdvancedPreviewScene。

隔离依赖 `SetVisibleInSceneCaptureOnly(true)`、Capture ShowOnly 清单、无业务 Actor 和专用 lighting channel 2；不依赖离屏远距离。共享渲染场景仍有边界：未来其他对象使用 channel 2 或增加其他 Capture 时要重新审计。独立 runtime scene 隔离更强，适合复杂多窗口编辑展示，但当前没有实现。

## 6. RenderTarget 的原理与颜色处理？

RT 是由 GPU 渲染写入、随后被 UI shader 采样的纹理。当前 RT 为 `PF_B8G8R8A8`、Gamma 2.2、无自动 mip；Capture 使用 FinalColorLDR、手动曝光和精简后处理。Opaque UI Domain 材质将 `PreviewTexture` 的 RGB 显示为最终颜色，不把 LDR Alpha 当作角色透明遮罩。

尺寸跟随 UI 宽高比，高度以 768 为起点、最长边限制 1024。同一打开会话中切换角色复用 RT，布局变化才 resize。512×768 的颜色面格式成本约 1.5 MiB，但这不包括中间缓冲、深度和驱动开销，不能当作实测总显存。

## 7. UObject GC 和异步资源如何管理？

Widget 用 UPROPERTY/TObjectPtr 持有 session；session 持有 stage、RT 和加载中的定义，Mesh 组件持有加载完成的 Mesh/动画。World/Profile 通过 TWeakObjectPtr 观察，不延长宿主生命。StreamableHandle 是非 UObject 的共享句柄，结束或取消时 reset。

TObjectPtr 是 UE GC 可追踪引用，不是 unique_ptr，不负责独占对象。对象生命周期还要遵守 Actor Destroy、World 清理及 Widget 拆除的业务边界；不使用 AddToRoot 掩盖所有权问题。

## 8. 如何避免 A 加载慢、覆盖 B 的竞态？

每次替换或取消先递增 `RequestGeneration` 并取消旧 handle；回调捕获自己的 generation，执行时要求它仍等于当前值、状态仍为 Loading、World 仍有效。委托本身是弱 UObject 委托。即使 cancel 没能阻止已排队回调，旧请求也无法安装结果。

当前 Loading/Ready 中重复选择同一 ID 直接返回；无效 ID、空 Mesh、加载或生成失败进入 Error，界面得到明确提示。Loading 广播可能触发同步重入，因此广播后也再次检查 generation，而不是默认监听者只会读数据。

## 9. 打开、关闭和关卡切换如何收尾？

打开创建 session、校验并异步加载；Ready 后才开启实时调度。Widget NativeDestruct 解绑 OnChanged 并 Close：使请求失效、取消加载、停止 Core ticker、移除 WorldCleanup 委托、停止 Capture、销毁 stage、清 RT 引用。WorldCleanup 针对正确的 World 重复走幂等清理，stage EndPlay 也停止组件。

BeginDestroy 是兜底；GC 期间避免直接改 World Actor 列表，必要时用弱引用排到游戏线程销毁。正常操作应经过显式 Close，不能只等 GC 扫描。

## 10. 暂停、动画和性能如何处理？

项目现有角色菜单会暂停 World，因此 World Timer 不适用。session 使用 Core ticker 实时时间，按约 30 Hz 上限更新；动画以单序列手动定位，禁用自动组件 Tick、战斗 AnimBP、PostProcess AnimBP、布料和 Notify。这样不恢复大世界时间，也不让动画触发音频或业务事件。

有动画则采样并 Capture；无动画仅 dirty 时 Capture；关闭后停止调度并移除 stage。当前没有实现动态分辨率、捕获频率设置或跨打开会话 RT 池。Game Thread、Render Thread、GPU 的实际耗时必须以固定环境的 profile 记录回答；不能以 30 Hz 配置推导性能已达标。

## 11. 自动构图及输入如何保证稳定？

读取真实 Mesh Bounds，先按 Bounds 中心校正 pivot；用包围球与水平/垂直较小视角求完整容纳距离，加 12% 余量。球体对水平旋转保持保守 framing，适合不同身高和宽高比；异常 Bounds 有回退。距离限制避免进入模型和无限缩远。

拖动使用局部像素位移，不乘帧时间；滚轮改变相机距离；释放、离开区域及捕获丢失结束拖动。输入只在预览区域消费，UIManager 保持输入模式、鼠标焦点、Esc/Back 及暂停所有权。宽高比变化触发 RT resize 与重新构图。

## 12. 换装、动作和多角色怎样扩展？

以下是未来设计，不是现有功能：

- 换装：装备子系统输出外观快照，stage 增加只展示的附属 Mesh；装备权威和存档格式保持在原系统。目前装备定义没有外观 Mesh。
- 动作预览：增加可选动作定义/选择，用相同 generation 和无 Notify 采样切换，补动作 Bounds、循环和加载优先级，不能直接启用真实 GameplayAbility。
- 多角色：扩展 stage 组件集合、联合 Bounds、ShowOnly 列表和相机排列；重新评估蒙皮与 Capture 成本。
- 更强隔离或透明背景：分别验证独立 runtime scene、Alpha 渲染源与 UI 混合链路，不能只改一个材质选项就宣称完成。

## 证据与资产边界

源码已实现的能力与最终实际验证结果分开记录：[VALIDATION.md](docs/character-preview/VALIDATION.md) 是通过/失败/未运行、日志、截图、性能和非编辑器运行状态的依据。`Saved/CharacterPreview/asset_wiring.json` 记录 Editor 接线和资源映射，仅能证明该层操作。

目前展示复用项目本地已有四个 MMD 模型及匹配 idle；没有下载新角色。它们的本地使用和技术测试不证明公开再分发授权。面试应说明这是本地展示资产，不能把商业角色和相应模型称为本人原创，也不能把本地包成功当作可以公开发布。

最短体验：默认 `Map_ObservationCar` → Play → Tab → 角色 → 左侧切换 → 预览区左拖、滚轮、重置 → Esc 返回或 X 关闭。实际可复现程度仍以最终验证记录为准。
