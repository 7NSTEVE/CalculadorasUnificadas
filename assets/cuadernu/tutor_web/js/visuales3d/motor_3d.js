/**
 * Motor 3D WebGL2 offline (Three.js r187 local, cero CDN) para el Tutor de Física III.
 *
 * Características:
 *  - Iluminación de estudio limpia estilo guías de interfaz sincronizada con el tema (Claro, Oscuro, Contraste).
 *  - Cámara orbital suave con arrastre de puntero, rueda/zoom, teclado y 5 vistas en 1 clic
 *    (3D, Frontal, Lateral, Superior, Corte transversal real con THREE.Plane).
 *  - Raycasting al tocar piezas 3D (distingue clic de arrastre para no interferir al girar).
 *  - Proyección 3D -> 2D para etiquetas HTML/SVG nítidas a cualquier DPI (4K).
 *  - Limpieza estricta de geometrías/materiales GPU y degradación segura si WebGL2 no existe.
 */

import * as THREE from '../../vendor/three.module.js';
import { menosMovimiento } from '../nucleo/api.js';
import { alCambiarTema, color, paleta, temaActual } from '../nucleo/paleta.js';

export { THREE };

export const VISTAS_3D = {
  perspectiva: { azimut: 0.56, elevacion: 0.34, distancia: 6.2, corte: false, nombre: '3D' },
  frontal:     { azimut: 0.0,  elevacion: 0.02, distancia: 6.0, corte: false, nombre: 'Frontal' },
  lateral:     { azimut: Math.PI / 2, elevacion: 0.04, distancia: 6.0, corte: false, nombre: 'Lateral' },
  superior:    { azimut: 0.01, elevacion: 1.44, distancia: 6.4, corte: false, nombre: 'Superior' },
  corte:       { azimut: 0.34, elevacion: 0.22, distancia: 5.5, corte: true,  nombre: 'Corte' },
};

// Tope de inclinación (R1): de casi cenital a un poco bajo el horizonte; la escena nunca se da vuelta.
const LIM_ELEV = 1.48;
const LIM_ELEV_MIN = -0.42;
const DIST_MIN = 2.2;
const DIST_MAX = 13.5;
// Doble toque (R1): dos toques cortos en menos de este tiempo y a menos de esta distancia.
const DOBLE_MS = 340;
const DOBLE_PX = 28;

const acotar = (x, a, b) => Math.max(a, Math.min(b, x));

export function hexColor(nombre, porDefecto = 'tinta') {
  return new THREE.Color(color(nombre, porDefecto));
}

export function crearMaterial(nombreColor, {
  opacidad = 1,
  metal = 0.08,
  rugosidad = 0.52,
  corte = null,
  dobleCara = false,
  emisivo = null,
  intensidadEmisiva = 0.18,
  wireframe = false,
} = {}) {
  const op = {
    color: hexColor(nombreColor),
    metalness: metal,
    roughness: rugosidad,
    transparent: opacidad < 0.995,
    opacity: opacidad,
    // R4: lo muy translúcido (vidrio del tanque, líquido, paredes) no escribe profundidad: si no,
    // su cara de adelante tapaba lo que está adentro (la esfera de RP-26I-3 no se veía).
    depthWrite: opacidad >= 0.6,
    side: dobleCara ? THREE.DoubleSide : THREE.FrontSide,
    wireframe,
  };
  if (corte) {
    op.clippingPlanes = Array.isArray(corte) ? corte : [corte];
    op.clipShadows = true;
  }
  if (emisivo) {
    op.emissive = hexColor(emisivo);
    op.emissiveIntensity = intensidadEmisiva;
  }
  return new THREE.MeshStandardMaterial(op);
}

/** Flecha 3D limpia (cilindro + cono) orientada de `origen` a `destino`. */
export function crearFlecha3D(origen, destino, nombreColor = 'azul', {
  radio = 0.042,
  punta = 0.22,
  opacidad = 1,
  info = null,
} = {}) {
  const a = new THREE.Vector3(...origen);
  const b = new THREE.Vector3(...destino);
  const dir = new THREE.Vector3().subVectors(b, a);
  const largo = dir.length();
  const g = new THREE.Group();
  if (largo < 1e-4) return g;

  const hCono = Math.min(punta, largo * 0.48);
  const hTallo = Math.max(0.01, largo - hCono);
  const mat = crearMaterial(nombreColor, { opacidad, rugosidad: 0.42, metal: 0.1 });

  const tallo = new THREE.Mesh(new THREE.CylinderGeometry(radio, radio, hTallo, 12), mat);
  tallo.position.y = hTallo / 2;
  const cono = new THREE.Mesh(new THREE.ConeGeometry(radio * 2.5, hCono, 14), mat);
  cono.position.y = hTallo + hCono / 2;

  if (info) {
    tallo.userData.info = info;
    cono.userData.info = info;
  }
  g.add(tallo, cono);
  g.position.copy(a);
  g.quaternion.setFromUnitVectors(new THREE.Vector3(0, 1, 0), dir.normalize());
  return g;
}

