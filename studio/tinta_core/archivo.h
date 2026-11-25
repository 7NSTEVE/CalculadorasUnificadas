// Formato de archivo del documento.
//
// Binario, little-endian, con cabecera y número de versión. Es el formato
// canónico que se sincroniza: la proyección a OneNote es una consecuencia, no la
// fuente de verdad.
//
// Por qué binario y no JSON: un cuaderno de un semestre son cientos de miles de
// muestras de lápiz, cada una con posición, presión, inclinación y tiempo. En
// texto eso multiplica por cinco el tamaño y el tiempo de carga, y lo que se
// gana —poder leerlo con los ojos— no sirve para datos de digitalizador.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "documento.h"

namespace tinta {

// Versión del formato. Sube cuando cambie la estructura; el lector rechaza lo
// que no entiende en vez de leer basura.
constexpr uint16_t kVersionArchivo = 5;

std::vector<uint8_t> Serializar(const Documento& documento);

// Devuelve false si la cabecera no cuadra, la versión es de otro formato, o los
// datos se acaban antes de tiempo. En ese caso `documento` queda sin tocar.
bool Deserializar(const std::vector<uint8_t>& datos, Documento* documento);

// Formato autocontenido de un solo trazo. Se usa para enviar operaciones de
// colaboración sin serializar la página completa y conserva muestras, tiempo,
// presión, inclinación, estilo e identidad estable.
std::vector<uint8_t> SerializarTrazo(const Trazo& trazo);
bool DeserializarTrazo(const std::vector<uint8_t>& datos, Trazo* trazo);

// Motivo del último fallo de `Deserializar`, para poder diagnosticar un archivo
// corrupto en vez de decir solo que no se pudo.
const char* UltimoErrorArchivo();

// Permite que las validaciones de la fachada C informen fallos de integridad
// que se detectan después de deserializar (por ejemplo, abrir bytes de otro
// documento bajo una identidad distinta).
void EstablecerErrorArchivo(const char* mensaje);

}  // namespace tinta
