import AVFoundation
import Speech

struct SegmentoTranscripcion: Codable, Equatable, Identifiable {
    var id = UUID()
    var inicio: Double
    var duracion: Double
    var texto: String
    var confianza: Float
    var hablante: String?
}

struct CapituloAudio: Codable, Equatable, Identifiable {
    var id = UUID()
    var inicio: Double
    var fin: Double
    var titulo: String
    var palabrasClave: [String]
}

enum ErrorTranscripcionLocal: LocalizedError {
    case sinPermiso, sinModeloLocal, audioInvalido, sinResultado, tiempoAgotado
    var errorDescription: String? {
        switch self {
        case .sinPermiso:
            return "No se concedió permiso para transcribir. Puede habilitar Reconocimiento de voz en Ajustes > Privacidad y seguridad."
        case .sinModeloLocal: return "No hay un modelo de dictado local instalado para este idioma."
        case .audioInvalido: return "El archivo de audio no se pudo leer."
        case .sinResultado: return "El reconocedor local no produjo transcripción."
        case .tiempoAgotado:
            return "El reconocedor local dejó de responder y el bloque se canceló después de 120 segundos."
        }
    }
}

/// Transcribe el M4A terminado en bloques locales. Nunca activa la ruta de
/// reconocimiento en servidor: si iPadOS no tiene el modelo descargado, deja
/// el audio intacto y registra el estado sin fabricar texto.
final class TranscriptorNotaViva {
    private final class EntregaUnica {
        private let bloqueo = NSLock()
        private var terminada = false

        func reclamar() -> Bool {
            bloqueo.lock()
            defer { bloqueo.unlock() }
            guard !terminada else { return false }
            terminada = true
            return true
        }

        var finalizada: Bool {
            bloqueo.lock()
            defer { bloqueo.unlock() }
            return terminada
        }
    }

    static let compartido = TranscriptorNotaViva()
    private var tareas: [UUID: SFSpeechRecognitionTask] = [:]
    private let bloqueo = NSLock()
    private init() {}

    func transcribir(
        audio: URL,
        manifiesto: URL,
        idioma: String = "es-CR",
        completado: @escaping (Result<Int, Error>) -> Void
    ) {
        SFSpeechRecognizer.requestAuthorization { [weak self] estado in
            guard let self else { return }
            guard estado == .authorized else {
                self.entregar(.failure(ErrorTranscripcionLocal.sinPermiso),
                              a: completado)
                return
            }
            self.iniciarTranscripcion(audio: audio, manifiesto: manifiesto,
                                      idioma: idioma, completado: completado)
        }
    }

    private func iniciarTranscripcion(
        audio: URL,
        manifiesto: URL,
        idioma: String,
        completado: @escaping (Result<Int, Error>) -> Void
    ) {
        guard let reconocedor = SFSpeechRecognizer(locale: Locale(identifier: idioma))
                ?? SFSpeechRecognizer(locale: Locale(identifier: "es")),
              reconocedor.supportsOnDeviceRecognition else {
            entregar(.failure(ErrorTranscripcionLocal.sinModeloLocal), a: completado)
            return
        }
        let asset = AVURLAsset(url: audio)
        Task { [weak self] in
            guard let self else { return }
            do {
                let tiempo = try await asset.load(.duration)
                let duracion = tiempo.seconds
                guard duracion.isFinite, duracion > 0 else {
                    throw ErrorTranscripcionLocal.audioInvalido
                }
                self.procesarBloque(
                    asset: asset, reconocedor: reconocedor, indice: 0,
                    inicio: 0, duracionTotal: duracion, acumulado: [],
                    manifiestoURL: manifiesto) { resultado in
                        self.entregar(resultado, a: completado)
                    }
            } catch {
                self.entregar(.failure(error), a: completado)
            }
        }
    }

    private func procesarBloque(asset: AVURLAsset, reconocedor: SFSpeechRecognizer,
                                indice: Int, inicio: Double, duracionTotal: Double,
                                acumulado: [SegmentoTranscripcion], manifiestoURL: URL,
                                completado: @escaping (Result<Int, Error>) -> Void) {
        guard inicio < duracionTotal else {
            guard !acumulado.isEmpty else {
                completado(.failure(ErrorTranscripcionLocal.sinResultado))
                return
            }
            do {
                try guardar(acumulado, en: manifiestoURL)
                completado(.success(acumulado.count))
            } catch {
                completado(.failure(error))
            }
            return
        }
        let duracion = min(55, duracionTotal - inicio)
        let carpeta = FileManager.default.temporaryDirectory
            .appendingPathComponent("TintaTranscripcion-\(UUID().uuidString)", isDirectory: true)
        do {
            try FileManager.default.createDirectory(at: carpeta,
                                                    withIntermediateDirectories: true)
        } catch {
            completado(.failure(error))
            return
        }
        let fragmento = carpeta.appendingPathComponent("bloque-\(indice).m4a")
        guard let exportador = AVAssetExportSession(asset: asset,
                                                     presetName: AVAssetExportPresetAppleM4A) else {
            try? FileManager.default.removeItem(at: carpeta)
            completado(.failure(ErrorTranscripcionLocal.audioInvalido))
            return
        }
        exportador.outputURL = fragmento
        exportador.outputFileType = .m4a
        exportador.timeRange = CMTimeRange(
            start: CMTime(seconds: inicio, preferredTimescale: 600),
            duration: CMTime(seconds: duracion, preferredTimescale: 600))
        exportador.exportAsynchronously { [weak self] in
            guard let self else {
                try? FileManager.default.removeItem(at: carpeta)
                return
            }
            guard exportador.status == .completed else {
                let error = exportador.error ?? ErrorTranscripcionLocal.audioInvalido
                try? FileManager.default.removeItem(at: carpeta)
                completado(.failure(error))
                return
            }
            self.reconocer(fragmento, reconocedor: reconocedor, desfase: inicio) { resultado in
                try? FileManager.default.removeItem(at: carpeta)
                switch resultado {
                case .failure(let error):
                    completado(.failure(error))
                case .success(let nuevos):
                    let siguiente = acumulado + nuevos
                    self.procesarBloque(
                        asset: asset, reconocedor: reconocedor,
                        indice: indice + 1, inicio: inicio + duracion,
                        duracionTotal: duracionTotal, acumulado: siguiente,
                        manifiestoURL: manifiestoURL, completado: completado)
                }
            }
        }
    }

