/**
 * Panel lateral: el visual animado que evoluciona con la explicación, el enunciado a mano y el
 * formulario oficial II-2026 con círculos de colores sincronizados paso a paso.
 *
 * La pizarra manda: el panel se pliega y se abre con el botón «Apoyo» de la barra (en ventanas
 * angostas es un cajón que se abre encima de la pizarra), el visual se ve en grande al tocarlo y el
 * panel nunca se pone solo en «Enunciado», que ya está escrito en la pizarra.
 */

import { TintaMate, TintaTexto } from '../mate/tinta.js';
import { ajustes, bus, guardarAjuste, urlApi } from '../nucleo/api.js';
import { crearVisual } from '../visuales/index.js';

const SVG_NS = 'http://www.w3.org/2000/svg';

/** Por debajo de este ancho el panel es un cajón (debe coincidir con la media query de tutor.css). */
const ANGOSTA = window.matchMedia('(max-width: 760px)');
/**
 * Solo en ventana ancha cabe una columna en la que el visual se lea (≥ 440 px): ahí el panel empieza
 * abierto si hay visual. En ventanas medianas empieza plegado; así no ocupa espacio con un dibujo diminuto.
 */
const ANCHA = window.matchMedia('(min-width: 1101px)');
const ICO_AMPLIAR = '<svg viewBox="0 0 24 24" aria-hidden="true"><path d="M21 11V3h-8l3.29 3.29-10 10L3 13v8h8l-3.29-3.29 10-10L21 11Z"/></svg>';

function svgEl(tag, attrs = {}) {
  const el = document.createElementNS(SVG_NS, tag);
  for (const [k, v] of Object.entries(attrs)) el.setAttribute(k, String(v));
  return el;
}

/** Trazo orgánico de círculo a mano alrededor de una celda del formulario oficial. */
function rutaCirculoMano(x, y, w, h, semilla = 1) {
  const padX = Math.min(12, w * 0.03);
  const padY = Math.min(9, h * 0.08);
  const rx = (w - padX * 2) * 0.505;
  const ry = (h - padY * 2) * 0.515;
  const cx = x + w * 0.5;
  const cy = y + h * 0.5;
  const s = ((semilla * 37) % 11) - 5; // leve variación determinista
  const ox = s * 0.55;
  const oy = ((semilla * 19) % 7 - 3) * 0.45;
  // Óvalo continuo con ligero traslape natural al cerrar (como marcado con piloto de color)
  const x0 = cx - rx * 0.88;
  const y0 = cy - ry * 0.46;
  const topX = cx + ox;
  const topY = cy - ry - 1.5;
  const derX = cx + rx + 2;
  const derY = cy + oy;
  const abaX = cx - ox * 0.8;
  const abaY = cy + ry + 1.8;
  const izqX = cx - rx - 1.5;
  const izqY = cy - oy * 0.7;
  const finX = cx - rx * 0.62;
  const finY = cy - ry * 0.84;
  return [
    `M ${x0.toFixed(1)} ${y0.toFixed(1)}`,
    `C ${(cx - rx * 0.65).toFixed(1)} ${(topY - 2).toFixed(1)}, ${(cx + rx * 0.55).toFixed(1)} ${(topY - 1).toFixed(1)}, ${(cx + rx * 0.88).toFixed(1)} ${(cy - ry * 0.48).toFixed(1)}`,
    `C ${(derX + 3).toFixed(1)} ${(cy - ry * 0.12).toFixed(1)}, ${(derX + 2).toFixed(1)} ${(cy + ry * 0.46).toFixed(1)}, ${(cx + rx * 0.76).toFixed(1)} ${(cy + ry * 0.72).toFixed(1)}`,
    `C ${(cx + rx * 0.38).toFixed(1)} ${(abaY + 2).toFixed(1)}, ${(cx - rx * 0.52).toFixed(1)} ${(abaY + 1).toFixed(1)}, ${(cx - rx * 0.85).toFixed(1)} ${(cy + ry * 0.52).toFixed(1)}`,
    `C ${(izqX - 2.5).toFixed(1)} ${(cy + ry * 0.14).toFixed(1)}, ${(izqX - 1.5).toFixed(1)} ${(cy - ry * 0.58).toFixed(1)}, ${finX.toFixed(1)} ${finY.toFixed(1)}`,
  ].join(' ');
}

