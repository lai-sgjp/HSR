"""Blender 3.6: extract CC0 Death01 from the documented standard library mirror."""
import bpy
from pathlib import Path
root = Path(__file__).resolve().parents[2]
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.gltf(filepath=str(root / 'Saved/Presentation/Quaternius/glTF/AnimationLibrary_Godot_Standard.gltf'))
rig = next(o for o in bpy.data.objects if o.type == 'ARMATURE')
death = next(a for a in bpy.data.actions if 'Death01' in a.name)
for obj in bpy.data.objects:
    if obj.animation_data:
        for track in list(obj.animation_data.nla_tracks):
            obj.animation_data.nla_tracks.remove(track)
        obj.animation_data.action = None
rig.animation_data_create()
rig.animation_data.action = death
bpy.context.scene.frame_start = int(death.frame_range[0])
bpy.context.scene.frame_end = int(death.frame_range[1])
out = root / 'ArtSource/Presentation/Quaternius'
out.mkdir(parents=True, exist_ok=True)
for stem,action_name in [('QuaterniusDeath','Death01'),('QuaterniusCast','Spell_Simple_Shoot'),('QuaterniusHeal','Spell_Simple_Enter')]:
    action=next(a for a in bpy.data.actions if a.name.startswith(action_name+'_'))
    rig.animation_data.action=action
    bpy.context.scene.frame_start=int(action.frame_range[0])
    bpy.context.scene.frame_end=int(action.frame_range[1])
    bpy.ops.export_scene.fbx(filepath=str(out/(stem+'.fbx')), object_types={'ARMATURE','MESH'}, add_leaf_bones=False, bake_anim=True, bake_anim_use_all_actions=False, bake_anim_use_nla_strips=False, bake_anim_simplify_factor=0, axis_forward='-Y', axis_up='Z')
    print('EXPORTED', action.name, tuple(action.frame_range))
