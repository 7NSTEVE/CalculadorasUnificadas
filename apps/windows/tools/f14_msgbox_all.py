import json
import sys

path = sys.argv[1] if len(sys.argv) > 1 else r'verification\2026-10-16-f1-dispatcher\frida-diag-manager4.jsonl'
n = 0
for line in open(path, encoding='utf-8'):
    r = json.loads(line)
    p = r.get('message', {})
    payload = p.get('payload', {})
    if p.get('type') == 'error':
        print('SCRIPT ERROR:', json.dumps(p)[:500])
    if payload.get('event') == 'msgbox':
        n += 1
        print(f'--- msgbox #{n} (seq {r["seq"]}) keys: {sorted(payload.keys())}')
        print('   caller:', payload.get('caller'))
        stack = payload.get('stack')
        rawstack = payload.get('rawstack')
        print('   stack:', stack if stack is not None else '<ausente>')
        if rawstack:
            for f in rawstack[:24]:
                print('     ', f)
