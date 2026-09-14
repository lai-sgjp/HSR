"""Render portraits from the project's existing character meshes, no external art."""
import unreal,json
from pathlib import Path
actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
root=Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
out=root/'ArtSource/Presentation/Portraits';out.mkdir(exist_ok=True)
report=[]
for name in ['Huohua','Remiel','EvernightMoon','Verina']:
 definition=unreal.load_asset('/Game/Data/VerticalSlice/Characters/DA_Character_'+name)
 mesh=definition.get_editor_property('character_mesh')
 a=actors.spawn_actor_from_class(unreal.SkeletalMeshActor,unreal.Vector(0,0,20000))
 a.skeletal_mesh_component.set_skeletal_mesh_asset(mesh)
 a.set_actor_rotation(unreal.Rotator(yaw=-90),False)
 center,extent=a.get_actor_bounds(False)
 capture=actors.spawn_actor_from_class(unreal.SceneCapture2D,unreal.Vector(200,0,20000+extent.z*1.58),unreal.Rotator(pitch=-2,yaw=180))
 c=capture.capture_component2d
 c.set_editor_property('projection_type',unreal.CameraProjectionMode.ORTHOGRAPHIC)
 c.set_editor_property('ortho_width',extent.z*.93)
 c.set_editor_property('primitive_render_mode',unreal.SceneCapturePrimitiveRenderMode.PRM_USE_SHOW_ONLY_LIST)
 c.show_only_actor_components(a)
 c.set_editor_property('capture_source',unreal.SceneCaptureSource.SCS_FINAL_COLOR_LDR)
 rt=unreal.RenderingLibrary.create_render_target2d(world,256,256,unreal.TextureRenderTargetFormat.RTF_RGBA8,unreal.LinearColor(.02,.035,.065,1))
 c.set_editor_property('texture_target',rt)
 c.capture_scene()
 unreal.RenderingLibrary.export_render_target(world,rt,str(out),name+'.png')
 tex=unreal.RenderingLibrary.render_target_create_static_texture2d_editor_only(rt,'/Game/Presentation/UI/T_Portrait_'+name,unreal.TextureCompressionSettings.TC_EDITOR_ICON,unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
 definition.set_editor_property('portrait',tex)
 unreal.EditorAssetLibrary.save_loaded_asset(tex);unreal.EditorAssetLibrary.save_loaded_asset(definition)
 report.append({'name':name,'extent':str(extent),'texture':str(tex)})
 actors.destroy_actor(capture);actors.destroy_actor(a)
 unreal.RenderingLibrary.release_render_target2d(rt)
print(json.dumps({'success':True,'portraits':report}))
