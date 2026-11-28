import { VisualPV } from './pv.js';
import { VisualConduccion } from './conduccion.js';
import { VisualDilatacion } from './dilatacion.js';
import { VisualCalentamiento, VisualCalor } from './calor.js';
import { VisualPared } from './fluidos.js';
import {
  VisualDistribucion, VisualEntropia, VisualFuncion, VisualGas, VisualMaquina, VisualOtto, VisualRadiacion,
} from './otros.js';
import { VisorEscena3D, soportaEscena3D } from '../visuales3d/visor_escena_3d.js';

const TIPOS = {
  pv: VisualPV, conduccion: VisualConduccion, dilatacion: VisualDilatacion, calor: VisualCalor,
  calentamiento: VisualCalentamiento, gas: VisualGas, distribucion: VisualDistribucion, radiacion: VisualRadiacion,
  maquina: VisualMaquina, otto: VisualOtto, entropia: VisualEntropia, funcion: VisualFuncion,
  pared: VisualPared,
};

export function hayVisual(tipo) {
  return Boolean(TIPOS[tipo]);
}

export { soportaEscena3D };

/** Crea el visual 2D clásico o devuelve null si la especificación no sirve (nunca rompe la lección). */
export function crearVisual(spec) {
  if (!spec || !TIPOS[spec.tipo]) return null;
  try {
    return new TIPOS[spec.tipo](spec);
  } catch (err) {
    console.error('visual', spec.tipo, err);
    return null;
  }
}

/** Crea el visor 3D WebGL2 interactivo para una lección a partir de `lec.apoyo_visual`. */
export function crearVisual3D(apoyo, opt = {}) {
  if (!apoyo || !soportaEscena3D(apoyo.modelo)) return null;
  try {
    return new VisorEscena3D(apoyo, opt);
  } catch (err) {
    console.error('visual3d', apoyo.modelo, err);
    return null;
  }
}