/** Resorte helicoidal 3D real mediante TubeGeometry entre `pA` y `pB`. */
export function crearResorte3D(pA, pB, {
  vueltas = 9,
  radioHelice = 0.22,
  radioTubo = 0.036,
  nombreColor = 'tinta2',
  segmentos = 120,
  info = null,
} = {}) {
  const a = new THREE.Vector3(...pA);
  const b = new THREE.Vector3(...pB);
  const eje = new THREE.Vector3().subVectors(b, a);
  const L = eje.length();
  if (L < 0.05) return new THREE.Group();
  const dir = eje.clone().normalize();
  const ref = Math.abs(dir.y) > 0.9 ? new THREE.Vector3(1, 0, 0) : new THREE.Vector3(0, 1, 0);
  const u = new THREE.Vector3().crossVectors(dir, ref).normalize();
  const v = new THREE.Vector3().crossVectors(dir, u).normalize();

  const pts = [];
  for (let i = 0; i <= segmentos; i += 1) {
    const t = i / segmentos;
    // Extremos rectos cortos para anclar limpio al muro y al bloque
    const env = t < 0.06 ? t / 0.06 : t > 0.94 ? (1 - t) / 0.06 : 1;
    const ang = t * vueltas * Math.PI * 2;
    const p = a.clone()
      .addScaledVector(dir, t * L)
      .addScaledVector(u, Math.cos(ang) * radioHelice * env)
      .addScaledVector(v, Math.sin(ang) * radioHelice * env);
    pts.push(p);
  }
  const curva = new THREE.CatmullRomCurve3(pts);
  const geo = new THREE.TubeGeometry(curva, segmentos, radioTubo, 8, false);
  const mat = crearMaterial(nombreColor, { metal: 0.45, rugosidad: 0.35 });
  const mesh = new THREE.Mesh(geo, mat);
  if (info) mesh.userData.info = info;
  return mesh;
}

/** Línea 3D (continua o punteada) para ejes, referencias y cotas geométricas. */
export function crearLinea3D(puntos, nombreColor = 'tinta3', { punteada = false, opacidad = 0.85 } = {}) {
  const vecs = puntos.map((p) => new THREE.Vector3(...p));
  const geo = new THREE.BufferGeometry().setFromPoints(vecs);
  const col = hexColor(nombreColor);
  const mat = punteada
    ? new THREE.LineDashedMaterial({ color: col, dashSize: 0.12, gapSize: 0.08, transparent: opacidad < 1, opacity: opacidad })
    : new THREE.LineBasicMaterial({ color: col, transparent: opacidad < 1, opacity: opacidad });
  const line = new THREE.Line(geo, mat);
  if (punteada) line.computeLineDistances();
  return line;
}

/** Grilla arquitectónica sutil para dar referencia espacial sin ruido visual. */
export function crearPisoSutil(y = -1.75, tam = 6.5, divisiones = 10) {
  const c1 = hexColor('borde');
  const c2 = hexColor('punto');
  const grid = new THREE.GridHelper(tam, divisiones, c1, c2);
  grid.position.y = y;
  if (Array.isArray(grid.material)) {
    grid.material.forEach((m) => { m.transparent = true; m.opacity = 0.36; });
  } else {
    grid.material.transparent = true;
    grid.material.opacity = 0.36;
  }
  return grid;
}

/** Libera recursivamente geometrías y materiales WebGL para evitar fugas de memoria GPU. */
export function disponerObjeto(obj) {
  if (!obj) return;
  obj.traverse((n) => {
    if (n.geometry && typeof n.geometry.dispose === 'function') n.geometry.dispose();
    if (n.material) {
      const mats = Array.isArray(n.material) ? n.material : [n.material];
      for (const m of mats) {
        if (m && typeof m.dispose === 'function') m.dispose();
      }
    }
  });
}

