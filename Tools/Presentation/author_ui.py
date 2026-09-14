import json
import unreal

helper=unreal.MCPythonHelper
paths=unreal.EditorAssetLibrary.list_assets('/Game/UI',recursive=True,include_folder=False)
changed=[]
for path in paths:
    bp=unreal.load_asset(path)
    if not isinstance(bp,unreal.WidgetBlueprint): continue
    info=json.loads(helper.umg_get_widget_info(bp))
    for entry in info.get('widgets',[]):
        w=helper.umg_find_widget(bp,entry['name'])
        if isinstance(w,unreal.TextBlock):
            w.set_is_enabled(True)
            font=w.get_editor_property('font'); font.set_editor_property('size',28 if entry['name'] in ['TXT_Title','Text_Title'] else 20); w.set_font(font)
            w.set_color_and_opacity(unreal.SlateColor(specified_color=unreal.LinearColor(.91,.94,1,1)))
        elif isinstance(w,unreal.Button):
            w.set_color_and_opacity(unreal.LinearColor(1,1,1,1))
            style=w.get_editor_property('widget_style')
            for state,color in [('normal',(.025,.055,.09,.94)),('hovered',(.09,.24,.31,1)),('pressed',(.43,.3,.11,1))]:
                brush=style.get_editor_property(state); brush.set_editor_property('tint_color',unreal.SlateColor(specified_color=unreal.LinearColor(*color))); style.set_editor_property(state,brush)
            w.set_editor_property('widget_style',style)
        elif isinstance(w,unreal.Border):
            w.set_brush_color(unreal.LinearColor(.018,.035,.06,.94))
            w.set_content_color_and_opacity(unreal.LinearColor(1,1,1,1))
        elif isinstance(w,unreal.ProgressBar):
            w.set_fill_color_and_opacity(unreal.LinearColor(.25,.8,.74,1))
        elif isinstance(w,unreal.EditableTextBox):
            w.set_foreground_color(unreal.LinearColor(.91,.94,1,1))
            style=w.get_editor_property('widget_style')
            for state in ['background_image_normal','background_image_hovered','background_image_focused','background_image_read_only']:
                brush=style.get_editor_property(state)
                brush.set_editor_property('tint_color',unreal.SlateColor(specified_color=unreal.LinearColor(.025,.05,.08,1)))
                style.set_editor_property(state,brush)
            w.set_editor_property('widget_style',style)
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    unreal.EditorAssetLibrary.save_loaded_asset(bp,only_if_is_dirty=False)
    changed.append(str(path))

# Preserve existing binding names/event graphs; relocate controls and add native card hosts.
bp=unreal.load_asset('/Game/UI/WBP_BattleCommandPanel')
info=json.loads(helper.umg_get_widget_info(bp)); root=info['root_widget']
def add(typ,name,parent=None):
    w=helper.umg_find_widget(bp,name)
    if not w:
        result=json.loads(helper.umg_add_widget(bp,typ,name,parent or root))
        if not result.get('success'): raise RuntimeError(str(result))
        helper.umg_set_widget_is_variable(bp,name,True)
        w=helper.umg_find_widget(bp,name)
    return w
def position(w,x,y,width,height,ax=0,ay=0):
    slot=w.get_editor_property('slot')
    if isinstance(slot,unreal.CanvasPanelSlot):
        slot.set_anchors(unreal.Anchors(minimum=unreal.Vector2D(ax,ay),maximum=unreal.Vector2D(ax,ay)))
        slot.set_position(unreal.Vector2D(x,y)); slot.set_size(unreal.Vector2D(width,height))
        slot.set_auto_size(False)
        slot.set_alignment(unreal.Vector2D(0,0))
    w.set_visibility(unreal.SlateVisibility.VISIBLE)
for typ,name,rect in [
 ('HorizontalBox','PR_Party',(160,-170,660,145,0,1)),
 ('HorizontalBox','PR_Enemies',(-460,24,900,145,.5,0)),
 ('VerticalBox','PR_Order',(24,110,145,560,0,0)),
 ('HorizontalBox','PR_Skills',(-550,-190,520,110,1,1))]:
    position(add(typ,name),*rect)
keep={'TXT_CurrentActor':(30,30,400,40,0,0),'TXT_SkillPoints':(-500,-235,460,36,1,1),
 'BTN_Execute':(-220,-60,190,45,1,1),'TXT_DisabledReason':(-530,-330,500,45,1,1),
 'TXT_Result':(-250,-60,500,75,.5,.5),'BTN_ResultConfirm':(-130,45,260,65,.5,.5),
 'TXT_Presentation':(-350,-340,700,50,.5,1),'TXT_Weakness':(-350,172,700,36,.5,0)}