function construirHojaSvg(mapa, { interactivo = true, alClic = null } = {}) {
  const [vw, vh] = mapa.aspecto || [1000, 1468];
  const marco = document.createElement('div');
  marco.className = 'hoja-formulario';

  const img = document.createElement('img');
  img.className = 'hoja-formulario-img';
  img.alt = mapa.titulo || 'Formulario oficial del III Parcial II-2026';
  img.src = urlApi(mapa.hoja_url || '/api/formulario/hoja.png');
  marco.appendChild(img);

  const svg = svgEl('svg', {
    class: 'hoja-formulario-svg',
    viewBox: `0 0 ${vw} ${vh}`,
    preserveAspectRatio: 'none',
    role: 'img',
    'aria-label': `Celdas marcadas en el formulario oficial: ${(mapa.celdas || []).map((c) => `${c.num}. ${c.nombre}`).join(', ')}`,
  });

  for (const c of mapa.celdas || []) {
    const [x0, y0, x1, y1] = c.caja;
    const x = x0 * vw;
    const y = y0 * vh;
    const w = (x1 - x0) * vw;
    const h = (y1 - y0) * vh;

    const g = svgEl('g', {
      class: `form-celda form-c-${c.color}`,
      'data-celda': c.id,
    });
    if (interactivo) {
      g.setAttribute('tabindex', '0');
      g.setAttribute('role', 'button');
      g.setAttribute('aria-label', `${c.num}. ${c.nombre} (${c.etiqueta})`);
      if (alClic) {
        g.addEventListener('click', () => alClic(c.id));
        g.addEventListener('keydown', (e) => {
          if (e.key === 'Enter' || e.key === ' ') { e.preventDefault(); alClic(c.id); }
        });
      }
    }

    const fondo = svgEl('rect', {
      class: 'form-fondo',
      x: (x + 5).toFixed(1),
      y: (y + 5).toFixed(1),
      width: Math.max(8, w - 10).toFixed(1),
      height: Math.max(8, h - 10).toFixed(1),
      rx: 12,
    });
    const circulo = svgEl('path', {
      class: 'form-circulo',
      d: rutaCirculoMano(x, y, w, h, c.num),
    });
    const badgeR = 16;
    const bx = x + 15;
    const by = y + 15;
    const badgeCirc = svgEl('circle', {
      class: 'form-badge-fondo',
      cx: bx.toFixed(1),
      cy: by.toFixed(1),
      r: badgeR,
    });
    const badgeTxt = svgEl('text', {
      class: 'form-badge-num',
      x: bx.toFixed(1),
      y: (by + 6).toFixed(1),
      'text-anchor': 'middle',
    });
    badgeTxt.textContent = String(c.num);

    g.append(fondo, circulo, badgeCirc, badgeTxt);
    svg.appendChild(g);
  }

  marco.appendChild(svg);
  return marco;
}

export class Lateral {
  constructor(el) {
    this.el = el;
    this.visual = null;
    this.lec = null;
    this.pasoActual = 0;
    this.tab = 'visual';
    this._hojaEl = null;
    this._leyendaEl = null;
    this._visorForm = null;
    this._dlgVisual = null;
    this._cuerpoVisual = null;
    this._anchoForm = 0;
    // Abierto o plegado. En ventanas anchas se recuerda lo que la persona elige con el botón.
    this.abierto = true;
    this._aMano = false;
    this.boton = document.getElementById('btn-apoyo');
    this.fondo = document.getElementById('apoyo-fondo');
    if (this.boton) this.boton.addEventListener('click', () => this.alternar());
    if (this.fondo) this.fondo.addEventListener('click', () => this.plegar({ devolverFoco: true }));
    ANGOSTA.addEventListener('change', () => this._alCambiarAncho());
    ANCHA.addEventListener('change', () => this._alCambiarAncho());
    this.barra = el.querySelector('.lateral-tabs');
    this.paneles = {
      visual: el.querySelector('#panel-visual'),
      enunciado: el.querySelector('#panel-enunciado'),
      formulas: el.querySelector('#panel-formulas'),
    };
    this.barra.addEventListener('click', (e) => {
      const b = e.target.closest('[data-tab]');
      if (b) this.ver(b.dataset.tab);
    });
    this.barra.addEventListener('keydown', (e) => {
      const bs = [...this.barra.querySelectorAll('[data-tab]:not([hidden])')];
      const i = bs.indexOf(document.activeElement);
      if (i < 0) return;
      const d = e.key === 'ArrowRight' ? 1 : e.key === 'ArrowLeft' ? -1 : 0;
      if (!d) return;
      e.preventDefault();
      const n = bs[(i + d + bs.length) % bs.length];
      n.focus();
      this.ver(n.dataset.tab);
    });
    bus.on('ajuste:letra', () => {
      if (this.lec) {
        this._formulas(this.lec);
        this.resaltarPasoFormulario(this.pasoActual);
      }
    });
    bus.on('leccion:paso', ({ i }) => {
      this.pasoActual = i;
      this.resaltarPasoFormulario(i);
    });
    bus.on('formulario:enfocar', ({ celdaId }) => {
      if (celdaId) this.enfocarFormulario(celdaId);
    });
  }

