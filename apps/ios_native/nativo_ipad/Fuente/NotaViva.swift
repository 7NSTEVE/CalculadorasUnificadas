import Foundation
import CoreGraphics
import QuartzCore

enum TipoEventoNotaViva: String, Codable {
    case inicio
    case pausa
    case reanudacion
    case fin
    case mePerdi
    case importante
    case examen
    case marcador
    case cambioDiapositiva
    case fotoPizarra
    case videoClase
    case explicacionOral
}

struct EventoNotaViva: Codable, Equatable {
    let tipo: TipoEventoNotaViva
    let segundo: Double
    var titulo: String?
    var pagina: Int?
    var x: Double?
    var y: Double?
    /// Hash o identificador estable del recurso relacionado. Permite que una
    /// foto, slide o vídeo del timeline vuelva a su fuente sin duplicarla.
    var recursoId: String? = nil
    var diapositivaId: UUID? = nil
}

/// Correspondencia entre el tiempo continuo del archivo de audio y el reloj de
/// los toques. Cada pausa cierra un segmento y cada reanudación abre otro; así
/// una pausa de veinte minutos no aparece como un hueco de veinte minutos al
/// reconstruir la tinta.
struct SegmentoRelojNotaViva: Codable {
    let segundoInicio: Double
    let monotonoInicio: Double
    var monotonoFin: Double?
}

struct ManifiestoNotaViva: Codable {
    static let versionActual = 7

    let version: Int
    let id: UUID
    let creado: Date
    var inicioMonotono: Double
    var duracion: Double
    let archivoTinta: String
    /// Estado base + deltas vectoriales de tinta, texto y medios. Los paquetes
    /// v1-v6 no lo tienen y continúan abriendo la fotografía final histórica.
    var archivoCronologia: String?
    /// UUID del documento del que salió la copia. Es opcional para poder abrir
    /// paquetes v1-v4; el reproductor de solo lectura también dispone de una
    /// ruta de importación explícita para esos paquetes antiguos.
    var documentoPersistenteId: UUID?
    var archivoAudio: String?
    /// Si el codificador falló, el archivo parcial se conserva para una futura
    /// reparación, pero nunca se presenta como la pista principal válida.
    var archivoAudioParcial: String?
    var advertencias: [String]?
    var archivoAudioMejorado: String?
    var ajustesAudioMejorado: AjustesMejoraAudio?
    var silencios: [SegmentoSilencioNotaViva]?
    /// Solo se rellena cuando la persona vincula expresamente un vídeo cuya
    /// grabación/uso está autorizado. PiP nunca activa una cámara oculta.
    var archivoVideoAutorizado: String?
    var videoAutorizadoConfirmado: Bool?
    var bitrateAudioKbps: Int
    /// En FOA el codificador y la tasa efectiva pertenecen al sistema. Cero en
    /// `bitrateAudioKbps` significa que no se prometió un bitrate AAC manual.
    var bitrateAudioAdministradoPorSistema: Bool?
    var audioEspacialFOA: Bool?
    var frecuenciaHz: Int
    var canales: Int
    var microfono: SeleccionMicrofono?
    var segmentos: [SegmentoRelojNotaViva]
    var eventos: [EventoNotaViva]
    var archivoTranscripcion: String?
    var transcripcion: [SegmentoTranscripcion]?
    var capitulos: [CapituloAudio]?
}

/// Un paquete Nota Viva es una carpeta autocontenida: manifiesto JSON, copia
/// canónica de la tinta y audio M4A o QuickTime FOA. El tiempo de cada muestra
/// de lápiz del iPad y CACurrentMediaTime comparten el reloj monotónico del
/// sistema, por lo que el manifiesto puede reconstruir la escritura sin
/// convertirla a píxeles.
final class GrabadoraNotaViva {

    enum Fallo: LocalizedError {
        case sinTinta
        case sinCronologia
        case sinPaquete
        case yaGrabando
        case noEstaGrabando

        var errorDescription: String? {
            switch self {
            case .sinTinta: return "No se pudo guardar la tinta de la Nota Viva."
            case .sinCronologia:
                return "No se pudo guardar la reproducción paso a paso de la Nota Viva."
            case .sinPaquete: return "No se pudo preparar el paquete Nota Viva."
            case .yaGrabando: return "Ya hay una Nota Viva en curso."
            case .noEstaGrabando: return "No hay una Nota Viva que detener."
            }
        }
    }

    private(set) var estado: EstadoGrabacion = .detenida
    private(set) var archivo: URL?
    private(set) var patronReal: PatronPolar = .automatico
    var alCambiarEstado: ((EstadoGrabacion) -> Void)?
    var alFallar: ((Error) -> Void)?
    var alAdvertir: ((Error) -> Void)?

    var segundosGrabados: Double {
        reloj.segundo(en: CACurrentMediaTime())
    }

