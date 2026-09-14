"""Replace the courtyard challenge prop with a real AI pawn and author navigation bounds."""
import json
import unreal

levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
levels.load_level('/Game/Maps/VerticalSlice/Map_NewEriduSixthStreetMetro')
lib = unreal.EditorAssetLibrary
definition = unreal.load_asset('/Game/Data/VerticalSlice/Enemies/DA_Enemy_CourtyardPatrol')
definition.set_editor_property('patrol_radius', 650.)
definition.set_editor_property('patrol_wait_time', 1.5)
definition.set_editor_property('sight_radius', 1300.)
definition.set_editor_property('lose_sight_radius', 1800.)
definition.set_editor_property('encounter_radius', 150.)
lib.save_loaded_asset(definition)
boss = unreal.load_asset('/Game/Data/VerticalSlice/Enemies/DA_Boss_Laigushi')
boss.set_editor_property('prefer_wounded_targets', True)
lib.save_loaded_asset(boss)
existing = actors.get_all_level_actors()
patrol = next((a for a in existing if a.get_actor_label() == 'GAME_CourtyardPatrolAI'), None)
if not patrol:
    patrol = actors.spawn_actor_from_class(unreal.HSREnemyCharacter, unreal.Vector(-9200,6500,100))
    patrol.set_actor_label('GAME_CourtyardPatrolAI')
patrol.set_editor_property('enemy_definition', definition)
patrol.get_editor_property('patrol_mesh').set_static_mesh(definition.get_editor_property('battle_mesh'))
for actor in existing:
    if actor.get_actor_label() == 'GAME_Patrol' and isinstance(actor, unreal.HSRSceneInteraction):
        actors.destroy_actor(actor)
nav = next((a for a in existing if a.get_actor_label() == 'GAME_PlayableNavigation'), None)
if not nav:
    nav = actors.spawn_actor_from_class(unreal.NavMeshBoundsVolume, unreal.Vector(0,0,350))
    nav.set_actor_label('GAME_PlayableNavigation')
    nav.set_actor_scale3d(unreal.Vector(140,140,12))
# Only the courtyard encounter needs navigation. Building all decorative city
# tiles delayed first movement by minutes on PIE startup.
nav.set_actor_location(unreal.Vector(-9200,6500,350),False,True)
nav.set_actor_scale3d(unreal.Vector(24,24,6))
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
unreal.SystemLibrary.execute_console_command(world, 'RebuildNavigation')
for actor in actors.get_all_level_actors():
    if isinstance(actor, unreal.RecastNavMesh):
        actor.set_editor_property('runtime_generation', unreal.RuntimeGenerationType.DYNAMIC)
        actor.set_editor_property('force_rebuild_on_load', True)
levels.save_current_level()
# Let the editor finish its build before persisting static navigation. Starting
# PIE while the editor world is mid-build suspends those unfinished tiles.
nav_elapsed=[0.];nav_handle=[None]
def finish_navigation(dt):
    nav_elapsed[0]+=dt
    if nav_elapsed[0]<1 or unreal.NavigationSystemV1.is_navigation_being_built_or_locked(world): return
    unreal.unregister_slate_post_tick_callback(nav_handle[0])
    for data in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.RecastNavMesh):
        data.set_editor_property('runtime_generation',unreal.RuntimeGenerationType.STATIC)
        data.set_editor_property('force_rebuild_on_load',False)
    levels.save_current_level()
    unreal.log('HSR_NAVIGATION_BAKED_READY')
nav_handle[0]=unreal.register_slate_post_tick_callback(finish_navigation)
print(json.dumps({'success': True, 'navigation': 'building; wait for HSR_NAVIGATION_BAKED_READY before PIE', 'nav_bounds': str(nav.get_actor_bounds(False)), 'patrol': patrol.get_path_name()}))
