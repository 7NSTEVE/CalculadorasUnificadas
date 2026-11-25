import Compression
import CryptoKit
import LocalAuthentication
import Security
import UIKit

enum NivelPrivacidadCuaderno: String, Codable, CaseIterable {
    case normal = "Normal"
    case privado = "Privado y cifrado"
    case protegido = "Cifrado y autenticación al abrir"
}

enum CompresionCuaderno: String, Codable, CaseIterable {
    case ninguna = "Sin compresión"
    case equilibrada = "LZFSE equilibrada"
    case maxima = "LZMA máxima"

    var algoritmo: compression_algorithm? {
        switch self {
        case .ninguna: return nil
        case .equilibrada: return COMPRESSION_LZFSE
        case .maxima: return COMPRESSION_LZMA
        }
    }
}

/// Presupuesto defensivo del journal. Los adjuntos se referencian por hash y no
/// se incrustan aquí, por lo que 128 MiB cubren un documento de tinta muy grande
/// sin permitir que un encabezado hostil solicite gigabytes de RAM de una vez.
enum LimitesAutoguardadoTinta {
    static let cargaClaraMaxima = 128 * 1_024 * 1_024
    static let registroCodificadoMaximo = cargaClaraMaxima + 4 * 1_024 * 1_024
}

struct ConfiguracionProteccionCuaderno: Codable, Equatable {
    var documentoId: UInt64
    var nivel: NivelPrivacidadCuaderno = .normal
    var ocultarPrevisualizaciones = false
    var cifrarRespaldos = true
    var compresion: CompresionCuaderno = .equilibrada
    var generacionesAutoguardado = 16
    var actualizado = Date()
}

final class RepositorioPrivacidadCuadernos {
    static let compartido = RepositorioPrivacidadCuadernos()

    private let bloqueo = NSLock()
    private let url: URL
    private var configuraciones: [String: ConfiguracionProteccionCuaderno] = [:]

    private init() {
        let documentos = FileManager.default.urls(for: .documentDirectory,
                                                   in: .userDomainMask).first!
        let carpeta = documentos.appendingPathComponent("Seguridad", isDirectory: true)
        try? FileManager.default.createDirectory(at: carpeta,
                                                 withIntermediateDirectories: true)
        url = carpeta.appendingPathComponent("privacidad-cuadernos.json")
        if let datos = try? Data(contentsOf: url),
           let leidas = try? JSONDecoder.seguridad.decode(
                [String: ConfiguracionProteccionCuaderno].self, from: datos) {
            configuraciones = leidas
        }
        ProtectorArchivos.aplicar(.complete, a: url)
    }

    func configuracion(_ documentoId: UInt64) -> ConfiguracionProteccionCuaderno {
        bloqueo.lock(); defer { bloqueo.unlock() }
        return configuraciones[String(documentoId)]
            ?? ConfiguracionProteccionCuaderno(documentoId: documentoId)
    }

    func guardar(_ configuracion: ConfiguracionProteccionCuaderno) {
        bloqueo.lock(); defer { bloqueo.unlock() }
        var copia = configuracion
        copia.actualizado = Date()
        copia.generacionesAutoguardado = min(max(copia.generacionesAutoguardado, 4), 64)
        configuraciones[String(copia.documentoId)] = copia
        guard let datos = try? JSONEncoder.seguridad.encode(configuraciones) else { return }
        try? datos.write(to: url, options: [.atomic, .completeFileProtection])
        ProtectorArchivos.aplicar(.complete, a: url)
    }
}

enum ErrorSeguridadCuaderno: LocalizedError {
    case noSeCreoClave(OSStatus)
    case noSeLeyóClave(OSStatus)
    case cifradoInvalido
    case autenticacionNoDisponible
    case autenticacionRechazada
    case registroCorrupto
    case documentoDistinto
    case documentoDemasiadoGrande(Int)

    var errorDescription: String? {
        switch self {
        case .noSeCreoClave(let estado):
            return "Keychain no pudo crear la clave local (código \(estado))."
        case .noSeLeyóClave(let estado):
            return "Keychain no pudo recuperar la clave local (código \(estado))."
        case .cifradoInvalido:
            return "El contenido cifrado no tiene un formato válido."
        case .autenticacionNoDisponible:
            return "El iPad no tiene una autenticación del propietario disponible."
        case .autenticacionRechazada:
            return "No se autorizó el acceso a este cuaderno."
        case .registroCorrupto:
            return "El autoguardado no superó la comprobación de integridad."
        case .documentoDistinto:
            return "El autoguardado pertenece a otro cuaderno."
        case .documentoDemasiadoGrande(let bytes):
            return "La instantánea requiere \(ByteCountFormatter.string(fromByteCount: Int64(bytes), countStyle: .file)); debe dividirse antes de crear un journal seguro."
        }
    }
}

enum AlmacenClavesCuaderno {
    private static let servicio = "com.calculadorasunificadas.tinta.claves-cuaderno"

