// Importar PDF, y sacar una página del PDF al lienzo libre.
//
// PDFKit sabe abrir el archivo y pintar la página; lo que no sabe es qué tamaño
// de lienzo le toca a cada hoja, si viene apaisada, cuánto margen dejar para
// anotar al lado, ni dónde cae el rectángulo del PDF dentro de la página. Eso es
// geometría, así que vive aquí y se prueba en Windows.
#pragma once

#include <string>
#include <vector>

#include "documento.h"
#include "elementos.h"

namespace tinta {

// Una página tal como la describe el PDF.
//
// El tamaño va en puntos PostScript (1/72 pulgada), que es lo que devuelve
// `PDFPage.bounds(for: .mediaBox)`. La rotación es la que el propio PDF declara
// y hay que aplicarla: una presentación exportada apaisada suele venir como
// página vertical con rotación 90, y quien la ignore la importa de canto.
struct PaginaPdfFuente {
  float ancho_pt = 612.0f;   // carta vertical
  float alto_pt = 792.0f;
  int rotacion = 0;          // 0, 90, 180 o 270
};

// Tamaño ya con la rotación aplicada, en puntos.
void TamanoRotado(const PaginaPdfFuente& fuente, float* ancho, float* alto);

// Vertical u horizontal, mirando el lado largo después de rotar.
OrientacionPagina OrientacionDe(const PaginaPdfFuente& fuente);

enum class AjustePdf {
  TamanoReal,       // los puntos del PDF a la resolución elegida
  AnchoDelLienzo,   // se estira hasta el ancho objetivo, el alto lo sigue
  Encajar,          // cabe entero dentro del objetivo, sin deformar
};

struct AjustesImportarPdf {
  // Puntos por pulgada a los que se convierte. 150 es el punto donde el texto
  // de un PDF de clase se lee sin que la página pese como una foto.
  float ppp = 150.0f;
  AjustePdf ajuste = AjustePdf::TamanoReal;
  float ancho_objetivo = 1240.0f;
  float alto_objetivo = 1754.0f;

  TipoPapel papel = TipoPapel::Blanco;
  Pauta pauta = Pauta::Ninguna;

  // Margen en blanco a la derecha para anotar al lado de la diapositiva, como
  // fracción del ancho del PDF. Es lo que hace útil importar presentaciones:
  // escribir encima tapa la diapositiva.
  bool margen_para_notas = false;
  float fraccion_margen = 0.35f;

  // Deja el alto sin tope aunque venga de un PDF, para poder seguir escribiendo
  // por debajo de la última línea de la diapositiva.
  bool alto_infinito = false;
};

// Tamaño de lienzo, en píxeles lógicos, que le corresponde a esa página.
void TamanoDePagina(const PaginaPdfFuente& fuente,
                    const AjustesImportarPdf& ajustes, float* ancho,
                    float* alto);

// Dónde cae el rectángulo del PDF dentro de la página, ya descontado el margen
// de notas y centrado si sobra sitio.
Rect MarcoDelPdf(const PaginaPdfFuente& fuente,
                 const AjustesImportarPdf& ajustes);

// Construye la página de la nota a partir de la del PDF.
//
// `hash` identifica el archivo PDF por contenido; el mismo PDF importado dos
// veces no duplica bytes.
Pagina PaginaDesdePdf(const PaginaPdfFuente& fuente, int indice,
                      const AjustesImportarPdf& ajustes,
                      const std::string& hash, const std::string& nombre);

// Importa el PDF entero. Cada página conserva su propia orientación: una
// presentación con diapositivas apaisadas y anexos verticales entra mezclada,
// que es como vienen de verdad.
std::vector<Pagina> ImportarPdf(const std::vector<PaginaPdfFuente>& paginas,
                                const AjustesImportarPdf& ajustes,
                                const std::string& hash,
                                const std::string& nombre);

// Saca la página del PDF al lienzo libre.
//
// La diapositiva pasa a ser un elemento más que se puede mover, escalar y rodear
// de notas, en vez de ser el fondo fijo de su hoja. Se lleva consigo la tinta
// que ya tuviera escrita encima, desplazada y escalada igual, porque separar la
// anotación de lo anotado es perderla.
//
// Devuelve false si esa página no venía de un PDF.
bool SoltarEnLienzoLibre(const Pagina& origen, Pagina* destino, float x, float y,
                         float escala);

}  // namespace tinta
