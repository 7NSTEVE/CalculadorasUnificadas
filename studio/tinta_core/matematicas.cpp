#include "matematicas.h"
#include <algorithm>
#include <cmath>

namespace tinta {

std::unique_ptr<NodoMatematico> AnalizarDisenoMatematico(const std::vector<SimboloReconocido>& simbolos) {
    if (simbolos.empty()) return nullptr;

    std::vector<const SimboloReconocido*> ordenados;
    for (const auto& s : simbolos) ordenados.push_back(&s);
    
    std::sort(ordenados.begin(), ordenados.end(), [](const SimboloReconocido* a, const SimboloReconocido* b) {
        return a->caja.min_x < b->caja.min_x;
    });

    auto raiz = std::make_unique<NodoMatematico>();
    raiz->tipo = TipoNodoMatematico::Fila;

    for (size_t i = 0; i < ordenados.size(); ++i) {
        const auto* s = ordenados[i];
        
        // Detección rudimentaria de superíndice
        if (!raiz->hijos.empty() && raiz->hijos.back()->tipo == TipoNodoMatematico::Simbolo) {
            const auto* anterior = raiz->hijos.back()->simbolo;
            float altura_anterior = anterior->caja.max_y - anterior->caja.min_y;
            
            if (s->caja.min_y < anterior->caja.min_y && s->caja.max_y < anterior->caja.max_y - altura_anterior * 0.3f) {
                auto superindice = std::make_unique<NodoMatematico>();
                superindice->tipo = TipoNodoMatematico::Superindice;
                superindice->hijos.push_back(std::move(raiz->hijos.back()));
                raiz->hijos.pop_back();
                
                auto nodo_exp = std::make_unique<NodoMatematico>();
                nodo_exp->tipo = TipoNodoMatematico::Simbolo;
                nodo_exp->simbolo = s;
                superindice->hijos.push_back(std::move(nodo_exp));
                
                raiz->hijos.push_back(std::move(superindice));
                continue;
            }
        }

        // Detección rudimentaria de fracción
        float ancho = s->caja.max_x - s->caja.min_x;
        float alto = s->caja.max_y - s->caja.min_y;
        if (s->texto == "-" && ancho > alto * 2.0f) {
            if (!raiz->hijos.empty() && i + 1 < ordenados.size()) {
                auto numerador = std::move(raiz->hijos.back());
                raiz->hijos.pop_back();

                auto denominador = std::make_unique<NodoMatematico>();
                denominador->tipo = TipoNodoMatematico::Simbolo;
                denominador->simbolo = ordenados[i + 1];

                auto fraccion = std::make_unique<NodoMatematico>();
                fraccion->tipo = TipoNodoMatematico::Fraccion;
                fraccion->hijos.push_back(std::move(numerador));
                fraccion->hijos.push_back(std::move(denominador));
                
                raiz->hijos.push_back(std::move(fraccion));
                ++i; 
                continue;
            }
        }

        auto nodo_simbolo = std::make_unique<NodoMatematico>();
        nodo_simbolo->tipo = TipoNodoMatematico::Simbolo;
        nodo_simbolo->simbolo = s;
        raiz->hijos.push_back(std::move(nodo_simbolo));
    }

    return raiz;
}

std::string GenerarLaTeX(const NodoMatematico* nodo) {
    if (!nodo) return "";

    std::string resultado;
    switch (nodo->tipo) {
        case TipoNodoMatematico::Fila:
            for (const auto& hijo : nodo->hijos) {
                resultado += GenerarLaTeX(hijo.get());
            }
            break;
        case TipoNodoMatematico::Simbolo:
            if (nodo->simbolo) {
                resultado += nodo->simbolo->texto;
            }
            break;
        case TipoNodoMatematico::Fraccion:
            if (nodo->hijos.size() == 2) {
                resultado += "\\frac{" + GenerarLaTeX(nodo->hijos[0].get()) + "}{" + GenerarLaTeX(nodo->hijos[1].get()) + "}";
            }
            break;
        case TipoNodoMatematico::Superindice:
            if (nodo->hijos.size() == 2) {
                resultado += GenerarLaTeX(nodo->hijos[0].get()) + "^{" + GenerarLaTeX(nodo->hijos[1].get()) + "}";
            }
            break;
    }
    return resultado;
}

ValorConUnidad MultiplicarValores(const ValorConUnidad& a, const ValorConUnidad& b) {
    double exp_a = static_cast<double>(a.prefijo);
    double exp_b = static_cast<double>(b.prefijo);
    
    double val_a = a.valor * std::pow(10.0, exp_a);
    double val_b = b.valor * std::pow(10.0, exp_b);

    ValorConUnidad resultado;
    resultado.valor = val_a * val_b;
    resultado.prefijo = PrefijoSI::Ninguno;
    
    if ((a.unidad == UnidadSI::Amperio && b.unidad == UnidadSI::Ohmio) ||
        (a.unidad == UnidadSI::Ohmio && b.unidad == UnidadSI::Amperio)) {
        resultado.unidad = UnidadSI::Voltio;
    } else {
        resultado.unidad = UnidadSI::SinUnidad; 
    }

    return resultado;
}

} // namespace tinta