for entry in info['widgets']:
    w=helper.umg_find_widget(bp,entry['name'])
    if entry['name'] in keep:
        if entry.get('parent')!=root: helper.umg_reparent_widget(bp,entry['name'],root)
        position(w,*keep[entry['name']])
    elif entry.get('parent')==root and not entry['name'].startswith('PR_'):
        w.set_visibility(unreal.SlateVisibility.COLLAPSED)
label=helper.umg_find_widget(bp,'TXT_button_Execute')
if label: label.set_text('确认行动  Enter')
label=helper.umg_find_widget(bp,'TXT_ResultConfirmLabel')
if label: label.set_text('领取并返回')
add('TextBlock','PR_Help').set_text('Q / E / R  选择技能     ← / →  切换目标')
position(helper.umg_find_widget(bp,'PR_Help'),-570,-270,550,25,1,1)
unreal.BlueprintEditorLibrary.compile_blueprint(bp)
unreal.EditorAssetLibrary.save_loaded_asset(bp,only_if_is_dirty=False)

bp=unreal.load_asset('/Game/UI/WBP_ExplorationHUD')
info=json.loads(helper.umg_get_widget_info(bp)); root=info['root_widget']
position(add('SizeBox','PR_MinimapHost'),26,30,210,210)
tracker=add('TextBlock','PR_QuestTracker'); tracker.set_auto_wrap_text(True)
position(tracker,28,260,390,125)
party=add('TextBlock','PR_PartyText'); position(party,-260,270,235,300,1,0)
help_text=add('TextBlock','PR_ControlHelp'); help_text.set_text('F  交互\nB  背包   T  队伍\nTab  菜单   M  地图')
position(help_text,-340,-145,310,110,1,1)
for name in ['Test','WBP_AttributeDebug']:
    helper.umg_find_widget(bp,name).set_visibility(unreal.SlateVisibility.COLLAPSED)
position(helper.umg_find_widget(bp,'TXT_InteractionPrompt'),-280,-215,560,55,.5,1)
for entry in info['widgets']:
    if 'Reward' in entry['type'] and entry.get('parent')==root:
        position(helper.umg_find_widget(bp,entry['name']),-260,150,520,150,.5,0)
unreal.BlueprintEditorLibrary.compile_blueprint(bp)
unreal.EditorAssetLibrary.save_loaded_asset(bp,only_if_is_dirty=False)

def text_label(name,value,parent):
    w=add('TextBlock',name,parent);w.set_text(value);w.set_auto_wrap_text(True)
    font=w.get_editor_property('font');font.set_editor_property('size',20);w.set_font(font)
    w.set_color_and_opacity(unreal.SlateColor(specified_color=unreal.LinearColor(.91,.94,1,1)))
    return w
def button(name,label,parent):
    b=add('Button',name,parent)
    text_label('PR_Label_'+name,label,name).set_auto_wrap_text(False)
    b.set_background_color(unreal.LinearColor(.05,.11,.17,1))
    slot=b.get_editor_property('slot')
    if hasattr(slot,'set_padding'):slot.set_padding(unreal.Margin(8,8,8,8))
    return b
def fill(name,value=1):
    w=helper.umg_find_widget(bp,name);s=w.get_editor_property('slot')
    if hasattr(s,'set_size'):s.set_size(unreal.SlateChildSize(value=value,size_rule=unreal.SlateSizeRule.FILL))
def padding(name,size):
    w=helper.umg_find_widget(bp,name);s=w.get_editor_property('slot')
    if hasattr(s,'set_padding'):s.set_padding(unreal.Margin(size,size,size,size))

