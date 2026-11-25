import AVFoundation
import Foundation

struct SegmentoSilencioNotaViva: Codable, Equatable {
    var inicio: Double
    var fin: Double

    var duracion: Double { max(0, fin - inicio) }
}

enum ModoCanalesMejoraAudio: String, Codable, CaseIterable {
    case conservar
    case mono
    case soloIzquierdo
    case soloDerecho

    var nombre: String {
        switch self {
        case .conservar: return "Original"
        case .mono: return "Mono mezclado"
        case .soloIzquierdo: return "Solo izquierdo"
        case .soloDerecho: return "Solo derecho"
        }
    }
}

/// Ajustes de una copia derivada. Nunca se aplican sobre el archivo original.
struct AjustesMejoraAudio: Codable, Equatable {
    var reduccionRuido = true
    var realceVoz = true
    var normalizar = true
    var eliminarZumbido = true
    var frecuenciaZumbidoHz = 60.0
    var canales: ModoCanalesMejoraAudio = .conservar
}

enum ErrorProcesamientoAudioLocal: LocalizedError {
    case formatoNoCompatible
    case sinCanales
    case noPudoCrearDestino

    var errorDescription: String? {
        switch self {
        case .formatoNoCompatible:
            return "El audio no pudo abrirse como PCM de punto flotante."
        case .sinCanales:
            return "El archivo no contiene canales de audio legibles."
        case .noPudoCrearDestino:
            return "No se pudo crear la copia de audio mejorada."
        }
    }
}

/// DSP local y determinista para grabaciones de clase. La salida es PCM/CAR,
/// deliberadamente separada del M4A original; así una mejora fallida o una
/// preferencia distinta nunca destruyen la fuente.
enum ProcesadorAudioLocal {
    private static let cuadrosPorBloque: AVAudioFrameCount = 4_096

    static func detectarSilencios(
        en url: URL,
        umbralDB: Float = -43,
        duracionMinima: Double = 0.55,
        progreso: ((Double) -> Void)? = nil
    ) throws -> [SegmentoSilencioNotaViva] {
        let archivo = try AVAudioFile(forReading: url)
        let formato = archivo.processingFormat
        guard formato.channelCount > 0 else { throw ErrorProcesamientoAudioLocal.sinCanales }
        guard formato.commonFormat == .pcmFormatFloat32 else {
            throw ErrorProcesamientoAudioLocal.formatoNoCompatible
        }

        let ventana = max(256, Int(formato.sampleRate * 0.025))
        var silencios: [SegmentoSilencioNotaViva] = []
        var inicioSilencio: Double?
        var muestraGlobal: Int64 = 0

        while archivo.framePosition < archivo.length {
            guard let buffer = AVAudioPCMBuffer(
                pcmFormat: formato, frameCapacity: cuadrosPorBloque) else {
                throw ErrorProcesamientoAudioLocal.formatoNoCompatible
            }
            try archivo.read(into: buffer, frameCount: cuadrosPorBloque)
            guard let datos = buffer.floatChannelData else {
                throw ErrorProcesamientoAudioLocal.formatoNoCompatible
            }
            let cuadros = Int(buffer.frameLength)
            var inicioVentana = 0
            while inicioVentana < cuadros {
                let cantidad = min(ventana, cuadros - inicioVentana)
                var energia: Double = 0
                for canal in 0..<Int(formato.channelCount) {
                    let muestras = datos[canal]
                    for indice in inicioVentana..<(inicioVentana + cantidad) {
                        let valor = Double(muestras[indice])
                        energia += valor * valor
                    }
                }
                let divisor = Double(max(1, cantidad * Int(formato.channelCount)))
                let rms = sqrt(energia / divisor)
                let db = Float(20 * log10(max(rms, 0.000_000_1)))
                let segundo = Double(muestraGlobal + Int64(inicioVentana)) / formato.sampleRate
                if db <= umbralDB {
                    if inicioSilencio == nil { inicioSilencio = segundo }
                } else if let inicio = inicioSilencio {
                    if segundo - inicio >= duracionMinima {
                        silencios.append(SegmentoSilencioNotaViva(inicio: inicio, fin: segundo))
                    }
                    inicioSilencio = nil
                }
                inicioVentana += cantidad
            }
            muestraGlobal += Int64(cuadros)
            progreso?(min(1, Double(archivo.framePosition) / Double(max(archivo.length, 1))))
        }

        if let inicio = inicioSilencio {
            let fin = Double(archivo.length) / formato.sampleRate
            if fin - inicio >= duracionMinima {
                silencios.append(SegmentoSilencioNotaViva(inicio: inicio, fin: fin))
            }
        }
        return fusionar(silencios, separacionMaxima: 0.12)
    }

