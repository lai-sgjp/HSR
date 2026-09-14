import unreal,json
from pathlib import Path
out={}
groups={
 '/Game/Data/VerticalSlice/Characters':['character_id','character_class','character_mesh','skill_definitions','cumulative_experience_curve'],
 '/Game/Data/VerticalSlice/Skills':['skill_id','category','target_type','ability_class','effect_gameplay_effect_class','cost_gameplay_effect_class','energy_refund_gameplay_effect_class','damage_rule','element_tag'],
 '/Game/Data/Skills':['skill_id','category','target_type','ability_class','effect_gameplay_effect_class','cost_gameplay_effect_class','energy_refund_gameplay_effect_class','damage_rule','element_tag'],
 '/Game/Data/VerticalSlice/Maps':['map_id','world','default_arrival_id'],
 '/Game/Data/VerticalSlice/Quests':['quest_id','objectives','completion_reward_definition'],
 '/Game/Data/VerticalSlice/Encounters':['encounter_id','enemy_definition_id','battle_map','prerequisite_encounter_ids','reward_item_definitions','reward_drop_table','victory_reward_definition'],
}
for folder,props in groups.items():
 for path in unreal.EditorAssetLibrary.list_assets(folder):
  a=unreal.load_asset(path); d={}
  for p in props:
   try:d[p]=str(a.get_editor_property(p))
   except Exception as e:d[p]=str(e)
  out[path]=d
for path in ['/Game/Assets/mmd/Character/Velina/BP_Velina','/Game/Assets/mmd/Character/Sparxie/BP_Sparxie']:
 b=unreal.load_asset(path)
 if b:
  c=unreal.get_default_object(b.generated_class());out[path]={'parent':str(c.get_class()),'mesh':str(c.get_editor_property('mesh').get_editor_property('skeletal_mesh_asset'))}
Path(unreal.Paths.project_dir(),'Saved/Presentation/runtime_assets.json').write_text(json.dumps(out,ensure_ascii=False,indent=2),encoding='utf-8')
print(json.dumps({'success':True,'count':len(out)}))
