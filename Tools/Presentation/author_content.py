"""Wire the formal vertical slice to its existing authorities; never fabricate save state."""
import unreal,json,uuid
from pathlib import Path
lib=unreal.EditorAssetLibrary
assets=unreal.AssetToolsHelpers.get_asset_tools()
actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
base='/Game/Data/VerticalSlice/'
def load(p):
 a=unreal.load_asset(p)
 if not a: raise RuntimeError('Missing authored asset '+p)
 return a
def data(folder,name,typ):
 path=base+folder+'/'+name
 if lib.does_asset_exist(path):return load(path)
 f=unreal.DataAssetFactory();f.set_editor_property('data_asset_class',unreal.load_class(None,'/Script/HSR.'+typ))
 return assets.create_asset(name,base+folder,None,f)
def save(a):lib.save_loaded_asset(a)
def props(a,**values):
 for k,v in values.items():a.set_editor_property(k,v)
def all_assets(folder):return [load(p) for p in lib.list_assets(base+folder) if not p.endswith('/')]
def clone(src,dst):
 if not lib.does_asset_exist(dst):lib.duplicate_asset(src,dst)
 return load(dst)
weapon=data('Equipment','DA_Weapon_EchoConductor','HSREquipmentDefinition')
props(weapon,definition_id='Demo.Weapon.EchoConductor',enhancement_cap=4,default_modifiers=[unreal.HSREquipmentModifier(stat=unreal.HSREquipmentStat.ATTACK,value=12)])
save(weapon)
weapon_item=data('Items/Weapons','DA_Item_EchoConductor','HSRItemDefinition')
props(weapon_item,item_id='Demo.Item.EchoConductor',display_name='回响导体',storage_kind=unreal.HSRItemStorageKind.UNIQUE,max_stack=1);save(weapon_item)
items=all_assets('Items/Relics')+all_assets('Items/Materials')+[weapon_item]
relics=all_assets('Relics/Pieces')
mapping=load(base+'Items/DA_ItemEquipmentMappingCatalog_VerticalSlice')
entries=[e for e in mapping.get_editor_property('mappings') if str(e.get_editor_property('item_id'))!='Demo.Item.EchoConductor']
entries.append(unreal.HSRItemEquipmentMappingEntry(item_id='Demo.Item.EchoConductor',equipment_definition_id='Demo.Weapon.EchoConductor',kind=unreal.HSREquipmentKind.EQUIPMENT,slot=0))
props(mapping,mappings=entries);save(mapping)
presentation=data('Items','DA_InventoryCatalog_VerticalSlice','HSRInventoryCatalog')
catalog_entries=[]
for i,item in enumerate(items):
 category=unreal.HSRInventoryCategory.WEAPON if item==weapon_item else unreal.HSRInventoryCategory.RELIC if item in items[:6] else unreal.HSRInventoryCategory.MATERIAL
 description='可装备到武器槽。提高攻击，强化前可预览材料消耗。' if category==unreal.HSRInventoryCategory.WEAPON else '可装备到对应遗器部位。同套装遗器组合可激活套装效果。' if category==unreal.HSRInventoryCategory.RELIC else '凝聚界域回响的成长材料，可用于装备强化。'
 catalog_entries.append(unreal.HSRInventoryCatalogEntry(item_id=item.get_editor_property('item_id'),display_name=item.get_editor_property('display_name'),category=category,description=description,rarity=4 if category!=unreal.HSRInventoryCategory.MATERIAL else 3,sort_order=i))
 item_id=str(item.get_editor_property('item_id'))
 icon_name=next((key for key in ['Head','Hands','Body','Feet','PlanarSphere','LinkRope'] if item_id.endswith(key)),'Weapon' if category==unreal.HSRInventoryCategory.WEAPON else 'Material')
 icon_path='/Game/Presentation/UI/T_Item_'+icon_name
 if lib.does_asset_exist(icon_path):catalog_entries[-1].set_editor_property('icon',load(icon_path))
