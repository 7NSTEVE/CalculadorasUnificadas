import AVFoundation
import CoreImage
import CryptoKit
import MultipeerConnectivity
import Security
import UIKit

enum RolColaboracion: String, Codable, CaseIterable {
    case profesor = "Profesor / anfitrión"
    case editor = "Editor"
    case companero = "Compañero que anota"
    case lector = "Solo lectura"
    case segundaPantalla = "Segunda pantalla"

    func permite(_ tipo: TipoOperacionColaborativa) -> Bool {
        switch self {
        case .profesor:
            return true
        case .editor:
            return tipo != .controlSesion
        case .companero:
            return [.anadirTrazo, .eliminarTrazo, .cursor, .portapapeles,
                    .marcador].contains(tipo)
        case .lector:
            return tipo == .cursor
        case .segundaPantalla:
            return tipo == .cursor || tipo == .vistaPresentacion
        }
    }
}

enum TipoOperacionColaborativa: String, Codable {
    case anadirTrazo
    case eliminarTrazo
    case actualizarTexto
    case eliminarTexto
    case actualizarBloque
    case eliminarBloque
    case actualizarTecnico
    case eliminarTecnico
    case cursor
    case vistaPresentacion
    case portapapeles
    case marcador
    case controlSesion
}

struct MarcaLamport: Codable, Comparable, Equatable {
    var contador: UInt64
    var autor: String

    static func < (izquierda: MarcaLamport, derecha: MarcaLamport) -> Bool {
        if izquierda.contador != derecha.contador {
            return izquierda.contador < derecha.contador
        }
        return izquierda.autor < derecha.autor
    }
}

struct OperacionColaborativa: Codable, Identifiable, Equatable {
    var id = UUID()
    var sesionId: UUID
    var documentoId: UInt64
    var marca: MarcaLamport
    var tipo: TipoOperacionColaborativa
    var objetoId: String
    var carga: Data?
    var tumba = false
    var rolAutor: RolColaboracion
    var fecha = Date()
}

final class RelojLamportColaboracion {
    static let compartido = RelojLamportColaboracion()
    let autor: String
    private var contador: UInt64
    private let bloqueo = NSLock()

    private init() {
        let preferencias = UserDefaults.standard
        if let guardado = preferencias.string(forKey: "Tinta.AutorColaboracion") {
            autor = guardado
        } else {
            let nuevo = UUID().uuidString
            preferencias.set(nuevo, forKey: "Tinta.AutorColaboracion")
            autor = nuevo
        }
        contador = UInt64(preferencias.object(forKey: "Tinta.RelojLamport") as? Int64 ?? 0)
    }

    func siguiente(observando remoto: UInt64? = nil) -> MarcaLamport {
        bloqueo.lock(); defer { bloqueo.unlock() }
        if let remoto { contador = max(contador, remoto) }
        contador &+= 1
        UserDefaults.standard.set(Int64(clamping: contador),
                                  forKey: "Tinta.RelojLamport")
        return MarcaLamport(contador: contador, autor: autor)
    }
}

final class RegistroOperacionesColaborativas {
    static let compartido = RegistroOperacionesColaborativas()

    private let bloqueo = NSRecursiveLock()
    private let url: URL
    private var porDocumento: [String: [OperacionColaborativa]] = [:]

    private init() {
        let documentos = FileManager.default.urls(for: .documentDirectory,
                                                   in: .userDomainMask).first!
        let carpeta = documentos.appendingPathComponent("Colaboracion", isDirectory: true)
        try? FileManager.default.createDirectory(at: carpeta,
                                                 withIntermediateDirectories: true)
        url = carpeta.appendingPathComponent("operaciones-crdt.plist")
        if let datos = try? Data(contentsOf: url),
           let leidas = try? PropertyListDecoder().decode(
                [String: [OperacionColaborativa]].self, from: datos) {
            porDocumento = leidas
        }
        ProtectorArchivos.aplicar(.completeUntilFirstUserAuthentication, a: url)
    }

    /// Devuelve si la operación era nueva y si quedó como ganadora LWW para su
    /// objeto. Todas se conservan para que dos pares puedan converger después.
    func registrar(_ operacion: OperacionColaborativa) -> (nueva: Bool, ganadora: Bool) {
        bloqueo.lock(); defer { bloqueo.unlock() }
        let clave = String(operacion.documentoId)
        var operaciones = porDocumento[clave] ?? []
        guard !operaciones.contains(where: { $0.id == operacion.id }) else {
            return (false, false)
        }
        operaciones.append(operacion)
        porDocumento[clave] = operaciones
        persistirSinBloqueo()
        let ganadora = operaciones
            .filter { $0.objetoId == operacion.objetoId && $0.tipo.familia == operacion.tipo.familia }
            .max { $0.marca < $1.marca }?.id == operacion.id
        return (true, ganadora)
    }

    func operaciones(documentoId: UInt64) -> [OperacionColaborativa] {
        bloqueo.lock(); defer { bloqueo.unlock() }
        return (porDocumento[String(documentoId)] ?? []).sorted {
            if $0.marca != $1.marca { return $0.marca < $1.marca }
            return $0.id.uuidString < $1.id.uuidString
        }
    }

    private func persistirSinBloqueo() {
        let encoder = PropertyListEncoder()
        encoder.outputFormat = .binary
        guard let datos = try? encoder.encode(porDocumento) else { return }
        try? datos.write(to: url, options: [.atomic, .completeFileProtection])
    }
}

private extension TipoOperacionColaborativa {
    var familia: String {
        switch self {
        case .anadirTrazo, .eliminarTrazo: return "trazo"
        case .actualizarTexto, .eliminarTexto: return "texto"
        case .actualizarBloque, .eliminarBloque: return "bloque"
        case .actualizarTecnico, .eliminarTecnico: return "tecnico"
        default: return rawValue
        }
    }
}

struct InvitacionColaboracion: Codable, Equatable {
    var version = 1
    var sesionId = UUID()
    var documentoId: UInt64
    var anfitrion: String
    var secreto: Data
    var rolesPermitidos: [RolColaboracion]
    var creada = Date()
    var vence: Date

    var expirada: Bool { vence < Date() }

