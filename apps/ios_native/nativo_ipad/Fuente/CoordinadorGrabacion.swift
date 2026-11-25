import Foundation
import CoreGraphics

/// Presenta una sola máquina de estados a la interfaz y enruta a tres
/// implementaciones distintas. De este modo «Solo audio» nunca crea un MP4 y
/// «Nota Viva» nunca rasteriza la hoja para fingir que es un vídeo.
final class CoordinadorGrabacion {

    enum Fallo: LocalizedError {
        case sinArchivo
        case espacialNoDisponibleEnPantalla
        case preparacionEnCurso
        case grabacionEnCurso

        var errorDescription: String? {
            switch self {
            case .sinArchivo:
                return "La grabación terminó sin producir un archivo."
            case .espacialNoDisponibleEnPantalla:
                return "El vídeo de pantalla usa ReplayKit, que no entrega audio FOA. Elija mono o estéreo."
            case .preparacionEnCurso:
                return "La grabación ya se está preparando. Espere a que termine la solicitud de permiso."
            case .grabacionEnCurso:
                return "Ya hay una grabación en curso. Deténgala antes de iniciar otra."
            }
        }
    }

    private let pantalla = Grabadora()
    private let audio = GrabadoraAudio()
    private let notaViva = GrabadoraNotaViva()
    /// Sidecar de recovery: no captura audio y nunca sustituye a los tres
    /// backends anteriores. Un fallo aquí sólo genera advertencia.
    private let audioResiliente = IntegradorAudioResilienteGrabacionIPad()
    private var modo: ModoCaptura = .notaViva
    private var ajustes = TintaAjustesGrabacion.porDefecto
    private var capacidades = TintaCapacidades()
    private var preparando = false

    private(set) var estado: EstadoGrabacion = .detenida
    private(set) var archivo: URL?
    private(set) var patronReal: PatronPolar = .automatico
    var alCambiarEstado: ((EstadoGrabacion) -> Void)?
    var alFallar: ((Error) -> Void)?
    var alAdvertir: ((Error) -> Void)?

    init() {
        pantalla.alCambiarEstado = { [weak self] estado in self?.cambiar(a: estado) }
        audio.alCambiarEstado = { [weak self] estado in self?.cambiar(a: estado) }
        notaViva.alCambiarEstado = { [weak self] estado in self?.cambiar(a: estado) }
        pantalla.alFallar = { [weak self] error in self?.alFallar?(error) }
        pantalla.alAdvertir = { [weak self] error in self?.alAdvertir?(error) }
        audio.alFallar = { [weak self] error in self?.alFallar?(error) }
        notaViva.alFallar = { [weak self] error in self?.alFallar?(error) }
        notaViva.alAdvertir = { [weak self] error in self?.alAdvertir?(error) }
    }

    var segundosGrabados: Double {
        switch modo {
        case .soloAudio: return audio.segundosGrabados
        case .pantallaConAudio: return pantalla.segundosGrabados
        case .notaViva: return notaViva.segundosGrabados
        }
    }

    func megabytesEstimados(_ cap: TintaCapacidades) -> Double {
        if ajustes.canalesElegidos == .espacial {
            let bytes: Int64
            switch modo {
            case .soloAudio: bytes = audio.bytesGrabados
            case .notaViva: bytes = notaViva.bytesAudioGrabados
            case .pantallaConAudio: bytes = 0
            }
            return Double(bytes) / 1_000_000.0
        }
        return segundosGrabados / 60.0 * ajustes.megabytesPorMinuto(cap)
    }

