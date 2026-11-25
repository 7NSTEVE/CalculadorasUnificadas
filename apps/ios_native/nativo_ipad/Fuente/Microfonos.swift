import AVFoundation
import UIKit

/// Identidad persistible de una entrada. La posición no alcanza: dos
/// interfaces USB, AirPods y el micrófono integrado pueden aparecer todos como
/// «automático». El UID del puerto más el ID de data source sí distinguen la
/// fuente exacta que eligió la persona.
struct SeleccionMicrofono: Codable, Equatable {
    let puertoUID: String
    let fuenteID: Int?
    let nombre: String
}

enum ErrorPermisoMicrofono: LocalizedError {
    case denegado

    var errorDescription: String? {
        "No se concedió acceso al micrófono. Puede habilitarlo en Ajustes > Privacidad y seguridad > Micrófono."
    }
}

enum ErrorSeleccionMicrofono: LocalizedError {
    case noDisponible(String)
    case sinEntrada

    var errorDescription: String? {
        switch self {
        case .noDisponible(let nombre):
            return "El micrófono «\(nombre)» ya no está conectado. Elija otra entrada o use Automático."
        case .sinEntrada:
            return "iPadOS no expuso ninguna entrada de micrófono disponible."
        }
    }
}

/// Elegir micrófono en un iPad, de verdad.
///
/// Los 4–5 micrófonos de un iPad Pro o de un iPhone **no se exponen por
/// separado**: iOS no deja grabar cada cápsula por su cuenta y no hay API
/// pública que lo haga. Lo que sí da `AVAudioSession` es:
///
/// 1. elegir la **fuente** por posición — superior, inferior, frontal, trasera;
/// 2. y encima un **patrón polar** — cardioide, subcardioide, omnidireccional o
///    estéreo — que el sistema construye combinando varias cápsulas.
///
/// Es decir: un micrófono virtual apuntado a donde uno quiera. Eso es control
/// real de la captación, y es lo que hay. Esta pantalla no promete cinco pistas
/// independientes porque no existen.
enum Microfonos {