    func enlace() -> URL? {
        let encoder = JSONEncoder.colaboracion
        guard let datos = try? encoder.encode(self) else { return nil }
        let carga = datos.base64EncodedString()
            .replacingOccurrences(of: "+", with: "-")
            .replacingOccurrences(of: "/", with: "_")
            .replacingOccurrences(of: "=", with: "")
        var componentes = URLComponents()
        componentes.scheme = "tinta"
        componentes.host = "colaborar"
        componentes.queryItems = [URLQueryItem(name: "invitacion", value: carga)]
        return componentes.url
    }

    static func leer(_ url: URL) -> InvitacionColaboracion? {
        guard url.scheme?.lowercased() == "tinta", url.host == "colaborar",
              let componentes = URLComponents(url: url, resolvingAgainstBaseURL: false),
              var texto = componentes.queryItems?.first(where: {
                  $0.name == "invitacion"
              })?.value else { return nil }
        texto = texto.replacingOccurrences(of: "-", with: "+")
            .replacingOccurrences(of: "_", with: "/")
        let resto = texto.count % 4
        if resto != 0 { texto += String(repeating: "=", count: 4 - resto) }
        guard let datos = Data(base64Encoded: texto),
              let invitacion = try? JSONDecoder.colaboracion.decode(
                InvitacionColaboracion.self, from: datos),
              invitacion.version == 1, invitacion.secreto.count == 32,
              !invitacion.expirada else { return nil }
        return invitacion
    }
}

struct DestinoEnlaceTinta: Codable, Equatable {
    var documentoId: UInt64
    var elementoId: String?
    var bloqueId: UUID?
    var x: CGFloat?
    var y: CGFloat?
    var zoom: CGFloat?
    var segundoAudio: Double?
    var titulo: String?

    func enlace() -> URL? {
        var componentes = URLComponents()
        componentes.scheme = "tinta"
        componentes.host = "abrir"
        var items = [URLQueryItem(name: "documento", value: String(documentoId))]
        if let elementoId { items.append(URLQueryItem(name: "elemento", value: elementoId)) }
        if let bloqueId { items.append(URLQueryItem(name: "bloque", value: bloqueId.uuidString)) }
        if let x { items.append(URLQueryItem(name: "x", value: String(Double(x)))) }
        if let y { items.append(URLQueryItem(name: "y", value: String(Double(y)))) }
        if let zoom { items.append(URLQueryItem(name: "zoom", value: String(Double(zoom)))) }
        if let segundoAudio { items.append(URLQueryItem(name: "audio", value: String(segundoAudio))) }
        if let titulo { items.append(URLQueryItem(name: "titulo", value: titulo)) }
        componentes.queryItems = items
        return componentes.url
    }

    static func leer(_ url: URL) -> DestinoEnlaceTinta? {
        guard url.scheme?.lowercased() == "tinta", url.host == "abrir",
              let items = URLComponents(url: url, resolvingAgainstBaseURL: false)?.queryItems
        else { return nil }
        func valor(_ nombre: String) -> String? {
            items.first(where: { $0.name == nombre })?.value
        }
        guard let textoDocumento = valor("documento"),
              let documento = UInt64(textoDocumento) else { return nil }
        return DestinoEnlaceTinta(
            documentoId: documento,
            elementoId: valor("elemento"),
            bloqueId: valor("bloque").flatMap { UUID(uuidString: $0) },
            x: valor("x").flatMap { Double($0) }.map { CGFloat($0) },
            y: valor("y").flatMap { Double($0) }.map { CGFloat($0) },
            zoom: valor("zoom").flatMap { Double($0) }.map { CGFloat($0) },
            segundoAudio: valor("audio").flatMap { Double($0) },
            titulo: valor("titulo"))
    }
}

extension Notification.Name {
    static let enlaceProfundoTinta = Notification.Name("Tinta.enlaceProfundo")
    static let operacionColaborativa = Notification.Name("Tinta.operacionColaborativa")
    static let estadoColaboracion = Notification.Name("Tinta.estadoColaboracion")
}

final class CentroEnlacesProfundos {
    static let compartido = CentroEnlacesProfundos()
    private(set) var pendiente: URL?
    private init() {}

    func recibir(_ url: URL) {
        pendiente = url
        NotificationCenter.default.post(name: .enlaceProfundoTinta, object: url)
    }

    func consumirPendiente() -> URL? {
        defer { pendiente = nil }
        return pendiente
    }
}

enum GeneradorQRLocal {
    static func imagen(_ texto: String, lado: CGFloat = 520) -> UIImage? {
        guard let filtro = CIFilter(name: "CIQRCodeGenerator") else { return nil }
        filtro.setValue(Data(texto.utf8), forKey: "inputMessage")
        filtro.setValue("Q", forKey: "inputCorrectionLevel")
        guard let salida = filtro.outputImage else { return nil }
        let escala = max(1, floor(lado / salida.extent.width))
        let transformada = salida.transformed(by: CGAffineTransform(scaleX: escala, y: escala))
        let contexto = CIContext(options: [.useSoftwareRenderer: false])
        guard let cg = contexto.createCGImage(transformada, from: transformada.extent) else {
            return nil
        }
        return UIImage(cgImage: cg)
    }
}

private struct SolicitudConexion: Codable {
    var sesionId: UUID
    var rol: RolColaboracion
    var nonce: UUID
    var firma: Data
}

private struct MensajeFirmadoColaboracion: Codable {
    var cuerpo: Data
    var firma: Data
}

private enum TipoMensajeColaboracion: String, Codable {
    case operaciones
    case solicitarSincronizacion
    case presencia
    case recursoPortapapeles
}

private struct RecursoPortapapelesColaborativo: Codable {
    var nombreTransferencia: String
    var entrada: EntradaPortapapeles
}

private struct MensajeColaboracion: Codable {
    var version = 1
    var sesionId: UUID
    var documentoId: UInt64
    var tipo: TipoMensajeColaboracion
    var operaciones: [OperacionColaborativa] = []
    var recurso: RecursoPortapapelesColaborativo? = nil
    var fecha = Date()
}

final class ServicioColaboracionLocal: NSObject {
    static let compartido = ServicioColaboracionLocal()

