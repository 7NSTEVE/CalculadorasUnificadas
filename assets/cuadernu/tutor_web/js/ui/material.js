/**
 * Material que trae la persona: imágenes pegadas, PDF y pedidos como "problema 9 del ordinario
 * I-2024". Lo pegado queda en la hoja y, si es un ejercicio del banco, abre su lección verificada.
 */

import { ajustes, bus, pedir, urlApi } from '../nucleo/api.js';
import { aviso } from './avisos.js';

const TIPOS_IMG = ['image/png', 'image/jpeg', 'image/webp', 'image/gif', 'image/bmp'];

function leerB64(blob) {
  return new Promise((res, rej) => {
    const r = new FileReader();
    r.onload = () => res(String(r.result).split(',')[1] || '');
    r.onerror = () => rej(r.error);
    r.readAsDataURL(blob);
  });
}

/** Reduce imágenes enormes y las deja en PNG o JPEG legible por la IA. */
async function normalizar(blob, lado = 2000) {
  const bmp = await createImageBitmap(blob);
  const k = Math.min(1, lado / Math.max(bmp.width, bmp.height));
  if (k === 1 && ['image/png', 'image/jpeg', 'image/webp'].includes(blob.type) && blob.size < 3.5e6) {
    bmp.close();
    return { blob, tipo: blob.type };
  }
  const c = document.createElement('canvas');
  c.width = Math.round(bmp.width * k);
  c.height = Math.round(bmp.height * k);
  const x = c.getContext('2d');
  x.fillStyle = '#fff';
  x.fillRect(0, 0, c.width, c.height);
  x.drawImage(bmp, 0, 0, c.width, c.height);
  bmp.close();
  const out = await new Promise((r) => c.toBlob(r, 'image/jpeg', 0.9));
  return { blob: out, tipo: 'image/jpeg' };
}

export class Material {
  constructor({ el, tablero, acciones, asistente }) {
    Object.assign(this, { el, tablero, acciones, asistente });
    this.n = 0;
    window.addEventListener('paste', (e) => this._pegar(e));
    const zona = document.getElementById('escena');
    zona.addEventListener('dragover', (e) => { if (e.dataTransfer && [...e.dataTransfer.types].includes('Files')) { e.preventDefault(); e.dataTransfer.dropEffect = 'copy'; } });
    zona.addEventListener('drop', (e) => {
      if (!e.dataTransfer || !e.dataTransfer.files.length) return;
      e.preventDefault();
      [...e.dataTransfer.files].forEach((f) => this.archivo(f));
    });
    window.addEventListener('dragover', (e) => e.preventDefault());
    window.addEventListener('drop', (e) => e.preventDefault());
    const inp = document.getElementById('archivo');
    inp.addEventListener('change', () => { [...inp.files].forEach((f) => this.archivo(f)); inp.value = ''; });
    bus.on('leccion:abierta', () => this._vaciar());
  }

  _vaciar() {
    this.el.replaceChildren();
    this.el.hidden = true;
  }

  _pegar(e) {
    const dt = e.clipboardData;
    if (!dt) return;
    const enCampo = e.target && /^(INPUT|TEXTAREA)$/.test(e.target.tagName);
    const img = [...dt.items].find((i) => i.kind === 'file' && i.type.startsWith('image/'));
    if (img) {
      e.preventDefault();
      const f = img.getAsFile();
      if (f) this.imagen(f, 'Imagen pegada');
      return;
    }
    const pdf = [...dt.items].find((i) => i.kind === 'file' && i.type === 'application/pdf');
    if (pdf) { e.preventDefault(); const f = pdf.getAsFile(); if (f) this.archivo(f); return; }
    if (enCampo) return;
    const t = (dt.getData('text/plain') || '').trim();
    if (t) {
      e.preventDefault();
      const c = document.getElementById('pregunta');
      c.value = t.slice(0, 4000);
      c.focus();
      aviso('Texto pegado en la pregunta. Pulsa Enviar cuando quieras.');
    }
  }

