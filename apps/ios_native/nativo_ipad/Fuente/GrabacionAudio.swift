import AVFoundation
import QuartzCore

/// Grabación AAC/M4A del puerto y data source elegidos. Esta ruta no inicia
/// ReplayKit ni crea una pista de vídeo: «solo audio» significa literalmente
/// solo audio.
final class GrabadoraAudio: NSObject, AVAudioRecorderDelegate {

    enum Fallo: LocalizedError {
        case yaGrabando
        case microfonoDesactivado
        case noPudoPreparar
        case noPudoComenzar
        case noEstaGrabando
        case archivoVacio
        case interrupcionNoRecuperable
        case microfonoDesconectado
        case serviciosDeAudioReiniciados

        var errorDescription: String? {
            switch self {
            case .yaGrabando: return "Ya hay una grabación de audio en curso."
            case .microfonoDesactivado: return "Active el micrófono para grabar solo audio."
            case .noPudoPreparar: return "No se pudo preparar el archivo M4A."
            case .noPudoComenzar: return "El micrófono no pudo comenzar a grabar."
            case .noEstaGrabando: return "No hay una grabación de audio que detener."
            case .archivoVacio: return "La grabación terminó sin audio utilizable."
            case .interrupcionNoRecuperable:
                return "iPadOS interrumpió la grabación y no permitió reanudarla."
            case .microfonoDesconectado:
                return "El micrófono seleccionado se desconectó. La grabación se pausó para no cambiar de fuente sin avisar."
            case .serviciosDeAudioReiniciados:
                return "iPadOS reinició los servicios de audio; esta grabación no puede continuar."
            }
        }
    }

    private(set) var estado: EstadoGrabacion = .detenida
    private(set) var archivo: URL?
    private(set) var patronReal: PatronPolar = .automatico
    var alCambiarEstado: ((EstadoGrabacion) -> Void)?
    var alFallar: ((Error) -> Void)?
    /// Cambios que no nacen de un botón de la app. Nota Viva usa estas señales
    /// para cerrar y abrir sus segmentos del reloj sin desincronizar la tinta.
    var alPausarPorSistema: (() -> Void)?
    var alReanudarPorSistema: (() -> Void)?

    var segundosGrabados: Double {
        if usandoEspacial { return espacial.segundosGrabados }
        if let grabador { return grabador.currentTime }
        return acumulado
    }

    var bytesGrabados: Int64 {
        if usandoEspacial { return espacial.bytesGrabados }
        guard let archivo else { return 0 }
        return Int64((try? archivo.resourceValues(forKeys: [.fileSizeKey]).fileSize) ?? 0)
    }

    private let espacial = GrabadoraAudioEspacial()
    private var grabador: AVAudioRecorder?
    private var acumulado: Double = 0
    private var usandoEspacial = false
    private var deteniendo = false
    private var falloGrabacion: Error?
    private var lineaTemporalSinAudio = false
    private var pausadaPorInterrupcion = false
    private var sesionMicrofonoActiva = false
    private var puertoEntradaActivoUID: String?
    private var observadores: [NSObjectProtocol] = []

    override init() {
        super.init()
        espacial.alCambiarEstado = { [weak self] estado in
            self?.cambiar(a: estado)
        }
        espacial.alFallar = { [weak self] error in self?.registrarFallo(error) }
        espacial.alPausarPorSistema = { [weak self] in
            self?.alPausarPorSistema?()
        }
        espacial.alReanudarPorSistema = { [weak self] in
            self?.alReanudarPorSistema?()
        }
        let centro = NotificationCenter.default
        observadores.append(centro.addObserver(
            forName: AVAudioSession.interruptionNotification,
            object: nil, queue: .main) { [weak self] notificacion in
                self?.manejarInterrupcion(notificacion)
            })
        observadores.append(centro.addObserver(
            forName: AVAudioSession.routeChangeNotification,
            object: nil, queue: .main) { [weak self] notificacion in
                self?.manejarCambioRuta(notificacion)
            })
        observadores.append(centro.addObserver(
            forName: AVAudioSession.mediaServicesWereResetNotification,
            object: nil, queue: .main) { [weak self] _ in
                self?.manejarReinicioServiciosAudio()
            })
    }

    deinit {
        observadores.forEach(NotificationCenter.default.removeObserver)
    }

