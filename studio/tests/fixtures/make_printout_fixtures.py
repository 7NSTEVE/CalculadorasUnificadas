#!/usr/bin/env python3
"""
Independent fixture generator for CuadernU Printout & Canvas Search tests.
Uses ONLY Python standard library (zero external dependencies, zero CuadernU code)
to avoid circular validation:
  1. fixture_200p.pdf : 200 A4 pages; pages 1..149 and 151..200 have embedded PDF text
                        ("Clave-NNN termodinamica" + "ecuación de Bernoulli") and a
                        raster image XObject; page 150 has ZERO PDF text streams and
                        instead embeds a high-contrast raster image displaying
                        "ESCANEADO 150" in pixel glyphs for Windows.Media.Ocr testing.
  2. fixture.docx     : 3-page Word OpenXML document with headings, page breaks,
                        a table ("Celda_Fourier_42"), and unique per-page keywords.
  3. fixture.pptx     : 5-slide 16:9 PowerPoint OpenXML presentation with slide backgrounds
                        and unique per-slide keywords ("Diapositiva_Maxwell_01".."05").
"""

import argparse
import os
import shutil
import struct
import subprocess
import tempfile
import zipfile
import zlib

# Crisp 5x7 uppercase/digit bitmap font for rendering "ESCANEADO 150" into page 150's raster image
GLYPHS_5X7 = {
    'E': ["11111", "10000", "10000", "11110", "10000", "10000", "11111"],
    'S': ["01111", "10000", "10000", "01110", "00001", "00001", "11110"],
    'C': ["01111", "10000", "10000", "10000", "10000", "10000", "01111"],
    'A': ["01110", "10001", "10001", "11111", "10001", "10001", "10001"],
    'N': ["10001", "11001", "10101", "10011", "10001", "10001", "10001"],
    'D': ["11110", "10001", "10001", "10001", "10001", "10001", "11110"],
    'O': ["01110", "10001", "10001", "10001", "10001", "10001", "01110"],
    '1': ["00100", "01100", "00100", "00100", "00100", "00100", "01110"],
    '5': ["11111", "10000", "10000", "11110", "00001", "00001", "11110"],
    '0': ["01110", "10001", "10001", "10001", "10001", "10001", "01110"],
    ' ': ["00000", "00000", "00000", "00000", "00000", "00000", "00000"],
}


def make_scanned_banner_rgb(text: str, scale: int = 8) -> tuple[int, int, bytes]:
    if os.name == "nt":
        try:
            with tempfile.NamedTemporaryFile(suffix=".rgb", delete=False) as tf:
                rgb_path = tf.name
            ps_script = f"""
Add-Type -AssemblyName System.Drawing
$w = 720; $h = 160
$bmp = New-Object System.Drawing.Bitmap($w, $h, [System.Drawing.Imaging.PixelFormat]::Format24bppRgb)
$g = [System.Drawing.Graphics]::FromImage($bmp)
$g.Clear([System.Drawing.Color]::White)
$g.TextRenderingHint = [System.Drawing.Text.TextRenderingHint]::AntiAliasGridFit
$font = New-Object System.Drawing.Font('Arial', 34, [System.Drawing.FontStyle]::Bold)
$g.DrawString('{text}', $font, [System.Drawing.Brushes]::Black, 36, 48)
$g.Dispose()
$ms = New-Object System.IO.MemoryStream
for ($y = 0; $y -lt $h; $y++) {{
    for ($x = 0; $x -lt $w; $x++) {{
        $c = $bmp.GetPixel($x, $y)
        $ms.WriteByte($c.R); $ms.WriteByte($c.G); $ms.WriteByte($c.B)
    }}
}}
$bmp.Dispose()
[System.IO.File]::WriteAllBytes('{rgb_path.replace("'", "''")}', $ms.ToArray())
"""
            subprocess.run(
                ["powershell.exe", "-NoProfile", "-NonInteractive", "-Command", ps_script],
                check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, timeout=20
            )
            with open(rgb_path, "rb") as rf:
                raw_rgb = rf.read()
            os.remove(rgb_path)
            if len(raw_rgb) == 720 * 160 * 3:
                return 720, 160, zlib.compress(raw_rgb)
        except Exception:
            pass

    char_w = 5 * scale
    char_h = 7 * scale
    gap = 2 * scale
    pad_x = 6 * scale
    pad_y = 6 * scale
    width = pad_x * 2 + len(text) * (char_w + gap)
    height = pad_y * 2 + char_h

    # White background (255, 255, 255)
    row_white = b"\xff\xff\xff" * width
    rows = [bytearray(row_white) for _ in range(height)]

    for idx, ch in enumerate(text):
        pat = GLYPHS_5X7.get(ch, GLYPHS_5X7[' '])
        ox = pad_x + idx * (char_w + gap)
        oy = pad_y
        for gy in range(7):
            for gx in range(5):
                if pat[gy][gx] == '1':
                    for sy in range(scale):
                        ry = oy + gy * scale + sy
                        r = rows[ry]
                        for sx in range(scale):
                            rx = (ox + gx * scale + sx) * 3
                            r[rx] = 0
                            r[rx + 1] = 0
                            r[rx + 2] = 0

    raw = b"".join(bytes(r) for r in rows)
    return width, height, zlib.compress(raw)


