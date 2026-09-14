"""Keep Blueprint event bindings while providing usable formation controls."""
import unreal,json
h=unreal.MCPythonHelper
bp=unreal.load_asset('/Game/UI/P17/Frontend/WBP_HSRPreBattlePanel_P17')
def find(name): return h.umg_find_widget(bp,name)
def add(kind,name,parent):
    w=find(name)
    if not w:
        result=json.loads(h.umg_add_widget(bp,kind,name,parent))
        if not result.get('success'): raise RuntimeError(str(result))
        h.umg_set_widget_is_variable(bp,name,True)
        w=find(name)
    return w
panel=find('Panel_Border')
slot=panel.get_editor_property('slot')
slot.set_anchors(unreal.Anchors(minimum=unreal.Vector2D(0,0),maximum=unreal.Vector2D(1,1)))
slot.set_offsets(unreal.Margin(0));slot.set_alignment(unreal.Vector2D(0,0))
panel.set_padding(unreal.Margin(160,100,160,100));panel.set_brush_color(unreal.LinearColor(.018,.035,.06,1))
brush=panel.get_editor_property('background');brush.set_editor_property('draw_as',unreal.SlateBrushDrawType.IMAGE);brush.set_editor_property('tint_color',unreal.SlateColor(specified_color=unreal.LinearColor(1,1,1,1)));panel.set_editor_property('background',brush)
for info in json.loads(h.umg_get_widget_info(bp))['widgets']:
    text=find(info['name'])
    if isinstance(text,unreal.TextBlock):
        font=text.get_editor_property('font');font.set_editor_property('size',22);text.set_font(font)
for i in range(4):
    column=find('CandidateSlot_'+str(i));s=column.get_editor_property('slot')
    s.set_size(unreal.SlateChildSize(value=1,size_rule=unreal.SlateSizeRule.FILL));s.set_padding(unreal.Margin(10))
    for name in ['Text_Slot%d_Label'%i,'Text_Slot%d_Character'%i,'Button_ReplaceSlot%d'%i]:
        find(name).get_editor_property('slot').set_padding(unreal.Margin(0,8))
    if i:
        clear=add('Button','Button_ClearSlot%d'%i,'CandidateSlot_'+str(i))
        label=add('TextBlock','Text_ClearSlot%d'%i,'Button_ClearSlot%d'%i)
        label.set_text('移出');label.set_color_and_opacity(unreal.SlateColor(specified_color=unreal.LinearColor(.91,.94,1,1)))
        clear.get_editor_property('slot').set_padding(unreal.Margin(0,8))
for name in ['Text_Title','CandidateSlots','SelectionPanel','Text_CandidateSnapshot','ActionsBox']:
    find(name).get_editor_property('slot').set_padding(unreal.Margin(0,12))
for name in ['Button_Confirm','Button_Cancel']:
    s=find(name).get_editor_property('slot');s.set_size(unreal.SlateChildSize(value=1,size_rule=unreal.SlateSizeRule.FILL));s.set_padding(unreal.Margin(8))
unreal.BlueprintEditorLibrary.compile_blueprint(bp)
unreal.EditorAssetLibrary.save_loaded_asset(bp,only_if_is_dirty=False)
print(json.dumps({'success':True}))
