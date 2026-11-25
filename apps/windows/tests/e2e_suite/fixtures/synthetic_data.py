"""Pure Python 3 stdlib synthetic fixture generators (zero PII or external deps)."""
from __future__ import annotations

import io
import math
import struct
import zipfile
import zlib
from typing import Any


def make_minimal_pdf(
    text_lines: list[str] | None = None,
    title: str = "Synthetic Document",
    use_flate: bool = False,
) -> bytes:
    """Build a valid single-page PDF 1.4 with accurate xref offsets and optional FlateDecode."""
    lines = text_lines or ["Synthetic E2E PDF Line 1"]
    stream_cmds = ["BT", "/F1 12 Tf", "50 750 Td"]
    for idx, line in enumerate(lines):
        escaped = (
            line.replace("\\", "\\\\")
            .replace("(", "\\(")
            .replace(")", "\\)")
        )
        if idx > 0:
            stream_cmds.append("0 -16 Td")
        stream_cmds.append(f"({escaped}) Tj")
    stream_cmds.append("ET")
    stream_raw = "\n".join(stream_cmds).encode("latin-1", errors="replace")

    if use_flate:
        payload = zlib.compress(stream_raw)
        stream_dict = f"<< /Length {len(payload)} /Filter /FlateDecode >>"
    else:
        payload = stream_raw
        stream_dict = f"<< /Length {len(payload)} >>"

    safe_title = title.replace("(", "[").replace(")", "]")
    objs: list[bytes] = [
        b"1 0 obj\n<< /Type /Catalog /Pages 2 0 R >>\nendobj\n",
        b"2 0 obj\n<< /Type /Pages /Kids [3 0 R] /Count 1 >>\nendobj\n",
        (
            b"3 0 obj\n<< /Type /Page /Parent 2 0 R /MediaBox [0 0 595 842] "
            b"/Resources << /Font << /F1 4 0 R >> >> /Contents 5 0 R >>\nendobj\n"
        ),
        b"4 0 obj\n<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica >>\nendobj\n",
        (
            f"5 0 obj\n{stream_dict}\nstream\n".encode("ascii")
            + payload
            + b"\nendstream\nendobj\n"
        ),
        f"6 0 obj\n<< /Title ({safe_title}) >>\nendobj\n".encode("latin-1", errors="replace"),
    ]

    buf = bytearray(b"%PDF-1.4\n%\xe2\xe3\xcf\xd3\n")
    offsets: list[int] = [0]
    for obj in objs:
        offsets.append(len(buf))
        buf.extend(obj)

    xref_pos = len(buf)
    buf.extend(f"xref\n0 {len(offsets)}\n".encode("ascii"))
    buf.extend(b"0000000000 65535 f \n")
    for off in offsets[1:]:
        buf.extend(f"{off:010d} 00000 n \n".encode("ascii"))

    buf.extend(
        (
            f"trailer\n<< /Size {len(offsets)} /Root 1 0 R /Info 6 0 R >>\n"
            f"startxref\n{xref_pos}\n%%EOF\n"
        ).encode("ascii")
    )
    return bytes(buf)


def _png_chunk(ctype: bytes, data: bytes) -> bytes:
    crc = zlib.crc32(ctype + data) & 0xFFFFFFFF
    return struct.pack(">I", len(data)) + ctype + data + struct.pack(">I", crc)


