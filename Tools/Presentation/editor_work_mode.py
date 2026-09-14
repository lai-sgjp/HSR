import unreal,json
unreal.get_default_object(unreal.load_class(None,'/Script/UnrealEd.EditorPerformanceSettings')).set_editor_property('bThrottleCPUWhenNotForeground',False)
print(json.dumps({'success':True,'background_throttle':'disabled for this editor session'}))
