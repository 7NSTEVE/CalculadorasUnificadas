import re
import glob
import os

root = r'E:\Descompilados\11_Otros\Emuladores\Calculadoras_blanca_y_gris\Calculadora Unificada\third_party\rom-native-reference'
files = []
for pat in ('*.cpp', '*.h'):
    files += glob.glob(os.path.join(root, '**', pat), recursive=True)

pat_re = re.compile(r'\(\s*(?:op|opcode|instruction)\s*&\s*0x([0-9A-Fa-f]+)u?\s*\)\s*==\s*0x([0-9A-Fa-f]+)u?')

patterns = []
for path in files:
    text = open(path, encoding='utf-8', errors='replace').read()
    for m in pat_re.finditer(text):
        mask = int(m.group(1), 16) & 0xFFFF
        value = int(m.group(2), 16) & 0xFFFF
        if (value & ~mask) != 0:
            continue
        patterns.append((mask, value, os.path.basename(path), text[:m.start()].count('\n') + 1))

print('archivos escaneados:', len(files))
print('patrones extraidos:', len(patterns))

covered = set()
for mask, value, src, line in patterns:
    free = (~mask) & 0xFFFF
    bits = [b for b in range(16) if free & (1 << b)]
    n = len(bits)
    if n > 16:
        continue
    for combo in range(1 << n):
        op = value
        for i, bitpos in enumerate(bits):
            if combo & (1 << i):
                op |= (1 << bitpos)
        covered.add(op)

total = 65536
print(f'COBERTURA ESTATICA (arbol core): {len(covered)}/{total} = {100.0*len(covered)/total:.2f}%')

blocks = {}
for op in covered:
    blocks[op >> 12] = blocks.get(op >> 12, 0) + 1
print('por bloque alto:', {f'{k:X}': v for k, v in sorted(blocks.items())})

# patrones por archivo (conteo)
byfile = {}
for mask, value, src, line in patterns:
    byfile[src] = byfile.get(src, 0) + 1
print('patrones por archivo:', dict(sorted(byfile.items(), key=lambda kv: -kv[1])[:10]))