    static func clave(documentoId: UInt64) throws -> SymmetricKey {
        let cuenta = "cuaderno-\(documentoId)"
        let consulta: [String: Any] = [
            kSecClass as String: kSecClassGenericPassword,
            kSecAttrService as String: servicio,
            kSecAttrAccount as String: cuenta,
            kSecReturnData as String: true,
            kSecMatchLimit as String: kSecMatchLimitOne
        ]
        var resultado: CFTypeRef?
        let estado = SecItemCopyMatching(consulta as CFDictionary, &resultado)
        if estado == errSecSuccess, let datos = resultado as? Data, datos.count == 32 {
            return SymmetricKey(data: datos)
        }
        guard estado == errSecItemNotFound else {
            throw ErrorSeguridadCuaderno.noSeLeyóClave(estado)
        }

        var bytes = [UInt8](repeating: 0, count: 32)
        let aleatorio = SecRandomCopyBytes(kSecRandomDefault, bytes.count, &bytes)
        guard aleatorio == errSecSuccess else {
            throw ErrorSeguridadCuaderno.noSeCreoClave(aleatorio)
        }
        let datos = Data(bytes)
        let nuevo: [String: Any] = [
            kSecClass as String: kSecClassGenericPassword,
            kSecAttrService as String: servicio,
            kSecAttrAccount as String: cuenta,
            kSecValueData as String: datos,
            kSecAttrAccessible as String: kSecAttrAccessibleWhenUnlockedThisDeviceOnly
        ]
        let agregado = SecItemAdd(nuevo as CFDictionary, nil)
        if agregado == errSecDuplicateItem {
            return try clave(documentoId: documentoId)
        }
        guard agregado == errSecSuccess else {
            throw ErrorSeguridadCuaderno.noSeCreoClave(agregado)
        }
        return SymmetricKey(data: datos)
    }
}

enum ProtectorDatosCuaderno {
    static func cifrar(_ datos: Data, documentoId: UInt64) throws -> Data {
        let sello = try AES.GCM.seal(datos, using: AlmacenClavesCuaderno.clave(
            documentoId: documentoId))
        guard let combinado = sello.combined else { throw ErrorSeguridadCuaderno.cifradoInvalido }
        return combinado
    }

    static func descifrar(_ datos: Data, documentoId: UInt64) throws -> Data {
        let sello = try AES.GCM.SealedBox(combined: datos)
        return try AES.GCM.open(sello, using: AlmacenClavesCuaderno.clave(
            documentoId: documentoId))
    }

    static func autorizar(_ configuracion: ConfiguracionProteccionCuaderno,
                          motivo: String,
                          terminar: @escaping (Result<Void, Error>) -> Void) {
        guard configuracion.nivel == .protegido else {
            terminar(.success(())); return
        }
        let contexto = LAContext()
        contexto.localizedCancelTitle = "Cancelar"
        var error: NSError?
        guard contexto.canEvaluatePolicy(.deviceOwnerAuthentication, error: &error) else {
            terminar(.failure(error ?? ErrorSeguridadCuaderno.autenticacionNoDisponible))
            return
        }
        contexto.evaluatePolicy(.deviceOwnerAuthentication,
                                 localizedReason: motivo) { autorizado, error in
            DispatchQueue.main.async {
                if autorizado { terminar(.success(())) }
                else { terminar(.failure(error ?? ErrorSeguridadCuaderno.autenticacionRechazada)) }
            }
        }
    }
}

enum ProtectorArchivos {
    static func aplicar(_ proteccion: FileProtectionType, a url: URL) {
        guard FileManager.default.fileExists(atPath: url.path) else { return }
        try? FileManager.default.setAttributes([.protectionKey: proteccion],
                                               ofItemAtPath: url.path)
    }
}

enum CompresorDatosLocal {
    static func comprimir(_ datos: Data, algoritmo: compression_algorithm?) -> Data? {
        guard datos.count <= LimitesAutoguardadoTinta.cargaClaraMaxima else {
            return nil
        }
        guard let algoritmo else { return datos }
        guard !datos.isEmpty else { return Data() }
        let (suma, desbordoSuma) = datos.count.addingReportingOverflow(65_536)
        let (doble, desbordoDoble) = datos.count.multipliedReportingOverflow(by: 2)
        guard !desbordoSuma, !desbordoDoble else { return nil }
        var capacidad = min(max(suma, doble),
                            LimitesAutoguardadoTinta.registroCodificadoMaximo)
        for _ in 0..<5 {
            var salida = Data(count: capacidad)
            let escritos = salida.withUnsafeMutableBytes { destino in
                datos.withUnsafeBytes { origen in
                    compression_encode_buffer(
                        destino.bindMemory(to: UInt8.self).baseAddress!, capacidad,
                        origen.bindMemory(to: UInt8.self).baseAddress!, datos.count,
                        nil, algoritmo)
                }
            }
            if escritos > 0 {
                salida.count = escritos
                return salida
            }
            let (siguiente, desbordo) = capacidad.multipliedReportingOverflow(by: 2)
            guard !desbordo,
                  siguiente <= LimitesAutoguardadoTinta.registroCodificadoMaximo
            else { return nil }
            capacidad = siguiente
        }
        return nil
    }

    static func descomprimir(_ datos: Data, tamanoOriginal: Int,
                             algoritmo: compression_algorithm?) -> Data? {
        guard tamanoOriginal >= 0,
              tamanoOriginal <= LimitesAutoguardadoTinta.cargaClaraMaxima,
              datos.count <= LimitesAutoguardadoTinta.registroCodificadoMaximo
        else { return nil }
        guard let algoritmo else {
            return datos.count == tamanoOriginal ? datos : nil
        }
        if tamanoOriginal == 0 { return Data() }
        var salida = Data(count: tamanoOriginal)
        let escritos = salida.withUnsafeMutableBytes { destino in
            datos.withUnsafeBytes { origen in
                compression_decode_buffer(
                    destino.bindMemory(to: UInt8.self).baseAddress!, tamanoOriginal,
                    origen.bindMemory(to: UInt8.self).baseAddress!, datos.count,
                    nil, algoritmo)
            }
        }
        guard escritos == tamanoOriginal else { return nil }
        return salida
    }
}