  ver(tab) {
    this.tab = tab;
    for (const [k, p] of Object.entries(this.paneles)) p.hidden = k !== tab;
    this.barra.querySelectorAll('[data-tab]').forEach((b) => {
      const act = b.dataset.tab === tab;
      b.setAttribute('aria-selected', String(act));
      b.tabIndex = act ? 0 : -1;
    });
    if (tab === 'formulas') this.resaltarPasoFormulario(this.pasoActual);
  }

  /** ¿Hay panel en esta lección? (que esté abierto o plegado lo decide `abrir`). */
  mostrar(si) {
    this.el.hidden = !si;
    document.body.classList.toggle('sin-lateral', !si);
    this._aplicar();
  }

  /** En ventanas angostas el panel es un cajón encima de la pizarra. */
  get esCajon() { return ANGOSTA.matches; }

  /** Abierto por defecto solo si hay un visual que acompañe la explicación (y en ventana ancha). */
  _abiertoPorDefecto() {
    return ANCHA.matches && ajustes.apoyo !== false && Boolean(this.visual);
  }

  /**
   * Abre o pliega el panel. `recordar`: guarda la preferencia (solo en ventana ancha; en ventanas
   * medianas y en el cajón siempre empieza plegado). `foco`: lleva el foco a la pestaña activa.
   */
  abrir(si = true, { recordar = false, foco = false, devolverFoco = false } = {}) {
    this.abierto = Boolean(si);
    if (recordar && ANCHA.matches) guardarAjuste('apoyo', this.abierto);
    this._aplicar();
    if (this.abierto && foco) (this.barra.querySelector('[aria-selected="true"]:not([hidden])') || this.el).focus?.();
    if (!this.abierto && devolverFoco && this.boton && !this.boton.hidden) this.boton.focus();
  }

  plegar(opt = {}) { this.abrir(false, opt); }

  /** Botón «Apoyo» de la barra. */
  alternar() {
    this._aMano = true;
    const abrir = !this.abierto;
    this.abrir(abrir, { recordar: true, foco: abrir && this.esCajon, devolverFoco: !abrir && this.esCajon });
  }

  /** Muestra u oculta el botón «Apoyo» (solo en la pizarra y si la lección tiene panel). */
  actualizarBoton() {
    if (!this.boton) return;
    this.boton.hidden = this.el.hidden || document.body.dataset.vista !== 'pizarra';
    this.boton.setAttribute('aria-pressed', String(!this.el.hidden && this.abierto));
  }

  _aplicar() {
    const visible = !this.el.hidden && this.abierto;
    document.body.classList.toggle('apoyo-oculto', !visible);
    this.actualizarBoton();
    // Las fórmulas se dibujaron con el panel plegado (sin ancho): se rehacen al abrirlo
    if (visible && this.lec && this.paneles.formulas.childElementCount) {
      requestAnimationFrame(() => {
        if (this.lec && Math.abs(this.el.clientWidth - this._anchoForm) > 40) {
          this._formulas(this.lec);
          this.resaltarPasoFormulario(this.pasoActual);
        }
      });
    }
  }

  _alCambiarAncho() {
    if (!this.lec) return;
    // Al angostar la ventana el panel se pliega; al volver a ancha se respeta la preferencia guardada
    this.abierto = this._aMano && ANCHA.matches ? ajustes.apoyo !== false : this._abiertoPorDefecto();
    this._aplicar();
  }

  _boton(tab, visible) {
    const b = this.barra.querySelector(`[data-tab="${tab}"]`);
    if (b) b.hidden = !visible;
  }

