import json
import sys

path = sys.argv[1] if len(sys.argv) > 1 else r'verification\2026-10-16-f1-dispatcher\frida-diag-manager3.jsonl'
for line in open(path, encoding='utf-8'):
    r = json.loads(line)
    p = r.get('message', {}).get('payload', {})
    if p.get('event') == 'msgbox':
        raw = p.get('rawText')
        if raw:
            b = bytes.fromhex(raw)
            print('--- msgbox rawText completo ---')
            print(repr(b))
            # decodificar como ascii/utf16
            print('ascii:', b.decode('ascii', 'replace'))
            try:
                print('utf16:', b.decode('utf-16-le', 'replace'))
            except Exception as e:
                print('utf16 err', e)
            print('flags:', p.get('flags'), 'title:', p.get('title'))
            stack = p.get('stack') or []
            print('STACK:')
            for f in stack:
                print('  ', f)
            rawstack = p.get('rawstack') or []
            print('RAWSTACK:')
            for f in rawstack:
                print('  ', f)
