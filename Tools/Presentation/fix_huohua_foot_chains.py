"""Run through the HSR Editor bridge after retargeting Huohua animations.

Repairs the ordinary MMD leg chain that still skins the boot toes while the
retargeter animates its parallel D/EX chain. Edits only eight animation tracks
on sequences reachable from Huohua's CURRENT definition/AnimBP. Keeps original
mesh, Skeleton, toe pivots, D motion, timing, curves and notifies. Original local
assets are backed up once under Saved/HuohuaFeet/BeforeAssets; no redistributable
character assets are added to Git.
"""
import hashlib
import json
from pathlib import Path
import shutil
import unreal

ROOT = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
OUT = ROOT / 'Saved/HuohuaFeet'
OUT.mkdir(parents=True, exist_ok=True)
LIB = unreal.EditorAssetLibrary
assert not unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world(), 'Stop PIE before editing animation assets'
DEFINITION = unreal.load_asset('/Game/Data/VerticalSlice/Characters/DA_Character_Huohua')
MESH = DEFINITION.get_editor_property('character_mesh')
SKELETON = MESH.get_editor_property('skeleton')
PAIRS = [(side + part, side + part + 'D', False) for side in ['左', '右'] for part in ['足', 'ひざ', '足首']]
PAIRS += [(side + 'つま先', side + '足先EX', True) for side in ['左', '右']]
TARGETS = {dst.casefold() for dst, _, _ in PAIRS}
OPTIONS = unreal.AnimPoseEvaluationOptions()
OPTIONS.evaluation_type = unreal.AnimDataEvalType.SOURCE
OPTIONS.optional_skeletal_mesh = MESH


def file_path(package):
    return ROOT / 'Content' / Path(package.removeprefix('/Game/')).with_suffix('.uasset')


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def package_of(obj):
    return obj.get_path_name().split('.')[0]


# Traverse only the current animation graph; do not edit historical LIVE/PR clips.
roots = [package_of(DEFINITION.get_editor_property('animation_class'))]
for field in ['preview_idle_animation', 'attack_animation', 'skill_animation', 'hit_animation', 'defeat_animation']:
    asset = DEFINITION.get_editor_property(field)
    if asset:
        roots.append(package_of(asset))
registry = unreal.AssetRegistryHelpers.get_asset_registry()
options = unreal.AssetRegistryDependencyOptions(include_hard_package_references=True, include_soft_package_references=True)
seen, pending, animations = set(), list(roots), []
while pending:
    package = pending.pop()
    if package in seen or not package.startswith('/Game/Presentation/Animation/') or 'Huohua' not in package:
        continue
    seen.add(package)
    asset = unreal.load_asset(package)
    if isinstance(asset, unreal.AnimSequence) and asset.get_editor_property('skeleton') == SKELETON:
        animations.append(asset)
    else:
        pending.extend(map(str, registry.get_dependencies(package, options)))
assert animations, 'No current Huohua animation sequences found'
protected = {file_path(package_of(obj)): digest(file_path(package_of(obj))) for obj in [MESH, SKELETON, DEFINITION]}
report = {'definition': package_of(DEFINITION), 'roots': roots, 'assets': [], 'method': 'Bake parallel D-chain motion into ordinary weighted leg/toe tracks; preserve ordinary toe translation'}


def ankle_gap(pose):
    return max((pose.get_bone_pose(side + '足首', unreal.AnimPoseSpaces.WORLD).translation -
                pose.get_bone_pose(side + '足首D', unreal.AnimPoseSpaces.WORLD).translation).length() for side in ['左', '右'])