  async archivo(f) {
    if (!f) return;
    if (TIPOS_IMG.includes(f.type) || /\.(png|jpe?g|webp|gif|bmp)$/i.test(f.name)) { await this.imagen(f, f.name); return; }
    if (f.type === 'application/pdf' || /\.pdf$/i.test(f.name)) { await this.pdf(f); return; }
    aviso('Puedo abrir PDF e imágenes (PNG, JPG o WebP).', { mal: true });
  }

  /* ------------------------------------------------------------ imágenes */

  async imagen(f, nombre) {
    this.acciones.pizarra();
    let norm;
    try { norm = await normalizar(f); } catch { aviso('No pude leer esa imagen.', { mal: true }); return; }
    const b64 = await leerB64(norm.blob);
    const url = URL.createObjectURL(norm.blob);
    const { caja, cuerpo } = this._tarjeta(nombre);
    const img = document.createElement('img');
    img.src = url;
    img.alt = nombre;
    img.addEventListener('click', () => bus.emit('ver-imagen', { src: url, alt: nombre }));
    cuerpo.appendChild(img);
    await img.decode().catch(() => {});
    this.tablero.seguir(caja);
    const estado = document.createElement('p');
    estado.className = 'aviso-fuente';
    estado.style.borderColor = 'var(--azul)';
    estado.textContent = 'Buscando si es uno de los ejercicios del banco…';
    cuerpo.appendChild(estado);
    let r = null;
    try {
      r = await pedir('/api/imagen/reconocer', { metodo: 'POST', cuerpo: { imagen: b64, tipo: norm.tipo }, tiempo: 45000 });
    } catch { r = null; }
    if (r && r.id) {
      estado.textContent = `Es ${r.etiqueta || r.id}: ${r.titulo || ''}`;
      const fila = document.createElement('div');
      fila.className = 'fila-botones';
      const b = document.createElement('button');
      b.type = 'button';
      b.className = 'btn btn-principal';
      b.textContent = 'Resolverlo paso a paso';
      b.addEventListener('click', () => this.acciones.abrir(r.id));
      const o = document.createElement('button');
      o.type = 'button';
      o.className = 'btn';
      o.textContent = 'No es ese';
      o.addEventListener('click', () => { estado.remove(); fila.remove(); this.asistente.imagen(b64, { nombre, tipo: norm.tipo, texto: r.texto || '' }); });
      fila.append(b, o);
      cuerpo.appendChild(fila);
      if (r.seguro && ajustes.resolverAlPegar) this.acciones.abrir(r.id);
      return;
    }
    estado.remove();
    if (ajustes.resolverAlPegar) this.asistente.imagen(b64, { nombre, tipo: norm.tipo, texto: (r && r.texto) || '' });
    else {
      const b = document.createElement('button');
      b.type = 'button';
      b.className = 'btn btn-principal';
      b.textContent = 'Resolver esta imagen';
      b.addEventListener('click', () => this.asistente.imagen(b64, { nombre, tipo: norm.tipo, texto: (r && r.texto) || '' }));
      cuerpo.appendChild(b);
    }
  }

  _tarjeta(nombre) {
    this.el.hidden = false;
    const caja = document.createElement('div');
    caja.className = 'pegado';
    const cab = document.createElement('div');
    cab.className = 'pegado-cab';
    const s = document.createElement('strong');
    s.textContent = nombre;
    const x = document.createElement('button');
    x.type = 'button';
    x.className = 'btn btn-suave';
    x.textContent = 'Quitar';
    x.addEventListener('click', () => { caja.remove(); if (!this.el.childElementCount) this.el.hidden = true; });
    cab.append(s, x);
    const cuerpo = document.createElement('div');
    caja.append(cab, cuerpo);
    this.el.appendChild(caja);
    return { caja, cuerpo };
  }

