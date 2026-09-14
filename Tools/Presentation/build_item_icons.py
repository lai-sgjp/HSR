"""Original small equipment sculpts rendered as inventory icons; Blender source included."""
import bpy,math
from mathutils import Vector
from pathlib import Path
root=Path(__file__).resolve().parents[2]/'ArtSource/Presentation/ItemIcons';root.mkdir(exist_ok=True)
bpy.ops.object.select_all(action='SELECT');bpy.ops.object.delete(use_global=False)
def mat(name,color,metal=0):
 m=bpy.data.materials.new(name);m.diffuse_color=(*color,1);m.use_nodes=True
 p=m.node_tree.nodes.get('Principled BSDF');p.inputs['Base Color'].default_value=(*color,1);p.inputs['Metallic'].default_value=metal;p.inputs['Roughness'].default_value=.27
 return m
gold=mat('Brass',(.7,.43,.11),.8);blue=mat('Midnight enamel',(.025,.08,.17),.55);gem=mat('Cyan crystal',(.08,.65,.7),.3);cloth=mat('Ivory',(.7,.76,.82));red=mat('Ruby',(.5,.04,.12),.3)
parts=[];groups={}
def add(typ,loc,scale,material,rotation=(0,0,0)):
 if typ=='cube':bpy.ops.mesh.primitive_cube_add(size=1,location=loc)
 elif typ=='sphere':bpy.ops.mesh.primitive_uv_sphere_add(segments=24,ring_count=12,radius=.5,location=loc)
 elif typ=='torus':bpy.ops.mesh.primitive_torus_add(major_segments=32,minor_segments=10,major_radius=.45,minor_radius=.065,location=loc)
 elif typ=='cone':bpy.ops.mesh.primitive_cone_add(vertices=8,radius1=.5,radius2=.22,depth=1,location=loc)
 o=bpy.context.object;o.scale=scale;o.rotation_euler=rotation;o.data.materials.append(material)
 bpy.ops.object.transform_apply(location=False,rotation=False,scale=True)
 if typ in ['cube','cone']:
  b=o.modifiers.new('Rounded edges','BEVEL');b.width=.06;b.segments=3;o.modifiers.new('Weighted normals','WEIGHTED_NORMAL')
 else:
  for f in o.data.polygons:f.use_smooth=True
 parts.append(o);return o
def group(name):
 groups[name]=list(parts);parts.clear()
add('cube',(0,0,0),(.58,.26,1.3),blue);add('cube',(0,-.15,0),(.4,.07,1.05),gold);add('sphere',(0,-.2,.15),(.37,.2,.37),gem);add('cube',(0,0,-.95),(.14,.15,.8),gold);group('Weapon')
add('sphere',(0,0,0),(1.1,1.1,1.1),gem);add('torus',(0,0,0),(1.35,1.35,1.35),gold,(math.pi/3,0,0));group('PlanarSphere')
for i in range(5):add('torus',(-.5+i*.25,0,.18*math.sin(i)),(.5,.65,.6),gold,(math.pi/2 if i%2 else 0,0,0))
add('sphere',(.7,0,.15),(.42,.42,.62),gem);group('LinkRope')
add('torus',(0,0,-.1),(1.4,1.4,1),gold)
for i in range(5):
 a=2*math.pi*i/5;add('cone',(.53*math.cos(a),.53*math.sin(a),.2),(.32,.32,.65),gold);add('sphere',(.53*math.cos(a),.53*math.sin(a),.52),(.17,.17,.17),red)
group('Head')
for x in [-.36,.36]:
 add('cube',(x,0,0),(.45,.55,.62),blue);add('cube',(x,0,.32),(.52,.61,.14),gold)
 for k in range(3):add('cube',(x-.14+k*.14,-.12,-.42),(.12,.33,.33),cloth)
group('Hands')
add('cone',(0,0,0),(1.3,.62,1.2),blue);add('cube',(0,-.27,.1),(.2,.15,.9),gold)
for x in [-.5,.5]:add('sphere',(x,0,.5),(.5,.6,.28),gold)
add('sphere',(0,-.35,.42),(.22,.15,.3),gem);group('Body')
for x in [-.32,.32]:
 add('cube',(x,0,.15),(.44,.48,.85),blue);add('cube',(x,-.2,-.25),(.48,.85,.32),blue);add('cube',(x,-.2,-.4),(.5,.9,.1),gold)
group('Feet')
for x,z,s in [(-.37,-.05,.8),(.2,.15,1),(.52,-.15,.65)]:add('cone',(x,0,z),(.4,.4,s),gem,(0,.15,0))
add('torus',(0,0,-.45),(1.25,1.25,.6),gold);group('Material')
scene=bpy.context.scene;scene.render.engine='CYCLES';scene.cycles.samples=24
scene.render.resolution_x=256;scene.render.resolution_y=256;scene.render.resolution_percentage=100
scene.render.image_settings.file_format='PNG';scene.render.image_settings.color_mode='RGBA';scene.render.film_transparent=True
scene.world.color=(.12,.12,.12)
bpy.ops.object.camera_add(location=(2.4,-4.6,2.5));camera=bpy.context.object;camera.rotation_euler=(Vector((0,0,0))-camera.location).to_track_quat('-Z','Y').to_euler();camera.data.type='ORTHO';camera.data.ortho_scale=2.4;scene.camera=camera
for loc,power,size in [((2,-3,4),500,4),((-3,-1,2),350,3),((0,3,2),650,2)]:
 bpy.ops.object.light_add(type='AREA',location=loc);o=bpy.context.object;o.data.energy=power;o.data.shape='DISK';o.data.size=size;o.rotation_euler=(-o.location).to_track_quat('-Z','Y').to_euler()
bpy.ops.wm.save_as_mainfile(filepath=str(root/'EquipmentIcons.blend'))
for name,objects in groups.items():
 for other,meshes in groups.items():
  for o in meshes:o.hide_render=other!=name
 scene.render.filepath=str(root/(name+'.png'));bpy.ops.render.render(write_still=True)