struct PaqueteRecuperacionDocumento: Codable {
    var version = 1
    var documentoId: UInt64
    var fecha = Date()
    var motivo: String
    var tinta: Data
    var bloques: [BloqueLienzo]
    var tecnicos: [ObjetoTecnico]
    var regiones: [RegionFondoDocumento]
    var capas: EstadoCapasDocumento
    var recursosEsperados: [String]
}

private struct RegistroAutoguardado: Codable {
    var version = 1
    var documentoId: UInt64
    var revision: UInt64
    var fecha = Date()
    var motivo: String
    var hashCargaClara: String
    var compresion: CompresionCuaderno
    var tamanoOriginal: Int
    var cifrado: Bool
    var carga: Data
}

struct ReporteDiagnosticoCuaderno {
    var registrosEncontrados = 0
    var registrosValidos = 0
    var registrosCorruptos = 0
    var ultimoAutoguardado: Date?
    var recursosFaltantes: [String] = []
    var bytesAutoguardado: Int64 = 0
    var bytesDisponibles: Int64?

    var texto: String {
        let fecha = ultimoAutoguardado?.formatted(date: .abbreviated, time: .standard)
            ?? "ninguno"
        let libre = bytesDisponibles.map { ByteCountFormatter.string(
            fromByteCount: $0, countStyle: .file) } ?? "no disponible"
        return """
        Registros: \(registrosValidos) válidos de \(registrosEncontrados)
        Corruptos: \(registrosCorruptos)
        Último autoguardado válido: \(fecha)
        Recursos referenciados ausentes: \(recursosFaltantes.count)
        Espacio usado por recuperación: \(ByteCountFormatter.string(fromByteCount: bytesAutoguardado, countStyle: .file))
        Espacio libre importante: \(libre)
        """
    }
}

final class RepositorioAutoguardado {
    static let compartido = RepositorioAutoguardado()

    private let base: URL
    private let cola = DispatchQueue(label: "tinta.autoguardado.archivos", qos: .utility)

    private init() {
        let documentos = FileManager.default.urls(for: .documentDirectory,
                                                   in: .userDomainMask).first!
        base = documentos.appendingPathComponent("Recuperacion", isDirectory: true)
        try? FileManager.default.createDirectory(at: base,
                                                 withIntermediateDirectories: true)
        ProtectorArchivos.aplicar(.completeUntilFirstUserAuthentication, a: base)
    }

    private func carpeta(_ documentoId: UInt64) -> URL {
        base.appendingPathComponent(String(documentoId), isDirectory: true)
    }

    func guardar(_ paquete: PaqueteRecuperacionDocumento,
                 revision: UInt64,
                 configuracion: ConfiguracionProteccionCuaderno,
                 terminar: @escaping (Result<URL, Error>) -> Void) {
        cola.async {
            do {
                let carpeta = self.carpeta(paquete.documentoId)
                try FileManager.default.createDirectory(at: carpeta,
                                                        withIntermediateDirectories: true)
                let codificador = PropertyListEncoder()
                codificador.outputFormat = .binary
                let clara = try codificador.encode(paquete)
                guard clara.count <= LimitesAutoguardadoTinta.cargaClaraMaxima else {
                    throw ErrorSeguridadCuaderno.documentoDemasiadoGrande(clara.count)
                }
                let hash = SHA256.hash(data: clara).map { String(format: "%02x", $0) }.joined()
                guard let comprimida = CompresorDatosLocal.comprimir(
                    clara, algoritmo: configuracion.compresion.algoritmo) else {
                    throw ErrorSeguridadCuaderno.registroCorrupto
                }
                let seCifra = configuracion.nivel != .normal
                let carga = seCifra
                    ? try ProtectorDatosCuaderno.cifrar(comprimida,
                                                        documentoId: paquete.documentoId)
                    : comprimida
                let registro = RegistroAutoguardado(
                    documentoId: paquete.documentoId,
                    revision: revision,
                    motivo: paquete.motivo,
                    hashCargaClara: hash,
                    compresion: configuracion.compresion,
                    tamanoOriginal: clara.count,
                    cifrado: seCifra,
                    carga: carga)
                let datos = try codificador.encode(registro)
                let ranura = Int(revision % UInt64(configuracion.generacionesAutoguardado))
                let destino = carpeta.appendingPathComponent("generacion-\(ranura).tintajournal")
                try datos.write(to: destino, options: [.atomic, .completeFileProtection])
                ProtectorArchivos.aplicar(seCifra ? .complete : .completeUntilFirstUserAuthentication,
                                          a: destino)
                DispatchQueue.main.async { terminar(.success(destino)) }
            } catch {
                DispatchQueue.main.async { terminar(.failure(error)) }
            }
        }
    }

    func ultimaValida(documentoId: UInt64) throws -> PaqueteRecuperacionDocumento? {
        let registros = try cargarRegistros(documentoId: documentoId)
            .sorted { $0.revision > $1.revision }
        for registro in registros {
            if let paquete = try? decodificar(registro, documentoId: documentoId) {
                return paquete
            }
        }
        return nil
    }

    func tieneAutoguardado(documentoId: UInt64) -> Bool {
        (try? ultimaValida(documentoId: documentoId)) != nil
    }

