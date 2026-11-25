// API en C del núcleo de tinta.
//
// Existe por una razón concreta: **Swift importa cabeceras de C directamente**,
// sin puente de Objective-C. Eso quita del proyecto todo el Objective-C++, que
// era código que no se puede compilar en Windows y que por tanto solo se podía
// verificar gastando minutos de runner macOS.
//
// Regla que impone este archivo: si algo se puede expresar en C, se expresa en C
// y se prueba aquí. El Swift se queda en entrada, pintado y widgets.
//
// Sirve igual para Kotlin en Android por JNI, cuando le toque el turno.
#ifndef TINTA_API_H
#define TINTA_API_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Sesión de edición: una página con su historial y su trazo en curso.
typedef struct TintaSesion TintaSesion;

TintaSesion* tinta_sesion_crear(void);
void tinta_sesion_liberar(TintaSesion* sesion);

// --- Trazo -----------------------------------------------------------------

void tinta_comenzar_trazo(TintaSesion* sesion, float ancho, uint32_t color_argb,
                          bool resaltador);

typedef struct {
  float opacidad;
  float exponente_presion;
  float factor_ancho_minimo;
  float velocidad_ancho_minimo;
  int suavizado_ancho;
  float estabilizacion;
  float taper_inicio;
  float taper_fin;
  float sensibilidad_inclinacion;
  int textura;
  int nivel_refinado;
} TintaAjustesPluma;

void tinta_pluma_configurar(TintaSesion* sesion, TintaAjustesPluma ajustes);
TintaAjustesPluma tinta_pluma_ajustes(const TintaSesion* sesion);
// Cambia solo la geometría derivada; las muestras originales quedan intactas.
bool tinta_trazo_refinar(TintaSesion* sesion, size_t indice, int nivel);
bool tinta_refinar_todos(TintaSesion* sesion, int nivel);

// `presion` negativa significa que el dispositivo no la reporta, que no es lo
// mismo que presión cero. `tiempo_s` es el timestamp del evento, en segundos.
void tinta_anadir_muestra(TintaSesion* sesion, float x, float y, float presion,
                          float altitud, float azimut, double tiempo_s);

// Variante identificada para propiedades que el digitalizador entregó como
// estimadas. Devuelve un identificador efímero distinto de cero, o cero si no
// había un trazo en curso. La capa nativa lo conserva hasta que el sistema deja
// de esperar actualizaciones para esa muestra.
uint64_t tinta_anadir_muestra_actualizable(
    TintaSesion* sesion, float x, float y, float presion, float altitud,
    float azimut, double tiempo_s);

typedef enum {
  TintaPropiedadMuestraPosicion = 1u << 0,
  TintaPropiedadMuestraFuerza = 1u << 1,
  TintaPropiedadMuestraAltitud = 1u << 2,
  TintaPropiedadMuestraAzimut = 1u << 3,
} TintaPropiedadMuestra;

// Sustituye únicamente las propiedades indicadas. Funciona tanto mientras el
// trazo está en curso como después de terminarlo y mantiene coherentes las
// copias de deshacer/rehacer. `tinta_finalizar_actualizaciones_muestra` libera
// el identificador cuando UIKit ya no espera más correcciones.
bool tinta_actualizar_muestra(TintaSesion* sesion, uint64_t id_muestra,
                              uint32_t propiedades, float x, float y,
                              float presion, float altitud, float azimut);
bool tinta_finalizar_actualizaciones_muestra(TintaSesion* sesion,
                                             uint64_t id_muestra);

// Muestras provisionales: aquí entran las de `predictedTouches`. Se dibujan pero
// no se guardan, y se limpian en cada frame. Si hay provisionales, el predictor
// del núcleo no entra: manda el sistema.
void tinta_limpiar_provisionales(TintaSesion* sesion);
void tinta_anadir_provisional(TintaSesion* sesion, float x, float y,
                              float presion, float altitud, float azimut,
                              double tiempo_s);

// Devuelve el índice estable de inserción o -1 si no había muestras.
// Un resaltador horizontal que cruza escritura existente se inserta detrás de
// esos trazos para no taparlos; sigue siendo una sola operación de historial.
int64_t tinta_terminar_trazo(TintaSesion* sesion);
// Asigna la identidad estable antes de copiar el trazo al historial. Así un
// deshacer/rehacer conserva el mismo ID colaborativo.
int64_t tinta_terminar_trazo_con_id(TintaSesion* sesion, const char* id);
void tinta_descartar_trazo(TintaSesion* sesion);

// --- Predicción ------------------------------------------------------------

void tinta_set_prediccion(TintaSesion* sesion, bool activa);
void tinta_set_horizonte_ms(TintaSesion* sesion, float ms);
size_t tinta_muestras_predichas(const TintaSesion* sesion);

// --- Geometría para la GPU -------------------------------------------------

