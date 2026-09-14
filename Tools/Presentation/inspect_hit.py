import unreal,json
w=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
h=unreal.SystemLibrary.line_trace_single(w,unreal.Vector(0,-9000,3000),unreal.Vector(0,-9000,-200),unreal.TraceTypeQuery.TRACE_TYPE_QUERY1,False,[],unreal.DrawDebugTrace.NONE)
print(json.dumps({'hit':str(h),'tuple':str(h.to_tuple())}))
