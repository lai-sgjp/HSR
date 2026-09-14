import unreal,json
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
rows=[]
for cls,names in [(unreal.HSRCharacterShellWidget,['DetailTabButton','WeaponTabButton','TXT_Weapon']),(unreal.HSRPartyWidget,['PartySlotList','ComboBoxString_Slot0','SizeBox_Slot0'])]:
 for w in unreal.WidgetLibrary.get_all_widgets_of_class(world,cls,False):
  if not w.is_visible():continue
  for name in names:
   x=w.get_editor_property(name);r={'name':name,'enabled':x.get_is_enabled(),'visibility':str(x.get_visibility()),'opacity':x.get_render_opacity()}
   if isinstance(x,unreal.Button):r['color']=str(x.get_editor_property('color_and_opacity'))
   if isinstance(x,unreal.TextBlock):r['color']=str(x.get_editor_property('color_and_opacity'))
   if isinstance(x,unreal.ComboBoxString):r['color']=str(x.get_editor_property('foreground_color'))
   rows.append(r)
print(json.dumps({'success':True,'rows':rows}))