# Shared root reserves compact navigation; the mounted module takes remaining space.
bp=unreal.load_asset('/Game/UI/P17/Frontend/WBP_FrontendModuleRoot_P17')
root=json.loads(helper.umg_get_widget_info(bp))['root_widget']
helper.umg_reparent_widget(bp,'ModuleContentHost','RootBox')
host=helper.umg_find_widget(bp,'ModuleContentHost')
fill('ModuleContentHost')
for name in ['TXT_Description','SizeBox_Fill']:helper.umg_find_widget(bp,name).set_visibility(unreal.SlateVisibility.COLLAPSED)
border=helper.umg_find_widget(bp,'BG_Border');border.set_visibility(unreal.SlateVisibility.VISIBLE);border.set_padding(unreal.Margin(0,0,0,0))
slot=border.get_editor_property('slot');slot.set_anchors(unreal.Anchors(minimum=unreal.Vector2D(0,0),maximum=unreal.Vector2D(1,1)));slot.set_offsets(unreal.Margin(0,0,0,0))
padding('TXT_ModuleTitle',16);padding('Row_Bottom',12)
helper.umg_find_widget(bp,'TXT_Back').set_text('返回  Esc');helper.umg_find_widget(bp,'TXT_Close').set_text('关闭  X')
unreal.BlueprintEditorLibrary.compile_blueprint(bp);unreal.EditorAssetLibrary.save_loaded_asset(bp,only_if_is_dirty=False)

bp=unreal.load_asset('/Game/UI/P17/Inventory/WBP_Inventory_P17')
root=json.loads(helper.umg_get_widget_info(bp))['root_widget']
helper.umg_find_widget(bp,'BG_Border').set_padding(unreal.Margin(48,32,48,32))
helper.umg_find_widget(bp,'NavBox').set_visibility(unreal.SlateVisibility.VISIBLE)
padding('NavBox',8)
fill('Spacer_Nav')
padding('TXT_Title',12)
fill('ContentRow');fill('LeftPanel',1);fill('RightPanel',1.2);fill('ListScrollBox');padding('RightPanel',18)
button('BTN_CatAll','全部','TabRow')
fill('BTN_CatAll')
button('BTN_NextCharacter','切换装备角色','NavBox')
helper.umg_find_widget(bp,'SortBox').set_visibility(unreal.SlateVisibility.COLLAPSED)
button('BTN_CycleSort','切换排序','LeftVBox')
text_label('TXT_SortLabel','默认排序','LeftVBox')
for name,value,parent in [('TXT_ActionPreview','','RightVBox'),('TXT_ActionResult','','RightVBox'),('TXT_EmptyState','暂无此类物品','LeftVBox')]:
    text_label(name,value,parent);padding(name,10)
add('HorizontalBox','PR_ConfirmRow','RightVBox')
button('BTN_ConfirmAction','确认','PR_ConfirmRow');button('BTN_CancelAction','取消','PR_ConfirmRow')
button('BTN_NextEnhancement','选择目标等级','RightVBox')
for name in ['TXT_DetailDescription','TXT_DetailName']:
    helper.umg_find_widget(bp,name).set_auto_wrap_text(True);padding(name,8)
helper.umg_find_widget(bp,'SearchBox').set_hint_text('搜索物品名称')
for name,value in [('TXT_Title','背包'),('TXT_Back','返回'),('TXT_Close','关闭'),('TXT_ActionEquip','装备预览'),('TXT_ActionEnhance','强化预览')]:
    helper.umg_find_widget(bp,name).set_text(value)
unreal.BlueprintEditorLibrary.compile_blueprint(bp);unreal.EditorAssetLibrary.save_loaded_asset(bp,only_if_is_dirty=False)

bp=unreal.load_asset('/Game/UI/P17/Character/WBP_CharacterShell_P17')
root=json.loads(helper.umg_get_widget_info(bp))['root_widget']
button('BTN_ChangeWeapon','更换武器','DetailPanel')
for name in ['EidolonTabButton','OutfitTabButton']:helper.umg_find_widget(bp,name).set_visibility(unreal.SlateVisibility.COLLAPSED)
unreal.BlueprintEditorLibrary.compile_blueprint(bp);unreal.EditorAssetLibrary.save_loaded_asset(bp,only_if_is_dirty=False)
bp=unreal.load_asset('/Game/UI/P17/Relic/WBP_RelicEquipment_P17')
root=json.loads(helper.umg_get_widget_info(bp))['root_widget']
button('BTN_Unequip','卸下','ActionRow')
helper.umg_find_widget(bp,'TXT_ConfirmEnhance').set_text('确认强化')
unreal.BlueprintEditorLibrary.compile_blueprint(bp);unreal.EditorAssetLibrary.save_loaded_asset(bp,only_if_is_dirty=False)
def full_canvas(name,inset=32):
    w=helper.umg_find_widget(bp,name);s=w.get_editor_property('slot')
    s.set_anchors(unreal.Anchors(minimum=unreal.Vector2D(0,0),maximum=unreal.Vector2D(1,1)))
    s.set_offsets(unreal.Margin(inset,inset,inset,inset));s.set_auto_size(False)
    s.set_alignment(unreal.Vector2D(0,0))

