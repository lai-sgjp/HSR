"""PID-verified Windows input and client screenshots for the explicitly launched UE practice."""
import argparse
import ctypes
from ctypes import wintypes
import json
from pathlib import Path
import time
import psutil
from PIL import ImageGrab

parser = argparse.ArgumentParser()
parser.add_argument('pid', type=int)
parser.add_argument('action', choices=['inspect', 'capture', 'key', 'click', 'close', 'record'])
parser.add_argument('--keys', default='')
parser.add_argument('--point', type=int, nargs=2)
parser.add_argument('--name', default='capture')
parser.add_argument('--seconds', type=float, default=5)
parser.add_argument('--schedule', type=Path)
args = parser.parse_args()
process = psutil.Process(args.pid)
assert process.name() in ['UnrealEditor.exe', 'HSR.exe', 'HSR-Win64-Shipping.exe']
assert any('-HSRUltimatePractice' == a for a in process.cmdline()), 'Refuse unrelated process'
user = ctypes.windll.user32
kernel = ctypes.windll.kernel32
user.SetProcessDPIAware()
windows = []
callback_type = ctypes.WINFUNCTYPE(wintypes.BOOL, wintypes.HWND, wintypes.LPARAM)
@callback_type
def visit(hwnd, unused):
    owner = wintypes.DWORD()
    user.GetWindowThreadProcessId(hwnd, ctypes.byref(owner))
    if owner.value == args.pid and user.IsWindowVisible(hwnd):
        rect = wintypes.RECT(); user.GetClientRect(hwnd, ctypes.byref(rect))
        if rect.right >= 640 and rect.bottom >= 360:
            title = ctypes.create_unicode_buffer(512); user.GetWindowTextW(hwnd, title, 512)
            windows.append((hwnd, rect.right, rect.bottom, title.value))
    return True
user.EnumWindows(visit, 0)
assert len(windows) == 1, repr(windows)
hwnd, width, height, title = windows[0]
print(json.dumps({'pid': args.pid, 'hwnd': hwnd, 'size': [width, height], 'title': title}))
if args.action == 'inspect': raise SystemExit(0)
if args.action == 'close': user.PostMessageW(hwnd, 0x0010, 0, 0); raise SystemExit(0)
user.ShowWindow(hwnd, 9)
user.SetWindowPos(hwnd, 0, 30, 30, 0, 0, 0x0001 | 0x0040)
user.SetForegroundWindow(hwnd)
if user.GetForegroundWindow() != hwnd:
    foreground_thread = user.GetWindowThreadProcessId(user.GetForegroundWindow(), None)
    current_thread = kernel.GetCurrentThreadId()
    attached = user.AttachThreadInput(current_thread, foreground_thread, True)
    try:
        user.BringWindowToTop(hwnd); user.SetForegroundWindow(hwnd); user.SetFocus(hwnd)
    finally:
        if attached: user.AttachThreadInput(current_thread, foreground_thread, False)
time.sleep(.15)
assert user.GetForegroundWindow() == hwnd, 'No foreground ownership; refuse global input'
origin = wintypes.POINT(); user.ClientToScreen(hwnd, ctypes.byref(origin))
output = Path(__file__).resolve().parents[2] / 'Saved/Ultimate/Runtime'
output.mkdir(parents=True, exist_ok=True)
def capture(name):
    image = ImageGrab.grab(bbox=(origin.x, origin.y, origin.x + width, origin.y + height), all_screens=True)
    path = output / (name + '.png'); image.save(path); return str(path)
def press(key):
    keys = {'enter': 0x0D, 'escape': 0x1B, 'left': 0x25, 'right': 0x27, 'tab': 0x09}
    value = keys.get(key.lower(), ord(key.upper()) if len(key) == 1 else None)
    assert value is not None, key
    user.keybd_event(value, 0, 0, 0); time.sleep(.025); user.keybd_event(value, 0, 0x0002, 0)
def click(point):
    assert point and 0 <= point[0] < width and 0 <= point[1] < height
    user.SetCursorPos(origin.x + point[0], origin.y + point[1])
    user.mouse_event(0x0002, 0, 0, 0, 0); time.sleep(.04); user.mouse_event(0x0004, 0, 0, 0, 0)
if args.action == 'capture': print(capture(args.name))
if args.action == 'click':
    click(args.point)
if args.action == 'key':
    for key in args.keys.split(','):
        press(key); time.sleep(.045)
if args.action == 'record':
    frames = []
	# Per-frame capture times preserve actual playback timing, including screenshot overhead.
    frame_times = []
    schedule = json.loads(args.schedule.read_text(encoding='utf-8-sig')) if args.schedule else []
    events = []
    index = 0
    started = time.monotonic()
    while time.monotonic() - started < args.seconds:
        assert user.GetForegroundWindow() == hwnd, 'Recording lost foreground'
        elapsed = time.monotonic() - started
        while index < len(schedule) and schedule[index]['at'] <= elapsed:
            event = schedule[index]
            if 'key' in event: press(event['key'])
            if 'click' in event: click(event['click'])
            if 'capture' in event: capture(event['capture'])
            events.append({'actual_seconds': time.monotonic()-started, **event})
            index += 1
        frames.append(ImageGrab.grab(bbox=(origin.x, origin.y, origin.x + width, origin.y + height), all_screens=True).resize((960,540)))
        frame_times.append(time.monotonic()-started)
        time.sleep(.12)
    if frames:
        capture_seconds = time.monotonic()-started
        durations = [max(20, round((frame_times[i+1]-frame_times[i])*1000)) for i in range(len(frame_times)-1)] + [140]
        frames[0].save(output / (args.name + '.gif'), save_all=True, append_images=frames[1:], duration=durations, loop=0)
        (output/(args.name+'-inputs.json')).write_text(json.dumps({'pid':args.pid,'title':title,'client_size':[width,height],'physical_windows_input':True,'capture_seconds':capture_seconds,'frame_times':frame_times,'events':events},ensure_ascii=False,indent=2),encoding='utf-8')
        print(json.dumps({'frames': len(frames), 'capture_seconds': capture_seconds, 'path': str(output/(args.name+'.gif'))}))