    private let tipoServicio = "tinta-colab"
    private let peer = MCPeerID(displayName: UIDevice.current.name)
    private lazy var sesion = MCSession(peer: peer, securityIdentity: nil,
                                        encryptionPreference: .required)
    private var anunciante: MCNearbyServiceAdvertiser?
    private var explorador: MCNearbyServiceBrowser?
    private var rolesRemotos: [MCPeerID: RolColaboracion] = [:]
    private let bloqueo = NSLock()
    private var metadatosRecursos: [String: RecursoPortapapelesColaborativo] = [:]
    private var recursosRecibidos: [String: AdjuntoImportado] = [:]
    private(set) var invitacion: InvitacionColaboracion?
    private(set) var rolLocal: RolColaboracion = .lector
    private(set) var esAnfitrion = false

    override private init() {
        super.init()
        sesion.delegate = self
    }

    var conectados: [MCPeerID] { sesion.connectedPeers }

    @discardableResult
    func crear(documentoId: UInt64,
               duracion: TimeInterval = 4 * 3600) -> InvitacionColaboracion? {
        detener()
        var bytes = [UInt8](repeating: 0, count: 32)
        guard SecRandomCopyBytes(kSecRandomDefault, bytes.count, &bytes) == errSecSuccess
        else { return nil }
        let nueva = InvitacionColaboracion(
            documentoId: documentoId,
            anfitrion: UIDevice.current.name,
            secreto: Data(bytes),
            rolesPermitidos: [.editor, .companero, .lector, .segundaPantalla],
            vence: Date().addingTimeInterval(duracion))
        invitacion = nueva
        rolLocal = .profesor
        esAnfitrion = true
        let anuncio = MCNearbyServiceAdvertiser(
            peer: peer,
            discoveryInfo: ["sid": nueva.sesionId.uuidString,
                            "doc": String(nueva.documentoId)],
            serviceType: tipoServicio)
        anuncio.delegate = self
        anuncio.startAdvertisingPeer()
        anunciante = anuncio
        anunciarEstado()
        return nueva
    }

    func unirse(_ invitacion: InvitacionColaboracion, como rol: RolColaboracion) -> Bool {
        guard !invitacion.expirada, invitacion.rolesPermitidos.contains(rol) else { return false }
        detener()
        self.invitacion = invitacion
        rolLocal = rol
        esAnfitrion = false
        let navegador = MCNearbyServiceBrowser(peer: peer, serviceType: tipoServicio)
        navegador.delegate = self
        navegador.startBrowsingForPeers()
        explorador = navegador
        anunciarEstado()
        return true
    }

    func detener() {
        anunciante?.stopAdvertisingPeer()
        explorador?.stopBrowsingForPeers()
        anunciante = nil
        explorador = nil
        sesion.disconnect()
        bloqueo.lock(); rolesRemotos.removeAll(); bloqueo.unlock()
        invitacion = nil
        rolLocal = .lector
        esAnfitrion = false
        anunciarEstado()
    }

    @discardableResult
    func publicar(tipo: TipoOperacionColaborativa, objetoId: String,
                  carga: Data? = nil, tumba: Bool = false) -> OperacionColaborativa? {
        guard let invitacion, rolLocal.permite(tipo),
              invitacion.documentoId > 0 else { return nil }
        let operacion = OperacionColaborativa(
            sesionId: invitacion.sesionId,
            documentoId: invitacion.documentoId,
            marca: RelojLamportColaboracion.compartido.siguiente(),
            tipo: tipo,
            objetoId: objetoId,
            carga: carga,
            tumba: tumba,
            rolAutor: rolLocal)
        _ = RegistroOperacionesColaborativas.compartido.registrar(operacion)
        enviar(MensajeColaboracion(sesionId: invitacion.sesionId,
                                   documentoId: invitacion.documentoId,
                                   tipo: .operaciones,
                                   operaciones: [operacion]))
        return operacion
    }

    func publicarTrazo(id: String, datos: Data) {
        _ = publicar(tipo: .anadirTrazo, objetoId: id, carga: datos)
    }

    func eliminarTrazo(id: String) {
        _ = publicar(tipo: .eliminarTrazo, objetoId: id, tumba: true)
    }

    func publicarVista(_ vista: EstadoVistaLienzo, documentoId: UInt64) {
        guard invitacion?.documentoId == documentoId,
              let datos = try? JSONEncoder.colaboracion.encode(vista) else { return }
        _ = publicar(tipo: .vistaPresentacion, objetoId: "vista-profesor", carga: datos)
    }

    func compartirPortapapeles(_ entrada: EntradaPortapapeles) {
        if let hash = entrada.hashAdjunto,
           let url = RepositorioAdjuntos.compartido.url(hash: hash),
           let invitacion, !conectados.isEmpty {
            let nombreSeguro = "tinta-\(entrada.id.uuidString)-\(url.lastPathComponent)"
                .replacingOccurrences(of: "/", with: "-")
            let recurso = RecursoPortapapelesColaborativo(
                nombreTransferencia: nombreSeguro, entrada: entrada)
            enviar(MensajeColaboracion(
                sesionId: invitacion.sesionId,
                documentoId: invitacion.documentoId,
                tipo: .recursoPortapapeles,
                recurso: recurso))
            for peer in conectados {
                sesion.sendResource(at: url, withName: nombreSeguro, toPeer: peer) { _ in }
            }
            return
        }
        guard let datos = try? JSONEncoder.colaboracion.encode(entrada) else { return }
        _ = publicar(tipo: .portapapeles, objetoId: entrada.id.uuidString, carga: datos)
    }

    func solicitarSincronizacion() {
        guard let invitacion else { return }
        enviar(MensajeColaboracion(sesionId: invitacion.sesionId,
                                   documentoId: invitacion.documentoId,
                                   tipo: .solicitarSincronizacion))
    }

    private func enviarHistorial(a destinos: [MCPeerID]? = nil) {
        guard let invitacion else { return }
        let operaciones = RegistroOperacionesColaborativas.compartido
            .operaciones(documentoId: invitacion.documentoId)
        for inicio in stride(from: 0, to: operaciones.count, by: 80) {
            let fin = min(inicio + 80, operaciones.count)
            enviar(MensajeColaboracion(
                sesionId: invitacion.sesionId,
                documentoId: invitacion.documentoId,
                tipo: .operaciones,
                operaciones: Array(operaciones[inicio..<fin])), a: destinos)
        }
    }