def make_small_swatch_rgb() -> tuple[int, int, bytes]:
    w, h = 24, 24
    buf = bytearray(w * h * 3)
    for y in range(h):
        for x in range(w):
            i = (y * w + x) * 3
            buf[i] = (40 + x * 6) & 0xFF
            buf[i + 1] = (90 + y * 5) & 0xFF
            buf[i + 2] = 210
    return w, h, zlib.compress(bytes(buf))


def build_200_page_pdf(out_path: str) -> None:
    objs: list[bytes] = []

    def add_obj(body: bytes) -> int:
        objs.append(body)
        return len(objs)

    # 1: Catalog, 2: Pages, 3: Font (Helvetica WinAnsiEncoding)
    add_obj(b"<< /Type /Catalog /Pages 2 0 R >>")
    add_obj(b"")  # placeholder for Pages
    add_obj(b"<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica /Encoding /WinAnsiEncoding >>")

    # 4: Small color swatch XObject shared by normal pages
    sw_w, sw_h, sw_z = make_small_swatch_rgb()
    swatch_obj = add_obj(
        f"<< /Type /XObject /Subtype /Image /Width {sw_w} /Height {sw_h} "
        f"/ColorSpace /DeviceRGB /BitsPerComponent 8 /Filter /FlateDecode /Length {len(sw_z)} >>\nstream\n".encode("ascii")
        + sw_z
        + b"\nendstream"
    )

    # 5: High-contrast raster image for Page 150 ("ESCANEADO 150")
    sc_w, sc_h, sc_z = make_scanned_banner_rgb("ESCANEADO 150", scale=7)
    scan_obj = add_obj(
        f"<< /Type /XObject /Subtype /Image /Width {sc_w} /Height {sc_h} "
        f"/ColorSpace /DeviceRGB /BitsPerComponent 8 /Filter /FlateDecode /Length {len(sc_z)} >>\nstream\n".encode("ascii")
        + sc_z
        + b"\nendstream"
    )

    page_ids: list[int] = []
    for p1 in range(1, 201):
        if p1 == 150:
            # Image-only scanned page (NO BT/ET text stream!)
            content_raw = f"q 420 0 0 92 72 520 cm /ImScan Do Q\n".encode("ascii")
            res_dict = f"<< /XObject << /ImScan {scan_obj} 0 R >> >>".encode("ascii")
        else:
            # WinAnsi byte \xf3 = 'ó' in "ecuación"
            line1 = f"Clave-{p1:03d} termodinamica".encode("latin1")
            line2 = f"Pagina {p1} - ecuaci\xf3n de Bernoulli y conservacion de energia".encode("latin1")
            content_raw = (
                b"BT /F1 18 Tf 54 760 Td (" + line1 + b") Tj ET\n"
                b"BT /F1 12 Tf 54 726 Td (" + line2 + b") Tj ET\n"
                b"q 64 0 0 64 54 620 cm /ImSw Do Q\n"
            )
            res_dict = f"<< /Font << /F1 3 0 R >> /XObject << /ImSw {swatch_obj} 0 R >> >>".encode("ascii")

        c_id = add_obj(
            f"<< /Length {len(content_raw)} >>\nstream\n".encode("ascii")
            + content_raw
            + b"endstream"
        )
        p_id = add_obj(
            f"<< /Type /Page /Parent 2 0 R /MediaBox [0 0 595 842] /Resources ".encode("ascii")
            + res_dict
            + f" /Contents {c_id} 0 R >>".encode("ascii")
        )
        page_ids.append(p_id)

    kids_str = " ".join(f"{pid} 0 R" for pid in page_ids)
    objs[1] = f"<< /Type /Pages /Count {len(page_ids)} /Kids [ {kids_str} ] >>".encode("ascii")

    out = bytearray(b"%PDF-1.4\n%\xe2\xe3\xcf\xd3\n")
    offsets = [0]
    for i, body in enumerate(objs, start=1):
        offsets.append(len(out))
        out.extend(f"{i} 0 obj\n".encode("ascii"))
        out.extend(body)
        out.extend(b"\nendobj\n")

    xref_pos = len(out)
    out.extend(f"xref\n0 {len(objs) + 1}\n".encode("ascii"))
    out.extend(b"0000000000 65535 f \n")
    for off in offsets[1:]:
        out.extend(f"{off:010d} 00000 n \n".encode("ascii"))
    out.extend(
        f"trailer\n<< /Size {len(objs) + 1} /Root 1 0 R >>\nstartxref\n{xref_pos}\n%%EOF\n".encode("ascii")
    )

    with open(out_path, "wb") as f:
        f.write(out)


