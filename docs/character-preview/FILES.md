# Character Preview 文件清单

仅本轮任务变更。开始时已有 8 个 VRM4U DLL 原样保留且不纳入提交；原始模型/动画未改动。

| 状态 | 文件 |
|---|---|
| 新增 | `CHARACTER_PREVIEW_INTERVIEW.md` |
| 修改 | `Content/Data/VerticalSlice/Characters/DA_Character_EvernightMoon.uasset` |
| 修改 | `Content/Data/VerticalSlice/Characters/DA_Character_Huohua.uasset` |
| 修改 | `Content/Data/VerticalSlice/Characters/DA_Character_Remiel.uasset` |
| 修改 | `Content/Data/VerticalSlice/Characters/DA_Character_Verina.uasset` |
| 新增 | `Content/UI/CharacterPreview/M_CharacterPreview.uasset` |
| 修改 | `Content/UI/P17/Character/WBP_CharacterShell_P17.uasset` |
| 修改 | `README.md` |
| 修改 | `Source/HSR/Data/Definitions/HSRCharacterDefinition.h` |
| 新增 | `Source/HSR/Tests/HSRCharacterPreviewTests.cpp` |
| 新增 | `Source/HSR/UI/Character/HSRCharacterPreviewWidget.cpp` |
| 新增 | `Source/HSR/UI/Character/HSRCharacterPreviewWidget.h` |
| 修改 | `Source/HSR/UI/Character/HSRCharacterShellWidget.cpp` |
| 修改 | `Source/HSR/UI/Character/HSRCharacterShellWidget.h` |
| 新增 | `Source/HSR/UI/Character/Preview/HSRCharacterPreviewSession.cpp` |
| 新增 | `Source/HSR/UI/Character/Preview/HSRCharacterPreviewSession.h` |
| 新增 | `Source/HSR/UI/Character/Preview/HSRCharacterPreviewStage.cpp` |
| 新增 | `Source/HSR/UI/Character/Preview/HSRCharacterPreviewStage.h` |
| 新增 | `Tools/Presentation/author_character_preview.py` |
| 新增 | `Tools/Presentation/character_preview_bridge.py` |
| 新增 | `Tools/Presentation/inspect_character_preview_input.py` |
| 新增 | `Tools/Presentation/mouse_character_preview.py` |
| 新增 | `Tools/Presentation/profile_character_preview_pie.py` |
| 新增 | `Tools/Presentation/run_character_preview_tests.py` |
| 新增 | `Tools/Presentation/start_character_preview_pie.py` |
| 新增 | `Tools/Presentation/summarize_character_preview_profile.py` |
| 新增 | `Tools/Presentation/verify_character_preview_edges_pie.py` |
| 新增 | `Tools/Presentation/verify_character_preview_mouse.py` |
| 新增 | `Tools/Presentation/verify_character_preview_pie.py` |
| 新增 | `docs/character-preview/ARCHITECTURE.md` |
| 新增 | `docs/character-preview/FILES.md` |
| 新增 | `docs/character-preview/PERFORMANCE.md` |
| 新增 | `docs/character-preview/VALIDATION.md` |

二进制作者/来源：原 4 个角色 DA 仅增加 PreviewIdle/yaw 配置；原 CharacterShell WBP 增加宿主及布局、绑定材质；新 Material 由 Editor MaterialEditingLibrary 创建。操作脚本 `Tools/Presentation/author_character_preview.py`，原资产备份与 SHA-256 见 `Saved/CharacterPreview/BeforeAssets/`、`asset_wiring.json`。RT 为运行时 transient，无 RT 资产文件。

本地运行截图、日志、CSV、缓存和插件 DLL 不作为源码交付；证据位于 `Saved/CharacterPreview` 与 `Saved/Logs`。当前模型和 idle 属于原有本地忽略资产，干净 public clone 不会自动带齐这些模型；本功能没有隐式下载替代资源。
