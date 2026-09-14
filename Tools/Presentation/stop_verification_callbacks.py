"""Stop only callbacks registered by the scoped presentation QA scripts."""
import gc,types,unreal,json
count=0
for obj in gc.get_objects():
    if isinstance(obj,types.FunctionType) and 'Tools' in obj.__code__.co_filename and 'verify_' in obj.__code__.co_filename:
        for key in ('handle','battle_handle'):
            value=obj.__globals__.get(key)
            if isinstance(value,list) and len(value)==1 and value[0] is not None:
                try: unreal.unregister_slate_post_tick_callback(value[0]);count+=1
                except Exception: pass
                value[0]=None
print(json.dumps({'success':True,'stopped':count}))
