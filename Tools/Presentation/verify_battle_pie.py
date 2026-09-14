import unreal,json
from pathlib import Path
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
w=next(x for x in unreal.WidgetLibrary.get_all_widgets_of_class(world,unreal.HSRBattleCommandWidget,False) if x.is_in_viewport())
steps=[]
for i in range(80):
 state=w.get_current_view_state()
 if state.result_view_state.visible:break
 candidates=[s for s in state.skills if s.available and s.candidate_target_ids]
 damage=[s for s in candidates if str(s.target_type).find('ENEMY')>=0]
 skill=(damage or candidates)[0] if candidates else None
 if not skill:
  steps.append({'failure':'no available skill','state':str(state)});break
 w.select_skill_by_id(skill.skill_id)
 w.select_target(skill.candidate_target_ids[0])
 resolution=w.submit_selected_skill()
 steps.append({'actor':str(state.current_actor_id),'skill':str(skill.skill_id),'resolution':str(resolution)})
state=w.get_current_view_state()
result={'world':world.get_path_name(),'steps':steps,'final':str(state),'completed':state.result_view_state.visible}
Path(unreal.Paths.project_dir(),'Saved/Presentation/battle_pie_validation.json').write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding='utf-8')
unreal.SystemLibrary.execute_console_command(world,'Shot showui')
print(json.dumps({'success':True,'steps':len(steps),'completed':result['completed'],'last':steps[-1] if steps else ''}))
