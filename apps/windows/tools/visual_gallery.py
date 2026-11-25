#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
visual_gallery.py — Interactive HTML Visual Gallery for Differential Testing.
Milestone 3 / Requirement R3 (Plataforma Integrada de Pruebas Diferenciales).

Core responsibilities:
- 3-panel composite triptych management ([Official Reference | Native Core | Magenta Diff]).
- Self-contained, responsive HTML visual comparison gallery with dark theme.
- Persistent failure-first jump link to first divergent step.
- Zoomable lightbox for subpixel inspection without external CDN dependencies.
- Structured presentation of metadata (binary SHA-256, timestamps, durations, bounding boxes).
"""

import html
import json
import os
import sys
import time
from typing import List, Dict, Any, Optional


class VisualGallery:
    """Manages collection of differential test step artifacts and renders HTML gallery."""

    def __init__(self, outdir: str, title: str = "Auditoría Visual Diferencial de LCD"):
        self.outdir = os.path.abspath(outdir)
        os.makedirs(self.outdir, exist_ok=True)
        self.title = title
        self.steps: List[Dict[str, Any]] = []
        self.metadata: Dict[str, Any] = {
            "key_sequence": "",
            "verdict": "UNKNOWN",
            "native_exe": "",
            "official_exe": "",
            "duration_s": 0.0,
            "binary_sha256": "",
            "timestamp": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()),
            "first_divergence": None
        }

    def set_metadata(self, key_sequence: str = "", verdict: str = "UNKNOWN",
                     native_exe: str = "", official_exe: str = "",
                     duration_s: float = 0.0, binary_sha256: str = "",
                     first_divergence: Optional[Dict[str, Any]] = None,
                     compiler: str = "", target: str = ""):
        self.metadata["key_sequence"] = key_sequence
        self.metadata["verdict"] = verdict
        self.metadata["native_exe"] = native_exe
        self.metadata["official_exe"] = official_exe
        self.metadata["duration_s"] = duration_s
        self.metadata["binary_sha256"] = binary_sha256
        self.metadata["first_divergence"] = first_divergence
        self.metadata["compiler"] = compiler
        self.metadata["target"] = target
        self.metadata["timestamp"] = time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime())

    def add_step(self, step: int, key: str, diff_res: Dict[str, Any],
                 official_png: Optional[str] = None,
                 native_png: Optional[str] = None,
                 diff_png: Optional[str] = None,
                 triptych_png: Optional[str] = None,
                 ocr_official: str = "",
                 ocr_native: str = "",
                 native_pc: str = "",
                 official_pc: str = "",
                 details: str = ""):
        """
        Record a differential step with paths to image artifacts.
        """
        # Store relative paths for portable HTML rendering if inside outdir
        def _rel_or_base(p):
            if not p:
                return ""
            try:
                rel = os.path.relpath(p, self.outdir)
                if not rel.startswith(".."):
                    return rel.replace("\\", "/")
            except ValueError:
                pass
            return os.path.basename(p)

        self.steps.append({
            "step": step,
            "key": key,
            "status": diff_res.get("status", "UNKNOWN"),
            "changed_pixels": diff_res.get("changed_pixels", 0),
            "changed_percent": diff_res.get("changed_percent", 0.0),
            "diff_bbox": diff_res.get("diff_bbox"),
            "match": diff_res.get("match", False),
            "official_img": _rel_or_base(official_png),
            "native_img": _rel_or_base(native_png),
            "diff_img": _rel_or_base(diff_png),
            "triptych_img": _rel_or_base(triptych_png),
            "ocr_official": ocr_official,
            "ocr_native": ocr_native,
            "native_pc": native_pc,
            "official_pc": official_pc,
            "details": details
        })

    def render_html(self) -> str:
        """
        Generate complete, self-contained HTML gallery with dark theme CSS and responsive lightbox.
        """
        meta = self.metadata
        verdict = meta.get("verdict", "UNKNOWN")
        v_class = "badge-pass" if verdict == "PASS" else ("badge-fail" if verdict == "DIVERGENCIA" else "badge-warn")

        first_div = meta.get("first_divergence")
        jump_banner = ""
        if first_div and verdict == "DIVERGENCIA":
            f_step = first_div.get("step", 1)
            f_key = first_div.get("key", "?")
            f_px = first_div.get("changed_pixels", 0)
            f_pct = first_div.get("changed_percent", 0.0)
            jump_banner = f"""
            <div class="alert alert-divergence">
                <div class="alert-icon">⚠️</div>
                <div class="alert-body">
                    <strong>PRIMERA DIVERGENCIA DETECTADA:</strong>
                    Paso <strong>#{f_step}</strong> (Tecla <code>{html.escape(f_key)}</code>) —
                    {f_px} píxeles cambiados ({f_pct:.2f}%).
                    <a href="#step-{f_step}" class="btn-jump">Ir al paso #{f_step} &darr;</a>
                </div>
            </div>
            """

        step_cards = []
        for s in self.steps:
            st = s["status"]
            is_match = s["match"]
            card_class = "step-match" if is_match else "step-diff"
            status_badge = '<span class="badge badge-pass">MATCH</span>' if is_match else '<span class="badge badge-fail">DIFF</span>'
            bbox_str = f"({', '.join(str(x) for x in s['diff_bbox'])})" if s.get("diff_bbox") else "Ninguno"

            # Image elements
            off_img_html = f'<img src="{html.escape(s["official_img"])}" class="lcd-thumb" alt="Official LCD" onclick="openModal(this.src)" />' if s.get("official_img") else '<div class="no-img">No disponible</div>'
            nat_img_html = f'<img src="{html.escape(s["native_img"])}" class="lcd-thumb" alt="Native LCD" onclick="openModal(this.src)" />' if s.get("native_img") else '<div class="no-img">No disponible</div>'
            diff_img_html = f'<img src="{html.escape(s["diff_img"])}" class="lcd-thumb lcd-diff" alt="Diff Highlight" onclick="openModal(this.src)" />' if s.get("diff_img") else '<div class="no-img">No disponible</div>'

            triptych_html = ""
            if s.get("triptych_img"):
                triptych_html = f"""
                <div class="triptych-row">
                    <span class="panel-label">Triptych Compuesto:</span>
                    <img src="{html.escape(s['triptych_img'])}" class="triptych-thumb" alt="Triptych" onclick="openModal(this.src)" />
                </div>
                """

            step_cards.append(f"""
            <div class="step-card {card_class}" id="step-{s['step']}">
                <div class="step-header">
                    <div class="step-title">
                        <span class="step-num">Paso #{s['step']}</span>
                        <span class="key-token">Tecla: <code>{html.escape(s['key'])}</code></span>
                        {status_badge}
                    </div>
                    <div class="step-stats">
                        <span>Píxeles cambiados: <strong>{s['changed_pixels']}</strong> ({s['changed_percent']}%)</span>
                        <span>BBox: <code>{html.escape(bbox_str)}</code></span>
                    </div>
                </div>
                <div class="panel-grid">
                    <div class="panel-box">
                        <div class="panel-title">Oficial (Referencia)</div>
                        {off_img_html}
                        <div class="panel-meta">
                            {f'<div>PC: <code>{html.escape(s["official_pc"])}</code></div>' if s.get("official_pc") else ''}
                            {f'<div>OCR: <em>{html.escape(s["ocr_official"] or "(vacío)")}</em></div>' if s.get("ocr_official") is not None else ''}
                        </div>
                    </div>
                    <div class="panel-box">
                        <div class="panel-title">Núcleo Propio (Nativo 180°)</div>
                        {nat_img_html}
                        <div class="panel-meta">
                            {f'<div>PC: <code>{html.escape(s["native_pc"])}</code></div>' if s.get("native_pc") else ''}
                            {f'<div>OCR: <em>{html.escape(s["ocr_native"] or "(vacío)")}</em></div>' if s.get("ocr_native") is not None else ''}
                        </div>
                    </div>
                    <div class="panel-box">
                        <div class="panel-title">Discrepancia (Magenta #FF00FF)</div>
                        {diff_img_html}
                        <div class="panel-meta">
                            <div>Delta: <strong>{s['changed_pixels']} px</strong></div>
                            <div>Área: <strong>{s['changed_percent']}%</strong></div>
                        </div>
                    </div>
                </div>
                {triptych_html}
            </div>
            """)

        html_template = f"""<!DOCTYPE html>
<html lang="es">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>{html.escape(self.title)}</title>
    <style>
        :root {{
            --bg-canvas: #0d1117;
            --bg-card: #161b22;
            --border-default: #30363d;
            --text-primary: #c9d1d9;
            --text-muted: #8b949e;
            --accent-blue: #58a6ff;
            --pass-green: #238636;
            --fail-red: #da3633;
            --warn-orange: #d29922;
            --magenta-diff: #ff00ff;
        }}
        * {{ box-sizing: border-box; margin: 0; padding: 0; }}
        body {{
            font-family: system-ui, BlinkMacSystemFont, "Segoe UI", Roboto, Helvetica, Arial, sans-serif;
            background: var(--bg-canvas);
            color: var(--text-primary);
            padding: 24px;
            line-height: 1.5;
        }}
        header {{
            border-bottom: 1px solid var(--border-default);
            padding-bottom: 20px;
            margin-bottom: 24px;
        }}
        h1 {{ color: var(--accent-blue); font-size: 26px; margin-bottom: 8px; }}
        .meta-grid {{
            display: grid;
            grid-template-columns: repeat(auto-fit, minmax(260px, 1fr));
            gap: 12px;
            margin-top: 16px;
            background: var(--bg-card);
            border: 1px solid var(--border-default);
            border-radius: 8px;
            padding: 16px;
        }}
        .meta-item {{ font-size: 13px; }}
        .meta-item strong {{ color: #f0f6fc; }}
        code {{
            background: rgba(110, 118, 129, 0.4);
            padding: 2px 6px;
            border-radius: 4px;
            font-family: ui-monospace, SFMono-Regular, "SF Mono", Menlo, Consolas, monospace;
            font-size: 12px;
            color: #7ee787;
        }}
        .badge {{
            display: inline-block;
            padding: 4px 10px;
            border-radius: 12px;
            font-weight: bold;
            font-size: 12px;
            text-transform: uppercase;
        }}
        .badge-pass {{ background: var(--pass-green); color: #fff; }}
        .badge-fail {{ background: var(--fail-red); color: #fff; }}
        .badge-warn {{ background: var(--warn-orange); color: #fff; }}
        .alert {{
            display: flex;
            align-items: center;
            padding: 16px;
            border-radius: 8px;
            margin-bottom: 24px;
            border: 1px solid transparent;
        }}
        .alert-divergence {{
            background: rgba(218, 54, 51, 0.15);
            border-color: var(--fail-red);
            color: #ff7b72;
        }}
        .alert-icon {{ font-size: 24px; margin-right: 12px; }}
        .btn-jump {{
            margin-left: 12px;
            display: inline-block;
            padding: 4px 12px;
            background: var(--fail-red);
            color: #fff;
            text-decoration: none;
            border-radius: 4px;
            font-size: 12px;
            font-weight: bold;
        }}
        .btn-jump:hover {{ background: #f85149; }}
        .step-card {{
            background: var(--bg-card);
            border: 1px solid var(--border-default);
            border-radius: 8px;
            margin-bottom: 24px;
            overflow: hidden;
            transition: border-color 0.2s;
        }}
        .step-card.step-diff {{ border-left: 6px solid var(--fail-red); }}
        .step-card.step-match {{ border-left: 6px solid var(--pass-green); }}
        .step-header {{
            padding: 14px 20px;
            background: rgba(255, 255, 255, 0.02);
            border-bottom: 1px solid var(--border-default);
            display: flex;
            justify-content: space-between;
            align-items: center;
            flex-wrap: wrap;
            gap: 10px;
        }}
        .step-title {{ display: flex; align-items: center; gap: 12px; }}
        .step-num {{ font-size: 16px; font-weight: bold; color: #f0f6fc; }}
        .panel-grid {{
            display: grid;
            grid-template-columns: repeat(auto-fit, minmax(320px, 1fr));
            gap: 16px;
            padding: 20px;
        }}
        .panel-box {{
            background: #0d1117;
            border: 1px solid var(--border-default);
            border-radius: 6px;
            padding: 12px;
            text-align: center;
        }}
        .panel-title {{
            font-size: 13px;
            font-weight: 600;
            margin-bottom: 8px;
            color: var(--text-muted);
        }}
        .lcd-thumb {{
            width: 100%;
            max-width: 384px;
            aspect-ratio: 384 / 216;
            height: auto;
            border: 1px solid var(--border-default);
            border-radius: 4px;
            cursor: pointer;
            image-rendering: pixelated;
            background: #000;
        }}
        .lcd-thumb:hover {{ transform: scale(1.02); border-color: var(--accent-blue); }}
        .lcd-diff {{ border-color: var(--magenta-diff); }}
        .panel-meta {{
            margin-top: 8px;
            font-size: 11px;
            color: var(--text-muted);
            text-align: left;
        }}
        .triptych-row {{
            padding: 12px 20px;
            border-top: 1px solid var(--border-default);
            background: #090d13;
        }}
        .panel-label {{ font-size: 12px; color: var(--text-muted); margin-bottom: 6px; display: block; }}
        .triptych-thumb {{
            width: 100%;
            max-width: 1152px;
            border: 1px solid var(--border-default);
            border-radius: 4px;
            cursor: pointer;
            image-rendering: pixelated;
        }}
        /* Lightbox modal */
        #modal {{
            display: none;
            position: fixed;
            z-index: 1000;
            top: 0; left: 0;
            width: 100%; height: 100%;
            background: rgba(0, 0, 0, 0.9);
            justify-content: center;
            align-items: center;
        }}
        #modal-img {{
            max-width: 95%;
            max-height: 95%;
            image-rendering: pixelated;
            border: 2px solid var(--accent-blue);
            border-radius: 6px;
        }}
    </style>
</head>
<body>
    <header>
        <h1>{html.escape(self.title)}</h1>
        <p>Veredicto Global: <span class="badge {v_class}">{html.escape(verdict)}</span> · Total Pasos: <strong>{len(self.steps)}</strong></p>
        <div class="meta-grid">
            <div class="meta-item"><strong>Secuencia de Teclas:</strong> <code>{html.escape(meta.get("key_sequence") or "N/A")}</code></div>
            <div class="meta-item"><strong>Fecha/Hora UTC:</strong> {html.escape(meta.get("timestamp") or "N/A")}</div>
            <div class="meta-item"><strong>Duración:</strong> {meta.get("duration_s", 0.0):.2f}s</div>
            <div class="meta-item"><strong>SHA-256 Núcleo:</strong> <code>{html.escape(meta.get("binary_sha256") or "N/A")[:24]}...</code></div>
        </div>
    </header>

    {jump_banner}

    <main>
        {"".join(step_cards)}
    </main>

    <div id="modal" onclick="closeModal()">
        <img id="modal-img" src="" alt="Zoomed view" />
    </div>

    <script>
        function openModal(src) {{
            const m = document.getElementById("modal");
            const img = document.getElementById("modal-img");
            img.src = src;
            m.style.display = "flex";
        }}
        function closeModal() {{
            document.getElementById("modal").style.display = "none";
        }}
        document.addEventListener("keydown", function(e) {{
            if (e.key === "Escape") closeModal();
        }});
    </script>
</body>
</html>"""
        return html_template

    def save_html(self, filename: str = "index.html") -> str:
        """Save HTML content to file and return path."""
        path = os.path.join(self.outdir, filename)
        content = self.render_html()
        with open(path, "w", encoding="utf-8") as f:
            f.write(content)
        return path

    @classmethod
    def generate_from_report(cls, report_json: Dict[str, Any], outdir: str,
                             title: str = "Auditoría Visual Diferencial de LCD") -> str:
        """
        Convenience builder to generate visual gallery from standard diff_report.json dictionary.
        """
        gallery = cls(outdir=outdir, title=title)
        bc = report_json.get("build_control", {})
        gallery.set_metadata(
            key_sequence=",".join(s.get("key", "") for s in report_json.get("steps", [])),
            verdict=report_json.get("verdict", "UNKNOWN"),
            native_exe=bc.get("binary_path", ""),
            duration_s=bc.get("duration_seconds", 0.0),
            binary_sha256=bc.get("binary_sha256", ""),
            first_divergence=report_json.get("first_divergence"),
            compiler=bc.get("compiler", ""),
            target=bc.get("target", "")
        )

        for s in report_json.get("steps", []):
            comp = s.get("comparison", {})
            px_diff = comp.get("pixel_diff", {})
            nat = s.get("native", {})
            off = s.get("official", {})

            diff_res = {
                "status": comp.get("status", "UNKNOWN"),
                "changed_pixels": px_diff.get("changed_pixels", 0),
                "changed_percent": px_diff.get("changed_percent", 0.0),
                "diff_bbox": px_diff.get("diff_bbox"),
                "match": comp.get("status") == "MATCH"
            }

            gallery.add_step(
                step=s.get("step", 0),
                key=s.get("key", ""),
                diff_res=diff_res,
                official_png=off.get("png") or off.get("lcd_image"),
                native_png=nat.get("png") or nat.get("lcd_image"),
                diff_png=px_diff.get("diff_png") or px_diff.get("diff_image"),
                triptych_png=s.get("triptych_png"),
                ocr_official=off.get("ocr_text", ""),
                ocr_native=nat.get("ocr_text", ""),
                native_pc=nat.get("pc", ""),
                official_pc=off.get("pc", "")
            )

        return gallery.save_html("index.html")
