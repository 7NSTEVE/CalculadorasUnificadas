import Foundation
import UIKit

/// Los enteros del API en C, con nombre y con etiqueta para la pantalla.
///
/// La decisión de qué kbps, qué tamaño y qué es imposible en este aparato la
/// toma el núcleo C++, que se prueba en Windows en segundos. Aquí solo se le
/// pone nombre a cada número y se dibuja.

// MARK: - Producto de grabación

enum ModoCaptura: Int32, CaseIterable {
    case soloAudio = 0, pantallaConAudio, notaViva

    var nombre: String {
        switch self {
        case .soloAudio: return "Solo audio"
        case .pantallaConAudio: return "Vídeo de pantalla"
        case .notaViva: return "Nota Viva"
        }
    }

    var explicacion: String {
        switch self {
        case .soloAudio:
            return "Guarda audio del micrófono elegido, sin capturar la pantalla: M4A en mono/estéreo o QuickTime FOA cuando el iPad admite 3D."
        case .pantallaConAudio:
            return "Guarda un MP4 de la pantalla, el micrófono y, si se pide, el audio de la app."
        case .notaViva:
            return "Guarda la tinta vectorial con sus tiempos y audio sincronizado para reconstruir cómo se escribió."
        }
    }

    var incluyeVideo: Bool { self == .pantallaConAudio }
}

// MARK: - Vídeo

enum ResolucionGrabacion: Int32, CaseIterable {
    case nativa = 0, r720p, r1080p, r1440p, r2160p, personalizada

    var nombre: String {
        switch self {
        case .nativa: return "Nativa"
        case .r720p: return "720p"
        case .r1080p: return "1080p"
        case .r1440p: return "1440p"
        case .r2160p: return "4K"
        case .personalizada: return "A mano"
        }
    }
}

enum CalidadVideo: Int32, CaseIterable {
    case baja = 0, media, alta, maxima

    var nombre: String {
        switch self {
        case .baja: return "Baja"
        case .media: return "Media"
        case .alta: return "Alta"
        case .maxima: return "Máxima"
        }
    }
}

enum CodecVideo: Int32, CaseIterable {
    case h264 = 0, hevc

    var nombre: String { self == .hevc ? "HEVC" : "H.264" }
}

// MARK: - Audio

enum ModoCanales: Int32, CaseIterable {
    case mono = 0, estereo, espacial

    var nombre: String {
        switch self {
        case .mono: return "Mono"
        case .estereo: return "Estéreo"
        case .espacial: return "3D"
        }
    }
}

enum PosicionMicrofono: Int32, CaseIterable {
    case automatica = 0, superior, inferior, frontal, trasera

    var nombre: String {
        switch self {
        case .automatica: return "Automático"
        case .superior: return "Superior"
        case .inferior: return "Inferior"
        case .frontal: return "Frontal"
        case .trasera: return "Trasero"
        }
    }
}

enum PatronPolar: Int32, CaseIterable {
    case automatico = 0, omnidireccional, cardioide, subcardioide, estereo

    var nombre: String {
        switch self {
        case .automatico: return "Automático"
        case .omnidireccional: return "Toda la sala"
        case .cardioide: return "Solo de frente"
        case .subcardioide: return "Intermedio"
        case .estereo: return "Estéreo"
        }
    }
}

// MARK: - Ajustes de grabación

extension TintaAjustesGrabacion {

    static var porDefecto: TintaAjustesGrabacion { tinta_grabacion_por_defecto() }

    var modoElegido: ModoCaptura {
        get { ModoCaptura(rawValue: modo) ?? .notaViva }
        set { modo = newValue.rawValue }
    }

    var resolucionElegida: ResolucionGrabacion {
        get { ResolucionGrabacion(rawValue: resolucion) ?? .nativa }
        set { resolucion = newValue.rawValue }
    }

    var calidadElegida: CalidadVideo {
        get { CalidadVideo(rawValue: calidad) ?? .alta }
        set { calidad = newValue.rawValue }
    }

    var codecElegido: CodecVideo {
        get { CodecVideo(rawValue: codec) ?? .hevc }
        set { codec = newValue.rawValue }
    }

