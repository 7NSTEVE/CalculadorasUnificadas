#ifndef TINTA_SIMULADOR_H
#define TINTA_SIMULADOR_H

#include <vector>

namespace nucleo {

enum class TipoComponente {
    Resistor,
    FuenteVoltaje,
    FuenteCorriente
};

struct Nodo {
    int id; 
    double voltaje;
};

struct Componente {
    TipoComponente tipo;
    int nodoA;
    int nodoB;
    double valor; 
};

// Adaptador de compatibilidad para consumidores antiguos. La implementación
// delega en el motor `tinta::ResolverDC`, aplica la misma validación estricta y
// devuelve false ante valores, nodos o topologías inválidas. No produce
// resultados parciales ni omite componentes silenciosamente.
bool ResolverCircuitoDC(const std::vector<Componente>& componentes, std::vector<Nodo>& nodos);

} // namespace nucleo

#endif // TINTA_SIMULADOR_H
