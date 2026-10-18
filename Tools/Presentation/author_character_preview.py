"""Wire only the existing character shell, UI material and four preview-idle fields.

Run through editor_bridge.py after compiling HSREditor. No raw character asset is changed.
The exact original packages are backed up in Saved/CharacterPreview/BeforeAssets.
"""
import hashlib
import json
import shutil
from pathlib import Path
import unreal

root = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
out = root / 'Saved/CharacterPreview'
out.mkdir(parents=True, exist_ok=True)
shell_path = '/Game/UI/P17/Character/WBP_CharacterShell_P17'
names = ['Huohua', 'EvernightMoon', 'Remiel', 'Verina']
asset_paths = [shell_path] + ['/Game/Data/VerticalSlice/Characters/DA_Character_' + n for n in names]
before = {}
for path in asset_paths:
    file = root / 'Content' / (path.removeprefix('/Game/') + '.uasset')
    backup = out / 'BeforeAssets' / file.relative_to(root)
    backup.parent.mkdir(parents=True, exist_ok=True)
    if not backup.exists():
        shutil.copy2(file, backup)
    before[path] = hashlib.sha256(backup.read_bytes()).hexdigest()

material_path = '/Game/UI/CharacterPreview/M_CharacterPreview'
material = unreal.load_asset(material_path)
if not material:
    material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        'M_CharacterPreview', '/Game/UI/CharacterPreview', unreal.Material, unreal.MaterialFactoryNew())
    material.set_editor_property('material_domain', unreal.MaterialDomain.MD_UI)
    material.set_editor_property('blend_mode', unreal.BlendMode.BLEND_OPAQUE)
    sample = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionTextureSampleParameter2D, -250, 0)
    sample.set_editor_property('parameter_name', 'PreviewTexture')
    sample.set_editor_property('texture', unreal.load_asset('/Engine/EngineResources/Black'))
    assert unreal.MaterialEditingLibrary.connect_material_property(sample, 'RGB', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    unreal.MaterialEditingLibrary.recompile_material(material)
    assert unreal.EditorAssetLibrary.save_loaded_asset(material, False)

bp = unreal.load_asset(shell_path)
assert isinstance(bp, unreal.WidgetBlueprint)
helper = unreal.MCPythonHelper
host = helper.umg_find_widget(bp, 'CharacterPreviewHost')
if not host:
    result = json.loads(helper.umg_add_widget(bp, 'SizeBox', 'CharacterPreviewHost', 'MainHBox'))
    assert result.get('success'), result
    host = helper.umg_find_widget(bp, 'CharacterPreviewHost')
    # Move the existing detail column after the new middle preview column.
    result = json.loads(helper.umg_reparent_widget(bp, 'DetailPanel', 'MainHBox'))
    assert result.get('success'), result
for name, weight in [('ListScrollBox', .3), ('CharacterPreviewHost', .7), ('DetailPanel', 1.0)]:
    widget = helper.umg_find_widget(bp, name)
    slot = widget.get_editor_property('slot')
    size = slot.get_editor_property('size')
    size.set_editor_property('size_rule', unreal.SlateSizeRule.FILL)
    size.set_editor_property('value', weight)
    slot.set_editor_property('size', size)
    slot.set_padding(unreal.Margin(8, 0, 8, 0))
    slot.set_horizontal_alignment(unreal.HorizontalAlignment.H_ALIGN_FILL)
    slot.set_vertical_alignment(unreal.VerticalAlignment.V_ALIGN_FILL)
    widget.set_visibility(unreal.SlateVisibility.VISIBLE)
host.clear_width_override()
host.clear_height_override()
unreal.get_default_object(bp.generated_class()).set_editor_property('preview_material', material)
unreal.BlueprintEditorLibrary.compile_blueprint(bp)
assert unreal.EditorAssetLibrary.save_loaded_asset(bp, False)

definitions = []
for name in names:
    path = '/Game/Data/VerticalSlice/Characters/DA_Character_' + name
    definition = unreal.load_asset(path)
    idle = unreal.load_asset('/Game/Presentation/Animation/Locomotion/' + name + '/FINAL_' + name + '_MM_Idle')
    mesh = definition.get_editor_property('character_mesh')
    assert mesh and idle, (path, mesh, idle)
    assert mesh.get_editor_property('skeleton') == idle.get_editor_property('skeleton'), name
    definition.set_editor_property('preview_idle_animation', idle)
    definition.set_editor_property('preview_yaw', -90.0)
    assert unreal.EditorAssetLibrary.save_loaded_asset(definition, False)
    definitions.append({'id': str(definition.get_editor_property('character_id')), 'definition': path,
                        'mesh': mesh.get_path_name(), 'idle': idle.get_path_name(),
                        'skeleton': mesh.get_editor_property('skeleton').get_path_name()})

# Read back after save; this is asset wiring evidence, separate from PIE evidence.
assert unreal.get_default_object(bp.generated_class()).get_editor_property('preview_material') == material
report = {'material': material.get_path_name(), 'shell': shell_path,
          'definitions': definitions, 'original_sha256': before,
          'widget_tree': json.loads(helper.umg_get_widget_info(bp)),
          'scope': 'local existing character references; no raw MMD assets copied or published'}
(out / 'asset_wiring.json').write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
print(json.dumps({'success': True, 'report': str(out / 'asset_wiring.json')}, ensure_ascii=False))