bp=unreal.load_asset('/Game/UI/P17/Character/WBP_CharacterShell_P17')
root=json.loads(helper.umg_get_widget_info(bp))['root_widget']
helper.umg_find_widget(bp,'MainPanel').set_padding(unreal.Margin(32,24,32,24))
fill('ListScrollBox',.3);fill('DetailPanel',1);padding('DetailPanel',24);fill('ContentHost')
helper.umg_find_widget(bp,'BackButton').set_visibility(unreal.SlateVisibility.COLLAPSED)
for name in ['DetailTabButton','WeaponTabButton','TracesTabButton','RelicsTabButton','InformationTabButton']:
    fill(name);padding(name,6)
text_label('TXT_Level','','DetailPanel')
portrait_frame=add('SizeBox','PR_PortraitFrame','DetailPanel');portrait_frame.set_width_override(96);portrait_frame.set_height_override(120)
portrait_frame.get_editor_property('slot').set_horizontal_alignment(unreal.HorizontalAlignment.H_ALIGN_LEFT)
portrait=add('Image','IMG_CharacterPortrait','PR_PortraitFrame')
portrait.set_visibility(unreal.SlateVisibility.COLLAPSED)
frame=helper.umg_find_widget(bp,'RelicHostSizeBox');frame.clear_width_override();frame.clear_height_override()
fill('RelicHostSizeBox')
padding('TXT_Level',8)
portrait_frame.set_visibility(unreal.SlateVisibility.COLLAPSED)
for name in ['SelectedCharacterText','TXT_Level','TabBar','StatsBox','ContentHost','UnavailableText','BTN_ChangeWeapon']:
    helper.umg_reparent_widget(bp,name,'DetailPanel')
cdo=unreal.get_default_object(bp.generated_class())
cdo.set_editor_property('relic_widget_class',unreal.load_asset('/Game/UI/P17/Relic/WBP_RelicEquipment_P17').generated_class())
cdo.set_editor_property('presentation_catalog',unreal.load_asset('/Game/Data/VerticalSlice/Items/DA_InventoryCatalog_VerticalSlice'))
cdo.set_editor_property('equipment_mapping_catalog',unreal.load_asset('/Game/Data/VerticalSlice/Items/DA_ItemEquipmentMappingCatalog_VerticalSlice'))
unreal.BlueprintEditorLibrary.compile_blueprint(bp);unreal.EditorAssetLibrary.save_loaded_asset(bp,only_if_is_dirty=False)

bp=unreal.load_asset('/Game/UI/P17/Frontend/WBP_HSRMapPanel_P17');root='CanvasPanel_48'
map_border=helper.umg_find_widget(bp,'Border_38')
map_border.set_horizontal_alignment(unreal.HorizontalAlignment.H_ALIGN_FILL);map_border.set_vertical_alignment(unreal.VerticalAlignment.V_ALIGN_FILL);map_border.set_padding(unreal.Margin(0))
position(add('SizeBox','PR_MapHost'),-100,-335,660,660,.5,.5)
position(helper.umg_find_widget(bp,'Text_CurrentMap'),40,40,480,60)
position(helper.umg_find_widget(bp,'Text_Result'),40,240,430,150)
helper.umg_find_widget(bp,'Text_Result').set_text('')
position(helper.umg_find_widget(bp,'BTN_TeleportAB'),40,140,360,65)
position(helper.umg_find_widget(bp,'BTN_TeleportBA'),40,140,360,65)
helper.umg_find_widget(bp,'Text_Revision').set_visibility(unreal.SlateVisibility.COLLAPSED)
unreal.BlueprintEditorLibrary.compile_blueprint(bp);unreal.EditorAssetLibrary.save_loaded_asset(bp,only_if_is_dirty=False)

bp=unreal.load_asset('/Game/UI/P17/Frontend/WBP_HSRQuestPanel_P17');root='CanvasPanel_218'
helper.umg_reparent_widget(bp,'ContentOverlay',root);full_canvas('ContentOverlay',40)
helper.umg_find_widget(bp,'SafeZone_105').set_visibility(unreal.SlateVisibility.COLLAPSED)
unreal.BlueprintEditorLibrary.compile_blueprint(bp);unreal.EditorAssetLibrary.save_loaded_asset(bp,only_if_is_dirty=False)

