import unreal,json
a=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
out={}
for name in ['Huohua','Remiel','EvernightMoon','Verina']:
 d=unreal.load_asset('/Game/Data/VerticalSlice/Characters/DA_Character_'+name)
 actor=a.spawn_actor_from_class(unreal.SkeletalMeshActor,unreal.Vector(0,0,20000))
 c=actor.skeletal_mesh_component;c.set_skeletal_mesh_asset(d.get_editor_property('character_mesh'))
 actor.set_actor_rotation(unreal.Rotator(yaw=-90),False)
 bones=[str(c.get_bone_name(i)) for i in range(c.get_num_bones())]
 names=[n for n in bones if 'head' in n.lower() or 'neck' in n.lower()]
 out[name]={'bounds':str(actor.get_actor_bounds(False)),'bones':{n:str(c.get_socket_location(n)) for n in names}}
 a.destroy_actor(actor)
print(json.dumps(out))
