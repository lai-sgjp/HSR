"""Drive real UI commands over time; never submit multiple turns in one frame."""
import json
from datetime import datetime
from pathlib import Path
import unreal

battle_elapsed = [0.]
battle_handle = [None]
battle_report = {'actions': [], 'transitions': [], 'samples': [], 'completed': False}
battle_last_actor = ['']
battle_last_sample = [-1.]
def battle_tick(dt):
    battle_elapsed[0] += dt
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
    widgets = unreal.WidgetLibrary.get_all_widgets_of_class(world, unreal.HSRBattleCommandWidget, False) if world else []
    widget = next((w for w in widgets if w.is_in_viewport()), None)
    if not widget:
        if battle_elapsed[0] > 10: finish('missing battle widget')
        return
    state = widget.get_current_view_state()
    actor = str(state.current_actor_id)
    if battle_last_actor[0] != actor:
        battle_report['transitions'].append({'time': battle_elapsed[0], 'actor': actor})
        battle_last_actor[0] = actor
    if battle_elapsed[0] - battle_last_sample[0] >= .2:
        battle_last_sample[0] = battle_elapsed[0]
        row = {'time': battle_elapsed[0], 'actor': actor, 'playing': state.action_playing,
            'can_submit': state.can_submit, 'health': {str(p.participant_id):p.health for p in state.participants}, 'montages': []}
        for pawn in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Character):
            instance = pawn.get_editor_property('mesh').get_anim_instance()
            if instance:
                montage = instance.get_current_active_montage()
                if montage: row['montages'].append({'actor':pawn.get_name(),'position':instance.montage_get_position(montage)})
        battle_report['samples'].append(row)
    if state.result_view_state.visible:
        battle_report['completed'] = True
        unreal.MCPythonHelper.capture_play_viewport('paced_battle_result')
        finish('completed')
        return
    if state.can_submit:
        skills = [s for s in state.skills if s.available and s.candidate_target_ids and s.target_type == unreal.HSRTargetType.SINGLE_ENEMY]
        if skills:
            skill = skills[0]
            widget.select_skill_by_id(skill.skill_id)
            widget.select_target(skill.candidate_target_ids[0])
            result = widget.submit_selected_skill()
            battle_report['actions'].append({'time':battle_elapsed[0],'actor':actor,'status':str(result.status),'failure':str(result.failure_reason)})
    if 4 < battle_elapsed[0] < 4.2: unreal.MCPythonHelper.capture_play_viewport('paced_battle_action')
    if battle_elapsed[0] > 180: finish('timeout')
def finish(reason):
    unreal.unregister_slate_post_tick_callback(battle_handle[0])
    battle_report['reason'] = reason
    Path(unreal.Paths.project_dir(),'Saved/Presentation/paced_battle_validation.json').write_text(json.dumps(battle_report,ensure_ascii=False,indent=2),encoding='utf-8')
    Path(unreal.Paths.project_dir(),'Saved/Presentation/paced_battle_'+datetime.now().strftime('%Y%m%d_%H%M%S')+'.json').write_text(json.dumps(battle_report,ensure_ascii=False,indent=2),encoding='utf-8')
battle_handle[0] = unreal.register_slate_post_tick_callback(battle_tick)
print(json.dumps({'success':True,'timeout_seconds':180}))