export class Motor3D {
  /**
   * alInteractuar: se llama en cada giro, zoom o tecla, para que el visor vuelva a dibujar
   *   (con la escena en pausa no hay bucle de animación que lo haga).
   * alDobleToque: doble toque, doble clic o tecla Inicio → volver a la vista del enunciado.
   */
  constructor(canvas, {
    alCambiarVista = null, alTocarPieza = null, alInteractuar = null, alDobleToque = null,
  } = {}) {
    this.canvas = canvas;
    this.alCambiarVista = alCambiarVista;
    this.alTocarPieza = alTocarPieza;
    this.alInteractuar = alInteractuar;
    this.alDobleToque = alDobleToque;
    this.fallback2d = false;
    this.renderer = null;
    this.escena = new THREE.Scene();
    this.raiz = new THREE.Group();
    this.escena.add(this.raiz);

    this.camara = new THREE.PerspectiveCamera(38, 16 / 10, 0.1, 100);
    this.objetivo = new THREE.Vector3(0, 0, 0);
    this.metaObjetivo = new THREE.Vector3(0, 0, 0);
    this.estadoCam = { ...VISTAS_3D.perspectiva };
    this.metaCam = { ...VISTAS_3D.perspectiva };
    this.vistaActual = 'perspectiva';

    // Plano de corte real en Z=0 (corta la mitad frontal para revelar el interior en vista «Corte»)
    this.planoCorte = new THREE.Plane(new THREE.Vector3(0, 0, -1), 0.001);
    this.corteActivo = false;

    this.raycaster = new THREE.Raycaster();
    this._puntero = new THREE.Vector2();
    this._ancho = 560;
    this._alto = 340;
    this._vivo = true;

    this._iniciarRenderer();
    this._armarLuces();
    this._aplicarTema();
    this._conectarGestos();
    this._quitaTema = alCambiarTema(() => this._aplicarTema());
    this._actualizarCamara(1);
  }

  _iniciarRenderer() {
    try {
      const ctx = this.canvas.getContext('webgl2', { antialias: true, alpha: true, preserveDrawingBuffer: true })
        || this.canvas.getContext('webgl', { antialias: true, alpha: true, preserveDrawingBuffer: true });
      if (!ctx) throw new Error('WebGL no disponible');
      this.renderer = new THREE.WebGLRenderer({
        canvas: this.canvas,
        context: ctx,
        antialias: true,
        alpha: true,
        preserveDrawingBuffer: true,
      });
      this.renderer.localClippingEnabled = true;
      this.renderer.outputColorSpace = THREE.SRGBColorSpace;
      this.renderer.toneMapping = THREE.ACESFilmicToneMapping;
      this.renderer.toneMappingExposure = 1.05;
      this.esWebGL2 = typeof WebGL2RenderingContext !== 'undefined' && ctx instanceof WebGL2RenderingContext;
      this.canvas.dataset.motor3d = this.esWebGL2 ? 'webgl2' : 'webgl1';
    } catch {
      this.fallback2d = true;
      this.esWebGL2 = false;
      this.canvas.dataset.motor3d = 'fallback2d';
    }
  }

  _armarLuces() {
    this.luzHemi = new THREE.HemisphereLight(0xffffff, 0xcbd5e1, 1.05);
    this.luzClave = new THREE.DirectionalLight(0xffffff, 1.15);
    this.luzClave.position.set(4.5, 6.2, 5.4);
    this.luzRelleno = new THREE.DirectionalLight(0xdbeafe, 0.55);
    this.luzRelleno.position.set(-5.0, 2.5, -3.5);
    this.escena.add(this.luzHemi, this.luzClave, this.luzRelleno);
  }

  _aplicarTema() {
    const modo = temaActual();
    const pal = paleta();
    if (this.renderer) {
      const bg = modo === 'light' ? pal.papel : modo === 'dark' ? '#161b24' : '#000000';
      this.renderer.setClearColor(new THREE.Color(bg), 1);
    }
    if (this.luzHemi) {
      this.luzHemi.intensity = modo === 'light' ? 1.12 : modo === 'dark' ? 0.95 : 1.25;
      this.luzClave.intensity = modo === 'light' ? 1.15 : 1.28;
    }
  }

  /** Devuelve el arreglo de clippingPlanes activo para materiales que soporten corte transversal. */
  planosCorte() {
    return this.corteActivo ? [this.planoCorte] : [];
  }

