#include "simulador.h"
#include "ingenieria.h"

#include <cmath>

namespace nucleo {

bool ResolverCircuitoDC(const std::vector<Componente>& componentes, std::vector<Nodo>& nodos) {
    std::vector<tinta::ComponenteElectrico> modernos;
    modernos.reserve(componentes.size());
    for (const auto& comp : componentes) {
        if (comp.nodoA < 0 || comp.nodoB < 0 || !std::isfinite(comp.valor)) {
            nodos.clear();
            return false;
        }
        tinta::TipoComponenteElectrico tipo = tinta::TipoComponenteElectrico::Resistor;
        switch (comp.tipo) {
            case TipoComponente::Resistor:
                tipo = tinta::TipoComponenteElectrico::Resistor;
                break;
            case TipoComponente::FuenteVoltaje:
                tipo = tinta::TipoComponenteElectrico::FuenteVoltaje;
                break;
            case TipoComponente::FuenteCorriente:
                tipo = tinta::TipoComponenteElectrico::FuenteCorriente;
                break;
        }
        tinta::ComponenteElectrico convertido;
        convertido.tipo = tipo;
        convertido.nodo_a = comp.nodoA;
        convertido.nodo_b = comp.nodoB;
        convertido.valor = comp.valor;
        modernos.push_back(convertido);
    }

    const tinta::ResultadoDC resultado = tinta::ResolverDC(modernos);
    if (!resultado.correcto) {
        nodos.clear();
        return false;
    }

    nodos.clear();
    nodos.reserve(resultado.voltajes.size());
    for (size_t indice = 0; indice < resultado.voltajes.size(); ++indice) {
        nodos.push_back({static_cast<int>(indice), resultado.voltajes[indice]});
    }
    return true;
}

} // namespace nucleo
