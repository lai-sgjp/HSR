"""Inspect authored and live character presentation without changing assets."""
import json
from pathlib import Path
import unreal

def prop(obj, name):
    try:
        return str(obj.get_editor_property(name))
    except Exception as error:
        return str(error)

report = {'characters': {}, 'runtime': []}
for name in ['Huohua', 'Remiel', 'EvernightMoon', 'Verina']:
    definition = unreal.load_asset('/Game/Data/VerticalSlice/Characters/DA_Character_' + name)
    row = {key: prop(definition, key) for key in ['character_id', 'character_class', 'character_mesh', 'animation_class']}
    mesh = definition.get_editor_property('character_mesh')
    row['skeleton'] = prop(mesh, 'skeleton') if mesh else None
    anim = definition.get_editor_property('animation_class')
    blueprint = unreal.load_asset(anim.get_path_name().split('.')[0]) if anim else None
    row['anim_skeleton'] = prop(blueprint, 'target_skeleton') if blueprint else None
    row['skeleton_matches'] = bool(blueprint and blueprint.get_editor_property('target_skeleton') == mesh.get_editor_property('skeleton'))
    row['combat'] = {key:prop(definition,key) for key in ['attack_animation','skill_animation','hit_animation','defeat_animation']}
    retargeter=unreal.load_asset('/Game/Presentation/Animation/RTG_Combat_'+name)
    row['retarget_ops']=unreal.IKRetargeterController.get_controller(retargeter).get_num_retarget_ops()
    report['characters'][name] = row
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
if world:
    for actor in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Character):
        mesh = actor.get_editor_property('mesh')
        report['runtime'].append({'actor': actor.get_path_name(), 'mesh': prop(mesh, 'skeletal_mesh_asset'),
            'mode': prop(mesh, 'animation_mode'), 'anim_class': prop(mesh, 'anim_class'),
            'instance': str(mesh.get_anim_instance()), 'tick': mesh.is_component_tick_enabled()})
report['world'] = str(world)
Path(unreal.Paths.project_dir(), 'Saved/Presentation/character_audit.json').write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
print(json.dumps(report, ensure_ascii=False))