  establecerCorte(activo, desplazamientoZ = 0.001) {
    this.corteActivo = Boolean(activo);
    this.planoCorte.constant = desplazamientoZ;
    this.canvas.dataset.corte = this.corteActivo ? '1' : '0';
  }

  redimensionar(anchoCss, altoCss) {
    const w = Math.max(120, Math.round(anchoCss));
    const h = Math.max(120, Math.round(altoCss));
    this._ancho = w;
    this._alto = h;
    this.camara.aspect = w / h;
    this.camara.updateProjectionMatrix();
    const dpr = Math.min(2.5, window.devicePixelRatio || 1);
    if (this.renderer) {
      this.renderer.setPixelRatio(dpr);
      this.renderer.setSize(w, h, false);
    } else {
      this.canvas.width = Math.round(w * dpr);
      this.canvas.height = Math.round(h * dpr);
    }
  }

  irAVista(id, { objetivo = null, distancia = null, inmediato = false } = {}) {
    const preset = VISTAS_3D[id] || VISTAS_3D.perspectiva;
    this.vistaActual = id;
    this.metaCam = {
      azimut: preset.azimut,
      elevacion: preset.elevacion,
      distancia: distancia ?? preset.distancia,
    };
    if (objetivo) this.metaObjetivo.set(...objetivo);
    else this.metaObjetivo.set(0, 0, 0);
    this.establecerCorte(preset.corte);
    if (inmediato || menosMovimiento()) this._actualizarCamara(1);
    this.canvas.dataset.vista = id;
    if (this.alCambiarVista) this.alCambiarVista(id);
  }

  enfocar({ azimut, elevacion, distancia, objetivo, corte } = {}) {
    if (typeof azimut === 'number') this.metaCam.azimut = azimut;
    if (typeof elevacion === 'number') this.metaCam.elevacion = acotar(elevacion, LIM_ELEV_MIN, LIM_ELEV);
    if (typeof distancia === 'number') this.metaCam.distancia = acotar(distancia, DIST_MIN, DIST_MAX);
    if (Array.isArray(objetivo) && objetivo.length === 3) this.metaObjetivo.set(...objetivo);
    if (typeof corte === 'boolean') this.establecerCorte(corte);
    if (menosMovimiento()) this._actualizarCamara(1);
  }

  limpiarEscena() {
    while (this.raiz.children.length) {
      const hijo = this.raiz.children[0];
      this.raiz.remove(hijo);
      disponerObjeto(hijo);
    }
  }

  _actualizarCamara(alpha = 0.16) {
    const k = Math.max(0, Math.min(1, alpha));
    this.estadoCam.azimut += (this.metaCam.azimut - this.estadoCam.azimut) * k;
    this.estadoCam.elevacion += (this.metaCam.elevacion - this.estadoCam.elevacion) * k;
    this.estadoCam.distancia += (this.metaCam.distancia - this.estadoCam.distancia) * k;
    this.objetivo.lerp(this.metaObjetivo, k);

    const { azimut, elevacion, distancia } = this.estadoCam;
    const ce = Math.cos(elevacion);
    const x = this.objetivo.x + distancia * ce * Math.sin(azimut);
    const y = this.objetivo.y + distancia * Math.sin(elevacion);
    const z = this.objetivo.z + distancia * ce * Math.cos(azimut);
    this.camara.position.set(x, y, z);
    this.camara.lookAt(this.objetivo);
    this.camara.updateMatrixWorld();
  }

  /** ¿La cámara todavía está volando suavemente hacia su meta? */
  camaraEnMovimiento() {
    return (
      Math.abs(this.metaCam.azimut - this.estadoCam.azimut) > 0.002 ||
      Math.abs(this.metaCam.elevacion - this.estadoCam.elevacion) > 0.002 ||
      Math.abs(this.metaCam.distancia - this.estadoCam.distancia) > 0.005 ||
      this.objetivo.distanceToSquared(this.metaObjetivo) > 0.0001
    );
  }

  /** Proyecta un punto 3D [x,y,z] a coordenadas CSS dentro del contenedor del lienzo. */
  proyectarPantalla(pos3) {
    const v = new THREE.Vector3(...pos3);
    v.project(this.camara);
    return {
      x: (v.x * 0.5 + 0.5) * this._ancho,
      y: (-v.y * 0.5 + 0.5) * this._alto,
      visible: v.z >= -1 && v.z <= 1,
    };
  }

