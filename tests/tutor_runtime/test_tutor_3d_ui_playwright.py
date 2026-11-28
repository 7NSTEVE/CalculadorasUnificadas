"""Auditoría end-to-end en navegador real (Playwright Chromium + WebGL2) del Tutor 3D,
Tarjeta de Memoria Viva, Práctica Sin Ver, Sprint 6 Días, Minipruebas, Formulario 3 Parciales
y Simulacro de Reposición.

Verifica:
  * 0 peticiones externas (100 % offline, cero CDN);
  * 0 errores de consola o excepciones JS no capturadas;
  * Contexto WebGL2/WebGL activo con píxeles renderizados en las 12 escenas 3D y los 4 casos dorados;
  * La escena 3D abre en pausa por defecto y responde a las 5 cámaras (3D/Frontal/Lateral/Superior/Corte)
    + arrastre orbital (sin abrir modal por accidente) + botón explícito Ampliar;
  * Capturas visuales en Claro (Porcelana), Oscuro y Alto Contraste bajo build/tutor_3d_audit/.
"""

from __future__ import annotations

import pathlib
import socket
import sys
import threading
import time
import unittest

import uvicorn
from playwright.sync_api import sync_playwright

REPO_ROOT = pathlib.Path(__file__).resolve().parent.parent.parent
if str(REPO_ROOT) not in sys.path:
    sys.path.insert(0, str(REPO_ROOT))

from tools.tutor_runtime.server import create_app  # noqa: E402

CAPTURAS_DIR = REPO_ROOT / "build" / "tutor_3d_audit"


def _puerto_libre() -> int:
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
        s.bind(("127.0.0.1", 0))
        return int(s.getsockname()[1])


