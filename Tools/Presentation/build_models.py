"""Original modular night-city/arena kit. Run with Blender --background --python.

Metres in Blender; FBX exports centimetres for Unreal. No downloaded geometry.
"""
import math
from pathlib import Path
import bpy

ROOT = Path(__file__).resolve().parents[2] / 'ArtSource/Presentation'
ROOT.mkdir(parents=True, exist_ok=True)
bpy.ops.object.select_all(action='SELECT')
bpy.ops.object.delete(use_global=False)
bpy.context.scene.unit_settings.system = 'METRIC'

def material(name, color, metal=0, rough=.5, emission=0):
    m = bpy.data.materials.new(name)
    m.diffuse_color = (*color, 1)
    m.use_nodes = True
    bsdf = m.node_tree.nodes.get('Principled BSDF')
    bsdf.inputs['Base Color'].default_value = (*color, 1)
    bsdf.inputs['Metallic'].default_value = metal
    bsdf.inputs['Roughness'].default_value = rough
    if emission:
        bsdf.inputs['Emission Color'].default_value = (*color, 1)
        bsdf.inputs['Emission Strength'].default_value = emission
    return m

stone = material('Stone_Blue', (.18,.25,.32), rough=.68)
trim = material('Stone_Ivory', (.58,.62,.63), rough=.55)
gold = material('Metal_Brass', (.63,.38,.12), .78,.28)
dark = material('Metal_Charcoal', (.035,.06,.085), .6,.32)
wood = material('Wood_Walnut', (.19,.085,.035), rough=.5)
glass = material('Glass_Midnight', (.022,.065,.105), .45,.18)
warm = material('Glow_Amber', (1,.47,.12), emission=3)
cyan = material('Glow_Cyan', (.1,.6,.85), emission=2)
leaf = material('Leaves_Teal', (.035,.19,.13), rough=.82)
red = material('Fabric_Wine', (.24,.025,.055), rough=.82)
kit = {}
parts = []

def cube(name, loc, size, mat, bevel=.035):
    bpy.ops.mesh.primitive_cube_add(size=1, location=loc)
    o=bpy.context.object; o.name=name
    o.dimensions=size
    bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
    if bevel:
        mod=o.modifiers.new('Soft machined edges', 'BEVEL'); mod.width=bevel; mod.segments=2
        bpy.ops.object.modifier_apply(modifier=mod.name)
    o.data.materials.append(mat); parts.append(o)
    return o

def cylinder(name, loc, radius, depth, mat, vertices=24):
    bpy.ops.mesh.primitive_cylinder_add(vertices=vertices, radius=radius, depth=depth, location=loc)
    o=bpy.context.object; o.name=name; o.data.materials.append(mat); parts.append(o)
    return o

def finish(name):
    bpy.ops.object.select_all(action='DESELECT')
    for o in parts: o.select_set(True)
    bpy.context.view_layer.objects.active=parts[0]
    bpy.ops.object.join()
    o=bpy.context.object; o.name='SM_'+name
    bpy.context.scene.cursor.location=(0,0,0)
    bpy.ops.object.origin_set(type='ORIGIN_CURSOR')
    bpy.ops.export_scene.fbx(filepath=str(ROOT/(o.name+'.fbx')), use_selection=True,
        global_scale=1, apply_unit_scale=True, bake_space_transform=False,
        object_types={'MESH'}, add_leaf_bones=False, axis_forward='-Y', axis_up='Z')
    kit[name]=o; parts.clear()

# Six metre facade bay: recessed glass, piers, cornices and warm shop windows.
cube('wall',(0,.22,4),(6,.5,8),stone)
for z in [.15,3.8,7.65,8]: cube('cornice',(0,-.12,z),(6.25,.8,.22),trim)
for x in [-2.8,2.8]:
    cube('pier',(x,-.18,3.9),(.42,.68,7.8),trim)
    cube('brass flute',(x,-.54,4),(.08,.035,6.7),gold,.01)
for x in [-1.65,0,1.65]:
    cube('shop glass',(x,-.06,1.9),(1.45,.15,2.75),warm)
    cube('upper glass',(x,-.06,5.8),(1.3,.15,2.25),glass)
    for dx in [-.72,.72]: cube('frame',(x+dx,-.2,1.9),(.06,.13,2.8),gold,.008)
    cube('upper sill',(x,-.32,4.6),(1.6,.55,.16),trim)