// Pares (x, y) en píxeles lógicos, listos para subir a un buffer de vértices.
// `cantidad` sale en número de vértices, no de floats. El puntero vale hasta la
// siguiente llamada a cualquier función de esta API.
const float* tinta_vertices_en_curso(TintaSesion* sesion, size_t* cantidad);
const float* tinta_vertices_trazo(TintaSesion* sesion, size_t indice,
                                  size_t* cantidad, uint32_t* color_argb);
// La misma geometría, recortada al timestamp monotónico absoluto indicado.
// Es la operación usada por Nota Viva para reconstruir el trazo paso a paso.
const float* tinta_vertices_trazo_hasta(TintaSesion* sesion, size_t indice,
                                        double tiempo_s, size_t* cantidad,
                                        uint32_t* color_argb);
uint32_t tinta_color_en_curso(const TintaSesion* sesion);

// --- Estado ----------------------------------------------------------------

size_t tinta_cantidad_trazos(const TintaSesion* sesion);
bool tinta_hay_trazo_en_curso(const TintaSesion* sesion);
double tinta_frecuencia_hz(const TintaSesion* sesion);
// Rango absoluto de los timestamps de tinta. Devuelve false si la página no
// tiene muestras cronometradas.
bool tinta_rango_temporal(const TintaSesion* sesion, double* inicio_s,
                          double* fin_s);

// Identidad y formato unitario para sincronización conflict-free. Importar dos
// veces un trazo con el mismo ID es idempotente. El puntero de serialización y
// el texto del ID valen hasta la siguiente llamada a esta sesión.
const char* tinta_trazo_id(TintaSesion* sesion, size_t indice);
bool tinta_trazo_asignar_id(TintaSesion* sesion, size_t indice, const char* id);
const uint8_t* tinta_trazo_serializar(TintaSesion* sesion, size_t indice,
                                      size_t* cantidad);
// Centros originales de las muestras [x,y]. El puntero vive hasta la próxima
// llamada a esta sesión y `cantidad_puntos` cuenta pares, no floats.
const float* tinta_trazo_puntos(TintaSesion* sesion, size_t indice,
                                size_t* cantidad_puntos);
bool tinta_trazo_importar(TintaSesion* sesion, const uint8_t* datos,
                          size_t cantidad);
// Importa la geometría como una copia nueva. A diferencia de la ruta CRDT,
// sustituye el ID serializado por `nuevo_id`; sirve para arrastrar una
// selección entre páginas sin crear colisiones de identidad.
bool tinta_trazo_importar_copia(TintaSesion* sesion, const uint8_t* datos,
                                size_t cantidad, const char* nuevo_id);
bool tinta_trazo_eliminar_id(TintaSesion* sesion, const char* id);

// --- Edición ---------------------------------------------------------------

void tinta_deshacer(TintaSesion* sesion);
void tinta_rehacer(TintaSesion* sesion);
bool tinta_puede_deshacer(const TintaSesion* sesion);
bool tinta_puede_rehacer(const TintaSesion* sesion);
void tinta_limpiar(TintaSesion* sesion);

// Borrador circular. `parcial` a true parte el trazo por donde pasa; a false
// borra el trazo entero que toque.
bool tinta_borrar(TintaSesion* sesion, float x, float y, float radio,
                  bool parcial);
// Filtros: 0 todos, 1 solo resaltador, 2 solo color RGB exacto,
// 3 solo escritura (trazos que no son resaltador). La capa nativa resuelve
// "capa actual" porque esa pertenencia no forma parte de la tinta abierta.
bool tinta_borrar_filtrado(TintaSesion* sesion, float x, float y, float radio,
                           bool parcial, int filtro, uint32_t color_argb);

// Selección de tinta por lazo. `puntos_xy` contiene pares x,y. El puntero de
// salida pertenece a la sesión y vale hasta la siguiente llamada a esa sesión.
const size_t* tinta_lazo_seleccionar_trazos(TintaSesion* sesion,
                                            const float* puntos_xy,
                                            size_t cantidad_puntos,
                                            bool requerir_completo,
                                            size_t* cantidad_indices);

// Selección avanzada. Los tres punteros pertenecen a la sesión y conservan su
// validez hasta la próxima llamada a esa sesión. `filtro`: 0 todo, 1 tinta,
// 2 texto, 3 imágenes, 4 PDF. Un color ARGB de 0 desactiva el filtro por color.
typedef struct {
  const size_t* trazos;
  size_t cantidad_trazos;
  const size_t* textos;
  size_t cantidad_textos;
  const size_t* medios;
  size_t cantidad_medios;
} TintaSeleccionInfo;

TintaSeleccionInfo tinta_seleccion_poligono(
    TintaSesion* sesion, const float* puntos_xy, size_t cantidad_puntos,
    int filtro, uint32_t color_argb, bool requerir_completo);
TintaSeleccionInfo tinta_seleccion_rectangulo(
    TintaSesion* sesion, float x, float y, float ancho, float alto,
    int filtro, uint32_t color_argb, bool requerir_completo);
TintaSeleccionInfo tinta_seleccion_elipse(
    TintaSesion* sesion, float centro_x, float centro_y, float radio_x,
    float radio_y, int filtro, uint32_t color_argb,
    bool requerir_completo);
