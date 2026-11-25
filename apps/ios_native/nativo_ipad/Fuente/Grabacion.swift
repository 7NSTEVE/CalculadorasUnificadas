import AVFoundation
import CoreImage
import ReplayKit
import UIKit

enum EstadoGrabacion { case detenida, grabando, pausada }

/// Grabación de pantalla con resolución, FPS, calidad y kbps de verdad.
///
/// **Por qué no basta con `RPScreenRecorder.startRecording`.** Esa vía graba y
/// entrega el archivo ya hecho: no deja elegir ni resolución, ni FPS, ni
/// bitrate, ni el códec. Los ajustes que se pidieron solo existen por el otro
/// camino: `startCapture`, que entrega los buffers en crudo, y codificarlos con
/// `AVAssetWriter`. Es más trabajo y es la única forma de que «elegir 30 FPS a
/// 8 000 kbps en 1080p» signifique algo.
///
/// Qué decide cada quién:
/// - el **núcleo C++** decide tamaño, kbps, canales y qué es imposible aquí;
/// - **esta clase** obedece y mueve los bytes.
final class Grabadora: NSObject {

    enum Fallo: LocalizedError {
        case noDisponible
        case ajustesImposibles(String)
        case sinEscritor
        case yaGrabando
        case noEstaGrabando
        case pistaNoDisponible(String)
        case escrituraFallida(String)
        case sinVideo
        case sinAudioMicrofono
        case muestrasDescartadas(Int, Int)
        case interrupcionNoRecuperable
        case microfonoDesconectado
        case serviciosDeAudioReiniciados

        var errorDescription: String? {
            switch self {
            case .noDisponible: return "La grabación de pantalla no está disponible."
            case .ajustesImposibles(let motivo): return motivo
            case .sinEscritor: return "No se pudo preparar el archivo de vídeo."
            case .yaGrabando: return "Ya hay una grabación en curso."
            case .noEstaGrabando: return "No hay una grabación de pantalla que detener."
            case .pistaNoDisponible(let pista):
                return "El archivo no admite la pista de \(pista) solicitada."
            case .escrituraFallida(let detalle):
                return "El codificador dejó de aceptar datos: \(detalle)"
            case .sinVideo:
                return "ReplayKit no entregó ningún fotograma de vídeo válido."
            case .sinAudioMicrofono:
                return "El micrófono estaba activado, pero no produjo ninguna muestra de audio."
            case .muestrasDescartadas(let descartadas, let recibidas):
                return "El codificador no alcanzó el ritmo y descartó \(descartadas) de \(recibidas) muestras; revise el vídeo antes de confiar en él."
            case .interrupcionNoRecuperable:
                return "iPadOS interrumpió el micrófono del vídeo y no permitió reanudarlo."
            case .microfonoDesconectado:
                return "El micrófono seleccionado se desconectó durante el vídeo."
            case .serviciosDeAudioReiniciados:
                return "iPadOS reinició los servicios de audio durante el vídeo."
            }
        }
    }

    // MARK: - Estado visible

    private(set) var estado: EstadoGrabacion = .detenida
    private(set) var archivo: URL?
    /// Patrón polar que quedó puesto de verdad, que puede no ser el pedido.
    private(set) var patronReal: PatronPolar = .automatico

    var alCambiarEstado: ((EstadoGrabacion) -> Void)?
    var alFallar: ((Error) -> Void)?
    var alAdvertir: ((Error) -> Void)?

    /// Segundos escritos al archivo. El tiempo en pausa no cuenta: si el
    /// contador sumara la pausa, no cuadraría con el vídeo ni con el aviso de
    /// espacio libre.
    var segundosGrabados: Double {
        switch estado {
        case .detenida, .pausada: return acumulado
        case .grabando: return acumulado + max(0, CACurrentMediaTime() - marcaInicio)
        }
    }

    func megabytesEstimados(_ cap: TintaCapacidades) -> Double {
        segundosGrabados / 60.0 * ajustes.megabytesPorMinuto(cap)
    }

    // MARK: - Interno

