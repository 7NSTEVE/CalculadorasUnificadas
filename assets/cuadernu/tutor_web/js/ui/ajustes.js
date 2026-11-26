/** Panel de ajustes: voz, letra, tema y ritmo. */

import { ajustes, bus, guardarAjuste } from '../nucleo/api.js';
import { ESTILOS_LETRA, usarLetra } from '../nucleo/letra.js';
import { aplicarTema } from '../nucleo/paleta.js';
import { VOCES_KOKORO, VOCES_NEURALES, probarVoz, vocesLocales } from '../leccion/voz_leccion.js';

function fila(txt, control) {
  const d = document.createElement('label');
  d.className = 'ajuste';
  const s = document.createElement('span');
  s.textContent = txt;
  d.append(s, control);
  return d;
}

function titulo(txt) {
  const h = document.createElement('h2');
  h.textContent = txt;
  return h;
}

function seg(opciones, valor, alElegir) {
  const d = document.createElement('div');
  d.className = 'seg';
  d.setAttribute('role', 'group');
  for (const [v, t] of opciones) {
    const b = document.createElement('button');
    b.type = 'button';
    b.textContent = t;
    b.setAttribute('aria-pressed', String(v === valor));
    b.addEventListener('click', () => {
      d.querySelectorAll('button').forEach((x) => x.setAttribute('aria-pressed', 'false'));
      b.setAttribute('aria-pressed', 'true');
      alElegir(v);
    });
    d.appendChild(b);
  }
  return d;
}

function casilla(k, alCambiar = null) {
  const i = document.createElement('input');
  i.type = 'checkbox';
  i.checked = Boolean(ajustes[k]);
  i.addEventListener('change', () => { guardarAjuste(k, i.checked); if (alCambiar) alCambiar(i.checked); });
  return i;
}

function bloque(txt, control) {
  const d = document.createElement('div');
  d.className = 'ajuste';
  const s = document.createElement('span');
  s.textContent = txt;
  d.append(s, control);
  return d;
}

