"""Run an explicit local script through this project's existing UnrealMCPython bridge."""
import argparse
import json
from pathlib import Path
import socket


def execute(code, timeout=240):
    with socket.create_connection(('127.0.0.1', 12029), timeout=timeout) as connection:
        connection.sendall(json.dumps({'type': 'python', 'code': code}, ensure_ascii=False).encode('utf-8'))
        chunks = []
        while chunk := connection.recv(65536):
            chunks.append(chunk)
    return json.loads(b''.join(chunks).decode('utf-8'))


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('script', type=Path)
    parser.add_argument('--timeout', type=int, default=240)
    args = parser.parse_args()
    # Each script owns its callback globals. Shared 'handle'/'state' names otherwise
    # let an earlier PIE callback unregister a later verification callback.
    source = args.script.read_text(encoding='utf-8-sig')
    response = execute(f"exec(compile({source!r}, {str(args.script)!r}, 'exec'), {{'__name__': '__main__'}})", args.timeout)
    print(json.dumps(response, ensure_ascii=False, indent=2))
    if not response.get('success', False):
        raise SystemExit(1)
