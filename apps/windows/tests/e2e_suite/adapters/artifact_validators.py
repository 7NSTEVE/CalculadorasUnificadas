"""Pure Python 3 stdlib structural validators for all E2E output artifacts."""
from __future__ import annotations

import csv
import hashlib
import json
import re
import struct
import xml.etree.ElementTree as ET
import zipfile
import zlib
from dataclasses import dataclass, field
from html.parser import HTMLParser
from pathlib import Path
from typing import Any


@dataclass
class ValidationResult:
    ok: bool
    errors: list[str] = field(default_factory=list)
    details: dict[str, Any] = field(default_factory=dict)

    def __bool__(self) -> bool:
        return self.ok


def sha256_file(path: Path | str) -> str:
    """Compute lowercase SHA-256 hex digest of a file."""
    p = Path(path)
    h = hashlib.sha256()
    with p.open("rb") as fh:
        while True:
            chunk = fh.read(65536)
            if not chunk:
                break
            h.update(chunk)
    return h.hexdigest()


def _paeth_predictor(a: int, b: int, c: int) -> int:
    p = a + b - c
    pa = abs(p - a)
    pb = abs(p - b)
    pc = abs(p - c)
    if pa <= pb and pa <= pc:
        return a
    if pb <= pc:
        return b
    return c


def _unfilter_png_scanlines(
    raw: bytes, width: int, height: int, bpp: int, stride: int
) -> list[bytes]:
    rows: list[bytes] = []
    prev = bytearray(stride)
    pos = 0
    for _ in range(height):
        if pos + 1 + stride > len(raw):
            break
        ftype = raw[pos]
        pos += 1
        src = raw[pos : pos + stride]
        pos += stride
        cur = bytearray(stride)
        if ftype == 0:
            cur[:] = src
        elif ftype == 1:
            for i in range(stride):
                left = cur[i - bpp] if i >= bpp else 0
                cur[i] = (src[i] + left) & 0xFF
        elif ftype == 2:
            for i in range(stride):
                cur[i] = (src[i] + prev[i]) & 0xFF
        elif ftype == 3:
            for i in range(stride):
                left = cur[i - bpp] if i >= bpp else 0
                cur[i] = (src[i] + ((left + prev[i]) >> 1)) & 0xFF
        elif ftype == 4:
            for i in range(stride):
                left = cur[i - bpp] if i >= bpp else 0
                up = prev[i]
                up_left = prev[i - bpp] if i >= bpp else 0
                cur[i] = (src[i] + _paeth_predictor(left, up, up_left)) & 0xFF
        else:
            cur[:] = src
        rows.append(bytes(cur))
        prev = cur
    return rows