    private func enviar(_ mensaje: MensajeColaboracion, a destinos: [MCPeerID]? = nil) {
        guard let invitacion,
              let cuerpo = try? JSONEncoder.colaboracion.encode(mensaje) else { return }
        let firmado = MensajeFirmadoColaboracion(
            cuerpo: cuerpo, firma: firma(cuerpo, secreto: invitacion.secreto))
        guard let datos = try? PropertyListEncoder().encode(firmado) else { return }
        let pares = destinos ?? conectados
        guard !pares.isEmpty else { return }
        try? sesion.send(datos, toPeers: pares, with: .reliable)
    }

    private func abrir(_ datos: Data) -> MensajeColaboracion? {
        guard let invitacion,
              let firmado = try? PropertyListDecoder().decode(
                MensajeFirmadoColaboracion.self, from: datos),
              HMAC<SHA256>.isValidAuthenticationCode(
                firmado.firma, authenticating: firmado.cuerpo,
                using: SymmetricKey(data: invitacion.secreto)),
              let mensaje = try? JSONDecoder.colaboracion.decode(
                MensajeColaboracion.self, from: firmado.cuerpo),
              mensaje.version == 1, mensaje.sesionId == invitacion.sesionId,
              mensaje.documentoId == invitacion.documentoId else { return nil }
        return mensaje
    }

    private func firma(_ datos: Data, secreto: Data) -> Data {
        Data(HMAC<SHA256>.authenticationCode(
            for: datos, using: SymmetricKey(data: secreto)))
    }

    private func contextoSolicitud(_ rol: RolColaboracion,
                                   invitacion: InvitacionColaboracion) -> Data? {
        let nonce = UUID()
        let texto = "\(invitacion.sesionId.uuidString)|\(rol.rawValue)|\(nonce.uuidString)"
        let solicitud = SolicitudConexion(
            sesionId: invitacion.sesionId, rol: rol, nonce: nonce,
            firma: firma(Data(texto.utf8), secreto: invitacion.secreto))
        return try? JSONEncoder.colaboracion.encode(solicitud)
    }

    private func validar(_ solicitud: SolicitudConexion,
                         invitacion: InvitacionColaboracion) -> Bool {
        guard solicitud.sesionId == invitacion.sesionId,
              invitacion.rolesPermitidos.contains(solicitud.rol), !invitacion.expirada else {
            return false
        }
        let texto = "\(solicitud.sesionId.uuidString)|\(solicitud.rol.rawValue)|\(solicitud.nonce.uuidString)"
        return HMAC<SHA256>.isValidAuthenticationCode(
            solicitud.firma, authenticating: Data(texto.utf8),
            using: SymmetricKey(data: invitacion.secreto))
    }

    private func rolRemoto(_ peer: MCPeerID) -> RolColaboracion {
        bloqueo.lock(); defer { bloqueo.unlock() }
        if let rol = rolesRemotos[peer] { return rol }
        return .lector
    }

    private func guardarRol(_ rol: RolColaboracion, peer: MCPeerID) {
        bloqueo.lock(); rolesRemotos[peer] = rol; bloqueo.unlock()
    }

    private func anunciarEstado() {
        DispatchQueue.main.async {
            NotificationCenter.default.post(
                name: .estadoColaboracion, object: self,
                userInfo: ["conectados": self.conectados.count,
                           "rol": self.rolLocal.rawValue])
        }
    }
}

extension ServicioColaboracionLocal: MCNearbyServiceAdvertiserDelegate {
    func advertiser(_ advertiser: MCNearbyServiceAdvertiser,
                    didReceiveInvitationFromPeer peerID: MCPeerID,
                    withContext context: Data?,
                    invitationHandler: @escaping (Bool, MCSession?) -> Void) {
        guard esAnfitrion, let invitacion, let context,
              let solicitud = try? JSONDecoder.colaboracion.decode(
                SolicitudConexion.self, from: context),
              validar(solicitud, invitacion: invitacion) else {
            invitationHandler(false, nil); return
        }
        guardarRol(solicitud.rol, peer: peerID)
        invitationHandler(true, sesion)
    }

    func advertiser(_ advertiser: MCNearbyServiceAdvertiser,
                    didNotStartAdvertisingPeer error: Error) { anunciarEstado() }
}

extension ServicioColaboracionLocal: MCNearbyServiceBrowserDelegate {
    func browser(_ browser: MCNearbyServiceBrowser, foundPeer peerID: MCPeerID,
                 withDiscoveryInfo info: [String: String]?) {
        guard let invitacion, info?["sid"] == invitacion.sesionId.uuidString,
              !sesion.connectedPeers.contains(peerID),
              let contexto = contextoSolicitud(rolLocal, invitacion: invitacion) else { return }
        guardarRol(.profesor, peer: peerID)
        browser.invitePeer(peerID, to: sesion, withContext: contexto, timeout: 30)
    }

    func browser(_ browser: MCNearbyServiceBrowser, lostPeer peerID: MCPeerID) {
        anunciarEstado()
    }

    func browser(_ browser: MCNearbyServiceBrowser,
                 didNotStartBrowsingForPeers error: Error) { anunciarEstado() }
}

extension ServicioColaboracionLocal: MCSessionDelegate {
    func session(_ session: MCSession, peer peerID: MCPeerID,
                 didChange state: MCSessionState) {
        anunciarEstado()
        guard state == .connected else { return }
        if esAnfitrion { enviarHistorial(a: [peerID]) }
        else { solicitarSincronizacion() }
    }