props(presentation,entries=catalog_entries);save(presentation)
material_id=items[6].get_editor_property('item_id')
enhancement=data('Equipment','DA_EnhancementCatalog_VerticalSlice','HSREquipmentEnhancementCatalog')
rules=[]
for definition in relics+[weapon]:
 modifiers=list(definition.get_editor_property('default_modifiers'))
 if not modifiers:
  modifiers=[unreal.HSREquipmentModifier(stat=unreal.HSREquipmentStat.MAX_HEALTH if 'Head' in definition.get_name() else unreal.HSREquipmentStat.ATTACK,value=40 if 'Head' in definition.get_name() else 8)]
  props(definition,default_modifiers=modifiers)
 props(definition,enhancement_cap=max(4,definition.get_editor_property('enhancement_cap')));save(definition)
 for level in range(1,5):
  target=[unreal.HSREquipmentModifier(stat=m.get_editor_property('stat'),value=m.get_editor_property('value')*(1+.25*level)) for m in modifiers]
  rules.append(unreal.HSREquipmentEnhancementRule(definition_id=definition.get_editor_property('definition_id'),kind=unreal.HSREquipmentKind.EQUIPMENT if definition==weapon else unreal.HSREquipmentKind.RELIC,target_level=level,material_item_id=material_id,material_cost=level*2,target_modifiers=target))
props(enhancement,rules=rules);save(enhancement)
for path in ['/Game/UI/P17/Inventory/WBP_Inventory_P17','/Game/UI/P17/Relic/WBP_RelicEquipment_P17']:
 bp=load(path);cdo=unreal.get_default_object(bp.generated_class())
 props(cdo,mapping_catalog=mapping,enhancement_catalog=enhancement)
 props(cdo,**({'catalog':presentation} if 'Inventory' in path else {'presentation_catalog':presentation}))
 unreal.BlueprintEditorLibrary.compile_blueprint(bp);save(bp)
drop=load(base+'Drops/DA_Drop_VerticalSlice')
rewards=all_assets('Rewards')
# Add usable equipment to a fixed, persisted one-time chest reward.
chest_reward=load(base+'Rewards/DA_Reward_WangXiaYiTong')
fixed=[e for e in chest_reward.get_editor_property('fixed_items') if str(e.get_editor_property('item_id')) not in ['Demo.Item.EchoConductor',str(material_id)]]
fixed += [unreal.HSRRewardItemEntry(item_id='Demo.Item.EchoConductor',quantity=1),unreal.HSRRewardItemEntry(item_id=material_id,quantity=12)]
props(chest_reward,fixed_items=fixed);save(chest_reward)
characters=all_assets('Characters')
characters=[c for c in characters if c.get_class().get_name()=='HSRCharacterDefinition']
catalog=data('Characters','DA_CharacterCatalog_VerticalSlice','HSRCharacterCatalog')
props(catalog,characters=[],character_assets=characters);save(catalog)

# Existing meshes are reused, never remodelled. Animation Blueprints must match their skeleton.
mesh_paths={
 'Verina':('/Game/Assets/mmd/Character/Velina/Meshes/SK_维琳娜6','/Game/Assets/mmd/Character/Velina/Animations/ABP_Velina_Unarmed'),
 'Huohua':('/Game/Assets/mmd/Character/Sparxie/Meshes/SK_星穹铁道—火花2','/Game/Assets/mmd/Character/Sparxie/Animations/ABP_Sparxie'),
 'EvernightMoon':('/Game/Assets/mmd/Character/Evernight/Meshes/SK_星穹铁道—长夜月2',None),
 'Remiel':('/Game/Assets/mmd/Character/Ramiel/Meshes/SK_蕾米埃尔·黑',None)}
