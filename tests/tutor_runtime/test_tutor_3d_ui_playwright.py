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

    # ------------------------------------------------------------------ requisitos R1 a R10

    _JS_AYUDA = """
    window.__px = {
      muestra(c) {
        const t = document.createElement('canvas'); t.width = 96; t.height = 60;
        const x = t.getContext('2d'); x.drawImage(c, 0, 0, 96, 60);
        return Array.from(x.getImageData(0, 0, 96, 60).data);
      },
      dif(a, b) {
        let n = 0;
        for (let i = 0; i < a.length; i += 4) if (Math.abs(a[i] - b[i]) + Math.abs(a[i+1] - b[i+1]) + Math.abs(a[i+2] - b[i+2]) > 24) n++;
        return n;
      },
      cam(c) { return (c.dataset.cam || '0,0,0').split(',').map(Number); },
    };
    """

    def _abrir(self, page, lid: str) -> None:
        page.evaluate(f"window.__tutor.acciones.abrir({lid!r}, {{ corte: '20.5' }})")
        # Esperar ESA lección (no el lienzo de la anterior, que sigue en pantalla mientras carga)
        page.wait_for_function(
            "(lid) => window.__tutor.rep && window.__tutor.rep.lec && window.__tutor.rep.lec.id === lid"
            " && document.querySelector('.visual-3d canvas.v3d-canvas')",
            arg=lid, timeout=15000,
        )
        page.evaluate("window.__tutor.rep && window.__tutor.rep.pausar && window.__tutor.rep.pausar()")
        page.wait_for_timeout(400)

    def _muestra(self, page):
        return page.evaluate("window.__px.muestra(document.querySelector('.visual-3d canvas.v3d-canvas'))")

    def _dif(self, page, a, b) -> int:
        return page.evaluate("([a, b]) => window.__px.dif(a, b)", [a, b])

    def _cam(self, page):
        return page.evaluate("window.__px.cam(document.querySelector('.visual-3d canvas.v3d-canvas'))")

    def test_requisitos_3d_r1_a_r10(self) -> None:
        url = f"http://127.0.0.1:{self.port}/?port={self.port}&token={self.token}"
        errores_js: list = []
        with sync_playwright() as p:
            browser = p.chromium.launch(
                headless=True,
                args=["--use-gl=angle", "--use-angle=swiftshader", "--enable-webgl", "--ignore-gpu-blocklist"],
            )
            ctx = browser.new_context(viewport={"width": 1366, "height": 860}, device_scale_factor=1)
            ctx.add_init_script(self._JS_AYUDA)
            page = ctx.new_page()
            page.on("pageerror", lambda exc: errores_js.append(f"pageerror: {exc}"))
            page.on("console", lambda msg: errores_js.append(f"console.{msg.type}: {msg.text}") if msg.type == "error" else None)
            page.goto(url, wait_until="domcontentloaded")
            page.wait_for_function("() => window.__tutor && window.__tutor.banco && window.__tutor.banco.listo", timeout=30000)
            page.evaluate("import('./js/nucleo/api.js').then((api) => api.guardarAjuste('narracion', false))")

            # ---- Q108 (pistón, vista inicial «Corte»)
            self._abrir(page, "Q108")
            canvas = page.locator(".visual-3d canvas.v3d-canvas")
            self.assertEqual(canvas.get_attribute("data-vista"), "corte", "R2: vista inicial = la del enunciado")
            caja = canvas.bounding_box()
            assert caja is not None
            cx, cy = caja["x"] + caja["width"] / 2, caja["y"] + caja["height"] / 2

            # R1: arrastrar con la escena EN PAUSA cambia la imagen (antes no se redibujaba)
            a = self._muestra(page)
            page.mouse.move(cx, cy)
            page.mouse.down()
            page.mouse.move(cx + 90, cy - 25, steps=6)
            page.mouse.up()
            page.wait_for_timeout(200)
            b = self._muestra(page)
            self.assertGreater(self._dif(page, a, b), 60, "R1: el giro con el mouse no redibujó la escena")
            self.assertEqual(canvas.get_attribute("data-vista"), "libre")

            # R1: tope de inclinación (ni arriba ni abajo se da vuelta)
            for dy, lim in ((900, 1.48), (-1800, -0.42)):
                page.mouse.move(cx, cy)
                page.mouse.down()
                page.mouse.move(cx, cy + dy, steps=8)
                page.mouse.up()
                page.wait_for_timeout(150)
                elev = self._cam(page)[1]
                self.assertLessEqual(abs(elev - lim), 0.01, f"R1: la inclinación debe quedar topada en {lim}, quedó {elev}")

            # R1: doble clic (o doble toque) vuelve a la vista del enunciado
            page.mouse.dblclick(cx, cy)
            page.wait_for_timeout(700)
            self.assertEqual(canvas.get_attribute("data-vista"), "corte", "R1: doble toque no volvió a la vista inicial")
            self.assertEqual(page.locator(".v3d-cam-btn[aria-pressed='true']").get_attribute("data-camara"), "corte")

            # R2: las vistas fijas cambian la imagen
            previa = self._muestra(page)
            for cam in ("frontal", "lateral", "superior", "perspectiva", "corte"):
                page.locator(f".visual-3d .v3d-cam-btn[data-camara='{cam}']").click()
                page.wait_for_timeout(700)
                self.assertEqual(canvas.get_attribute("data-vista"), cam)
                ahora = self._muestra(page)
                self.assertGreater(self._dif(page, previa, ahora), 30, f"R2: la vista {cam} se ve igual a la anterior")
                previa = ahora

            # R3: rueda y pellizco con dos dedos cambian la distancia
            d0 = self._cam(page)[2]
            page.mouse.move(cx, cy)
            for _ in range(3):
                page.mouse.wheel(0, -200)
                page.wait_for_timeout(60)
            page.wait_for_timeout(700)
            d1 = self._cam(page)[2]
            self.assertLess(d1, d0 - 0.3, f"R3: la rueda no acercó ({d0} → {d1})")
            page.evaluate(
                """([cx, cy]) => {
                  const c = document.querySelector('.visual-3d canvas.v3d-canvas');
                  const ev = (t, x, id) => c.dispatchEvent(new PointerEvent(t, { bubbles: true, cancelable: true,
                    clientX: x, clientY: cy, pointerId: id, pointerType: 'touch', isPrimary: id === 31, buttons: t === 'pointerup' ? 0 : 1 }));
                  ev('pointerdown', cx - 30, 31); ev('pointerdown', cx + 30, 32);
                  for (let k = 1; k <= 6; k += 1) { ev('pointermove', cx - 30 + k * 4, 31); ev('pointermove', cx + 30 - k * 4, 32); }
                  ev('pointerup', cx - 6, 31); ev('pointerup', cx + 6, 32);
                }""",
                [cx, cy],
            )
            page.wait_for_timeout(400)
            d2 = self._cam(page)[2]
            self.assertGreater(d2, d1 + 0.3, f"R3: juntar dos dedos no alejó ({d1} → {d2})")

            # R9: teclado (flechas giran; Inicio vuelve) y descripción para el lector de pantalla
            canvas.focus()
            az0 = self._cam(page)[0]
            page.keyboard.press("ArrowLeft")
            page.keyboard.press("ArrowLeft")
            page.wait_for_timeout(700)
            self.assertLess(self._cam(page)[0], az0 - 0.2, "R9: las flechas no giraron la escena")
            page.keyboard.press("Home")
            page.wait_for_timeout(700)
            self.assertEqual(canvas.get_attribute("data-vista"), "corte")
            self.assertEqual(canvas.get_attribute("tabindex"), "0")
            self.assertIn("Arrastrá", canvas.get_attribute("aria-label") or "")
            desc = canvas.get_attribute("aria-describedby")
            self.assertTrue(desc and page.locator(f"#{desc}").inner_text().strip(), "R9: la escena no tiene descripción")
            page.keyboard.press("Tab")
            self.assertEqual(page.evaluate("document.activeElement.tagName"), "BUTTON", "R9: Tab no llega a un botón")

            # R7: en el paso «Datos», datos en naranja con su valor y lo que piden en morado, sin LaTeX crudo
            datos = page.locator(".visual-3d .v3d-tag.dato").all_inner_texts()
            piden = page.locator(".visual-3d .v3d-tag.pide").all_inner_texts()
            self.assertGreaterEqual(len(datos), 4, f"R7: faltan datos marcados: {datos}")
            self.assertGreaterEqual(len(piden), 2, f"R7: falta lo que piden: {piden}")
            todo = " ".join(datos + piden)
            self.assertIn("1 mol", todo)
            self.assertIn("0,0045", todo)
            self.assertNotIn("$", todo)
            self.assertNotIn("\\", todo)
            self.assertEqual(page.locator(".visual-3d .v3d-tag.dato").first.get_attribute("data-color"), "naranja")
            # Al salir de los pasos «Datos» (Q108 tiene «Datos» y «Datos (sigue)») los datos se van
            page.evaluate(
                "window.__tutor.rep.irA(window.__tutor.rep.lec.pasos.findIndex("
                "(p) => !/^\\s*(?:\\d+\\.\\s*)?datos\\b/i.test(p.titulo)), { reproducir: false })"
            )
            page.wait_for_timeout(500)
            self.assertEqual(page.locator(".visual-3d .v3d-tag.dato").count(), 0, "R7: los datos deben verse solo en el paso «Datos»")

            # R6: animar / pausar y deslizador
            animar = page.locator(".visual-3d .v3d-fila-acc .mando-btn")
            self.assertEqual(animar.get_attribute("aria-pressed"), "false")
            deslizador = page.locator(".visual-3d .v3d-mandos input[type=range]").first
            v0 = float(deslizador.input_value())
            animar.click()
            page.wait_for_timeout(800)
            self.assertEqual(animar.get_attribute("aria-pressed"), "true")
            self.assertNotEqual(float(deslizador.input_value()), v0, "R6: Animar no movió el proceso")
            animar.click()
            v1 = float(deslizador.input_value())
            page.wait_for_timeout(500)
            self.assertEqual(float(deslizador.input_value()), v1, "R6: Pausar no detuvo el proceso")

            # R8: la franja barre entre los límites y la suma llega a la fuerza total (T-INT0: 156,8 kN)
            self._abrir(page, "T-INT0")
            barra = page.locator(".visual-3d .v3d-integral")
            self.assertTrue(barra.is_visible(), "R8: falta la suma de franjas")
            sl = page.locator(".visual-3d .v3d-mandos input[type=range]").first
            sl.fill("0.5")
            page.wait_for_timeout(300)
            mitad = page.locator(".visual-3d .v3d-integral-txt").inner_text()
            sl.fill("1")
            page.wait_for_timeout(300)
            final = page.locator(".visual-3d .v3d-integral-txt").inner_text()
            self.assertIn("156,8 kN de 156,8 kN", final, f"R8: la suma no llega al total: {final}")
            self.assertNotIn("156,8 kN de 156,8 kN", mitad)
            self.assertEqual(page.locator(".visual-3d .v3d-integral-barra").get_attribute("aria-valuenow"), "100")
            page.screenshot(path=str(CAPTURAS_DIR / "20_t_int0_suma_franjas.png"))

            # R10: primera imagen en menos de 1 s desde que se crea el visor
            ms = page.evaluate(
                """async () => {
                  const { VisorEscena3D } = await import('./js/visuales3d/visor_escena_3d.js');
                  const apoyo = window.__tutor.rep.lec.apoyo_visual;
                  const caja = document.createElement('div');
                  caja.style.cssText = 'position:fixed;left:0;top:0;width:420px;z-index:9999';
                  document.body.appendChild(caja);
                  const t0 = performance.now();
                  const v = new VisorEscena3D(apoyo, {});
                  caja.appendChild(v.el);
                  let ms = null;
                  while (performance.now() - t0 < 5000) {
                    await new Promise((r) => requestAnimationFrame(r));
                    const a = window.__px.muestra(v.canvas);
                    let n = 0;
                    for (let i = 0; i < a.length; i += 4) if (Math.abs(a[i] - a[0]) + Math.abs(a[i+1] - a[1]) + Math.abs(a[i+2] - a[2]) > 18) n++;
                    if (n > 40) { ms = performance.now() - t0; break; }
                  }
                  v.destruir(); caja.remove();
                  return ms;
                }"""
            )
            self.assertIsNotNone(ms, "R10: la escena nunca se dibujó")
            self.assertLess(ms, 1000, f"R10: la escena tardó {ms:.0f} ms en aparecer")

            # R10: temas (claro, oscuro, contraste) cambian el fondo; «Ver en grande» agranda la escena
            fondos = []
            for tema in ("light", "dark", "high-contrast"):
                page.evaluate(f"window.__tutor.aplicarTema({tema!r})")
                page.wait_for_timeout(400)
                fondos.append(tuple(self._muestra(page)[:3]))
            self.assertEqual(len(set(fondos)), 3, f"R10: los tres temas deben verse distintos: {fondos}")
            page.evaluate("window.__tutor.aplicarTema('light')")
            ancho_panel = canvas.bounding_box()["width"]
            page.locator(".apoyo-vistas button", has_text="Ampliar").click()
            page.wait_for_selector("dialog.visor-visual[open] .visual-3d canvas.v3d-canvas", timeout=5000)
            page.wait_for_timeout(400)
            grande = page.locator("dialog.visor-visual[open] .visual-3d canvas.v3d-canvas").bounding_box()
            self.assertGreater(grande["width"], ancho_panel * 1.3, "R10: «Ver en grande» no agrandó la escena")
            page.locator("dialog.visor-visual button", has_text="Cerrar").click()

            # R6: con «menos movimiento» no hay animación (el botón avanza un paso fijo)
            ctx_rm = browser.new_context(viewport={"width": 1366, "height": 860}, reduced_motion="reduce")
            ctx_rm.add_init_script(self._JS_AYUDA)
            pg = ctx_rm.new_page()
            pg.goto(url, wait_until="domcontentloaded")
            pg.wait_for_function("() => window.__tutor && window.__tutor.banco && window.__tutor.banco.listo", timeout=30000)
            self._abrir(pg, "Q108")
            boton = pg.locator(".visual-3d .v3d-fila-acc .mando-btn")
            self.assertEqual(boton.inner_text().strip(), "Avanzar paso")
            sl = pg.locator(".visual-3d .v3d-mandos input[type=range]").first
            x0 = float(sl.input_value())
            boton.click()
            pg.wait_for_timeout(600)
            x1 = float(sl.input_value())
            self.assertAlmostEqual((x1 - x0) % 1, 0.16, places=2)
            pg.wait_for_timeout(600)
            self.assertEqual(float(sl.input_value()), x1, "R6: con menos movimiento no debe animarse solo")
            ctx_rm.close()

            # R10: sin WebGL, el panel muestra el dibujo 2D de la lección
            ctx_sin = browser.new_context(viewport={"width": 1366, "height": 860})
            ctx_sin.add_init_script("""
              const orig = HTMLCanvasElement.prototype.getContext;
              HTMLCanvasElement.prototype.getContext = function (tipo, ...r) {
                if (/webgl/i.test(String(tipo))) return null;
                return orig.call(this, tipo, ...r);
              };
            """)
            ps = ctx_sin.new_page()
            ps.goto(url, wait_until="domcontentloaded")
            ps.wait_for_function("() => window.__tutor && window.__tutor.banco && window.__tutor.banco.listo", timeout=30000)
            ps.evaluate("window.__tutor.acciones.abrir('Q108', { corte: '20.5' })")
            ps.wait_for_selector(".apoyo-figura .visual canvas", timeout=15000)
            ps.wait_for_timeout(300)
            self.assertEqual(ps.locator(".apoyo-figura .visual-3d").count(), 0, "R10: sin WebGL no debe quedar la escena 3D vacía")
            self.assertGreater(ps.locator(".apoyo-figura .visual canvas").count(), 0, "R10: sin WebGL falta el dibujo 2D")
            ctx_sin.close()

            browser.close()

        self.assertEqual(errores_js, [], f"Errores JS/Consola: {errores_js}")


if __name__ == "__main__":
    unittest.main()