  _etiquetaTab(tab, texto) {
    const b = this.barra.querySelector(`[data-tab="${tab}"]`);
    if (b) b.textContent = texto;
  }

  ponerVisual(spec) {
    if (this.visual) { this.visual.destruir(); this.visual = null; }
    const v = crearVisual(spec);
    this.paneles.visual.replaceChildren();
    this._boton('visual', Boolean(v));
    if (!v) {
      if (this._dlgVisual) this._dlgVisual.close();
      if (this.tab === 'visual') this.ver(this._pestanaSinVisual());
      return;
    }
    this.visual = v;
    this._prepararAmpliar(v);
    // Si el visual está en grande, el nuevo también se ve en grande
    (this._dlgVisual ? this._cuerpoVisual : this.paneles.visual).appendChild(v.el);
    this.ver('visual');
    // Un visual que aparece a media lección abre el panel, salvo que la persona lo haya plegado
    if (this.lec && !this.abierto && !this._aMano && this._abiertoPorDefecto()) this.abrir(true);
  }

  /** Pestaña para cuando no hay visual: las fórmulas; el enunciado solo si no hay nada más. */
  _pestanaSinVisual() {
    const f = this.barra.querySelector('[data-tab="formulas"]');
    return f && !f.hidden && this.paneles.formulas.childElementCount ? 'formulas' : 'enunciado';
  }

  /** El dibujo se toca (o Enter) para verlo en grande; un icono discreto en su esquina lo dice. */
  _prepararAmpliar(v) {
    if (!v.canvas || !v.marco) return;
    const lienzo = document.createElement('span');
    lienzo.className = 'visual-lienzo';
    lienzo.tabIndex = 0;
    lienzo.setAttribute('role', 'button');
    lienzo.setAttribute('aria-label', 'Ver el visual en grande');
    lienzo.title = 'Ver en grande';
    v.canvas.replaceWith(lienzo);
    const ico = document.createElement('span');
    ico.className = 'visual-ampliar';
    ico.innerHTML = ICO_AMPLIAR;
    lienzo.append(v.canvas, ico);
    const ir = () => { if (!this._dlgVisual) this.ampliarVisual(); };
    lienzo.addEventListener('click', ir);
    lienzo.addEventListener('keydown', (e) => {
      if (e.key !== 'Enter' && e.key !== ' ') return;
      e.preventDefault();
      e.stopPropagation(); // que el Espacio no pause la lección
      ir();
    });
  }

  /** El mismo visual (vivo, sigue a la explicación) pasa a un diálogo grande y vuelve al cerrarlo. */
  ampliarVisual() {
    if (!this.visual || this._dlgVisual) return;
    const dlg = document.createElement('dialog');
    dlg.className = 'visor visor-visual';
    dlg.setAttribute('aria-label', 'Visual en grande');
    const cab = document.createElement('div');
    cab.className = 'visor-form-cab';
    const tit = document.createElement('strong');
    tit.textContent = 'Visual';
    const cerrar = document.createElement('button');
    cerrar.type = 'button';
    cerrar.className = 'btn';
    cerrar.textContent = 'Cerrar';
    cerrar.addEventListener('click', () => dlg.close());
    cab.append(tit, cerrar);
    const cuerpo = document.createElement('div');
    cuerpo.className = 'visor-visual-cuerpo';
    dlg.append(cab, cuerpo);
    dlg.addEventListener('click', (e) => { if (e.target === dlg) dlg.close(); });
    dlg.addEventListener('close', () => {
      if (this.visual && this.visual.el.parentElement === cuerpo) this.paneles.visual.appendChild(this.visual.el);
      dlg.remove();
      if (this._dlgVisual === dlg) { this._dlgVisual = null; this._cuerpoVisual = null; }
    });
    document.body.appendChild(dlg);
    this._dlgVisual = dlg;
    this._cuerpoVisual = cuerpo;
    cuerpo.appendChild(this.visual.el);
    dlg.showModal();
    cerrar.focus();
  }

  /** `mostrar` = traer la pestaña del visual al frente. */
  cambiarVisual(cambio, mostrar = true) {
    if (!this.visual) return;
    this.visual.cambiar(cambio);
    if (mostrar && this.tab !== 'visual') this.ver('visual');
  }