    private let grabador = RPScreenRecorder.shared()
    private var escritor: AVAssetWriter?
    private var entradaVideo: AVAssetWriterInput?
    private var entradaMicro: AVAssetWriterInput?
    private var entradaApp: AVAssetWriterInput?
    private var adaptador: AVAssetWriterInputPixelBufferAdaptor?

    private var ajustes = TintaAjustesGrabacion.porDefecto
    private var capacidades = TintaCapacidades()
    private var tamano = CGSize.zero

    private var sesionAbierta = false
    private var acumulado: Double = 0
    private var marcaInicio: Double = 0

    /// Cuánto tiempo se ha pasado en pausa, para descontarlo de las marcas de
    /// tiempo. Sin esto el vídeo sale con un hueco congelado del tamaño de la
    /// pausa.
    private var desfase = CMTime.zero
    private var pausadoEn: CMTime?
    private var ultimoVideo = CMTime.invalid
    private var ultimoPTSOrigen = CMTime.invalid
    private var enPausa = false
    private var deteniendo = false
    private var falloCaptura: Error?
    private var falloNotificado = false
    private var videosEscritos = 0
    private var audiosMicroEscritos = 0
    private var audiosAppEscritos = 0
    private var descartesPorPresion = 0
    private var muestrasRecibidas = 0
    private var puertoEntradaActivoUID: String?
    private var pausadaPorInterrupcion = false
    private var aceptandoEventos = false
    private var observadores: [NSObjectProtocol] = []

    private let cola = DispatchQueue(label: "cr.estudiantec.tinta.grabacion")
    private lazy var contexto = CIContext(options: [.useSoftwareRenderer: false])

