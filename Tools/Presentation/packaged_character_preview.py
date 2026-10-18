"""PID-scoped real Windows input/screenshots for this task's local Development package."""
import argparse
import ctypes
from ctypes import wintypes
import json
from pathlib import Path
import time

parser = argparse.ArgumentParser()
parser.add_argument('pid', type=int)
parser.add_argument('action', choices=['inspect', 'capture', 'key', 'click', 'drag', 'wheel', 'console', 'close'])
parser.add_argument('--key', choices=['tab', 'escape', 'x', 'tilde'])
parser.add_argument('--point', nargs=2, type=int, default=[430, 400])
parser.add_argument('--command')
parser.add_argument('--name', default='packaged_capture')
parser.add_argument('--window-messages', action='store_true', help='Target only this HWND without foreground input; not physical-mouse evidence')
args = parser.parse_args()
assert not args.window_messages or args.action in ['inspect', 'key', 'console', 'close'], 'Message-only mode cannot send global mouse input'
root = Path(__file__).resolve().parents[2]
out = root / 'Saved/CharacterPreview/PackagedEvidence'
out.mkdir(parents=True, exist_ok=True)
expected = (root / 'Saved/CharacterPreview/Package/Windows/HSR/Binaries/Win64/HSR.exe').resolve()
kernel = ctypes.windll.kernel32
kernel.OpenProcess.argtypes = [wintypes.DWORD, wintypes.BOOL, wintypes.DWORD]
kernel.OpenProcess.restype = wintypes.HANDLE
kernel.QueryFullProcessImageNameW.argtypes = [wintypes.HANDLE, wintypes.DWORD, wintypes.LPWSTR, ctypes.POINTER(wintypes.DWORD)]
kernel.CloseHandle.argtypes = [wintypes.HANDLE]
process = kernel.OpenProcess(0x1000, False, args.pid)
assert process, 'Cannot inspect target PID'
try:
    buffer = ctypes.create_unicode_buffer(32768)
    length = wintypes.DWORD(len(buffer))
    assert kernel.QueryFullProcessImageNameW(process, 0, buffer, ctypes.byref(length))
    assert Path(buffer.value).resolve() == expected, 'PID does not belong to the task package'
finally:
    kernel.CloseHandle(process)

user = ctypes.windll.user32
user.SetProcessDPIAware()
windows = []
callback_type = ctypes.WINFUNCTYPE(wintypes.BOOL, wintypes.HWND, wintypes.LPARAM)

@callback_type
def visit(hwnd, param):
    owner = wintypes.DWORD()
    user.GetWindowThreadProcessId(hwnd, ctypes.byref(owner))
    if owner.value == args.pid:
        rect = wintypes.RECT()
        user.GetClientRect(hwnd, ctypes.byref(rect))
        if rect.right == 1280 and rect.bottom == 720: windows.append(hwnd)
    return True

user.EnumWindows(visit, 0)
assert len(windows) == 1, 'Expected one 1280x720 package window: ' + str(len(windows))
hwnd = windows[0]
if args.action != 'close' and not args.window_messages:
    user.ShowWindow(hwnd, 9)
    user.SetWindowPos(hwnd, 0, 40, 40, 0, 0, 0x0001 | 0x0040)
    user.SetForegroundWindow(hwnd)
    if user.GetForegroundWindow() != hwnd:
        # Windows can deny focus to a background automation process. Temporarily
        # share its input queue with the foreground thread, then detach.
        foreground_thread = user.GetWindowThreadProcessId(user.GetForegroundWindow(), None)
        current_thread = kernel.GetCurrentThreadId()
        attached = bool(user.AttachThreadInput(current_thread, foreground_thread, True))
        try:
            user.BringWindowToTop(hwnd)
            user.SetForegroundWindow(hwnd)
            user.SetFocus(hwnd)
        finally:
            if attached: user.AttachThreadInput(current_thread, foreground_thread, False)
    time.sleep(.2)
    assert user.GetForegroundWindow() == hwnd, 'Target did not gain foreground; refuse unrelated input'
origin = wintypes.POINT()
user.ClientToScreen(hwnd, ctypes.byref(origin))
old = wintypes.POINT()
user.GetCursorPos(ctypes.byref(old))

def move(x, y):
    assert 0 <= x < 1280 and 0 <= y < 720
    user.SetCursorPos(origin.x + x, origin.y + y)
    time.sleep(.04)

def key(vk):
    if args.window_messages:
        scan = user.MapVirtualKeyW(vk, 0)
        user.PostMessageW(hwnd, 0x0100, vk, 1 | (scan << 16))
        time.sleep(.04)
        user.PostMessageW(hwnd, 0x0101, vk, 1 | (scan << 16) | 0xC0000000)
    else:
        user.keybd_event(vk, 0, 0, 0)
        time.sleep(.04)
        user.keybd_event(vk, 0, 2, 0)

report = {'pid': args.pid, 'exe': str(expected), 'action': args.action, 'window': int(hwnd),
          'input_method': 'targeted_window_messages' if args.window_messages else 'foreground_OS_input',
          'client_size': [1280, 720], 'time': time.time()}
try:
    if args.action == 'key':
        assert args.key
        key({'tab': 9, 'escape': 27, 'x': 0x58, 'tilde': 0xC0}[args.key])
        report['key'] = args.key
    elif args.action == 'click':
        move(*args.point)
        user.mouse_event(2, 0, 0, 0, 0)
        time.sleep(.08)
        user.mouse_event(4, 0, 0, 0, 0)
        report['point'] = args.point
    elif args.action == 'drag':
        move(410, 400)
        user.mouse_event(2, 0, 0, 0, 0)
        for x in range(420, 551, 10): move(x, 400)
        user.mouse_event(4, 0, 0, 0, 0)
    elif args.action == 'wheel':
        move(*args.point)
        user.mouse_event(0x0800, 0, 0, 240, 0)
    elif args.action == 'capture':
        assert args.name and all(c.isalnum() or c in '_-' for c in args.name)
        assert user.GetForegroundWindow() == hwnd, 'Target is not foreground; refuse unrelated screenshot'
        from PIL import ImageGrab
        target = out / (args.name + '.png')
        ImageGrab.grab(bbox=(origin.x, origin.y, origin.x + 1280, origin.y + 720), all_screens=True).save(target)
        report['image'] = str(target)
    elif args.action == 'console':
        assert args.command and args.command.lower().startswith(('getall ', 'obj list ', 'shot ', 'csvprofile '))
        key(0xC0)
        time.sleep(.15)
        for char in args.command:
            assert 32 <= ord(char) < 127
            user.PostMessageW(hwnd, 0x0102, ord(char), 0)
            time.sleep(.003)
        key(13)
        time.sleep(.15)
        key(0xC0)
        report['command'] = args.command
    elif args.action == 'close': user.PostMessageW(hwnd, 0x0010, 0, 0)
finally:
    user.SetCursorPos(old.x, old.y)
report['success'] = True
with (out / 'actions.jsonl').open('a', encoding='utf8') as file: file.write(json.dumps(report) + '\n')
print(json.dumps(report))
