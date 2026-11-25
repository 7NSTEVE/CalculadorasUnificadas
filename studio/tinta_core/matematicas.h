#pragma once

#include <vector>
#include <string>
#include <memory>
#include "tinta.h"

namespace tinta {

struct CajaContenedora {
    float min_x = 0.0f;
    float min_y = 0.0f;
    float max_x = 0.0f;
    float max_y = 0.0f;

    bool intersecta(const CajaContenedora& otra) const {
        return !(max_x < otra.min_x || min_x > otra.max_x ||
                 max_y < otra.min_y || min_y > otra.max_y);
    }
};

struct SimboloReconocido {
    std::string texto;
    CajaContenedora caja;
    // Punteros a los trazos originales. Se preservan sin destruir los datos originales.
    std::vector<const Trazo*> trazos_originales;
};

enum class TipoNodoMatematico {
    Fila,         // Secuencia horizontal
    Simbolo,      // Nodo hoja
    Fraccion,     // Numerador y denominador
    Superindice   // Base y exponente
};

struct NodoMatematico {
    TipoNodoMatematico tipo;
    
    const SimboloReconocido* simbolo = nullptr;
    std::vector<std::unique_ptr<NodoMatematico>> hijos;
};

// Analizador geométrico local de filas, fracciones y superíndices. Recibe
// símbolos ya reconocidos; no afirma reconocer escritura por sí mismo.
std::unique_ptr<NodoMatematico> AnalizarDisenoMatematico(const std::vector<SimboloReconocido>& simbolos);

// Genera código LaTeX básico
std::string GenerarLaTeX(const NodoMatematico* raiz);

// Unidades del Sistema Internacional y prefijos
enum class PrefijoSI {
    Pico = -12,
    Nano = -9,
    Micro = -6,
    Mili = -3,
    Ninguno = 0,
    Kilo = 3,
    Mega = 6,
    Giga = 9,
    Tera = 12
};

enum class UnidadSI {
    SinUnidad,
    Metro,
    Gramo,
    Segundo,
    Amperio,
    Kelvin,
    Mol,
    Candela,
    Ohmio,
    Voltio
};

struct ValorConUnidad {
    double valor;
    PrefijoSI prefijo = PrefijoSI::Ninguno;
    UnidadSI unidad = UnidadSI::SinUnidad;
};

ValorConUnidad MultiplicarValores(const ValorConUnidad& a, const ValorConUnidad& b);

} // namespace tinta
