import { VisualPV } from './pv.js';
import { VisualConduccion } from './conduccion.js';
import { VisualDilatacion } from './dilatacion.js';
import { VisualCalentamiento, VisualCalor } from './calor.js';
import {
  VisualDistribucion, VisualEntropia, VisualFuncion, VisualGas, VisualMaquina, VisualOtto, VisualRadiacion,
} from './otros.js';

const TIPOS = {
  pv: VisualPV, conduccion: VisualConduccion, dilatacion: VisualDilatacion, calor: VisualCalor,
  calentamiento: VisualCalentamiento, gas: VisualGas, distribucion: VisualDistribucion, radiacion: VisualRadiacion,
  maquina: VisualMaquina, otto: VisualOtto, entropia: VisualEntropia, funcion: VisualFuncion,
};

export function hayVisual(tipo) {
  return Boolean(TIPOS[tipo]);
}

/** Crea el visual o devuelve null si la especificación no sirve (nunca rompe la lección). */
export function crearVisual(spec) {
  if (!spec || !TIPOS[spec.tipo]) return null;
  try {
    return new TIPOS[spec.tipo](spec);
  } catch (err) {
    console.error('visual', spec.tipo, err);
    return null;
  }
}