bp=unreal.load_asset('/Game/UI/P17/Frontend/WBP_HSRPartyPanel_P17');root='RootCanvas'
full_canvas('StateOverlay',32);helper.umg_find_widget(bp,'Card_Ready').set_padding(unreal.Margin(32,24,32,24))
helper.umg_find_widget(bp,'Button_Swap').set_visibility(unreal.SlateVisibility.COLLAPSED)
fill('VBox_Left');fill('VBox_Right');padding('VBox_Left',16);padding('VBox_Right',16)
for i in range(4):
    frame=helper.umg_find_widget(bp,'SizeBox_Slot'+str(i));frame.clear_width_override();frame.set_height_override(50)
    padding('Button_ClearSlot'+str(i),8)
padding('HBox_Actions',16);padding('Text_Result',12)
helper.umg_find_widget(bp,'PartySlotList').set_visibility(unreal.SlateVisibility.COLLAPSED)
add('HorizontalBox','PR_PartyChoices','ReadyBox')
for i in range(4):
    box='PR_PartyChoice'+str(i);add('VerticalBox',box,'PR_PartyChoices');fill(box);padding(box,12)
    text_label('PR_SlotCaption'+str(i),'队伍位置 '+str(i+1),box)
    helper.umg_reparent_widget(bp,'SizeBox_Slot'+str(i),box)
    helper.umg_reparent_widget(bp,'Button_ClearSlot'+str(i),box)
helper.umg_find_widget(bp,'HBox_Columns').set_visibility(unreal.SlateVisibility.COLLAPSED)
for name in ['HBox_Actions','Text_Result']:helper.umg_reparent_widget(bp,name,'ReadyBox')
unreal.BlueprintEditorLibrary.compile_blueprint(bp);unreal.EditorAssetLibrary.save_loaded_asset(bp,only_if_is_dirty=False)

bp=unreal.load_asset('/Game/UI/P17/Frontend/WBP_HSRSavePanel_P17');root='CanvasPanel_48'
position(helper.umg_find_widget(bp,'Card_Slots'),48,32,680,530)
helper.umg_find_widget(bp,'Card_Slots').set_padding(unreal.Margin(24,24,24,24))
for name in ['BTN_SaveSlot1','BTN_LoadSlot1','BTN_SaveSlot2','btn_LoadSlot2','TXT_Slot1Summary','TXT_Slot2Summary']:padding(name,10)
position(helper.umg_find_widget(bp,'TXT_SaveResult'),770,40,430,130)
position(helper.umg_find_widget(bp,'TXT_SaveRecovery'),770,180,430,130)
position(helper.umg_find_widget(bp,'TXT_OverwriteMessage'),770,340,430,100)
position(helper.umg_find_widget(bp,'Border_OverwriteConfirm'),770,450,430,70)
helper.umg_find_widget(bp,'TXT_SaveGeneration').set_visibility(unreal.SlateVisibility.COLLAPSED)
unreal.BlueprintEditorLibrary.compile_blueprint(bp);unreal.EditorAssetLibrary.save_loaded_asset(bp,only_if_is_dirty=False)

bp=unreal.load_asset('/Game/UI/P17/Frontend/WBP_HSRChallengeDirectory_P17');root='Root_Canvas'
full_canvas('Panel_Border',32);helper.umg_find_widget(bp,'Panel_Border').set_padding(unreal.Margin(24,24,24,24))
fill('Card_List');padding('BTN_Enter',12)
unreal.BlueprintEditorLibrary.compile_blueprint(bp);unreal.EditorAssetLibrary.save_loaded_asset(bp,only_if_is_dirty=False)
bp=unreal.load_asset('/Game/UI/P17/Frontend/WBP_HSRChallengeEntry_P17')
helper.umg_find_widget(bp,'Root_SizeBox').clear_width_override()
helper.umg_find_widget(bp,'Root_SizeBox').set_height_override(100)
for name,width in [('SizeBox_347',220),('SizeBox_450',380),('SizeBox_569',160),('SizeBox_676',200),('SizeBox_850',170)]:
    helper.umg_find_widget(bp,name).set_width_override(width)
    helper.umg_find_widget(bp,name).get_editor_property('slot').set_size(unreal.SlateChildSize(value=1,size_rule=unreal.SlateSizeRule.AUTOMATIC))