  /** Subraya un dato sobre el enunciado del panel, a la vez que en la hoja. */
  marcarDato(caja, color = 'naranja') {
    if (!this.capa) return;
    const m = document.createElement('span');
    m.className = 'dato-marca';
    m.style.left = `${(caja.x * 100).toFixed(2)}%`;
    m.style.top = `${((caja.y + caja.h) * 100).toFixed(2)}%`;
    m.style.width = `${(caja.w * 100).toFixed(2)}%`;
    m.style.background = `var(--${color})`;
    this.capa.appendChild(m);
    if (this.tab === 'enunciado') {
      const r = m.getBoundingClientRect();
      const q = this.paneles.enunciado.getBoundingClientRect();
      if (r.top > q.bottom - 30 || r.top < q.top + 20) m.scrollIntoView({ block: 'center', behavior: 'smooth' });
    }
  }

  quitarDatos() {
    if (this.capa) this.capa.querySelectorAll('.dato-marca').forEach((m) => m.remove());
  }

  abrirLeccion(lec) {
    this.lec = null; // mientras se arma, ponerVisual no decide si abrir o plegar
    this.pasoActual = 0;
    this._aMano = false;
    this.ponerVisual(lec.visual);
    this.lec = lec;
    // Abierto solo si hay un visual que acompañe (en ventana ancha y si la persona no lo plegó antes)
    this.abierto = this._abiertoPorDefecto();
    this.mostrar(true); // con su ancho real antes de dibujar las fórmulas
    this._enunciado(lec);
    this._formulas(lec);
    this.resaltarPasoFormulario(0);
    // El enunciado ya está escrito en la pizarra: el panel empieza en el visual o en las fórmulas
    this.ver(this.visual ? 'visual' : this._pestanaSinVisual());
    if (![...this.barra.querySelectorAll('[data-tab]')].some((b) => !b.hidden)) this.mostrar(false);
  }

  _enunciado(lec) {
    const p = this.paneles.enunciado;
    p.replaceChildren();
    this.capa = null;
    if (lec.recorte && lec.recorte.url) {
      const marco = document.createElement('div');
      marco.className = 'lateral-recorte';
      const img = document.createElement('img');
      img.alt = 'Enunciado del problema';
      img.src = urlApi(lec.recorte.url);
      img.addEventListener('click', () => bus.emit('ver-imagen', { src: img.src, alt: img.alt }));
      marco.appendChild(img);
      p.appendChild(marco);
      this.capa = marco;
    }
    if (lec.enunciado && !lec.recorte) {
      const t = new TintaTexto(lec.enunciado, { clase: 'lateral-enunciado', px: 19 });
      t.progreso(1);
      p.appendChild(t.el);
    }
    this._boton('enunciado', Boolean(p.childElementCount));
  }