  /* ------------------------------------------------------------ PDF */

  async pdf(f) {
    if (f.size > 60e6) { aviso('Ese PDF pasa de 60 MB.', { mal: true }); return; }
    this.acciones.pizarra();
    const { caja, cuerpo } = this._tarjeta(f.name);
    const p = document.createElement('p');
    p.textContent = 'Leyendo el documento…';
    cuerpo.appendChild(p);
    this.tablero.seguir(caja);
    try {
      const d = await pedir(`/api/documentos/subir?nombre=${encodeURIComponent(f.name.slice(0, 120))}`, { metodo: 'POST', crudo: await f.arrayBuffer(), tipo: 'application/pdf', tiempo: 90000 });
      p.remove();
      this._listaProblemas(cuerpo, d);
    } catch (err) {
      p.textContent = `No pude leer el PDF: ${err.message || 'error'}`;
    }
  }

  _listaProblemas(cuerpo, d) {
    if (!d.problemas || !d.problemas.length) {
      const p = document.createElement('p');
      p.textContent = 'No encontré problemas numerados. Puedes encerrar o pegar un recorte de la parte que te interesa.';
      cuerpo.appendChild(p);
      return;
    }
    const ul = document.createElement('ul');
    ul.className = 'lista-ejs';
    for (const q of d.problemas) {
      const li = document.createElement('li');
      const b = document.createElement('button');
      b.type = 'button';
      b.className = 'ej';
      const c = document.createElement('span');
      c.className = 'cod';
      c.textContent = q.num;
      const m = document.createElement('span');
      const t = document.createElement('strong');
      t.textContent = q.titulo || q.texto || '';
      const s = document.createElement('small');
      s.textContent = [q.leccion ? 'Resuelto en el banco' : '', `pág. ${q.pagina}`].filter(Boolean).join(' · ');
      m.append(t, s);
      const e = document.createElement('span');
      b.append(c, m, e);
      b.addEventListener('click', () => this.abrirProblema(d.doc, q));
      li.appendChild(b);
      ul.appendChild(li);
    }
    cuerpo.appendChild(ul);
    this.tablero.programarRecolocar();
  }

  /** Problema de un documento: lección del banco si existe; si no, recorte + IA. */
  async abrirProblema(doc, q) {
    if (q.leccion) { this.acciones.abrir(q.leccion); return; }
    try {
      const r = await pedir(`/api/documentos/${encodeURIComponent(doc)}/problema/${encodeURIComponent(q.clave)}`, { tiempo: 30000 });
      this.recorteYResolver(r);
    } catch (err) { aviso(err.message || 'No pude abrir ese problema.', { mal: true }); }
  }

  recorteYResolver(r) {
    this.acciones.pizarra();
    const { caja, cuerpo } = this._tarjeta(r.etiqueta || 'Problema');
    const img = document.createElement('img');
    img.src = urlApi(r.recorte.url);
    img.alt = r.texto || 'Recorte del problema';
    img.addEventListener('click', () => bus.emit('ver-imagen', { src: img.src, alt: img.alt }));
    cuerpo.appendChild(img);
    this.tablero.seguir(caja);
    this.asistente.documento(r);
  }

  /** "problema 9 del ordinario I-2024", "folleto ejercicio 12"… */
  async resolverPedido(q) {
    if (!/(examen|parcial|ordinario|extra|reposici|folleto|superexamen|recomendad|20\d\d|problema|pregunta)/i.test(q) || !/\d/.test(q)) return false;
    try {
      const r = await pedir(`/api/documentos/resolver?q=${encodeURIComponent(q.slice(0, 200))}`, { tiempo: 20000 });
      if (!r) return false;
      if (r.leccion) { this.acciones.abrir(r.leccion); return true; }
      if (r.recorte) { this.recorteYResolver(r); return true; }
    } catch { /* se trata como pregunta libre */ }
    return false;
  }
}
