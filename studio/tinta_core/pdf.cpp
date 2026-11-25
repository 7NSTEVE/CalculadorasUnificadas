#include "pdf.h"

namespace tinta {

Medio DocumentoPdf::CrearPrintout(int pagina, float x, float y) const {
    Medio m;
    m.id = "printout_" + id + "_" + std::to_string(pagina);
    m.tipo = TipoMedio::Imagen;
    m.hash = hash;
    m.pagina_fuente = pagina;
    m.marco = Rect{x, y, 100.0f, 100.0f};
    return m;
}

void HacerLienzoInfinitoAlrededorDePdf(Pagina* pagina, float margen_inicial_px) {
    if (!pagina) return;
    
    pagina->limites.ancho_infinito = true;
    pagina->limites.alto_infinito = true;
    pagina->ancho += margen_inicial_px * 2.0f;
    pagina->alto += margen_inicial_px * 2.0f;
}

}