    var bytesAudioGrabados: Int64 { audio.bytesGrabados }

    private let audio = GrabadoraAudio()
    private weak var sesion: Sesion?
    private var paquete: URL?
    private var manifiesto: ManifiestoNotaViva?
    private var reloj = RelojNotaViva()
    private var deteniendo = false
    private var falloAudioPendiente: Error?

    init() {
        audio.alFallar = { [weak self] error in
            self?.manejarFalloAudio(error)
        }
        audio.alPausarPorSistema = { [weak self] in
            self?.pausarLineaTemporal(solicitarAudio: false)
        }
        audio.alReanudarPorSistema = { [weak self] in
            self?.reanudarLineaTemporal(solicitarAudio: false)
        }
    }

    func comenzar(ajustes: TintaAjustesGrabacion,
                  capacidades: TintaCapacidades,
                  seleccionMicrofono: SeleccionMicrofono?,
                  carpeta: URL,
                  sesion: Sesion,
                  completado: @escaping (Result<Void, Error>) -> Void) {
        guard estado == .detenida else {
            completado(.failure(Fallo.yaGrabando))
            return
        }
        deteniendo = false
        falloAudioPendiente = nil
        let ahora = CACurrentMediaTime()
        do {
            try FileManager.default.createDirectory(at: carpeta,
                                                    withIntermediateDirectories: true)
            let paquete = carpeta.appendingPathComponent(nombrePaquete(),
                                                          isDirectory: true)
            try FileManager.default.createDirectory(at: paquete,
                                                    withIntermediateDirectories: false)
            self.paquete = paquete
            self.archivo = paquete.appendingPathComponent("manifest.json")
            self.sesion = sesion
            guard sesion.comenzarCronologiaNotaViva(en: ahora) else {
                throw Fallo.sinCronologia
            }
            self.manifiesto = ManifiestoNotaViva(
                version: ManifiestoNotaViva.versionActual,
                id: UUID(), creado: Date(), inicioMonotono: ahora,
                duracion: 0, archivoTinta: "nota.tinta",
                archivoCronologia: nil,
                documentoPersistenteId: sesion.idPersistente.rawValue,
                archivoAudio: nil,
                archivoAudioParcial: nil, advertencias: nil,
                archivoAudioMejorado: nil, ajustesAudioMejorado: nil,
                silencios: nil, archivoVideoAutorizado: nil,
                videoAutorizadoConfirmado: nil,
                bitrateAudioKbps: ajustes.canalesElegidos == .espacial
                    ? 0 : ajustes.kbpsAudio(capacidades),
                bitrateAudioAdministradoPorSistema:
                    ajustes.canalesElegidos == .espacial,
                audioEspacialFOA: ajustes.canalesElegidos == .espacial,
                frecuenciaHz: Int(ajustes.frecuencia_hz),
                canales: ajustes.canalesDeAudio,
                microfono: seleccionMicrofono,
                segmentos: [],
                eventos: [EventoNotaViva(tipo: .inicio, segundo: 0)],
                archivoTranscripcion: nil, transcripcion: nil, capitulos: nil)

            guard let checkpoint = self.manifiesto,
                  let manifiestoURL = self.archivo else {
                throw Fallo.sinPaquete
            }
            try RepositorioManifiestoNotaViva.guardarCheckpoint(
                checkpoint, en: manifiestoURL)

            audio.comenzar(ajustes: ajustes, capacidades: capacidades,
                           seleccionMicrofono: seleccionMicrofono,
                           carpeta: paquete, permitirSilencio: true) { [weak self] resultado in
                guard let self else { return }
                switch resultado {
                case .failure(let error):
                    self.eliminarPaqueteFallidoAntesDeComenzar()
                    self.reiniciarEstado()
                    completado(.failure(error))
                case .success:
                    self.patronReal = self.audio.patronReal
                    let inicio = CACurrentMediaTime()
                    _ = self.reloj.comenzar(en: inicio)
                    self.manifiesto?.inicioMonotono = inicio
                    self.manifiesto?.segmentos = [SegmentoRelojNotaViva(
                        segundoInicio: 0, monotonoInicio: inicio,
                        monotonoFin: nil)]
                    self.cambiar(a: .grabando)
                    completado(.success(()))
                }
            }
        } catch {
            eliminarPaqueteFallidoAntesDeComenzar()
            reiniciarEstado()
            completado(.failure(error))
        }
    }

    func marcar(_ tipo: TipoEventoNotaViva, titulo: String? = nil,
                pagina: Int? = nil, punto: CGPoint? = nil,
                recursoId: String? = nil, diapositivaId: UUID? = nil) {
        guard estado != .detenida else { return }
        manifiesto?.eventos.append(EventoNotaViva(
            tipo: tipo, segundo: segundosGrabados, titulo: titulo,
            pagina: pagina, x: punto.map { Double($0.x) },
            y: punto.map { Double($0.y) }, recursoId: recursoId,
            diapositivaId: diapositivaId))
    }

