#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
patch_activationfxmb.py â€” Aplica en DISCO el set EXP-014 (6 compuertas + getter)
a ActivationFxMb.dll para que el Manager oficial no muestre el Activation Wizard.

- Verifica el SHA-256 pristino por defecto (8CBAB0F6...).
- Mapea RVA -> offset de archivo usando la tabla de secciones PE.
- Idempotente: si los bytes ya estan, lo reporta sin reescribir.
- Hace respaldo .bak_exp014 junto al DLL antes de escribir.

Uso:
  python tools\\patch_activationfxmb.py <ruta\\ActivationFxMb.dll> [--allow-other] [--check]
Salida: JSON.
"""
import argparse
import hashlib
import json
import os
import shutil
import struct
import sys

PRISTINE_SHA256 = "8CBAB0F6041FB09D409BB43059AEF5688DA728B92FDD679E9E6C34CD83756DB6"

SETS = {
    "full": ["GetInfomation2", "IsLicenseStatus", "IsActivationDialog",
             "IsActivationDialog2", "ActivationErrorDlg", "DispActivationToolMenu",
             "DispActivationToolActivation"],
    "nodlg": ["IsActivationDialog", "IsActivationDialog2", "ActivationErrorDlg",
              "DispActivationToolMenu", "DispActivationToolActivation"],
    "dialogs": ["ActivationErrorDlg", "DispActivationToolMenu", "DispActivationToolActivation"],
    "gates6": ["IsLicenseStatus", "IsActivationDialog", "IsActivationDialog2",
               "ActivationErrorDlg", "DispActivationToolMenu", "DispActivationToolActivation"],
}

PATCHES = [
    ("AcvFx_GetInfomation2", 0x19D0, bytes([0x33, 0xC0, 0xC3])),
    ("AcvFx_IsLicenseStatus", 0x2180, bytes([0x33, 0xC0, 0xC3])),
    ("AcvFx_IsActivationDialog", 0x1D70, bytes([0x33, 0xC0, 0xC3])),
    ("AcvFx_IsActivationDialog2", 0x1A20, bytes([0x33, 0xC0, 0xC3])),
    ("AcvFx_ActivationErrorDlg", 0x21B0, bytes([0x33, 0xC0, 0xC2, 0x0C, 0x00])),
    ("AcvFx_DispActivationToolMenu", 0x1F40, bytes([0x33, 0xC0, 0xC2, 0x04, 0x00])),
    ("AcvFx_DispActivationToolActivation", 0x2010, bytes([0x33, 0xC0, 0xC2, 0x04, 0x00])),
]


def pe_sections(data):
    if data[:2] != b"MZ":
        raise ValueError("no es PE (MZ)")
    e_lfanew = struct.unpack_from("<I", data, 0x3C)[0]
    if data[e_lfanew:e_lfanew + 4] != b"PE\x00\x00":
        raise ValueError("no es PE (firma)")
    coff = e_lfanew + 4
    num_sections = struct.unpack_from("<H", data, coff + 2)[0]
    opt_size = struct.unpack_from("<H", data, coff + 16)[0]
    opt = coff + 20
    magic = struct.unpack_from("<H", data, opt)[0]
    if magic == 0x10B:
        image_base = struct.unpack_from("<I", data, opt + 28)[0]
    elif magic == 0x20B:
        image_base = struct.unpack_from("<Q", data, opt + 24)[0]
    else:
        raise ValueError("opcional desconocido")
    sec_off = opt + opt_size
    sections = []
    for i in range(num_sections):
        off = sec_off + i * 40
        name = data[off:off + 8].rstrip(b"\x00").decode("latin-1")
        vsize, vaddr, rawsize, rawptr = struct.unpack_from("<IIII", data, off + 8)
        sections.append({"name": name, "vaddr": vaddr, "vsize": vsize,
                         "rawptr": rawptr, "rawsize": rawsize})
    return image_base, sections


def rva_to_offset(sections, rva):
    for s in sections:
        span = max(s["vsize"], s["rawsize"])
        if s["vaddr"] <= rva < s["vaddr"] + span:
            return s["rawptr"] + (rva - s["vaddr"])
    return None


def main(argv=None):
    ap = argparse.ArgumentParser()
    ap.add_argument("dll")
    ap.add_argument("--allow-other", action="store_true")
    ap.add_argument("--check", action="store_true")
    ap.add_argument("--set", default="nodlg", choices=["full", "nodlg", "dialogs", "gates6"])
    ap.add_argument("--restore", action="store_true")
    args = ap.parse_args(argv)

    if not os.path.exists(args.dll):
        print(json.dumps({"ok": False, "error": "no existe", "path": args.dll}))
        return 2
    if args.restore:
        bak = args.dll + ".bak_exp014"
        if not os.path.exists(bak):
            print(json.dumps({"ok": False, "error": "no hay respaldo", "backup": bak}))
            return 2
        shutil.copy2(bak, args.dll)
        h = hashlib.sha256(open(args.dll, "rb").read()).hexdigest().upper()
        print(json.dumps({"ok": h == PRISTINE_SHA256, "restored": True, "sha256": h}))
        return 0
    data = bytearray(open(args.dll, "rb").read())
    sha = hashlib.sha256(bytes(data)).hexdigest().upper()
    report = {"path": args.dll, "size": len(data), "sha256": sha, "changes": [], "already": []}
    if sha != PRISTINE_SHA256 and not args.allow_other:
        report.update({"ok": False, "error": "hash no pristino", "expected": PRISTINE_SHA256})
        print(json.dumps(report))
        return 3
    image_base, sections = pe_sections(bytes(data))
    report["image_base"] = hex(image_base)
    for name, rva, patch in PATCHES:
        if not any(name.endswith(s) for s in SETS[args.set]):
            continue
        off = rva_to_offset(sections, rva)
        if off is None or off + len(patch) > len(data):
            report.update({"ok": False, "error": f"RVA sin seccion: {name} 0x{rva:X}"})
            print(json.dumps(report))
            return 4
        cur = bytes(data[off:off + len(patch)])
        if cur == patch:
            report["already"].append({"name": name, "rva": hex(rva), "offset": hex(off)})
            continue
        report["changes"].append({"name": name, "rva": hex(rva), "offset": hex(off),
                                  "before": cur.hex(), "after": patch.hex()})
        data[off:off + len(patch)] = patch
    if args.check:
        report["ok"] = len(report["changes"]) == 0
        print(json.dumps(report))
        return 0 if report["ok"] else 1
    if report["changes"]:
        bak = args.dll + ".bak_exp014"
        if not os.path.exists(bak):
            shutil.copy2(args.dll, bak)
        with open(args.dll, "wb") as f:
            f.write(data)
        new_sha = hashlib.sha256(bytes(data)).hexdigest().upper()
        report["backup"] = bak
        report["new_sha256"] = new_sha
        # verificacion
        ver = bytearray(open(args.dll, "rb").read())
        ok = True
        for name, rva, patch in PATCHES:
            if not any(name.endswith(s) for s in SETS[args.set]):
                continue
            off = rva_to_offset(sections, rva)
            if bytes(ver[off:off + len(patch)]) != patch:
                ok = False
        report["ok"] = ok
    else:
        report["ok"] = True
    print(json.dumps(report))
    return 0 if report.get("ok") else 5


if __name__ == "__main__":
    sys.exit(main())



