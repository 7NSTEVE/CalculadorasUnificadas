import Foundation
import UIKit

enum ClaseAnchoCuadernUIPad: String, Codable { case compacta, media, expandida }
enum PanelLateralCuadernUIPad: String, Codable { case ninguno, biblioteca, herramientas, estudio, comentarios, busqueda }

struct DisposicionAdaptativaCuadernUIPad: Equatable {
    var claseAncho: ClaseAnchoCuadernUIPad
    var columnas: Int
    var mostrarEtiquetasToolbar: Bool
    var permitePanelLateralFijo: Bool
    var anchoPanelSugerido: CGFloat
    var controlesCompactos: Bool
}

enum PoliticaDisposicionCuadernUIPad {
    static func calcular(tamano: CGSize, tecladoVisible: Bool = false) -> DisposicionAdaptativaCuadernUIPad {
        let ancho = max(tamano.width, 0)
        let clase: ClaseAnchoCuadernUIPad = ancho < 600 ? .compacta : (ancho < 980 ? .media : .expandida)
        switch clase {
        case .compacta:
            return .init(claseAncho: clase, columnas: 1,
                         mostrarEtiquetasToolbar: false,
                         permitePanelLateralFijo: false,
                         anchoPanelSugerido: min(320, ancho), controlesCompactos: true)
        case .media:
            return .init(claseAncho: clase, columnas: tecladoVisible ? 1 : 2,
                         mostrarEtiquetasToolbar: !tecladoVisible,
                         permitePanelLateralFijo: !tecladoVisible,
                         anchoPanelSugerido: 300, controlesCompactos: tecladoVisible)
        case .expandida:
            return .init(claseAncho: clase, columnas: tecladoVisible ? 2 : 3,
                         mostrarEtiquetasToolbar: true,
                         permitePanelLateralFijo: true,
                         anchoPanelSugerido: 340, controlesCompactos: false)
        }
    }
}

struct EstadoEscenaCuadernUIPad: Codable, Equatable, Identifiable {
    var id: String { escenaId }
    var escenaId: String
    var cuadernoId: String?
    var documentoId: DocumentoPersistenteID?
    var pestanaSeleccionadaId: UUID?
    var panelLateral: PanelLateralCuadernUIPad = .ninguno
    var paginaVisible: Int?
    var frameVisibleId: UUID?
    var escalaLienzo: Double = 1
    var desplazamientoX: Double = 0
    var desplazamientoY: Double = 0
    /// Estado del split principal/referencia. Es opcional para que los archivos
    /// v1 existentes sigan decodificando sin migración destructiva.
    var referenciaVisible: Bool?
    var actualizado = Date()

    mutating func normalizar() {
        paginaVisible = paginaVisible.map { max($0, 0) }
        escalaLienzo = min(max(escalaLienzo.isFinite ? escalaLienzo : 1, 0.05), 20)
        if !desplazamientoX.isFinite { desplazamientoX = 0 }
        if !desplazamientoY.isFinite { desplazamientoY = 0 }
        actualizado = Date()
    }

    /// Vacía únicamente el contexto documental de esta ventana. La identidad de
    /// la UIScene se conserva para que cerrar un cuaderno no borre el estado de
    /// otra ventana ni requiera crear un registro nuevo.
    mutating func cerrarDocumento() {
        cuadernoId = nil
        documentoId = nil
        pestanaSeleccionadaId = nil
        panelLateral = .ninguno
        paginaVisible = nil
        frameVisibleId = nil
        escalaLienzo = 1
        desplazamientoX = 0
        desplazamientoY = 0
        referenciaVisible = nil
    }
}

private struct ArchivoEscenasCuadernUIPad: Codable {
    var version = 1
    var escenas: [EstadoEscenaCuadernUIPad] = []
}

final class RepositorioEstadoEscenasCuadernUIPad {
    static let compartido = RepositorioEstadoEscenasCuadernUIPad()
    private let bloqueo = NSLock(); private let url: URL
    private var archivo: ArchivoEscenasCuadernUIPad

    init(base: URL? = nil, archivos: FileManager = .default) {
        let carpeta = base ?? archivos.urls(for: .applicationSupportDirectory, in: .userDomainMask)[0]
            .appendingPathComponent("Escenas", isDirectory: true)
        try? archivos.createDirectory(at: carpeta, withIntermediateDirectories: true)
        url = carpeta.appendingPathComponent("estado-v1.json")
        let decoder = JSONDecoder(); decoder.dateDecodingStrategy = .iso8601
        if let datos = try? Data(contentsOf: url),
           let leido = try? decoder.decode(ArchivoEscenasCuadernUIPad.self, from: datos) {
            archivo = leido
        } else { archivo = ArchivoEscenasCuadernUIPad() }
    }

