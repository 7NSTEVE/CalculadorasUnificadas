import AVFoundation
import Speech

/// Escribir a voz, con la transcripción apareciendo mientras se habla.
///
/// Dos decisiones:
///
/// - **`requiresOnDeviceRecognition`.** El reconocimiento va en el propio iPad:
///   funciona sin red —en un aula sin señal decente es la diferencia entre que
///   sirva o no— y la voz no sale del aparato. Si el idioma no está descargado
///   se cae al servidor de iPadOS, y eso se dice en pantalla.
/// - **Resultados parciales.** Se entrega texto en cuanto hay algo, no al
///   terminar la frase: eso es lo que hace que se vea «en tiempo real».
///
/// La limpieza del texto —«coma» a «,», mayúscula después de punto— **no está
/// aquí**: está en el núcleo C++, donde se prueba en Windows sin micrófono.
final class DictadoEnVivo {

    enum Fallo: LocalizedError {
        case sinPermiso
        case idiomaNoDisponible(String)
        case motorCaido
        case modeloLocalNoDisponible(String)

        var errorDescription: String? {
            switch self {
            case .sinPermiso:
                return "Falta el permiso de dictado o de micrófono."
            case .idiomaNoDisponible(let idioma):
                return "El reconocimiento de voz no está disponible para \(idioma)."
            case .motorCaido:
                return "No se pudo arrancar el micrófono."
            case .modeloLocalNoDisponible(let idioma):
                return "Descargue el reconocimiento de voz de \(idioma) para usar este modo sin enviar audio fuera del iPad."
            }
        }
    }

    /// Texto que se va formando. Llega muchas veces por segundo con el mismo
    /// trozo corregido, no una vez por palabra.
    var alTranscribir: ((String, Bool) -> Void)?
    var alFallar: ((Error) -> Void)?

    private(set) var escuchando = false
    private(set) var enDispositivo = false

    var puntuacionHablada = true
    var mayusculaInicial = true
    /// Los modos de estudio oral lo activan para fallar cerrado si el modelo
    /// local del idioma no está instalado.
    var exigirEnDispositivo = false

    private let motor = AVAudioEngine()
    private var reconocedor: SFSpeechRecognizer?
    private var peticion: SFSpeechAudioBufferRecognitionRequest?
    private var tarea: SFSpeechRecognitionTask?

    private let idioma: String

    init(idioma: String = "es-CR") {
        self.idioma = idioma
        // Si el país concreto no está, el idioma suelto casi siempre sí.
        reconocedor = SFSpeechRecognizer(locale: Locale(identifier: idioma))
            ?? SFSpeechRecognizer(locale: Locale(identifier: String(idioma.prefix(2))))
    }

    // MARK: - Permisos

    /// Pide los dos permisos que hacen falta: dictado y micrófono.
    static func pedirPermisos(_ completado: @escaping (Bool) -> Void) {
        SFSpeechRecognizer.requestAuthorization { estado in
            guard estado == .authorized else {
                DispatchQueue.main.async { completado(false) }
                return
            }
            let sesion = AVAudioSession.sharedInstance()
            if #available(iOS 17.0, *) {
                AVAudioApplication.requestRecordPermission { concedido in
                    DispatchQueue.main.async { completado(concedido) }
                }
            } else {
                sesion.requestRecordPermission { concedido in
                    DispatchQueue.main.async { completado(concedido) }
                }
            }
        }
    }

    // MARK: - Escuchar

    func comenzar() throws {
        guard !escuchando else { return }
        guard let reconocedor, reconocedor.isAvailable else {
            throw Fallo.idiomaNoDisponible(idioma)
        }
        guard SFSpeechRecognizer.authorizationStatus() == .authorized else {
            throw Fallo.sinPermiso
        }

        detener()

        let sesion = AVAudioSession.sharedInstance()
        try sesion.setCategory(.playAndRecord, mode: .measurement,
                               options: [.duckOthers, .defaultToSpeaker])
        try sesion.setActive(true, options: .notifyOthersOnDeactivation)

        let peticion = SFSpeechAudioBufferRecognitionRequest()
        peticion.shouldReportPartialResults = true
        // Si el modelo está descargado, todo pasa dentro del iPad.
        enDispositivo = reconocedor.supportsOnDeviceRecognition
        if exigirEnDispositivo && !enDispositivo {
            throw Fallo.modeloLocalNoDisponible(idioma)
        }
        peticion.requiresOnDeviceRecognition = enDispositivo
        if #available(iOS 16.0, *) {
            // El sistema pone la puntuación obvia; la dictada a propósito
            // («coma», «punto y aparte») la resuelve el núcleo encima.
            peticion.addsPunctuation = true
        }
        self.peticion = peticion

        let entrada = motor.inputNode
        let formato = entrada.outputFormat(forBus: 0)
        entrada.removeTap(onBus: 0)
        entrada.installTap(onBus: 0, bufferSize: 1024, format: formato) { buffer, _ in
            peticion.append(buffer)
        }

        motor.prepare()
        do {
            try motor.start()
        } catch {
            throw Fallo.motorCaido
        }

        tarea = reconocedor.recognitionTask(with: peticion) { [weak self] resultado, error in
            guard let self else { return }
            if let resultado {
                let crudo = resultado.bestTranscription.formattedString
                let limpio = Dictado.limpiar(crudo,
                                             puntuacionHablada: self.puntuacionHablada,
                                             mayusculaInicial: self.mayusculaInicial)
                self.alTranscribir?(limpio, resultado.isFinal)
            }
            if let error {
                self.detener()
                self.alFallar?(error)
            }
        }

        escuchando = true
    }

    func detener() {
        if motor.isRunning {
            motor.stop()
            motor.inputNode.removeTap(onBus: 0)
        }
        peticion?.endAudio()
        tarea?.cancel()
        peticion = nil
        tarea = nil
        escuchando = false
    }

    deinit { detener() }
}
