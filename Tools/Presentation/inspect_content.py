import json
from pathlib import Path
import unreal
result={}
for path in ['/Game/Blueprints','/Game/Data/VerticalSlice']:
    result[path]=list(unreal.EditorAssetLibrary.list_assets(path,recursive=True,include_folder=False))
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
result['world_settings']=str(world.get_world_settings().get_editor_property('default_game_mode'))
for name in ['HSRRewardChest','HSRGrayboxInteractable','HSREncounterTrigger','HSRDialogueInteractable','HSRMapTravelInteractable']:
    cls=unreal.load_class(None,'/Script/HSR.'+name)
    result[name]=str(cls)
Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()),'Saved/Presentation/content.json').write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding='utf-8')
print(json.dumps({'success':True,'world_settings':result['world_settings']}))