    func pausar() {
        pausarLineaTemporal(solicitarAudio: true)
    }

    private func pausarLineaTemporal(solicitarAudio: Bool) {
        guard estado == .grabando else { return }
        let finMonotono = CACurrentMediaTime()
        guard let segundo = reloj.pausar(en: finMonotono) else { return }
        if solicitarAudio { audio.pausar() }
        if let ultimo = manifiesto?.segmentos.indices.last {
            manifiesto?.segmentos[ultimo].monotonoFin = finMonotono
        }
        manifiesto?.eventos.append(EventoNotaViva(tipo: .pausa,
                                                  segundo: segundo))
        cambiar(a: .pausada)
    }

    func reanudar() {
        reanudarLineaTemporal(solicitarAudio: true)
    }

    private func reanudarLineaTemporal(solicitarAudio: Bool) {
        guard estado == .pausada else { return }
        if solicitarAudio {
            audio.reanudar()
            guard audio.estado == .grabando else { return }
        }
        let inicio = CACurrentMediaTime()
        guard reloj.reanudar(en: inicio) else { return }
        manifiesto?.segmentos.append(SegmentoRelojNotaViva(
            segundoInicio: reloj.acumulado, monotonoInicio: inicio,
            monotonoFin: nil))
        manifiesto?.eventos.append(EventoNotaViva(tipo: .reanudacion,
                                                  segundo: reloj.acumulado))
        cambiar(a: .grabando)
    }

    func detener(completado: @escaping (Result<URL, Error>) -> Void) {
        guard estado != .detenida, !deteniendo else {
            completado(.failure(Fallo.noEstaGrabando))
            return
        }
        deteniendo = true
        let finMonotono = CACurrentMediaTime()
        _ = reloj.detener(en: finMonotono)
        if let ultimo = manifiesto?.segmentos.indices.last,
           manifiesto?.segmentos[ultimo].monotonoFin == nil {
            manifiesto?.segmentos[ultimo].monotonoFin = finMonotono
        }
        audio.detener { [weak self] resultado in
            guard let self else { return }
            switch resultado {
            case .failure(let error):
                self.conservarNotaPeseAFalloAudio(error, completado: completado)
            case .success(let audioURL):
                do {
                    let manifiestoURL = try self.finalizar(audioURL: audioURL)
                    self.deteniendo = false
                    self.cambiar(a: .detenida)
                    completado(.success(manifiestoURL))
                    if let audioURL {
                        TranscriptorNotaViva.compartido.transcribir(
                            audio: audioURL, manifiesto: manifiestoURL
                        ) { [weak self] resultado in
                            guard case .failure(let error) = resultado else { return }
                            self?.registrarAdvertenciaTranscripcion(
                                error, manifiestoURL: manifiestoURL)
                        }
                    }
                } catch {
                    self.deteniendo = false
                    self.cambiar(a: .detenida)
                    completado(.failure(error))
                }
            }
        }
    }

    private func finalizar(audioURL: URL?) throws -> URL {
        guard let paquete, let sesion, var manifiesto else { throw Fallo.sinPaquete }
        guard let datosTinta = sesion.guardar() else { throw Fallo.sinTinta }
        // La fotografía final es la red de seguridad del paquete. Debe quedar
        // persistida antes de consumir la cronología opcional paso a paso.
        try datosTinta.write(to: paquete.appendingPathComponent(manifiesto.archivoTinta),
                            options: [.atomic, .completeFileProtection])
        guardarCronologiaSiDisponible(en: paquete, sesion: sesion,
                                      manifiesto: &manifiesto)
        manifiesto.duracion = reloj.acumulado
        manifiesto.archivoAudio = audioURL?.lastPathComponent
        manifiesto.eventos.append(EventoNotaViva(tipo: .fin,
                                                 segundo: reloj.acumulado))
        let destino = paquete.appendingPathComponent("manifest.json")
        try RepositorioManifiestoNotaViva.guardar(manifiesto, en: destino)
        if let audioURL {
            try? FileManager.default.setAttributes(
                [.protectionKey: FileProtectionType.complete],
                ofItemAtPath: audioURL.path)
        }
        self.manifiesto = manifiesto
        return destino
    }

