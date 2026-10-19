"""Upgrade an existing combat skill's targeting, preserving its stable identity and effects."""
import json
from pathlib import Path
import unreal

path = '/Game/Data/VerticalSlice/Skills/DA_Skill_Remiel_SkyJudgment'
skill = unreal.load_asset(path)
if not skill:
    raise RuntimeError('Missing existing Remiel ultimate')
before = {'id': str(skill.get_editor_property('skill_id')), 'target': str(skill.get_editor_property('target_type'))}
skill.modify()
skill.set_editor_property('category', unreal.HSRSkillCategory.ULTIMATE)
skill.set_editor_property('ability_class', unreal.load_class(None, '/Script/HSR.HSRUltimateAbility'))
skill.set_editor_property('target_type', unreal.HSRTargetType.ALL_ENEMIES)
skill.set_editor_property('description', '攻击全体存活敌人。消耗一次自身能量，每个确认目标结算一次伤害及削韧；等待期间目标集合变化时强制取消，不自动换目标。不占用普通回合。')
assert unreal.EditorAssetLibrary.save_loaded_asset(skill, only_if_is_dirty=False)
assert str(skill.get_editor_property('skill_id')) == before['id']
report = {'success': True, 'asset': path, 'before': before, 'after': str(skill.get_editor_property('target_type'))}
Path(unreal.Paths.project_saved_dir(), 'Ultimate/author_ultimate_aoe.json').write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
unreal.log('UltimateAOEAuthoring ' + json.dumps(report, ensure_ascii=False))