    /// Solicita el permiso antes de configurar rutas o crear archivos. Las
    /// grabadoras no deben convertir una negativa del sistema en un M4A/MP4
    /// vacío que luego aparezca como una grabación válida.
    static func solicitarPermiso(
        _ terminar: @escaping (Result<Void, Error>) -> Void
    ) {
        let responder: (Bool) -> Void = { concedido in
            DispatchQueue.main.async {
                terminar(concedido
                    ? .success(())
                    : .failure(ErrorPermisoMicrofono.denegado))
            }
        }
        if #available(iOS 17.0, *) {
            AVAudioApplication.requestRecordPermission(completionHandler: responder)
        } else {
            AVAudioSession.sharedInstance().requestRecordPermission(responder)
        }
    }

    struct Fuente {
        let puerto: AVAudioSessionPortDescription
        let dataSource: AVAudioSessionDataSourceDescription?
        let nombre: String
        let posicion: PosicionMicrofono
        let patrones: [PatronPolar]

        var seleccion: SeleccionMicrofono {
            SeleccionMicrofono(puertoUID: puerto.uid,
                               fuenteID: dataSource?.dataSourceID.intValue,
                               nombre: nombre)
        }

        var descripcion: String {
            let lista = patrones.filter { $0 != .automatico }
                .map(\.nombre).joined(separator: ", ")
            return lista.isEmpty ? nombre : "\(nombre) — \(lista)"
        }
    }

    /// Todo lo que se puede elegir ahora mismo, incluidos micrófonos externos
    /// enchufados por USB-C o Bluetooth.
    static func disponibles() -> [Fuente] {
        let sesion = AVAudioSession.sharedInstance()
        let categoriaOriginal = sesion.category
        let modoOriginal = sesion.mode
        let opcionesOriginales = sesion.categoryOptions
        let yaAdmiteEntrada = categoriaOriginal == .record
            || categoriaOriginal == .playAndRecord
            || categoriaOriginal == .multiRoute
        if !yaAdmiteEntrada {
            try? sesion.setCategory(.playAndRecord, mode: .videoRecording,
                                    options: [.mixWithOthers, .defaultToSpeaker,
                                              .allowBluetoothHFP, .allowBluetoothA2DP])
        }
        defer {
            if !yaAdmiteEntrada {
                try? sesion.setCategory(categoriaOriginal, mode: modoOriginal,
                                        options: opcionesOriginales)
            }
        }
        var salida: [Fuente] = []

        for puerto in sesion.availableInputs ?? [] {
            let fuentes = puerto.dataSources ?? []
            if fuentes.isEmpty {
                // Un micrófono externo suele venir sin data sources: es una sola
                // cápsula y no hay nada que elegir dentro de él.
                salida.append(Fuente(puerto: puerto, dataSource: nil,
                                     nombre: puerto.portName,
                                     posicion: .automatica, patrones: [.automatico]))
                continue
            }
            for fuente in fuentes {
                salida.append(Fuente(puerto: puerto,
                                     dataSource: fuente,
                                     nombre: fuente.dataSourceName,
                                     posicion: posicion(de: fuente),
                                     patrones: patrones(de: fuente)))
            }
        }
        return salida
    }

    /// Resuelve primero la identidad exacta persistida. Si ese accesorio ya no
    /// está conectado, cae a la posición solicitada y por último a la entrada
    /// que iPadOS ofrezca como primera opción.
    static func fuente(seleccion: SeleccionMicrofono?,
                       posicion: PosicionMicrofono) -> Fuente? {
        let todas = disponibles()
        if let seleccion,
           let exacta = todas.first(where: { $0.seleccion.puertoUID == seleccion.puertoUID
               && $0.seleccion.fuenteID == seleccion.fuenteID }) {
            return exacta
        }
        if posicion == .automatica { return todas.first }
        return todas.first { $0.posicion == posicion } ?? todas.first
    }

    static func fuenteExacta(_ seleccion: SeleccionMicrofono) -> Fuente? {
        disponibles().first {
            $0.seleccion.puertoUID == seleccion.puertoUID
                && $0.seleccion.fuenteID == seleccion.fuenteID
        }
    }

    /// ¿Este aparato graba en estéreo? Desde iOS 14 lo hace el hardware
    /// compatible, y se reconoce porque alguna fuente admite el patrón estéreo.
    static func admiteEstereo() -> Bool {
        disponibles().contains { $0.patrones.contains(.estereo) }
    }

    /// Máximo que reporta la ruta de entrada actual. Se consulta después de
    /// preparar una categoría de grabación porque antes iPadOS puede devolver
    /// cero aunque exista micrófono.
    static func maximoCanalesDeEntrada() -> Int {
        let sesion = AVAudioSession.sharedInstance()
        let categoriaOriginal = sesion.category
        let modoOriginal = sesion.mode
        let opcionesOriginales = sesion.categoryOptions
        do {
            try sesion.setCategory(.playAndRecord, mode: .videoRecording,
                                   options: [.mixWithOthers, .defaultToSpeaker,
                                             .allowBluetoothHFP, .allowBluetoothA2DP])
            try sesion.setActive(true)
            let maximo = max(1, sesion.maximumInputNumberOfChannels)
            try? sesion.setActive(false, options: [.notifyOthersOnDeactivation])
            try? sesion.setCategory(categoriaOriginal, mode: modoOriginal,
                                    options: opcionesOriginales)
            return maximo
        } catch {
            try? sesion.setActive(false, options: [.notifyOthersOnDeactivation])
            try? sesion.setCategory(categoriaOriginal, mode: modoOriginal,
                                    options: opcionesOriginales)
            return 1
        }
    }

    /// Cuatro canales no significan por sí solos sonido 3D: una interfaz USB
    /// puede dar cuatro canales planos sin metadatos ambisónicos. Se consulta
    /// el modo FOA del `AVCaptureDeviceInput`, que es la capacidad pública y
    /// real que usa la grabadora QuickTime espacial.
    static func admiteAudioEspacialReal() -> Bool {
        GrabadoraAudioEspacial.disponibleEnEsteIPad()
    }

    /// El modo FOA de iPadOS solo actúa sobre el arreglo integrado. En
    /// automático se elegirá una fuente integrada; una selección explícita USB,
    /// Bluetooth o de auriculares se rechaza en lugar de rotularla como 3D.
    static func seleccionPermiteAudioEspacial(_ seleccion: SeleccionMicrofono?) -> Bool {
        if let seleccion {
            return disponibles().first {
                $0.seleccion.puertoUID == seleccion.puertoUID
                    && $0.seleccion.fuenteID == seleccion.fuenteID
            }?.puerto.portType == .builtInMic
        }
        return disponibles().contains { $0.puerto.portType == .builtInMic }
    }

    /// Prepara la sesión de audio y deja el micrófono apuntado donde se pidió.
    ///
    /// Devuelve el patrón que quedó puesto de verdad, que no tiene por qué ser
    /// el pedido: si el aparato no lo admite, el sistema se queda con el suyo y
    /// hay que enseñarlo en pantalla en vez de mentir.
    @discardableResult
    static func preparar(ajustes: TintaAjustesGrabacion,
                         seleccion: SeleccionMicrofono? = nil) throws -> PatronPolar {
        let sesion = AVAudioSession.sharedInstance()

        // `.playAndRecord` con `.mixWithOthers` para que grabar no corte lo que
        // esté sonando, y `.videoRecording` porque es el modo que aplica el
        // procesado pensado para grabar, no para llamadas.
        try sesion.setCategory(.playAndRecord,
                               mode: .videoRecording,
                               options: [.mixWithOthers, .defaultToSpeaker,
                                         .allowBluetoothHFP, .allowBluetoothA2DP])
        try sesion.setPreferredSampleRate(Double(ajustes.frecuencia_hz))
        try sesion.setActive(true)

        let elegida: Fuente?
        if let seleccion {
            guard let exacta = fuenteExacta(seleccion) else {
                throw ErrorSeleccionMicrofono.noDisponible(seleccion.nombre)
            }
            elegida = exacta
        } else if ajustes.canalesElegidos == .espacial {
            elegida = fuenteIntegrada(seleccion: seleccion,
                                      posicion: ajustes.posicionElegida)
        } else {
            elegida = fuente(seleccion: seleccion,
                             posicion: ajustes.posicionElegida)
        }
        guard ajustes.microfono else { return .automatico }
        guard let elegida else { throw ErrorSeleccionMicrofono.sinEntrada }
        if ajustes.canalesElegidos == .espacial,
           elegida.puerto.portType != .builtInMic {
            throw GrabadoraAudioEspacial.Fallo.requiereMicrofonoIntegrado
        }

        try sesion.setPreferredInput(elegida.puerto)
        if ajustes.canalesElegidos == .espacial {
            if let dataSource = elegida.dataSource {
                try elegida.puerto.setPreferredDataSource(dataSource)
            }
            try sesion.setPreferredInputOrientation(orientacionDeEntrada())
            return .automatico
        }
        if let dataSource = elegida.dataSource {
            try elegida.puerto.setPreferredDataSource(dataSource)

            let pedido = patronDeseado(ajustes: ajustes, fuente: elegida)
            if let patron = patronDeSistema(pedido),
               dataSource.supportedPolarPatterns?.contains(patron) == true {
                try dataSource.setPreferredPolarPattern(patron)

                // El estéreo necesita saber cómo está agarrado el iPad: sin
                // esto los canales salen cambiados al girar la pantalla.
                if patron == .stereo {
                    try sesion.setPreferredInputOrientation(orientacionDeEntrada())
                }
                tryPreferirCanales(ajustes.canalesDeAudio, en: sesion)
                return pedido
            }
        }
        tryPreferirCanales(ajustes.canalesDeAudio, en: sesion)
        return .automatico
    }

    static func liberar() {
        try? AVAudioSession.sharedInstance().setActive(
            false, options: [.notifyOthersOnDeactivation])
    }

    // MARK: - Traducción

    private static func fuenteIntegrada(seleccion: SeleccionMicrofono?,
                                        posicion: PosicionMicrofono) -> Fuente? {
        let integradas = disponibles().filter { $0.puerto.portType == .builtInMic }
        if let seleccion {
            return integradas.first {
                $0.seleccion.puertoUID == seleccion.puertoUID
                    && $0.seleccion.fuenteID == seleccion.fuenteID
            }
        }
        if posicion != .automatica,
           let coincidencia = integradas.first(where: { $0.posicion == posicion }) {
            return coincidencia
        }
        return integradas.first
    }

    private static func patronDeseado(ajustes: TintaAjustesGrabacion,
                                      fuente: Fuente) -> PatronPolar {
        if ajustes.patronElegido != .automatico { return ajustes.patronElegido }
        // En automático manda lo que se pidió del audio: si es estéreo y el
        // aparato lo tiene, estéreo; si no, cardioide, que es lo que mejor coge
        // al profesor y peor coge la sala.
        if ajustes.canalesElegidos != .mono, fuente.patrones.contains(.estereo) {
            return .estereo
        }
        return fuente.patrones.contains(.cardioide) ? .cardioide : .automatico
    }

    private static func tryPreferirCanales(_ pedidos: Int, en sesion: AVAudioSession) {
        let maximo = sesion.maximumInputNumberOfChannels
        guard maximo > 0 else { return }
        try? sesion.setPreferredInputNumberOfChannels(min(max(1, pedidos), maximo))
    }

    private static func patronDeSistema(_ patron: PatronPolar) -> AVAudioSession.PolarPattern? {
        switch patron {
        case .automatico: return nil
        case .omnidireccional: return .omnidirectional
        case .cardioide: return .cardioid
        case .subcardioide: return .subcardioid
        case .estereo: return .stereo
        }
    }

    private static func patrones(de fuente: AVAudioSessionDataSourceDescription) -> [PatronPolar] {
        var salida: [PatronPolar] = [.automatico]
        for patron in fuente.supportedPolarPatterns ?? [] {
            switch patron {
            case .omnidirectional: salida.append(.omnidireccional)
            case .cardioid: salida.append(.cardioide)
            case .subcardioid: salida.append(.subcardioide)
            case .stereo: salida.append(.estereo)
            default: break
            }
        }
        return salida
    }

    private static func posicion(de fuente: AVAudioSessionDataSourceDescription) -> PosicionMicrofono {
        // La posición sale de dos datos que el sistema da por separado: en qué
        // extremo del aparato está la cápsula, y hacia qué cara mira.
        switch fuente.orientation {
        case .some(.front): return .frontal
        case .some(.back): return .trasera
        default: break
        }
        switch fuente.location {
        case .some(.upper): return .superior
        case .some(.lower): return .inferior
        default: return .automatica
        }
    }

    private static func orientacionDeEntrada() -> AVAudioSession.StereoOrientation {
        switch UIDevice.current.orientation {
        case .landscapeLeft: return .landscapeLeft
        case .landscapeRight: return .landscapeRight
        case .portraitUpsideDown: return .portraitUpsideDown
        default: return .portrait
        }
    }
}
