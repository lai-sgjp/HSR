import json
import math
import sys
from pathlib import Path
import unreal

root=Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
sys.path.insert(0,str(root/'Tools/Presentation'))
from scene_layout import layout
levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
report={}
for kind,name in [('hub','Map_ObservationCar'),('city','Map_NewEriduSixthStreetMetro'),('arena','Map_HertaSupportSection')]:
    levels.load_level('/Game/Maps/VerticalSlice/'+name)
    unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world().get_world_settings().set_editor_property('force_no_precomputed_lighting',True)
    # Only replace our authored presentation and the inventoried graybox ground/light.
    for a in actors.get_all_level_actors():
        label=a.get_actor_label()
        if label.startswith('PR_') or label in ['Ground_Floor','DirectionalLight']:
            actors.destroy_actor(a)
    meshes={}
    group=actors.spawn_actor_from_class(unreal.load_class(None,'/Script/HSR.HSRInstancedScene'),unreal.Vector())
    group.set_actor_label('PR_InstancedArchitecture')
    for i,entry in enumerate(layout(kind)):
        key=entry['mesh']
        if key not in meshes:
            meshes[key]=unreal.load_asset('/Engine/BasicShapes/'+key if key in ['Cube','Cylinder'] else '/Game/Presentation/Meshes/SM_'+key)
        if not meshes[key]: raise RuntimeError('Missing mesh '+key)
        loc=unreal.Vector(*(v*100 for v in entry['p']))
        material=unreal.load_asset('/Game/Presentation/Materials/M_'+entry['mat']) if entry.get('mat') else None
        transform=unreal.Transform(location=loc,rotation=unreal.Rotator(yaw=entry['yaw']),scale=unreal.Vector(*entry['scale']))
        group.add_scene_instance(meshes[key],material,transform,key not in ['Palm','Banner','StreetLamp','Bin','Hydrant'])
    sun=actors.spawn_actor_from_class(unreal.DirectionalLight,unreal.Vector(0,0,3000),unreal.Rotator(pitch=-35,yaw=-35,roll=0))
    sun.set_actor_label('PR_Moon'); sun.light_component.set_editor_property('intensity',.7)
    sun.light_component.set_mobility(unreal.ComponentMobility.MOVABLE)
    sun.light_component.set_light_color(unreal.LinearColor(.38,.58,1,1))
    sky=actors.spawn_actor_from_class(unreal.SkyLight,unreal.Vector(0,0,2000))
    sky.set_actor_label('PR_Skylight'); sky.light_component.set_editor_property('intensity',.65)
    sky.light_component.set_mobility(unreal.ComponentMobility.MOVABLE)
    sky.light_component.set_editor_property('lower_hemisphere_is_black',False)
    dome=actors.spawn_actor_from_class(unreal.StaticMeshActor,unreal.Vector())
    dome.set_actor_label('PR_NightSky');dome.static_mesh_component.set_static_mesh(unreal.load_asset('/Engine/BasicShapes/Sphere'))
    dome.set_actor_scale3d(unreal.Vector(1000,1000,1000));dome.static_mesh_component.set_material(0,unreal.load_asset('/Game/Presentation/Materials/M_NightSky'))
    dome.static_mesh_component.set_collision_profile_name('NoCollision')
    dome.static_mesh_component.set_cast_shadow(False)
    sky.light_component.set_editor_property('real_time_capture',True)
    fog=actors.spawn_actor_from_class(unreal.ExponentialHeightFog,unreal.Vector(0,0,-500)); fog.set_actor_label('PR_Fog')
    fog.component.set_editor_property('fog_density',.008)
    post=actors.spawn_actor_from_class(unreal.PostProcessVolume,unreal.Vector(0,0,0)); post.set_actor_label('PR_Exposure')
    post.set_editor_property('unbound',True)
    settings=post.get_editor_property('settings')
    for p,v in [('override_auto_exposure_min_brightness',True),('override_auto_exposure_max_brightness',True),('auto_exposure_min_brightness',.5),('auto_exposure_max_brightness',.5),('override_bloom_intensity',True),('bloom_intensity',.45)]:
        settings.set_editor_property(p,v)
    post.set_editor_property('settings',settings)
    for i,e in enumerate(layout(kind)):
        if e['mesh'] not in ['StreetLamp','Brazier']: continue
        # Limited unshadowed pools of light; emissive fixtures supply the visible source.
        x,y,z=e['p']
        light=actors.spawn_actor_from_class(unreal.PointLight,unreal.Vector(x*100,y*100,z*100+330))
        light.set_actor_label('PR_WarmPool_'+str(i))
        light.light_component.set_mobility(unreal.ComponentMobility.MOVABLE)
        light.light_component.set_editor_property('intensity',90)
        light.light_component.set_editor_property('attenuation_radius',650)
        light.light_component.set_editor_property('cast_shadows',False)
        light.light_component.set_light_color(unreal.LinearColor(1,.57,.23,1))
    start=(-2800,-800,110) if kind=='hub' else (0,-9200,110) if kind=='city' else (-600,-360,110)
    for a in actors.get_all_level_actors():
        if isinstance(a,unreal.PlayerStart) or a.get_class().get_name()=='HSRMapArrivalPoint': a.set_actor_location(unreal.Vector(*start),False,False)
    if kind=='arena':
        stage_class=unreal.load_class(None,'/Script/HSR.HSRBattleStage')
        if stage_class:
            stage=actors.spawn_actor_from_class(stage_class,unreal.Vector(0,0,35)); stage.set_actor_label('PR_BattleStage')
            stage.set_editor_property('boss_definition_ids',['Demo.Boss.Laigushi'])
            stage.set_editor_property('enemy_view',unreal.Transform(location=unreal.Vector(-1900,-1700,1000),rotation=unreal.Rotator(pitch=-20,yaw=42)))
            stage.set_editor_property('ally_view',unreal.Transform(location=unreal.Vector(1900,-1400,800),rotation=unreal.Rotator(pitch=-17,yaw=144)))
            stage.set_editor_property('result_view',unreal.Transform(location=unreal.Vector(800,-1300,600),rotation=unreal.Rotator(pitch=-16,yaw=145)))
    levels.save_current_level()
    report[name]=len(actors.get_all_level_actors())
print(json.dumps({'success':True,'actor_counts':report}))
