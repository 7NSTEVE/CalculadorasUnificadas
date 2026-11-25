#!/usr/bin/env python3
"""Generate C++ descriptor table from cpu73050_702_regs_forensic.json for cg50_register_cert_lab.cpp."""
import json
import pathlib

CERT_DIR = pathlib.Path(__file__).resolve().parents[1] / "certification_v47"
IN_JSON = CERT_DIR / "cpu73050_702_regs_forensic.json"
OUT_HDR = CERT_DIR / "cg50_702_reg_table.h"


def cstr(s: str) -> str:
    return '"' + s.replace("\\", "\\\\").replace('"', '\\"') + '"'


def main() -> int:
    data = json.loads(IN_JSON.read_text(encoding="utf-8"))
    regs = data["registers"]
    lines = [
        "// Auto-generated from cpu73050_702_regs_forensic.json (CPU73050.dll + CGM.dlm + SystemROM.raw)",
        "#pragma once",
        "#include <cstdint>",
        "#include <cstddef>",
        "",
        "struct Sh7305RegSpec {",
        "    uint32_t index;",
        "    const char* compId;",
        "    const char* name;",
        "    const char* desc;",
        "    uint32_t physAddr;",
        "    uint32_t virtAddr;",
        "    uint8_t widthBits;",
        "    uint8_t sizeBytes;",
        "    bool presentOnCg50;",
        "    uint32_t cpu73050Reset;",
        "    bool hasWriteCb;",
        "    bool cbW0c;",
        "    bool cbRo;",
        "    bool cbHasKey;",
        "    uint32_t romLiteralRefs;",
        "    const char* boardForensicStatus;",
        "};",
        "",
        f"static constexpr std::size_t kSh7305RegCount = {len(regs)}u;",
        "static const Sh7305RegSpec kSh7305Regs[kSh7305RegCount] = {",
    ]
    for r in regs:
        cb = r["write_cb_analysis"]
        lines.append(
            f"    {{{r['index']}u, {cstr(r['component_id'])}, {cstr(r['name'])}, {cstr(r['desc'])}, "
            f"0x{r['phys_int']:08X}u, 0x{r['virt_int']:08X}u, {r['width_bits']}u, {r['size_bytes']}u, "
            f"{'true' if r['present_on_cg50'] else 'false'}, 0x{r['reset_int']:08X}u, "
            f"{'true' if cb['has_cb'] else 'false'}, {'true' if cb['w0c_detected'] else 'false'}, "
            f"{'true' if cb['ro_detected'] else 'false'}, {'true' if len(cb['write_keys']) > 0 else 'false'}, "
            f"{r['system_rom_exact_literal_refs']}u, {cstr(r['board_forensic_status'])}}},"
        )
    lines.append("};")
    OUT_HDR.write_text("\n".join(lines) + "\n", encoding="utf-8")
    print(f"Wrote {OUT_HDR} ({len(regs)} registers)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