helper.umg_find_widget(bp,'Root_SizeBox').clear_width_override()
for name in ['TXT_EncounterId','TXT_EnemyInfo','TXT_Diagnostic']:helper.umg_find_widget(bp,name).set_auto_wrap_text(True)
unreal.BlueprintEditorLibrary.compile_blueprint(bp);unreal.EditorAssetLibrary.save_loaded_asset(bp,only_if_is_dirty=False)
bp=unreal.load_asset('/Game/UI/P17/Relic/WBP_RelicEquipment_P17')
helper.umg_find_widget(bp,'MainPanel').set_padding(unreal.Margin(12,12,12,12))
panel=helper.umg_find_widget(bp,'MainPanel')
panel.set_horizontal_alignment(unreal.HorizontalAlignment.H_ALIGN_FILL);panel.set_vertical_alignment(unreal.VerticalAlignment.V_ALIGN_FILL)
panel_slot=panel.get_editor_property('slot');panel_slot.set_horizontal_alignment(unreal.HorizontalAlignment.H_ALIGN_FILL);panel_slot.set_vertical_alignment(unreal.VerticalAlignment.V_ALIGN_FILL)
for name in ['ListRowSizeBox','EnhanceBoxSizeBox','EnhancementSizeBox']:
    frame=helper.umg_find_widget(bp,name);frame.clear_width_override();frame.clear_height_override()
fill('ContentScrollBox');fill('ListRowSizeBox',1.2);fill('RightColumn',1);fill('SlotBox');fill('CandidateBox')
padding('CandidateBox',12);padding('RightColumn',12)
helper.umg_find_widget(bp,'ListRowSizeBox').set_height_override(550)
helper.umg_find_widget(bp,'EnhancementSizeBox').set_height_override(180)
unreal.BlueprintEditorLibrary.compile_blueprint(bp);unreal.EditorAssetLibrary.save_loaded_asset(bp,only_if_is_dirty=False)
bp=unreal.load_asset('/Game/UI/P17/Dialogue/WBP_DialogueOverlay_P17')
bg=helper.umg_find_widget(bp,'BG_Border');main=helper.umg_find_widget(bp,'MainVBox')
card=helper.umg_find_widget(bp,'PR_DialogueCard')
if not card:
    card=unreal.new_object(unreal.Border,outer=bg.get_outer(),name='PR_DialogueCard')
    bg.set_content(card);card.set_content(main)
    helper.umg_set_widget_is_variable(bp,'PR_DialogueCard',True)
bg.set_brush_color(unreal.LinearColor(.01,.02,.035,.2));bg.set_vertical_alignment(unreal.VerticalAlignment.V_ALIGN_BOTTOM)
bg.set_padding(unreal.Margin(96,0,96,48));card.set_padding(unreal.Margin(32,24,32,24));card.set_brush_color(unreal.LinearColor(.018,.035,.06,.96))
card.get_editor_property('slot').set_vertical_alignment(unreal.VerticalAlignment.V_ALIGN_BOTTOM)
card.get_editor_property('slot').set_horizontal_alignment(unreal.HorizontalAlignment.H_ALIGN_FILL)
card.get_editor_property('slot').set_padding(unreal.Margin(96,0,96,48))
for name in ['TXT_Body','ChoicesBox','CloseButton']:padding(name,10)
helper.umg_find_widget(bp,'TXT_Body').set_auto_wrap_text(True)
unreal.BlueprintEditorLibrary.compile_blueprint(bp);unreal.EditorAssetLibrary.save_loaded_asset(bp,only_if_is_dirty=False)

bp=unreal.load_asset('/Game/UI/P17/Frontend/WBP_FrontendShell_P17');root='RootCanvas'
full_canvas('BG_Border',0);helper.umg_find_widget(bp,'BG_Border').set_padding(unreal.Margin(160,100,160,100))
for name in ['BTN_Character','BTN_Inventory','BTN_Party','BTN_Quest','BTN_Map','BTN_Challenge','BTN_Save']:
    fill(name);padding(name,12)
helper.umg_find_widget(bp,'TXT_Title').set_text('星轨 · 旅途菜单')
for name,label in [('TXT_Character','角色'),('TXT_Inventory','背包'),('TXT_Party','队伍'),('TXT_Quest','任务'),('TXT_Map','地图'),('TXT_Challenge','挑战'),('TXT_Save','存档'),('TXT_Back','继续旅程'),('TXT_Close','关闭菜单')]:helper.umg_find_widget(bp,name).set_text(label)
unreal.BlueprintEditorLibrary.compile_blueprint(bp);unreal.EditorAssetLibrary.save_loaded_asset(bp,only_if_is_dirty=False)
print(json.dumps({'success':True,'styled_widgets':len(changed),'inventory_confirm':True,'relic_unequip':True}))