    func session(_ session: MCSession, didReceive data: Data,
                 fromPeer peerID: MCPeerID) {
        guard let mensaje = abrir(data) else { return }
        switch mensaje.tipo {
        case .solicitarSincronizacion:
            enviarHistorial(a: [peerID])
        case .presencia:
            break
        case .recursoPortapapeles:
            guard rolRemoto(peerID).permite(.portapapeles),
                  let recurso = mensaje.recurso else { return }
            bloqueo.lock()
            metadatosRecursos[recurso.nombreTransferencia] = recurso
            let adjunto = recursosRecibidos.removeValue(
                forKey: recurso.nombreTransferencia)
            bloqueo.unlock()
            if let adjunto { publicar(recurso: recurso, adjunto: adjunto) }
        case .operaciones:
            let rol = rolRemoto(peerID)
            var aceptadas: [OperacionColaborativa] = []
            for operacion in mensaje.operaciones {
                guard rol.permite(operacion.tipo),
                      (rol == .profesor || operacion.rolAutor == rol),
                      operacion.sesionId == mensaje.sesionId,
                      operacion.documentoId == mensaje.documentoId else { continue }
                _ = RelojLamportColaboracion.compartido.siguiente(
                    observando: operacion.marca.contador)
                let resultado = RegistroOperacionesColaborativas.compartido
                    .registrar(operacion)
                if resultado.nueva && resultado.ganadora { aceptadas.append(operacion) }
            }
            if esAnfitrion, !aceptadas.isEmpty {
                let otros = conectados.filter { $0 != peerID }
                if !otros.isEmpty {
                    enviar(MensajeColaboracion(
                        sesionId: mensaje.sesionId,
                        documentoId: mensaje.documentoId,
                        tipo: .operaciones,
                        operaciones: aceptadas), a: otros)
                }
            }
            for operacion in aceptadas {
                DispatchQueue.main.async {
                    NotificationCenter.default.post(
                        name: .operacionColaborativa, object: operacion)
                }
            }
        }
    }

    func session(_ session: MCSession, didReceive stream: InputStream,
                 withName streamName: String, fromPeer peerID: MCPeerID) {}
    func session(_ session: MCSession,
                 didStartReceivingResourceWithName resourceName: String,
                 fromPeer peerID: MCPeerID, with progress: Progress) {}
    func session(_ session: MCSession,
                 didFinishReceivingResourceWithName resourceName: String,
                 fromPeer peerID: MCPeerID, at localURL: URL?,
                 withError error: Error?) {
        guard error == nil, rolRemoto(peerID).permite(.portapapeles),
              let localURL,
              let adjunto = try? RepositorioAdjuntos.compartido.importar(url: localURL)
        else { return }
        bloqueo.lock()
        let metadatos = metadatosRecursos.removeValue(forKey: resourceName)
        if metadatos == nil { recursosRecibidos[resourceName] = adjunto }
        bloqueo.unlock()
        if let metadatos { publicar(recurso: metadatos, adjunto: adjunto) }
    }

    private func publicar(recurso: RecursoPortapapelesColaborativo,
                          adjunto: AdjuntoImportado) {
        var entrada = recurso.entrada
        entrada.hashAdjunto = adjunto.hash
        entrada.nombre = adjunto.nombreOriginal
        DispatchQueue.main.async {
            HistorialPortapapeles.compartido.agregar(entrada)
        }
    }
}

struct TextoColaborativo: Codable {
    var id: String
    var texto: String
    var marco: CGRect
    var rotacion: CGFloat
    var altoAutomatico: Bool
    var desdeDictado: Bool
}

struct CursorColaborativo: Codable {
    var autor: String
    var nombre: String
    var punto: CGPoint
    var colorARGB: UInt32
    var fecha = Date()
}

final class CoordinadorColaboracionDocumento {
    let servicio = ServicioColaboracionLocal.compartido
    private let sesion: Sesion
    private weak var navegador: VistaLienzoNavegable?
    private weak var autoguardado: CoordinadorAutoguardado?
    private var observador: NSObjectProtocol?
    private var aplicandoRemoto = false
    private var idsTecnicosConocidos: Set<UUID>

    init(sesion: Sesion, navegador: VistaLienzoNavegable,
         autoguardado: CoordinadorAutoguardado) {
        self.sesion = sesion
        self.navegador = navegador
        self.autoguardado = autoguardado
        idsTecnicosConocidos = Set(navegador.capaTecnica.objetos.map(\.id))
        navegador.lienzo.alCrearTrazoColaborativo = { [weak self] id, datos in
            guard self?.servicio.invitacion?.documentoId == sesion.documentoPersistenteId
            else { return }
            self?.servicio.publicarTrazo(id: id, datos: datos)
        }
        navegador.lienzo.alEliminarTrazosColaborativos = { [weak self] ids in
            guard self?.servicio.invitacion?.documentoId == sesion.documentoPersistenteId
            else { return }
            ids.forEach { self?.servicio.eliminarTrazo(id: $0) }
        }
        navegador.capaElementos.alCambiarTexto = { [weak self] texto in
            guard self?.aplicandoRemoto == false else { return }
            self?.publicarTexto(texto)
        }
        navegador.capaElementos.alEliminarTexto = { [weak self] id in
            guard self?.aplicandoRemoto == false else { return }
            _ = self?.servicio.publicar(tipo: .eliminarTexto, objetoId: id, tumba: true)
        }
        navegador.capaBloques.alCambiar = { [weak self] bloque in
            guard self?.aplicandoRemoto == false else { return }
            self?.publicarBloque(bloque)
        }
        navegador.capaBloques.alEliminar = { [weak self] id in
            guard self?.aplicandoRemoto == false else { return }
            _ = self?.servicio.publicar(tipo: .eliminarBloque,
                                        objetoId: id.uuidString, tumba: true)
        }
        navegador.capaTecnica.alCambiar = { [weak self] objetos in
            guard let self, !self.aplicandoRemoto else { return }
            let actuales = Set(objetos.map(\.id))
            for id in self.idsTecnicosConocidos.subtracting(actuales) {
                _ = self.servicio.publicar(tipo: .eliminarTecnico,
                                            objetoId: id.uuidString, tumba: true)
            }
            objetos.forEach(self.publicarTecnico)
            self.idsTecnicosConocidos = actuales
        }
        observador = NotificationCenter.default.addObserver(
            forName: nil, object: nil, queue: .main) { [weak self] nota in
                guard let self else { return }
                if nota.name == .operacionColaborativa {
                    guard let operacion = nota.object as? OperacionColaborativa else { return }
                    self.aplicar(operacion)
                    return
                }
                guard nota.name == CanalMutacionDurableLienzoIPad.nombre,
                      CanalMutacionDurableLienzoIPad.manejadorNucleo(de: nota)
                        == UInt64(self.sesion.manejadorNucleo) else { return }
                self.autoguardado?.marcarCambio()
            }
    }

    func publicarEstadoInicial() {
        guard servicio.invitacion?.documentoId == sesion.documentoPersistenteId else { return }
        for indice in 0..<sesion.cantidadTrazos {
            guard let id = sesion.asegurarIdTrazo(indice),
                  let datos = sesion.serializarTrazo(indice) else { continue }
            servicio.publicarTrazo(id: id, datos: datos)
        }
        sesion.textos.forEach(publicarTexto)
        navegador?.capaBloques.bloques.forEach(publicarBloque)
        navegador?.capaTecnica.objetos.forEach(publicarTecnico)
    }

