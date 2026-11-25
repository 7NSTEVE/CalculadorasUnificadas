// Cabecera puente unica de CuadernU (iOS / iPadOS): lo unico de los nucleos
// C/C++ que ve Swift.
//
// Todos los encabezados incluidos aqui exponen exclusivamente ABI C puro:
//   - calcemu.h             : core/casio_nx (fx-570LA X / fx-570VN PLUS)
//   - casio_ffi.h           : core/casio_ffi v47 (fx-CG50, fx-991CW, fx-570LA X, fx-570VN PLUS)
//   - api.h                 : studio/tinta_core (motor Metal Ink 120 Hz)
//   - PPTXZipBridge.h       : lector ZIP para importacion PPTX editable
//   - cuadernu_studio_ffi.h : studio/c_api (.cudoc Windows/Android, CAS #varsub,
//                             Quimica 118, MNA/Bode/Laplace/K-Map, Grafo, SM-2, HorariosTEC)
//
// Verificado en Windows (MSVC x64 / C++20) que las 5 cabeceras conviven en un
// mismo binario sin colision de simbolos ni dependencias de Objective-C++.
#if defined(__APPLE__)
#include <zlib.h>
#endif
#include "calcemu.h"
#include "casio_ffi.h"
#include "api.h"
#include "duplicacion.h"
#include "transaccion_historial_api.h"
#include "buscador_api.h"
#include "PPTXZipBridge.h"
#include "cuadernu_studio_ffi.h"
