import unreal,json
result=[]
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
for a in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors():
 row={'name':a.get_actor_label(),'position':str(a.get_actor_location())}
 if a.get_actor_label().startswith('PR_Instanced'):
  row['components']=[{'name':c.get_name(),'count':c.get_instance_count(),'mesh':str(c.get_editor_property('static_mesh'))} for c in a.get_components_by_class(unreal.HierarchicalInstancedStaticMeshComponent)]
 result.append(row)
print(json.dumps({'success':True,'actors':result}))