TintaSeleccionInfo tinta_seleccion_pincel(
    TintaSesion* sesion, const float* puntos_xy, size_t cantidad_puntos,
    float radio, int filtro, uint32_t color_argb,
    bool requerir_completo);
TintaSeleccionInfo tinta_seleccion_todo(TintaSesion* sesion, int filtro,
                                        uint32_t color_argb);

// Edición de una selección. Todas registran una sola operación en el historial.
// La transformación usa radianes y un pivote en coordenadas del documento.
bool tinta_trazos_transformar(TintaSesion* sesion, const size_t* indices,
                              size_t cantidad, float desplazar_x,
                              float desplazar_y, float escala,
                              float rotacion, float pivote_x, float pivote_y,
                              bool reflejar_horizontal,
                              bool reflejar_vertical);
bool tinta_trazos_eliminar(TintaSesion* sesion, const size_t* indices,
                           size_t cantidad);
bool tinta_trazos_recolorear(TintaSesion* sesion, const size_t* indices,
                             size_t cantidad, uint32_t color_argb);

// Operaciones sobre una selección mixta de tinta, texto y medios. La escala
// por eje permite edición proporcional o libre sin cambiar la API antigua.
bool tinta_seleccion_transformar(
    TintaSesion* sesion, const size_t* trazos, size_t cantidad_trazos,
    const size_t* textos, size_t cantidad_textos, const size_t* medios,
    size_t cantidad_medios, float desplazar_x, float desplazar_y,
    float escala_x, float escala_y, float rotacion, float pivote_x,
    float pivote_y, bool reflejar_horizontal, bool reflejar_vertical);
bool tinta_seleccion_eliminar(
    TintaSesion* sesion, const size_t* trazos, size_t cantidad_trazos,
    const size_t* textos, size_t cantidad_textos, const size_t* medios,
    size_t cantidad_medios);
TintaSeleccionInfo tinta_seleccion_duplicar(
    TintaSesion* sesion, const size_t* trazos, size_t cantidad_trazos,
    const size_t* textos, size_t cantidad_textos, const size_t* medios,
    size_t cantidad_medios, float desplazar_x, float desplazar_y);
TintaSeleccionInfo tinta_seleccion_ordenar(
    TintaSesion* sesion, const size_t* trazos, size_t cantidad_trazos,
    const size_t* textos, size_t cantidad_textos, const size_t* medios,
    size_t cantidad_medios, bool traer_al_frente);

// Reconoce el último trazo y lo sustituye por la forma ideal si la confianza
// alcanza. Devuelve el tipo reconocido: 0 ninguna, 1 línea, 2 rectángulo,
// 3 elipse, 4 triángulo, 5 flecha.
int tinta_enderezar_ultimo_trazo(TintaSesion* sesion);

// --- Papel y lienzo --------------------------------------------------------

// `tipo`: 0 blanco, 1 blanco sol, 2 crema, 3 gris, 4 oscuro, 5 negro.
// `pauta`: 0 ninguna, 1 rayado, 2 cuadrícula, 3 puntos, 4 pentagrama.
void tinta_set_papel(TintaSesion* sesion, int tipo, int pauta, float separacion,
                     float margen_izquierdo);
uint32_t tinta_color_papel(const TintaSesion* sesion);
uint32_t tinta_color_pauta(const TintaSesion* sesion);
// Color de tinta que le corresponde al papel actual. En papel oscuro devuelve
// una tinta clara: escribir negro sobre negro es el error fácil.
uint32_t tinta_color_tinta_sugerido(const TintaSesion* sesion);

// Segmentos de la pauta visibles en el rectángulo dado, como cuartetos
// (x0, y0, x1, y1). `cantidad` sale en número de segmentos.
const float* tinta_pauta_visible(TintaSesion* sesion, float x, float y,
                                 float ancho, float alto, size_t* cantidad);
// Quintetos (x0, y0, x1, y1, destacada), para pintar ejes y líneas maestras
// con otro grosor sin perder la API antigua de cuartetos.
const float* tinta_pauta_visible_avanzada(TintaSesion* sesion, float x, float y,
                                          float ancho, float alto,
                                          size_t* cantidad);

void tinta_set_papel_avanzado(TintaSesion* sesion, int pauta,
                              float separacion, float opacidad,
                              float grosor, uint32_t color_argb,
                              float margen_izquierdo, float margen_derecho,
                              float margen_superior, float margen_inferior,
                              bool mostrar_ejes, float eje_x, float eje_y,
                              bool pauta_al_exportar);

void tinta_set_limites(TintaSesion* sesion, bool ancho_infinito,
                       bool alto_infinito, float ancho_min, float ancho_max,
                       float alto_min, float alto_max);
