"""Preview each authored combat clip through the same DefaultSlot used by combat.
This validates pose output; actual rule settlement is covered by paced battle QA.
"""
import unreal,json
from pathlib import Path
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
pc=unreal.GameplayStatics.get_player_controller(world,0);pc.request_close_frontend_to_root()
names=['Huohua','Remiel','EvernightMoon','Verina']
fields=['attack_animation','skill_animation','hit_animation','defeat_animation']
elapsed=[0.];index=[-1];handle=[None];report=[];captured=set()
def tick(dt):
    elapsed[0]+=dt
    step=int(elapsed[0]/2)
    if step>=16:
        unreal.unregister_slate_post_tick_callback(handle[0])
        Path(unreal.Paths.project_dir(),'Saved/Presentation/motion_clips_pie.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8');return
    slot=step//4;field=fields[step%4]
    if index[0]!=step:
        index[0]=step
        pc.switch_exploration_character(slot)
        pawn=unreal.GameplayStatics.get_player_pawn(world,0)
        pawn.set_actor_location(unreal.Vector(-2000,-500,180),False,True)
        definition=unreal.load_asset('/Game/Data/VerticalSlice/Characters/DA_Character_'+names[slot])
        clip=definition.get_editor_property(field)
        if not isinstance(clip,unreal.AnimSequenceBase): clip=unreal.load_asset(str(clip))
        instance=pawn.get_editor_property('mesh').get_anim_instance()
        instance.montage_stop(0.)
        montage=instance.play_slot_animation_as_dynamic_montage(clip,'DefaultSlot',.12,.15,max(1.,clip.get_play_length()/1.2))
        if montage and field=='defeat_animation':
            montage.set_editor_property('enable_auto_blend_out',False)
            instance.montage_stop(0.)
            instance.montage_play(montage,max(1.,clip.get_play_length()/1.2))
        report.append({'character':names[slot],'field':field,'clip':clip.get_path_name(),'montage_started':bool(montage)})
    if elapsed[0]%2>(1.35 if field=='defeat_animation' else .8) and step not in captured:
        captured.add(step)
        report[-1]['capture']=unreal.MCPythonHelper.capture_play_viewport('motion_'+names[slot]+'_'+field)
handle[0]=unreal.register_slate_post_tick_callback(tick)
print(json.dumps({'success':True,'preview_seconds':32}))
