import unreal,json
from pathlib import Path
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
w=next(w for w in unreal.WidgetLibrary.get_all_widgets_of_class(world,unreal.HSRPreBattleCandidateWidget,False) if w.is_in_viewport())
report={'initial':str(w.get_candidate_snapshot()),'capture':unreal.MCPythonHelper.capture_play_viewport('PREBATTLE')}
w.get_editor_property('Button_ClearSlot3').on_clicked.broadcast()
report['cleared']=str(w.get_candidate_snapshot())
w.get_editor_property('Button_ReplaceSlot3').on_clicked.broadcast()
choices=w.get_editor_property('SelectionPanel')
report['choices']=choices.get_children_count()
report['buff_visibility']=str(w.get_editor_property('BuffSelection').get_visibility())
# Cancel through the real Blueprint click path, including RemoveFromParent.
w.get_editor_property('Button_Cancel').on_clicked.broadcast()
report['removed']=not w.is_in_viewport()
Path(unreal.Paths.project_dir(),'Saved/Presentation/prebattle_pie_validation.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
print(json.dumps({'success':True,'choices':report['choices'],'removed':report['removed']}))