  _formulas(lec) {
    const p = this.paneles.formulas;
    p.replaceChildren();
    this._hojaEl = null;
    this._leyendaEl = null;
    this._anchoForm = this.el.clientWidth;

    const mapa = lec.formulario_mapa;
    if (mapa && Array.isArray(mapa.celdas)) {
      const n = mapa.celdas.length;
      this._etiquetaTab('formulas', n > 0 ? `Fórmulas (${n})` : 'Fórmulas');

      const cab = document.createElement('div');
      cab.className = 'form-cab';
      const info = document.createElement('div');
      info.className = 'form-cab-info';
      const tit = document.createElement('strong');
      tit.textContent = mapa.titulo || 'Formulario oficial II-2026';
      const sub = document.createElement('span');
      sub.textContent = n === 0
        ? 'Hoja de referencia del examen'
        : n === 1
          ? '1 celda marcada en la hoja'
          : `${n} celdas marcadas por color`;
      info.append(tit, sub);

      const accCab = document.createElement('div');
      accCab.className = 'form-cab-acc';

      if (!mapa.parcial || mapa.parcial === 3) {
        const btnEst = document.createElement('button');
        btnEst.type = 'button';
        btnEst.className = 'btn btn-suave form-btn-zoom';
        btnEst.textContent = 'Estudiar';
        btnEst.title = 'Abrir la guía interactiva de esta fórmula';
        btnEst.addEventListener('click', () => {
          const pasoIds = (mapa.por_paso && mapa.por_paso[this.pasoActual]) || [];
          const celdaId = pasoIds[0] || (mapa.celdas[0] && mapa.celdas[0].id) || null;
          bus.emit('formulario:estudiar', { celdaId });
        });
        accCab.append(btnEst);
      }

      const btnZoom = document.createElement('button');
      btnZoom.type = 'button';
      btnZoom.className = 'btn btn-suave form-btn-zoom';
      btnZoom.textContent = 'Ampliar';
      btnZoom.addEventListener('click', () => this._abrirHojaGrande(mapa));

      accCab.append(btnZoom);
      cab.append(info, accCab);
      p.appendChild(cab);

      const hoja = construirHojaSvg(mapa, {
        interactivo: true,
        alClic: (cid) => this._enfocarLeyenda(cid),
      });
      this._hojaEl = hoja;
      p.appendChild(hoja);

      if (n > 0) {
        const lista = document.createElement('div');
        lista.className = 'form-leyenda';
        const maxW = Math.max(180, (this.el.clientWidth || 380) - 96);
        for (const c of mapa.celdas) {
          const item = document.createElement('button');
          item.type = 'button';
          item.className = `form-item form-c-${c.color}`;
          item.dataset.celda = c.id;

          const num = document.createElement('span');
          num.className = 'form-item-num';
          num.textContent = String(c.num);

          const cuerpo = document.createElement('span');
          cuerpo.className = 'form-item-cuerpo';

          const lineaTop = document.createElement('span');
          lineaTop.className = 'form-item-top';
          const nom = document.createElement('strong');
          nom.textContent = c.nombre;
          const ubic = document.createElement('span');
          ubic.className = 'form-item-ubic';
          const pasosTxt = (c.pasos || []).map((idx) => idx + 1).join(', ');
          ubic.textContent = pasosTxt ? `${c.id} · Paso ${pasosTxt}` : `${c.id} · ${c.etiqueta}`;
          lineaTop.append(nom, ubic);
          cuerpo.appendChild(lineaTop);

          const eq = new TintaMate(c.tex, { px: 22, color: c.color, maxAncho: maxW });
          eq.progreso(1);
          eq.el.classList.add('form-item-eq');
          cuerpo.appendChild(eq.el);

          if (c.derivado) {
            const der = document.createElement('span');
            der.className = 'form-item-deriv';
            der.textContent = c.derivado;
            cuerpo.appendChild(der);
          }

          // Qué es cada letra de esta fórmula, con sus unidades
          if (Array.isArray(c.simbolos) && c.simbolos.length) {
            const dl = document.createElement('span');
            dl.className = 'form-item-sim';
            for (const [s, que, u] of c.simbolos) {
              const fila = document.createElement('span');
              fila.className = 'form-sim-fila';
              const k = s.indexOf('_');
              const b = document.createElement('i');
              b.textContent = k < 0 ? s : s.slice(0, k);
              const simb = document.createElement('span');
              simb.className = 'form-sim-s';
              simb.append(b);
              if (k >= 0) { const sub = document.createElement('sub'); sub.textContent = s.slice(k + 1); simb.append(sub); }
              const txt = document.createElement('span');
              txt.textContent = u ? `${que} · ${u}` : que;
              fila.append(simb, txt);
              dl.appendChild(fila);
            }
            cuerpo.appendChild(dl);
          }

          item.append(num, cuerpo);
          item.addEventListener('click', () => this.enfocarFormulario(c.id));
          lista.appendChild(item);
        }
        this._leyendaEl = lista;
        p.appendChild(lista);
      }

      if (Array.isArray(mapa.fuera_de_hoja) && mapa.fuera_de_hoja.length) {
        for (const av of mapa.fuera_de_hoja) {
          const box = document.createElement('div');
          box.className = 'form-fuera';
          const st = document.createElement('strong');
          st.textContent = av.tema || 'Fuera de la hoja II-2026';
          const tx = document.createElement('span');
          tx.textContent = av.nota || '';
          box.append(st, tx);
          p.appendChild(box);
        }
      }

      this._boton('formulas', true);
      return;
    }

    this._etiquetaTab('formulas', 'Fórmulas');
    for (const f of lec.formulas || []) {
      const t = new TintaMate(f, { px: 30, color: 'azul', maxAncho: Math.max(200, this.el.clientWidth - 60) });
      t.progreso(1);
      p.appendChild(t.el);
    }
    this._boton('formulas', Boolean(p.childElementCount));
  }

