import json
errs = {}
hooks = {}
for line in open(r'verification\2026-10-16-f1-dispatcher\frida-oficial-traza.jsonl', encoding='utf-8'):
    r = json.loads(line)
    p = r.get('message', {}).get('payload', {})
    if p.get('event') == 'hook_error':
        key = str(p.get('where')) + ' :: ' + str(p.get('error'))[:130]
        errs[key] = errs.get(key, 0) + 1
    if p.get('event') == 'hooked':
        hooks[p.get('module', '?') + '!' + p.get('symbol', '?')] = 1
print('ERRORES:')
for k, v in errs.items():
    print(' ', v, k)
print('HOOKS OK:', len(hooks))
for k in list(hooks)[:30]:
    print(' ', k)