def _sample_png_pixels(
    rows: list[bytes],
    width: int,
    height: int,
    channels: int,
    bit_depth: int,
    seen_pixels: set[bytes | tuple[int, ...]],
) -> None:
    if width <= 0 or height <= 0 or not rows:
        return
    step_y = max(1, height // 64)
    step_x = max(1, width // 64)
    if bit_depth in (8, 16):
        pixel_bytes = channels * (2 if bit_depth == 16 else 1)
        for y in range(0, height, step_y):
            row = rows[y]
            for x in range(0, width, step_x):
                seen_pixels.add(row[x * pixel_bytes : (x + 1) * pixel_bytes])
                if len(seen_pixels) >= 2:
                    return
    elif bit_depth in (1, 2, 4):
        mask = (1 << bit_depth) - 1
        for y in range(0, height, step_y):
            row = rows[y]
            for x in range(0, width, step_x):
                if channels == 1:
                    bit_off = x * bit_depth
                    val = (row[bit_off >> 3] >> (8 - bit_depth - (bit_off & 7))) & mask
                    seen_pixels.add((val,))
                else:
                    px = tuple(
                        (row[((x * channels + c) * bit_depth) >> 3] >> (8 - bit_depth - (((x * channels + c) * bit_depth) & 7))) & mask
                        for c in range(channels)
                    )
                    seen_pixels.add(px)
                if len(seen_pixels) >= 2:
                    return


def validate_png(
    path: Path | str,
    min_width: int = 1,
    min_height: int = 1,
    require_non_uniform: bool = True,
) -> ValidationResult:
    p = Path(path)
    if not p.exists() or not p.is_file():
        return ValidationResult(False, [f"PNG file not found: {p}"])
    data = p.read_bytes()
    if len(data) < 24 or data[:8] != b"\x89PNG\r\n\x1a\n":
        return ValidationResult(False, ["Invalid PNG signature"])

    pos = 8
    ihdr: dict[str, int] | None = None
    idat_chunks: list[bytes] = []
    saw_iend = False
    errors: list[str] = []

    while pos + 8 <= len(data):
        length = struct.unpack(">I", data[pos : pos + 4])[0]
        ctype = data[pos + 4 : pos + 8]
        pos += 8
        if pos + length + 4 > len(data):
            errors.append(f"Truncated PNG chunk {ctype!r}")
            break
        cdata = data[pos : pos + length]
        pos += length
        crc_actual = struct.unpack(">I", data[pos : pos + 4])[0]
        pos += 4
        crc_expected = zlib.crc32(ctype + cdata) & 0xFFFFFFFF
        if crc_actual != crc_expected:
            errors.append(f"CRC mismatch in chunk {ctype!r}")
            break

        if ctype == b"IHDR":
            if len(cdata) != 13:
                errors.append("Invalid IHDR length")
                break
            w, h, bit_depth, color_type, comp, filt, interlace = struct.unpack(">IIBBBBB", cdata)
            ihdr = {
                "width": w,
                "height": h,
                "bit_depth": bit_depth,
                "color_type": color_type,
                "compression": comp,
                "filter": filt,
                "interlace": interlace,
            }
        elif ctype == b"IDAT":
            idat_chunks.append(cdata)
        elif ctype == b"IEND":
            saw_iend = True
            break

    if ihdr is None:
        errors.append("Missing IHDR chunk")
    if not saw_iend:
        errors.append("Missing IEND chunk")
    if not idat_chunks:
        errors.append("Missing IDAT image data")
    if errors:
        return ValidationResult(False, errors, {"ihdr": ihdr})

    assert ihdr is not None
    w, h = ihdr["width"], ihdr["height"]
    if w < min_width or h < min_height:
        errors.append(f"PNG dimensions {w}x{h} below minimum {min_width}x{min_height}")

    try:
        raw = zlib.decompress(b"".join(idat_chunks))
    except zlib.error as exc:
        return ValidationResult(False, [f"IDAT zlib decompression failed: {exc}"], {"ihdr": ihdr})

    channels_map = {0: 1, 2: 3, 3: 1, 4: 2, 6: 4}
    bit_depth = ihdr["bit_depth"]
    color_type = ihdr["color_type"]
    interlace = ihdr["interlace"]
    distinct_pixels = 0

    if color_type not in channels_map or bit_depth not in (1, 2, 4, 8, 16) or interlace not in (0, 1):
        errors.append(
            f"Unsupported PNG format: color_type={color_type}, bit_depth={bit_depth}, interlace={interlace}"
        )
    else:
        channels = channels_map[color_type]
        bits_per_pixel = channels * bit_depth
        bpp = max(1, (bits_per_pixel + 7) // 8)
        seen_pixels: set[bytes | tuple[int, ...]] = set()

        if interlace == 0:
            stride = (w * bits_per_pixel + 7) // 8
            rows = _unfilter_png_scanlines(raw, w, h, bpp, stride)
            if len(rows) < h:
                errors.append(f"Incomplete PNG scanlines: {len(rows)} < {h}")
            elif require_non_uniform and w * h > 1:
                _sample_png_pixels(rows, w, h, channels, bit_depth, seen_pixels)
        else:
            adam7_passes = (
                (0, 0, 8, 8),
                (4, 0, 8, 8),
                (0, 4, 4, 8),
                (2, 0, 4, 4),
                (0, 2, 2, 4),
                (1, 0, 2, 2),
                (0, 1, 1, 2),
            )
            pos = 0
            for x0, y0, dx, dy in adam7_passes:
                if x0 >= w or y0 >= h:
                    continue
                pw = (w - x0 + dx - 1) // dx
                ph = (h - y0 + dy - 1) // dy
                stride = (pw * bits_per_pixel + 7) // 8
                pass_len = ph * (1 + stride)
                if pos + pass_len > len(raw):
                    errors.append(f"Incomplete PNG Adam7 pass scanlines at offset {pos}")
                    break
                rows = _unfilter_png_scanlines(raw[pos : pos + pass_len], pw, ph, bpp, stride)
                pos += pass_len
                if len(rows) < ph:
                    errors.append(f"Incomplete PNG Adam7 pass scanlines: {len(rows)} < {ph}")
                    break
                if require_non_uniform and w * h > 1 and len(seen_pixels) < 2:
                    _sample_png_pixels(rows, pw, ph, channels, bit_depth, seen_pixels)

        if not errors and require_non_uniform and w * h > 1:
            distinct_pixels = len(seen_pixels)
            if distinct_pixels < 2:
                errors.append("PNG image is completely uniform/blank")

    return ValidationResult(
        len(errors) == 0,
        errors,
        {"ihdr": ihdr, "file_bytes": len(data), "distinct_sampled_pixels": distinct_pixels},
    )


def validate_zip_container(
    path: Path | str,
    required_entries: list[str] | None = None,
    max_uncompressed_bytes: int = 512 * 1024 * 1024,
    max_ratio: float = 200.0,
) -> ValidationResult:
    p = Path(path)
    if not p.exists() or not p.is_file():
        return ValidationResult(False, [f"ZIP file not found: {p}"])
    errors: list[str] = []
    entries: list[str] = []
    total_uncompressed = 0
    total_compressed = 0
    try:
        with zipfile.ZipFile(p, "r") as zf:
            bad = zf.testzip()
            if bad is not None:
                errors.append(f"ZIP CRC error in member: {bad}")
            infos = zf.infolist()
            if not infos:
                errors.append("ZIP archive contains zero entries")
            for info in infos:
                name = info.filename
                entries.append(name)
                norm = name.replace("\\", "/")
                if (
                    norm.startswith("/")
                    or bool(re.match(r"^[A-Za-z]:", norm))
                    or ".." in norm.split("/")
                ):
                    errors.append(f"Unsafe ZIP member path (traversal/absolute/drive): {name}")
                total_uncompressed += info.file_size
                total_compressed += max(1, info.compress_size)
                if info.compress_size > 0 and (info.file_size / info.compress_size) > max_ratio:
                    errors.append(
                        f"Suspicious compression ratio ({info.file_size / info.compress_size:.1f}x) on {name}"
                    )
            if total_uncompressed > max_uncompressed_bytes:
                errors.append(
                    f"ZIP uncompressed size {total_uncompressed} exceeds cap {max_uncompressed_bytes}"
                )
    except (zipfile.BadZipFile, OSError, zlib.error) as exc:
        return ValidationResult(False, [f"Invalid ZIP container: {exc}"])

    if required_entries:
        entry_set = set(entries)
        for req in required_entries:
            if req not in entry_set:
                errors.append(f"Missing required ZIP entry: {req}")

    return ValidationResult(
        len(errors) == 0,
        errors,
        {
            "entries": entries,
            "entry_count": len(entries),
            "total_uncompressed": total_uncompressed,
            "total_compressed": total_compressed,
        },
    )


def validate_cudoc(path: Path | str, wal_path: Path | str | None = None) -> ValidationResult:
    zres = validate_zip_container(path, required_entries=["manifest.json", "pages.json", "objects.json"])
    if not zres.ok:
        return zres

    errors: list[str] = []
    details: dict[str, Any] = {"entries": zres.details["entries"]}
    try:
        with zipfile.ZipFile(Path(path), "r") as zf:
            manifest = json.loads(zf.read("manifest.json").decode("utf-8"))
            pages = json.loads(zf.read("pages.json").decode("utf-8"))
            objects = json.loads(zf.read("objects.json").decode("utf-8"))

            if not isinstance(manifest, dict):
                errors.append("manifest.json is not a JSON object")
            else:
                if manifest.get("format") != "CalculadoraUnificada.DocumentStudio":
                    errors.append(f"Unexpected .cudoc format magic: {manifest.get('format')!r}")
                if not isinstance(manifest.get("formatVersion"), int) or manifest["formatVersion"] < 1:
                    errors.append(f"Invalid formatVersion: {manifest.get('formatVersion')!r}")
                if manifest.get("mode") not in ("paginated_pdf", "infinite_canvas"):
                    errors.append(f"Invalid document mode: {manifest.get('mode')!r}")
                details["manifest"] = {
                    "title": manifest.get("title"),
                    "mode": manifest.get("mode"),
                    "pageCount": manifest.get("pageCount"),
                    "canvasObjectCount": manifest.get("canvasObjectCount"),
                }

            if not isinstance(pages, list):
                errors.append("pages.json must be a JSON array")
            if not isinstance(objects, dict) or not isinstance(objects.get("canvasObjects"), list):
                errors.append("objects.json must contain canvasObjects array")

            entry_set = set(zf.namelist())
            for container in (
                [obj for pg in (pages if isinstance(pages, list) else []) for obj in pg.get("objects", [])]
                + (objects.get("canvasObjects", []) if isinstance(objects, dict) else [])
            ):
                inline_path = container.get("inlineAssetPath")
                if inline_path and inline_path not in entry_set:
                    errors.append(f"Missing inlineAssetPath in .cudoc: {inline_path}")
    except (KeyError, UnicodeDecodeError, json.JSONDecodeError, zipfile.BadZipFile) as exc:
        errors.append(f"Failed parsing .cudoc JSON members: {exc}")

    if wal_path is not None and Path(wal_path).exists():
        wal_bytes = Path(wal_path).read_bytes()
        pos = 0
        wal_frames = 0
        while pos < len(wal_bytes):
            if pos + 12 > len(wal_bytes):
                errors.append("Truncated CWAL frame header")
                break
            magic = wal_bytes[pos : pos + 4]
            if magic != b"CWAL":
                errors.append(f"Invalid WAL frame magic: {magic!r}")
                break
            body_len, body_crc = struct.unpack("<II", wal_bytes[pos + 4 : pos + 12])
            pos += 12
            if pos + body_len > len(wal_bytes):
                errors.append("Truncated CWAL frame body")
                break
            body = wal_bytes[pos : pos + body_len]
            pos += body_len
            if (zlib.crc32(body) & 0xFFFFFFFF) != body_crc:
                errors.append(f"CWAL frame {wal_frames} CRC mismatch")
                break
            try:
                json.loads(body.decode("utf-8"))
            except (UnicodeDecodeError, json.JSONDecodeError) as exc:
                errors.append(f"CWAL frame {wal_frames} invalid JSON: {exc}")
                break
            wal_frames += 1
        details["wal_frames"] = wal_frames

    return ValidationResult(len(errors) == 0, errors, details)


def validate_pdf(
    path: Path | str,
    require_tounicode: bool = False,
    require_signature: bool = False,
) -> ValidationResult:
    p = Path(path)
    if not p.exists() or not p.is_file():
        return ValidationResult(False, [f"PDF file not found: {p}"])
    data = p.read_bytes()
    errors: list[str] = []

    hdr_idx = data[:1024].find(b"%PDF-")
    if hdr_idx < 0:
        errors.append("Missing %PDF- header")
    if b"%%EOF" not in data[-2048:]:
        errors.append("Missing %%EOF trailer marker")
    if b"xref" not in data and b"/XRef" not in data:
        errors.append("Missing xref table or /XRef stream")
    if b"/Root" not in data or b"/Pages" not in data:
        errors.append("Missing /Root or /Pages catalog structure")

    has_tounicode = b"/ToUnicode" in data
    if require_tounicode and not has_tounicode:
        errors.append("Missing required /ToUnicode CMap")

    has_sig = b"/Sig" in data and b"/ByteRange" in data
    byte_range: list[int] = []
    if has_sig:
        m = re.search(rb"/ByteRange\s*\[\s*(\d+)\s+(\d+)\s+(\d+)\s+(\d+)\s*\]", data)
        if m:
            byte_range = [int(m.group(i)) for i in range(1, 5)]
            a, b, c, d = byte_range
            if not (a >= 0 and b > 0 and a + b < c and c + d <= len(data)):
                errors.append(f"Invalid PDF /ByteRange geometry: {byte_range} vs size={len(data)}")
        elif require_signature:
            errors.append("Malformed /ByteRange array")
    elif require_signature:
        errors.append("Missing /Sig and /ByteRange digital signature")

    return ValidationResult(
        len(errors) == 0,
        errors,
        {
            "size_bytes": len(data),
            "has_tounicode": has_tounicode,
            "has_signature": has_sig,
            "byte_range": byte_range,
        },
    )


def _validate_ooxml(
    path: Path | str,
    required_parts: list[str],
    prefix_match: str | None = None,
) -> ValidationResult:
    zres = validate_zip_container(path, required_entries=required_parts)
    if not zres.ok:
        return zres
    errors: list[str] = []
    entries = zres.details["entries"]
    if prefix_match and not any(e.startswith(prefix_match) and e.endswith(".xml") for e in entries):
        errors.append(f"Missing OOXML part matching {prefix_match}*.xml")
    try:
        with zipfile.ZipFile(Path(path), "r") as zf:
            for member in entries:
                if member.endswith(".xml") or member.endswith(".rels"):
                    ET.fromstring(zf.read(member))
    except (ET.ParseError, KeyError, zipfile.BadZipFile) as exc:
        errors.append(f"OOXML XML parse failure: {exc}")
    return ValidationResult(len(errors) == 0, errors, {"entries": entries})


def validate_ooxml_docx(path: Path | str) -> ValidationResult:
    return _validate_ooxml(path, ["[Content_Types].xml", "_rels/.rels", "word/document.xml"])


def validate_ooxml_pptx(path: Path | str) -> ValidationResult:
    return _validate_ooxml(
        path,
        ["[Content_Types].xml", "_rels/.rels", "ppt/presentation.xml"],
        prefix_match="ppt/slides/slide",
    )


def validate_ooxml_xlsx(path: Path | str) -> ValidationResult:
    return _validate_ooxml(
        path,
        ["[Content_Types].xml", "_rels/.rels", "xl/workbook.xml"],
        prefix_match="xl/worksheets/sheet",
    )


def validate_svg(path: Path | str) -> ValidationResult:
    p = Path(path)
    if not p.exists() or not p.is_file():
        return ValidationResult(False, [f"SVG file not found: {p}"])
    errors: list[str] = []
    try:
        root = ET.fromstring(p.read_text(encoding="utf-8"))
    except (ET.ParseError, UnicodeDecodeError, OSError) as exc:
        return ValidationResult(False, [f"Invalid SVG XML: {exc}"])

    tag_local = root.tag.split("}")[-1].lower()
    if tag_local != "svg":
        errors.append(f"Root XML tag is '{tag_local}', expected 'svg'")
    if len(list(root)) == 0:
        errors.append("SVG contains no child drawing elements")
    return ValidationResult(
        len(errors) == 0,
        errors,
        {"child_count": len(list(root)), "viewBox": root.attrib.get("viewBox", "")},
    )


def validate_dxf(path: Path | str) -> ValidationResult:
    p = Path(path)
    if not p.exists() or not p.is_file():
        return ValidationResult(False, [f"DXF file not found: {p}"])
    text = p.read_text(encoding="utf-8", errors="replace")
    lines = [ln.strip() for ln in text.splitlines()]
    errors: list[str] = []
    if "SECTION" not in lines:
        errors.append("Missing DXF SECTION")
    if "ENTITIES" not in lines:
        errors.append("Missing DXF ENTITIES section")
    if "ENDSEC" not in lines:
        errors.append("Missing DXF ENDSEC")
    if "EOF" not in lines[-4:]:
        errors.append("Missing DXF EOF marker")
    return ValidationResult(len(errors) == 0, errors, {"line_count": len(lines)})


def validate_kicad_sch(path: Path | str) -> ValidationResult:
    p = Path(path)
    if not p.exists() or not p.is_file():
        return ValidationResult(False, [f"KiCad file not found: {p}"])
    text = p.read_text(encoding="utf-8", errors="replace").strip()
    errors: list[str] = []

    if text.startswith("(kicad_sch") or text.startswith("(export"):
        depth = 0
        in_quotes = False
        escaped = False
        for ch in text:
            if escaped:
                escaped = False
                continue
            if ch == "\\" and in_quotes:
                escaped = True
                continue
            if ch == '"':
                in_quotes = not in_quotes
                continue
            if not in_quotes:
                if ch == "(":
                    depth += 1
                elif ch == ")":
                    depth -= 1
                    if depth < 0:
                        errors.append("Unbalanced closing ')' in KiCad S-expression")
                        break
        if depth != 0:
            errors.append(f"Unbalanced KiCad S-expression parentheses (depth={depth})")
        if "(components" not in text and "(symbol" not in text and "(nets" not in text:
            errors.append("KiCad file lacks components/symbol/nets declarations")
        return ValidationResult(len(errors) == 0, errors, {"format": "sexpr", "bytes": len(text)})

    # SPICE / KiCad netlist format (`* title` + component lines + `.end`)
    lines = [ln.strip() for ln in text.splitlines() if ln.strip()]
    if not lines or not lines[0].startswith("*"):
        errors.append("Missing (kicad_sch/(export S-expression or SPICE '*' title line")
    if ".end" not in [ln.lower() for ln in lines]:
        errors.append("Missing .end terminator in KiCad/SPICE netlist")
    comp_lines = [ln for ln in lines if not ln.startswith("*") and not ln.startswith(".")]
    if not comp_lines:
        errors.append("KiCad/SPICE netlist contains zero component lines")
    return ValidationResult(
        len(errors) == 0,
        errors,
        {"format": "spice_netlist", "component_lines": len(comp_lines), "bytes": len(text)},
    )


def validate_wav_pcm(path: Path | str, min_duration_ms: float = 0.0) -> ValidationResult:
    p = Path(path)
    if not p.exists() or not p.is_file():
        return ValidationResult(False, [f"WAV file not found: {p}"])
    data = p.read_bytes()
    if len(data) < 44 or data[:4] != b"RIFF" or data[8:12] != b"WAVE":
        return ValidationResult(False, ["Invalid RIFF/WAVE header"])

    pos = 12
    fmt_info: dict[str, int] | None = None
    data_bytes = b""
    errors: list[str] = []
    while pos + 8 <= len(data):
        cid = data[pos : pos + 4]
        csize = struct.unpack("<I", data[pos + 4 : pos + 8])[0]
        pos += 8
        if pos + csize > len(data):
            errors.append(f"Truncated WAV chunk {cid!r}")
            break
        chunk = data[pos : pos + csize]
        pos += csize + (csize & 1)
        if cid == b"fmt " and len(chunk) >= 16:
            afmt, nch, sr, br, ba, bps = struct.unpack("<HHIIHH", chunk[:16])
            fmt_info = {
                "audio_format": afmt,
                "channels": nch,
                "sample_rate": sr,
                "byte_rate": br,
                "block_align": ba,
                "bits_per_sample": bps,
            }
        elif cid == b"data":
            data_bytes = chunk

    if fmt_info is None:
        errors.append("Missing WAV fmt chunk")
        return ValidationResult(False, errors)
    if fmt_info["audio_format"] != 1:
        errors.append(f"Expected PCM audio_format=1, got {fmt_info['audio_format']}")
    if fmt_info["sample_rate"] < 8000 or fmt_info["channels"] < 1:
        errors.append(f"Invalid WAV sample_rate/channels: {fmt_info}")
    if fmt_info["bits_per_sample"] not in (8, 16, 24, 32):
        errors.append(f"Unsupported bits_per_sample: {fmt_info['bits_per_sample']}")
    if not data_bytes:
        errors.append("Empty WAV data chunk")

    duration_ms = 0.0
    if fmt_info["byte_rate"] > 0:
        duration_ms = (len(data_bytes) / fmt_info["byte_rate"]) * 1000.0
    if duration_ms < min_duration_ms:
        errors.append(f"WAV duration {duration_ms:.1f}ms below minimum {min_duration_ms:.1f}ms")

    return ValidationResult(
        len(errors) == 0,
        errors,
        {"fmt": fmt_info, "data_bytes": len(data_bytes), "duration_ms": round(duration_ms, 2)},
    )


def validate_csv(
    path: Path | str,
    min_rows: int = 1,
    expected_headers: list[str] | None = None,
) -> ValidationResult:
    p = Path(path)
    if not p.exists() or not p.is_file():
        return ValidationResult(False, [f"CSV file not found: {p}"])
    errors: list[str] = []
    rows: list[list[str]] = []
    try:
        with p.open("r", encoding="utf-8", newline="") as fh:
            reader = csv.reader(fh)
            for row in reader:
                rows.append(row)
    except (OSError, UnicodeDecodeError, csv.Error) as exc:
        return ValidationResult(False, [f"CSV parse failed: {exc}"])

    if len(rows) < min_rows:
        errors.append(f"CSV row count {len(rows)} < {min_rows}")
    if rows:
        col_count = len(rows[0])
        if col_count == 0:
            errors.append("CSV header row has 0 columns")
        for idx, row in enumerate(rows[1:], start=1):
            if len(row) != col_count:
                errors.append(f"CSV row {idx} column count {len(row)} != {col_count}")
                break
        if expected_headers is not None and rows[0] != expected_headers:
            errors.append(f"CSV header {rows[0]!r} != expected {expected_headers!r}")

    return ValidationResult(
        len(errors) == 0,
        errors,
        {"row_count": len(rows), "headers": rows[0] if rows else []},
    )


class _StrictHtmlChecker(HTMLParser):
    def __init__(self) -> None:
        super().__init__()
        self.tags_seen: list[str] = []

    def handle_starttag(self, tag: str, attrs: list[tuple[str, str | None]]) -> None:
        self.tags_seen.append(tag.lower())


def validate_html5(path: Path | str) -> ValidationResult:
    p = Path(path)
    if not p.exists() or not p.is_file():
        return ValidationResult(False, [f"HTML file not found: {p}"])
    text = p.read_text(encoding="utf-8", errors="replace")
    lower = text.lower()
    errors: list[str] = []
    if "<!doctype html>" not in lower:
        errors.append("Missing <!DOCTYPE html>")
    parser = _StrictHtmlChecker()
    try:
        parser.feed(text)
        parser.close()
    except Exception as exc:  # noqa: BLE001
        errors.append(f"HTMLParser error: {exc}")
    for req_tag in ("html", "head", "body"):
        if req_tag not in parser.tags_seen:
            errors.append(f"Missing <{req_tag}> element")
    return ValidationResult(len(errors) == 0, errors, {"tag_count": len(parser.tags_seen)})
