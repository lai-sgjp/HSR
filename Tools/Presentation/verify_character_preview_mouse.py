"""External driver: actual OS input -> UMG -> session state, with assertions."""
import base64
import json
from pathlib import Path
import subprocess
import sys
import time
from editor_bridge import execute

folder = Path(__file__).resolve().parent
root = folder.parents[1]
out = root / 'Saved/CharacterPreview'
probe = (folder / 'inspect_character_preview_input.py').read_bytes()
report = []

def code(source):
    payload = base64.b64encode(source).decode('ascii')
    response = execute("import base64\nexec(compile(base64.b64decode('" + payload + "'), '<mouse_probe>', 'exec'), {})")
    assert response.get('success'), response
    return json.loads(response['result'])

def inspect():
    result = code(probe)
    if result.get('opened'):
        time.sleep(.5)
        result = code(probe)
    return result

def action(name):
    subprocess.run([sys.executable, str(folder / 'mouse_character_preview.py'), name], check=True)
    time.sleep(.15)

def record(name, passed, **facts):
    report.append({'name': name, 'pass': bool(passed), **facts})
    (out / 'mouse_matrix.json').write_text(json.dumps(report, indent=2), encoding='utf8')
    assert passed, name

inspect()
code(b"import json,unreal\nw=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()\ns=next(x for x in unreal.WidgetLibrary.get_all_widgets_of_class(w,unreal.HSRCharacterShellWidget,False) if x.get_character_preview())\ns.select_character('Demo.Character.Huohua')\nprint(json.dumps({'success':True}))")
time.sleep(.6)
before = inspect()
action('drag')
dragged = inspect()
record('real left drag reaches preview', abs(dragged['yaw']-before['yaw']) > 1, before=before, after=dragged)
action('wheel')
zoomed = inspect()
record('real wheel reaches preview', zoomed['distance'] < dragged['distance'], before=dragged, after=zoomed)
action('reset')
reset = inspect()
record('real reset button', abs(reset['yaw']-before['yaw']) < .01 and abs(reset['distance']-before['distance']) < .01, after=reset)
action('character')
selected = inspect()
deadline = time.monotonic() + 3
while selected['character'] != 'Demo.Character.EvernightMoon' and time.monotonic() < deadline:
    time.sleep(.2)
    selected = inspect()
record('real roster click switches independent mesh', selected['character'] == 'Demo.Character.EvernightMoon', after=selected)
action('leave')
left = inspect()
record('drag leaves region safely', left['state'] == '<HSRCharacterPreviewState.READY: 2>', after=left)
action('back')
closed = code(b"import json,unreal\nw=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()\nprint(json.dumps({'success':True,'stages':len(unreal.GameplayStatics.get_all_actors_of_class(w,unreal.HSRCharacterPreviewStage)),'paused':unreal.GameplayStatics.is_game_paused(w)}))")
record('real Escape closes character page', closed['stages'] == 0 and closed['paused'], after=closed)
action('close')
closed = code(b"import json,unreal\nw=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()\nprint(json.dumps({'success':True,'stages':len(unreal.GameplayStatics.get_all_actors_of_class(w,unreal.HSRCharacterPreviewStage)),'paused':unreal.GameplayStatics.is_game_paused(w)}))")
record('real X returns to world input/pause policy', closed['stages'] == 0 and not closed['paused'], after=closed)
print(json.dumps({'success': True, 'checks': len(report)}))