    func estado(escenaId: String) -> EstadoEscenaCuadernUIPad {
        bloqueo.lock(); defer { bloqueo.unlock() }
        return archivo.escenas.first { $0.escenaId == escenaId }
            ?? EstadoEscenaCuadernUIPad(escenaId: escenaId)
    }

    @discardableResult
    func guardar(_ estado: EstadoEscenaCuadernUIPad) -> Bool {
        return actualizar(escenaId: estado.escenaId) { $0 = estado }
    }

    /// Read-modify-write bajo un único lock y copy-on-write. El estado en memoria
    /// sólo avanza cuando el JSON durable ya fue reemplazado atómicamente.
    @discardableResult
    func actualizar(
        escenaId: String,
        _ cambio: (inout EstadoEscenaCuadernUIPad) -> Void
    ) -> Bool {
        bloqueo.lock(); defer { bloqueo.unlock() }
        let limpio = escenaId.trimmingCharacters(in: .whitespacesAndNewlines)
        guard !limpio.isEmpty else { return false }
        var nuevo = archivo.escenas.first { $0.escenaId == escenaId }
            ?? EstadoEscenaCuadernUIPad(escenaId: escenaId)
        cambio(&nuevo)
        guard nuevo.escenaId == escenaId else { return false }
        nuevo.normalizar()

        var candidato = archivo
        if let i = candidato.escenas.firstIndex(where: { $0.escenaId == escenaId }) {
            candidato.escenas[i] = nuevo
        } else {
            candidato.escenas.append(nuevo)
        }
        guard persistirSinBloqueo(candidato) else { return false }
        archivo = candidato
        return true
    }

    @discardableResult
    func eliminar(escenaId: String) -> Bool {
        bloqueo.lock(); defer { bloqueo.unlock() }
        var candidato = archivo
        candidato.escenas.removeAll { $0.escenaId == escenaId }
        guard persistirSinBloqueo(candidato) else { return false }
        archivo = candidato
        return true
    }

    @discardableResult
    func purgar(anterioresA fecha: Date) -> Bool {
        bloqueo.lock(); defer { bloqueo.unlock() }
        var candidato = archivo
        candidato.escenas.removeAll { $0.actualizado < fecha }
        guard persistirSinBloqueo(candidato) else { return false }
        archivo = candidato
        return true
    }

    private func persistirSinBloqueo(_ candidato: ArchivoEscenasCuadernUIPad) -> Bool {
        let encoder = JSONEncoder(); encoder.dateEncodingStrategy = .iso8601; encoder.outputFormatting = [.sortedKeys]
        guard let datos = try? encoder.encode(candidato) else { return false }
        do { try datos.write(to: url, options: [.atomic, .completeFileProtection]); return true }
        catch { return false }
    }
}

/// Adaptador pequeño para que SceneDelegate/AppPrincipal sólo tenga que pasar el
/// persistentIdentifier de UISceneSession. No conserva UIView ni controladores.
final class SesionEscenaCuadernUIPad {
    let escenaId: String
    private let repo: RepositorioEstadoEscenasCuadernUIPad

    init(session: UISceneSession,
         repo: RepositorioEstadoEscenasCuadernUIPad = .compartido) {
        escenaId = session.persistentIdentifier; self.repo = repo
    }

    /// Entrada determinista para pruebas y coordinadores que ya poseen el
    /// persistentIdentifier y no necesitan retener UISceneSession.
    init(escenaId: String,
         repo: RepositorioEstadoEscenasCuadernUIPad = .compartido) {
        self.escenaId = escenaId
        self.repo = repo
    }

    var estado: EstadoEscenaCuadernUIPad { repo.estado(escenaId: escenaId) }
    func guardar(_ estado: EstadoEscenaCuadernUIPad) -> Bool {
        guard estado.escenaId == escenaId else { return false }
        return repo.guardar(estado)
    }
    @discardableResult
    func actualizar(_ cambio: (inout EstadoEscenaCuadernUIPad) -> Void) -> Bool {
        return repo.actualizar(escenaId: escenaId, cambio)
    }
    func descartar() { _ = repo.eliminar(escenaId: escenaId) }
}
