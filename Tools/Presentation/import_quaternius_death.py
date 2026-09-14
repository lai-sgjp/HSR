"""Import the CC0 source skeleton and complete death animation."""
from pathlib import Path
import unreal, json
unreal.SystemLibrary.execute_console_command(None, 'Interchange.FeatureFlags.Import.FBX 0')
t=unreal.AssetImportTask()
t.filename=str(Path(unreal.Paths.project_dir(),'ArtSource/Presentation/Quaternius/QuaterniusDeath.fbx').resolve())
t.destination_path='/Game/Presentation/Animation/CC0'
t.destination_name='QuaterniusDeath'
t.automated=True
t.save=True
t.replace_existing=True
t.factory=unreal.FbxFactory()
o=unreal.FbxImportUI()
o.import_as_skeletal=True
o.import_mesh=True
o.import_animations=True
o.import_materials=False
o.import_textures=False
o.mesh_type_to_import=unreal.FBXImportType.FBXIT_SKELETAL_MESH
o.automated_import_should_detect_type=False
t.options=o
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([t])
print(json.dumps({'success':True,'assets':list(t.imported_object_paths)}))
