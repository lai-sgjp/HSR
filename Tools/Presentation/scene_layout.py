"""Deterministic authored layouts. All positions and dimensions are in metres."""
import math

def layout(kind):
    items=[]
    def mesh(name,x,y,z=0,yaw=0,scale=(1,1,1)):
        items.append(dict(mesh=name,p=(x,y,z),yaw=yaw,scale=scale))
    def box(name,p,size,mat='Stone_Blue',yaw=0):
        items.append(dict(mesh='Cube',name=name,p=p,scale=size,mat=mat,yaw=yaw))
    def disc(name,x,y,z,diameter,height,mat):
        items.append(dict(mesh='Cylinder',name=name,p=(x,y,z),scale=(diameter,diameter,height),mat=mat,yaw=0))
    def lamp(x,y,z=0): mesh('StreetLamp',x,y,z)
    def facade(x,y,yaw=0,floors=2):
        for floor in range(floors): mesh('Facade',x,y,floor*8,yaw+180)
    def railing(x,y,z=0,yaw=0): mesh('Railing',x,y,z,yaw)
    if kind=='city':
        box('CityFoundation',(0,0,-.6),(250,250,1.2),'Paving')
        for sign in [-1,1]:
            box('BoundaryWall',(sign*124,0,3),(1,250,6))
            box('BoundaryWall',(0,sign*124,3),(250,1,6))
        # Paved axial streets and contrasting pedestrian borders.
        box('NorthSouthAvenue',(0,0,.015),(12,248,.03),'Stone_Blue')
        box('EastWestAvenue',(0,0,.02),(248,12,.04),'Stone_Blue')
        for t in [-7,7]:
            box('AvenueTrim',(t,0,.05),(.08,248,.05),'Metal_Brass')
            box('AvenueTrim',(0,t,.05),(248,.08,.05),'Metal_Brass')
        disc('PlazaOuter',0,0,.1,55,.2,'Stone_Ivory')
        disc('PlazaBrass',0,0,.22,53,.05,'Metal_Brass')
        disc('PlazaInner',0,0,.26,52.5,.06,'Paving')
        disc('FountainBase',0,0,.4,9,.8,'Stone_Ivory')
        disc('FountainRim',0,0,.85,8.5,.2,'Metal_Brass')
        disc('FountainWater',0,0,.97,7.8,.05,'Water')
        mesh('Column',0,0,1,scale=(.7,.7,.85))
        disc('MonumentHalo',0,0,7.4,3.2,.15,'Glow_Cyan')
        # Architectural blocks leave a navigable cross, courtyards and an outer loop.
        for signx in [-1,1]:
            for signy in [-1,1]:
                cx,cy=signx*60,signy*57
                # Keep the eastern ramp clear all the way to its upper landing.
                if signx>0 and signy>0: cx=83
                box('CityBlock',(cx,cy,8),(38,32,16))
                for dx in [-15,-9,-3,3,9,15]:
                    facade(cx+dx,cy-signy*16,0 if signy>0 else 180)
                for dy in [-12,-6,0,6,12]: facade(cx-signx*19,cy+dy,90 if signx<0 else -90)
                # Roof parapets, utilities and raised central roof silhouette.
                box('RoofCornice',(cx,cy,16.2),(39,33,.4),'Stone_Ivory')
                box('RoofPavilion',(cx+6,cy+4,19),(14,15,6))
                for t in [-1,1]: box('RoofFin',(cx+t*18,cy,18),(.6,32,4),'Metal_Brass')
        for x in range(-114,115,6):
            facade(x,120,0,3); facade(x,-120,180,2)
        for y in range(-108,109,6):
            facade(120,y,-90,2); facade(-120,y,90,3)
        # Upper terrace, wide stairs and a walkable ramp with a 5 metre rise.
        box('Terrace',(0,83,2.5),(92,28,5),'Stone_Blue')
        box('TerraceSurface',(0,83,5.05),(92,28,.1),'Stone_Ivory')
        for x in [-38,38]:
            for step in range(30):
                h=(step+1)/6
                box('TerraceStair',(x,39+step, h/2),(7,1.02,h),'Stone_Ivory')
        for x in range(-42,43,3):
            if abs(abs(x)-38)>5: railing(x,69,5)
            railing(x,97,5)
        for y in range(0,70):
            h=(y+1)*5/70
            box('AccessibleRamp',(50,y,h/2),(7,1.02,h),'Paving')
        box('RampLanding',(48,72,2.5),(11,8,5),'Paving')
        for x in [-30,0,30]:
            mesh('Planter',x,88,5); mesh('Bench',x,84,5); lamp(x+3,84,5)
        mesh('Arch',0,-103,0,scale=(1.3,1.3,1.3))
        for angle in [22.5+i*45 for i in range(8)]:
            a=math.radians(angle); x,y=22*math.cos(a),22*math.sin(a)
            mesh('Bench',x,y,0,angle-90)
            mesh('Planter',x*1.12,y*1.12,0,angle)
            mesh('Palm',x*1.18,y*1.18)
            lamp(x*1.27,y*1.27)
        for s in [-1,1]:
            for p in range(-100,101,20):
                if abs(p)<32: continue
                lamp(s*10,p); lamp(p,s*10)
                mesh('Bin',s*10+1,p)
            for p in [-86,-30,30,86]: mesh('Hydrant',s*13,p)
        # Sightline markers and unambiguous interaction destinations.
        for x,y in [(-91,-70),(-92,65),(88,85)]:
            disc('EncounterRing',x,y,.09,9,.08,'Metal_Brass')
            disc('EncounterCentre',x,y,.14,8.6,.03,'Paving')
        return items
    if kind=='hub':
        box('HubFoundation',(0,0,-.5),(70,25,1),'Paving')
        box('ObservationCarFloor',(5,3,.12),(54,16,.24),'Wood_Walnut')
        for x in range(-21,34,6):
            mesh('Facade',x,11,0,180)
            if x!=-15: mesh('Facade',x,-5,0,0)
            mesh('Column',x,10,scale=(.55,.55,.78))
        box('Roof',(5,3,7.8),(55,17,.4),'Metal_Charcoal')
        for x in range(-19,33,4):
            box('CeilingLight',(x,3,7.55),(.08,14,.04),'Glow_Amber')
        for y in range(-3,10,3):
            box('EndWall',(-23,y,3.5),(.5,3,7))
            box('EndWall',(33,y,3.5),(.5,3,7))
        for x in [-12,-4,8,20]:
            mesh('Bench',x,5,.25); mesh('Bench',x,1,.25,180)
            box('LoungeTable',(x,3,.75),(2,1,.18),'Metal_Brass')
            mesh('Planter',x,9,.25)
        for x in [-28,-14,0,14,28]: lamp(x,-10)
        for x in range(-30,31,3): railing(x,-12)
        for y in range(-9,13,3):
            railing(-34,y,0,90);railing(34,y,0,90)
        for x in [-16,0,16]: mesh('Palm',x,22,scale=(1.6,1.6,1.6))
        return items
    if kind=='arena':
        disc('ArenaFoundation',0,0,-.5,65,1,'Stone_Blue')
        for d,z,mat in [(40,.05,'Stone_Ivory'),(32,.15,'Metal_Brass'),(31.5,.21,'Paving'),(22,.25,'Metal_Brass'),(21.7,.29,'Paving'),(5,.33,'Stone_Ivory')]:
            disc('ArenaInlay',0,0,z,d,.06,mat)
        for i in range(40):
            a=i*math.tau/40
            for row in range(6):
                r=21+row*1.65
                box('TieredSeating',(r*math.cos(a),r*math.sin(a),.4+row*.65),(1.7,3.7,.8+row*1.3),'Stone_Blue',math.degrees(a))
            if i%5==0:
                r=30; x,y=r*math.cos(a),r*math.sin(a)
                mesh('Column',x,y,4,math.degrees(a))
                mesh('Banner',x*.91,y*.91,4,math.degrees(a)-90)
                mesh('Brazier',x*.67,y*.67,.35)
            if i%10==0: mesh('Arch',32*math.cos(a),32*math.sin(a),4,math.degrees(a)-90)
        return items
    raise ValueError(kind)

if __name__=='__main__':
    import json
    from pathlib import Path
    output=Path(__file__).resolve().parents[2]/'ArtSource/Presentation/layouts.json'
    output.write_text(json.dumps({k:layout(k) for k in ['city','hub','arena']},indent=2),encoding='utf-8')
    print({k:len(layout(k)) for k in ['city','hub','arena']})