def build_docx_fixture(out_path: str) -> None:
    content_types = """<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<Types xmlns="http://schemas.openxmlformats.org/package/2006/content-types">
  <Default Extension="rels" ContentType="application/vnd.openxmlformats-package.relationships+xml"/>
  <Default Extension="xml" ContentType="application/xml"/>
  <Override PartName="/word/document.xml" ContentType="application/vnd.openxmlformats-officedocument.wordprocessingml.document.main+xml"/>
</Types>"""
    root_rels = """<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships">
  <Relationship Id="rId1" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument" Target="word/document.xml"/>
</Relationships>"""
    doc_xml = """<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<w:document xmlns:w="http://schemas.openxmlformats.org/wordprocessingml/2006/main"
            xmlns:m="http://schemas.openxmlformats.org/officeDocument/2006/math">
  <w:body>
    <w:p><w:pPr><w:pStyle w:val="Heading1"/></w:pPr><w:r><w:rPr><w:b/></w:rPr><w:t>CapituloWordUno Electromagnetismo</w:t></w:r></w:p>
    <w:p><w:r><w:t>Teorema de Gauss y densidad de carga en conductores.</w:t></w:r></w:p>
    <w:p><w:r><w:br w:type="page"/></w:r></w:p>
    <w:p><w:pPr><w:pStyle w:val="Heading1"/></w:pPr><w:r><w:rPr><w:b/></w:rPr><w:t>CapituloWordDos AnalisisArmonico</w:t></w:r></w:p>
    <w:tbl>
      <w:tr>
        <w:tc><w:p><w:r><w:t>Magnitud</w:t></w:r></w:p></w:tc>
        <w:tc><w:p><w:r><w:t>Celda_Fourier_42</w:t></w:r></w:p></w:tc>
      </w:tr>
    </w:tbl>
    <w:p><w:r><w:br w:type="page"/></w:r></w:p>
    <w:p><w:pPr><w:pStyle w:val="Heading1"/></w:pPr><w:r><w:rPr><w:b/></w:rPr><w:t>CapituloWordTres LaplaceTransform</w:t></w:r></w:p>
    <w:p><w:r><w:t>Respuesta en frecuencia y estabilidad asintotica.</w:t></w:r></w:p>
  </w:body>
</w:document>"""
    with zipfile.ZipFile(out_path, "w", zipfile.ZIP_DEFLATED) as z:
        z.writestr("[Content_Types].xml", content_types)
        z.writestr("_rels/.rels", root_rels)
        z.writestr("word/document.xml", doc_xml)


def build_pptx_fixture(out_path: str) -> None:
    # Keep a complete OpenXML presentation for deterministic converter tests.
    fixture_path = os.path.join(os.path.dirname(__file__), "printout_5slides.pptx")
    shutil.copyfile(fixture_path, out_path)

def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--out", required=True, help="Output directory for generated fixtures")
    args = parser.parse_args()
    os.makedirs(args.out, exist_ok=True)

    pdf_path = os.path.join(args.out, "fixture_200p.pdf")
    docx_path = os.path.join(args.out, "fixture.docx")
    pptx_path = os.path.join(args.out, "fixture.pptx")

    build_200_page_pdf(pdf_path)
    build_docx_fixture(docx_path)
    build_pptx_fixture(pptx_path)

    print(f"GENERATED: {pdf_path} ({os.path.getsize(pdf_path)} bytes)")
    print(f"GENERATED: {docx_path} ({os.path.getsize(docx_path)} bytes)")
    print(f"GENERATED: {pptx_path} ({os.path.getsize(pptx_path)} bytes)")


if __name__ == "__main__":
    main()
