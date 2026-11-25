// Grabación de pantalla: qué se graba y con qué calidad.
//
// Aquí no se graba nada. Lo que vive aquí es **la decisión**: qué resolución
// sale de la pantalla que hay, cuántos kbps le tocan a ese tamaño con esos FPS,
// cuántos canales tiene el audio, qué cabe en el disco que queda y qué
// combinaciones son imposibles en este aparato.
//
// Está en el núcleo por lo de siempre: esto se prueba en Windows en segundos.
// La capa nativa (ReplayKit + AVAssetWriter en iPadOS) solo obedece.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace tinta {

// Tres productos distintos, no tres nombres para el mismo MP4.
//
// - SoloAudio escribe un archivo AAC/M4A y no captura la pantalla.
// - PantallaConAudio usa ReplayKit y AVAssetWriter.
// - NotaViva guarda audio más la tinta vectorial con sus marcas de tiempo; se
//   reproduce paso a paso y ocupa mucho menos que un vídeo de dos horas.
enum class ModoCaptura { SoloAudio, PantallaConAudio, NotaViva };

// ---------------------------------------------------------------------------
// Vídeo
// ---------------------------------------------------------------------------

enum class Resolucion {
  Nativa,          // la de la pantalla, sin reescalar
  R720p,
  R1080p,
  R1440p,
  R2160p,
  Personalizada,   // ancho y alto a mano
};

struct TamanoVideo {
  int ancho = 0;
  int alto = 0;

  bool valido() const { return ancho >= 2 && alto >= 2; }
  int lado_corto() const { return ancho < alto ? ancho : alto; }
  long long pixeles() const {
    return static_cast<long long>(ancho) * static_cast<long long>(alto);
  }
};

enum class CalidadVideo { Baja, Media, Alta, Maxima };

// H.264 lo reproduce cualquier cosa; HEVC pesa casi la mitad a igual calidad
// pero no lo abren todos los reproductores viejos.
enum class CodecVideo { H264, HEVC };

// Los codificadores de hardware trabajan sobre macrobloques: un lado impar
// obliga a rellenar y algunos reproductores muestran una franja verde.
constexpr int kLadoMinimo = 160;
constexpr int kLadoMaximo = 4096;
constexpr int kFpsMinimo = 1;
constexpr int kFpsMaximo = 240;
constexpr int kKbpsVideoMinimo = 200;
constexpr int kKbpsVideoMaximo = 200000;

struct AjustesVideo {
  Resolucion resolucion = Resolucion::Nativa;
  int ancho_personalizado = 1920;
  int alto_personalizado = 1080;
  int fps = 60;
  CalidadVideo calidad = CalidadVideo::Alta;
  // 0 significa automático: lo calcula el núcleo a partir de tamaño, FPS,
  // calidad y códec. Cualquier otro valor manda sobre la calidad.
  int kbps = 0;
  CodecVideo codec = CodecVideo::HEVC;
  bool hdr = false;
};

// ---------------------------------------------------------------------------
// Audio
// ---------------------------------------------------------------------------

// Mono, estéreo o espacial.
//
// «Espacial» es ambisónico de primer orden (cuatro canales). En iPadOS está
// marcado como no verificado: si el aparato no lo ofrece, `AjustarACapacidades`
// lo baja a estéreo en vez de fallar en mitad de la grabación.
enum class ModoCanales { Mono, Estereo, Espacial };

// El patrón de captación del micrófono virtualizado.
//
// Los 4–5 micrófonos de un iPad Pro **no se exponen por separado**: iOS no deja
// grabar cada cápsula. Lo que sí da `AVAudioSession` es elegir la fuente por
// posición y encima un patrón polar, que es un micrófono virtual construido a
// partir de varias cápsulas. Esto modela exactamente eso, ni más ni menos.
enum class PatronPolar {
  Automatico,
  Omnidireccional,  // capta toda la sala: la clase entera
  Cardioide,        // capta de frente: el que habla
  Subcardioide,     // intermedio
  Estereo,          // el par estéreo del aparato
};

enum class PosicionMicrofono {
  Automatica,
  Superior,
  Inferior,
  Frontal,
  Trasera,
};

constexpr int kKbpsAudioMinimo = 24;
constexpr int kKbpsAudioMaximo = 512;

struct AjustesAudio {
  bool microfono = true;
  bool audio_de_la_app = false;
  ModoCanales canales = ModoCanales::Estereo;
  // 0 = automático, igual que en vídeo.
  int kbps = 0;
  int frecuencia_hz = 48000;
  PosicionMicrofono posicion = PosicionMicrofono::Automatica;
  PatronPolar patron = PatronPolar::Automatico;
  // Nombre de la fuente elegida tal como la reporta el sistema. Es informativo:
  // el núcleo no lo interpreta.
  std::string fuente;
};

