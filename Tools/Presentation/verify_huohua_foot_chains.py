"""Editor QA for repaired Huohua foot chains, including runtime compression."""
import hashlib
import json
from pathlib import Path
import unreal
root=Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
out=root/'Saved/HuohuaFeet'
repair=json.loads((out/'repair-current-animations.json').read_text(encoding='utf8'))
report={'success':True,'assets':[],'method':'All keyframes and half-key intervals in Source and Compressed pose evaluation; chain agreement is a continuity check, not complete art acceptance'}
for item in repair['assets']:
 animation=unreal.load_asset(item['asset'])
 model=animation.data_model_interface;rate=model.get_frame_rate();fps=rate.numerator/rate.denominator
 row={'asset':item['asset'],'evaluations':0,'max_source_gap_cm':0.0,'max_compressed_gap_cm':0.0,'max_compressed_ankle_angle_rad':0.0,'max_toe_rotation_delta_rad':0.0,'max_toe_pivot_delta_cm':0.0}
 for kind,field in [(unreal.AnimDataEvalType.SOURCE,'max_source_gap_cm'),(unreal.AnimDataEvalType.COMPRESSED,'max_compressed_gap_cm')]:
  options=unreal.AnimPoseEvaluationOptions();options.evaluation_type=kind
  for frame in range(model.get_number_of_frames()*2+1):
   pose=animation.get_anim_pose_at_time(min(frame/(2*fps),animation.sequence_length),options);row['evaluations']+=1
   for side in ['左','右']:
    foot=pose.get_bone_pose(side+'足首',unreal.AnimPoseSpaces.WORLD);driver=pose.get_bone_pose(side+'足首D',unreal.AnimPoseSpaces.WORLD)
    row[field]=max(row[field],(foot.translation-driver.translation).length())
    if kind==unreal.AnimDataEvalType.COMPRESSED:row['max_compressed_ankle_angle_rad']=max(row['max_compressed_ankle_angle_rad'],foot.rotation.angular_distance(driver.rotation))
    toe=pose.get_bone_pose(side+'つま先');toe_driver=pose.get_bone_pose(side+'足先EX')
    row['max_toe_rotation_delta_rad']=max(row['max_toe_rotation_delta_rad'],toe.rotation.angular_distance(toe_driver.rotation))
    row['max_toe_pivot_delta_cm']=max(row['max_toe_pivot_delta_cm'],(toe.translation-pose.get_ref_bone_pose(side+'つま先').translation).length())
 row['pass']=row['max_source_gap_cm']<.01 and row['max_compressed_gap_cm']<.05 and row['max_compressed_ankle_angle_rad']<.01 and row['max_toe_rotation_delta_rad']<.01 and row['max_toe_pivot_delta_cm']<.05
 report['success'] &= row['pass'];report['assets'].append(row)
(out/'compressed-verification.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf8')
assert report['success'],[x for x in report['assets'] if not x['pass']]
print(json.dumps({'success':True,'assets':len(report['assets']),'evaluations':sum(x['evaluations'] for x in report['assets']),'max_compressed_cm':max(x['max_compressed_gap_cm'] for x in report['assets'])}))