for c in characters:
 short=str(c.get_editor_property('character_id')).split('.')[-1]
 mesh,_=mesh_paths[short]
 anim='/Game/Presentation/Animation/Locomotion/'+short+'/FINAL_'+short+'_ABP_Unarmed'
 props(c,character_mesh=load(mesh))
 props(c,animation_class=load(anim).generated_class())
 for field,stem in [('attack_animation','MM_Attack_01'),('skill_animation','MM_ChargedAttack'),('hit_animation','MM_HitReact_Front_Med_01')]:
  props(c,**{field:load('/Game/Presentation/Animation/Combat/LIVE_'+short+'_'+stem)})
 props(c,defeat_animation=load('/Game/Presentation/Animation/Combat/FULL_'+short+'_QuaterniusDeath_Anim'))
 props(c,melee_basic_attack=short not in ['Remiel','Verina'])
 if short in ['Remiel','Verina']:
  props(c,attack_animation=load('/Game/Presentation/Animation/Combat/CAST_'+short+'_QuaterniusCast'),
   skill_animation=load('/Game/Presentation/Animation/Combat/CAST_'+short+('_QuaterniusHeal' if short=='Verina' else '_QuaterniusCast')))
 save(c)
heal=load(base+'Skills/DA_Skill_Verina_BreathingGarden')
props(heal,cost_gameplay_effect_class=load('/Game/GameplayEffects/BP_GE_P6_UltimateEnergyCost').generated_class(),
 energy_refund_gameplay_effect_class=load('/Game/GameplayEffects/BP_GE_P7_UltimateEnergyRefund').generated_class(),display_energy_cost=100.,energy_gain=0.)
save(heal)

maps=[]
for n,title in [('ObservationCar','星穹列车 · 观景厅'),('NewEriduSixthStreetMetro','夜幕城区 · 第六街'),('HertaSupportSection','回响竞技场')]:
 a=load(base+'Maps/DA_Map_'+n);props(a,display_name=title,world=load('/Game/Maps/VerticalSlice/Map_'+n));save(a);maps.append(a)
teleports=all_assets('Teleports')
for t in teleports:props(t,initially_unlocked=True);save(t)
mapcat=data('Maps','DA_MapCatalog_VerticalSlice','HSRMapCatalog');props(mapcat,maps=maps,teleports=teleports);save(mapcat)
quest=load(base+'Quests/DA_Quest_DomainEcho')
descriptions=['调查观景厅的界域记录','调查第六街广场的回响','打开广场旁的王下一桶','击败侧巷的巡检机','前往北庭击败来古士']
objectives=list(quest.get_editor_property('objectives'))
for o,d in zip(objectives,descriptions):o.set_editor_property('description',d)
props(quest,display_name='界域回响',objectives=objectives);save(quest)
encounters=all_assets('Encounters')
for e in encounters:
 props(e,battle_map=load('/Game/Maps/VerticalSlice/Map_HertaSupportSection'),reward_item_definitions=items);save(e)
patrol=clone(base+'Encounters/DA_Encounter_SupportSectionInspector',base+'Encounters/DA_Encounter_CourtyardPatrol')
props(patrol,encounter_id='Demo.Encounter.CourtyardPatrol',enemy_definition_id='Demo.Enemy.CourtyardPatrol',victory_reward_definition=load(base+'Rewards/DA_Reward_DomainEcho'));save(patrol)
enemy=clone(base+'Enemies/DA_Enemy_SupportSectionInspector',base+'Enemies/DA_Enemy_CourtyardPatrol')
props(enemy,enemy_definition_id='Demo.Enemy.CourtyardPatrol',display_name='庭院巡逻机',encounter_definition=patrol,formation_count=3);save(enemy)
enemies=[load(base+'Enemies/DA_Enemy_SupportSectionInspector'),enemy,load(base+'Enemies/DA_Boss_Laigushi')]
for short,title,description in [('SupportSectionInspector','侧巷巡检机','清除封锁侧巷的巡检机'),('CourtyardPatrol','庭院巡逻队','迎战三台巡逻机'),('Laigushi','来古士 · 精英遭遇','完成侧巷遭遇后挑战大型敌人')]:
 encounter=load(base+'Encounters/DA_Encounter_'+short);props(encounter,display_name=title,description=description);save(encounter)