    private func guardarCronologiaSiDisponible(
        en paquete: URL,
        sesion: Sesion,
        manifiesto: inout ManifiestoNotaViva
    ) {
        guard let datosCronologia = sesion.finalizarCronologiaNotaViva() else {
            let detalle = sesion.ultimoError
            sesion.cancelarCronologiaNotaViva()
            registrarAdvertenciaCronologia(
                detalle.isEmpty ? Fallo.sinCronologia.localizedDescription : detalle,
                manifiesto: &manifiesto)
            alAdvertir?(Fallo.sinCronologia)
            return
        }
        let nombreCronologia = "cronologia.tintalive"
        do {
            try datosCronologia.write(
                to: paquete.appendingPathComponent(nombreCronologia),
                options: [.atomic, .completeFileProtection])
            manifiesto.archivoCronologia = nombreCronologia
        } catch {
            manifiesto.archivoCronologia = nil
            registrarAdvertenciaCronologia(error.localizedDescription,
                                           manifiesto: &manifiesto)
            alAdvertir?(error)
        }
    }

    private func registrarAdvertenciaCronologia(
        _ detalle: String,
        manifiesto: inout ManifiestoNotaViva
    ) {
        let limpio = detalle.trimmingCharacters(in: .whitespacesAndNewlines)
        let mensaje = limpio.isEmpty
            ? "La Nota Viva se guardó sin reproducción paso a paso; la copia final de tinta permanece disponible."
            : "La Nota Viva se guardó sin reproducción paso a paso. La copia final de tinta permanece disponible. Detalle: \(limpio)"
        if manifiesto.advertencias?.contains(mensaje) != true {
            manifiesto.advertencias = (manifiesto.advertencias ?? []) + [mensaje]
        }
    }

    /// La tinta y la cronología no se desechan porque falle el micrófono. Se
    /// cierra un paquete reproducible, se marca la pista como parcial y se deja
    /// la advertencia dentro del manifiesto para que recuperación y UI no la
    /// confundan con audio íntegro.
    private func conservarNotaPeseAFalloAudio(
        _ error: Error,
        completado: @escaping (Result<URL, Error>) -> Void
    ) {
        guard var manifiesto else {
            deteniendo = false
            cambiar(a: .detenida)
            completado(.failure(error))
            return
        }
        let detalle = error.localizedDescription
        if manifiesto.advertencias?.contains(detalle) != true {
            manifiesto.advertencias = (manifiesto.advertencias ?? []) + [detalle]
        }
        if let parcial = audio.archivo,
           ((try? parcial.resourceValues(forKeys: [.fileSizeKey]).fileSize) ?? 0) > 0 {
            manifiesto.archivoAudioParcial = parcial.lastPathComponent
            try? FileManager.default.setAttributes(
                [.protectionKey: FileProtectionType.complete],
                ofItemAtPath: parcial.path)
        }
        self.manifiesto = manifiesto
        do {
            let manifiestoURL = try finalizar(audioURL: nil)
            deteniendo = false
            cambiar(a: .detenida)
            if falloAudioPendiente == nil { alFallar?(error) }
            completado(.success(manifiestoURL))
        } catch {
            deteniendo = false
            cambiar(a: .detenida)
            completado(.failure(error))
        }
    }

    private func manejarFalloAudio(_ error: Error) {
        if falloAudioPendiente == nil { falloAudioPendiente = error }
        if estado == .grabando {
            pausarLineaTemporal(solicitarAudio: true)
        }
        alFallar?(error)
    }

    private func registrarAdvertenciaTranscripcion(
        _ falloTranscripcion: Error,
        manifiestoURL: URL
    ) {
        do {
            let detalle = "Transcripción local: \(falloTranscripcion.localizedDescription)"
            _ = try RepositorioManifiestoNotaViva.actualizar(manifiestoURL) {
                if $0.advertencias?.contains(detalle) != true {
                    $0.advertencias = ($0.advertencias ?? []) + [detalle]
                }
            }
        } catch {
            // El aviso principal sigue siendo el fallo de transcripción. El
            // manifiesto ya existe y no se vuelve a escribir con datos dudosos.
        }
        alAdvertir?(falloTranscripcion)
    }

    private func cambiar(a nuevo: EstadoGrabacion) {
        estado = nuevo
        alCambiarEstado?(nuevo)
    }

    private func nombrePaquete() -> String {
        let formato = ISO8601DateFormatter()
        formato.formatOptions = [.withFullDate, .withTime, .withColonSeparatorInTime]
        let sello = formato.string(from: Date()).replacingOccurrences(of: ":", with: "-")
        return "Nota Viva \(sello)-\(UUID().uuidString.prefix(8)).tintaviva"
    }

    private func eliminarPaqueteFallidoAntesDeComenzar() {
        guard let paquete else { return }
        try? FileManager.default.removeItem(at: paquete)
    }

    private func reiniciarEstado() {
        sesion?.cancelarCronologiaNotaViva()
        paquete = nil
        archivo = nil
        sesion = nil
        manifiesto = nil
        reloj.reiniciar()
        deteniendo = false
        falloAudioPendiente = nil
        cambiar(a: .detenida)
    }
}