export function armarAjustes(el, acciones) {
  const pintar = () => {
    const vel = document.createElement('input');
    vel.type = 'range';
    vel.min = '0.6'; vel.max = '1.6'; vel.step = '0.05'; vel.value = String(ajustes.vel);
    const velTxt = document.createElement('span');
    velTxt.textContent = `${Number(ajustes.vel).toFixed(2).replace('.', ',')}×`;
    vel.addEventListener('input', () => { velTxt.textContent = `${Number(vel.value).toFixed(2).replace('.', ',')}×`; });
    vel.addEventListener('change', () => guardarAjuste('vel', Number(vel.value)));
    const velCaja = document.createElement('span');
    velCaja.style.display = 'flex'; velCaja.style.gap = '0.6em'; velCaja.style.alignItems = 'center';
    velCaja.append(vel, velTxt);

    const motor = document.createElement('select');
    for (const [v, t] of [
      ['auto', 'Automático (Edge Neural)'],
      ['local_http', 'Local GPU/Vulkan (ZONOS3 / VoxCPM2 / Qwen3)'],
      ['kokoro', 'Kokoro-82M local (ONNX)'],
      ['local', 'Voz del sistema (sin internet)'],
      ['mudo', 'Sin voz'],
    ]) {
      const o = document.createElement('option');
      o.value = v; o.textContent = t; o.selected = (v === 'mudo' ? !ajustes.narracion : ajustes.narracion && ajustes.motorVoz === v);
      motor.appendChild(o);
    }
    motor.addEventListener('change', () => {
      guardarAjuste('narracion', motor.value !== 'mudo');
      if (motor.value !== 'mudo') guardarAjuste('motorVoz', motor.value);
      acciones.cambioVoz();
      pintar();
    });

    const voz = document.createElement('select');
    const esKokoro = ajustes.motorVoz === 'kokoro';
    const listaVoces = esKokoro ? VOCES_KOKORO : VOCES_NEURALES;
    const vozActual = esKokoro ? ajustes.vozKokoro : ajustes.voz;
    for (const [v, t] of listaVoces) {
      const o = document.createElement('option');
      o.value = v; o.textContent = t; o.selected = vozActual === v;
      voz.appendChild(o);
    }
    voz.addEventListener('change', () => {
      guardarAjuste(esKokoro ? 'vozKokoro' : 'voz', voz.value);
      acciones.cambioVoz();
    });

    const btnProbar = document.createElement('button');
    btnProbar.type = 'button';
    btnProbar.className = 'btn';
    btnProbar.textContent = 'Probar';
    btnProbar.addEventListener('click', async () => {
      btnProbar.disabled = true;
      btnProbar.textContent = 'Cargando…';
      try { await probarVoz(ajustes.motorVoz === 'local_http' ? ajustes.vozLocalHttp : voz.value); } catch { /* sin conexión */ }
      btnProbar.disabled = false;
      btnProbar.textContent = 'Probar';
    });
    const vozCaja = document.createElement('span');
    vozCaja.style.display = 'flex'; vozCaja.style.gap = '0.45em'; vozCaja.style.alignItems = 'center';
    vozCaja.style.justifyContent = 'flex-end'; vozCaja.style.flex = '0 1 auto';
    vozCaja.append(voz);

    const urlLocal = document.createElement('input');
    urlLocal.type = 'text';
    urlLocal.value = ajustes.servidorVozLocal || 'http://127.0.0.1:8880/v1/audio/speech';
    urlLocal.placeholder = 'http://127.0.0.1:8880/v1/audio/speech';
    urlLocal.style.maxWidth = '15em';
    urlLocal.style.border = '1px solid var(--borde)';
    urlLocal.style.borderRadius = '8px';
    urlLocal.style.padding = '0.35em 0.55em';
    urlLocal.style.background = 'var(--papel)';
    urlLocal.addEventListener('change', () => { guardarAjuste('servidorVozLocal', urlLocal.value.trim()); acciones.cambioVoz(); });

    const vozHttp = document.createElement('input');
    vozHttp.type = 'text';
    vozHttp.value = ajustes.vozLocalHttp || 'es-default';
    vozHttp.placeholder = 'es-default';
    vozHttp.style.maxWidth = '10em';
    vozHttp.style.border = '1px solid var(--borde)';
    vozHttp.style.borderRadius = '8px';
    vozHttp.style.padding = '0.35em 0.55em';
    vozHttp.style.background = 'var(--papel)';
    vozHttp.addEventListener('change', () => { guardarAjuste('vozLocalHttp', vozHttp.value.trim() || 'es-default'); acciones.cambioVoz(); });
    const httpCaja = document.createElement('span');
    httpCaja.style.display = 'flex'; httpCaja.style.gap = '0.45em'; httpCaja.style.alignItems = 'center';
    httpCaja.style.justifyContent = 'flex-end'; httpCaja.style.flex = '0 1 auto';
    httpCaja.append(vozHttp);
    (ajustes.motorVoz === 'local_http' ? httpCaja : vozCaja).append(btnProbar);

    const vloc = document.createElement('select');
    const auto = document.createElement('option');
    auto.value = ''; auto.textContent = 'Automática';
    vloc.appendChild(auto);
    for (const v of vocesLocales()) {
      const o = document.createElement('option');
      o.value = v.name; o.textContent = v.name.replace(/^Microsoft\s+/, '').replace(/ - .*/, ` (${v.lang})`); o.selected = ajustes.vozLocal === v.name;
      vloc.appendChild(o);
    }
    vloc.addEventListener('change', () => { guardarAjuste('vozLocal', vloc.value); acciones.cambioVoz(); });

    const modelo = document.createElement('select');
    const grupos = new Map();
    for (const [v, t, grp] of acciones.modelos()) {
      const o = document.createElement('option');
      o.value = v; o.textContent = t; o.selected = ajustes.modelo === v;
      if (grp) {
        if (!grupos.has(grp)) {
          const og = document.createElement('optgroup');
          og.label = grp;
          grupos.set(grp, og);
          modelo.appendChild(og);
        }
        grupos.get(grp).appendChild(o);
      } else {
        modelo.appendChild(o);
      }
    }
    modelo.addEventListener('change', () => { guardarAjuste('modelo', modelo.value); acciones.cambioModelo(modelo.value); });

    const reporte = document.createElement('button');
    reporte.type = 'button';
    reporte.className = 'btn';
    reporte.textContent = 'Marcar un fallo';
    reporte.addEventListener('click', () => { el.hidePopover?.(); acciones.reportar(); });

    const nodos = [
      titulo('Voz'),
      bloque('Velocidad', velCaja),
      fila('Motor de voz', motor),
      ...(ajustes.motorVoz === 'local_http'
        ? [fila('URL local (loopback)', urlLocal), bloque('Voz local', httpCaja)]
        : [bloque('Voz del profesor', vozCaja)]),
      fila('Respaldo sin internet', vloc),
      titulo('Lectura'),
      bloque('Letra', seg(Object.entries(ESTILOS_LETRA).map(([k, v]) => [k, v.nombre]), ajustes.letra, async (v) => {
        ajustes.letra = v;
        await usarLetra(v);
        guardarAjuste('letra', v);
      })),
      bloque('Tema', seg([['light', 'Claro'], ['dark', 'Oscuro'], ['high-contrast', 'Contraste']], ajustes.tema, (v) => { guardarAjuste('tema', v); aplicarTema(v); })),
      titulo('Ritmo'),
      fila('Plegar los pasos anteriores', casilla('foco')),
      fila('Modo sin saltos (mostrar despejes)', casilla('sinSaltos')),
      fila('Mostrar qué es cada letra al escribir', casilla('simbolos')),
      fila('Pasar solo al siguiente paso', casilla('autoSeguir')),
      fila('Menos movimiento', casilla('menosMovimiento')),
      titulo('Preguntas libres e imágenes'),
      fila('Inteligencia', modelo),
      fila('Resolver al pegar una imagen', casilla('resolverAlPegar')),
    ];
    if (acciones.enCuaderno) {
      const g = document.createElement('button');
      g.type = 'button';
      g.className = 'btn btn-principal';
      g.textContent = 'Guardar en el cuaderno';
      g.addEventListener('click', () => { el.hidePopover?.(); acciones.guardarCuaderno(); });
      nodos.push(titulo('Cuaderno'), g);
    }
    nodos.push(titulo('Ayuda'), reporte);
    el.replaceChildren(...nodos);
  };
  el.addEventListener('beforetoggle', (e) => { if (e.newState === 'open') pintar(); });
  bus.on('ajuste:tema', () => {});
  pintar();
}
