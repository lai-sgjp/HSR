import unreal,json
edit=unreal.MaterialEditingLibrary
tools=unreal.AssetToolsHelpers.get_asset_tools()
def mat(name):
 path='/Game/Presentation/Materials/M_'+name
 a=unreal.load_asset(path)
 return a or tools.create_asset('M_'+name,'/Game/Presentation/Materials',unreal.Material,unreal.MaterialFactoryNew())
sky=mat('NightSky');edit.delete_all_material_expressions(sky)
sky.set_editor_property('two_sided',True)
sky.set_editor_property('shading_model',unreal.MaterialShadingModel.MSM_UNLIT)
sky.set_editor_property('is_sky',True)
c=edit.create_material_expression(sky,unreal.MaterialExpressionConstant3Vector)
c.set_editor_property('constant',unreal.LinearColor(.006,.013,.042,1))
edit.connect_material_property(c,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR)
edit.recompile_material(sky);unreal.EditorAssetLibrary.save_loaded_asset(sky)
# Real-world-scale paving joints and fine stone variation; independent of large actor scale.
for name,color in [('Paving',(.115,.16,.205)),('Stone_Blue',(.18,.25,.32)),('Stone_Ivory',(.58,.62,.63))]:
 m=mat(name);edit.delete_all_material_expressions(m)
 pos=edit.create_material_expression(m,unreal.MaterialExpressionWorldPosition)
 custom=edit.create_material_expression(m,unreal.MaterialExpressionCustom)
 inp=unreal.CustomInput();inp.set_editor_property('input_name','Position')
 custom.set_editor_property('inputs',[inp])
 custom.set_editor_property('output_type',unreal.CustomMaterialOutputType.CMOT_FLOAT3)
 r,g,b=color
 custom.set_editor_property('code',f'''float2 p=Position.xy/80.;
 p.x+=fmod(floor(p.y),2.)*.5;
 float2 cell=frac(p); float joint=step(.018,min(min(cell.x,cell.y),min(1-cell.x,1-cell.y)));
 float grain=frac(sin(dot(floor(Position.xy*2),float2(12.9898,78.233)))*43758.5453);
 return float3({r},{g},{b})*(.65+.35*joint)*(.94+.12*grain);''')
 edit.connect_material_expressions(pos,'',custom,'Position');edit.connect_material_property(custom,'',unreal.MaterialProperty.MP_BASE_COLOR)
 rough=edit.create_material_expression(m,unreal.MaterialExpressionConstant);rough.set_editor_property('r',.65)
 edit.connect_material_property(rough,'',unreal.MaterialProperty.MP_ROUGHNESS)
 edit.recompile_material(m);unreal.EditorAssetLibrary.save_loaded_asset(m)
for name in ['Arch','Facade','Railing','Bench']:
 mesh=unreal.load_asset('/Game/Presentation/Meshes/SM_'+name)
 body=mesh.get_editor_property('body_setup')
 body.set_editor_property('collision_trace_flag',unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
 unreal.EditorAssetLibrary.save_loaded_asset(mesh)
print(json.dumps({'success':True,'materials':4,'collision_openings':4}))
