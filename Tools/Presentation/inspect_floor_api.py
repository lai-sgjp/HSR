import unreal,json
print(json.dumps({'line':unreal.SystemLibrary.line_trace_single.__doc__,'capsule':unreal.SystemLibrary.capsule_trace_single.__doc__,'hit':unreal.GameplayStatics.break_hit_result.__doc__ if hasattr(unreal.GameplayStatics,'break_hit_result') else ''}))