    func marcarSesion(_ documentoId: UInt64, activa: Bool) {
        let url = carpeta(documentoId).appendingPathComponent("estado-sesion.json")
        try? FileManager.default.createDirectory(at: carpeta(documentoId),
                                                 withIntermediateDirectories: true)
        let estado = ["activa": activa, "fecha": Date().timeIntervalSince1970] as [String: Any]
        if let datos = try? JSONSerialization.data(withJSONObject: estado,
                                                   options: [.sortedKeys]) {
            try? datos.write(to: url, options: [.atomic, .completeFileProtection])
        }
    }

    func sesionAnteriorInconclusa(_ documentoId: UInt64) -> Bool {
        let url = carpeta(documentoId).appendingPathComponent("estado-sesion.json")
        guard let datos = try? Data(contentsOf: url),
              let objeto = try? JSONSerialization.jsonObject(with: datos) as? [String: Any]
        else { return false }
        return objeto["activa"] as? Bool ?? false
    }

    func diagnosticar(documentoId: UInt64) -> ReporteDiagnosticoCuaderno {
        var reporte = ReporteDiagnosticoCuaderno()
        let carpeta = carpeta(documentoId)
        let urls = (try? FileManager.default.contentsOfDirectory(
            at: carpeta, includingPropertiesForKeys: [.fileSizeKey],
            options: [.skipsHiddenFiles])) ?? []
        let diarios = urls.filter { $0.pathExtension == "tintajournal" }
        reporte.registrosEncontrados = diarios.count
        for url in diarios {
            let valores = try? url.resourceValues(forKeys: [.fileSizeKey])
            reporte.bytesAutoguardado += Int64(valores?.fileSize ?? 0)
            do {
                guard (valores?.fileSize ?? Int.max)
                        <= LimitesAutoguardadoTinta.registroCodificadoMaximo else {
                    throw ErrorSeguridadCuaderno.registroCorrupto
                }
                let datos = try Data(contentsOf: url)
                let registro = try PropertyListDecoder().decode(
                    RegistroAutoguardado.self, from: datos)
                let paquete = try decodificar(registro, documentoId: documentoId)
                reporte.registrosValidos += 1
                reporte.ultimoAutoguardado = max(reporte.ultimoAutoguardado ?? .distantPast,
                                                  registro.fecha)
                for hash in paquete.recursosEsperados
                where RepositorioAdjuntos.compartido.url(hash: hash) == nil {
                    reporte.recursosFaltantes.append(hash)
                }
            } catch {
                reporte.registrosCorruptos += 1
            }
        }
        reporte.recursosFaltantes = Array(Set(reporte.recursosFaltantes)).sorted()
        reporte.bytesDisponibles = try? base.resourceValues(
            forKeys: [.volumeAvailableCapacityForImportantUsageKey])
            .volumeAvailableCapacityForImportantUsage
        return reporte
    }

    private func cargarRegistros(documentoId: UInt64) throws -> [RegistroAutoguardado] {
        let urls = try FileManager.default.contentsOfDirectory(
            at: carpeta(documentoId), includingPropertiesForKeys: [.fileSizeKey],
            options: [.skipsHiddenFiles])
            .filter { $0.pathExtension == "tintajournal" }
        return urls.compactMap { url in
            guard let tamano = try? url.resourceValues(forKeys: [.fileSizeKey]).fileSize,
                  tamano <= LimitesAutoguardadoTinta.registroCodificadoMaximo
            else { return nil }
            guard let datos = try? Data(contentsOf: url) else { return nil }
            return try? PropertyListDecoder().decode(RegistroAutoguardado.self, from: datos)
        }
    }

    private func decodificar(_ registro: RegistroAutoguardado,
                             documentoId: UInt64) throws -> PaqueteRecuperacionDocumento {
        guard registro.version == 1,
              registro.tamanoOriginal >= 0,
              registro.tamanoOriginal <= LimitesAutoguardadoTinta.cargaClaraMaxima,
              registro.carga.count <= LimitesAutoguardadoTinta.registroCodificadoMaximo,
              registro.hashCargaClara.count == 64
        else { throw ErrorSeguridadCuaderno.registroCorrupto }
        guard registro.documentoId == documentoId else {
            throw ErrorSeguridadCuaderno.documentoDistinto
        }
        let comprimida = registro.cifrado
            ? try ProtectorDatosCuaderno.descifrar(registro.carga, documentoId: documentoId)
            : registro.carga
        guard let clara = CompresorDatosLocal.descomprimir(
            comprimida, tamanoOriginal: registro.tamanoOriginal,
            algoritmo: registro.compresion.algoritmo) else {
            throw ErrorSeguridadCuaderno.registroCorrupto
        }
        let hash = SHA256.hash(data: clara).map { String(format: "%02x", $0) }.joined()
        guard hash == registro.hashCargaClara else {
            throw ErrorSeguridadCuaderno.registroCorrupto
        }
        let paquete = try PropertyListDecoder().decode(
            PaqueteRecuperacionDocumento.self, from: clara)
        guard paquete.version == 1 else {
            throw ErrorSeguridadCuaderno.registroCorrupto
        }
        guard paquete.documentoId == documentoId else {
            throw ErrorSeguridadCuaderno.documentoDistinto
        }
        return paquete
    }
}

enum EstadoAutoguardado: Equatable {
    case sinCambios
    case pendiente
    case guardando
    case guardado(Date)
    case fallo(String)

    var texto: String {
        switch self {
        case .sinCambios, .guardado: return "Guardado"
        case .pendiente: return "Por guardar"
        case .guardando: return "Guardando…"
        case .fallo: return "No guardado"
        }
    }