float tinta_ancho_pagina(const TintaSesion* sesion);
float tinta_alto_pagina(const TintaSesion* sesion);
bool tinta_pagina_ancho_infinito(const TintaSesion* sesion);
bool tinta_pagina_alto_infinito(const TintaSesion* sesion);
// Crece el lienzo si la tinta se acerca al borde por un eje infinito.
bool tinta_crecer_si_hace_falta(TintaSesion* sesion, float margen);

// --- Página híbrida y vista de impresión ---------------------------------

// `modo`: 0 libro, 1 OneNote/infinito, 2 Goodnotes/páginas, 3 híbrido,
// 4 presentación, 5 pizarra, 6 infinito horizontal.
void tinta_pagina_aplicar_modo(TintaSesion* sesion, int modo);
// `unidad`: 0 px, 1 mm, 2 cm, 3 pulgadas. `reacomodo`: 0 conservar,
// 1 escalar proporcionalmente.
bool tinta_pagina_cambiar_tamano(TintaSesion* sesion, float ancho, float alto,
                                 int unidad, float ppp, int reacomodo);
bool tinta_pagina_ajustar_al_contenido(TintaSesion* sesion, float margen);
// `eje`: 0 horizontal/X, 1 vertical/Y.
bool tinta_pagina_insertar_espacio(TintaSesion* sesion, int eje, float desde,
                                   float cantidad);
// `borde`: 0 izquierdo, 1 derecho, 2 superior, 3 inferior.
bool tinta_pagina_mover_borde(TintaSesion* sesion, int borde, float delta,
                              bool desplazar_contenido);

typedef struct {
  size_t indice_trazo;
  float delta_x;
  float delta_y;
} TintaTransformacionTrazo;

// Calcula un reflow sin alterar el original. `modo`: 0 espaciado vertical,
// 1 alinear a la izquierda, 2 distribuir en filas. Devuelve el total del plan.
size_t tinta_reflujo_planificar(TintaSesion* sesion,
                                float x, float y, float ancho, float alto,
                                float factor_espaciado, int modo,
                                TintaTransformacionTrazo* resultado,
                                size_t capacidad);
bool tinta_reflujo_aplicar(TintaSesion* sesion,
                           const TintaTransformacionTrazo* plan,
                           size_t cantidad);
// Devuelve posiciones Y. El puntero vale hasta la siguiente llamada al API.
const float* tinta_pagina_cortes_impresion(TintaSesion* sesion,
                                           float alto_hoja,
                                           float margen_seguro,
                                           size_t* cantidad);

// --- Guardar y cargar ------------------------------------------------------

// Identidad durable del agregado. No es `TintaDocumentoId`: ese entero sigue
// siendo un handle efímero del proceso. La identidad debe establecerse antes
// de guardar/cargar y queda incluida en el formato canónico desde v5.
bool tinta_documento_asignar_identidad(TintaSesion* sesion,
                                        const char* identidad);
const char* tinta_documento_identidad(const TintaSesion* sesion);

// Serializa la página en el formato canónico. El puntero vale hasta la
// siguiente llamada a cualquier función de esta API.
const uint8_t* tinta_guardar(TintaSesion* sesion, size_t* cantidad);
bool tinta_cargar(TintaSesion* sesion, const uint8_t* datos, size_t cantidad);
// Importa deliberadamente una copia bajo una identidad nueva. Es la única ruta
// que puede cambiar la identidad embebida; apertura/autosave siempre usan
// `tinta_cargar` y fallan si reciben bytes de otro documento.
bool tinta_cargar_como_copia(TintaSesion* sesion, const uint8_t* datos,
                             size_t cantidad, const char* identidad_nueva);
// Motivo del último fallo de carga, para poder diagnosticar en vez de decir solo
// que no se pudo.
const char* tinta_ultimo_error(void);

// --- Grabación de pantalla -------------------------------------------------
//
// Aquí no se graba: se decide. Qué tamaño sale de la pantalla que hay, cuántos
// kbps le tocan, cuántos canales, qué cabe en el disco y qué combinación es
// imposible en este aparato. La capa de Swift obedece y llama a ReplayKit.

// `modo`:       0 solo audio, 1 pantalla + audio, 2 Nota viva (tinta + audio).
// `resolucion`: 0 nativa, 1 720p, 2 1080p, 3 1440p, 4 2160p, 5 personalizada.
// `calidad`:    0 baja, 1 media, 2 alta, 3 máxima.
// `codec`:      0 H.264, 1 HEVC.
// `canales`:    0 mono, 1 estéreo, 2 espacial (ambisónico de primer orden).
// `posicion`:   0 automática, 1 superior, 2 inferior, 3 frontal, 4 trasera.
// `patron`:     0 automático, 1 omnidireccional, 2 cardioide, 3 subcardioide,
//               4 estéreo.
// Los dos `kbps` a 0 significan automático: los calcula el núcleo.
typedef struct {
  int modo;
  int resolucion;
  int ancho_personalizado;
  int alto_personalizado;
  int fps;
  int calidad;
  int kbps_video;
  int codec;
  bool hdr;

  bool microfono;
  bool audio_de_la_app;
  int canales;
  int kbps_audio;
  int frecuencia_hz;
  int posicion_microfono;
  int patron_polar;

  bool mostrar_puntero;
} TintaAjustesGrabacion;