  renderizar() {
    if (!this._vivo) return;
    this._actualizarCamara(menosMovimiento() ? 1 : 0.18);
    // Estado de la cámara a la vista (azimut, elevación, distancia): lo leen las pruebas de R1–R3.
    const { azimut, elevacion, distancia } = this.estadoCam;
    this.canvas.dataset.cam = `${azimut.toFixed(3)},${elevacion.toFixed(3)},${distancia.toFixed(3)}`;
    if (this.renderer) {
      this.renderer.render(this.escena, this.camara);
    } else if (this.fallback2d) {
      const ctx = this.canvas.getContext('2d');
      if (ctx) {
        const pal = paleta();
        ctx.save();
        ctx.setTransform(1, 0, 0, 1, 0, 0);
        ctx.fillStyle = pal.papel;
        ctx.fillRect(0, 0, this.canvas.width, this.canvas.height);
        ctx.fillStyle = pal.tinta2;
        ctx.font = '600 15px system-ui, sans-serif';
        ctx.textAlign = 'center';
        ctx.fillText('Este equipo no puede mostrar 3D: mirá el dibujo 2D o el enunciado', this.canvas.width / 2, this.canvas.height / 2);
        ctx.restore();
      }
    }
  }

  _avisar() {
    if (this.alInteractuar) this.alInteractuar();
  }

  _marcarLibre() {
    if (this.vistaActual === 'libre') return;
    this.vistaActual = 'libre';
    this.canvas.dataset.vista = 'libre';
    if (this.alCambiarVista) this.alCambiarVista('libre');
  }