def repair(animation):
    path = package_of(animation)
    names = set(map(lambda x: str(x).casefold(), animation.data_model_interface.get_bone_track_names()))
    assert all(dst.casefold() in names and src.casefold() in names for dst, src, _ in PAIRS), path + ': incomplete paired tracks'
    first = animation.get_anim_pose_at_time(0, OPTIONS)
    for dst, src, toe in PAIRS:
        target, driver = first.get_ref_bone_pose(dst), first.get_ref_bone_pose(src)
        assert target.rotation.angular_distance(driver.rotation) < .0001, path + ': bind axes differ'
        if not toe:
            assert (target.translation - driver.translation).length() < .001, path + ': bind pivots differ'
    model = animation.data_model_interface
    rate = model.get_frame_rate()
    fps = rate.numerator / rate.denominator
    count = model.get_number_of_frames() + 1
    assert count > 1 and 1 <= fps <= 240, path + ': invalid timeline'
    sample_times = [animation.sequence_length * f for f in [0, .25, .5, .75, 1]]
    samples = [animation.get_anim_pose_at_time(t, OPTIONS) for t in sample_times]
    gap_before = max(map(ankle_gap, samples))
    toe_before = max(p.get_bone_pose(dst).rotation.angular_distance(p.get_bone_pose(src).rotation)
                     for p in samples for dst, src, toe in PAIRS if toe)
    # A sparse phase check cannot exclude a short toe-only error between phases.
    if gap_before < .001 and toe_before < .0001:
        for half_frame in range((count - 1) * 2 + 1):
            pose = animation.get_anim_pose_at_time(min(half_frame / (2 * fps), animation.sequence_length), OPTIONS)
            gap_before = max(gap_before, ankle_gap(pose))
            toe_before = max(toe_before, *(pose.get_bone_pose(dst).rotation.angular_distance(pose.get_bone_pose(src).rotation)
                                          for dst, src, toe in PAIRS if toe))
    row = {'asset': path, 'frames': count, 'before_sampled_gap_cm': gap_before}
    if gap_before < .001 and toe_before < .0001:
        row['result'] = 'already_consistent'
        report['assets'].append(row)
        return
    source = file_path(path)
    backup = OUT / 'BeforeAssets' / source.relative_to(ROOT / 'Content')
    backup.parent.mkdir(parents=True, exist_ok=True)
    if not backup.exists():
        shutil.copy2(source, backup)
    row.update(backup=str(backup), original_sha256=digest(backup), before_sha256=digest(source))
    keys = {dst: [[], [], []] for dst, _, _ in PAIRS}
    for frame in range(count):
        pose = animation.get_anim_pose_at_time(min(frame / fps, animation.sequence_length), OPTIONS)
        for dst, src, toe in PAIRS:
            target, driver = pose.get_bone_pose(dst), pose.get_bone_pose(src)
            positions, rotations, scales = keys[dst]
            positions.append(target.translation if toe else driver.translation)
            rotations.append(driver.rotation)
            scales.append(target.scale3d if toe else driver.scale3d)
    controller = animation.controller
    controller.open_bracket('Repair Huohua weighted MMD foot chains', False)
    try:
        for dst, _, _ in PAIRS:
            assert controller.set_bone_track_keys(dst, *keys[dst], False), path + ': failed ' + dst
    finally:
        controller.close_bracket(False)
    maximum = 0.0
    for frame in range(count):
        pose = animation.get_anim_pose_at_time(min(frame / fps, animation.sequence_length), OPTIONS)
        maximum = max(maximum, ankle_gap(pose))
    assert maximum < .01, (path, maximum)
    # Verify all other local transforms at representative phases remain intact.
    maximum_other_position, maximum_other_rotation = 0.0, 0.0
    for t, before in zip(sample_times, samples):
        after = animation.get_anim_pose_at_time(t, OPTIONS)
        for bone in before.get_bone_names():
            if str(bone).casefold() in TARGETS:
                continue
            old, new = before.get_bone_pose(bone), after.get_bone_pose(bone)
            maximum_other_position = max(maximum_other_position, (old.translation - new.translation).length())
            maximum_other_rotation = max(maximum_other_rotation, old.rotation.angular_distance(new.rotation))
    assert maximum_other_position < .001 and maximum_other_rotation < .0001, path + ': unrelated motion changed'
    updated_rate = model.get_frame_rate()
    assert model.get_number_of_frames() + 1 == count and updated_rate.numerator == rate.numerator and updated_rate.denominator == rate.denominator
    assert LIB.save_loaded_asset(animation, False), path + ': save failed'
    row.update(result='repaired', modified_tracks=[dst for dst, _, _ in PAIRS], after_all_keys_gap_cm=maximum,
               other_sampled_position_delta_cm=maximum_other_position, other_sampled_rotation_delta_rad=maximum_other_rotation,
               after_sha256=digest(source))
    report['assets'].append(row)
    OUT.joinpath('repair-current-animations.json').write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf8')


for animation in sorted(animations, key=lambda x: x.get_path_name()):
    repair(animation)
report['protected_unchanged'] = {str(path): digest(path) == before for path, before in protected.items()}
assert all(report['protected_unchanged'].values())
report['success'] = True
OUT.joinpath('repair-current-animations.json').write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf8')
print(json.dumps({'success': True, 'reachable_sequences': len(animations), 'repaired': sum(x['result'] == 'repaired' for x in report['assets'])}))