// Lo que el aparato puede de verdad. Lo rellena Swift preguntándole al sistema.
typedef struct {
  int ancho_pantalla;  // en píxeles reales, no en puntos
  int alto_pantalla;
  int fps_maximo;
  bool estereo;
  bool espacial;
  bool hevc;
  bool hdr;
  int canales_entrada_max;
} TintaCapacidades;

TintaAjustesGrabacion tinta_grabacion_por_defecto(void);

int tinta_grabacion_cantidad_preajustes(void);
TintaAjustesGrabacion tinta_grabacion_preajuste(int indice);
const char* tinta_grabacion_nombre_preajuste(int indice);
const char* tinta_grabacion_para_que_preajuste(int indice);

// Corrige lo imposible a lo más parecido que sí se puede en este aparato.
TintaAjustesGrabacion tinta_grabacion_ajustar(TintaAjustesGrabacion ajustes,
                                              TintaCapacidades cap);
// `motivo` recibe el primer problema en español, para enseñarlo en pantalla.
bool tinta_grabacion_validar(TintaAjustesGrabacion ajustes,
                             TintaCapacidades cap, char* motivo, size_t tam);

void tinta_grabacion_tamano(TintaAjustesGrabacion ajustes, TintaCapacidades cap,
                            int* ancho, int* alto);
int tinta_grabacion_kbps_video(TintaAjustesGrabacion ajustes,
                               TintaCapacidades cap);
int tinta_grabacion_kbps_audio(TintaAjustesGrabacion ajustes,
                               TintaCapacidades cap);
int tinta_grabacion_canales(TintaAjustesGrabacion ajustes);
double tinta_grabacion_mb_por_minuto(TintaAjustesGrabacion ajustes,
                                     TintaCapacidades cap);
double tinta_grabacion_minutos_que_caben(TintaAjustesGrabacion ajustes,
                                         TintaCapacidades cap,
                                         double megabytes_libres);

// --- Puntero del lápiz del iPad ----------------------------------------------
//
// El puntero no es la pluma: el grosor es el ancho del trazo en el papel y el
// radio del puntero es la marca que señala en el aire dónde está la punta.

float tinta_puntero_radio_efectivo(float radio);
float tinta_puntero_grosor_anillo(float radio, float grosor);
uint32_t tinta_puntero_color_efectivo(uint32_t color_puntero,
                                      uint32_t color_pluma,
                                      bool seguir_color_pluma);
bool tinta_puntero_debe_verse(bool visible, bool hay_hover, bool dibujando,
                              bool mostrar_al_dibujar);

// --- Pantalla siempre activa -----------------------------------------------

bool tinta_pantalla_siempre_activa(bool siempre, bool al_grabar,
                                   bool al_escribir, bool grabando,
                                   bool escribiendo);

// --- Dictado ---------------------------------------------------------------

// Convierte la puntuación dictada en signos, quita el espacio sobrante delante
// del signo y pone las mayúsculas. Devuelve los bytes escritos, sin contar el
// terminador. Si `salida` es NULL devuelve el tamaño que haría falta.
size_t tinta_limpiar_dictado(const char* crudo, bool puntuacion_hablada,
                             bool mayuscula_inicial, char* salida, size_t tam);

// --- Importar PDF ----------------------------------------------------------

// `rotacion` es la que declara el propio PDF: una presentación apaisada suele
// venir como página vertical con rotación 90, y quien la ignore la importa de
// canto. Devuelve 0 vertical, 1 horizontal.
int tinta_pdf_orientacion(float ancho_pt, float alto_pt, int rotacion);

// `ajuste`: 0 tamaño real, 1 ancho del lienzo, 2 encajar.
// Devuelve el tamaño de la hoja en píxeles lógicos, ya con el margen de notas.
void tinta_pdf_tamano_pagina(float ancho_pt, float alto_pt, int rotacion,
                             float ppp, int ajuste, float ancho_objetivo,
                             float alto_objetivo, bool margen_para_notas,
                             float fraccion_margen, float* ancho, float* alto);

// Dónde cae el rectángulo del PDF dentro de esa hoja.
void tinta_pdf_marco(float ancho_pt, float alto_pt, int rotacion, float ppp,
                     int ajuste, float ancho_objetivo, float alto_objetivo,
                     float* x, float* y, float* ancho, float* alto);

// --- Recortar imágenes -----------------------------------------------------

// Compone un recorte nuevo sobre el que ya tenía la imagen.
//
// El recorte nuevo viene en fracciones de **lo que hoy se ve**, no del
// original. Componerlo así es lo que permite recortar dos veces seguidas sin
// que la segunda se calcule contra la imagen entera. Los cuatro primeros
// parámetros entran con el recorte actual y salen con el nuevo.
//
// El original nunca se toca: el recorte se guarda como recorte, así que
// siempre se puede deshacer.
void tinta_recorte_componer(float* recorte_x, float* recorte_y,
                            float* recorte_ancho, float* recorte_alto, float x,
                            float y, float ancho, float alto);

