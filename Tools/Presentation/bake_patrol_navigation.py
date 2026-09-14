"""Persist completed navigation tiles so PIE does not discard them on startup."""
import unreal,json
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
if unreal.NavigationSystemV1.is_navigation_being_built_or_locked(world):
    raise RuntimeError('Navigation build is still running; retry after completion')
rows=[]
for a in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.RecastNavMesh):
    a.set_editor_property('runtime_generation',unreal.RuntimeGenerationType.STATIC)
    a.set_editor_property('force_rebuild_on_load',False)
    rows.append(str(a.get_actor_bounds(False)))
unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
print(json.dumps({'success':True,'bounds':rows}))