    /// Conserva el detalle para VoiceOver y diagnóstico sin llenar la barra
    /// principal con una hora que cambia constantemente o errores extensos.
    var detalleAccesible: String? {
        switch self {
        case .guardado(let fecha):
            return "Último guardado \(fecha.formatted(date: .omitted, time: .standard))"
        case .fallo(let mensaje):
            return mensaje
        default:
            return nil
        }
    }

    var simbolo: String {
        switch self {
        case .sinCambios, .guardado: return "checkmark.icloud"
        case .pendiente: return "clock"
        case .guardando: return "arrow.triangle.2.circlepath"
        case .fallo: return "exclamationmark.triangle"
        }
    }
}

/// Separa la edición más reciente de la última instantánea confirmada. Una
/// escritura asíncrona vieja nunca puede volver verde un cambio más nuevo.
struct GeneracionesAutoguardado {
    private(set) var actual: UInt64 = 0
    private(set) var segura: UInt64 = 0

    var pendiente: Bool { segura != actual }

    @discardableResult
    mutating func marcarCambio() -> UInt64 {
        actual &+= 1
        return actual
    }

    /// Devuelve `true` únicamente cuando esta confirmación cubre todo lo editado.
    @discardableResult
    mutating func confirmar(_ generacion: UInt64) -> Bool {
        if generacion > segura { segura = generacion }
        return !pendiente
    }
}

final class CoordinadorAutoguardado {
    typealias PersistidorCanonico = SecuenciaCommitAutoguardado.PersistidorCanonico

    private let sesion: Sesion
    private weak var navegador: VistaLienzoNavegable?
    private var revision = UInt64(Date().timeIntervalSince1970 * 1_000)
    private var trabajoPendiente: DispatchWorkItem?
    private var temporizador: Timer?
    private var observadores: [NSObjectProtocol] = []
    private var generaciones = GeneracionesAutoguardado()
    private var persistidorCanonico: PersistidorCanonico?
    private(set) var estado: EstadoAutoguardado = .sinCambios {
        didSet { alCambiarEstado?(estado) }
    }
    var alCambiarEstado: ((EstadoAutoguardado) -> Void)?

    init(sesion: Sesion, navegador: VistaLienzoNavegable) {
        self.sesion = sesion
        self.navegador = navegador
    }

    /// Vincula el journal de recuperación con la fuente de verdad que abre la
    /// pestaña. El estado no se confirma hasta que ambas escrituras terminan.
    /// Debe configurarse antes de cargar o editar el documento.
    func configurarPersistidorCanonico(_ persistidor: @escaping PersistidorCanonico) {
        precondition(Thread.isMainThread,
                     "La persistencia canónica se configura desde el host UIKit")
        persistidorCanonico = persistidor
    }

    @discardableResult
    func iniciar() -> Bool {
        let id = sesion.documentoPersistenteId
        let inconclusa = RepositorioAutoguardado.compartido
            .sesionAnteriorInconclusa(id)
            && RepositorioAutoguardado.compartido.tieneAutoguardado(documentoId: id)
        RepositorioAutoguardado.compartido.marcarSesion(id, activa: true)
        temporizador = Timer.scheduledTimer(withTimeInterval: 20, repeats: true) {
            [weak self] _ in
            guard let self, self.generaciones.pendiente else { return }
            self.guardarAhora(motivo: "intervalo de seguridad")
        }
        // El propietario único de didEnterBackground es ControladorLienzo.
        // Aquí sólo conservamos señales que no crean un segundo commit del mismo
        // documento durante la ventana crítica de suspensión.
        let centro = NotificationCenter.default
        observadores.append(centro.addObserver(
            forName: UIApplication.willEnterForegroundNotification,
            object: nil, queue: .main) { [weak self] _ in
                guard let self else { return }
                RepositorioAutoguardado.compartido.marcarSesion(
                    self.sesion.documentoPersistenteId, activa: true)
            })
        observadores.append(centro.addObserver(
            forName: UIApplication.protectedDataWillBecomeUnavailableNotification,
            object: nil, queue: .main) { [weak self] _ in
                self?.guardarAhora(motivo: "iPad bloqueado")
            })
        return inconclusa
    }

    func marcarCambio() {
        generaciones.marcarCambio()
        estado = .pendiente
        trabajoPendiente?.cancel()
        let trabajo = DispatchWorkItem { [weak self] in
            self?.guardarAhora(motivo: "edición")
        }
        trabajoPendiente = trabajo
        DispatchQueue.main.asyncAfter(deadline: .now() + 1.5, execute: trabajo)
    }

