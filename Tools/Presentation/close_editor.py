import json
import unreal
unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True,True)
print(json.dumps({'success':True,'saved':True}))
unreal.SystemLibrary.quit_editor()