bp=load('/Game/UI/P17/Frontend/WBP_HSRChallengeDirectory_P17')
props(unreal.get_default_object(bp.generated_class()),challenge_sources=[unreal.HSRChallengeDirectorySource(definition=load(base+'Encounters/DA_Encounter_'+s)) for s in ['SupportSectionInspector','CourtyardPatrol','Laigushi']])
unreal.BlueprintEditorLibrary.compile_blueprint(bp);save(bp)
for definition,stats in zip(enemies,[(180,18,5,90),(90,12,3,85),(360,25,7,100)]):
 props(definition,use_authored_base_stats=True,base_max_health=stats[0],base_attack=stats[1],base_defense=stats[2],base_speed=stats[3]);save(definition)
for e in enemies:
 boss=str(e.get_editor_property('enemy_definition_id'))=='Demo.Boss.Laigushi'
 props(e,battle_mesh=load('/Game/Presentation/Meshes/SM_Sentry'),battle_mesh_scale=2.2 if boss else 1.0)
 save(e)
ec=load(base+'Enemies/DA_EnemyCatalog');props(ec,enemies=enemies);save(ec)
dialogue=data('Dialogues','DA_Dialogue_Catherine','HSRDialogueDefinition')
choice=unreal.HSRDialogueChoiceDefinition(choice_id='Continue',target_node_id='End',display_text='明白了，我去调查。')
nodes=[unreal.HSRDialogueNodeDefinition(node_id='Start',speaker_text='凯瑟琳',text='观景厅的界域记录出现了异常回响。请先查看大厅里的记录，然后从月台前往第六街。广场、侧巷和北庭都值得调查。',choices=[choice]),
 unreal.HSRDialogueNodeDefinition(node_id='End',speaker_text='凯瑟琳',text='调查结果会记录在任务页中。准备好后，使用青色界域锚点出发吧。')]
props(dialogue,dialogue_id='Demo.Dialogue.Catherine',quest_id='Demo.Quest.DomainEcho',start_node_id='Start',nodes=nodes);save(dialogue)

def spawn(typ,label,pos,yaw=0):
 a=actors.spawn_actor_from_class(unreal.load_class(None,'/Script/HSR.'+typ),unreal.Vector(*(v*100 for v in pos)),unreal.Rotator(yaw=yaw))
 a.set_actor_label('GAME_'+label);return a
def interaction(label,pos,prompt,mesh='TravelAnchor',**p):
 a=spawn('HSRSceneInteraction',label,pos)
 props(a,prompt=prompt,**p)
 a.get_editor_property('collision_component').set_collision_profile_name('OverlapAllDynamic')
 a.get_editor_property('display_mesh').set_static_mesh(load('/Game/Presentation/Meshes/SM_'+mesh))
 return a
def guid(label):
 h=uuid.uuid5(uuid.NAMESPACE_URL,'hsr.verticalslice.'+label).hex
 value=unreal.Guid()
 for prop,i in zip(['a','b','c','d'],range(0,32,8)):value.set_editor_property(prop,int(h[i:i+8],16))
 return value