class TestTutor3DPlaywright(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        CAPTURAS_DIR.mkdir(parents=True, exist_ok=True)
        cls.token = "pw-audit-token-3d"
        cls.port = _puerto_libre()
        app = create_app({
            "session_token": cls.token,
            "repo_root": str(REPO_ROOT),
            "telemetry_enabled": False,
            "memory_db_path": ":memory:",
            "progreso_path": None,
        })
        cfg = uvicorn.Config(app, host="127.0.0.1", port=cls.port, log_level="warning")
        cls.server = uvicorn.Server(cfg)
        cls.server.install_signal_handlers = lambda: None  # type: ignore[method-assign]
        import asyncio
        cls.thread = threading.Thread(target=lambda: asyncio.run(cls.server.serve()), daemon=True)
        cls.thread.start()
        for _ in range(150):
            if cls.server.started:
                break
            time.sleep(0.1)
        assert cls.server.started, "El servidor FastAPI del Tutor no arrancó a tiempo"

    @classmethod
    def tearDownClass(cls) -> None:
        cls.server.should_exit = True
        cls.thread.join(timeout=5)

    def test_flujo_completo_3d_offline_casos_dorados_y_temas(self) -> None:
        url = f"http://127.0.0.1:{self.port}/?port={self.port}&token={self.token}"
        errores_js = []
        externas = []

        with sync_playwright() as p:
            browser = p.chromium.launch(
                headless=True,
                args=["--use-gl=angle", "--use-angle=swiftshader", "--enable-webgl", "--ignore-gpu-blocklist"],
            )
            ctx = browser.new_context(viewport={"width": 1366, "height": 860}, device_scale_factor=1.5)
            page = ctx.new_page()
            page.on("pageerror", lambda exc: errores_js.append(f"pageerror: {exc}"))
            page.on("console", lambda msg: errores_js.append(f"console.{msg.type}: {msg.text}") if msg.type == "error" else None)
            page.on("request", lambda req: externas.append(req.url) if not req.url.startswith((f"http://127.0.0.1:{self.port}", "data:", "blob:")) else None)

            page.goto(url, wait_until="domcontentloaded")
            page.wait_for_function("() => window.__tutor && window.__tutor.banco && window.__tutor.banco.listo", timeout=30000)

            # 1. Portada «Hoy» con cuenta regresiva y plan Sprint 6 días
            txt_hoy = page.locator("#vista-inicio").inner_text()
            self.assertIn("Sprint 6 días", txt_hoy)
            page.screenshot(path=str(CAPTURAS_DIR / "01_hoy_claro.png"))

            # 2. Recorrer los 4 casos dorados + muestra de las 12 escenas 3D
            muestra_3d = [
                ("P1O26-8", "resorte"),
                ("P2O26-8", "manometro"),
                ("Q108", "piston"),
                ("O26-8", "solido"),
                ("T-PEND", "pendulo"),
                ("T-ONDA", "cuerda"),
                ("T-SON", "sonido"),
                ("RP-25II-2", "compuerta"),
                ("T-FLOT", "flotacion"),
                ("RP-24II-2", "tuberia"),
                ("P2E26-2", "tension"),
                ("T-DIL", "dilatacion"),
                ("O26-10", "calor"),
                ("T-RAD", "radiacion"),
            ]

            for lid, modelo_esp in muestra_3d:
                page.evaluate(f"window.__tutor.acciones.abrir({lid!r}, {{ corte: '20.5' }})")
                page.wait_for_selector(f".visual-3d[data-modelo='{modelo_esp}'] canvas.v3d-canvas", timeout=10000)
                page.wait_for_timeout(220)

                # Confirmar motor WebGL2/WebGL activo, escena pausada por defecto y píxeles 3D realmente pintados
                info = page.evaluate("""() => {
                    const c = document.querySelector('.visual-3d canvas.v3d-canvas');
                    const anim = document.querySelector('.visual-3d .v3d-fila-acc .mando-btn');
                    const r = c.getBoundingClientRect();
                    // Muestrear píxeles del canvas 3D para comprobar que NO está en blanco
                    const tmp = document.createElement('canvas');
                    tmp.width = 48; tmp.height = 30;
                    const ctx2 = tmp.getContext('2d');
                    ctx2.drawImage(c, 0, 0, 48, 30);
                    const d = ctx2.getImageData(0, 0, 48, 30).data;
                    const r0 = d[0], g0 = d[1], b0 = d[2];
                    let distintos = 0;
                    for (let i = 0; i < d.length; i += 4) {
                        if (Math.abs(d[i] - r0) + Math.abs(d[i+1] - g0) + Math.abs(d[i+2] - b0) > 18) distintos++;
                    }
                    const tags = document.querySelectorAll('.visual-3d .v3d-tag').length;
                    return {
                        motor: c.dataset.motor3d || '',
                        w: Math.round(r.width),
                        h: Math.round(r.height),
                        animPressed: anim ? anim.getAttribute('aria-pressed') : null,
                        distintos,
                        tags,
                    };
                }""")
                self.assertIn(info["motor"], ("webgl2", "webgl1"), f"{lid} sin WebGL activo: {info}")
                self.assertGreater(info["w"], 120, lid)
                self.assertGreater(info["h"], 100, lid)
                self.assertEqual(info["animPressed"], "false", f"{lid} debe abrir en pausa (pasos cortos)")
                self.assertGreater(info["distintos"], 15, f"{lid} canvas 3D apareció en blanco: {info}")

                if lid in ("P1O26-8", "P2O26-8", "Q108", "O26-8", "RP-25II-2"):
                    # Probar las 5 vistas de cámara (3D, Frontal, Lateral, Superior, Corte)
                    for cam in ("frontal", "lateral", "superior", "corte", "perspectiva"):
                        page.locator(f".visual-3d .v3d-cam-btn[data-camara='{cam}']").click()
                        page.wait_for_timeout(80)
                        self.assertEqual(
                            page.locator(".visual-3d canvas.v3d-canvas").get_attribute("data-vista"),
                            cam,
                            f"{lid} cámara {cam}",
                        )

                    # Arrastre orbital sobre el canvas NO debe abrir modal
                    box = page.locator(".visual-3d canvas.v3d-canvas").bounding_box()
                    assert box is not None
                    page.mouse.move(box["x"] + box["width"] / 2, box["y"] + box["height"] / 2)
                    page.mouse.down()
                    page.mouse.move(box["x"] + box["width"] / 2 + 65, box["y"] + box["height"] / 2 - 20, steps=5)
                    page.mouse.up()
                    page.wait_for_timeout(80)
                    self.assertEqual(page.locator("dialog.visor-visual[open]").count(), 0, "Arrastrar 3D no debe abrir modal")
                    self.assertEqual(page.locator(".visual-3d canvas.v3d-canvas").get_attribute("data-vista"), "libre")

                if lid == "P1O26-8":
                    page.evaluate("window.__tutor.rep.irA(Math.min(4, window.__tutor.rep.total - 1), { reproducir: false })")
                    page.wait_for_timeout(240)
                    page.screenshot(path=str(CAPTURAS_DIR / "11_p1o26_8_resorte3d_pasos_claro.png"))

                if lid == "P2O26-8":
                    page.evaluate("window.__tutor.rep.irA(Math.min(4, window.__tutor.rep.total - 1), { reproducir: false })")
                    page.wait_for_timeout(240)
                    page.screenshot(path=str(CAPTURAS_DIR / "10_p2o26_8_manometro3d_pasos.png"))

                if lid == "O26-8":
                    page.evaluate("window.__tutor.rep.irA(Math.min(4, window.__tutor.rep.total - 1), { reproducir: false })")
                    page.locator(".visual-3d .v3d-cam-btn[data-camara='corte']").click()
                    page.wait_for_timeout(260)
                    page.screenshot(path=str(CAPTURAS_DIR / "02_o26_8_esfera3d_corte_claro.png"))
                    # Probar Tarjeta de Memoria Viva: Capa 0 Intuición y Capa 1 Puente
                    page.locator(".mv-capas button[data-capa='intuicion']").click()
                    page.wait_for_timeout(120)
                    self.assertIn("Capa 0", page.locator(".mv-cuerpo").inner_text())
                    page.locator(".mv-capas button[data-capa='puente']").click()
                    page.wait_for_timeout(120)
                    self.assertIn("Capa 1", page.locator(".mv-cuerpo").inner_text())
                    page.locator(".mv-capas button[data-capa='examen']").click()
                    # Probar botón explícito Ampliar
                    page.locator(".apoyo-vistas button", has_text="Ampliar").click()
                    page.wait_for_selector("dialog.visor-visual[open]", timeout=5000)
                    page.wait_for_timeout(220)
                    page.screenshot(path=str(CAPTURAS_DIR / "03_o26_8_ampliado_3d.png"))
                    page.locator("dialog.visor-visual button", has_text="Cerrar").click()

                if lid == "Q108":
                    page.evaluate("window.__tutor.rep.irA(Math.min(4, window.__tutor.rep.total - 1), { reproducir: false })")
                    page.wait_for_timeout(240)
                    page.screenshot(path=str(CAPTURAS_DIR / "04_q108_piston_pv_3d.png"))

                if lid == "RP-25II-2":
                    page.evaluate("window.__tutor.rep.irA(Math.min(4, window.__tutor.rep.total - 1), { reproducir: false })")
                    page.wait_for_timeout(240)
                    page.screenshot(path=str(CAPTURAS_DIR / "05_represa_3d_integral.png"))

            # 3. Probar Modo «Hazlo tú sin ver» desde P1O26-8
            page.evaluate("window.__tutor.acciones.practicaCiega('RP-26I-7')")
            page.wait_for_selector("dialog.visor-ciega[open]", timeout=8000)
            page.locator("dialog.visor-ciega .ciega-pistas button").click()
            self.assertIn("Fórmulas clave", page.locator("dialog.visor-ciega .ciega-pista-txt").inner_text())
            page.screenshot(path=str(CAPTURAS_DIR / "06_practica_sin_ver.png"))
            page.locator("dialog.visor-ciega button", has_text="Cerrar").click()

            # 4. Probar Sprint 6 días + Diagnóstico 1-a-1 + Miniprueba A (soluciones ocultas)
            page.evaluate("window.__tutor.acciones.preparacion('sprint6')")
            page.wait_for_selector(".intensivo-panel button[data-intensivo='miniprueba-A']", timeout=10000)
            page.screenshot(path=str(CAPTURAS_DIR / "07_sprint_6_dias.png"))
            page.locator(".intensivo-panel button[data-intensivo='miniprueba-A']").click()
            page.wait_for_selector("dialog.mini-prueba[open]", timeout=8000)
            self.assertIn("Soluciones y pistas cerradas", page.locator("dialog.mini-prueba").inner_text())
            page.screenshot(path=str(CAPTURAS_DIR / "13_miniprueba_a_oculta.png"))
            page.locator("dialog.mini-prueba button", has_text="Cerrar").click()

            # 5. Probar Guía del Formulario para Parcial 1, 2 y 3
            page.evaluate("window.__tutor.acciones.estudiarFormulario(null, 'hoja', 1)")
            page.wait_for_selector(".est-parciales", timeout=8000)
            self.assertEqual(page.locator(".est-parciales button[aria-pressed='true']").inner_text(), "Parcial 1")
            page.wait_for_timeout(200)
            page.screenshot(path=str(CAPTURAS_DIR / "12_formulario_parcial1.png"))

            # 6. Probar Simulacro Reposición 3 Parciales
            page.evaluate("window.__tutor.acciones.simulacro()")
            page.wait_for_selector("button[data-modo='reposicion_i2026']", timeout=8000)
            page.locator("button[data-modo='reposicion_i2026']").click()
            page.wait_for_selector("[data-simulacro-modo='reposicion_i2026']", timeout=10000)
            self.assertEqual(page.locator("[data-simulacro-modo='reposicion_i2026'] section.seccion").count(), 9)
            page.wait_for_timeout(200)
            page.screenshot(path=str(CAPTURAS_DIR / "14_simulacro_reposicion_3p.png"))

            # 7. Probar temas Oscuro y Alto Contraste sobre modelo 3D
            page.evaluate("window.__tutor.acciones.abrir('P1O26-8')")
            page.wait_for_selector(".visual-3d[data-modelo='resorte'] canvas.v3d-canvas", timeout=10000)
            page.evaluate("window.__tutor.aplicarTema('dark')")
            page.wait_for_timeout(180)
            page.screenshot(path=str(CAPTURAS_DIR / "08_p1o26_8_resorte3d_oscuro.png"))
            page.evaluate("window.__tutor.aplicarTema('high-contrast')")
            page.wait_for_timeout(180)
            page.screenshot(path=str(CAPTURAS_DIR / "09_p1o26_8_resorte3d_contraste.png"))

            browser.close()

        self.assertEqual(externas, [], f"Se detectaron peticiones externas (prohibido CDN): {externas}")
        self.assertEqual(errores_js, [], f"Errores JS/Consola en Playwright: {errores_js}")


if __name__ == "__main__":
    unittest.main()