    func guardarAhora(motivo: String, terminar: ((Result<URL, Error>) -> Void)? = nil) {
        precondition(Thread.isMainThread, "La instantánea del núcleo se captura en el hilo principal")
        guard let navegador, let tinta = sesion.guardar() else {
            estado = .fallo("el núcleo no entregó una instantánea")
            terminar?(.failure(ErrorSeguridadCuaderno.registroCorrupto))
            return
        }
        trabajoPendiente?.cancel()
        let generacionCapturada = generaciones.actual
        revision &+= 1
        let id = sesion.documentoPersistenteId
        let paquete = PaqueteRecuperacionDocumento(
            documentoId: id,
            motivo: motivo,
            tinta: tinta,
            bloques: navegador.capaBloques.bloques,
            tecnicos: navegador.capaTecnica.objetos,
            regiones: RepositorioRegionesFondo.compartido.regiones(id),
            capas: RepositorioCapas.compartido.estado(id),
            recursosEsperados: Array(Set(sesion.medios.map(\.hash))).sorted())
        let configuracion = RepositorioPrivacidadCuadernos.compartido.configuracion(id)
        let persistidorCanonico = self.persistidorCanonico
        estado = .guardando
        let revisionCapturada = revision
        SecuenciaCommitAutoguardado.ejecutar(
            datosCanonicos: tinta,
            persistidorJournal: { finalizarJournal in
                RepositorioAutoguardado.compartido.guardar(
                    paquete, revision: revisionCapturada,
                    configuracion: configuracion,
                    terminar: finalizarJournal)
            },
            persistidorCanonico: persistidorCanonico) { [weak self] resultado in
                switch resultado {
                case .success:
                    if let self {
                        self.estado = self.generaciones.confirmar(generacionCapturada)
                            ? .guardado(Date()) : .pendiente
                        // El host ya inició este commit al recibir
                        // didEnterBackground. Sólo tras confirmar journal +
                        // canónico se marca la sesión como cierre limpio.
                        if UIApplication.shared.applicationState == .background {
                            self.marcarCierreLimpio()
                        }
                    }
                case .failure(let error):
                    self?.estado = .fallo(error.localizedDescription)
                }
                terminar?(resultado)
            }
    }

    func autorizarYRestaurar(terminar: @escaping (Result<Void, Error>) -> Void) {
        let id = sesion.documentoPersistenteId
        let configuracion = RepositorioPrivacidadCuadernos.compartido.configuracion(id)
        ProtectorDatosCuaderno.autorizar(
            configuracion, motivo: "Abrir la recuperación cifrada de este cuaderno") {
                [weak self] autorizacion in
                guard case .success = autorizacion, let self, let navegador = self.navegador
                else { terminar(autorizacion); return }
                do {
                    guard let paquete = try RepositorioAutoguardado.compartido
                        .ultimaValida(documentoId: id) else {
                        throw ErrorSeguridadCuaderno.registroCorrupto
                    }
                    guard self.sesion.cargar(paquete.tinta) else {
                        throw ErrorSeguridadCuaderno.registroCorrupto
                    }
                    navegador.capaBloques.reemplazar(paquete.bloques)
                    navegador.capaTecnica.objetos = paquete.tecnicos
                    RepositorioRegionesFondo.compartido.guardar(paquete.regiones,
                                                                documento: id)
                    navegador.lienzo.regionesFondo = RepositorioRegionesFondo
                        .render(paquete.regiones)
                    RepositorioCapas.compartido.guardar(paquete.capas, documento: id)
                    navegador.aplicarCapas(paquete.capas)
                    navegador.sincronizarTamanoDocumento(conservarCentro: true)
                    navegador.recargarElementos()
                    navegador.lienzo.rehornearYDibujar()
                    guard let persistidorCanonico = self.persistidorCanonico else {
                        throw ErrorCommitAutoguardado.destinoCanonicoAusente
                    }
                    self.estado = .guardando
                    persistidorCanonico(paquete.tinta) { [weak self] resultado in
                        switch resultado {
                        case .success:
                            self?.estado = .guardado(paquete.fecha)
                            terminar(.success(()))
                        case .failure(let error):
                            self?.estado = .fallo(error.localizedDescription)
                            terminar(.failure(error))
                        }
                    }
                } catch {
                    self.estado = .fallo(error.localizedDescription)
                    terminar(.failure(error))
                }
            }
    }

    func marcarCierreLimpio() {
        RepositorioAutoguardado.compartido.marcarSesion(
            sesion.documentoPersistenteId, activa: false)
    }

    deinit {
        temporizador?.invalidate()
        trabajoPendiente?.cancel()
        observadores.forEach(NotificationCenter.default.removeObserver)
    }
}

struct ResumenAlmacenamientoIPad {
    var categorias: [(String, Int64)]
    var total: Int64 { categorias.reduce(0) { $0 + $1.1 } }
}

enum GestorAlmacenamientoIPad {
    static func medir() -> ResumenAlmacenamientoIPad {
        let documentos = FileManager.default.urls(for: .documentDirectory,
                                                   in: .userDomainMask).first!
        let nombres = ["Adjuntos", "Grabaciones", "Recuperacion", "Versiones",
                       "Backups", "Academico", "Capas", "RegionesFondo"]
        let categorias = nombres.map { nombre -> (String, Int64) in
            let url = documentos.appendingPathComponent(nombre, isDirectory: true)
            return (nombre, tamano(url))
        }.filter { $0.1 > 0 }
        return ResumenAlmacenamientoIPad(categorias: categorias)
    }

    private static func tamano(_ carpeta: URL) -> Int64 {
        guard let enumerador = FileManager.default.enumerator(
            at: carpeta, includingPropertiesForKeys: [.isRegularFileKey, .fileSizeKey],
            options: [.skipsHiddenFiles, .skipsPackageDescendants]) else { return 0 }
        var total: Int64 = 0
        for case let url as URL in enumerador {
            guard let valores = try? url.resourceValues(forKeys: [.isRegularFileKey,
                                                                  .fileSizeKey]),
                  valores.isRegularFile == true else { continue }
            total += Int64(valores.fileSize ?? 0)
        }
        return total
    }
}

final class EjecutorOperacionesPesadas {
    static let compartido = EjecutorOperacionesPesadas()
    private let cola: OperationQueue = {
        let cola = OperationQueue()
        cola.name = "Tinta · operaciones locales pesadas"
        cola.qualityOfService = .utility
        cola.maxConcurrentOperationCount = 1
        return cola
    }()

    private init() {}

