import unreal,json
from pathlib import Path
base=Path(unreal.Paths.project_content_dir(),'Data/VerticalSlice/Skills')
out=[]
for path in base.glob('*.uasset'):
 skill=unreal.load_asset('/Game/Data/VerticalSlice/Skills/'+path.stem)
 if not isinstance(skill,unreal.HSRSkillDefinition):continue
 category=str(skill.get_editor_property('category'))
 target=str(skill.get_editor_property('target_type'))
 heal='HEAL' in category
 target_text='自身' if 'SELF' in target else '一名存活队友' if 'ALLY' in target else '一名敌人'
 multiplier=skill.get_editor_property('ability_multiplier')
 delta=skill.get_editor_property('skill_point_delta')
 if delta==0:delta=1 if 'BASIC_ATTACK' in category else -1 if 'SKILL' in category else 0
 text=('为'+target_text+'恢复生命。' if heal else '对'+target_text+'造成伤害，攻击倍率 '+f'{multiplier*100:g}'+'%。')
 if not heal:text+='命中对应弱点时削减 '+f'{skill.get_editor_property("toughness_damage"):g}'+' 点韧性。'
 if delta<0:text+='消耗 '+str(-delta)+' 点战技点。'
 elif delta>0:text+='恢复 '+str(delta)+' 点战技点。'
 cost=skill.get_editor_property('display_energy_cost')
 if cost>0:text+='消耗 '+f'{cost:g}'+' 点能量。'
 skill.modify();skill.set_editor_property('description',text);unreal.EditorAssetLibrary.save_loaded_asset(skill,only_if_is_dirty=False)
 out.append({'skill':str(skill.get_editor_property('skill_id')),'description':text})
print(json.dumps({'success':True,'skills':out},ensure_ascii=False))
