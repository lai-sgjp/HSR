import unreal,json
unreal.SystemLibrary.execute_console_command(unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world(),'Automation RunTests HSR.Presentation+HSR.Equipment+HSR.UI.Inventory+HSR.UI.RelicEquipment+HSR.UI.Presentation+HSR.UI.Party+HSR.UI.CharacterShell+HSR.UI.Save+HSR.UI.RewardNotification+HSR.UI.PreBattleCandidate+HSR.GAS.HealPresentation+HSR.Map.SafePlacement+HSR.Save.MapV5Integration+HSR.Save.QuestCanonicalRestore+HSR.Battle.CasualtyTurn+HSR.InteractionBattle.PersistentCompletionAdmission')
print(json.dumps({'success':True,'queued':True}))