    @discardableResult
    func ejecutar(nombre: String, unidades: Int64 = 100,
                  trabajo: @escaping (Progress) throws -> Void,
                  terminar: @escaping (Result<Void, Error>) -> Void) -> Progress {
        let progreso = Progress(totalUnitCount: unidades)
        var tareaFondo: UIBackgroundTaskIdentifier = .invalid
        tareaFondo = UIApplication.shared.beginBackgroundTask(withName: nombre) {
            progreso.cancel()
            UIApplication.shared.endBackgroundTask(tareaFondo)
            tareaFondo = .invalid
        }
        cola.addOperation {
            let resultado: Result<Void, Error>
            do {
                try autoreleasepool { try trabajo(progreso) }
                resultado = .success(())
            } catch {
                resultado = .failure(error)
            }
            DispatchQueue.main.async {
                if tareaFondo != .invalid {
                    UIApplication.shared.endBackgroundTask(tareaFondo)
                    tareaFondo = .invalid
                }
                terminar(resultado)
            }
        }
        return progreso
    }
}

final class PanelSeguridadRecuperacion: UITableViewController {
    private let sesion: Sesion
    private weak var navegador: VistaLienzoNavegable?
    private let autoguardado: CoordinadorAutoguardado
    private var configuracion: ConfiguracionProteccionCuaderno
    private var diagnostico: ReporteDiagnosticoCuaderno?
    private var almacenamiento: ResumenAlmacenamientoIPad?

    init(sesion: Sesion, navegador: VistaLienzoNavegable,
         autoguardado: CoordinadorAutoguardado) {
        self.sesion = sesion
        self.navegador = navegador
        self.autoguardado = autoguardado
        configuracion = RepositorioPrivacidadCuadernos.compartido.configuracion(
            sesion.documentoPersistenteId)
        super.init(style: .insetGrouped)
    }

    required init?(coder: NSCoder) { fatalError("init(coder:) no está implementado") }