    @discardableResult
    static func mejorar(
        original: URL,
        destino: URL,
        ajustes: AjustesMejoraAudio,
        progreso: ((Double) -> Void)? = nil
    ) throws -> URL {
        let inspeccion = try inspeccionar(original)
        let entrada = try AVAudioFile(forReading: original)
        let formatoEntrada = entrada.processingFormat
        guard formatoEntrada.commonFormat == .pcmFormatFloat32,
              formatoEntrada.channelCount > 0 else {
            throw ErrorProcesamientoAudioLocal.formatoNoCompatible
        }

        let canalesSalida: AVAudioChannelCount = ajustes.canales == .conservar
            ? formatoEntrada.channelCount : 1
        guard let formatoSalida = AVAudioFormat(
            commonFormat: .pcmFormatFloat32,
            sampleRate: formatoEntrada.sampleRate,
            channels: canalesSalida,
            interleaved: false) else {
            throw ErrorProcesamientoAudioLocal.noPudoCrearDestino
        }
        let salida = try AVAudioFile(
            forWriting: destino,
            settings: formatoSalida.settings,
            commonFormat: .pcmFormatFloat32,
            interleaved: false)

        let gananciaNormalizacion: Float = ajustes.normalizar
            ? min(8, 0.89 / max(inspeccion.pico, 0.000_1)) : 1
        let umbralRuido = max(0.000_8, inspeccion.rms * 0.16)
        var estados = (0..<Int(canalesSalida)).map { _ in
            EstadoDSP(
                frecuenciaMuestreo: formatoEntrada.sampleRate,
                frecuenciaZumbido: ajustes.frecuenciaZumbidoHz)
        }

        while entrada.framePosition < entrada.length {
            guard let bufferEntrada = AVAudioPCMBuffer(
                pcmFormat: formatoEntrada, frameCapacity: cuadrosPorBloque),
                  let bufferSalida = AVAudioPCMBuffer(
                    pcmFormat: formatoSalida, frameCapacity: cuadrosPorBloque) else {
                throw ErrorProcesamientoAudioLocal.formatoNoCompatible
            }
            try entrada.read(into: bufferEntrada, frameCount: cuadrosPorBloque)
            bufferSalida.frameLength = bufferEntrada.frameLength
            guard let origen = bufferEntrada.floatChannelData,
                  let destinoDatos = bufferSalida.floatChannelData else {
                throw ErrorProcesamientoAudioLocal.formatoNoCompatible
            }

            let cuadros = Int(bufferEntrada.frameLength)
            let cantidadEntrada = Int(formatoEntrada.channelCount)
            for canalSalida in 0..<Int(canalesSalida) {
                for cuadro in 0..<cuadros {
                    var muestra: Float
                    switch ajustes.canales {
                    case .conservar:
                        muestra = origen[min(canalSalida, cantidadEntrada - 1)][cuadro]
                    case .mono:
                        var suma: Float = 0
                        for canal in 0..<cantidadEntrada { suma += origen[canal][cuadro] }
                        muestra = suma / Float(max(cantidadEntrada, 1))
                    case .soloIzquierdo:
                        muestra = origen[0][cuadro]
                    case .soloDerecho:
                        muestra = origen[min(1, cantidadEntrada - 1)][cuadro]
                    }

                    muestra = estados[canalSalida].procesar(
                        muestra,
                        reduccionRuido: ajustes.reduccionRuido,
                        realceVoz: ajustes.realceVoz,
                        eliminarZumbido: ajustes.eliminarZumbido,
                        umbralRuido: umbralRuido)
                    muestra *= gananciaNormalizacion
                    // Saturación suave evita clipping duro después del realce.
                    destinoDatos[canalSalida][cuadro] = tanh(muestra)
                }
            }
            try salida.write(from: bufferSalida)
            progreso?(min(1, Double(entrada.framePosition) / Double(max(entrada.length, 1))))
        }
        return destino
    }

    static func destinoUnico(juntoA original: URL) -> URL {
        let carpeta = original.deletingLastPathComponent()
        let base = original.deletingPathExtension().lastPathComponent
        var indice = 1
        var candidato = carpeta.appendingPathComponent("\(base) mejorado.caf")
        while FileManager.default.fileExists(atPath: candidato.path) {
            indice += 1
            candidato = carpeta.appendingPathComponent("\(base) mejorado \(indice).caf")
        }
        return candidato
    }

    private static func inspeccionar(_ url: URL) throws -> (pico: Float, rms: Float) {
        let archivo = try AVAudioFile(forReading: url)
        let formato = archivo.processingFormat
        guard formato.commonFormat == .pcmFormatFloat32,
              formato.channelCount > 0 else {
            throw ErrorProcesamientoAudioLocal.formatoNoCompatible
        }
        var pico: Float = 0
        var energia: Double = 0
        var cantidad: Int64 = 0
        while archivo.framePosition < archivo.length {
            guard let buffer = AVAudioPCMBuffer(
                pcmFormat: formato, frameCapacity: cuadrosPorBloque) else {
                throw ErrorProcesamientoAudioLocal.formatoNoCompatible
            }
            try archivo.read(into: buffer, frameCount: cuadrosPorBloque)
            guard let datos = buffer.floatChannelData else {
                throw ErrorProcesamientoAudioLocal.formatoNoCompatible
            }
            for canal in 0..<Int(formato.channelCount) {
                for indice in 0..<Int(buffer.frameLength) {
                    let muestra = datos[canal][indice]
                    pico = max(pico, abs(muestra))
                    energia += Double(muestra * muestra)
                    cantidad += 1
                }
            }
        }
        return (pico, Float(sqrt(energia / Double(max(cantidad, 1)))))
    }