party=['Demo.Character.Huohua','Demo.Character.Remiel','Demo.Character.EvernightMoon','Demo.Character.Verina']
for kind,n in [('hub','ObservationCar'),('city','NewEriduSixthStreetMetro'),('arena','HertaSupportSection')]:
 levels.load_level('/Game/Maps/VerticalSlice/Map_'+n)
 for a in actors.get_all_level_actors():
  if a.get_actor_label().startswith('GAME_'):actors.destroy_actor(a)
 source='/Game/Blueprints/Framework/BP_HSRBattleGameMode' if kind=='arena' else '/Game/Blueprints/Framework/BP_HSRGameMode'
 bp=clone(source,'/Game/Presentation/Blueprints/BP_'+n+'GameMode')
 cdo=unreal.get_default_object(bp.generated_class())
 if kind=='arena':props(cdo,character_catalog=catalog,enemy_catalog=ec)
 else:props(cdo,character_catalog=catalog,initial_character_id=party[0],initial_party_ids=party,map_catalog=mapcat,initial_map_id='Demo.Map.'+n)
 unreal.BlueprintEditorLibrary.compile_blueprint(bp);save(bp)
 world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
 world.get_world_settings().set_editor_property('default_game_mode',bp.generated_class())
 content=spawn('HSRSceneContent','Content',(0,0,0))
 props(content,quest_definition=quest,items=items,drop_table=drop,rewards=rewards,equipment_definitions=[weapon],relic_definitions=relics,
  reward_events={'Demo.Reward.WangXiaYiTong':'Demo.Event.DomainEcho.OpenWangXiaYiTong'},
  encounter_events={'Demo.Encounter.SupportSectionInspector':'Demo.Event.DomainEcho.DefeatSupportSectionInspector','Demo.Encounter.Laigushi':'Demo.Event.DomainEcho.DefeatLaigushi'})
 if kind!='arena':spawn('HSRExplorationReturnConsumer','BattleReturn',(0,0,0))
 if kind=='hub':
  npc=spawn('HSRDialogueInteractable','Catherine',(-10,-1,.3))
  props(npc,dialogue_id='Demo.Dialogue.Catherine',dialogue_definition=dialogue,quest_definition=quest)
  interaction('TravelToCity',(-28,-5,0),'前往第六街',teleport_id='Demo.Teleport.ObservationCarToSixthStreet')
  interaction('SurveyObservation',(20,7,.3),'调查 · 观景厅界域记录',quest_event_id='Demo.Event.DomainEcho.SurveyObservationCar',discovery_id='Demo.Discovery.ObservationRecord')
 elif kind=='city':
  interaction('TravelToHub',(0,-94,0),'返回观景列车',teleport_id='Demo.Teleport.SixthStreetToObservationCar')
  for label,pos in [('Plaza',(-15,12,0)),('Terrace',(18,85,5.15)),('Courtyard',(89,28,0))]:
   interaction('Survey'+label,pos,'调查 · 界域回响',quest_event_id='Demo.Event.DomainEcho.SurveySixthStreet',discovery_id='Demo.Discovery.'+label)
  for i,pos in enumerate([(-26,-28,.2),(83,26,.2),(-84,24,.2),(22,87,5.15)]):
   chest=spawn('HSRRewardChest','WangXiaYiTong' if i==0 else 'Cache'+str(i),pos)
   props(chest,stable_claim_id=guid('WangXiaYiTong' if i==0 else 'Cache'+str(i)),item_definitions=items,drop_table_definition=drop,
    reward_definition=load(base+'Rewards/DA_Reward_WangXiaYiTong') if i==0 else load(base+'Rewards/DA_Reward_DomainEcho'),reward_seed=410+i)
   chest.get_editor_property('mesh_component').set_static_mesh(load('/Game/Presentation/Meshes/SM_Chest'))
   chest.get_editor_property('mesh_component').set_collision_profile_name('NoCollision')
   chest.get_editor_property('collision_component').set_collision_profile_name('OverlapAllDynamic')
   chest.get_editor_property('collision_component').set_sphere_radius(180)
  for label,pos,definition in [('Inspector',(-91,-70,0),'SupportSectionInspector'),('Patrol',(-92,65,0),'CourtyardPatrol'),('Laigushi',(88,85,0),'Laigushi')]:
   if label=='Patrol':
    a=spawn('HSREnemyCharacter','CourtyardPatrolAI',(pos[0],pos[1],1))
    props(a,enemy_definition=load(base+'Enemies/DA_Enemy_CourtyardPatrol'))
   else:
    a=interaction(label,pos,'挑战 · '+('来古士' if label=='Laigushi' else '巡检机'),mesh='Sentry',encounter_definition=load(base+'Encounters/DA_Encounter_'+definition))
 levels.save_current_level()
# Navigation is authored by the focused patrol script after rebuilding the maps.
exec(Path(unreal.Paths.project_dir(),'Tools/Presentation/author_patrol_fix.py').read_text(encoding='utf-8'))
exec(Path(unreal.Paths.project_dir(),'Tools/Presentation/repair_equipment_effect.py').read_text(encoding='utf-8'))
print(json.dumps({'success':True,'characters':len(characters),'maps':3,'investigations':4,'chests':4,'encounters':3}))