    func comenzar(ajustes pedidos: TintaAjustesGrabacion,
                  capacidades: TintaCapacidades,
                  seleccionMicrofono: SeleccionMicrofono?,
                  carpeta: URL,
                  permitirSilencio: Bool = false,
                  completado: @escaping (Result<Void, Error>) -> Void) {
        guard estado == .detenida else {
            completado(.failure(Fallo.yaGrabando))
            return
        }

        archivo = nil
        patronReal = .automatico
        usandoEspacial = false
        deteniendo = false
        falloGrabacion = nil
        lineaTemporalSinAudio = false
        pausadaPorInterrupcion = false
        sesionMicrofonoActiva = false
        puertoEntradaActivoUID = nil

        let ajustes = pedidos.ajustado(a: capacidades)
        if !ajustes.microfono {
            if permitirSilencio {
                lineaTemporalSinAudio = true
                acumulado = 0
                cambiar(a: .grabando)
                completado(.success(()))
            } else {
                completado(.failure(Fallo.microfonoDesactivado))
            }
            return
        }

        if ajustes.canalesElegidos == .espacial {
            usandoEspacial = true
            espacial.comenzar(ajustes: ajustes,
                              seleccionMicrofono: seleccionMicrofono,
                              carpeta: carpeta) { [weak self] resultado in
                guard let self else { return }
                switch resultado {
                case .success:
                    self.archivo = self.espacial.archivo
                    self.patronReal = .automatico
                case .failure:
                    self.usandoEspacial = false
                }
                completado(resultado)
            }
            return
        }

        do {
            sesionMicrofonoActiva = true
            patronReal = try Microfonos.preparar(ajustes: ajustes,
                                                 seleccion: seleccionMicrofono)
            try FileManager.default.createDirectory(at: carpeta,
                                                    withIntermediateDirectories: true)
            let destino = carpeta.appendingPathComponent(nombreArchivo(ext: "m4a"))
            archivo = destino
            let configuracion: [String: Any] = [
                AVFormatIDKey: kAudioFormatMPEG4AAC,
                AVSampleRateKey: Double(ajustes.frecuencia_hz),
                AVNumberOfChannelsKey: max(1, ajustes.canalesDeAudio),
                AVEncoderBitRateKey: max(24_000, ajustes.kbpsAudio(capacidades) * 1000),
                AVEncoderAudioQualityKey: AVAudioQuality.high.rawValue,
            ]
            let grabador = try AVAudioRecorder(url: destino,
                                               settings: configuracion)
            grabador.delegate = self
            guard grabador.prepareToRecord() else { throw Fallo.noPudoPreparar }
            guard grabador.record() else { throw Fallo.noPudoComenzar }
            self.grabador = grabador
            puertoEntradaActivoUID = AVAudioSession.sharedInstance()
                .currentRoute.inputs.first?.uid
                ?? Microfonos.fuente(
                    seleccion: seleccionMicrofono,
                    posicion: ajustes.posicionElegida)?.puerto.uid
            acumulado = 0
            cambiar(a: .grabando)
            completado(.success(()))
        } catch {
            let parcial = archivo
            archivo = nil
            if let parcial { try? FileManager.default.removeItem(at: parcial) }
            liberarSesionMicrofono()
            completado(.failure(error))
        }
    }

    func pausar() {
        if usandoEspacial {
            espacial.pausar()
            return
        }
        guard estado == .grabando else { return }
        pausadaPorInterrupcion = false
        acumulado = grabador?.currentTime ?? acumulado
        grabador?.pause()
        cambiar(a: .pausada)
    }

    func reanudar() {
        if usandoEspacial {
            espacial.reanudar()
            return
        }
        guard estado == .pausada, !pausadaPorInterrupcion else { return }
        guard falloGrabacion == nil else { return }
        pausadaPorInterrupcion = false
        if lineaTemporalSinAudio {
            cambiar(a: .grabando)
        } else if let grabador, grabador.record() {
            cambiar(a: .grabando)
        } else {
            registrarFallo(Fallo.noPudoComenzar)
        }
    }

    func detener(completado: @escaping (Result<URL?, Error>) -> Void) {
        if usandoEspacial {
            guard !deteniendo else {
                completado(.failure(Fallo.noEstaGrabando))
                return
            }
            deteniendo = true
            espacial.detener { [weak self] resultado in
                guard let self else { return }
                if case .success(let url) = resultado { self.archivo = url }
                self.usandoEspacial = false
                self.deteniendo = false
                if let falloGrabacion = self.falloGrabacion {
                    completado(.failure(falloGrabacion))
                } else {
                    completado(resultado)
                }
            }
            return
        }
        guard estado != .detenida, !deteniendo else {
            completado(.failure(Fallo.noEstaGrabando))
            return
        }
        deteniendo = true
        acumulado = grabador?.currentTime ?? acumulado
        grabador?.stop()
        let destino = archivo
        grabador = nil
        cambiar(a: .detenida)
        liberarSesionMicrofono()
        deteniendo = false
        lineaTemporalSinAudio = false
        pausadaPorInterrupcion = false
        if let falloGrabacion {
            completado(.failure(falloGrabacion))
        } else if let destino {
            do {
                let archivoAudio = try AVAudioFile(forReading: destino)
                guard archivoAudio.length > 0 else { throw Fallo.archivoVacio }
                try? FileManager.default.setAttributes(
                    [.protectionKey: FileProtectionType.complete],
                    ofItemAtPath: destino.path)
                completado(.success(destino))
            } catch {
                completado(.failure(error))
            }
        } else {
            // Nota Viva puede registrar únicamente la línea temporal cuando el
            // usuario desactiva expresamente el micrófono.
            completado(.success(nil))
        }
    }

