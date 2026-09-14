import unreal,json
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world() or unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
rows=[]
for cls in [unreal.NavMeshBoundsVolume,unreal.RecastNavMesh]:
    for a in unreal.GameplayStatics.get_all_actors_of_class(world,cls):
        row={'class':cls.__name__,'name':a.get_name(),'bounds':str(a.get_actor_bounds(False))}
        for p in ['runtime_generation','force_rebuild_on_load','brush','brush_builder','cell_size','cell_height']:
            try:row[p]=str(a.get_editor_property(p))
            except Exception:pass
        rows.append(row)
print(json.dumps({'world':str(world),'actors':rows,'building':unreal.NavigationSystemV1.is_navigation_being_built_or_locked(world)}))
