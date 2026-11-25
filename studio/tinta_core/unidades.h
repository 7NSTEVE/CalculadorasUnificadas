#pragma once
#include <string>

namespace tinta {

struct Dimension {
    int longitud = 0;
    int masa = 0;
    int tiempo = 0;
    int corriente = 0;

    bool operator==(const Dimension& otra) const {
        return longitud == otra.longitud && masa == otra.masa && 
               tiempo == otra.tiempo && corriente == otra.corriente;
    }
    bool operator!=(const Dimension& otra) const {
        return !(*this == otra);
    }
    Dimension operator*(const Dimension& otra) const {
        return {longitud + otra.longitud, masa + otra.masa, tiempo + otra.tiempo, corriente + otra.corriente};
    }
    Dimension operator/(const Dimension& otra) const {
        return {longitud - otra.longitud, masa - otra.masa, tiempo - otra.tiempo, corriente - otra.corriente};
    }
};

struct ResultadoDimension {
    bool correcto = false;
    std::string error;
    Dimension dimension_izq;
    Dimension dimension_der;
};

ResultadoDimension AnalizarDimensiones(const std::string& expresion);

} // namespace tinta
