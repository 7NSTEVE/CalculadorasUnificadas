import AVFoundation
import CoreMedia
import Foundation

/// Captura ambisonía de primer orden (FOA) mediante la ruta pública de
/// AVFoundation. No convierte un M4A de cuatro canales ni duplica una pista
/// estéreo: `AVCaptureMovieFileOutput` es quien escribe el QuickTime con la
/// pista FOA, la alternativa estéreo y sus relaciones de compatibilidad.
///
/// Esta API existe desde iOS 18 y solo hace efecto con el arreglo de
/// micrófonos integrado. El bitrate y el códec de las pistas espaciales los
/// decide iPadOS; aplicar aquí el valor AAC de mono/estéreo produciría un
/// archivo distinto de lo que la interfaz promete.
final class GrabadoraAudioEspacial: NSObject,
                                    AVCaptureFileOutputRecordingDelegate {

    enum Fallo: LocalizedError {
        case yaGrabando
        case sistemaNoCompatible
        case requiereMicrofonoIntegrado
        case sinDispositivoDeAudio
        case entradaNoCompatible
        case noSePuedeAgregarEntrada
        case noSePuedeAgregarSalida
        case noComenzo
        case archivoVacio
        case errorDeSesion(String)

        var errorDescription: String? {
            switch self {
            case .yaGrabando:
                return "Ya hay una captura de audio 3D en curso."
            case .sistemaNoCompatible:
                return "El audio 3D requiere iPadOS 18 o posterior y hardware con captura FOA."
            case .requiereMicrofonoIntegrado:
                return "El audio 3D solo funciona con el arreglo de micrófonos integrado del iPad."
            case .sinDispositivoDeAudio:
                return "iPadOS no expuso un dispositivo de captura de audio."
            case .entradaNoCompatible:
                return "La entrada activa no admite ambisonía de primer orden."
            case .noSePuedeAgregarEntrada:
                return "No se pudo conectar el micrófono a la sesión de audio 3D."
            case .noSePuedeAgregarSalida:
                return "No se pudo preparar la salida QuickTime de audio 3D."
            case .noComenzo:
                return "La captura de audio 3D no llegó a comenzar."
            case .archivoVacio:
                return "La captura de audio 3D terminó sin muestras reproducibles."
            case .errorDeSesion(let detalle):
                return "La sesión de audio 3D se interrumpió: \(detalle)"
            }
        }
    }

    private let sesion = AVCaptureSession()
    private let salida = AVCaptureMovieFileOutput()
    private let cola = DispatchQueue(label: "cr.tinta.audio-espacial")
    private var alComenzar: ((Result<Void, Error>) -> Void)?
    private var alDetener: ((Result<URL?, Error>) -> Void)?
    private var preparando = false
    private var falloInesperado: Error?
    private var interrumpidaPorSistema = false
    private var observadores: [NSObjectProtocol] = []

    private(set) var estado: EstadoGrabacion = .detenida
    private(set) var archivo: URL?
    var alCambiarEstado: ((EstadoGrabacion) -> Void)?
    var alFallar: ((Error) -> Void)?
    var alPausarPorSistema: (() -> Void)?
    var alReanudarPorSistema: (() -> Void)?

    override init() {
        super.init()
        observadores.append(NotificationCenter.default.addObserver(
            forName: AVCaptureSession.runtimeErrorNotification,
            object: sesion, queue: .main) { [weak self] notificacion in
                let error = notificacion.userInfo?[AVCaptureSessionErrorKey]
                    as? Error
                    ?? Fallo.errorDeSesion("error desconocido de AVFoundation")
                self?.manejarFalloInesperado(error)
            })
        observadores.append(NotificationCenter.default.addObserver(
            forName: AVCaptureSession.wasInterruptedNotification,
            object: sesion, queue: .main) { [weak self] _ in
                self?.manejarInterrupcion()
            })
        observadores.append(NotificationCenter.default.addObserver(
            forName: AVCaptureSession.interruptionEndedNotification,
            object: sesion, queue: .main) { [weak self] _ in
                self?.manejarFinInterrupcion()
            })
    }

    deinit {
        observadores.forEach(NotificationCenter.default.removeObserver)
    }

    var segundosGrabados: Double {
        let valor = CMTimeGetSeconds(salida.recordedDuration)
        return valor.isFinite && valor > 0 ? valor : 0
    }

    var bytesGrabados: Int64 { max(salida.recordedFileSize, 0) }

    /// Consulta la capacidad del `AVCaptureDeviceInput`; no se infiere por la
    /// cantidad de micrófonos ni por `maximumInputNumberOfChannels`.
    static func disponibleEnEsteIPad() -> Bool {
        guard #available(iOS 18.0, *) else { return false }
        guard let dispositivo = AVCaptureDevice.default(for: .audio),
              let entrada = try? AVCaptureDeviceInput(device: dispositivo) else {
            return false
        }
        return entrada.isMultichannelAudioModeSupported(.firstOrderAmbisonics)
    }

    func comenzar(ajustes: TintaAjustesGrabacion,
                  seleccionMicrofono: SeleccionMicrofono?,
                  carpeta: URL,
                  completado: @escaping (Result<Void, Error>) -> Void) {
        guard estado == .detenida, !preparando else {
            completado(.failure(Fallo.yaGrabando))
            return
        }
        guard #available(iOS 18.0, *) else {
            completado(.failure(Fallo.sistemaNoCompatible))
            return
        }
        guard Microfonos.seleccionPermiteAudioEspacial(seleccionMicrofono) else {
            completado(.failure(Fallo.requiereMicrofonoIntegrado))
            return
        }

        preparando = true
        falloInesperado = nil
        interrumpidaPorSistema = false
        archivo = nil
        alComenzar = completado

        do {
            _ = try Microfonos.preparar(ajustes: ajustes,
                                        seleccion: seleccionMicrofono)
            try FileManager.default.createDirectory(at: carpeta,
                                                    withIntermediateDirectories: true)
        } catch {
            preparando = false
            alComenzar = nil
            Microfonos.liberar()
            completado(.failure(error))
            return
        }

        let destino = carpeta.appendingPathComponent(nombreArchivo())
        archivo = destino
        cola.async { [weak self] in
            guard let self else { return }
            do {
                try self.configurarFOA()
                self.sesion.startRunning()
                self.salida.startRecording(to: destino, recordingDelegate: self)
            } catch {
                if self.sesion.isRunning { self.sesion.stopRunning() }
                DispatchQueue.main.async { [weak self] in
                    guard let self else { return }
                    self.preparando = false
                    if let parcial = self.archivo {
                        try? FileManager.default.removeItem(at: parcial)
                    }
                    self.archivo = nil
                    let respuesta = self.alComenzar
                    self.alComenzar = nil
                    Microfonos.liberar()
                    respuesta?(.failure(error))
                }
            }
        }
    }

    func pausar() {
        guard estado == .grabando, salida.isRecording else { return }
        interrumpidaPorSistema = false
        cola.async { [weak self] in
            guard let self, self.salida.isRecording else { return }
            if #available(iOS 18.0, *) {
                self.salida.pauseRecording()
            }
        }
        cambiar(a: .pausada)
    }

    func reanudar() {
        guard estado == .pausada, salida.isRecording else { return }
        interrumpidaPorSistema = false
        cola.async { [weak self] in
            guard let self, self.salida.isRecording else { return }
            if #available(iOS 18.0, *) {
                self.salida.resumeRecording()
            }
        }
        cambiar(a: .grabando)
    }

    func detener(completado: @escaping (Result<URL?, Error>) -> Void) {
        guard estado != .detenida || preparando else {
            completado(.success(archivo))
            return
        }
        alDetener = completado
        cola.async { [weak self] in
            guard let self else { return }
            if self.salida.isRecording {
                self.salida.stopRecording()
            } else {
                DispatchQueue.main.async { [weak self] in
                    self?.finalizarSinGrabacion(error: Fallo.noComenzo)
                }
            }
        }
    }

    @available(iOS 18.0, *)
    private func configurarFOA() throws {
        if sesion.isRunning { sesion.stopRunning() }
        sesion.beginConfiguration()
        defer { sesion.commitConfiguration() }

        sesion.inputs.forEach(sesion.removeInput)
        sesion.outputs.forEach(sesion.removeOutput)
        sesion.usesApplicationAudioSession = true
        sesion.automaticallyConfiguresApplicationAudioSession = false
        sesion.configuresApplicationAudioSessionToMixWithOthers = true

        guard let dispositivo = AVCaptureDevice.default(for: .audio) else {
            throw Fallo.sinDispositivoDeAudio
        }
        let entrada = try AVCaptureDeviceInput(device: dispositivo)
        guard entrada.isMultichannelAudioModeSupported(.firstOrderAmbisonics) else {
            throw Fallo.entradaNoCompatible
        }
        entrada.multichannelAudioMode = .firstOrderAmbisonics

        guard sesion.canAddInput(entrada) else {
            throw Fallo.noSePuedeAgregarEntrada
        }
        sesion.addInput(entrada)
        guard sesion.canAddOutput(salida) else {
            throw Fallo.noSePuedeAgregarSalida
        }
        sesion.addOutput(salida)

        // Fragmentos cortos reducen la pérdida recuperable si iPadOS corta la
        // sesión por falta de espacio o una interrupción del sistema.
        salida.movieFragmentInterval = CMTime(seconds: 2,
                                              preferredTimescale: 600)
    }

    func fileOutput(_ output: AVCaptureFileOutput,
                    didStartRecordingTo fileURL: URL,
                    from connections: [AVCaptureConnection]) {
        cola.async { [weak self] in
            guard let self else { return }
            if self.falloInesperado != nil {
                if self.salida.isRecording { self.salida.stopRecording() }
                return
            }
            DispatchQueue.main.async { [weak self] in
                guard let self else { return }
                self.preparando = false
                self.cambiar(a: .grabando)
                let respuesta = self.alComenzar
                self.alComenzar = nil
                respuesta?(.success(()))
            }
        }
    }

    func fileOutput(_ output: AVCaptureFileOutput,
                    didFinishRecordingTo outputFileURL: URL,
                    from connections: [AVCaptureConnection],
                    error: Error?) {
        let errorReal: Error?
        if let error {
            let ns = error as NSError
            let terminoBien = ns.userInfo[AVErrorRecordingSuccessfullyFinishedKey]
                as? Bool ?? false
            errorReal = terminoBien ? nil : error
        } else {
            errorReal = nil
        }

        cola.async { [weak self] in
            guard let self else { return }
            if self.sesion.isRunning { self.sesion.stopRunning() }
            DispatchQueue.main.async { [weak self] in
                guard let self else { return }
                self.preparando = false
                self.interrumpidaPorSistema = false
                self.cambiar(a: .detenida)
                Microfonos.liberar()
                try? FileManager.default.setAttributes(
                    [.protectionKey: FileProtectionType.complete],
                    ofItemAtPath: outputFileURL.path)

                let bytes = (try? outputFileURL.resourceValues(
                    forKeys: [.fileSizeKey]).fileSize) ?? 0
                let segundos = CMTimeGetSeconds(output.recordedDuration)
                let archivoValido = bytes > 0 && segundos.isFinite && segundos > 0
                let falloFinal = errorReal
                    ?? self.falloInesperado
                    ?? (archivoValido ? nil : Fallo.archivoVacio)
                self.falloInesperado = nil

                var falloEntregado = false
                if let inicio = self.alComenzar {
                    self.alComenzar = nil
                    inicio(.failure(falloFinal ?? Fallo.noComenzo))
                    falloEntregado = true
                }
                let respuesta = self.alDetener
                self.alDetener = nil
                if let falloFinal {
                    if let respuesta {
                        respuesta(.failure(falloFinal))
                        falloEntregado = true
                    }
                    if !falloEntregado { self.alFallar?(falloFinal) }
                } else {
                    self.archivo = outputFileURL
                    respuesta?(.success(outputFileURL))
                }
            }
        }
    }

    private func finalizarSinGrabacion(error: Error) {
        preparando = false
        cambiar(a: .detenida)
        Microfonos.liberar()
        if let parcial = archivo,
           ((try? parcial.resourceValues(forKeys: [.fileSizeKey]).fileSize) ?? 0) == 0 {
            try? FileManager.default.removeItem(at: parcial)
            archivo = nil
        }
        let inicio = alComenzar
        alComenzar = nil
        inicio?(.failure(error))
        let fin = alDetener
        alDetener = nil
        if let fin {
            fin(.failure(error))
        } else {
            alFallar?(error)
        }
    }

    private func manejarFalloInesperado(_ error: Error) {
        guard preparando || estado != .detenida else { return }
        cola.async { [weak self] in
            guard let self else { return }
            self.falloInesperado = error
            if self.salida.isRecording {
                self.salida.stopRecording()
            } else {
                if self.sesion.isRunning { self.sesion.stopRunning() }
                DispatchQueue.main.async { [weak self] in
                    self?.finalizarSinGrabacion(error: error)
                }
            }
        }
    }

    private func manejarInterrupcion() {
        guard estado == .grabando, salida.isRecording else { return }
        interrumpidaPorSistema = true
        cambiar(a: .pausada)
        alPausarPorSistema?()
    }

    private func manejarFinInterrupcion() {
        guard interrumpidaPorSistema, estado == .pausada,
              salida.isRecording else { return }
        interrumpidaPorSistema = false
        cambiar(a: .grabando)
        alReanudarPorSistema?()
    }

    private func cambiar(a nuevo: EstadoGrabacion) {
        estado = nuevo
        alCambiarEstado?(nuevo)
    }

    private func nombreArchivo() -> String {
        let formato = ISO8601DateFormatter()
        formato.formatOptions = [.withFullDate, .withTime,
                                 .withColonSeparatorInTime]
        let sello = formato.string(from: Date())
            .replacingOccurrences(of: ":", with: "-")
        return "Audio espacial \(sello)-\(UUID().uuidString.prefix(8)).mov"
    }
}
