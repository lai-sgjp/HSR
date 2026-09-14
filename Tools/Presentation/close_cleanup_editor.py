"""Close the isolated cleanup session without saving transient legacy packages."""
import unreal,json
print(json.dumps({'success':True,'saved_dirty_legacy_packages':False}))
unreal.SystemLibrary.quit_editor()