    var canalesElegidos: ModoCanales {
        get { ModoCanales(rawValue: canales) ?? .estereo }
        set { canales = newValue.rawValue }
    }

    var posicionElegida: PosicionMicrofono {
        get { PosicionMicrofono(rawValue: posicion_microfono) ?? .automatica }
        set { posicion_microfono = newValue.rawValue }
    }

    var patronElegido: PatronPolar {
        get { PatronPolar(rawValue: patron_polar) ?? .automatico }
        set { patron_polar = newValue.rawValue }
    }

    // MARK: Cálculo, todo del núcleo

    func tamano(_ cap: TintaCapacidades) -> CGSize {
        var ancho: Int32 = 0
        var alto: Int32 = 0
        tinta_grabacion_tamano(self, cap, &ancho, &alto)
        return CGSize(width: CGFloat(ancho), height: CGFloat(alto))
    }

    func kbpsVideo(_ cap: TintaCapacidades) -> Int { Int(tinta_grabacion_kbps_video(self, cap)) }
    func kbpsAudio(_ cap: TintaCapacidades) -> Int { Int(tinta_grabacion_kbps_audio(self, cap)) }
    var canalesDeAudio: Int { Int(tinta_grabacion_canales(self)) }
    func megabytesPorMinuto(_ cap: TintaCapacidades) -> Double {
        tinta_grabacion_mb_por_minuto(self, cap)
    }
    func minutosQueCaben(_ cap: TintaCapacidades, megabytesLibres: Double) -> Double {
        tinta_grabacion_minutos_que_caben(self, cap, megabytesLibres)
    }

    /// Corrige lo imposible a lo más parecido que sí se puede en este aparato.
    func ajustado(a cap: TintaCapacidades) -> TintaAjustesGrabacion {
        tinta_grabacion_ajustar(self, cap)
    }

    /// `nil` si todo está bien; si no, el motivo en español para la pantalla.
    func problema(_ cap: TintaCapacidades) -> String? {
        var buffer = [CChar](repeating: 0, count: 160)
        let bien = tinta_grabacion_validar(self, cap, &buffer, buffer.count)
        return bien ? nil : String(cString: buffer)
    }
}

struct PreajusteGrabacion {
    let nombre: String
    let paraQue: String
    let ajustes: TintaAjustesGrabacion

    static var todos: [PreajusteGrabacion] {
        (0..<Int(tinta_grabacion_cantidad_preajustes())).map { i in
            let indice = Int32(i)
            return PreajusteGrabacion(
                nombre: String(cString: tinta_grabacion_nombre_preajuste(indice)),
                paraQue: String(cString: tinta_grabacion_para_que_preajuste(indice)),
                ajustes: tinta_grabacion_preajuste(indice))
        }
    }
}

// MARK: - Capacidades reales del aparato

extension TintaCapacidades {

    /// Lo que este iPad puede de verdad. Nada de esto se adivina: sale del
    /// sistema, y el núcleo se encarga de recortar lo que no alcanza.
    static func deEsteAparato() -> TintaCapacidades {
        var cap = TintaCapacidades()

        let pantalla = UIScreen.main.nativeBounds  // píxeles reales, no puntos
        cap.ancho_pantalla = Int32(pantalla.width)
        cap.alto_pantalla = Int32(pantalla.height)
        cap.fps_maximo = Int32(UIScreen.main.maximumFramesPerSecond)

        cap.hevc = Grabadora.admiteCodec(.hevc)
        // ReplayKit no garantiza buffers HDR ni metadatos de transferencia
        // Main10 para una captura de pantalla. Se declara no disponible de
        // forma deliberada: codificar un buffer SDR con una etiqueta HDR
        // produciría color incorrecto y sería una opción falsa.
        cap.hdr = false

        cap.canales_entrada_max = Int32(Microfonos.maximoCanalesDeEntrada())
        cap.estereo = Microfonos.admiteEstereo() || cap.canales_entrada_max >= 2

        // FOA se consulta en el AVCaptureDeviceInput y se graba por una ruta
        // QuickTime separada. ReplayKit continúa limitado a mono/estéreo.
        cap.espacial = Microfonos.admiteAudioEspacialReal()

        return cap
    }
}