  /** Sincroniza los círculos del formulario con el paso actual de la pizarra. */
  resaltarPasoFormulario(pasoIdx) {
    const mapa = this.lec && this.lec.formulario_mapa;
    if (!mapa || !this._hojaEl) return;
    const ids = (mapa.por_paso && mapa.por_paso[pasoIdx]) || [];
    const activas = new Set(ids);
    const hayActivas = activas.size > 0;

    this._hojaEl.querySelectorAll('.form-celda').forEach((g) => {
      const act = activas.has(g.dataset.celda);
      g.classList.toggle('activa', act);
      g.classList.toggle('atenuada', hayActivas && !act);
    });
    if (this._leyendaEl) {
      this._leyendaEl.querySelectorAll('.form-item').forEach((it) => {
        const act = activas.has(it.dataset.celda);
        it.classList.toggle('activa', act);
        it.classList.toggle('atenuada', hayActivas && !act);
      });
    }
  }

  /** Abre la pestaña Fórmulas, enfoca la celda circulada y la desplaza al centro. */
  enfocarFormulario(celdaId) {
    if (!this._hojaEl) return;
    if (this.el.hidden) this.mostrar(true);
    if (!this.abierto) this.abrir(true); // lo pidió la persona: se abre (sin cambiar su preferencia)
    if (this.tab !== 'formulas') this.ver('formulas');
    this._hojaEl.querySelectorAll('.form-celda').forEach((g) => {
      const es = g.dataset.celda === celdaId;
      g.classList.toggle('enfocada', es);
      g.classList.toggle('activa', es);
      g.classList.toggle('atenuada', !es);
    });
    if (this._leyendaEl) {
      this._leyendaEl.querySelectorAll('.form-item').forEach((it) => {
        const es = it.dataset.celda === celdaId;
        it.classList.toggle('enfocada', es);
        it.classList.toggle('activa', es);
        it.classList.toggle('atenuada', !es);
      });
    }
    const target = this._hojaEl.querySelector(`.form-celda[data-celda="${CSS.escape(celdaId)}"]`);
    if (target) {
      target.scrollIntoView({ block: 'center', behavior: 'smooth' });
    }
  }

  _enfocarLeyenda(celdaId) {
    this.enfocarFormulario(celdaId);
    if (!this._leyendaEl) return;
    const it = this._leyendaEl.querySelector(`.form-item[data-celda="${CSS.escape(celdaId)}"]`);
    if (it) it.scrollIntoView({ block: 'nearest', behavior: 'smooth' });
  }

  _abrirHojaGrande(mapa) {
    if (this._visorForm) this._visorForm.remove();
    const dlg = document.createElement('dialog');
    dlg.className = 'visor visor-formulario';
    const cab = document.createElement('div');
    cab.className = 'visor-form-cab';
    const tit = document.createElement('strong');
    tit.textContent = mapa.titulo || 'Formulario oficial · III Parcial FI2103 II-2026';
    const cerrar = document.createElement('button');
    cerrar.type = 'button';
    cerrar.className = 'btn';
    cerrar.textContent = 'Cerrar';
    cerrar.addEventListener('click', () => dlg.close());
    cab.append(tit, cerrar);

    const cuerpo = document.createElement('div');
    cuerpo.className = 'visor-form-cuerpo';
    const hoja = construirHojaSvg(mapa, { interactivo: false });
    const ids = (mapa.por_paso && mapa.por_paso[this.pasoActual]) || [];
    const activas = new Set(ids);
    if (activas.size) {
      hoja.querySelectorAll('.form-celda').forEach((g) => {
        g.classList.toggle('activa', activas.has(g.dataset.celda));
      });
    }
    cuerpo.appendChild(hoja);
    dlg.append(cab, cuerpo);
    dlg.addEventListener('click', (e) => { if (e.target === dlg) dlg.close(); });
    dlg.addEventListener('close', () => { dlg.remove(); if (this._visorForm === dlg) this._visorForm = null; });
    document.body.appendChild(dlg);
    this._visorForm = dlg;
    dlg.showModal();
  }

  cerrar() {
    if (this._dlgVisual) this._dlgVisual.close();
    if (this.visual) { this.visual.destruir(); this.visual = null; }
    if (this._visorForm) { this._visorForm.close(); this._visorForm = null; }
    this.lec = null;
    this._hojaEl = null;
    this._leyendaEl = null;
    this._etiquetaTab('formulas', 'Fórmulas');
    for (const p of Object.values(this.paneles)) p.replaceChildren();
    this.mostrar(false);
  }
}
