/**
 * El enunciado pegado en la pizarra: el recorte del documento original (o el texto, si el
 * documento no está a mano) y, en selección única, las opciones.
 */

import { TintaTexto } from '../mate/tinta.js';
import { bus, urlApi } from '../nucleo/api.js';

export function normalizaDato(texto) {
  const literal = String(texto || '').trim();
  return literal.replace(/(^|\s)[“„](?=\s|$)/g, '$1=')
    .replace(/´/g, '').replace(/ı/g, 'i').replace(/[º˝˚◦](?=\s*[CF]\b)/g, '°').normalize('NFKD')
    .replace(/\b([A-Za-z])\{(\d+)(?![\d}])/g, '$1/$2')
    .replace(/\\?\^\{?\\circ\}?(?=\s*(?:\\text\{)?[CF])/g, '°')
    .replace(/\p{M}/gu, '').replace(/[‐-―−]/g, '-')
    .replace(/\\(?:d|t)?frac\s*\{([^{}]*)\}\s*\{([^{}]*)\}/g, '$1/$2')
    .replace(/\\(?:text|mathrm|mathit|operatorname|left|right)\b/g, '')
    .replace(/\\(?:cdot|times)\b/g, '×').replace(/[·⋅]/g, '×')
    .replace(/\\(?:circ|degree)\b/g, '°').replace(/\\[,;:! ]/g, '').replace(/\\(%)/g, '$1')
    .replace(/[{}$^]/g, '').replace(/(?<=\d)[,.]\s*(?=\d)/g, '.').toLowerCase()
    .replace(/^[\s([«“"¿¡]+|[\s)\]»”".,;:?!]+$/g, '').replace(/\s+/g, '');
}

/** Busca un tramo completo, aun cuando el PDF separa un número de la unidad. */
function buscaDato(frase, palabras, { ocurrencia, contexto } = {}) {
  const buscado = normalizaDato(frase);
  if (!buscado) return [];
  const tokens = palabras.map((p) => normalizaDato(p.t));
  const ancla = normalizaDato(contexto);
  const candidatas = [];
  for (let i = 0; i < tokens.length; i += 1) {
    if (!tokens[i]) continue;
    let texto = '';
    for (let k = i; k < tokens.length; k += 1) {
      texto += ' ' + palabras[k].t;
      const junto = normalizaDato(texto);
      if (!buscado.startsWith(junto)) break;
      if (junto !== buscado) continue;
      if (!ancla || tokens.slice(Math.max(0, i - 8), k + 9).join('').includes(ancla)) {
        candidatas.push(palabras.slice(i, k + 1));
      }
      break;
    }
  }
  if (Number.isInteger(ocurrencia) && ocurrencia > 0) return candidatas[ocurrencia - 1] || [];
  return candidatas.length === 1 ? candidatas[0] : [];
}

function cajasPorLinea(rectangulos) {
  const lineas = [];
  for (const r of rectangulos) {
    const centro = r.y + r.h / 2;
    const linea = lineas.find((x) => Math.abs(centro - x.centro) < Math.max(r.h, x.h) * 0.7);
    if (!linea) {
      lineas.push({ ...r, centro });
      continue;
    }
    const derecha = Math.max(linea.x + linea.w, r.x + r.w);
    const abajo = Math.max(linea.y + linea.h, r.y + r.h);
    linea.x = Math.min(linea.x, r.x);
    linea.y = Math.min(linea.y, r.y);
    linea.w = derecha - linea.x;
    linea.h = abajo - linea.y;
  }
  return lineas;
}

export function cajasDeFrase(frase, palabras, contexto) {
  return cajasPorLinea(buscaDato(frase, palabras || [], contexto));
}

export class Enunciado {
  constructor(tablero, lec) {
    this.tablero = tablero;
    this.lec = lec;
    this.paso = tablero.nuevoPaso({ n: 0, titulo: lec.recorte ? `${lec.recorte.etiqueta || 'Enunciado'}` : 'Enunciado', clase: 'enunciado' });
    this.paso.cab.classList.add('cab-enunciado');
    this.palabras = [];
    this.marcasDato = new Map();
    this.datosSinMarca = new Map();
    this.lista = Promise.resolve();
    if (lec.recorte && lec.recorte.url) this._recorte(lec.recorte);
    else this._texto();
    if (lec.aviso) {
      const p = document.createElement('p');
      p.className = 'aviso-fuente';
      p.textContent = lec.aviso;
      this.paso.cuerpo.appendChild(p);
    }
    // Con recorte las opciones ya se ven en la imagen: la lista se escribe al revisarlas
    this.listas = [];
    if (lec.tipo === 'seleccion' && lec.opciones && Object.keys(lec.opciones).length && !lec.recorte) {
      this.paso.cuerpo.appendChild(this._opciones());
    }
  }

  _recorte(r) {
    const fig = document.createElement('figure');
    fig.className = 'recorte';
    const img = document.createElement('img');
    img.alt = this.lec.enunciado ? this.lec.enunciado.replace(/\$[^$]*\$/g, '') : 'Recorte del problema';
    img.decoding = 'async';
    img.draggable = false;
    this.lista = new Promise((res) => {
      const fin = () => { this.tablero.programarRecolocar(); res(); };
      img.addEventListener('load', fin, { once: true });
      img.addEventListener('error', () => { fig.remove(); this._texto(); fin(); }, { once: true });
      setTimeout(res, 2500);
    });
    img.src = urlApi(r.url);
    img.addEventListener('click', () => bus.emit('ver-imagen', { src: img.src, alt: img.alt }));
    this.img = img;
    this.palabras = r.palabras || [];
    const pie = document.createElement('figcaption');
    pie.textContent = r.pie || '';
    fig.append(img, pie);
    this.paso.cuerpo.appendChild(fig);
  }

  _texto() {
    if (!this.lec.enunciado || this.tablero.tinta('enunciado')) return;
    const t = new TintaTexto(this.lec.enunciado, { clase: 'enunciado-texto', px: this.tablero.px * 0.6 });
    this.tablero.agregar(t, { paso: this.paso, id: 'enunciado' });
    t.progreso(1);
  }

  _opciones() {
    const ul = document.createElement('ul');
    ul.className = 'opciones';
    const mapa = new Map();
    for (const [letra, txt] of Object.entries(this.lec.opciones)) {
      const li = document.createElement('li');
      li.className = 'opcion';
      li.dataset.letra = letra;
      const b = document.createElement('span');
      b.className = 'opcion-letra';
      b.textContent = letra;
      const c = document.createElement('span');
      c.className = 'opcion-txt';
      const t = new TintaTexto(txt, { etiqueta: 'span', px: this.tablero.px * 0.56 });
      t.progreso(1);
      c.appendChild(t.el);
      const m = document.createElement('span');
      m.className = 'opcion-motivo';
      m.hidden = true;
      c.appendChild(m);
      li.append(b, c);
      ul.appendChild(li);
      mapa.set(letra, { li, m });
    }
    this.listas.push({ ul, mapa });
    return ul;
  }

  /** Escribe la lista de opciones dentro de un paso (una sola vez por paso). */
  opcionesEn(vista) {
    if (!vista || vista.el.querySelector('.opciones')) return null;
    const ul = this._opciones();
    vista.cuerpo.appendChild(ul);
    return ul;
  }

  opcion(letra, estado, motivo) {
    this.listas = this.listas.filter((x) => x.ul.isConnected);
    for (const { mapa } of this.listas) {
      const o = mapa.get(letra);
      if (!o) continue;
      o.li.classList.remove('correcta', 'descartada');
      if (estado) o.li.classList.add(estado);
      if (motivo) {
        const t = new TintaTexto(motivo, { etiqueta: 'span', px: this.tablero.px * 0.52 });
        t.progreso(1);
        o.m.replaceChildren(t.el);
        o.m.hidden = false;
      }
      this.ultimaOpcion = o.li;
    }
    this.tablero.programarRecolocar();
  }

  reiniciarOpciones() {
    for (const { mapa } of this.listas) for (const { li, m } of mapa.values()) { li.classList.remove('correcta', 'descartada'); m.hidden = true; }
  }

  /** La transcripción se usa también cuando el PDF no permite localizar una frase. */
  marcarDato(dato, color, { temporal = false } = {}) {
    const ref = typeof dato === 'string' ? { frase: dato } : dato;
    if (!ref?.frase) return null;
    const { id, frase } = ref;
    if (!temporal && id && this.marcasDato.has(id)) return this.marcasDato.get(id);
    const marcas = this.tablero.marcas;
    const cajasPdf = this.img?.isConnected ? cajasDeFrase(frase, this.palabras, ref) : [];
    let grupo = [];
    let tinta = null;
    if (!cajasPdf.length) {
      this._texto();
      tinta = this.tablero.tinta('enunciado');
      const unidades = tinta?.unidades.map((u) => ({ t: u.tex || u.el.textContent, el: u.el })) || [];
      grupo = buscaDato(ref.frase_texto || ref.frase_original || frase, unidades, ref.frase_texto ? {} : ref);
      if (!grupo.length) {
        if (!temporal && !this.datosSinMarca.has(id || frase)) {
          const aviso = document.createElement('p');
          aviso.className = 'aviso-fuente';
          aviso.textContent = 'No pude ubicar este dato en el enunciado. Revisalo en el renglón del paso.';
          this.paso.cuerpo.appendChild(aviso);
          this.datosSinMarca.set(id || frase, aviso);
        }
        return null;
      }
    }
    const origen = cajasPdf.length ? this.img : tinta.el;
    const lado = this.alDato && cajasPdf.map((c) => this.alDato(c, color));
    const marca = marcas.crear({
      tipo: 'subraya', que: frase, color, dy: 1,
      cajas: () => {
        if (!origen.isConnected || origen.offsetParent === null) return [];
        if (!cajasPdf.length) return cajasPorLinea(grupo.map(({ el }) => this.tablero.rectEnHoja(el)));
        const r = this.tablero.rectEnHoja(origen);
        return cajasPdf.map((c) => ({ x: r.x + c.x * r.w, y: r.y + c.y * r.h, w: c.w * r.w, h: c.h * r.h }));
      },
    });
    marca.g.dataset.dato = frase;
    marca.g.classList.toggle('origen-temporal', temporal);
    marca.objetivo = grupo[0]?.el || origen;
    const quitar = marca.quitar.bind(marca);
    marca.quitar = () => {
      lado?.forEach((m) => m?.remove());
      if (this.marcasDato.get(id) === marca) this.marcasDato.delete(id);
      quitar();
    };
    if (!temporal && id) this.marcasDato.set(id, marca);
    return marca;
  }

  reiniciarDatos() {
    for (const marca of this.marcasDato.values()) marca.quitar();
    for (const aviso of this.datosSinMarca.values()) aviso.remove();
    this.datosSinMarca.clear();
  }
}
