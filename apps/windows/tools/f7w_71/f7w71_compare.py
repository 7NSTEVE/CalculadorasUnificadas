"""Compara dos corridas JSON de f7w71_ref.exe (modo determinismo).

Compara metadatos, reset, secuencia y, sobre todo, el estado por tecla:
CPU, E/S, registros y hash del LCD. Escribe un JSON de resultados y devuelve:

  0  PASS (sin diferencias)
  1  FAIL (hay diferencias)
  2  error de entrada
"""

import argparse
import datetime
import hashlib
import json
import os
import sys


def utc_now():
    return datetime.datetime.now(datetime.timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")


def read_json(path):
    with open(path, "r", encoding="utf-8-sig") as f:
        return json.load(f)


def file_sha256(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


MISSING = "<missing>"


def get(obj, path, default=MISSING):
    cur = obj
    for part in path.split("."):
        if not isinstance(cur, dict) or part not in cur:
            return default
        cur = cur[part]
    return cur


def compare_values(path, a, b, differences):
    if a != b:
        differences.append({"path": path, "a": a, "b": b})
        return False
    return True


def compare_step(index, sa, sb, differences):
    prefix = "steps[%d]" % index
    local = []
    compare_values(prefix + ".key.code", get(sa, "key.code"), get(sb, "key.code"), local)
    compare_values(prefix + ".injected", get(sa, "injected"), get(sb, "injected"), local)
    compare_values(prefix + ".wait_steps", get(sa, "wait_steps"), get(sb, "wait_steps"), local)
    compare_values(prefix + ".injected_at_instruction",
                   get(sa, "injected_at_instruction"),
                   get(sb, "injected_at_instruction"), local)
    compare_values(prefix + ".executed_total", get(sa, "executed_total"), get(sb, "executed_total"), local)
    compare_values(prefix + ".cpu.icount", get(sa, "cpu.icount"), get(sb, "cpu.icount"), local)
    compare_values(prefix + ".cpu.pc", get(sa, "cpu.pc"), get(sb, "cpu.pc"), local)
    compare_values(prefix + ".cpu.sp", get(sa, "cpu.sp"), get(sb, "cpu.sp"), local)
    compare_values(prefix + ".io.cmd_after_settle", get(sa, "io.cmd_after_settle"),
                   get(sb, "io.cmd_after_settle"), local)
    compare_values(prefix + ".io.ki_after_settle", get(sa, "io.ki_after_settle"),
                   get(sb, "io.ki_after_settle"), local)
    compare_values(prefix + ".io.ko_after_settle", get(sa, "io.ko_after_settle"),
                   get(sb, "io.ko_after_settle"), local)
    compare_values(prefix + ".lcd.sha256", get(sa, "lcd.sha256"), get(sb, "lcd.sha256"), local)
    compare_values(prefix + ".lcd.non_zero_bytes", get(sa, "lcd.non_zero_bytes"),
                   get(sb, "lcd.non_zero_bytes"), local)

    ra = get(sa, "registers", {})
    rb = get(sb, "registers", {})
    if not isinstance(ra, dict):
        ra = {}
    if not isinstance(rb, dict):
        rb = {}
    for key in sorted(set(ra) | set(rb)):
        compare_values(prefix + ".registers." + key, ra.get(key, MISSING), rb.get(key, MISSING), local)

    differences.extend(local)
    return {
        "step": index,
        "key": get(sa, "key.code"),
        "label": get(sa, "key.label"),
        "equal": len(local) == 0,
        "differences": local,
    }


def main():
    ap = argparse.ArgumentParser(description="Compara dos corridas de f7w71_ref.exe")
    ap.add_argument("--a", required=True, help="JSON de la corrida A")
    ap.add_argument("--b", required=True, help="JSON de la corrida B")
    ap.add_argument("--out", default="", help="JSON de resultados (opcional)")
    ap.add_argument("--label", default="", help="etiqueta para el JSON de salida")
    args = ap.parse_args()

    try:
        a = read_json(args.a)
        b = read_json(args.b)
    except (OSError, ValueError) as exc:
        print("ERROR: %s" % exc, file=sys.stderr)
        return 2

    differences = []
    compare_values("schema", get(a, "schema"), get(b, "schema"), differences)
    compare_values("model", get(a, "model"), get(b, "model"), differences)
    compare_values("firmware.sha256", get(a, "firmware.sha256"), get(b, "firmware.sha256"), differences)
    compare_values("dll.sha256", get(a, "dll.sha256"), get(b, "dll.sha256"), differences)
    compare_values("sequence.codes", get(a, "sequence.codes"), get(b, "sequence.codes"), differences)
    compare_values("sequence.hold", get(a, "sequence.hold"), get(b, "sequence.hold"), differences)
    compare_values("sequence.settle", get(a, "sequence.settle"), get(b, "sequence.settle"), differences)
    compare_values("sequence.poll", get(a, "sequence.poll"), get(b, "sequence.poll"), differences)
    compare_values("sequence.budget_per_key", get(a, "sequence.budget_per_key"),
                   get(b, "sequence.budget_per_key"), differences)
    compare_values("reset.icount", get(a, "reset.icount"), get(b, "reset.icount"), differences)
    compare_values("reset.pc", get(a, "reset.pc"), get(b, "reset.pc"), differences)
    compare_values("reset.sp", get(a, "reset.sp"), get(b, "reset.sp"), differences)
    compare_values("final.icount", get(a, "final.icount"), get(b, "final.icount"), differences)
    compare_values("final.pc", get(a, "final.pc"), get(b, "final.pc"), differences)
    compare_values("final.sp", get(a, "final.sp"), get(b, "final.sp"), differences)
    compare_values("output_sha256", get(a, "output_sha256"), get(b, "output_sha256"), differences)

    sa = get(a, "steps", [])
    sb = get(b, "steps", [])
    if not isinstance(sa, list):
        sa = []
    if not isinstance(sb, list):
        sb = []
    compare_values("steps.count", len(sa), len(sb), differences)

    step_results = []
    for i in range(min(len(sa), len(sb))):
        step_results.append(compare_step(i, sa[i], sb[i], differences))

    status = "pass" if not differences else "fail"
    result = {
        "schema": "f7w71-s1-compare/1",
        "generated_utc": utc_now(),
        "status": status,
        "label": args.label,
        "a": {"path": os.path.abspath(args.a), "sha256": file_sha256(args.a)},
        "b": {"path": os.path.abspath(args.b), "sha256": file_sha256(args.b)},
        "model": get(a, "model"),
        "steps_compared": len(step_results),
        "differences_count": len(differences),
        "step_results": step_results,
        "differences": differences,
    }

    if args.out:
        out_dir = os.path.dirname(os.path.abspath(args.out))
        if out_dir and not os.path.isdir(out_dir):
            os.makedirs(out_dir, exist_ok=True)
        with open(args.out, "w", encoding="utf-8") as f:
            json.dump(result, f, indent=2, ensure_ascii=False)
            f.write("\n")

    print("%s: %s (%d diferencias, %d pasos)" %
          (status.upper(), args.label or os.path.basename(args.out) or "comparison",
           len(differences), len(step_results)))
    for diff in differences[:12]:
        print("  %s: A=%r B=%r" % (diff["path"], diff["a"], diff["b"]))
    if len(differences) > 12:
        print("  ... %d diferencias mas" % (len(differences) - 12))
    return 0 if status == "pass" else 1


if __name__ == "__main__":
    sys.exit(main())