cube('sign',(0,-.4,3.4),(5.3,.2,.48),dark)
cube('sign highlight',(0,-.515,3.56),(4.8,.02,.025),gold,.005)
finish('Facade')

for x in [-1.05,1.05]:
    for y in [-.28,.28]: cube('bench foot',(x,y,.23),(.11,.13,.46),dark)
for y in [-.28,-.09,.1,.29]: cube('seat slat',(0,y,.5),(2.6,.16,.09),wood)
for z in [.8,1.02]: cube('back slat',(0,.38,z),(2.6,.1,.17),wood)
for x in [-1.1,1.1]: cube('back support',(x,.4,.7),(.1,.1,.65),dark)
finish('Bench')

cylinder('foot',(0,0,.12),.23,.24,dark)
cylinder('pole',(0,0,1.9),.065,3.6,dark)
for z in [.35,3.4]: cylinder('collar',(0,0,z),.12,.1,gold)
cube('lantern',(0,0,3.9),(.3,.3,.75),warm)
for x in [-.18,.18]:
    for y in [-.18,.18]: cube('frame',(x,y,3.9),(.035,.035,.84),dark)
cube('cap',(0,0,4.35),(.52,.52,.12),gold)
finish('StreetLamp')

cube('planter',(0,0,.32),(2.5,1.15,.64),stone)
cube('rim',(0,0,.65),(2.65,1.3,.12),gold)
for x in [-.8,0,.8]:
    bpy.ops.mesh.primitive_ico_sphere_add(subdivisions=2,radius=.62,location=(x,0,.92))
    o=bpy.context.object; o.scale=(1,.75,.7); o.data.materials.append(leaf); parts.append(o)
finish('Planter')

for x in [-1.45,1.45]:
    cube('post',(x,0,.6),(.16,.18,1.2),trim)
    cube('cap',(x,0,1.2),(.24,.26,.08),gold)
for z in [.2,1.1]: cube('rail',(0,0,z),(3,.09,.09),gold)
for x in [-1,-.5,0,.5,1]: cube('bar',(x,0,.65),(.035,.065,.9),dark,.008)
finish('Railing')

cylinder('bin body',(0,0,.5),.34,1,dark)
cylinder('rim',(0,0,1.03),.37,.12,gold)
cube('opening',(0,-.33,.86),(.4,.04,.12),glass)
finish('Bin')

cylinder('hydrant',(0,0,.44),.16,.88,red)
cylinder('base',(0,0,.08),.25,.16,dark)
cylinder('cap',(0,0,.9),.21,.12,gold)
for x in [-.23,.23]: cube('outlet',(x,0,.6),(.22,.22,.22),red)
finish('Hydrant')

cube('plinth',(0,0,.22),(1.5,1.5,.44),stone)
cube('step',(0,0,.53),(1.2,1.2,.18),trim)
cylinder('column',(0,0,3.9),.42,6.6,trim,32)
for z in [.8,6.9,7.2]: cylinder('capital',(0,0,z),.62,.25,gold,32)
finish('Column')

# True segmented arch rather than a solid wall across the opening.
for x in [-3.2,3.2]: cube('arch pier',(x,0,2.5),(1,.95,5),trim)
for i in range(20):
    a=math.pi*i/20; b=math.pi*(i+1)/20
    verts=[]
    for y in [-.5,.5]:
        for r,t in [(2.7,a),(3.7,a),(3.7,b),(2.7,b)]: verts.append((r*math.cos(t),y,5+r*math.sin(t)))
    mesh=bpy.data.meshes.new('voussoir'); mesh.from_pydata(verts,[],[(0,3,2,1),(4,5,6,7),(0,1,5,4),(1,2,6,5),(2,3,7,6),(3,0,4,7)]); mesh.update()
    o=bpy.data.objects.new('arch segment',mesh); bpy.context.collection.objects.link(o); o.data.materials.append(trim if i%3 else gold); parts.append(o)
finish('Arch')

