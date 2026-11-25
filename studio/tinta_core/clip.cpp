#include "clip.h"

namespace tinta {

Medio MedioDesdeClip(const RecorteClip& clip, const std::string& hash_imagen) {
    Medio m;
    m.id = "medio_clip_" + clip.id;
    m.tipo = TipoMedio::Imagen;
    m.hash = hash_imagen;
    m.marco = clip.marco;
    return m;
}

}
