import unreal,json
rows=[]
for a in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors():
    if isinstance(a,unreal.StaticMeshActor):
        c=a.static_mesh_component
        row={'label':a.get_actor_label(),'mesh':str(c.static_mesh),'location':str(a.get_actor_location())}
        try:row['navigation']=str(c.get_editor_property('can_ever_affect_navigation'))
        except Exception as e:row['navigation']=str(e)
        rows.append(row)
print(json.dumps(rows[:25]))