    func publicarTexto(_ texto: ElementoTexto) {
        let valor = TextoColaborativo(
            id: texto.id, texto: texto.texto, marco: texto.marco,
            rotacion: texto.rotacion, altoAutomatico: texto.altoAutomatico,
            desdeDictado: texto.desdeDictado)
        guard let datos = try? JSONEncoder.colaboracion.encode(valor) else { return }
        _ = servicio.publicar(tipo: .actualizarTexto, objetoId: texto.id, carga: datos)
    }

    func publicarBloque(_ bloque: BloqueLienzo) {
        guard let datos = try? JSONEncoder.colaboracion.encode(bloque) else { return }
        _ = servicio.publicar(tipo: .actualizarBloque,
                              objetoId: bloque.id.uuidString, carga: datos)
    }

    func publicarTecnico(_ objeto: ObjetoTecnico) {
        guard let datos = try? JSONEncoder.colaboracion.encode(objeto) else { return }
        _ = servicio.publicar(tipo: .actualizarTecnico,
                              objetoId: objeto.id.uuidString, carga: datos)
    }

    func publicarVista() {
        guard let navegador else { return }
        servicio.publicarVista(navegador.estadoActual,
                               documentoId: sesion.documentoPersistenteId)
    }

    func enlaceARegionActual(titulo: String = "Región compartida") -> URL? {
        guard let navegador else { return nil }
        let centro = navegador.centroVisibleDocumento
        return DestinoEnlaceTinta(
            documentoId: sesion.documentoPersistenteId,
            x: centro.x, y: centro.y,
            zoom: navegador.estadoActual.zoom,
            titulo: titulo).enlace()
    }

    private func aplicar(_ operacion: OperacionColaborativa) {
        guard operacion.documentoId == sesion.documentoPersistenteId,
              let navegador else { return }
        aplicandoRemoto = true
        defer {
            idsTecnicosConocidos = Set(navegador.capaTecnica.objetos.map(\.id))
            aplicandoRemoto = false
        }
        var cambioPersistente = false
        switch operacion.tipo {
        case .anadirTrazo:
            if let datos = operacion.carga,
               sesion.importarTrazoColaborativo(datos) {
                navegador.lienzo.rehornearYDibujar(); cambioPersistente = true
            }
        case .eliminarTrazo:
            if sesion.eliminarTrazoColaborativo(id: operacion.objetoId) {
                navegador.lienzo.rehornearYDibujar(); cambioPersistente = true
            }
        case .actualizarTexto:
            guard let datos = operacion.carga,
                  let valor = try? JSONDecoder.colaboracion.decode(
                    TextoColaborativo.self, from: datos) else { return }
            if sesion.textos.contains(where: { $0.id == valor.id }) {
                _ = sesion.actualizarTexto(ElementoTexto(
                    id: valor.id, texto: valor.texto, marco: valor.marco,
                    rotacion: valor.rotacion, altoAutomatico: valor.altoAutomatico,
                    desdeDictado: valor.desdeDictado))
            } else {
                _ = sesion.anadirTexto(valor.texto, marco: valor.marco,
                                       dictado: valor.desdeDictado, id: valor.id)
            }
            navegador.recargarElementos(); cambioPersistente = true
        case .eliminarTexto:
            _ = sesion.eliminarTexto(id: operacion.objetoId)
            navegador.recargarElementos(); cambioPersistente = true
        case .actualizarBloque:
            guard let datos = operacion.carga,
                  let bloque = try? JSONDecoder.colaboracion.decode(
                    BloqueLienzo.self, from: datos) else { return }
            var bloques = navegador.capaBloques.bloques
            if let indice = bloques.firstIndex(where: { $0.id == bloque.id }) {
                bloques[indice] = bloque
            } else { bloques.append(bloque) }
            navegador.capaBloques.reemplazar(bloques); cambioPersistente = true
        case .eliminarBloque:
            var bloques = navegador.capaBloques.bloques
            bloques.removeAll { $0.id.uuidString == operacion.objetoId }
            navegador.capaBloques.reemplazar(bloques); cambioPersistente = true
        case .actualizarTecnico:
            guard let datos = operacion.carga,
                  let objeto = try? JSONDecoder.colaboracion.decode(
                    ObjetoTecnico.self, from: datos) else { return }
            if let indice = navegador.capaTecnica.objetos.firstIndex(where: {
                $0.id == objeto.id
            }) {
                navegador.capaTecnica.objetos[indice] = objeto
            } else { navegador.capaTecnica.objetos.append(objeto) }
            cambioPersistente = true
        case .eliminarTecnico:
            navegador.capaTecnica.objetos.removeAll {
                $0.id.uuidString == operacion.objetoId
            }
            cambioPersistente = true
        case .vistaPresentacion:
            guard servicio.rolLocal == .segundaPantalla,
                  let datos = operacion.carga,
                  let vista = try? JSONDecoder.colaboracion.decode(
                    EstadoVistaLienzo.self, from: datos) else { return }
            navegador.restaurar(vista, animado: true)
        case .portapapeles:
            guard let datos = operacion.carga,
                  let entrada = try? JSONDecoder.colaboracion.decode(
                    EntradaPortapapeles.self, from: datos) else { return }
            HistorialPortapapeles.compartido.agregar(entrada)
        case .cursor, .marcador, .controlSesion:
            break
        }
        if cambioPersistente { autoguardado?.marcarCambio() }
    }

    deinit {
        if let observador { NotificationCenter.default.removeObserver(observador) }
    }
}

final class PanelColaboracionLocal: UITableViewController {
    private let coordinador: CoordinadorColaboracionDocumento
    private let documentoId: UInt64
    private var observador: NSObjectProtocol?

    init(coordinador: CoordinadorColaboracionDocumento, documentoId: UInt64) {
        self.coordinador = coordinador
        self.documentoId = documentoId
        super.init(style: .insetGrouped)
    }

    required init?(coder: NSCoder) { fatalError("init(coder:) no está implementado") }