    func audioRecorderEncodeErrorDidOccur(
        _ recorder: AVAudioRecorder, error: Error?
    ) {
        registrarFallo(error ?? Fallo.archivoVacio)
    }

    func audioRecorderDidFinishRecording(
        _ recorder: AVAudioRecorder, successfully flag: Bool
    ) {
        if !flag { registrarFallo(Fallo.archivoVacio) }
    }

    private func manejarInterrupcion(_ notificacion: Notification) {
        guard !usandoEspacial, sesionMicrofonoActiva,
              let valor = notificacion.userInfo?[AVAudioSessionInterruptionTypeKey]
                as? UInt,
              let tipo = AVAudioSession.InterruptionType(rawValue: valor)
        else { return }
        switch tipo {
        case .began:
            guard estado == .grabando else { return }
            pausadaPorInterrupcion = true
            acumulado = grabador?.currentTime ?? acumulado
            grabador?.pause()
            cambiar(a: .pausada)
            alPausarPorSistema?()
        case .ended:
            guard estado == .pausada, pausadaPorInterrupcion else { return }
            pausadaPorInterrupcion = false
            let opcionesValor = notificacion.userInfo?[AVAudioSessionInterruptionOptionKey]
                as? UInt ?? 0
            let opciones = AVAudioSession.InterruptionOptions(rawValue: opcionesValor)
            if opciones.contains(.shouldResume), grabador?.record() == true {
                cambiar(a: .grabando)
                alReanudarPorSistema?()
            } else {
                registrarFallo(Fallo.interrupcionNoRecuperable)
            }
        @unknown default:
            registrarFallo(Fallo.interrupcionNoRecuperable)
        }
    }

    private func manejarCambioRuta(_ notificacion: Notification) {
        guard !usandoEspacial, sesionMicrofonoActiva,
              estado != .detenida,
              let valor = notificacion.userInfo?[AVAudioSessionRouteChangeReasonKey]
                as? UInt,
              AVAudioSession.RouteChangeReason(rawValue: valor) == .oldDeviceUnavailable
        else { return }
        let entradaActual = AVAudioSession.sharedInstance().currentRoute.inputs.first?.uid
        guard entradaActual != puertoEntradaActivoUID else { return }
        acumulado = grabador?.currentTime ?? acumulado
        grabador?.pause()
        pausadaPorInterrupcion = false
        cambiar(a: .pausada)
        alPausarPorSistema?()
        registrarFallo(Fallo.microfonoDesconectado)
    }

    private func manejarReinicioServiciosAudio() {
        guard !usandoEspacial, sesionMicrofonoActiva,
              estado != .detenida else { return }
        acumulado = grabador?.currentTime ?? acumulado
        registrarFallo(Fallo.serviciosDeAudioReiniciados)
        grabador?.stop()
        grabador = nil
        pausadaPorInterrupcion = false
        cambiar(a: .detenida)
        liberarSesionMicrofono()
        alPausarPorSistema?()
    }

    private func registrarFallo(_ error: Error) {
        guard falloGrabacion == nil else { return }
        falloGrabacion = error
        if !usandoEspacial, estado == .grabando {
            acumulado = grabador?.currentTime ?? acumulado
            grabador?.pause()
            cambiar(a: .pausada)
        }
        alFallar?(error)
    }

    private func liberarSesionMicrofono() {
        guard sesionMicrofonoActiva else { return }
        sesionMicrofonoActiva = false
        puertoEntradaActivoUID = nil
        Microfonos.liberar()
    }

    private func cambiar(a nuevo: EstadoGrabacion) {
        estado = nuevo
        alCambiarEstado?(nuevo)
    }

    private func nombreArchivo(ext: String) -> String {
        let formato = ISO8601DateFormatter()
        formato.formatOptions = [.withFullDate, .withTime, .withColonSeparatorInTime]
        let sello = formato.string(from: Date()).replacingOccurrences(of: ":", with: "-")
        return "Audio \(sello)-\(UUID().uuidString.prefix(8)).\(ext)"
    }
}