    private func reconocer(_ url: URL, reconocedor: SFSpeechRecognizer,
                           desfase: Double,
                           completado: @escaping (Result<[SegmentoTranscripcion], Error>) -> Void) {
        let peticion = SFSpeechURLRecognitionRequest(url: url)
        peticion.shouldReportPartialResults = false
        peticion.requiresOnDeviceRecognition = true
        if #available(iOS 16.0, *) { peticion.addsPunctuation = true }
        let id = UUID()
        let entrega = EntregaUnica()
        let tiempoAgotado = DispatchWorkItem { [weak self] in
            guard entrega.reclamar() else { return }
            self?.cancelar(id)
            completado(.failure(ErrorTranscripcionLocal.tiempoAgotado))
        }
        let tarea = reconocedor.recognitionTask(with: peticion) { [weak self] resultado, error in
            if let error {
                guard entrega.reclamar() else { return }
                tiempoAgotado.cancel()
                self?.retirar(id)
                completado(.failure(error))
                return
            }
            guard let resultado, resultado.isFinal else { return }
            guard entrega.reclamar() else { return }
            tiempoAgotado.cancel()
            let segmentos = resultado.bestTranscription.segments.map {
                SegmentoTranscripcion(
                    inicio: desfase + $0.timestamp, duracion: $0.duration,
                    texto: $0.substring, confianza: $0.confidence)
            }
            self?.retirar(id)
            completado(.success(segmentos))
        }
        if entrega.finalizada {
            tarea.cancel()
        } else {
            bloqueo.lock(); tareas[id] = tarea; bloqueo.unlock()
            DispatchQueue.global(qos: .utility).asyncAfter(
                deadline: .now() + 120, execute: tiempoAgotado)
        }
    }

    private func retirar(_ id: UUID) {
        bloqueo.lock(); tareas.removeValue(forKey: id); bloqueo.unlock()
    }

    private func cancelar(_ id: UUID) {
        bloqueo.lock()
        let tarea = tareas.removeValue(forKey: id)
        bloqueo.unlock()
        tarea?.cancel()
    }

    private func guardar(_ segmentos: [SegmentoTranscripcion], en url: URL) throws {
        guard !segmentos.isEmpty else { throw ErrorTranscripcionLocal.sinResultado }
        let capitulos = generarCapitulos(segmentos)
        let carpeta = url.deletingLastPathComponent()
        let datosTranscripcion = try JSONEncoder().encode(segmentos)
        try datosTranscripcion.write(
            to: carpeta.appendingPathComponent("transcripcion.json"),
            options: [.atomic, .completeFileProtection])
        let manifiesto = try RepositorioManifiestoNotaViva.actualizar(url) {
            $0.transcripcion = segmentos
            $0.capitulos = capitulos
            $0.archivoTranscripcion = "transcripcion.json"
        }
        DispatchQueue.main.async {
            self.indexar(segmentos, manifiesto: manifiesto, manifiestoURL: url)
        }
    }

    private func entregar(
        _ resultado: Result<Int, Error>,
        a completado: @escaping (Result<Int, Error>) -> Void
    ) {
        DispatchQueue.main.async { completado(resultado) }
    }

    private func generarCapitulos(_ segmentos: [SegmentoTranscripcion]) -> [CapituloAudio] {
        guard let primero = segmentos.first else { return [] }
        var grupos: [[SegmentoTranscripcion]] = [[primero]]
        for segmento in segmentos.dropFirst() {
            let anterior = grupos.last!.last!
            let pausa = segmento.inicio - (anterior.inicio + anterior.duracion)
            let largo = segmento.inicio - grupos.last!.first!.inicio
            if pausa >= 7 || largo >= 300 { grupos.append([segmento]) }
            else { grupos[grupos.count - 1].append(segmento) }
        }
        return grupos.map { grupo in
            let texto = grupo.map(\.texto).joined(separator: " ")
            let claves = EtiquetadorLocal.etiquetas(texto)
            return CapituloAudio(
                inicio: grupo.first!.inicio,
                fin: grupo.last!.inicio + grupo.last!.duracion,
                titulo: claves.prefix(3).joined(separator: " · ").capitalized,
                palabrasClave: claves)
        }
    }

    private func indexar(_ segmentos: [SegmentoTranscripcion],
                         manifiesto: ManifiestoNotaViva, manifiestoURL: URL) {
        for capitulo in manifiesto.capitulos ?? [] {
            let texto = segmentos.filter {
                $0.inicio >= capitulo.inicio && $0.inicio <= capitulo.fin
            }.map(\.texto).joined(separator: " ")
            let fuente = ReferenciaFuente(
                tipo: .transcripcion, titulo: capitulo.titulo,
                hashArchivo: manifiesto.id.uuidString,
                segundoAudio: capitulo.inicio)
            CentroAcademico.compartido.agregarNodo(NodoConocimiento(
                titulo: capitulo.titulo, texto: texto, fuente: fuente,
                etiquetas: capitulo.palabrasClave))
        }
    }
}