    override func viewDidLoad() {
        super.viewDidLoad()
        title = "Colaboración local cifrada"
        navigationItem.leftBarButtonItem = UIBarButtonItem(
            barButtonSystemItem: .close, target: self, action: #selector(cerrar))
        observador = NotificationCenter.default.addObserver(
            forName: .estadoColaboracion, object: nil, queue: .main) {
                [weak self] _ in self?.tableView.reloadData()
            }
    }

    override func numberOfSections(in tableView: UITableView) -> Int { 3 }

    override func tableView(_ tableView: UITableView,
                            titleForHeaderInSection section: Int) -> String? {
        ["Sesión", "Compartir y presentar", "Pares conectados"][section]
    }

    override func tableView(_ tableView: UITableView,
                            numberOfRowsInSection section: Int) -> Int {
        switch section {
        case 0: return 4
        case 1: return 5
        default: return max(coordinador.servicio.conectados.count, 1)
        }
    }

    override func tableView(_ tableView: UITableView,
                            cellForRowAt indexPath: IndexPath) -> UITableViewCell {
        let celda = UITableViewCell(style: .subtitle, reuseIdentifier: nil)
        switch (indexPath.section, indexPath.row) {
        case (0, 0):
            celda.textLabel?.text = "Crear sesión en este iPad"
            celda.detailTextLabel?.text = "Cifrado obligatorio, sin nube ni cuenta"
        case (0, 1):
            celda.textLabel?.text = "Unirse pegando invitación"
        case (0, 2):
            celda.textLabel?.text = "Escanear QR"
        case (0, 3):
            celda.textLabel?.text = "Cerrar sesión local"
            celda.textLabel?.textColor = .systemRed
        case (1, 0):
            celda.textLabel?.text = "Mostrar QR de invitación"
        case (1, 1):
            celda.textLabel?.text = "Compartir enlace de invitación"
        case (1, 2):
            celda.textLabel?.text = "Enviar vista a segunda pantalla"
        case (1, 3):
            celda.textLabel?.text = "Compartir portapapeles actual"
        case (1, 4):
            celda.textLabel?.text = "QR a la región visible del apunte"
            celda.detailTextLabel?.text = "Enlace profundo, sin copiar el cuaderno"
        default:
            let pares = coordinador.servicio.conectados
            if pares.isEmpty {
                celda.textLabel?.text = "Sin pares conectados"
                celda.detailTextLabel?.text = estadoTexto
            } else {
                celda.textLabel?.text = pares[indexPath.row].displayName
                celda.detailTextLabel?.text = "Conexión local cifrada"
                celda.imageView?.image = UIImage(systemName: "lock.shield")
            }
        }
        return celda
    }

    private var estadoTexto: String {
        guard let invitacion = coordinador.servicio.invitacion else {
            return "No hay una sesión activa."
        }
        return "\(coordinador.servicio.rolLocal.rawValue) · vence \(invitacion.vence.formatted(date: .omitted, time: .shortened))"
    }

    override func tableView(_ tableView: UITableView, didSelectRowAt indexPath: IndexPath) {
        tableView.deselectRow(at: indexPath, animated: true)
        switch (indexPath.section, indexPath.row) {
        case (0, 0): crear()
        case (0, 1): pedirEnlace()
        case (0, 2): escanear()
        case (0, 3): coordinador.servicio.detener(); tableView.reloadData()
        case (1, 0): mostrarQR()
        case (1, 1): compartirInvitacion()
        case (1, 2): coordinador.publicarVista()
        case (1, 3): compartirPortapapeles()
        case (1, 4): compartirRegionActual()
        default: break
        }
    }

    private func crear() {
        guard let invitacion = coordinador.servicio.crear(documentoId: documentoId) else {
            avisar("No se pudo generar el secreto criptográfico local."); return
        }
        coordinador.publicarEstadoInicial()
        tableView.reloadData()
        if let enlace = invitacion.enlace() { mostrarQR(enlace) }
    }

    private func pedirEnlace() {
        let alerta = UIAlertController(title: "Pegar invitación",
                                       message: "El enlace empieza con tinta://colaborar",
                                       preferredStyle: .alert)
        alerta.addTextField { $0.autocapitalizationType = .none }
        alerta.addAction(UIAlertAction(title: "Continuar", style: .default) {
            [weak self] _ in
            guard let texto = alerta.textFields?.first?.text,
                  let url = URL(string: texto),
                  let invitacion = InvitacionColaboracion.leer(url) else {
                self?.avisar("La invitación es inválida o ya venció."); return
            }
            self?.elegirRol(invitacion)
        })
        alerta.addAction(UIAlertAction(title: "Cancelar", style: .cancel))
        present(alerta, animated: true)
    }

    func ofrecer(_ invitacion: InvitacionColaboracion) {
        loadViewIfNeeded()
        elegirRol(invitacion)
    }

    private func elegirRol(_ invitacion: InvitacionColaboracion) {
        guard invitacion.documentoId == documentoId else {
            avisar("La invitación pertenece a otro cuaderno abierto."); return
        }
        let hoja = UIAlertController(title: "Rol en la sesión", message: nil,
                                     preferredStyle: .actionSheet)
        for rol in invitacion.rolesPermitidos {
            hoja.addAction(UIAlertAction(title: rol.rawValue, style: .default) {
                [weak self] _ in
                guard let self else { return }
                if !self.coordinador.servicio.unirse(invitacion, como: rol) {
                    self.avisar("No se pudo iniciar la búsqueda local.")
                }
                self.tableView.reloadData()
            })
        }
        hoja.addAction(UIAlertAction(title: "Cancelar", style: .cancel))
        hoja.popoverPresentationController?.sourceView = view
        present(hoja, animated: true)
    }

    private func escanear() {
        let escaner = ControladorEscanerQR()
        escaner.alLeer = { [weak self, weak escaner] texto in
            escaner?.dismiss(animated: true)
            guard let url = URL(string: texto),
                  let invitacion = InvitacionColaboracion.leer(url) else {
                self?.avisar("El QR no contiene una invitación Tinta válida."); return
            }
            self?.elegirRol(invitacion)
        }
        present(UINavigationController(rootViewController: escaner), animated: true)
    }

    private func mostrarQR() {
        guard let enlace = coordinador.servicio.invitacion?.enlace() else {
            avisar("Primero cree una sesión."); return
        }
        mostrarQR(enlace)
    }

    private func mostrarQR(_ enlace: URL) {
        let controlador = ControladorMostrarQR(enlace: enlace)
        present(UINavigationController(rootViewController: controlador), animated: true)
    }

    private func compartirInvitacion() {
        guard let enlace = coordinador.servicio.invitacion?.enlace() else {
            avisar("Primero cree una sesión."); return
        }
        let compartir = UIActivityViewController(activityItems: [enlace],
                                                 applicationActivities: nil)
        compartir.popoverPresentationController?.sourceView = view
        present(compartir, animated: true)
    }

    private func compartirPortapapeles() {
        guard let entrada = HistorialPortapapeles.compartido.capturarActual() else {
            avisar("El portapapeles actual no contiene texto, enlace, archivo ni imagen.")
            return
        }
        coordinador.servicio.compartirPortapapeles(entrada)
    }

    private func compartirRegionActual() {
        guard let enlace = coordinador.enlaceARegionActual() else { return }
        mostrarQR(enlace)
    }

    private func avisar(_ texto: String) {
        let alerta = UIAlertController(title: nil, message: texto, preferredStyle: .alert)
        alerta.addAction(UIAlertAction(title: "Aceptar", style: .default))
        present(alerta, animated: true)
    }

    @objc private func cerrar() { dismiss(animated: true) }

    deinit {
        if let observador { NotificationCenter.default.removeObserver(observador) }
    }
}

final class ControladorMostrarQR: UIViewController {
    private let enlace: URL
    init(enlace: URL) { self.enlace = enlace; super.init(nibName: nil, bundle: nil) }
    required init?(coder: NSCoder) { fatalError("init(coder:) no está implementado") }