    private static func fusionar(
        _ segmentos: [SegmentoSilencioNotaViva],
        separacionMaxima: Double
    ) -> [SegmentoSilencioNotaViva] {
        guard var actual = segmentos.first else { return [] }
        var salida: [SegmentoSilencioNotaViva] = []
        for siguiente in segmentos.dropFirst() {
            if siguiente.inicio - actual.fin <= separacionMaxima {
                actual.fin = max(actual.fin, siguiente.fin)
            } else {
                salida.append(actual)
                actual = siguiente
            }
        }
        salida.append(actual)
        return salida
    }
}

private struct EstadoDSP {
    private var pasoAlto: FiltroBiquad
    private var zumbido: FiltroBiquad
    private var vozBaja: FiltroBiquad
    private var vozAlta: FiltroBiquad
    private var gananciaPuerta: Float = 1

    init(frecuenciaMuestreo: Double, frecuenciaZumbido: Double) {
        pasoAlto = .pasoAlto(frecuencia: 75, muestra: frecuenciaMuestreo, q: 0.707)
        zumbido = .rechazoBanda(
            frecuencia: min(max(frecuenciaZumbido, 45), 70),
            muestra: frecuenciaMuestreo,
            q: 24)
        vozBaja = .pico(frecuencia: 1_500, muestra: frecuenciaMuestreo, q: 0.9, db: 2.2)
        vozAlta = .pico(frecuencia: 3_400, muestra: frecuenciaMuestreo, q: 1.1, db: 1.6)
    }

    mutating func procesar(
        _ entrada: Float,
        reduccionRuido: Bool,
        realceVoz: Bool,
        eliminarZumbido: Bool,
        umbralRuido: Float
    ) -> Float {
        var salida = pasoAlto.aplicar(entrada)
        if eliminarZumbido { salida = zumbido.aplicar(salida) }
        if realceVoz {
            salida = vozBaja.aplicar(salida)
            salida = vozAlta.aplicar(salida)
        }
        if reduccionRuido {
            let objetivo: Float = abs(salida) < umbralRuido ? 0.18 : 1
            let factor: Float = objetivo < gananciaPuerta ? 0.035 : 0.006
            gananciaPuerta += (objetivo - gananciaPuerta) * factor
            salida *= gananciaPuerta
        }
        return salida
    }
}

private struct FiltroBiquad {
    var b0: Float
    var b1: Float
    var b2: Float
    var a1: Float
    var a2: Float
    var z1: Float = 0
    var z2: Float = 0

    mutating func aplicar(_ entrada: Float) -> Float {
        let salida = b0 * entrada + z1
        z1 = b1 * entrada - a1 * salida + z2
        z2 = b2 * entrada - a2 * salida
        return salida
    }

    static func pasoAlto(frecuencia: Double, muestra: Double, q: Double) -> FiltroBiquad {
        let w0 = 2 * Double.pi * frecuencia / muestra
        let coseno = cos(w0)
        let alfa = sin(w0) / (2 * q)
        let a0 = 1 + alfa
        return FiltroBiquad(
            b0: Float((1 + coseno) / 2 / a0),
            b1: Float(-(1 + coseno) / a0),
            b2: Float((1 + coseno) / 2 / a0),
            a1: Float(-2 * coseno / a0),
            a2: Float((1 - alfa) / a0))
    }

    static func rechazoBanda(frecuencia: Double, muestra: Double, q: Double) -> FiltroBiquad {
        let w0 = 2 * Double.pi * frecuencia / muestra
        let coseno = cos(w0)
        let alfa = sin(w0) / (2 * q)
        let a0 = 1 + alfa
        return FiltroBiquad(
            b0: Float(1 / a0),
            b1: Float(-2 * coseno / a0),
            b2: Float(1 / a0),
            a1: Float(-2 * coseno / a0),
            a2: Float((1 - alfa) / a0))
    }

    static func pico(
        frecuencia: Double, muestra: Double, q: Double, db: Double
    ) -> FiltroBiquad {
        let amplitud = pow(10, db / 40)
        let w0 = 2 * Double.pi * frecuencia / muestra
        let coseno = cos(w0)
        let alfa = sin(w0) / (2 * q)
        let a0 = 1 + alfa / amplitud
        return FiltroBiquad(
            b0: Float((1 + alfa * amplitud) / a0),
            b1: Float(-2 * coseno / a0),
            b2: Float((1 - alfa * amplitud) / a0),
            a1: Float(-2 * coseno / a0),
            a2: Float((1 - alfa / amplitud) / a0))
    }
}
