import json
from pathlib import Path
import unreal

# UE5.6 Interchange can re-enter the task graph during a synchronous editor bridge call.
# The installed engine's legacy FBX factory supports this explicit automated task path.
unreal.SystemLibrary.execute_console_command(None, 'Interchange.FeatureFlags.Import.FBX 0')

root = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
source = root / 'ArtSource/Presentation'
destination = '/Game/Presentation/Meshes'
palette = {
 'Stone_Blue':(.18,.25,.32,0,.68,0), 'Stone_Ivory':(.58,.62,.63,0,.55,0),
 'Metal_Brass':(.63,.38,.12,.78,.28,0), 'Metal_Charcoal':(.035,.06,.085,.6,.32,0),
 'Wood_Walnut':(.19,.085,.035,0,.5,0), 'Glass_Midnight':(.022,.065,.105,.45,.18,0),
 'Glow_Amber':(1,.47,.12,0,.3,3), 'Glow_Cyan':(.1,.6,.85,0,.3,2),
 'Leaves_Teal':(.035,.19,.13,0,.82,0), 'Fabric_Wine':(.24,.025,.055,0,.82,0),
 'Paving':(.115,.16,.205,0,.65,0), 'Water':(.025,.14,.2,.6,.13,0)
}
tools=unreal.AssetToolsHelpers.get_asset_tools()
materials={}
for name,values in palette.items():
    path='/Game/Presentation/Materials/M_'+name
    m=unreal.load_asset(path)
    if not m:
        m=tools.create_asset('M_'+name,'/Game/Presentation/Materials',unreal.Material,unreal.MaterialFactoryNew())
        color=unreal.MaterialEditingLibrary.create_material_expression(m,unreal.MaterialExpressionConstant3Vector)
        color.set_editor_property('constant',unreal.LinearColor(*values[:3],1))
        unreal.MaterialEditingLibrary.connect_material_property(color,'',unreal.MaterialProperty.MP_BASE_COLOR)
        for value,prop in [(values[3],unreal.MaterialProperty.MP_METALLIC),(values[4],unreal.MaterialProperty.MP_ROUGHNESS)]:
            n=unreal.MaterialEditingLibrary.create_material_expression(m,unreal.MaterialExpressionConstant)
            n.set_editor_property('r',value); unreal.MaterialEditingLibrary.connect_material_property(n,'',prop)
        if values[5]:
            glow=unreal.MaterialEditingLibrary.create_material_expression(m,unreal.MaterialExpressionConstant3Vector)
            glow.set_editor_property('constant',unreal.LinearColor(*(v*values[5] for v in values[:3]),1))
            unreal.MaterialEditingLibrary.connect_material_property(glow,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR)
        m.set_editor_property('two_sided',name=='Leaves_Teal')
        unreal.MaterialEditingLibrary.recompile_material(m)
        unreal.EditorAssetLibrary.save_loaded_asset(m)
    materials[name]=m

tasks=[]
for file in sorted(source.glob('SM_*.fbx')):
    task=unreal.AssetImportTask()
    task.set_editor_property('filename',str(file))
    task.set_editor_property('destination_path',destination)
    task.set_editor_property('destination_name',file.stem)
    task.set_editor_property('automated',True)
    task.set_editor_property('save',True)
    task.set_editor_property('replace_existing',True)
    task.set_editor_property('factory',unreal.FbxFactory())
    options=unreal.FbxImportUI()
    options.set_editor_property('import_mesh',True)
    options.set_editor_property('import_materials',False)
    options.set_editor_property('import_textures',False)
    options.set_editor_property('import_as_skeletal',False)
    options.set_editor_property('mesh_type_to_import',unreal.FBXImportType.FBXIT_STATIC_MESH)
    options.static_mesh_import_data.set_editor_property('combine_meshes',True)
    options.static_mesh_import_data.set_editor_property('auto_generate_collision',True)
    task.set_editor_property('options',options); tasks.append(task)
tools.import_asset_tasks(tasks)
result=[]
for file in sorted(source.glob('SM_*.fbx')):
    mesh=unreal.load_asset(destination+'/'+file.stem)
    if not mesh: raise RuntimeError('Import failed: '+file.stem)
    for i,slot in enumerate(mesh.get_editor_property('static_materials')):
        name=str(slot.get_editor_property('imported_material_slot_name'))
        if name in materials: mesh.set_material(i,materials[name])
    unreal.EditorAssetLibrary.save_loaded_asset(mesh)
    bounds=mesh.get_bounding_box()
    result.append({'mesh':file.stem,'size':str(bounds.max-bounds.min)})
print(json.dumps({'success':True,'meshes':result}))
