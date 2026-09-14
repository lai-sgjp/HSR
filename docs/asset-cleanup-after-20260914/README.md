# 清理后剩余资产清单（未追加授权删除）

生成清单时本地提交：`0691fe397cf3d5506c3f16313e6936d43a4e7640`。Git 状态按本次生成时重新读取；推送是否一致以交付中的远端核验为准。

已确认的 2,093 个文件已完成备份和删除，详见 [清理与复验记录](RESULTS.md) 和 [可筛选清单](index.html)。此表仅列清理后剩余的 1,264 个文件；逐文件路径、引用者、大小、Git 状态和备份要求见 [files.csv](files.csv)。

依赖包含硬引用、软引用和 Asset Manager 管理引用；源码和配置动态目录保守保留。测试/制作脚本独立列为待核实。未登记文件不自动判删。

只有明确确认的 files.csv 具体行才能进入 Editor 引用感知删除；候选中存在互相引用时应按依赖组处理，删除前再次核对当前引用。

|结论|目录|文件数|MiB|tracked / ignored / untracked|
|---|---|---:|---:|---|
|仍被使用|Content/AI/Enemy|2|0.03|2 / 0 / 0|
|仍被使用|Content/Assets/Mannequins|24|78.22|0 / 24 / 0|
|仍被使用|Content/Assets/mmd|372|116.71|0 / 372 / 0|
|仍被使用|Content/Blueprints/Character|1|0.03|1 / 0 / 0|
|仍被使用|Content/Blueprints/Input|1|0.05|1 / 0 / 0|
|仍被使用|Content/Blueprints/UI|1|0.03|1 / 0 / 0|
|仍被使用|Content/Characters/Player|13|23.46|13 / 0 / 0|
|仍被使用|Content/Data/Damage|1|0.00|1 / 0 / 0|
|仍被使用|Content/Data/Drops|1|0.00|1 / 0 / 0|
|仍被使用|Content/Data/Items|9|0.02|9 / 0 / 0|
|仍被使用|Content/Data/RelicSets|1|0.00|1 / 0 / 0|
|仍被使用|Content/Data/Relics|6|0.01|6 / 0 / 0|
|仍被使用|Content/Data/Rewards|1|0.00|1 / 0 / 0|
|仍被使用|Content/Data/Skills|4|0.01|4 / 0 / 0|
|仍被使用|Content/Data/Status|4|0.01|4 / 0 / 0|
|仍被使用|Content/Data/VerticalSlice|58|0.16|58 / 0 / 0|
|仍被使用|Content/GameplayEffects/BP_GE_InitializeCoreAttributes.uasset|1|0.06|1 / 0 / 0|
|仍被使用|Content/GameplayEffects/BP_GE_P6_Heal.uasset|1|0.01|1 / 0 / 0|
|仍被使用|Content/GameplayEffects/BP_GE_P6_UltimateDamage.uasset|1|0.01|1 / 0 / 0|
|仍被使用|Content/GameplayEffects/BP_GE_P6_UltimateEnergyCost.uasset|1|0.01|1 / 0 / 0|
|仍被使用|Content/GameplayEffects/BP_GE_P7_DamageExecution.uasset|1|0.01|1 / 0 / 0|
|仍被使用|Content/GameplayEffects/BP_GE_P7_UltimateEnergyRefund.uasset|1|0.01|1 / 0 / 0|
|仍被使用|Content/GameplayEffects/BP_GE_P8_ToughnessDamage.uasset|1|0.01|1 / 0 / 0|
|仍被使用|Content/GameplayEffects/BP_GE_Test_EnergyBounds.uasset|1|0.02|1 / 0 / 0|
|仍被使用|Content/GameplayEffects/BP_GE_Test_HealthAboveMax.uasset|1|0.01|1 / 0 / 0|
|仍被使用|Content/GameplayEffects/BP_GE_Test_HealthBelowZero.uasset|1|0.01|1 / 0 / 0|
|仍被使用|Content/GameplayEffects/BP_GE_Test_LowerMaxHealth.uasset|1|0.01|1 / 0 / 0|
|仍被使用|Content/GameplayEffects/BP_GE_Test_SpeedBelowZero.uasset|1|0.01|1 / 0 / 0|
|仍被使用|Content/GameplayEffects/GE_CharacterProgression_P11.uasset|1|0.03|1 / 0 / 0|
|仍被使用|Content/GameplayEffects/GE_Equipment_P12.uasset|1|0.03|1 / 0 / 0|
|仍被使用|Content/GameplayEffects/GE_RelicSet_P12_A.uasset|1|0.01|1 / 0 / 0|
|仍被使用|Content/GameplayEffects/GE_Status_AttackUpStack_P9_002.uasset|1|0.01|1 / 0 / 0|
|仍被使用|Content/GameplayEffects/GE_Status_AttackUp_P9.uasset|1|0.01|1 / 0 / 0|
|仍被使用|Content/GameplayEffects/GE_Status_BreakDebuff_P9_003.uasset|1|0.01|1 / 0 / 0|
|仍被使用|Content/GameplayEffects/GE_Status_DamageOverTime_P9_003.uasset|1|0.01|1 / 0 / 0|
|仍被使用|Content/GameplayEffects/GE_Status_DebuffImmunity_P9_004.uasset|1|0.01|1 / 0 / 0|
|仍被使用|Content/Input/IA_Interact.uasset|1|0.00|1 / 0 / 0|
|仍被使用|Content/Input/IA_Jump.uasset|1|0.00|1 / 0 / 0|
|仍被使用|Content/Input/IA_Look.uasset|1|0.00|1 / 0 / 0|
|仍被使用|Content/Input/IA_Move.uasset|1|0.00|1 / 0 / 0|
|仍被使用|Content/Input/IA_UI_Challenge.uasset|1|0.00|1 / 0 / 0|
|仍被使用|Content/Input/IA_UI_CloseToRoot.uasset|1|0.00|1 / 0 / 0|
|仍被使用|Content/Input/IA_UI_Inventory.uasset|1|0.00|1 / 0 / 0|
|仍被使用|Content/Input/IA_UI_Map.uasset|1|0.00|1 / 0 / 0|
|仍被使用|Content/Input/IA_UI_Party.uasset|1|0.00|1 / 0 / 0|
|仍被使用|Content/Input/IA_UI_PauseBack.uasset|1|0.00|1 / 0 / 0|
|仍被使用|Content/Input/IMC_Exploration.uasset|1|0.01|1 / 0 / 0|
|仍被使用|Content/Input/IMC_FrontendNavigation.uasset|1|0.00|1 / 0 / 0|
|仍被使用|Content/Maps/VerticalSlice|3|0.58|3 / 0 / 0|
|仍被使用|Content/Presentation/Animation|103|95.11|0 / 0 / 103|
|仍被使用|Content/Presentation/Blueprints|3|0.07|3 / 0 / 0|
|仍被使用|Content/Presentation/Materials|13|0.12|13 / 0 / 0|
|仍被使用|Content/Presentation/Meshes|15|0.62|15 / 0 / 0|
|仍被使用|Content/Presentation/UI|12|0.71|8 / 0 / 4|
|仍被使用|Content/UI/P17|21|3.94|21 / 0 / 0|
|仍被使用|Content/UI/WBP_AttributeDebug.uasset|1|0.27|1 / 0 / 0|
|仍被使用|Content/UI/WBP_BattleCommandPanel.uasset|1|0.19|1 / 0 / 0|
|仍被使用|Content/UI/WBP_CharacterDetail_P11.uasset|1|0.18|1 / 0 / 0|
|仍被使用|Content/UI/WBP_EquipmentDetail_P12.uasset|1|0.15|1 / 0 / 0|
|仍被使用|Content/UI/WBP_ExplorationHUD.uasset|1|0.06|1 / 0 / 0|
|仍被使用|Content/UI/WBP_Inventory_P13.uasset|1|0.08|1 / 0 / 0|
|仍被使用|Content/UI/WBP_RewardSummary_P13.uasset|1|0.05|1 / 0 / 0|
|待核实|Content/Assets/Mannequins|26|12.86|0 / 26 / 0|
|待核实|Content/Assets/mmd|50|39.19|0 / 50 / 0|
|待核实|Content/Blueprints/Character|1|0.03|1 / 0 / 0|
|待核实|Content/Blueprints/Exploration|2|0.06|2 / 0 / 0|
|待核实|Content/Blueprints/Framework|2|0.05|2 / 0 / 0|
|待核实|Content/Characters/Mannequins|1|15.10|0 / 1 / 0|
|待核实|Content/Data/Characters|2|0.01|2 / 0 / 0|
|待核实|Content/Data/Dialogue|1|0.01|1 / 0 / 0|
|待核实|Content/Data/Encounters|1|0.00|1 / 0 / 0|
|待核实|Content/Data/Enemies|1|0.00|1 / 0 / 0|
|待核实|Content/Data/Maps|10|0.02|10 / 0 / 0|
|待核实|Content/Data/Progression|2|0.01|2 / 0 / 0|
|待核实|Content/Data/Status|1|0.00|1 / 0 / 0|
|待核实|Content/GameplayEffects/GE_StageAttack_P17.uasset|1|0.01|1 / 0 / 0|
|待核实|Content/Maps/Map_Battle.umap|1|0.06|1 / 0 / 0|
|待核实|Content/Maps/Map_BattleTest.umap|1|0.07|1 / 0 / 0|
|待核实|Content/Maps/Map_Exploration_P15_A.umap|1|0.16|1 / 0 / 0|
|待核实|Content/Maps/Map_Exploration_P15_B.umap|1|0.16|1 / 0 / 0|
|待核实|Content/Polytope_Studio/Nature_Free|2|0.00|0 / 2 / 0|
|待核实|Content/Presentation/Animation|283|197.65|7 / 0 / 276|
|待核实|Content/Scrapopolis/Effects|1|1.59|0 / 1 / 0|
|待核实|Content/Scrapopolis/Levels|2|142.13|0 / 2 / 0|
|待核实|Content/Scrapopolis/Materials|38|0.45|0 / 38 / 0|
|待核实|Content/Scrapopolis/Meshes|37|38.98|0 / 37 / 0|
|待核实|Content/Scrapopolis/Textures|86|409.18|0 / 86 / 0|
|待核实|Content/UI/WBP_Reward_Summary_P13.uasset|1|0.04|1 / 0 / 0|
|待核实|Content/__ExternalActors__/ThirdPerson|1|0.00|0 / 1 / 0|

项目外备份尚未执行；待确认后对非 Git 文件建立独立归档及逐文件 SHA-256 清单，校验完成后才删除。缓存、插件、源码、私人文件均不在清理范围。

四人模型、FINAL 移动动画、LIVE 战斗动画、FULL 倒地和 CAST 施法动画、骨架、重定向器及其依赖保留。旧地图或资源包只因名字陈旧不会被判删。

候选行附 SHA-256，确认只适用于该内容版本；后续发生变化的文件需重新核对。
