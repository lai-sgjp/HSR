"""Geometric route sampling, reported separately from actual CharacterMovement walking."""
import unreal,json,math
from pathlib import Path
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
pawn=unreal.GameplayStatics.get_player_pawn(world,0)
routes={
 'avenues':[(0,-94),(0,-30),(-26,-28),(0,-30),(12,-12),(12,12),(-15,12),(-12,12),(-12,0),(-95,0),(-95,-70)],
 'west_stairs':[(-38,30),(-38,72),(-10,76),(18,85)],
 'east_stairs':[(38,30),(38,72),(18,85)],
 'ramp':[(50,-4),(50,72),(42,74),(22,87)],
 'courtyard':[(-95,-70),(-95,24),(-84,24),(-95,24),(-95,65)],
 'east_loop':[(88,-30),(88,26),(110,26),(110,85),(88,85),(110,100),(0,106),(-95,100),(-95,65)]}
out={}
for name,points in routes.items():
 failures=[];samples=[]
 for first,last in zip(points,points[1:]):
  count=max(1,math.ceil(math.dist(first,last)))
  for i in range(count+1):
   x=(first[0]+(last[0]-first[0])*i/count)*100;y=(first[1]+(last[1]-first[1])*i/count)*100
   hit=unreal.SystemLibrary.line_trace_single(world,unreal.Vector(x,y,3000),unreal.Vector(x,y,-200),unreal.TraceTypeQuery.TRACE_TYPE_QUERY1,False,[pawn],unreal.DrawDebugTrace.NONE)
   if not hit:failures.append({'x':x,'y':y,'reason':'no ground'});continue
   location=hit.to_tuple()[5]
   z=location.z
   if z>600:failures.append({'x':x,'y':y,'z':z,'reason':'route hits tall geometry'});continue
   block=unreal.SystemLibrary.capsule_trace_single(world,unreal.Vector(x,y,z+100),unreal.Vector(x,y,z+101),34,88,unreal.TraceTypeQuery.TRACE_TYPE_QUERY1,False,[pawn],unreal.DrawDebugTrace.NONE)
   if block:failures.append({'x':x,'y':y,'z':z,'reason':'capsule blocked'})
   if samples and abs(z-samples[-1][2])>45:failures.append({'x':x,'y':y,'z':z,'reason':'height discontinuity','previous_z':samples[-1][2]})
   samples.append([x,y,z])
 out[name]={'samples':len(samples),'failures':failures}
Path(unreal.Paths.project_dir(),'Saved/Presentation/route_clearance.json').write_text(json.dumps(out,indent=2),encoding='utf-8')
print(json.dumps({'success':True,'routes':{k:{'samples':v['samples'],'failures':len(v['failures'])} for k,v in out.items()}}))