// --- Elementos colocables -------------------------------------------------

typedef struct {
  const char* id;
  const char* texto;
  float x, y, ancho, alto;
  float rotacion;
  bool alto_automatico;
  bool desde_dictado;
} TintaTextoInfo;

bool tinta_texto_anadir(TintaSesion* sesion, const char* id,
                        const char* texto, float x, float y,
                        float ancho, float alto, bool desde_dictado);
size_t tinta_texto_cantidad(const TintaSesion* sesion);
TintaTextoInfo tinta_texto_info(const TintaSesion* sesion, size_t indice);
bool tinta_texto_actualizar(TintaSesion* sesion, const char* id,
                            const char* texto, float x, float y,
                            float ancho, float alto, float rotacion);
bool tinta_texto_eliminar(TintaSesion* sesion, const char* id);

typedef struct {
  const char* id;
  const char* hash;
  const char* nombre_original;
  int tipo;
  float x, y, ancho, alto;
  float rotacion;
  float opacidad;
  float recorte_x, recorte_y, recorte_ancho, recorte_alto;
  int pagina_fuente;
  double segundo_inicio;
} TintaMedioInfo;

// `tipo`: 0 imagen, 1 vídeo, 2 PDF, 3 documento, 4 audio.
bool tinta_medio_anadir(TintaSesion* sesion, const char* id,
                        const char* hash, const char* nombre_original,
                        int tipo, float x, float y, float ancho, float alto);
size_t tinta_medio_cantidad(const TintaSesion* sesion);
TintaMedioInfo tinta_medio_info(const TintaSesion* sesion, size_t indice);
bool tinta_medio_actualizar_marco(TintaSesion* sesion, const char* id,
                                  float x, float y, float ancho, float alto,
                                  float rotacion, float opacidad);
bool tinta_medio_set_fuente(TintaSesion* sesion, const char* id,
                            int pagina_fuente, double segundo_inicio);
bool tinta_medio_recortar(TintaSesion* sesion, const char* id,
                          float x, float y, float ancho, float alto);
bool tinta_medio_quitar_recorte(TintaSesion* sesion, const char* id);
bool tinta_medio_eliminar(TintaSesion* sesion, const char* id);

// --- Ingeniería electrónica ----------------------------------------------

// `tipo` usa los valores 0...52 de TipoComponenteElectrico, definidos en
// ingenieria.h y reflejados por el enum Swift del mismo nombre. Un entero fuera
// de ese contrato se rechaza como tipo desconocido; nunca se convierte en R.
typedef struct {
  const char* id;
  int tipo;
  int nodo_a;
  int nodo_b;
  int nodo_control_a;
  int nodo_control_b;
  double valor;
  double auxiliar;
  double fase_grados;
  bool activo;
} TintaComponenteElectrico;

typedef struct { double real; double imaginario; } TintaComplejo;

// Punto de una gráfica de circuito. `eje` es voltaje de barrido, frecuencia en
// Hz o tiempo en segundos; `real`/`imaginario` contienen el voltaje del nodo.
typedef struct {
  double eje;
  double real;
  double imaginario;
} TintaMuestraCircuito;

// --- Matemáticas avanzadas locales ---------------------------------------

typedef struct {
  const char* nombre;
  double real;
  double imaginario;
} TintaVariableCompleja;

typedef struct {
  double x;
  TintaComplejo y;
  bool valida;
} TintaMuestraFuncion;

// Evalúa expresiones reales o complejas sin red. Admite variables, i/j,
// prefijos de ingeniería, trigonometría, logaritmos y funciones complejas.
bool tinta_matematica_evaluar(const char* expresion,
                              const TintaVariableCompleja* variables,
                              size_t cantidad_variables,
                              TintaComplejo* resultado,
                              char* error, size_t tam_error);

// Devuelve la cantidad total de muestras solicitadas. Escribe hasta
// `capacidad`; cada muestra indica si su resultado fue finito y válido.
size_t tinta_matematica_muestrear(const char* expresion,
                                  const char* variable,
                                  double inicio, double fin,
                                  size_t cantidad,
                                  const TintaVariableCompleja* variables,
                                  size_t cantidad_variables,
                                  TintaMuestraFuncion* muestras,
                                  size_t capacidad,
                                  char* error, size_t tam_error);

bool tinta_matematica_derivar(const char* expresion, const char* variable,
                              double x,
                              const TintaVariableCompleja* variables,
                              size_t cantidad_variables,
                              TintaComplejo* resultado,
                              char* error, size_t tam_error);

bool tinta_matematica_integrar(const char* expresion, const char* variable,
                               double inicio, double fin, int subintervalos,
                               const TintaVariableCompleja* variables,
                               size_t cantidad_variables,
                               TintaComplejo* resultado,
                               char* error, size_t tam_error);

