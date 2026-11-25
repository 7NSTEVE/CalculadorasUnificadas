// Reconocimiento de circuitos eléctricos a partir de trazos de tinta.
//
// Modela un grafo de nodos y componentes y provee heurísticas no destructivas
// para adivinar qué componente intentó dibujar el usuario, usando el sistema
// de formas base.
#pragma once

#include <vector>
#include <string>

#include "tinta.h"
#include "formas.h"

namespace tinta {

enum class TipoComponenteCircuito {
  Resistor,
  Capacitor,
  Inductor,
  FuenteVoltaje,
  FuenteCorriente,
  Tierra,
  Cable
};

struct NodoCircuito {
  int id = 0;
  Punto posicion; // Posición ideal en el lienzo
};

struct ComponenteCircuito {
  TipoComponenteCircuito tipo = TipoComponenteCircuito::Cable;
  std::vector<int> nodos; // IDs de los nodos a los que se conecta
  float valor = 0.0f;     // Valor numérico para SPICE (resistencia, capacitancia, etc)
  std::string id_texto;   // Identificador, ej. "R1", "C1", "V1"
};

// Grafo conectado del circuito
struct Circuito {
  std::vector<NodoCircuito> nodos;
  std::vector<ComponenteCircuito> componentes;
};

// Resultado de intentar reconocer un componente a partir de la tinta
struct ReconocimientoCircuito {
  TipoComponenteCircuito tipo = TipoComponenteCircuito::Cable;
  float confianza = 0.0f; // 0..1
  std::vector<Punto> terminales; // Puntos donde este componente espera conectarse
};

// Adivina qué componente es un conjunto de trazos.
// No modifica los trazos; es una operación no destructiva.
ReconocimientoCircuito ReconocerComponente(const std::vector<Trazo>& trazos,
                                           const ParametrosForma& params = {});

// Descriptor geométrico normalizado (independiente de escala y traslación)
// para plantillas que el usuario enseña localmente. Su longitud es estable en
// el formato del API; no contiene el trazo ni datos personales.
std::vector<float> ExtraerCaracteristicasCircuito(
    const std::vector<Trazo>& trazos,
    const ParametrosForma& params = {});

// Genera una netlist básica en formato SPICE a partir del modelo de grafo.
// Componentes incompletos se omiten con un comentario explícito; nunca se les
// asigna un valor eléctrico inventado.
std::string GenerarSpice(const Circuito& circuito);

}  // namespace tinta
