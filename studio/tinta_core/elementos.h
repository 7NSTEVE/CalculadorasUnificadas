// Elementos de una página que no son tinta: texto, imágenes y notas fijas.
//
// Todo el modelo vive en el núcleo. La capa nativa decodifica el PNG y dibuja la
// tipografía, pero qué hay, dónde está y cómo se recorta se decide aquí, donde
// se puede probar.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "tinta.h"

namespace tinta {

// Rectángulo en píxeles lógicos de la página.
struct Rect {
  float x = 0.0f;
  float y = 0.0f;
  float ancho = 0.0f;
  float alto = 0.0f;

  float derecha() const { return x + ancho; }
  float abajo() const { return y + alto; }
  bool vacio() const { return ancho <= 0.0f || alto <= 0.0f; }
  bool Contiene(const Punto& p) const;
  bool Intersecta(const Rect& otro) const;
};

// ---------------------------------------------------------------------------
// Texto
// ---------------------------------------------------------------------------

enum class Alineacion { Izquierda, Centro, Derecha, Justificado };

struct EstiloTexto {
  std::string familia = "system";
  float tamano = 17.0f;
  bool negrita = false;
  bool cursiva = false;
  bool subrayado = false;
  bool tachado = false;
  uint32_t color_argb = 0xFF1A1A1Au;
  uint32_t color_resaltado = 0x00000000u;  // 0 = sin resaltar
  Alineacion alineacion = Alineacion::Izquierda;
  float interlineado = 1.25f;
};

// Un cuadro de texto colocable libremente, al estilo del lienzo de OneNote.
struct CuadroTexto {
  std::string id;
  std::string texto;
  Rect marco;
  EstiloTexto estilo;
  float rotacion = 0.0f;
  // Si es true, el alto lo decide el contenido y no el marco. Es el
  // comportamiento normal al escribir; el marco fijo se usa al importar.
  bool alto_automatico = true;

  // De dictado por voz y no de teclado. Se guarda para poder marcar en la
  // interfaz lo que aún no ha revisado nadie.
  bool desde_dictado = false;
};

// ---------------------------------------------------------------------------
// Imágenes
// ---------------------------------------------------------------------------

enum class TipoMedio { Imagen, Video, Pdf, Documento, Audio };

// Un medio incrustado.
//
// Los bytes no viven aquí: se guardan aparte y se referencian por hash. Así una
// misma foto pegada en diez páginas ocupa una vez, y la sincronización puede
// subir el archivo una sola vez.
struct Medio {
  std::string id;
  std::string hash;            // identidad del contenido
  std::string nombre_original;
  TipoMedio tipo = TipoMedio::Imagen;

  Rect marco;                  // dónde se dibuja en la página
  float rotacion = 0.0f;
  float opacidad = 1.0f;

  // Recorte, en fracciones 0..1 del original. Se guarda como recorte y no
  // reescribiendo los píxeles para que siempre se pueda deshacer: recortar
  // destruyendo el original es irreversible y es lo que hace GoodNotes.
  float recorte_x = 0.0f;
  float recorte_y = 0.0f;
  float recorte_ancho = 1.0f;
  float recorte_alto = 1.0f;

  // Para PDF y vídeo: qué página o en qué segundo.
  int pagina_fuente = 0;
  double segundo_inicio = 0.0;

  bool recorte_valido() const;
  // Rectángulo del original que queda visible, en píxeles del original.
  Rect RegionVisible(float ancho_original, float alto_original) const;
};

// Aplica un recorte nuevo expresado en fracciones del **recorte actual**, no del
// original. Es lo que hace falta para poder recortar dos veces seguidas sin que
// la segunda se calcule contra la imagen entera.
void RecortarRelativo(Medio* medio, float x, float y, float ancho, float alto);

// ---------------------------------------------------------------------------
// Notas fijas
// ---------------------------------------------------------------------------

// Hasta dónde sigue visible una nota.
//
// Es la diferencia entre pegar un papelito en una hoja y clavarlo en el
// escritorio: la nota de alcance Global sigue en pantalla aunque cierres la
// página, cambies de página o cambies de cuaderno.
enum class AlcanceNota {
  Pagina,    // vive en su página y desaparece al cambiar de página
  Cuaderno,  // sigue visible en todas las páginas de su cuaderno
  Global,    // sigue visible aunque cambies de cuaderno
};

struct NotaFija {
  std::string id;
  std::string texto;
  EstiloTexto estilo;

  // En alcance Pagina la posición es de la página y se desplaza con el lienzo.
  // En Cuaderno y Global es de la pantalla: la nota se queda quieta.
  Punto posicion{24.0f, 24.0f};
  float ancho = 220.0f;
  float alto = 160.0f;

  uint32_t color_argb = 0xFFFFF3B0u;  // amarillo papelito
  AlcanceNota alcance = AlcanceNota::Pagina;
  bool contraida = false;
  bool anclada = true;  // false mientras se arrastra

  // Trazos escritos a mano dentro de la nota. Una nota se puede escribir con el
  // lápiz igual que la página.
  std::vector<Trazo> trazos;
};

}  // namespace tinta