struct AjustesGrabacion {
  ModoCaptura modo = ModoCaptura::NotaViva;
  AjustesVideo video;
  AjustesAudio audio;
  // El puntero del lápiz del iPad y los toques, dentro del vídeo.
  bool mostrar_puntero = true;
};

// ---------------------------------------------------------------------------
// Lo que el aparato puede de verdad
// ---------------------------------------------------------------------------

// Se rellena en la capa nativa preguntándole al sistema, no se adivina aquí.
struct CapacidadesGrabacion {
  TamanoVideo pantalla{2048, 2732};  // píxeles reales, no puntos
  int fps_maximo = 120;
  bool estereo = true;
  bool espacial = false;
  bool hevc = true;
  bool hdr = false;
  // El máximo que reporta la ruta seleccionada. Los micrófonos integrados de
  // iPad normalmente exponen uno o dos; una interfaz USB puede exponer más.
  int canales_entrada_max = 2;
  // Frecuencias que acepta la sesión de audio.
  std::vector<int> frecuencias{44100, 48000};
};

// ---------------------------------------------------------------------------
// Cálculo
// ---------------------------------------------------------------------------

// Tamaño real del vídeo, con la relación de aspecto de la pantalla y los dos
// lados pares.
//
// Nunca amplía por encima de lo nativo: grabar más grande que la pantalla gasta
// bitrate en píxeles inventados. Pedir 4K en un panel de 1080 devuelve 1080.
TamanoVideo ResolverTamano(const AjustesVideo& video,
                           const CapacidadesGrabacion& cap);

// Bitrate que le corresponde a ese tamaño y esos FPS con esa calidad.
//
// Sale de bits por píxel y por frame, que es como se dimensiona de verdad: el
// mismo número de kbps es holgado en 720p y miserable en 4K.
int KbpsVideoSugerido(const TamanoVideo& tamano, int fps, CalidadVideo calidad,
                      CodecVideo codec);

// El que se va a usar: el que puso el usuario si puso alguno, y si no el sugerido.
int KbpsVideoEfectivo(const AjustesVideo& video,
                      const CapacidadesGrabacion& cap);

bool IncluyeVideo(ModoCaptura modo);

int CanalesDe(ModoCanales modo);
int KbpsAudioSugerido(ModoCanales canales, int frecuencia_hz);
int KbpsAudioEfectivo(const AjustesAudio& audio,
                      const CapacidadesGrabacion& cap);

// Megabytes por minuto, en MB decimales, que es como los cuenta iOS.
double MegabytesPorMinuto(const AjustesGrabacion& ajustes,
                          const CapacidadesGrabacion& cap);

// Cuántos minutos caben en el espacio libre. Sirve para avisar antes de empezar
// una clase de dos horas, no después.
double MinutosQueCaben(const AjustesGrabacion& ajustes,
                       const CapacidadesGrabacion& cap, double megabytes_libres);

// Devuelve una copia con todo lo imposible corregido a lo más parecido que sí
// se puede: FPS por encima del panel, espacial sin hardware, HEVC sin
// codificador, HDR sin soporte, frecuencia no admitida.
AjustesGrabacion AjustarACapacidades(AjustesGrabacion ajustes,
                                     const CapacidadesGrabacion& cap);

// Dice si los ajustes son grabables tal cual. `motivo` explica el primer
// problema en español, para poder enseñarlo en pantalla en vez de un código.
bool Validar(const AjustesGrabacion& ajustes, const CapacidadesGrabacion& cap,
             std::string* motivo);

struct PreajusteGrabacion {
  std::string nombre;
  std::string para_que;
  AjustesGrabacion ajustes;
};

// Preajustes pensados para clase, no genéricos.
std::vector<PreajusteGrabacion> Preajustes();

// ---------------------------------------------------------------------------
// La grabación en curso
// ---------------------------------------------------------------------------

enum class EstadoGrabacion { Detenida, Grabando, Pausada };

// Contabilidad de la grabación: qué transiciones valen y cuánto se lleva
// grabado de verdad.
//
// El tiempo en pausa **no cuenta**. Parece obvio y es justo lo que se rompe:
// si el contador de pantalla suma la pausa, el minutaje no coincide con el
// archivo y el aviso de «te queda medio giga» miente.
class SesionGrabacion {
 public:
  EstadoGrabacion estado() const { return estado_; }
  bool grabando() const { return estado_ == EstadoGrabacion::Grabando; }

  // Todas devuelven false si la transición no vale desde el estado actual.
  bool Comenzar(double t);
  bool Pausar(double t);
  bool Reanudar(double t);
  bool Detener(double t);

  // Segundos de vídeo escritos, sin contar las pausas.
  double SegundosGrabados(double ahora) const;
  double MegabytesEstimados(double ahora, const AjustesGrabacion& ajustes,
                            const CapacidadesGrabacion& cap) const;

 private:
  EstadoGrabacion estado_ = EstadoGrabacion::Detenida;
  double acumulado_ = 0.0;
  double ultimo_ = 0.0;
};

}  // namespace tinta