  /**
   * Gestos (R1, R3, R9): arrastrar gira (mouse, lápiz o dedo), dos dedos pellizcan para el zoom,
   * la rueda acerca, las flechas orbitan, + y − acercan, Inicio o doble toque vuelven a la vista
   * del enunciado. Cada gesto avisa al visor para que dibuje aunque la escena esté en pausa.
   */
  _conectarGestos() {
    const c = this.canvas;
    const punteros = new Map();
    let arrastre = null;
    let pellizco = null;
    let ultimoToque = { t: -1e9, x: 0, y: 0 };

    const empezarArrastre = (id, x, y) => {
      arrastre = { id, x0: x, y0: y, movido: 0, az: this.metaCam.azimut, el: this.metaCam.elevacion };
    };

    this._onDown = (ev) => {
      if (ev.pointerType === 'mouse' && ev.button !== 0) return;
      // Si el puntero no se puede capturar (algunos lápices, eventos sintéticos), el gesto sigue igual.
      try { c.setPointerCapture?.(ev.pointerId); } catch { /* sin captura */ }
      punteros.set(ev.pointerId, { x: ev.clientX, y: ev.clientY });
      if (punteros.size === 2) {
        const [a, b] = [...punteros.values()];
        pellizco = { d0: Math.hypot(a.x - b.x, a.y - b.y) || 1, dist0: this.metaCam.distancia };
        arrastre = null;
        return;
      }
      if (punteros.size === 1) empezarArrastre(ev.pointerId, ev.clientX, ev.clientY);
    };

    this._onMove = (ev) => {
      if (!punteros.has(ev.pointerId)) return;
      punteros.set(ev.pointerId, { x: ev.clientX, y: ev.clientY });
      if (pellizco && punteros.size >= 2) {
        const [a, b] = [...punteros.values()];
        const d = Math.hypot(a.x - b.x, a.y - b.y) || 1;
        this.metaCam.distancia = acotar(pellizco.dist0 * (pellizco.d0 / d), DIST_MIN, DIST_MAX);
        this.estadoCam.distancia = this.metaCam.distancia;
        this._avisar();
        return;
      }
      if (!arrastre || ev.pointerId !== arrastre.id) return;
      const dx = ev.clientX - arrastre.x0;
      const dy = ev.clientY - arrastre.y0;
      arrastre.movido = Math.max(arrastre.movido, Math.hypot(dx, dy));
      if (arrastre.movido > 3) {
        this.metaCam.azimut = arrastre.az - dx * 0.010;
        this.metaCam.elevacion = acotar(arrastre.el + dy * 0.0085, LIM_ELEV_MIN, LIM_ELEV);
        this.estadoCam.azimut = this.metaCam.azimut;
        this.estadoCam.elevacion = this.metaCam.elevacion;
        this._marcarLibre();
        this._avisar();
      }
    };

    const soltar = (ev, cancelado) => {
      if (!punteros.has(ev.pointerId)) return;
      punteros.delete(ev.pointerId);
      if (pellizco) {
        // Al levantar un dedo del pellizco no se toma como toque ni se gira de golpe.
        if (punteros.size < 2) pellizco = null;
        arrastre = null;
        if (punteros.size === 1) {
          const [[id, p]] = [...punteros.entries()];
          empezarArrastre(id, p.x, p.y);
          arrastre.movido = 99;
        }
        return;
      }
      if (!arrastre || ev.pointerId !== arrastre.id) return;
      const fueClic = !cancelado && arrastre.movido < 5;
      arrastre = null;
      if (!fueClic) return;
      const ahora = performance.now();
      const esDoble = ahora - ultimoToque.t < DOBLE_MS
        && Math.hypot(ev.clientX - ultimoToque.x, ev.clientY - ultimoToque.y) < DOBLE_PX;
      ultimoToque = esDoble ? { t: -1e9, x: 0, y: 0 } : { t: ahora, x: ev.clientX, y: ev.clientY };
      if (esDoble) {
        if (this.alDobleToque) this.alDobleToque();
        this._avisar();
        return;
      }
      if (this.renderer && this.alTocarPieza) this._inspeccionarEn(ev.clientX, ev.clientY);
    };
    this._onUp = (ev) => soltar(ev, false);
    this._onCancel = (ev) => soltar(ev, true);

    this._onWheel = (ev) => {
      ev.preventDefault();
      const factor = ev.deltaY > 0 ? 1.09 : 0.92;
      this.metaCam.distancia = acotar(this.metaCam.distancia * factor, DIST_MIN, DIST_MAX);
      this._avisar();
    };

    this._onKey = (ev) => {
      if (ev.key === 'Home') {
        ev.preventDefault();
        ev.stopPropagation();
        if (this.alDobleToque) this.alDobleToque();
        this._avisar();
        return;
      }
      const mapa = {
        ArrowLeft: [-0.14, 0, 0],
        ArrowRight: [0.14, 0, 0],
        ArrowUp: [0, 0.12, 0],
        ArrowDown: [0, -0.12, 0],
        '+': [0, 0, -0.4],
        '=': [0, 0, -0.4],
        '-': [0, 0, 0.4],
      };
      const d = mapa[ev.key];
      if (!d) return;
      ev.preventDefault();
      ev.stopPropagation();
      this.metaCam.azimut += d[0];
      this.metaCam.elevacion = acotar(this.metaCam.elevacion + d[1], LIM_ELEV_MIN, LIM_ELEV);
      this.metaCam.distancia = acotar(this.metaCam.distancia + d[2], DIST_MIN, DIST_MAX);
      if (d[0] || d[1]) this._marcarLibre();
      this._avisar();
    };

    c.addEventListener('pointerdown', this._onDown);
    c.addEventListener('pointermove', this._onMove);
    c.addEventListener('pointerup', this._onUp);
    c.addEventListener('pointercancel', this._onCancel);
    c.addEventListener('wheel', this._onWheel, { passive: false });
    c.addEventListener('keydown', this._onKey);
  }

  _inspeccionarEn(clientX, clientY) {
    const r = this.canvas.getBoundingClientRect();
    if (r.width < 10 || r.height < 10) return;
    this._puntero.x = ((clientX - r.left) / r.width) * 2 - 1;
    this._puntero.y = -((clientY - r.top) / r.height) * 2 + 1;
    this.raycaster.setFromCamera(this._puntero, this.camara);
    const hits = this.raycaster.intersectObjects(this.raiz.children, true);
    for (const h of hits) {
      let cur = h.object;
      while (cur) {
        if (cur.userData && cur.userData.info) {
          this.alTocarPieza(cur.userData.info);
          return;
        }
        cur = cur.parent;
      }
    }
    this.alTocarPieza(null);
  }

  destruir() {
    this._vivo = false;
    this._quitaTema?.();
    const c = this.canvas;
    c.removeEventListener('pointerdown', this._onDown);
    c.removeEventListener('pointermove', this._onMove);
    c.removeEventListener('pointerup', this._onUp);
    c.removeEventListener('pointercancel', this._onCancel);
    c.removeEventListener('wheel', this._onWheel);
    c.removeEventListener('keydown', this._onKey);
    this.limpiarEscena();
    if (this.renderer) {
      this.renderer.dispose();
      this.renderer = null;
    }
  }
}