    override init() {
        super.init()
        let centro = NotificationCenter.default
        observadores.append(centro.addObserver(
            forName: AVAudioSession.interruptionNotification,
            object: nil, queue: .main) { [weak self] notificacion in
                self?.manejarInterrupcionAudio(notificacion)
            })
        observadores.append(centro.addObserver(
            forName: AVAudioSession.routeChangeNotification,
            object: nil, queue: .main) { [weak self] notificacion in
                self?.manejarCambioRutaAudio(notificacion)
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

    // MARK: - Capacidades

    static func admiteCodec(_ codec: CodecVideo) -> Bool {
        // La pregunta se le hace al propio escritor en vez de mirar el modelo de
        // iPad: es la única respuesta que no envejece.
        let prueba: [String: Any] = [
            AVVideoCodecKey: codec == .hevc ? AVVideoCodecType.hevc : AVVideoCodecType.h264,
            AVVideoWidthKey: 1920,
            AVVideoHeightKey: 1080,
        ]
        let tempUrl = URL(fileURLWithPath: NSTemporaryDirectory()).appendingPathComponent(UUID().uuidString + ".mp4")
        guard let writer = try? AVAssetWriter(url: tempUrl, fileType: .mp4) else { return false }
        defer {
            writer.cancelWriting()
            try? FileManager.default.removeItem(at: tempUrl)
        }
        return writer.canApply(outputSettings: prueba, forMediaType: .video)
    }

    // MARK: - Empezar

    func comenzar(ajustes pedidos: TintaAjustesGrabacion,
                  capacidades cap: TintaCapacidades,
                  seleccionMicrofono: SeleccionMicrofono? = nil,
                  carpeta: URL,
                  completado: @escaping (Result<Void, Error>) -> Void) {
        guard estado == .detenida else { return completado(.failure(Fallo.yaGrabando)) }
        guard grabador.isAvailable else { return completado(.failure(Fallo.noDisponible)) }

        // Lo imposible se corrige antes de tocar nada, no en mitad de la clase.
        let corregidos = pedidos.ajustado(a: cap)
        if let motivo = corregidos.problema(cap) {
            return completado(.failure(Fallo.ajustesImposibles(motivo)))
        }

        ajustes = corregidos
        capacidades = cap
        tamano = corregidos.tamano(cap)

        do {
            patronReal = corregidos.microfono
                ? try Microfonos.preparar(ajustes: corregidos,
                                          seleccion: seleccionMicrofono)
                : .automatico
            let destino = try prepararEscritor(en: carpeta)
            archivo = destino
            puertoEntradaActivoUID = corregidos.microfono
                ? (AVAudioSession.sharedInstance().currentRoute.inputs.first?.uid
                    ?? Microfonos.fuente(
                        seleccion: seleccionMicrofono,
                        posicion: corregidos.posicionElegida)?.puerto.uid)
                : nil
        } catch {
            cancelarYEliminarArchivoParcial()
            if corregidos.microfono { Microfonos.liberar() }
            return completado(.failure(error))
        }

        grabador.isMicrophoneEnabled = corregidos.microfono
        grabador.startCapture(handler: { [weak self] buffer, tipo, error in
            guard let self else { return }
            self.cola.async {
                if let error {
                    self.registrarFallo(error)
                } else {
                    self.recibir(buffer, tipo: tipo)
                }
            }
        }, completionHandler: { [weak self] error in
            DispatchQueue.main.async {
                guard let self else { return }
                if let error {
                    self.cola.async {
                        self.cancelarYEliminarArchivoParcial()
                        DispatchQueue.main.async {
                            if self.ajustes.microfono { Microfonos.liberar() }
                            completado(.failure(error))
                        }
                    }
                    return
                }
                self.acumulado = 0
                self.marcaInicio = CACurrentMediaTime()
                self.cambiar(a: .grabando)
                completado(.success(()))
            }
        })
    }

    // MARK: - Pausar

    /// ReplayKit no sabe pausar: lo que se hace es dejar de escribir buffers y
    /// llevar la cuenta del tiempo perdido para descontarlo después. Así el
    /// vídeo queda continuo en vez de con un congelado en medio.
    func pausar() {
        guard estado == .grabando else { return }
        pausadaPorInterrupcion = false
        cola.async {
            self.enPausa = true
            self.pausadoEn = self.ultimoPTSOrigen.isValid
                ? self.ultimoPTSOrigen : nil
        }
        acumulado += max(0, CACurrentMediaTime() - marcaInicio)
        cambiar(a: .pausada)
    }

    func reanudar() {
        guard estado == .pausada, !pausadaPorInterrupcion else { return }
        pausadaPorInterrupcion = false
        cola.async { self.enPausa = false }
        marcaInicio = CACurrentMediaTime()
        cambiar(a: .grabando)
    }

    // MARK: - Detener

    func detener(completado: @escaping (Result<URL, Error>) -> Void) {
        guard estado != .detenida, !deteniendo else {
            completado(.failure(Fallo.noEstaGrabando))
            return
        }
        deteniendo = true
        if estado == .grabando {
            acumulado += max(0, CACurrentMediaTime() - marcaInicio)
        }

        grabador.stopCapture { [weak self] error in
            guard let self else { return }
            self.cola.async {
                self.aceptandoEventos = false
                self.entradaVideo?.markAsFinished()
                self.entradaMicro?.markAsFinished()
                self.entradaApp?.markAsFinished()

                guard let escritor = self.escritor else {
                    let usabaMicrofono = self.ajustes.microfono
                    self.limpiar()
                    DispatchQueue.main.async {
                        self.cambiar(a: .detenida)
                        if usabaMicrofono { Microfonos.liberar() }
                        completado(.failure(error ?? Fallo.sinEscritor))
                    }
                    return
                }
                escritor.finishWriting { [weak self] in
                    guard let self else { return }
                    self.cola.async {
                        let destino = self.archivo
                        let advertencia = self.advertenciaDePresion()
                        let usabaMicrofono = self.ajustes.microfono
                        let fallo = self.falloCaptura
                            ?? escritor.error
                            ?? error
                            ?? (self.videosEscritos == 0 ? Fallo.sinVideo : nil)
                            ?? (usabaMicrofono && self.audiosMicroEscritos == 0
                                ? Fallo.sinAudioMicrofono : nil)
                        if let destino {
                            try? FileManager.default.setAttributes(
                                [.protectionKey: FileProtectionType.complete],
                                ofItemAtPath: destino.path)
                        }
                        self.limpiar()
                        DispatchQueue.main.async {
                            self.cambiar(a: .detenida)
                            if usabaMicrofono { Microfonos.liberar() }
                            if let destino, fallo == nil {
                                completado(.success(destino))
                                if let advertencia { self.alAdvertir?(advertencia) }
                            } else {
                                completado(.failure(fallo ?? Fallo.sinEscritor))
                            }
                        }
                    }
                }
            }
        }
    }

    // MARK: - Preparar el escritor

    private func prepararEscritor(en carpeta: URL) throws -> URL {
        try FileManager.default.createDirectory(at: carpeta,
                                                withIntermediateDirectories: true)
        let sello = ISO8601DateFormatter()
        sello.formatOptions = [.withFullDate, .withTime, .withColonSeparatorInTime]
        let nombre = "Clase \(sello.string(from: Date()))"
            .replacingOccurrences(of: ":", with: "-")
        let destino = carpeta.appendingPathComponent(
            "\(nombre)-\(UUID().uuidString.prefix(8)).mp4")
        archivo = destino

        let escritor = try AVAssetWriter(outputURL: destino, fileType: .mp4)
        self.escritor = escritor

        // --- Vídeo ---
        let kbps = ajustes.kbpsVideo(capacidades)
        let fps = Int(ajustes.fps)
        var compresion: [String: Any] = [
            AVVideoAverageBitRateKey: kbps * 1000,
            AVVideoExpectedSourceFrameRateKey: fps,
            // Un fotograma clave cada dos segundos: buscar dentro del vídeo sin
            // que el archivo engorde.
            AVVideoMaxKeyFrameIntervalDurationKey: 2,
            // Sin reordenar fotogramas el codificador no necesita mirar hacia
            // delante, que es latencia y memoria a cambio de poco.
            AVVideoAllowFrameReorderingKey: false,
        ]
        if ajustes.codecElegido == .h264 {
            compresion[AVVideoProfileLevelKey] = AVVideoProfileLevelH264HighAutoLevel
        }
        let ajustesVideo: [String: Any] = [
            AVVideoCodecKey: ajustes.codecElegido == .hevc
                ? AVVideoCodecType.hevc : AVVideoCodecType.h264,
            AVVideoWidthKey: Int(tamano.width),
            AVVideoHeightKey: Int(tamano.height),
            AVVideoCompressionPropertiesKey: compresion,
        ]
        let video = AVAssetWriterInput(mediaType: .video, outputSettings: ajustesVideo)
        video.expectsMediaDataInRealTime = true
        guard escritor.canAdd(video) else {
            throw Fallo.pistaNoDisponible("vídeo")
        }
        escritor.add(video)
        entradaVideo = video

        adaptador = AVAssetWriterInputPixelBufferAdaptor(
            assetWriterInput: video,
            sourcePixelBufferAttributes: [
                kCVPixelBufferPixelFormatTypeKey as String: kCVPixelFormatType_32BGRA,
                kCVPixelBufferWidthKey as String: Int(tamano.width),
                kCVPixelBufferHeightKey as String: Int(tamano.height),
                kCVPixelBufferMetalCompatibilityKey as String: true,
            ])

        // --- Audio ---
        //
        // El micrófono y el audio de la app van en dos pistas separadas. No se
        // mezclan porque mezclarlas de verdad pide remuestrear y alinear dos
        // relojes distintos, y hacerlo mal suena peor que tenerlas aparte.
        let kbpsAudio = ajustes.kbpsAudio(capacidades)
        if kbpsAudio > 0 {
            let ajustesAudio: [String: Any] = [
                AVFormatIDKey: kAudioFormatMPEG4AAC,
                AVSampleRateKey: Double(ajustes.frecuencia_hz),
                AVNumberOfChannelsKey: ajustes.canalesDeAudio,
                AVEncoderBitRateKey: kbpsAudio * 1000,
            ]
            if ajustes.microfono {
                let micro = AVAssetWriterInput(mediaType: .audio, outputSettings: ajustesAudio)
                micro.expectsMediaDataInRealTime = true
                guard escritor.canAdd(micro) else {
                    throw Fallo.pistaNoDisponible("micrófono")
                }
                escritor.add(micro)
                entradaMicro = micro
            }
            if ajustes.audio_de_la_app {
                let app = AVAssetWriterInput(mediaType: .audio, outputSettings: ajustesAudio)
                app.expectsMediaDataInRealTime = true
                guard escritor.canAdd(app) else {
                    throw Fallo.pistaNoDisponible("audio de la app")
                }
                escritor.add(app)
                entradaApp = app
            }
        }

        guard escritor.startWriting() else {
            throw escritor.error ?? Fallo.sinEscritor
        }
        sesionAbierta = false
        desfase = .zero
        pausadoEn = nil
        ultimoVideo = .invalid
        ultimoPTSOrigen = .invalid
        enPausa = false
        falloCaptura = nil
        falloNotificado = false
        videosEscritos = 0
        audiosMicroEscritos = 0
        audiosAppEscritos = 0
        descartesPorPresion = 0
        muestrasRecibidas = 0
        puertoEntradaActivoUID = nil
        pausadaPorInterrupcion = false
        aceptandoEventos = true
        return destino
    }

    // MARK: - Recibir buffers

    private func recibir(_ buffer: CMSampleBuffer, tipo: RPSampleBufferType) {
        guard let escritor, escritor.status == .writing else { return }
        guard CMSampleBufferDataIsReady(buffer) else { return }

        let pts = CMSampleBufferGetPresentationTimeStamp(buffer)
        if tipo == .video { ultimoPTSOrigen = pts }

        if !sesionAbierta {
            guard tipo == .video else { return }  // la sesión la abre el vídeo
            escritor.startSession(atSourceTime: pts)
            sesionAbierta = true
        }

        // En pausa se tira todo, y al reanudar se descuenta lo que duró.
        if enPausa {
            if pausadoEn == nil { pausadoEn = pts }
            return
        }
        if let inicioPausa = pausadoEn {
            let intervalo = CMTimeSubtract(pts, inicioPausa)
            if intervalo.isValid, CMTimeCompare(intervalo, .zero) > 0 {
                desfase = CMTimeAdd(desfase, intervalo)
            }
            pausadoEn = nil
        }

        let ajustado = CMTimeSubtract(pts, desfase)

        switch tipo {
        case .video:
            escribirVideo(buffer, en: ajustado)
        case .audioMic:
            escribirAudio(buffer, en: ajustado, entrada: entradaMicro)
        case .audioApp:
            escribirAudio(buffer, en: ajustado, entrada: entradaApp)
        @unknown default:
            break
        }
    }

    private func escribirVideo(_ buffer: CMSampleBuffer, en tiempo: CMTime) {
        guard let entrada = entradaVideo, let adaptador else {
            registrarFallo(Fallo.pistaNoDisponible("vídeo"))
            return
        }
        guard let origen = CMSampleBufferGetImageBuffer(buffer) else {
            registrarFallo(Fallo.escrituraFallida("fotograma sin imagen"))
            return
        }

        // ReplayKit entrega a la frecuencia del panel — 120 Hz en ProMotion —,
        // así que grabar a 30 FPS es tirar tres de cada cuatro fotogramas. Se
        // decide aquí y no en el codificador: lo que no se codifica no cuesta
        // ni batería ni bitrate.
        let intervalo = CMTime(value: 1, timescale: CMTimeScale(max(1, ajustes.fps)))
        if ultimoVideo.isValid,
           CMTimeSubtract(tiempo, ultimoVideo) < CMTimeMultiplyByFloat64(intervalo, multiplier: 0.98) {
            return
        }
        ultimoVideo = tiempo
        muestrasRecibidas += 1
        guard entrada.isReadyForMoreMediaData else {
            descartesPorPresion += 1
            return
        }

        let ancho = CVPixelBufferGetWidth(origen)
        let alto = CVPixelBufferGetHeight(origen)

        if ancho == Int(tamano.width) && alto == Int(tamano.height) {
            if adaptador.append(origen, withPresentationTime: tiempo) {
                videosEscritos += 1
            } else {
                registrarFallo(escritor?.error
                    ?? Fallo.escrituraFallida("no se pudo anexar un fotograma"))
            }
            return
        }

        // Reescalar a la resolución elegida. Se hace con Core Image sobre la GPU
        // porque hacerlo en CPU a 120 Hz no cabe en el presupuesto de un frame.
        guard let pool = adaptador.pixelBufferPool else {
            registrarFallo(Fallo.escrituraFallida("no existe un pool de píxeles"))
            return
        }
        var destino: CVPixelBuffer?
        guard CVPixelBufferPoolCreatePixelBuffer(nil, pool, &destino) == kCVReturnSuccess,
              let destino else {
            registrarFallo(Fallo.escrituraFallida("no hay memoria para reescalar vídeo"))
            return
        }

        let imagen = CIImage(cvPixelBuffer: origen)
        let escala = min(tamano.width / CGFloat(ancho), tamano.height / CGFloat(alto))
        let anchoEscalado = CGFloat(ancho) * escala
        let altoEscalado = CGFloat(alto) * escala
        let traslacion = CGAffineTransform(
            translationX: (tamano.width - anchoEscalado) / 2,
            y: (tamano.height - altoEscalado) / 2)
        let escalada = imagen.transformed(by: CGAffineTransform(
            scaleX: escala, y: escala).concatenating(traslacion))
        let marco = CGRect(origin: .zero, size: tamano)
        let fondo = CIImage(color: CIColor.black).cropped(to: marco)
        contexto.render(escalada.composited(over: fondo), to: destino,
                        bounds: marco, colorSpace: CGColorSpaceCreateDeviceRGB())
        if adaptador.append(destino, withPresentationTime: tiempo) {
            videosEscritos += 1
        } else {
            registrarFallo(escritor?.error
                ?? Fallo.escrituraFallida("no se pudo anexar el vídeo reescalado"))
        }
    }

    private func escribirAudio(_ buffer: CMSampleBuffer, en tiempo: CMTime,
                               entrada: AVAssetWriterInput?) {
        guard let entrada else { return }
        muestrasRecibidas += 1
        guard entrada.isReadyForMoreMediaData else {
            descartesPorPresion += 1
            return
        }
        guard let corregido = conNuevaMarca(buffer, tiempo: tiempo) else {
            registrarFallo(Fallo.escrituraFallida("marca de tiempo de audio inválida"))
            return
        }
        guard entrada.append(corregido) else {
            registrarFallo(escritor?.error
                ?? Fallo.escrituraFallida("no se pudo anexar una muestra de audio"))
            return
        }
        if entrada === entradaMicro { audiosMicroEscritos += 1 }
        if entrada === entradaApp { audiosAppEscritos += 1 }
    }

    /// Copia el buffer cambiándole la marca de tiempo. Hace falta para descontar
    /// las pausas también en el audio: si solo se corrige el vídeo, la voz se
    /// desincroniza justo por lo que duró la pausa.
    private func conNuevaMarca(_ buffer: CMSampleBuffer, tiempo: CMTime) -> CMSampleBuffer? {
        var info = CMSampleTimingInfo(
            duration: CMSampleBufferGetDuration(buffer),
            presentationTimeStamp: tiempo,
            decodeTimeStamp: .invalid)
        var salida: CMSampleBuffer?
        let estado = CMSampleBufferCreateCopyWithNewTiming(
            allocator: kCFAllocatorDefault,
            sampleBuffer: buffer,
            sampleTimingEntryCount: 1,
            sampleTimingArray: &info,
            sampleBufferOut: &salida)
        return estado == noErr ? salida : nil
    }

    // MARK: - Limpieza

    private func limpiar() {
        escritor = nil
        entradaVideo = nil
        entradaMicro = nil
        entradaApp = nil
        adaptador = nil
        sesionAbierta = false
        pausadoEn = nil
        ultimoVideo = .invalid
        ultimoPTSOrigen = .invalid
        desfase = .zero
        enPausa = false
        deteniendo = false
        falloCaptura = nil
        falloNotificado = false
        videosEscritos = 0
        audiosMicroEscritos = 0
        audiosAppEscritos = 0
        descartesPorPresion = 0
        muestrasRecibidas = 0
        puertoEntradaActivoUID = nil
        pausadaPorInterrupcion = false
        aceptandoEventos = false
    }

    /// Solo se usa antes de que ReplayKit confirme el arranque. En ese punto el
    /// archivo todavía no representa trabajo del usuario y dejarlo en la
    /// biblioteca produciría un MP4 corrupto que aparenta ser una grabación.
    private func cancelarYEliminarArchivoParcial() {
        escritor?.cancelWriting()
        let parcial = archivo
        limpiar()
        archivo = nil
        if let parcial { try? FileManager.default.removeItem(at: parcial) }
    }

    private func advertenciaDePresion() -> Error? {
        guard muestrasRecibidas > 0 else { return nil }
        // Un pico aislado es normal en tiempo real. Se avisa cuando se perdió
        // al menos 1 % del flujo o diez muestras, lo que ocurra más tarde.
        let umbral = max(10, muestrasRecibidas / 100)
        guard descartesPorPresion >= umbral else { return nil }
        return Fallo.muestrasDescartadas(descartesPorPresion, muestrasRecibidas)
    }

    private func registrarFallo(_ error: Error) {
        guard aceptandoEventos else { return }
        if falloCaptura == nil { falloCaptura = error }
        guard !falloNotificado else { return }
        falloNotificado = true
        DispatchQueue.main.async { [weak self] in self?.alFallar?(error) }
    }

    private func manejarInterrupcionAudio(_ notificacion: Notification) {
        guard ajustes.microfono,
              let valor = notificacion.userInfo?[AVAudioSessionInterruptionTypeKey]
                as? UInt,
              let tipo = AVAudioSession.InterruptionType(rawValue: valor)
        else { return }
        switch tipo {
        case .began:
            guard estado == .grabando else { return }
            pausar()
            pausadaPorInterrupcion = true
        case .ended:
            guard estado == .pausada, pausadaPorInterrupcion else { return }
            pausadaPorInterrupcion = false
            let valorOpciones = notificacion.userInfo?[AVAudioSessionInterruptionOptionKey]
                as? UInt ?? 0
            let opciones = AVAudioSession.InterruptionOptions(rawValue: valorOpciones)
            if opciones.contains(.shouldResume) {
                reanudar()
            } else {
                cola.async { [weak self] in
                    self?.registrarFallo(Fallo.interrupcionNoRecuperable)
                }
            }
        @unknown default:
            cola.async { [weak self] in
                self?.registrarFallo(Fallo.interrupcionNoRecuperable)
            }
        }
    }

    private func manejarCambioRutaAudio(_ notificacion: Notification) {
        guard ajustes.microfono, estado != .detenida,
              let valor = notificacion.userInfo?[AVAudioSessionRouteChangeReasonKey]
                as? UInt,
              AVAudioSession.RouteChangeReason(rawValue: valor) == .oldDeviceUnavailable
        else { return }
        let actual = AVAudioSession.sharedInstance().currentRoute.inputs.first?.uid
        guard actual != puertoEntradaActivoUID else { return }
        if estado == .grabando { pausar() }
        cola.async { [weak self] in
            self?.registrarFallo(Fallo.microfonoDesconectado)
        }
    }

    private func manejarReinicioServiciosAudio() {
        guard ajustes.microfono, estado != .detenida else { return }
        if estado == .grabando { pausar() }
        cola.async { [weak self] in
            self?.registrarFallo(Fallo.serviciosDeAudioReiniciados)
        }
    }

    private func cambiar(a nuevo: EstadoGrabacion) {
        estado = nuevo
        alCambiarEstado?(nuevo)
    }
}
