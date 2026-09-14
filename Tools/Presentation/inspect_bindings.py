import json
from pathlib import Path
import unreal
result={}
for name in ['BP_HSRGameMode','BP_HSRBattleGameMode']:
    bp=unreal.load_asset('/Game/Blueprints/Framework/'+name)
    cdo=unreal.get_default_object(bp.generated_class())
    result[name]={}
    for prop in ['default_pawn_class','hud_class','character_catalog','initial_character_id','map_catalog','player_controller_class','basic_attack_skill_definition','participant_initialization_gameplay_effect','character_progression_gameplay_effect','battle_command_widget_class','enemy_catalog']:
        try: result[name][prop]=str(cdo.get_editor_property(prop))
        except Exception: pass
for name in ['EvernightMoon','Huohua','Remiel','Verina']:
    a=unreal.load_asset('/Game/Data/VerticalSlice/Characters/DA_Character_'+name)
    result[name]={'type':str(a.get_class()),'class':str(a.get_editor_property('character_class')),'mesh':str(a.get_editor_property('character_mesh'))} if not isinstance(a,unreal.Blueprint) else {'type':'Blueprint','generated':str(a.generated_class())}
mesh=unreal.load_asset('/Game/Presentation/Meshes/SM_Facade')
result['facade']=[{'slot':str(s.get_editor_property('imported_material_slot_name')),'mat':str(s.get_editor_property('material_interface'))} for s in mesh.get_editor_property('static_materials')]
out=Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))/'Saved/Presentation/bindings.json'
out.write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding='utf-8')
print(json.dumps({'success':True,'bindings':result},ensure_ascii=False))
