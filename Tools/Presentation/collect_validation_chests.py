import unreal,json
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
pc=unreal.GameplayStatics.get_player_controller(world,0);pc.request_close_frontend_to_root()
pawn=unreal.GameplayStatics.get_player_pawn(world,0)
results=[]
for chest in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.HSRRewardChest):
    p=chest.get_actor_location();p.x-=120;p.z+=110
    pawn.set_actor_location(p,False,True)
    results.append({'chest':chest.get_actor_label(),'result':str(pawn.get_component_by_class(unreal.HSRInteractionComponent).try_interact())})
print(json.dumps({'success':True,'results':results}))
