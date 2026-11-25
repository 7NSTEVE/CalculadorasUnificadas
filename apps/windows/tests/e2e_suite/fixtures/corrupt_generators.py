"""Adversarial & malformed input generators for Tier 2 boundary/security tests."""
from __future__ import annotations

import io
import json
import struct
import zipfile
import zlib


def make_truncated_zip(valid_zip_bytes: bytes, keep_ratio: float = 0.5) -> bytes:
    cut = max(8, int(len(valid_zip_bytes) * keep_ratio))
    return valid_zip_bytes[:cut]


def make_bad_crc_zip() -> bytes:
    buf = io.BytesIO()
    with zipfile.ZipFile(buf, "w", compression=zipfile.ZIP_DEFLATED) as zf:
        zf.writestr("manifest.json", '{"format":"CalculadoraUnificada.DocumentStudio"}')
    raw = bytearray(buf.getvalue())
    # Corrupt payload bytes inside local/central data without updating CRC
    if len(raw) > 45:
        raw[38] ^= 0xFF
    return bytes(raw)


def make_path_traversal_zip(evil_name: str = "../../evil.txt") -> bytes:
    buf = io.BytesIO()
    with zipfile.ZipFile(buf, "w", compression=zipfile.ZIP_DEFLATED) as zf:
        zf.writestr(evil_name, "traversal_payload")
    return buf.getvalue()


def make_zip_bomb_payload(uncompressed_mb: int = 50) -> bytes:
    """Create a tiny highly-compressed ZIP inflating to `uncompressed_mb` MiB of zeros."""
    buf = io.BytesIO()
    chunk = b"\x00" * (1024 * 1024)
    with zipfile.ZipFile(buf, "w", compression=zipfile.ZIP_DEFLATED) as zf:
        with zf.open("bomb.bin", "w") as member:
            for _ in range(uncompressed_mb):
                member.write(chunk)
    return buf.getvalue()


def make_truncated_pdf() -> bytes:
    return b"%PDF-1.4\n1 0 obj\n<< /Type /Catalog >>\n"


def make_bad_magic_pdf(as_html_login: bool = False) -> bytes:
    if as_html_login:
        return b"<!DOCTYPE html><html><body><form id='loginForm'>Sign in</form></body></html>"
    return b"BADMAGIC_NOT_A_PDF_STREAM_0000"


def make_corrupt_wal(valid_records: list[dict] | None = None, corrupt_tail: bool = True) -> bytes:
    """Build valid CWAL binary frames optionally followed by a corrupted CRC tail frame."""
    recs = valid_records if valid_records is not None else [
        {"opTag": "set_title", "payloadJson": '{"title":"WalRecovered"}'}
    ]
    out = bytearray()
    for r in recs:
        body = json.dumps({"op": r.get("opTag", "add_obj"), "payload": r.get("payloadJson", "{}")}).encode("utf-8")
        crc = zlib.crc32(body) & 0xFFFFFFFF
        out.extend(b"CWAL")
        out.extend(struct.pack("<II", len(body), crc))
        out.extend(body)

    if corrupt_tail:
        bad_body = b'{"op":"add_obj","payload":"{corrupt'
        out.extend(b"CWAL")
        out.extend(struct.pack("<II", len(bad_body), 0xDEADBEEF))
        out.extend(bad_body)
    return bytes(out)


def make_corrupt_png_bad_crc(valid_png: bytes) -> bytes:
    raw = bytearray(valid_png)
    if len(raw) > 25:
        raw[20] ^= 0x55
    return bytes(raw)


def make_blank_png(width: int = 32, height: int = 32) -> bytes:
    raw_rows = bytearray()
    for _ in range(height):
        raw_rows.append(0)
        raw_rows.extend(b"\xff\xff\xff\xff" * width)
    ihdr = struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0)
    idat = zlib.compress(bytes(raw_rows))

    def _chunk(ctype: bytes, data: bytes) -> bytes:
        crc = zlib.crc32(ctype + data) & 0xFFFFFFFF
        return struct.pack(">I", len(data)) + ctype + data + struct.pack(">I", crc)

    return b"\x89PNG\r\n\x1a\n" + _chunk(b"IHDR", ihdr) + _chunk(b"IDAT", idat) + _chunk(b"IEND", b"")


def make_corrupt_horarios_catalogs() -> list[tuple[str, str]]:
    """Return (case_label, raw_json_text) tuples for malformed HorariosTEC catalog testing."""
    return [
        ("empty_string", ""),
        ("empty_object", "{}"),
        ("empty_array", "[]"),
        ("syntax_error", "{bad_json"),
        (
            "invalid_hour_25",
            json.dumps({
                "courses": [{
                    "code": "XX1000",
                    "name": "BadHour",
                    "groups": [{"id": "G1", "sessions": [{"day": "L", "start": "25:00", "end": "26:00"}]}],
                }]
            }),
        ),
        (
            "end_before_start",
            json.dumps({
                "courses": [{
                    "code": "XX1001",
                    "name": "Backwards",
                    "groups": [{"id": "G1", "sessions": [{"day": "L", "start": "11:00", "end": "10:00"}]}],
                }]
            }),
        ),
        (
            "empty_groups",
            json.dumps({
                "courses": [{
                    "code": "CS0000",
                    "name": "SinGrupos",
                    "groups": [],
                }]
            }),
        ),
    ]