def make_minimal_png(
    width: int = 64,
    height: int = 64,
    pattern: str = "checker",
) -> bytes:
    """Build a valid 8-bit RGBA PNG with non-uniform pixels (passes PNG validator)."""
    raw_rows = bytearray()
    for y in range(height):
        raw_rows.append(0)  # filter type 0
        for x in range(width):
            if pattern == "checker":
                on = ((x // 8) + (y // 8)) % 2 == 0
                r, g, b, a = (28, 100, 220, 255) if on else (240, 244, 248, 255)
            else:
                r = (x * 255) // max(1, width - 1)
                g = (y * 255) // max(1, height - 1)
                b = 140
                a = 255
            raw_rows.extend((r, g, b, a))

    ihdr = struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0)
    idat = zlib.compress(bytes(raw_rows))
    return (
        b"\x89PNG\r\n\x1a\n"
        + _png_chunk(b"IHDR", ihdr)
        + _png_chunk(b"IDAT", idat)
        + _png_chunk(b"IEND", b"")
    )


def _xml_escape(text: str) -> str:
    return (
        text.replace("&", "&amp;")
        .replace("<", "&lt;")
        .replace(">", "&gt;")
        .replace('"', "&quot;")
    )


def make_minimal_docx(paragraphs: list[str] | None = None) -> bytes:
    """Build a valid OOXML .docx ZIP archive containing the given paragraphs."""
    paras = paragraphs or ["Transformada de Laplace F(s) = 1/(s+a)"]
    para_xml = "".join(
        f"<w:p><w:r><w:t>{_xml_escape(p)}</w:t></w:r></w:p>" for p in paras
    )
    doc_xml = (
        '<?xml version="1.0" encoding="UTF-8" standalone="yes"?>'
        '<w:document xmlns:w="http://schemas.openxmlformats.org/wordprocessingml/2006/main">'
        f"<w:body>{para_xml}</w:body></w:document>"
    )
    content_types = (
        '<?xml version="1.0" encoding="UTF-8" standalone="yes"?>'
        '<Types xmlns="http://schemas.openxmlformats.org/package/2006/content-types">'
        '<Default Extension="rels" ContentType="application/vnd.openxmlformats-package.relationships+xml"/>'
        '<Default Extension="xml" ContentType="application/xml"/>'
        '<Override PartName="/word/document.xml" '
        'ContentType="application/vnd.openxmlformats-officedocument.wordprocessingml.document.main+xml"/>'
        "</Types>"
    )
    rels = (
        '<?xml version="1.0" encoding="UTF-8" standalone="yes"?>'
        '<Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships">'
        '<Relationship Id="rId1" '
        'Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument" '
        'Target="word/document.xml"/>'
        "</Relationships>"
    )
    buf = io.BytesIO()
    with zipfile.ZipFile(buf, "w", compression=zipfile.ZIP_DEFLATED) as zf:
        zf.writestr("[Content_Types].xml", content_types)
        zf.writestr("_rels/.rels", rels)
        zf.writestr("word/document.xml", doc_xml)
    return buf.getvalue()


def make_minimal_xlsx(rows: list[list[Any]] | None = None) -> bytes:
    """Build a valid OOXML .xlsx ZIP archive containing inline string/number cells."""
    data_rows = rows or [["Concepto", "Nota"], ["Nota_Final", 95]]
    row_xml_parts: list[str] = []
    for r_idx, row in enumerate(data_rows, start=1):
        cells: list[str] = []
        for c_idx, val in enumerate(row):
            col_letter = chr(ord("A") + (c_idx % 26))
            ref = f"{col_letter}{r_idx}"
            if isinstance(val, (int, float)) and not isinstance(val, bool):
                cells.append(f'<c r="{ref}"><v>{val}</v></c>')
            else:
                cells.append(
                    f'<c r="{ref}" t="inlineStr"><is><t>{_xml_escape(str(val))}</t></is></c>'
                )
        row_xml_parts.append(f'<row r="{r_idx}">{"".join(cells)}</row>')

    sheet_xml = (
        '<?xml version="1.0" encoding="UTF-8" standalone="yes"?>'
        '<worksheet xmlns="http://schemas.openxmlformats.org/spreadsheetml/2006/main">'
        f"<sheetData>{''.join(row_xml_parts)}</sheetData></worksheet>"
    )
    workbook_xml = (
        '<?xml version="1.0" encoding="UTF-8" standalone="yes"?>'
        '<workbook xmlns="http://schemas.openxmlformats.org/spreadsheetml/2006/main" '
        'xmlns:r="http://schemas.openxmlformats.org/officeDocument/2006/relationships">'
        '<sheets><sheet name="Datos" sheetId="1" r:id="rId1"/></sheets></workbook>'
    )
    wb_rels = (
        '<?xml version="1.0" encoding="UTF-8" standalone="yes"?>'
        '<Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships">'
        '<Relationship Id="rId1" '
        'Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/worksheet" '
        'Target="worksheets/sheet1.xml"/>'
        "</Relationships>"
    )
    content_types = (
        '<?xml version="1.0" encoding="UTF-8" standalone="yes"?>'
        '<Types xmlns="http://schemas.openxmlformats.org/package/2006/content-types">'
        '<Default Extension="rels" ContentType="application/vnd.openxmlformats-package.relationships+xml"/>'
        '<Default Extension="xml" ContentType="application/xml"/>'
        '<Override PartName="/xl/workbook.xml" '
        'ContentType="application/vnd.openxmlformats-officedocument.spreadsheetml.sheet.main+xml"/>'
        '<Override PartName="/xl/worksheets/sheet1.xml" '
        'ContentType="application/vnd.openxmlformats-officedocument.spreadsheetml.worksheet+xml"/>'
        "</Types>"
    )
    rels = (
        '<?xml version="1.0" encoding="UTF-8" standalone="yes"?>'
        '<Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships">'
        '<Relationship Id="rId1" '
        'Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument" '
        'Target="xl/workbook.xml"/>'
        "</Relationships>"
    )
    buf = io.BytesIO()
    with zipfile.ZipFile(buf, "w", compression=zipfile.ZIP_DEFLATED) as zf:
        zf.writestr("[Content_Types].xml", content_types)
        zf.writestr("_rels/.rels", rels)
        zf.writestr("xl/workbook.xml", workbook_xml)
        zf.writestr("xl/_rels/workbook.xml.rels", wb_rels)
        zf.writestr("xl/worksheets/sheet1.xml", sheet_xml)
    return buf.getvalue()


def make_minimal_wav(
    duration_ms: int = 200,
    freq_hz: float = 440.0,
    sample_rate: int = 16000,
) -> bytes:
    """Build a valid 16-bit mono PCM WAV tone."""
    num_samples = max(1, int((sample_rate * duration_ms) / 1000))
    pcm = bytearray()
    for i in range(num_samples):
        sample = int(8000.0 * math.sin(2.0 * math.pi * freq_hz * (i / sample_rate)))
        pcm.extend(struct.pack("<h", sample))

    data_size = len(pcm)
    byte_rate = sample_rate * 2
    header = (
        b"RIFF"
        + struct.pack("<I", 36 + data_size)
        + b"WAVE"
        + b"fmt "
        + struct.pack("<IHHIIHH", 16, 1, 1, sample_rate, byte_rate, 2, 16)
        + b"data"
        + struct.pack("<I", data_size)
    )
    return header + bytes(pcm)


def make_horarios_catalog_canonical() -> dict[str, Any]:
    """Canonical 4-course HorariosTEC catalog (12 Cartesian combos, 7 conflict-free, 5 conflicting)."""
    return {
        "semester": "S-2-2026",
        "courses": [
            {
                "code": "EL2113",
                "name": "Sistemas Digitales",
                "credits": 4,
                "groups": [
                    {
                        "id": "Gr 50",
                        "modality": "Regular",
                        "seats": 12,
                        "sessions": [
                            {"day": "L", "start": "07:30", "end": "09:20"},
                            {"day": "M", "start": "07:30", "end": "09:20"},
                        ],
                    }
                ],
            },
            {
                "code": "FI1102",
                "name": "Física General II",
                "credits": 4,
                "groups": [
                    {
                        "id": "Gr 50",
                        "modality": "Regular",
                        "seats": 8,
                        "sessions": [
                            {"day": "K", "start": "09:45", "end": "11:30"},
                            {"day": "J", "start": "09:45", "end": "11:30"},
                        ],
                    },
                    {
                        "id": "Gr 51",
                        "modality": "Semipresencial",
                        "seats": 5,
                        "sessions": [
                            {"day": "K", "start": "13:00", "end": "14:50"},
                            {"day": "J", "start": "13:00", "end": "14:50"},
                        ],
                    },
                ],
            },
            {
                "code": "FI2103",
                "name": "Física Moderna",
                "credits": 4,
                "groups": [
                    {
                        "id": "Gr 50",
                        "modality": "Regular",
                        "seats": 10,
                        "sessions": [
                            {"day": "K", "start": "09:45", "end": "11:30"},
                            {"day": "V", "start": "09:45", "end": "11:30"},
                        ],
                    },
                    {
                        "id": "Gr 51",
                        "modality": "Regular",
                        "seats": 6,
                        "sessions": [
                            {"day": "L", "start": "15:00", "end": "16:50"},
                            {"day": "M", "start": "15:00", "end": "16:50"},
                        ],
                    },
                ],
            },
            {
                "code": "MA1103",
                "name": "Cálculo y Álgebra Lineal",
                "credits": 4,
                "groups": [
                    {
                        "id": "Gr 50",
                        "modality": "Regular",
                        "seats": 15,
                        "sessions": [
                            {"day": "L", "start": "09:45", "end": "11:30"},
                            {"day": "M", "start": "09:45", "end": "11:30"},
                        ],
                    },
                    {
                        "id": "Gr 51",
                        "modality": "Regular",
                        "seats": 7,
                        "sessions": [
                            {"day": "K", "start": "09:45", "end": "11:30"},
                            {"day": "J", "start": "09:45", "end": "11:30"},
                        ],
                    },
                    {
                        "id": "Gr 52",
                        "modality": "Semipresencial",
                        "seats": 4,
                        "sessions": [
                            {"day": "V", "start": "17:00", "end": "19:50"},
                        ],
                    },
                ],
            },
        ],
    }


def make_horarios_catalog_large(num_courses: int = 8, groups_per_course: int = 6) -> dict[str, Any]:
    """Generate a large combinatorial HorariosTEC catalog (6^8 leaves) for pruning benchmarks."""
    days = ["L", "K", "M", "J", "V", "S"]
    courses: list[dict[str, Any]] = []
    for c_idx in range(num_courses):
        groups: list[dict[str, Any]] = []
        for g_idx in range(groups_per_course):
            day = days[(c_idx + g_idx) % len(days)]
            start_h = 7 + ((c_idx * 2 + g_idx) % 12)
            groups.append({
                "id": f"Gr {50 + g_idx}",
                "modality": "Regular" if g_idx % 2 == 0 else "Semipresencial",
                "seats": 10 + g_idx,
                "sessions": [
                    {"day": day, "start": f"{start_h:02d}:00", "end": f"{start_h + 1:02d}:50"}
                ],
            })
        courses.append({
            "code": f"SY{1000 + c_idx}",
            "name": f"Curso Combinatorio {c_idx + 1}",
            "credits": 3,
            "groups": groups,
        })
    return {"semester": "S-2-2026", "courses": courses}


def make_mna_rc_netlist() -> str:
    return "\n".join([
        "V1 1 0 12.0",
        "R1 1 2 1000.0",
        "C1 2 0 1e-6",
        "R2 2 0 2000.0",
    ])


def make_casio_step_specs() -> list[str]:
    return [
        "fx-991LAX|2,+,3,=|2+3",
        "fx-570VN PLUS|5,*,4,=|5*4",
        "fx-CG50|1,+,1,=|1+1",
        "fx-991CW|V_out=4.7kOhm*2mA|4.7 kOhm * 2 mA | V",
    ]