    func comenzar(ajustes pedidos: TintaAjustesGrabacion,
                  capacidades cap: TintaCapacidades,
                  seleccionMicrofono: SeleccionMicrofono?,
                  carpeta: URL,
                  sesion: Sesion,
                  completado: @escaping (Result<Void, Error>) -> Void) {
        guard !preparando else {
            completado(.failure(Fallo.preparacionEnCurso))
            return
        }
        guard estado == .detenida else {
            completado(.failure(Fallo.grabacionEnCurso))
            return
        }
        ajustes = pedidos.ajustado(a: cap)
        capacidades = cap
        modo = ajustes.modoElegido
        guard modo != .pantallaConAudio || ajustes.canalesElegidos != .espacial else {
            completado(.failure(Fallo.espacialNoDisponibleEnPantalla))
            return
        }

        let recibir: (Result<Void, Error>) -> Void = { [weak self] resultado in
            guard let self else { return }
            self.preparando = false
            switch self.modo {
            case .soloAudio:
                self.archivo = self.audio.archivo
                self.patronReal = self.audio.patronReal
            case .pantallaConAudio:
                self.archivo = self.pantalla.archivo
                self.patronReal = self.pantalla.patronReal
            case .notaViva:
                self.archivo = self.notaViva.archivo
                self.patronReal = self.notaViva.patronReal
            }

            switch resultado {
            case .success:
                let fuente: URL?
                switch self.modo {
                case .soloAudio: fuente = self.audio.archivo
                case .notaViva: fuente = self.notaViva.archivo
                case .pantallaConAudio: fuente = nil
                }
                if let advertencia = self.audioResiliente.vincularFuente(
                    fuente, modo: self.modo) {
                    self.alAdvertir?(advertencia)
                }
            case .failure(let error):
                self.audioResiliente.cancelarPreparacion(
                    motivo: error.localizedDescription)
            }
            completado(resultado)
        }

        let iniciar = { [weak self] in
            guard let self else { return }
            if let advertencia = self.audioResiliente.preparar(
                documentoId: sesion.documentoPersistenteId,
                modo: self.modo,
                microfono: self.ajustes.microfono) {
                self.alAdvertir?(advertencia)
            }
            switch self.modo {
            case .soloAudio:
                self.audio.comenzar(ajustes: self.ajustes, capacidades: cap,
                                    seleccionMicrofono: seleccionMicrofono,
                                    carpeta: carpeta, completado: recibir)
            case .pantallaConAudio:
                self.pantalla.comenzar(ajustes: self.ajustes, capacidades: cap,
                                       seleccionMicrofono: seleccionMicrofono,
                                       carpeta: carpeta, completado: recibir)
            case .notaViva:
                self.notaViva.comenzar(ajustes: self.ajustes, capacidades: cap,
                                       seleccionMicrofono: seleccionMicrofono,
                                       carpeta: carpeta, sesion: sesion,
                                       completado: recibir)
            }
        }

        preparando = true
        if ajustes.microfono {
            Microfonos.solicitarPermiso { [weak self] resultado in
                guard let self else { return }
                switch resultado {
                case .success:
                    iniciar()
                case .failure(let error):
                    self.preparando = false
                    completado(.failure(error))
                }
            }
        } else {
            iniciar()
        }
    }

    func pausar() {
        switch modo {
        case .soloAudio: audio.pausar()
        case .pantallaConAudio: pantalla.pausar()
        case .notaViva: notaViva.pausar()
        }
        guard modo != .pantallaConAudio, estado == .pausada else { return }
        if let advertencia = audioResiliente.pausar() {
            alAdvertir?(advertencia)
        }
    }

    func reanudar() {
        switch modo {
        case .soloAudio: audio.reanudar()
        case .pantallaConAudio: pantalla.reanudar()
        case .notaViva: notaViva.reanudar()
        }
        guard modo != .pantallaConAudio, estado == .grabando else { return }
        if let advertencia = audioResiliente.reanudar() {
            alAdvertir?(advertencia)
        }
    }

    func marcar(_ tipo: TipoEventoNotaViva, titulo: String? = nil,
                pagina: Int? = nil, punto: CGPoint? = nil,
                recursoId: String? = nil, diapositivaId: UUID? = nil) {
        guard modo == .notaViva else { return }
        notaViva.marcar(tipo, titulo: titulo, pagina: pagina, punto: punto,
                        recursoId: recursoId, diapositivaId: diapositivaId)
    }

    func detener(completado: @escaping (Result<URL, Error>) -> Void) {
        // Se captura antes del backend porque su callback puede haber limpiado
        // reloj/estado interno cuando intentemos confirmar el sidecar.
        let duracionFinal = segundosGrabados
        let ajustesFinales = ajustes
        let modoFinal = modo

        switch modoFinal {
        case .soloAudio:
            audio.detener { [weak self] resultado in
                guard let self else {
                    switch resultado {
                    case .success(let url):
                        if let url { completado(.success(url)) }
                        else { completado(.failure(Fallo.sinArchivo)) }
                    case .failure(let error): completado(.failure(error))
                    }
                    return
                }
                switch resultado {
                case .success(let url):
                    guard let url else {
                        let error = Fallo.sinArchivo
                        self.audioResiliente.registrarFalloFinal(error)
                        completado(.failure(error))
                        return
                    }
                    if let advertencia = self.audioResiliente.confirmarFinal(
                        resultadoPrincipal: url,
                        modo: modoFinal,
                        duracion: duracionFinal,
                        ajustes: ajustesFinales) {
                        self.alAdvertir?(advertencia)
                    }
                    completado(.success(url))
                case .failure(let error):
                    self.audioResiliente.registrarFalloFinal(error)
                    completado(.failure(error))
                }
            }
        case .pantallaConAudio:
            pantalla.detener(completado: completado)
        case .notaViva:
            notaViva.detener { [weak self] resultado in
                guard let self else {
                    completado(resultado)
                    return
                }
                switch resultado {
                case .success(let manifiestoURL):
                    if let advertencia = self.audioResiliente.confirmarFinal(
                        resultadoPrincipal: manifiestoURL,
                        modo: modoFinal,
                        duracion: duracionFinal,
                        ajustes: ajustesFinales) {
                        self.alAdvertir?(advertencia)
                    }
                    completado(.success(manifiestoURL))
                case .failure(let error):
                    self.audioResiliente.registrarFalloFinal(error)
                    completado(.failure(error))
                }
            }
        }
    }

    private func cambiar(a nuevo: EstadoGrabacion) {
        estado = nuevo
        alCambiarEstado?(nuevo)
    }
}
