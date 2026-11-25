#include "grabacion.h"

#include <algorithm>
#include <cmath>

namespace tinta {
namespace {

// Un lado impar obliga al codificador a rellenar el macrobloque y algunos
// reproductores pintan una franja verde en el borde.
int Par(int v) { return v - (v % 2); }

// Bits por píxel y por frame. Son los que da la experiencia con H.264: por
// debajo de 0,04 la tinta fina se deshace en bloques, y por encima de 0,18 el
// archivo crece sin que se note nada.
double BitsPorPixel(CalidadVideo calidad) {
  switch (calidad) {
    case CalidadVideo::Baja:
      return 0.040;
    case CalidadVideo::Media:
      return 0.070;
    case CalidadVideo::Alta:
      return 0.110;
    case CalidadVideo::Maxima:
      return 0.180;
  }
  return 0.110;
}

// HEVC consigue la misma calidad con algo más de un tercio menos de bitrate.
double FactorCodec(CodecVideo codec) {
  return codec == CodecVideo::HEVC ? 0.62 : 1.0;
}

int FrecuenciaAdmitida(int pedida, const CapacidadesGrabacion& cap) {
  if (cap.frecuencias.empty()) return pedida;
  for (int hz : cap.frecuencias) {
    if (hz == pedida) return pedida;
  }
  // La más cercana por debajo, y si no hay ninguna, la menor de todas.
  int mejor = cap.frecuencias.front();
  int distancia = std::abs(mejor - pedida);
  for (int hz : cap.frecuencias) {
    const int d = std::abs(hz - pedida);
    if (d < distancia) {
      distancia = d;
      mejor = hz;
    }
  }
  return mejor;
}

}  // namespace

// ---------------------------------------------------------------------------
// Vídeo
// ---------------------------------------------------------------------------

TamanoVideo ResolverTamano(const AjustesVideo& video,
                           const CapacidadesGrabacion& cap) {
  TamanoVideo nativo = cap.pantalla;
  if (!nativo.valido()) nativo = TamanoVideo{1920, 1080};

  TamanoVideo r = nativo;
  int objetivo_corto = 0;
  switch (video.resolucion) {
    case Resolucion::Nativa:
      break;
    case Resolucion::R720p:
      objetivo_corto = 720;
      break;
    case Resolucion::R1080p:
      objetivo_corto = 1080;
      break;
    case Resolucion::R1440p:
      objetivo_corto = 1440;
      break;
    case Resolucion::R2160p:
      objetivo_corto = 2160;
      break;
    case Resolucion::Personalizada:
      // A mano se respeta lo pedido, pero tampoco se amplía por encima de la
      // pantalla: esos píxeles no existen, se inventarían al reescalar.
      r.ancho = std::clamp(video.ancho_personalizado, kLadoMinimo,
                           std::min(nativo.ancho, kLadoMaximo));
      r.alto = std::clamp(video.alto_personalizado, kLadoMinimo,
                          std::min(nativo.alto, kLadoMaximo));
      break;
  }

  if (objetivo_corto > 0) {
    const int corto = nativo.lado_corto();
    // Se escala por el lado corto para no deformar: 1080p en un iPad 4:3 no es
    // 1920×1080, es el alto o el ancho puesto a 1080 y el otro lo que salga.
    const double escala =
        static_cast<double>(std::min(objetivo_corto, corto)) / corto;
    r.ancho = static_cast<int>(std::lround(nativo.ancho * escala));
    r.alto = static_cast<int>(std::lround(nativo.alto * escala));
  }

  r.ancho = std::max(Par(std::min(r.ancho, kLadoMaximo)), 2);
  r.alto = std::max(Par(std::min(r.alto, kLadoMaximo)), 2);
  return r;
}

int KbpsVideoSugerido(const TamanoVideo& tamano, int fps, CalidadVideo calidad,
                      CodecVideo codec) {
  if (!tamano.valido() || fps <= 0) return kKbpsVideoMinimo;
  const double bits = static_cast<double>(tamano.pixeles()) * fps *
                      BitsPorPixel(calidad) * FactorCodec(codec);
  const long long kbps = std::llround(bits / 1000.0);
  return static_cast<int>(std::clamp<long long>(kbps, kKbpsVideoMinimo,
                                                kKbpsVideoMaximo));
}

int KbpsVideoEfectivo(const AjustesVideo& video,
                      const CapacidadesGrabacion& cap) {
  const TamanoVideo tamano = ResolverTamano(video, cap);
  const int fps = std::clamp(video.fps, kFpsMinimo, kFpsMaximo);
  if (video.kbps > 0) {
    return std::clamp(video.kbps, kKbpsVideoMinimo, kKbpsVideoMaximo);
  }
  return KbpsVideoSugerido(tamano, fps, video.calidad, video.codec);
}

bool IncluyeVideo(ModoCaptura modo) {
  return modo == ModoCaptura::PantallaConAudio;
}

// ---------------------------------------------------------------------------
// Audio
// ---------------------------------------------------------------------------

int CanalesDe(ModoCanales modo) {
  switch (modo) {
    case ModoCanales::Mono:
      return 1;
    case ModoCanales::Estereo:
      return 2;
    case ModoCanales::Espacial:
      return 4;  // ambisónico de primer orden: W, X, Y, Z
  }
  return 2;
}

int KbpsAudioSugerido(ModoCanales canales, int frecuencia_hz) {
  // 96 kbps por canal es transparente para voz en AAC. El espacial no escala
  // lineal: los cuatro canales están muy correlacionados y el codificador lo
  // aprovecha.
  int base = 0;
  switch (canales) {
    case ModoCanales::Mono:
      base = 96;
      break;
    case ModoCanales::Estereo:
      base = 192;
      break;
    case ModoCanales::Espacial:
      base = 320;
      break;
  }
  // Por debajo de 32 kHz no hay agudos que codificar y gastar el mismo bitrate
  // es tirarlo.
  if (frecuencia_hz > 0 && frecuencia_hz < 32000) base = base / 2;
  return std::clamp(base, kKbpsAudioMinimo, kKbpsAudioMaximo);
}

int KbpsAudioEfectivo(const AjustesAudio& audio,
                      const CapacidadesGrabacion& cap) {
  if (!audio.microfono && !audio.audio_de_la_app) return 0;
  const int hz = FrecuenciaAdmitida(audio.frecuencia_hz, cap);
  const int kbps = audio.kbps > 0 ? audio.kbps
                                  : KbpsAudioSugerido(audio.canales, hz);
  return std::clamp(kbps, kKbpsAudioMinimo, kKbpsAudioMaximo);
}

// ---------------------------------------------------------------------------
// Tamaño del archivo
// ---------------------------------------------------------------------------

double MegabytesPorMinuto(const AjustesGrabacion& ajustes,
                          const CapacidadesGrabacion& cap) {
  const double kbps = (IncluyeVideo(ajustes.modo)
                           ? KbpsVideoEfectivo(ajustes.video, cap)
                           : 0) +
                      KbpsAudioEfectivo(ajustes.audio, cap);
  // kbps × 60 s ÷ 8 bits ÷ 1000 = MB decimales, que es como los cuenta iOS.
  return kbps * 60.0 / 8000.0;
}

double MinutosQueCaben(const AjustesGrabacion& ajustes,
                       const CapacidadesGrabacion& cap,
                       double megabytes_libres) {
  const double por_minuto = MegabytesPorMinuto(ajustes, cap);
  if (por_minuto <= 0.0 || megabytes_libres <= 0.0) return 0.0;
  return megabytes_libres / por_minuto;
}

// ---------------------------------------------------------------------------
// Corrección y validación
// ---------------------------------------------------------------------------

AjustesGrabacion AjustarACapacidades(AjustesGrabacion ajustes,
                                     const CapacidadesGrabacion& cap) {
  // Fuera de ReplayKit no existe una pista pública de «audio de otras apps».
  // Se apaga aquí para que el cálculo de tamaño y el archivo real coincidan.
  if (ajustes.modo != ModoCaptura::PantallaConAudio) {
    ajustes.audio.audio_de_la_app = false;
  }
  const int tope_fps = std::clamp(cap.fps_maximo, kFpsMinimo, kFpsMaximo);
  ajustes.video.fps = std::clamp(ajustes.video.fps, kFpsMinimo, tope_fps);

  if (ajustes.video.codec == CodecVideo::HEVC && !cap.hevc) {
    ajustes.video.codec = CodecVideo::H264;
  }
  if (ajustes.video.hdr && !cap.hdr) ajustes.video.hdr = false;

  if (ajustes.video.kbps > 0) {
    ajustes.video.kbps =
        std::clamp(ajustes.video.kbps, kKbpsVideoMinimo, kKbpsVideoMaximo);
  }
  if (ajustes.video.resolucion == Resolucion::Personalizada) {
    const TamanoVideo t = ResolverTamano(ajustes.video, cap);
    ajustes.video.ancho_personalizado = t.ancho;
    ajustes.video.alto_personalizado = t.alto;
  }

  // Espacial sin hardware baja a estéreo, y estéreo sin hardware baja a mono.
  // Se degrada en cadena en vez de fallar: mejor grabar la clase en mono que no
  // grabarla.
  if (ajustes.audio.canales == ModoCanales::Espacial && !cap.espacial) {
    ajustes.audio.canales = ModoCanales::Estereo;
  }
  if (ajustes.audio.canales == ModoCanales::Estereo && !cap.estereo) {
    ajustes.audio.canales = ModoCanales::Mono;
  }
  while (CanalesDe(ajustes.audio.canales) >
             std::max(1, cap.canales_entrada_max) &&
         ajustes.audio.canales != ModoCanales::Mono) {
    ajustes.audio.canales = ajustes.audio.canales == ModoCanales::Espacial
                                ? ModoCanales::Estereo
                                : ModoCanales::Mono;
  }
  if (ajustes.audio.patron == PatronPolar::Estereo &&
      ajustes.audio.canales == ModoCanales::Mono) {
    ajustes.audio.patron = PatronPolar::Cardioide;
  }
  ajustes.audio.frecuencia_hz = FrecuenciaAdmitida(ajustes.audio.frecuencia_hz, cap);
  if (ajustes.audio.kbps > 0) {
    ajustes.audio.kbps =
        std::clamp(ajustes.audio.kbps, kKbpsAudioMinimo, kKbpsAudioMaximo);
  }
  return ajustes;
}

bool Validar(const AjustesGrabacion& ajustes, const CapacidadesGrabacion& cap,
             std::string* motivo) {
  auto fallar = [motivo](const char* texto) {
    if (motivo) *motivo = texto;
    return false;
  };

  if (ajustes.modo == ModoCaptura::SoloAudio && !ajustes.audio.microfono) {
    return fallar("Solo audio necesita un microfono de entrada.");
  }
  if (ajustes.modo != ModoCaptura::PantallaConAudio &&
      ajustes.audio.audio_de_la_app) {
    return fallar("El audio de la app solo se captura junto con la pantalla.");
  }

  if (IncluyeVideo(ajustes.modo)) {
    const TamanoVideo tamano = ResolverTamano(ajustes.video, cap);
    if (!tamano.valido()) return fallar("El tamano de video no es valido.");
    if (ajustes.video.fps < kFpsMinimo) return fallar("Los FPS deben ser al menos 1.");
    if (ajustes.video.fps > cap.fps_maximo) {
      return fallar("Esos FPS superan lo que da la pantalla.");
    }
    if (ajustes.video.codec == CodecVideo::HEVC && !cap.hevc) {
      return fallar("Este aparato no codifica HEVC.");
    }
    if (ajustes.video.hdr && !cap.hdr) return fallar("Este aparato no graba HDR.");
    if (ajustes.video.kbps > 0 &&
        (ajustes.video.kbps < kKbpsVideoMinimo ||
         ajustes.video.kbps > kKbpsVideoMaximo)) {
      return fallar("El bitrate de video esta fuera de rango.");
    }
  }

  if (ajustes.audio.microfono || ajustes.audio.audio_de_la_app) {
    if (ajustes.audio.canales == ModoCanales::Espacial && !cap.espacial) {
      return fallar("Este aparato no graba audio espacial.");
    }
    if (ajustes.audio.canales == ModoCanales::Estereo && !cap.estereo) {
      return fallar("Este aparato no graba en estereo.");
    }
    if (CanalesDe(ajustes.audio.canales) >
        std::max(1, cap.canales_entrada_max)) {
      return fallar("La entrada elegida no ofrece tantos canales.");
    }
    if (ajustes.audio.kbps > 0 &&
        (ajustes.audio.kbps < kKbpsAudioMinimo ||
         ajustes.audio.kbps > kKbpsAudioMaximo)) {
      return fallar("El bitrate de audio esta fuera de rango.");
    }
    if (FrecuenciaAdmitida(ajustes.audio.frecuencia_hz, cap) !=
        ajustes.audio.frecuencia_hz) {
      return fallar("Esa frecuencia de muestreo no la admite la sesion de audio.");
    }
    // Un patrón estéreo con una sola pista no tiene sentido y el sistema lo
    // rechaza en tiempo de ejecución, que es el peor momento para enterarse.
    if (ajustes.audio.patron == PatronPolar::Estereo &&
        ajustes.audio.canales == ModoCanales::Mono) {
      return fallar("El patron estereo necesita dos canales.");
    }
  }

  if (motivo) motivo->clear();
  return true;
}

std::vector<PreajusteGrabacion> Preajustes() {
  std::vector<PreajusteGrabacion> lista;

  {
    PreajusteGrabacion p;
    p.nombre = "Clase en pizarra";
    p.para_que = "Audio y tinta vectorial sincronizados, sin video pesado.";
    p.ajustes.modo = ModoCaptura::NotaViva;
    p.ajustes.video.resolucion = Resolucion::R1080p;
    p.ajustes.video.fps = 30;
    p.ajustes.video.calidad = CalidadVideo::Alta;
    p.ajustes.video.codec = CodecVideo::HEVC;
    p.ajustes.audio.canales = ModoCanales::Mono;
    p.ajustes.audio.patron = PatronPolar::Cardioide;
    p.ajustes.audio.posicion = PosicionMicrofono::Superior;
    lista.push_back(p);
  }
  {
    PreajusteGrabacion p;
    p.nombre = "Explicar con el lapiz";
    p.para_que = "60 FPS para que el trazo se vea fluido al reproducir.";
    p.ajustes.modo = ModoCaptura::PantallaConAudio;
    p.ajustes.video.resolucion = Resolucion::Nativa;
    p.ajustes.video.fps = 60;
    p.ajustes.video.calidad = CalidadVideo::Alta;
    p.ajustes.video.codec = CodecVideo::HEVC;
    p.ajustes.audio.canales = ModoCanales::Estereo;
    p.ajustes.audio.patron = PatronPolar::Automatico;
    lista.push_back(p);
  }
  {
    PreajusteGrabacion p;
    p.nombre = "Maxima calidad";
    p.para_que = "Todo lo que da el aparato. Pesa mucho.";
    p.ajustes.modo = ModoCaptura::PantallaConAudio;
    p.ajustes.video.resolucion = Resolucion::Nativa;
    p.ajustes.video.fps = 120;
    p.ajustes.video.calidad = CalidadVideo::Maxima;
    p.ajustes.video.codec = CodecVideo::HEVC;
    p.ajustes.audio.canales = ModoCanales::Espacial;
    p.ajustes.audio.patron = PatronPolar::Omnidireccional;
    p.ajustes.audio.frecuencia_hz = 48000;
    lista.push_back(p);
  }
  {
    PreajusteGrabacion p;
    p.nombre = "Ahorro de espacio";
    p.para_que = "Dos horas de clase sin llenar el iPad.";
    p.ajustes.modo = ModoCaptura::NotaViva;
    p.ajustes.video.resolucion = Resolucion::R720p;
    p.ajustes.video.fps = 24;
    p.ajustes.video.calidad = CalidadVideo::Baja;
    p.ajustes.video.codec = CodecVideo::HEVC;
    p.ajustes.audio.canales = ModoCanales::Mono;
    p.ajustes.audio.kbps = 64;
    p.ajustes.audio.frecuencia_hz = 44100;
    p.ajustes.audio.patron = PatronPolar::Cardioide;
    lista.push_back(p);
  }
  {
    PreajusteGrabacion p;
    p.nombre = "Solo la voz";
    p.para_que = "Grabar al profe sin gastar en video.";
    p.ajustes.modo = ModoCaptura::SoloAudio;
    p.ajustes.video.resolucion = Resolucion::R720p;
    p.ajustes.video.fps = 15;
    p.ajustes.video.calidad = CalidadVideo::Baja;
    p.ajustes.audio.canales = ModoCanales::Mono;
    p.ajustes.audio.patron = PatronPolar::Cardioide;
    p.ajustes.audio.kbps = 128;
    p.ajustes.audio.audio_de_la_app = false;
    lista.push_back(p);
  }

  return lista;
}

// ---------------------------------------------------------------------------
// La grabación en curso
// ---------------------------------------------------------------------------

bool SesionGrabacion::Comenzar(double t) {
  if (estado_ != EstadoGrabacion::Detenida) return false;
  acumulado_ = 0.0;
  ultimo_ = t;
  estado_ = EstadoGrabacion::Grabando;
  return true;
}

bool SesionGrabacion::Pausar(double t) {
  if (estado_ != EstadoGrabacion::Grabando) return false;
  acumulado_ += std::max(0.0, t - ultimo_);
  ultimo_ = t;
  estado_ = EstadoGrabacion::Pausada;
  return true;
}

bool SesionGrabacion::Reanudar(double t) {
  if (estado_ != EstadoGrabacion::Pausada) return false;
  ultimo_ = t;
  estado_ = EstadoGrabacion::Grabando;
  return true;
}

bool SesionGrabacion::Detener(double t) {
  if (estado_ == EstadoGrabacion::Detenida) return false;
  if (estado_ == EstadoGrabacion::Grabando) {
    acumulado_ += std::max(0.0, t - ultimo_);
  }
  ultimo_ = t;
  estado_ = EstadoGrabacion::Detenida;
  return true;
}

double SesionGrabacion::SegundosGrabados(double ahora) const {
  if (estado_ != EstadoGrabacion::Grabando) return acumulado_;
  return acumulado_ + std::max(0.0, ahora - ultimo_);
}

double SesionGrabacion::MegabytesEstimados(
    double ahora, const AjustesGrabacion& ajustes,
    const CapacidadesGrabacion& cap) const {
  return SegundosGrabados(ahora) / 60.0 * MegabytesPorMinuto(ajustes, cap);
}

}  // namespace tinta
