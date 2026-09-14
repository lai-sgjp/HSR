"""Add ceiling illumination without touching stable interactions or architecture."""
import unreal,json
levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
levels.load_level('/Game/Maps/VerticalSlice/Map_ObservationCar')
existing={a.get_actor_label():a for a in actors.get_all_level_actors()}
for i,x in enumerate(range(-18,32,7)):
 label='PR_InteriorFill_'+str(i)
 light=existing.get(label) or actors.spawn_actor_from_class(unreal.PointLight,unreal.Vector(x*100,300,620))
 light.set_actor_label(label)
 c=light.light_component
 c.set_mobility(unreal.ComponentMobility.MOVABLE)
 c.set_editor_property('intensity_units',unreal.LightUnits.LUMENS)
 c.set_editor_property('intensity',1200)
 c.set_editor_property('attenuation_radius',1600)
 c.set_editor_property('cast_shadows',False)
 c.set_editor_property('specular_scale',0)
 c.set_light_color(unreal.LinearColor(1,.79,.57,1))
label='PR_InteriorFurniture'
if label in existing:actors.destroy_actor(existing[label])
group=actors.spawn_actor_from_class(unreal.HSRInstancedScene,unreal.Vector())
group.set_actor_label(label)
cube=unreal.load_asset('/Engine/BasicShapes/Cube')
brass=unreal.load_asset('/Game/Presentation/Materials/M_Metal_Brass')
for x in [-12,-4,8,20]:
 for dx in [-.8,.8]:
  for dy in [-.35,.35]:
   group.add_scene_instance(cube,brass,unreal.Transform(location=unreal.Vector((x+dx)*100,(3+dy)*100,47),scale=unreal.Vector(.08,.08,.5)),True)
levels.save_current_level()
print(json.dumps({'success':True,'interior_lights':8}))
