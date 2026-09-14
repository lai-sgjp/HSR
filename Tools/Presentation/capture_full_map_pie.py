import unreal,json
w=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
pc=unreal.GameplayStatics.get_player_controller(w,0)
pc.request_close_frontend_to_root();pc.request_open_pause_screen()
shell=next(x for x in unreal.WidgetLibrary.get_all_widgets_of_class(w,unreal.HSRFrontendShellWidget,False) if x.is_in_viewport())
shell.request_open_module(unreal.HSRFrontendModule.MAP)
t=[0.];h=[None]
def capture(dt):
    t[0]+=dt
    if t[0]<.5:return
    unreal.unregister_slate_post_tick_callback(h[0])
    unreal.MCPythonHelper.capture_play_viewport('full_map_shared_markers')
h[0]=unreal.register_slate_post_tick_callback(capture)
print(json.dumps({'success':True}))
