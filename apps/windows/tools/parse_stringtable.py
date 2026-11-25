#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
parse_stringtable.py — Enumera los STRING TABLE (RT_STRING) de un PE32 y
decodifica sus cadenas UTF-16 con sus IDs.

Uso: python parse_stringtable.py <archivo.exe> [filtro-subcadena]
Herramienta de investigación del proyecto Calculadora Unificada (2026-10-16).
"""
import struct
import sys


def parse_pe(data):
    e_lfanew = struct.unpack_from('<I', data, 0x3C)[0]
    opt = e_lfanew + 4 + 20
    rsrc_rva, rsrc_size = struct.unpack_from('<II', data, opt + 96 + 2 * 8)
    return rsrc_rva, rsrc_size


def rva_to_off_rsrc(data, rsrc_rva, rva):
    # dentro de .rsrc el mapeo es lineal (una sola seccion)
    # localizar la seccion .rsrc real:
    e_lfanew = struct.unpack_from('<I', data, 0x3C)[0]
    fh = e_lfanew + 4
    n = struct.unpack_from('<H', data, fh + 2)[0]
    optsz = struct.unpack_from('<H', data, fh + 16)[0]
    so = fh + 20 + optsz
    for i in range(n):
        s = so + i * 40
        name = data[s:s + 8].rstrip(b'\0').decode('ascii', 'replace')
        vs, va, rs, rp = struct.unpack_from('<IIII', data, s + 8)
        if name == '.rsrc':
            return rp + (rva - va)
    return None


def walk(data, rsrc_rva, dir_off, level, path, out):
    chars, ts, maj, mn, nname, nid = struct.unpack_from(
        '<IIHHHH', data, dir_off)
    for i in range(nname + nid):
        name_or_id, offset = struct.unpack_from(
            '<II', data, dir_off + 16 + i * 8)
        sub_off = offset & 0x7FFFFFFF
        if level == 0:
            rtype = name_or_id
            sub_path = (rtype,)
        elif level == 1:
            sub_path = path + (name_or_id,)
        else:
            if offset & 0x80000000:
                continue
            # entrada hoja: data_rva, size
            data_rva, size, cp, rva2 = struct.unpack_from(
                '<IIII', data, sub_off)
            out.append((path, name_or_id, data_rva, size))
            continue
        walk(data, rsrc_rva, rva_to_off_rsrc(data, rsrc_rva, rsrc_rva) and
             (rva_to_off_rsrc(data, rsrc_rva, rsrc_rva + sub_off)), level + 1,
             sub_path, out)


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        return 1
    sys.stdout.reconfigure(encoding='utf-8', errors='replace')
    path = sys.argv[1]
    filt = sys.argv[2] if len(sys.argv) > 2 else None
    data = open(path, 'rb').read()
    rsrc_rva, _ = parse_pe(data)
    e_lfanew = struct.unpack_from('<I', data, 0x3C)[0]
    fh = e_lfanew + 4
    n = struct.unpack_from('<H', data, fh + 2)[0]
    optsz = struct.unpack_from('<H', data, fh + 16)[0]
    so = fh + 20 + optsz
    base_off = None
    for i in range(n):
        s = so + i * 40
        name = data[s:s + 8].rstrip(b'\0').decode('ascii', 'replace')
        vs, va, rs, rp = struct.unpack_from('<IIII', data, s + 8)
        if name == '.rsrc':
            base_off = rp
            rsrc_va = va
            break
    if base_off is None:
        print('sin .rsrc')
        return 1

    def dir_at(rva_dir):
        return base_off + (rva_dir - rsrc_va)

    def walk_dir(dir_file_off, level, type_id, name_id, out):
        chars, ts, maj, mn, nname, nid = struct.unpack_from(
            '<IIHHHH', data, dir_file_off)
        for i in range(nname + nid):
            n_or_i, offset = struct.unpack_from(
                '<II', data, dir_file_off + 16 + i * 8)
            if offset & 0x80000000:  # subdirectorio (offset relativo a .rsrc)
                sub = base_off + (offset & 0x7FFFFFFF)
                if level == 0:
                    walk_dir(sub, 1, n_or_i, None, out)
                else:
                    walk_dir(sub, 2, type_id, n_or_i, out)
            else:
                leaf = base_off + offset  # relativo a .rsrc
                data_rva, size, cp, rva2 = struct.unpack_from(
                    '<IIII', data, leaf)
                if type_id == 6:  # RT_STRING
                    out.append((name_id, rsrc_va + (data_rva - rsrc_va), size))

    leaves = []
    walk_dir(base_off, 0, None, None, leaves)
    print(f'{len(leaves)} tablas RT_STRING')
    for name_id, _rva, size in leaves:
        off = rva_to_off_rsrc(data, rsrc_va, _rva)
        blob = data[off:off + size]
        base_id = (name_id - 1) * 16
        pos = 0
        for k in range(16):
            if pos + 2 > len(blob):
                break
            ln = struct.unpack_from('<H', blob, pos)[0]
            pos += 2
            text = blob[pos:pos + ln * 2].decode('utf-16-le', 'replace')
            pos += ln * 2
            sid = base_id + k
            if text and (filt is None or filt.lower() in text.lower()):
                print(f'  id={sid} (tabla {name_id} idx {k}): {text!r}')
    return 0


if __name__ == '__main__':
    sys.exit(main())
