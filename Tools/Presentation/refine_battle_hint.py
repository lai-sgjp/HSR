import unreal,json
bp=unreal.load_asset('/Game/UI/WBP_BattleCommandPanel')
w=unreal.MCPythonHelper.umg_find_widget(bp,'PR_Help')
w.slot.set_position(unreal.Vector2D(-570,-270))
unreal.MCPythonHelper.umg_find_widget(bp,'TXT_DisabledReason').slot.set_position(unreal.Vector2D(-530,-330))
unreal.MCPythonHelper.umg_find_widget(bp,'TXT_Result').set_editor_property('justification',unreal.TextJustify.CENTER)
unreal.BlueprintEditorLibrary.compile_blueprint(bp)
unreal.EditorAssetLibrary.save_loaded_asset(bp,only_if_is_dirty=False)
print(json.dumps({'success':True,'hint_above_commands':True}))