    override func viewDidLoad() {
        super.viewDidLoad()
        title = "Invitación local"
        view.backgroundColor = .systemBackground
        navigationItem.leftBarButtonItem = UIBarButtonItem(
            barButtonSystemItem: .close, target: self, action: #selector(cerrar))
        let imagen = UIImageView(image: GeneradorQRLocal.imagen(enlace.absoluteString))
        imagen.contentMode = .scaleAspectFit
        let aviso = UILabel()
        aviso.text = "Quien escanee este QR recibe acceso temporal a la sesión local. No lo publique."
        aviso.numberOfLines = 0; aviso.textAlignment = .center
        let pila = UIStackView(arrangedSubviews: [imagen, aviso])
        pila.axis = .vertical; pila.spacing = 20; pila.translatesAutoresizingMaskIntoConstraints = false
        view.addSubview(pila)
        NSLayoutConstraint.activate([
            pila.centerXAnchor.constraint(equalTo: view.centerXAnchor),
            pila.centerYAnchor.constraint(equalTo: view.centerYAnchor),
            pila.widthAnchor.constraint(lessThanOrEqualTo: view.widthAnchor, multiplier: 0.82),
            imagen.widthAnchor.constraint(equalTo: imagen.heightAnchor),
            imagen.heightAnchor.constraint(lessThanOrEqualToConstant: 520),
        ])
    }
    @objc private func cerrar() { dismiss(animated: true) }
}

final class ControladorEscanerQR: UIViewController, AVCaptureMetadataOutputObjectsDelegate {
    var alLeer: ((String) -> Void)?
    private let captura = AVCaptureSession()
    private var previa: AVCaptureVideoPreviewLayer?

    override func viewDidLoad() {
        super.viewDidLoad()
        title = "Escanear invitación"
        view.backgroundColor = .black
        navigationItem.leftBarButtonItem = UIBarButtonItem(
            barButtonSystemItem: .cancel, target: self, action: #selector(cerrar))
        AVCaptureDevice.requestAccess(for: .video) { [weak self] concedido in
            DispatchQueue.main.async {
                if concedido { self?.configurar() }
                else { self?.mostrarSinCamara() }
            }
        }
    }

    override func viewDidLayoutSubviews() {
        super.viewDidLayoutSubviews(); previa?.frame = view.bounds
    }

    override func viewWillDisappear(_ animated: Bool) {
        super.viewWillDisappear(animated)
        if captura.isRunning {
            DispatchQueue.global(qos: .utility).async { [captura] in captura.stopRunning() }
        }
    }

    private func configurar() {
        guard let camara = AVCaptureDevice.default(for: .video),
              let entrada = try? AVCaptureDeviceInput(device: camara),
              captura.canAddInput(entrada) else { mostrarSinCamara(); return }
        captura.addInput(entrada)
        let salida = AVCaptureMetadataOutput()
        guard captura.canAddOutput(salida) else { mostrarSinCamara(); return }
        captura.addOutput(salida)
        salida.setMetadataObjectsDelegate(self, queue: .main)
        salida.metadataObjectTypes = [.qr]
        let capa = AVCaptureVideoPreviewLayer(session: captura)
        capa.videoGravity = .resizeAspectFill
        view.layer.insertSublayer(capa, at: 0); previa = capa
        DispatchQueue.global(qos: .userInitiated).async { [weak self] in
            self?.captura.startRunning()
        }
    }

    private func mostrarSinCamara() {
        let etiqueta = UILabel(frame: view.bounds.insetBy(dx: 30, dy: 30))
        etiqueta.autoresizingMask = [.flexibleWidth, .flexibleHeight]
        etiqueta.text = "La cámara no está disponible. Puede pegar el enlace de invitación."
        etiqueta.textColor = .white; etiqueta.numberOfLines = 0
        etiqueta.textAlignment = .center; view.addSubview(etiqueta)
    }

    func metadataOutput(_ output: AVCaptureMetadataOutput,
                        didOutput metadataObjects: [AVMetadataObject],
                        from connection: AVCaptureConnection) {
        guard let codigo = metadataObjects.first as? AVMetadataMachineReadableCodeObject,
              let texto = codigo.stringValue else { return }
        captura.stopRunning(); alLeer?(texto)
    }

    @objc private func cerrar() { dismiss(animated: true) }
}

private extension JSONEncoder {
    static var colaboracion: JSONEncoder {
        let encoder = JSONEncoder()
        encoder.dateEncodingStrategy = .iso8601
        encoder.outputFormatting = [.sortedKeys]
        return encoder
    }
}

private extension JSONDecoder {
    static var colaboracion: JSONDecoder {
        let decoder = JSONDecoder()
        decoder.dateDecodingStrategy = .iso8601
        return decoder
    }
}