bool tinta_matematica_convertir_unidad(double valor, const char* origen,
                                       const char* destino, double* resultado,
                                       char* error, size_t tam_error);

bool tinta_matriz_determinante(const TintaComplejo* valores, size_t orden,
                               TintaComplejo* resultado,
                               char* error, size_t tam_error);
size_t tinta_matriz_invertir(const TintaComplejo* valores, size_t orden,
                             TintaComplejo* resultado, size_t capacidad,
                             char* error, size_t tam_error);
size_t tinta_sistema_lineal_resolver(const TintaComplejo* coeficientes,
                                     const TintaComplejo* terminos,
                                     size_t orden, TintaComplejo* resultado,
                                     size_t capacidad,
                                     char* error, size_t tam_error);
bool tinta_vector_producto_punto(const TintaComplejo* a,
                                 const TintaComplejo* b, size_t cantidad,
                                 TintaComplejo* resultado,
                                 char* error, size_t tam_error);
size_t tinta_vector_producto_cruz(const TintaComplejo* a,
                                  const TintaComplejo* b,
                                  TintaComplejo* resultado, size_t capacidad,
                                  char* error, size_t tam_error);

bool tinta_matematica_buscar_raiz(const char* expresion, const char* variable,
                                  double estimacion,
                                  const TintaVariableCompleja* variables,
                                  size_t cantidad_variables,
                                  TintaComplejo* resultado,
                                  char* error, size_t tam_error);
// `lado`: -1 izquierdo, 0 bilateral, 1 derecho.
bool tinta_matematica_limite(const char* expresion, const char* variable,
                             double punto, int lado,
                             const TintaVariableCompleja* variables,
                             size_t cantidad_variables,
                             TintaComplejo* resultado,
                             char* error, size_t tam_error);
bool tinta_matematica_sumatoria(const char* expresion, const char* variable,
                                int64_t inicio, int64_t fin,
                                const TintaVariableCompleja* variables,
                                size_t cantidad_variables,
                                TintaComplejo* resultado,
                                char* error, size_t tam_error);
size_t tinta_matematica_fourier(const TintaComplejo* muestras, size_t cantidad,
                                bool inversa, TintaComplejo* resultado,
                                size_t capacidad);
bool tinta_matematica_laplace(const char* expresion, const char* variable,
                              TintaComplejo s, double tiempo_maximo, int pasos,
                              const TintaVariableCompleja* variables,
                              size_t cantidad_variables,
                              TintaComplejo* resultado,
                              char* error, size_t tam_error);

typedef struct { double t; TintaComplejo y; } TintaMuestraEDO;
size_t tinta_matematica_edo_primer_orden(
    const char* derivada, double t0, TintaComplejo y0, double t1,
    size_t pasos, const TintaVariableCompleja* variables,
    size_t cantidad_variables, TintaMuestraEDO* resultado, size_t capacidad,
    char* error, size_t tam_error);

// Devuelve la cantidad total de voltajes (incluye nodo 0), o -1 si falla.
// Si el búfer es menor, escribe lo que cabe y aun devuelve el total requerido.
int tinta_circuito_resolver_dc(const TintaComponenteElectrico* componentes,
                               size_t cantidad, double* voltajes,
                               size_t capacidad, char* error, size_t tam_error);
int tinta_circuito_resolver_ac(const TintaComponenteElectrico* componentes,
                               size_t cantidad, double frecuencia_hz,
                               TintaComplejo* voltajes, size_t capacidad,
                               char* error, size_t tam_error);

// Devuelven el total de muestras (aunque el búfer sea menor) o -1 al fallar.
// El barrido DC modifica temporalmente una fuente independiente por su índice.
int tinta_circuito_barrido_dc(
    const TintaComponenteElectrico* componentes, size_t cantidad,
    int indice_fuente, double inicio, double fin, int puntos, int nodo,
    TintaMuestraCircuito* muestras, size_t capacidad,
    char* error, size_t tam_error);
int tinta_circuito_barrido_ac(
    const TintaComponenteElectrico* componentes, size_t cantidad,
    double inicio_hz, double fin_hz, int puntos_por_decada, int nodo,
    TintaMuestraCircuito* muestras, size_t capacidad,
    char* error, size_t tam_error);
int tinta_circuito_transitorio(
    const TintaComponenteElectrico* componentes, size_t cantidad,
    double duracion_s, double paso_s, int nodo,
    TintaMuestraCircuito* muestras, size_t capacidad,
    char* error, size_t tam_error);

bool tinta_circuito_equivalente(const TintaComponenteElectrico* componentes,
                                size_t cantidad, int nodo_a, int nodo_b,
                                double* voltaje_thevenin,
                                double* resistencia_thevenin,
                                double* corriente_norton,
                                char* error, size_t tam_error);

double tinta_toolbox_divisor(double entrada_v, double r_superior,
                             double r_inferior);
double tinta_toolbox_opamp_no_inversor(double r_realimentacion,
                                       double r_tierra);
double tinta_toolbox_opamp_inversor(double r_entrada,
                                    double r_realimentacion);
