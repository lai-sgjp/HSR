"""Execute preview scripts without ExecuteFile misreading embedded docstrings as paths."""
import argparse
import base64
import json
from pathlib import Path
from editor_bridge import execute

parser = argparse.ArgumentParser()
parser.add_argument('script', type=Path)
parser.add_argument('--timeout', type=int, default=240)
args = parser.parse_args()
payload = base64.b64encode(args.script.read_bytes()).decode('ascii')
response = execute("import base64\nexec(compile(base64.b64decode('" + payload
                   + "'), '<character_preview>', 'exec'), {'__name__': '__main__'})", args.timeout)
print(json.dumps(response, ensure_ascii=False, indent=2))
if not response.get('success', False):
    raise SystemExit(1)
