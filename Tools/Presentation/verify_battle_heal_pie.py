"""Actual battle UI selections and execution; no attribute or resource mutation."""
import unreal,json
from pathlib import Path
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
w=next(x for x in unreal.WidgetLibrary.get_all_widgets_of_class(world,unreal.HSRBattleCommandWidget,False) if x.is_in_viewport())
rows=[];heals=[];unavailable=[]
for i in range(90):
 state=w.get_current_view_state()
 if state.result_view_state.visible:break
 for skill in state.skills:
  if skill.energy_cost>=100 and not skill.available:unavailable.append(str(skill))
 candidates=[s for s in state.skills if s.available and s.candidate_target_ids]
 healing=[s for s in candidates if 'ALLY' in str(s.target_type)]
 healing.sort(key=lambda s:-s.energy_cost)
 chosen=None;target=None
 for skill in healing:
  wounded=[p for p in state.participants if p.participant_id in skill.candidate_target_ids and p.health<p.max_health and p.health>0]
  if wounded:chosen=skill;target=min(wounded,key=lambda p:p.health/p.max_health).participant_id;break
 if chosen is None:
  damage=[s for s in candidates if 'ENEMY' in str(s.target_type)]
  chosen=(damage or candidates)[0] if candidates else None
  if chosen:target=chosen.candidate_target_ids[0]
 if chosen is None:raise RuntimeError('No available battle command')
 before=next(p for p in state.participants if p.participant_id==state.current_actor_id)
 w.select_skill_by_id(chosen.skill_id);w.select_target(target)
 result=w.submit_selected_skill()
 row={'actor':str(state.current_actor_id),'skill':str(chosen.skill_id),'target':str(target),'result':str(result)}
 rows.append(row)
 if result.has_heal_result:
  after=next(p for p in w.get_current_view_state().participants if p.participant_id==state.current_actor_id)
  heals.append({**row,'energy_before':before.energy,'energy_after':after.energy,'heal_amount':result.heal_amount,'authored_cost':chosen.energy_cost})
report={'completed':w.get_current_view_state().result_view_state.visible,'actions':len(rows),'heals':heals,'unavailable_skills':unavailable,'steps':rows}
Path(unreal.Paths.project_dir(),'Saved/Presentation/battle_heal_pie_validation.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
print(json.dumps({'completed':report['completed'],'heals':heals,'unavailable_count':len(unavailable)},ensure_ascii=False))
