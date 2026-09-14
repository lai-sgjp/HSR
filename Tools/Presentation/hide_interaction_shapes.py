"""Query collision remains enabled; editor prototype outlines are hidden in game."""
import unreal,json
levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
count=0
for name in ['Map_ObservationCar','Map_NewEriduSixthStreetMetro','Map_HertaSupportSection']:
 levels.load_level('/Game/Maps/VerticalSlice/'+name)
 for actor in actors.get_all_level_actors():
  if not actor.get_actor_label().startswith('GAME_'):continue
  for c in actor.get_components_by_class(unreal.ShapeComponent):
   c.set_hidden_in_game(True)
   c.set_visibility(False,False)
   count+=1
 levels.save_current_level()
print(json.dumps({'success':True,'hidden_query_shapes':count}))