// MARK: - Puntero del lápiz del iPad

/// El puntero **no es la pluma**: el grosor es el ancho de lo que queda escrito
/// y el radio del puntero es la marca que señala en el aire dónde está la punta
/// antes de tocar el cristal. Son dos ajustes distintos.
struct AjustesPuntero {
    var visible = true
    var radio: CGFloat = 6
    var colorArgb: UInt32 = 0x8C0A_84FF
    var seguirColorPluma = false
    var anillo = false
    var grosorAnillo: CGFloat = 1.5
    var mostrarAlDibujar = false

    var radioEfectivo: CGFloat {
        CGFloat(tinta_puntero_radio_efectivo(Float(radio)))
    }

    var grosorAnilloEfectivo: CGFloat {
        CGFloat(tinta_puntero_grosor_anillo(Float(radio), Float(grosorAnillo)))
    }

    func colorEfectivo(colorPluma: UInt32) -> UIColor {
        UIColor(argb: tinta_puntero_color_efectivo(colorArgb, colorPluma, seguirColorPluma))
    }

    func debeVerse(hayHover: Bool, dibujando: Bool) -> Bool {
        tinta_puntero_debe_verse(visible, hayHover, dibujando, mostrarAlDibujar)
    }
}

// MARK: - Pantalla siempre activa

struct AjustesPantalla {
    var siempreActiva = false
    var siempreActivaAlGrabar = true
    var siempreActivaAlEscribir = true

    func debe(grabando: Bool, escribiendo: Bool) -> Bool {
        tinta_pantalla_siempre_activa(siempreActiva, siempreActivaAlGrabar,
                                      siempreActivaAlEscribir, grabando, escribiendo)
    }

    /// Aplica la decisión al sistema. `isIdleTimerDisabled` es lo único que hay
    /// que tocar: iPadOS no apaga la pantalla mientras esté puesto.
    func aplicar(grabando: Bool, escribiendo: Bool) {
        UIApplication.shared.isIdleTimerDisabled = debe(grabando: grabando,
                                                        escribiendo: escribiendo)
    }
}

// MARK: - Dictado

enum ModoEscritura: Int, CaseIterable {
    case lapiz, teclado, voz

    var nombre: String {
        switch self {
        case .lapiz: return "Lápiz"
        case .teclado: return "Teclado"
        case .voz: return "Voz"
        }
    }

    var simbolo: String {
        switch self {
        case .lapiz: return "pencil.tip"
        case .teclado: return "keyboard"
        case .voz: return "mic"
        }
    }
}

enum Dictado {
    /// Convierte la puntuación dictada en signos y pone las mayúsculas.
    /// La lógica está en el núcleo, probada en Windows sin micrófono.
    static func limpiar(_ crudo: String, puntuacionHablada: Bool = true,
                        mayusculaInicial: Bool = true) -> String {
        let necesario = crudo.withCString {
            tinta_limpiar_dictado($0, puntuacionHablada, mayusculaInicial, nil, 0)
        }
        var buffer = [CChar](repeating: 0, count: necesario + 1)
        crudo.withCString { origen in
            _ = tinta_limpiar_dictado(origen, puntuacionHablada, mayusculaInicial,
                                      &buffer, buffer.count)
        }
        return String(cString: buffer)
    }
}

extension UIColor {
    convenience init(argb: UInt32) {
        self.init(red: CGFloat((argb >> 16) & 0xFF) / 255.0,
                  green: CGFloat((argb >> 8) & 0xFF) / 255.0,
                  blue: CGFloat(argb & 0xFF) / 255.0,
                  alpha: CGFloat((argb >> 24) & 0xFF) / 255.0)
    }

    var argb: UInt32 {
        var r: CGFloat = 0, g: CGFloat = 0, b: CGFloat = 0, a: CGFloat = 0
        getRed(&r, green: &g, blue: &b, alpha: &a)
        let byte: (CGFloat) -> UInt32 = { UInt32(max(0, min(1, $0)) * 255) }
        return (byte(a) << 24) | (byte(r) << 16) | (byte(g) << 8) | byte(b)
    }
}