double tinta_toolbox_corte_rc(double resistencia_ohm, double capacitancia_f);
double tinta_toolbox_resonancia_lc(double inductancia_h, double capacitancia_f);
double tinta_toolbox_constante_rc(double resistencia_ohm, double capacitancia_f);
double tinta_toolbox_a_db(double relacion, bool potencia);
double tinta_toolbox_desde_db(double db, bool potencia);

typedef struct {
  int tipo;
  float confianza;
  float terminal_ax, terminal_ay;
  float terminal_bx, terminal_by;
} TintaReconocimientoCircuito;

// Reconoce los últimos trazos sin borrarlos ni sustituirlos.
bool tinta_circuito_reconocer_ultimos(const TintaSesion* sesion,
                                      size_t cantidad_trazos,
                                      TintaReconocimientoCircuito* resultado);
bool tinta_circuito_reconocer_seleccion(const TintaSesion* sesion,
                                        const size_t* indices,
                                        size_t cantidad_trazos,
                                        TintaReconocimientoCircuito* resultado);
// Descriptor normalizado para ejemplos de reconocimiento almacenados en el
// dispositivo. Devuelve el total y escribe hasta `capacidad` valores.
size_t tinta_circuito_caracteristicas_ultimos(
    const TintaSesion* sesion, size_t cantidad_trazos,
    float* salida, size_t capacidad);
size_t tinta_circuito_caracteristicas_seleccion(
    const TintaSesion* sesion, const size_t* indices, size_t cantidad_trazos,
    float* salida, size_t capacidad);

// --- Panel lateral: las Casio dentro del cuaderno --------------------------

// Reparto de la pantalla entre el lienzo y el panel de la calculadora.
//
// Es geometría, así que vive aquí y se prueba en Windows; a Swift le queda
// colocar dos vistas en los rectángulos que salen de esto.
//
// `lado`: 0 = derecha, 1 = izquierda.
// Los cuatro sale-por-puntero pueden ser NULL si no interesan.
void tinta_panel_reparto(float ancho_total, bool visible, int lado,
                         float fraccion, float min_pt, float max_fraccion,
                         float ancho_minimo_para_dividir, float* origen_lienzo,
                         float* ancho_lienzo, float* origen_panel,
                         float* ancho_panel);

// ¿El panel tiene que cubrir la pantalla entera en vez de repartirla? Cierto en
// iPhone vertical y en cualquier ancho donde no quepan los dos.
bool tinta_panel_cubre_todo(float ancho_total, float min_pt, float max_fraccion,
                            float ancho_minimo_para_dividir);

// Al soltar el divisor, el ancho se pega a 1/3, 1/2 o 2/3. Devuelve 0 si hay
// que cerrar el panel, que es lo que significa arrastrarlo hasta el borde.
float tinta_panel_fraccion_al_soltar(float fraccion_cruda, float max_fraccion);

// --- Documentos y Split View -----------------------------------------------

typedef uint64_t TintaDocumentoId;

typedef enum {
  TintaViewportPrincipal = 0,
  TintaViewportSecundario = 1
} TintaViewport;

// Abre un documento nuevo (creando su sesión) y devuelve su ID.
TintaDocumentoId tinta_documento_abrir(void);

// Cierra el documento y libera su memoria.
void tinta_documento_cerrar(TintaDocumentoId id);

// --- Guardar y cargar en almacen ---------------------------------------------

struct TintaMetadatos {
  char id[64];
  char titulo[128];
  int64_t fecha_creacion;
  int64_t fecha_modificacion;
};

void tinta_almacen_configurar_ruta(const char* ruta_base);
// Retorna la cantidad total de cuadernos. Escribe hasta 'capacidad' en 'buffer'.
int tinta_almacen_listar(struct TintaMetadatos* buffer, int capacidad);
// Guarda el documento activo en la sesión dada.
bool tinta_almacen_guardar_sesion(TintaDocumentoId id);
// Crea una nueva sesión cargando el cuaderno dado. Retorna el ID de sesión.
TintaDocumentoId tinta_almacen_abrir_cuaderno(const char* id);

// Copia los IDs de los documentos abiertos a `ids` (hasta `max_cantidad`).
// Devuelve la cantidad total de documentos abiertos.
size_t tinta_documentos_abiertos(TintaDocumentoId* ids, size_t max_cantidad);

// Devuelve la sesión de un documento para usarla con el resto de la API.
TintaSesion* tinta_documento_sesion(TintaDocumentoId id);

// Establece el documento activo en un viewport dado.
void tinta_viewport_set_documento(TintaViewport viewport, TintaDocumentoId id);
TintaDocumentoId tinta_viewport_get_documento(TintaViewport viewport);

// Configura y obtiene el estado de vista (zoom y pan) de un viewport.
void tinta_viewport_set_vista(TintaViewport viewport, float zoom, float pan_x, float pan_y);
void tinta_viewport_get_vista(TintaViewport viewport, float* zoom, float* pan_x, float* pan_y);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // TINTA_API_H
