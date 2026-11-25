import json
path = r'verification\2026-10-16-f1-dispatcher\frida-diag-manager.jsonl'
for line in open(path, encoding='utf-8'):
    r = json.loads(line)
    p = r.get('message', {}).get('payload', {})
    ev = p.get('event', '')
    if ev == 'hooked':
        print(f"{r['seq']:4d} HOOK {p.get('module')}!{p.get('symbol')}")
    elif ev in ('reg', 'msgbox', 'window', 'EXIT_PROCESS', 'ACT', 'winhttp'):
        print(f"{r['seq']:4d} {ev:12s} {json.dumps(p, ensure_ascii=False)[:220]}")
    elif ev == 'hook_error':
        print(f"{r['seq']:4d} ERR  {p.get('module')}!{p.get('symbol')} :: {str(p.get('error'))[:80]}")