cube('brazier base',(0,0,.2),(1.1,1.1,.4),stone)
cylinder('stem',(0,0,.95),.22,1.3,gold)
bpy.ops.mesh.primitive_cone_add(vertices=32,radius1=.32,radius2=.8,depth=.38,location=(0,0,1.7))
o=bpy.context.object; o.data.materials.append(gold); parts.append(o)
for x,y,z in [(0,0,2.1),(.23,0,2),(-.15,.15,2.05)]:
    bpy.ops.mesh.primitive_ico_sphere_add(subdivisions=2,radius=.3,location=(x,y,z))
    o=bpy.context.object; o.scale=(.65,.65,2.1); o.data.materials.append(warm); parts.append(o)
finish('Brazier')

cube('banner',(0,0,2.2),(1.45,.06,3.3),red)
cube('top rail',(0,0,3.9),(1.7,.14,.14),gold)
for x in [-.65,.65]: cube('embroidery',(x,-.04,2.2),(.035,.025,3.2),gold,.004)
o=cube('emblem',(0,-.06,2.3),(.5,.03,.5),gold); o.rotation_euler.y=math.pi/4
finish('Banner')

# Palms give the plaza a distinct silhouette.
cylinder('trunk',(0,0,2.8),.2,5.6,wood)
for i in range(9):
    a=i*math.tau/9
    verts=[(0,0,5.6),(1.7*math.cos(a-.2),1.7*math.sin(a-.2),6.15),(3.2*math.cos(a),3.2*math.sin(a),5.25),(1.7*math.cos(a+.2),1.7*math.sin(a+.2),6.15)]
    mesh=bpy.data.meshes.new('frond'); mesh.from_pydata(verts,[],[(0,1,2,3),(3,2,1,0)]); mesh.update()
    o=bpy.data.objects.new('palm frond',mesh); bpy.context.collection.objects.link(o); o.data.materials.append(leaf); parts.append(o)
finish('Palm')

cylinder('anchor base',(0,0,.12),.85,.24,stone,48)
cylinder('anchor ring',(0,0,.3),.7,.12,gold,48)
for i in range(4):
    a=i*math.pi/2
    o=cube('crystal',(math.cos(a)*.26,math.sin(a)*.26,1.3),(.18,.18,1.7),cyan)
    o.rotation_euler.y=.2
finish('TravelAnchor')

cube('chest',(0,0,.38),(1.1,.7,.7),wood)
for x in [-.46,.46]: cube('strap',(x,0,.4),(.07,.75,.78),gold)
cube('lid',(0,0,.78),(1.15,.75,.15),dark)
cube('lock',(0,-.39,.55),(.18,.08,.2),gold)
finish('Chest')

# Original clockwork sentry silhouette, shared by patrols and the scaled guardian.
for side in [-1,1]:
    cube('armour boot',(side*.28,0,.18),(.43,.68,.36),dark)
    cylinder('shin',(side*.28,0,.58),.16,.58,gold)
    cube('knee',(side*.28,0,.87),(.34,.36,.3),stone)
    cylinder('thigh',(side*.28,0,1.14),.19,.48,dark)
    cube('shoulder',(side*.58,0,1.97),(.5,.65,.5),gold)
    cylinder('arm',(side*.7,0,1.52),.16,.55,dark)
    cube('gauntlet',(side*.7,0,1.2),(.3,.4,.36),stone)
cube('waist',(0,0,1.42),(.65,.46,.28),gold)
cube('chestplate',(0,0,1.8),(.86,.6,.68),stone,.09)
cube('energy core',(0,-.32,1.84),(.26,.05,.35),cyan,.04)
cube('head',(0,0,2.35),(.43,.47,.5),dark,.06)
cube('visor',(0,-.25,2.38),(.37,.05,.11),warm,.02)
cube('crest',(0,0,2.67),(.09,.55,.23),gold)
finish('Sentry')

# Save the full editable library, spaced apart for inspection.
for i,o in enumerate(kit.values()): o.location=((i%5)*12,(i//5)*14,0)
bpy.ops.wm.save_as_mainfile(filepath=str(ROOT/'NightCityKit.blend'))
print('PRESENTATION_MODELS_READY',len(kit))
