"""Each equipment instance owns a separate GE handle; never merge sources by target."""
import unreal,json
path='/Game/GameplayEffects/GE_Equipment_P12'
bp=unreal.load_asset(path)
default=unreal.get_default_object(bp.generated_class())
default.modify()
default.set_editor_property('stacking_type',unreal.GameplayEffectStackingType.NONE)
unreal.BlueprintEditorLibrary.compile_blueprint(bp)
unreal.EditorAssetLibrary.save_loaded_asset(bp,False)
print(json.dumps({'success':True,'stacking':str(unreal.get_default_object(bp.generated_class()).get_editor_property('stacking_type'))}))
