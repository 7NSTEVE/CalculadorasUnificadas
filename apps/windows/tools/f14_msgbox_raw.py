import json
import sys

path = sys.argv[1] if len(sys.argv) > 1 else r'verification\2026-10-16-f1-dispatcher\frida-diag-manager4.jsonl'
for line in open(path, encoding='utf-8'):
    r = json.loads(line)
    p = r.get('message', {})
    if p.get('type') == 'error':
        print('SCRIPT ERROR:', json.dumps(p)[:400])
    payload = p.get('payload', {})
    if payload.get('event') == 'msgbox':
        print('MSGBOX RAW EVENT:')
        print(json.dumps(payload, indent=1, ensure_ascii=False)[:3000])
        break
