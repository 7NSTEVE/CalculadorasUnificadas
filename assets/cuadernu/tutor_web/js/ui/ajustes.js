/** Panel de ajustes: voz, letra, tema y ritmo. */

import { ajustes, bus, guardarAjuste } from '../nucleo/api.js';
import { ESTILOS_LETRA, usarLetra } from '../nucleo/letra.js';
import { aplicarTema } from '../nucleo/paleta.js';
import { VOCES_NEURALES, probarVoz, vocesLocales } from '../leccion/voz_leccion.js';

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
    for (const [v, t] of [['auto', 'Voz natural (recomendada)'], ['local', 'Voz del sistema (sin internet)'], ['mudo', 'Sin voz']]) {
      const o = document.createElement('option');
      o.value = v; o.textContent = t; o.selected = (v === 'mudo' ? !ajustes.narracion : ajustes.narracion && ajustes.motorVoz === v);
      motor.appendChild(o);
    }
    motor.addEventListener('change', () => {
      guardarAjuste('narracion', motor.value !== 'mudo');
      if (motor.value !== 'mudo') guardarAjuste('motorVoz', motor.value);
      acciones.cambioVoz();
    });

    const voz = document.createElement('select');
    for (const [v, t] of VOCES_NEURALES) {
      const o = document.createElement('option');
      o.value = v; o.textContent = t; o.selected = ajustes.voz === v;
      voz.appendChild(o);
    }
    voz.addEventListener('change', () => { guardarAjuste('voz', voz.value); acciones.cambioVoz(); });

    const btnProbar = document.createElement('button');
    btnProbar.type = 'button';
    btnProbar.className = 'btn';
    btnProbar.textContent = 'Probar';
    btnProbar.addEventListener('click', async () => {
      btnProbar.disabled = true;
      btnProbar.textContent = 'Cargando…';
      try { await probarVoz(voz.value); } catch { /* sin conexión */ }
      btnProbar.disabled = false;
      btnProbar.textContent = 'Probar';
    });
    const vozCaja = document.createElement('span');
    vozCaja.style.display = 'flex'; vozCaja.style.gap = '0.45em'; vozCaja.style.alignItems = 'center';
    vozCaja.style.justifyContent = 'flex-end'; vozCaja.style.flex = '0 1 auto';
    vozCaja.append(voz, btnProbar);

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
    for (const [v, t] of acciones.modelos()) {
      const o = document.createElement('option');
      o.value = v; o.textContent = t; o.selected = ajustes.modelo === v;
      modelo.appendChild(o);
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
      bloque('Voz del profesor', vozCaja),
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
