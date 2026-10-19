"""Run once with the newly built UE Editor: update the existing healer Ultimate in place.

Only DA_Skill_Verina_BreathingGarden is written. Character references, stable SkillId,
the ordinary heal, animation resources, and save/progression definitions remain intact.
"""
import json
from pathlib import Path

import unreal


ASSET_PATH = "/Game/Data/VerticalSlice/Skills/DA_Skill_Verina_BreathingGarden"


def require_asset(path):
    asset = unreal.load_asset(path)
    if not asset:
        raise RuntimeError("Missing existing Ultimate dependency: " + path)
    return asset


def main():
    skill = require_asset(ASSET_PATH)
    ability = unreal.load_class(None, "/Script/HSR.HSRUltimateAbility")
    if not ability:
        raise RuntimeError("Rebuild HSR Editor before running this authoring script")
    cost = require_asset("/Game/GameplayEffects/BP_GE_P6_UltimateEnergyCost").generated_class()
    refund = require_asset("/Game/GameplayEffects/BP_GE_P7_UltimateEnergyRefund").generated_class()
    heal = require_asset("/Game/GameplayEffects/BP_GE_P6_Heal").generated_class()
    before = {
        "category": str(skill.get_editor_property("category")),
        "ability": str(skill.get_editor_property("ability_class")),
        "skill_id": str(skill.get_editor_property("skill_id")),
    }
    properties = {
        "category": unreal.HSRSkillCategory.ULTIMATE,
        "target_type": unreal.HSRTargetType.ALL_ALLIES,
        "ability_class": ability,
        "ultimate_restores_health": True,
        "ultimate_release_threshold": 100.0,
        "display_energy_cost": 100.0,
        "cost_gameplay_effect_class": cost,
        "energy_refund_gameplay_effect_class": refund,
        "effect_gameplay_effect_class": heal,
        "skill_point_delta": 0,
        "energy_gain": 5.0,
        "description": "为全体存活队友恢复生命。消耗100点自身能量，结算后恢复5点能量。可在其他角色行动时提交，当前动作结算后按接受顺序释放；不消耗普通回合。",
    }
    skill.modify()
    for name, value in properties.items():
        skill.set_editor_property(name, value)
    if not unreal.EditorAssetLibrary.save_loaded_asset(skill, only_if_is_dirty=False):
        raise RuntimeError("Could not save existing healer Ultimate")
    if str(skill.get_editor_property("skill_id")) != before["skill_id"]:
        raise RuntimeError("Stable SkillId changed unexpectedly")
    report = {
        "success": True,
        "asset": ASSET_PATH,
        "before": before,
        "after": {name: str(skill.get_editor_property(name)) for name in properties},
        "boundary": "AllAllies healing Ultimate; one Cost and one GE per living teammate",
    }
    output = Path(unreal.Paths.project_saved_dir()) / "Ultimate" / "author_ultimate_demo.json"
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
    unreal.log("UltimateDemoAuthoring " + json.dumps(report, ensure_ascii=False))


main()
