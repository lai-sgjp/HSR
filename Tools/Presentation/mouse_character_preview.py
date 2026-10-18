"""Send real Windows mouse events to the task's explicit 1280x720 PIE test window.

Coordinates are from the reviewed viewport screenshot, not used by production UI.
No input is sent unless exactly one matching window with the expected size exists.
"""
import argparse
import ctypes
from ctypes import wintypes
import json
import time

parser = argparse.ArgumentParser()
parser.add_argument('action', choices=['drag', 'wheel', 'reset', 'character', 'leave', 'back', 'close'])
args = parser.parse_args()
user = ctypes.windll.user32
user.SetProcessDPIAware()
windows = []
callback_type = ctypes.WINFUNCTYPE(wintypes.BOOL, wintypes.HWND, wintypes.LPARAM)

@callback_type
def visit(hwnd, param):
    title = ctypes.create_unicode_buffer(512)
    user.GetWindowTextW(hwnd, title, 512)
    if title.value == 'HSR Presentation Verification':
        rect = wintypes.RECT()
        user.GetClientRect(hwnd, ctypes.byref(rect))
        if rect.right == 1280 and rect.bottom == 720: windows.append(hwnd)
    return True

user.EnumWindows(visit, 0)
assert len(windows) == 1, 'Expected exactly one task PIE window: ' + str(len(windows))
hwnd = windows[0]
user.ShowWindow(hwnd, 9)
user.SetForegroundWindow(hwnd)
time.sleep(.15)
origin = wintypes.POINT(0, 0)
user.ClientToScreen(hwnd, ctypes.byref(origin))
old = wintypes.POINT()
user.GetCursorPos(ctypes.byref(old))

def move(x, y):
    user.SetCursorPos(origin.x + x, origin.y + y)
    time.sleep(.04)

def click(x, y):
    move(x, y)
    user.mouse_event(2, 0, 0, 0, 0)
    time.sleep(.06)
    user.mouse_event(4, 0, 0, 0, 0)

try:
    if args.action in ['drag', 'leave']:
        move(410, 400)
        user.mouse_event(2, 0, 0, 0, 0)
        for x in range(420, 551, 10): move(x, 400)
        if args.action == 'leave':
            move(900, 400)
            move(410, 400)
        user.mouse_event(4, 0, 0, 0, 0)
    elif args.action == 'wheel':
        move(430, 400)
        user.mouse_event(0x0800, 0, 0, 240, 0)
    elif args.action == 'reset': click(430, 684)
    elif args.action == 'character': click(115, 140)
    elif args.action in ['back', 'close']:
        key = 0x1b if args.action == 'back' else 0x58
        user.keybd_event(key, 0, 0, 0)
        user.keybd_event(key, 0, 2, 0)
finally:
    user.SetCursorPos(old.x, old.y)
print(json.dumps({'success': True, 'action': args.action, 'method': 'Win32 actual mouse/key events', 'window': int(hwnd)}))