    override func viewDidLoad() {
        super.viewDidLoad()
        title = "Privacidad, recuperación y almacenamiento"
        navigationItem.leftBarButtonItem = UIBarButtonItem(
            barButtonSystemItem: .close, target: self, action: #selector(cerrar))
    }

    override func numberOfSections(in tableView: UITableView) -> Int { 4 }

    override func tableView(_ tableView: UITableView,
                            titleForHeaderInSection section: Int) -> String? {
        ["Privacidad por cuaderno", "Autoguardado y recuperación",
         "Diagnóstico", "Almacenamiento local"][section]
    }

    override func tableView(_ tableView: UITableView,
                            numberOfRowsInSection section: Int) -> Int {
        [3, 3, 2, max((almacenamiento?.categorias.count ?? 0) + 1, 1)][section]
    }

    override func tableView(_ tableView: UITableView,
                            cellForRowAt indexPath: IndexPath) -> UITableViewCell {
        let celda = UITableViewCell(style: .subtitle, reuseIdentifier: nil)
        celda.textLabel?.numberOfLines = 0
        celda.detailTextLabel?.numberOfLines = 0
        switch (indexPath.section, indexPath.row) {
        case (0, 0):
            celda.textLabel?.text = "Nivel de privacidad"
            celda.detailTextLabel?.text = configuracion.nivel.rawValue
            celda.accessoryType = .disclosureIndicator
        case (0, 1):
            celda.textLabel?.text = "Ocultar previsualizaciones del sistema"
            celda.detailTextLabel?.text = configuracion.ocultarPrevisualizaciones
                ? "Activado para este cuaderno" : "Desactivado"
            celda.accessoryType = configuracion.ocultarPrevisualizaciones ? .checkmark : .none
        case (0, 2):
            celda.textLabel?.text = "Compresión de copias"
            celda.detailTextLabel?.text = configuracion.compresion.rawValue
            celda.accessoryType = .disclosureIndicator
        case (1, 0):
            celda.textLabel?.text = "Guardar ahora"
            celda.detailTextLabel?.text = autoguardado.estado.texto
        case (1, 1):
            celda.textLabel?.text = "Restaurar último estado íntegro"
            celda.detailTextLabel?.text = "No elimina el estado actual ni las generaciones anteriores."
        case (1, 2):
            celda.textLabel?.text = "Generaciones retenidas"
            celda.detailTextLabel?.text = "\(configuracion.generacionesAutoguardado) copias rotativas y atómicas"
        case (2, 0):
            celda.textLabel?.text = "Ejecutar diagnóstico local"
            celda.detailTextLabel?.text = "Comprueba hashes, recursos y espacio sin modificar el cuaderno."
        case (2, 1):
            celda.textLabel?.text = diagnostico == nil ? "Sin diagnóstico reciente" : "Resultado"
            celda.detailTextLabel?.text = diagnostico?.texto
        case (3, 0):
            celda.textLabel?.text = "Medir almacenamiento"
            if let almacenamiento {
                celda.detailTextLabel?.text = "Total administrado: \(ByteCountFormatter.string(fromByteCount: almacenamiento.total, countStyle: .file))"
            }
        default:
            if let item = almacenamiento?.categorias[indexPath.row - 1] {
                celda.textLabel?.text = item.0
                celda.detailTextLabel?.text = ByteCountFormatter.string(
                    fromByteCount: item.1, countStyle: .file)
            }
        }
        return celda
    }

    override func tableView(_ tableView: UITableView, didSelectRowAt indexPath: IndexPath) {
        tableView.deselectRow(at: indexPath, animated: true)
        switch (indexPath.section, indexPath.row) {
        case (0, 0): elegirPrivacidad()
        case (0, 1):
            configuracion.ocultarPrevisualizaciones.toggle(); persistirConfiguracion()
        case (0, 2): elegirCompresion()
        case (1, 0):
            autoguardado.guardarAhora(motivo: "solicitud manual") { [weak self] resultado in
                self?.avisar(resultado.mensajeSeguridad)
                self?.tableView.reloadData()
            }
        case (1, 1): confirmarRestauracion()
        case (1, 2): elegirGeneraciones()
        case (2, 0):
            diagnostico = RepositorioAutoguardado.compartido.diagnosticar(
                documentoId: sesion.documentoPersistenteId)
            tableView.reloadSections(IndexSet(integer: 2), with: .automatic)
        case (3, 0):
            almacenamiento = GestorAlmacenamientoIPad.medir()
            tableView.reloadSections(IndexSet(integer: 3), with: .automatic)
        default: break
        }
    }

    private func elegirPrivacidad() {
        let hoja = UIAlertController(title: "Privacidad de este cuaderno",
                                     message: "El cifrado usa AES-GCM y una clave que no sale de este iPad.",
                                     preferredStyle: .actionSheet)
        for nivel in NivelPrivacidadCuaderno.allCases {
            hoja.addAction(UIAlertAction(title: nivel.rawValue, style: .default) {
                [weak self] _ in self?.configuracion.nivel = nivel; self?.persistirConfiguracion()
            })
        }
        hoja.addAction(UIAlertAction(title: "Cancelar", style: .cancel))
        hoja.popoverPresentationController?.sourceView = view
        present(hoja, animated: true)
    }

    private func elegirCompresion() {
        let hoja = UIAlertController(title: "Compresión",
                                     message: "LZMA ahorra más espacio, pero consume más CPU y batería.",
                                     preferredStyle: .actionSheet)
        for nivel in CompresionCuaderno.allCases {
            hoja.addAction(UIAlertAction(title: nivel.rawValue, style: .default) {
                [weak self] _ in self?.configuracion.compresion = nivel; self?.persistirConfiguracion()
            })
        }
        hoja.addAction(UIAlertAction(title: "Cancelar", style: .cancel))
        hoja.popoverPresentationController?.sourceView = view
        present(hoja, animated: true)
    }

    private func elegirGeneraciones() {
        let alerta = UIAlertController(title: "Generaciones 4–64", message: nil,
                                       preferredStyle: .alert)
        alerta.addTextField {
            $0.keyboardType = .numberPad
            $0.text = String(self.configuracion.generacionesAutoguardado)
        }
        alerta.addAction(UIAlertAction(title: "Aplicar", style: .default) { [weak self] _ in
            guard let self, let texto = alerta.textFields?.first?.text,
                  let cantidad = Int(texto) else { return }
            self.configuracion.generacionesAutoguardado = min(max(cantidad, 4), 64)
            self.persistirConfiguracion()
        })
        alerta.addAction(UIAlertAction(title: "Cancelar", style: .cancel))
        present(alerta, animated: true)
    }

    private func confirmarRestauracion() {
        let alerta = UIAlertController(
            title: "Restaurar recuperación",
            message: "Se cargará la generación íntegra más reciente. Primero se crea una versión del estado actual.",
            preferredStyle: .actionSheet)
        alerta.addAction(UIAlertAction(title: "Restaurar", style: .default) { [weak self] _ in
            guard let self, let navegador = self.navegador else { return }
            _ = GestorVersionesDocumento.compartido.crear(
                nombre: "Antes de recuperación", motivo: "respaldo de seguridad",
                sesion: self.sesion, bloques: navegador.capaBloques.bloques,
                tecnicos: navegador.capaTecnica.objetos,
                capas: RepositorioCapas.compartido.estado(
                    self.sesion.documentoPersistenteId))
            self.autoguardado.autorizarYRestaurar { [weak self] resultado in
                self?.avisar(resultado.mensajeSeguridad)
                self?.tableView.reloadData()
            }
        })
        alerta.addAction(UIAlertAction(title: "Cancelar", style: .cancel))
        alerta.popoverPresentationController?.sourceView = view
        present(alerta, animated: true)
    }

    private func persistirConfiguracion() {
        RepositorioPrivacidadCuadernos.compartido.guardar(configuracion)
        tableView.reloadData()
    }

    private func avisar(_ texto: String) {
        let alerta = UIAlertController(title: nil, message: texto, preferredStyle: .alert)
        alerta.addAction(UIAlertAction(title: "Aceptar", style: .default))
        present(alerta, animated: true)
    }

    @objc private func cerrar() { dismiss(animated: true) }
}

private extension Result where Success == Void, Failure == Error {
    var mensajeSeguridad: String {
        switch self {
        case .success: return "Operación terminada."
        case .failure(let error): return error.localizedDescription
        }
    }
}

private extension Result where Success == URL, Failure == Error {
    var mensajeSeguridad: String {
        switch self {
        case .success(let url): return "Autoguardado íntegro: \(url.lastPathComponent)"
        case .failure(let error): return error.localizedDescription
        }
    }
}

private extension JSONEncoder {
    static var seguridad: JSONEncoder {
        let encoder = JSONEncoder()
        encoder.dateEncodingStrategy = .iso8601
        encoder.outputFormatting = [.sortedKeys]
        return encoder
    }
}

private extension JSONDecoder {
    static var seguridad: JSONDecoder {
        let decoder = JSONDecoder()
        decoder.dateDecodingStrategy = .iso8601
        return decoder
    }
}
