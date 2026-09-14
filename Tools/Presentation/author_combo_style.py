import unreal,json
h=unreal.MCPythonHelper
bp=unreal.load_asset('/Game/UI/P17/Frontend/WBP_HSRPartyPanel_P17')
for i in range(4):
    w=h.umg_find_widget(bp,'ComboBoxString_Slot'+str(i))
    w.set_editor_property('foreground_color',unreal.SlateColor(specified_color=unreal.LinearColor(.91,.94,1,1)))
    style=w.get_editor_property('widget_style');combo=style.get_editor_property('combo_button_style');button=combo.get_editor_property('button_style')
    for state,color in [('normal',(.025,.055,.09,1)),('hovered',(.09,.24,.31,1)),('pressed',(.43,.3,.11,1))]:
        brush=button.get_editor_property(state);brush.set_editor_property('tint_color',unreal.SlateColor(specified_color=unreal.LinearColor(*color)));button.set_editor_property(state,brush)
    combo.set_editor_property('button_style',button);style.set_editor_property('combo_button_style',combo);w.set_editor_property('widget_style',style)
unreal.BlueprintEditorLibrary.compile_blueprint(bp);unreal.EditorAssetLibrary.save_loaded_asset(bp,only_if_is_dirty=False)
print(json.dumps({'success':True}))
