import ctypes,json,sys
from ctypes import wintypes
from pathlib import Path
pid=int(sys.argv[1]);rows=[]
user=ctypes.windll.user32
callback=ctypes.WINFUNCTYPE(wintypes.BOOL,wintypes.HWND,wintypes.LPARAM)
@callback
def visit(hwnd,param):
 owner=wintypes.DWORD();user.GetWindowThreadProcessId(hwnd,ctypes.byref(owner))
 if owner.value==pid:
  rect=wintypes.RECT();user.GetClientRect(hwnd,ctypes.byref(rect))
  if rect.right>100 and rect.bottom>100:
   rows.append({'pid':pid,'client_width':rect.right,'client_height':rect.bottom,'visible':bool(user.IsWindowVisible(hwnd))})
   if '--close' in sys.argv or '--close-only' in sys.argv:user.PostMessageW(hwnd,0x0010,0,0)
 return True
user.EnumWindows(visit,0)
if '--close-only' not in sys.argv:
 Path(__file__).resolve().parents[2].joinpath('Saved/Presentation/Performance/window_dimensions.json').write_text(json.dumps(rows,indent=2),encoding='utf-8')
print(json.dumps(rows))
