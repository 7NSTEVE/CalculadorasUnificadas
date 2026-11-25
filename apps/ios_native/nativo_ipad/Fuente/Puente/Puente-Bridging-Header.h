// Lo que Swift ve del núcleo de tinta.
//
// Es C puro, no Objective-C. Swift importa cabeceras de C directamente, así que
// no hace falta ninguna capa intermedia — y a diferencia de un puente en
// Objective-C++, este código sí se compila y se prueba en Windows antes de
// gastar un minuto de runner macOS.
#include "api.h"
#include "PPTXZipBridge.h"
