import CryptoKit
import Foundation

struct EntradaRevisionGuardadoIPad: Codable, Equatable {
    var destinoPath: String
    var presente: Bool
    var bytes: Int64
    var sha256: String?
}

struct RevisionGuardadoDocumentoIPad: Codable, Equatable {
    var version = 1
    var revisionId: UUID
    var documentoId: DocumentoPersistenteID
    var revisionAnteriorId: UUID?
    var hashManifestAnterior: String?
    var fecha: Date
    var entradas: [EntradaRevisionGuardadoIPad]
}

struct InconsistenciaRevisionDocumentoIPad: Equatable {
    enum Tipo: Equatable {
        case faltante
        case presentePeroDebiaEstarBorrado
        case tamanoDistinto
        case hashDistinto
    }

    var destinoPath: String
    var tipo: Tipo
}

struct ReporteRevisionDocumentoIPad: Equatable {
    var documentoId: DocumentoPersistenteID
    var revisionId: UUID
    var archivosVerificados: Int
    var inconsistencias: [InconsistenciaRevisionDocumentoIPad]
    var integra: Bool { inconsistencias.isEmpty }
}

enum ErrorRevisionGuardadoDocumentoIPad: LocalizedError, Equatable {
    case manifestNoExiste
    case manifestCorrupto
    case documentoDistinto
    case manifestIncluidoComoDestino

    var errorDescription: String? {
        switch self {
        case .manifestNoExiste:
            return "No existe una revisión de guardado para este documento."
        case .manifestCorrupto:
            return "El manifest de revisión no puede validarse."
        case .documentoDistinto:
            return "El manifest pertenece a otro documento."
        case .manifestIncluidoComoDestino:
            return "Un participante intentó escribir directamente el manifest de revisión reservado."
        }
    }
}

struct PreparacionRevisionGuardadoIPad {
    var revision: RevisionGuardadoDocumentoIPad
    var cambioManifest: CambioArchivoTransaccionalIPad
}

/// Manifest acumulativo de la última revisión conocida. Las entradas no tocadas
/// se heredan de la revisión anterior, de modo que la verificación de reopen no
/// se limita sólo a los sidecars modificados en el último save.
final class RepositorioRevisionesGuardadoIPad {
    static let compartido = RepositorioRevisionesGuardadoIPad()
    private let base: URL
    private let archivos: FileManager

    init(base: URL? = nil, archivos: FileManager = .default) {
        self.archivos = archivos
        self.base = base ?? archivos.urls(for: .applicationSupportDirectory,
                                           in: .userDomainMask)[0]
            .appendingPathComponent("Persistencia/Revisiones", isDirectory: true)
        try? archivos.createDirectory(at: self.base, withIntermediateDirectories: true)
    }

    func preparar(documentoId: DocumentoPersistenteID,
                  cambios: [CambioArchivoTransaccionalIPad]) throws
        -> PreparacionRevisionGuardadoIPad {
        let manifestURL = url(documentoId)
        guard !cambios.contains(where: {
            $0.destino.standardizedFileURL == manifestURL.standardizedFileURL
        }) else {
            throw ErrorRevisionGuardadoDocumentoIPad.manifestIncluidoComoDestino
        }

        let anteriorDatos = archivos.fileExists(atPath: manifestURL.path)
            ? try Data(contentsOf: manifestURL, options: [.mappedIfSafe])
            : nil
        let anterior: RevisionGuardadoDocumentoIPad?
        if let anteriorDatos {
            let decoder = JSONDecoder(); decoder.dateDecodingStrategy = .iso8601
            guard let decodificada = try? decoder.decode(
                RevisionGuardadoDocumentoIPad.self, from: anteriorDatos),
                  decodificada.documentoId == documentoId else {
                throw ErrorRevisionGuardadoDocumentoIPad.manifestCorrupto
            }
            anterior = decodificada
        } else {
            anterior = nil
        }

        var porRuta = Dictionary(uniqueKeysWithValues:
            (anterior?.entradas ?? []).map { ($0.destinoPath, $0) })
        for cambio in cambios {
            let ruta = cambio.destino.standardizedFileURL.path
            switch cambio.mutacion {
            case .escribir(let datos):
                porRuta[ruta] = .init(
                    destinoPath: ruta,
                    presente: true,
                    bytes: Int64(datos.count),
                    sha256: Self.sha256(datos))
            case .eliminar:
                porRuta[ruta] = .init(
                    destinoPath: ruta,
                    presente: false,
                    bytes: 0,
                    sha256: nil)
            }
        }

        let revision = RevisionGuardadoDocumentoIPad(
            revisionId: UUID(),
            documentoId: documentoId,
            revisionAnteriorId: anterior?.revisionId,
            hashManifestAnterior: anteriorDatos.map(Self.sha256),
            fecha: Date(),
            entradas: porRuta.values.sorted { $0.destinoPath < $1.destinoPath })
        let encoder = JSONEncoder(); encoder.dateEncodingStrategy = .iso8601
        encoder.outputFormatting = [.prettyPrinted, .sortedKeys]
        let nuevosDatos = try encoder.encode(revision)
        let precondicion: PrecondicionArchivoTransaccionalIPad = anteriorDatos
            .map { .sha256(Self.sha256($0)) } ?? .debeNoExistir

        return .init(
            revision: revision,
            cambioManifest: .init(
                destino: manifestURL,
                mutacion: .escribir(nuevosDatos),
                precondicion: precondicion))
    }

    func cargar(_ documentoId: DocumentoPersistenteID) throws
        -> RevisionGuardadoDocumentoIPad {
        let ruta = url(documentoId)
        guard archivos.fileExists(atPath: ruta.path) else {
            throw ErrorRevisionGuardadoDocumentoIPad.manifestNoExiste
        }
        let decoder = JSONDecoder(); decoder.dateDecodingStrategy = .iso8601
        guard let revision = try? decoder.decode(
            RevisionGuardadoDocumentoIPad.self,
            from: Data(contentsOf: ruta)),
              revision.documentoId == documentoId else {
            throw ErrorRevisionGuardadoDocumentoIPad.manifestCorrupto
        }
        return revision
    }

    func verificar(_ documentoId: DocumentoPersistenteID) throws
        -> ReporteRevisionDocumentoIPad {
        let revision = try cargar(documentoId)
        var inconsistencias: [InconsistenciaRevisionDocumentoIPad] = []
        var verificados = 0

        for entrada in revision.entradas {
            let destino = URL(fileURLWithPath: entrada.destinoPath)
            let existe = archivos.fileExists(atPath: destino.path)
            if !entrada.presente {
                if existe {
                    inconsistencias.append(.init(
                        destinoPath: entrada.destinoPath,
                        tipo: .presentePeroDebiaEstarBorrado))
                }
                verificados += 1
                continue
            }

            guard existe else {
                inconsistencias.append(.init(
                    destinoPath: entrada.destinoPath,
                    tipo: .faltante))
                continue
            }
            let datos = try Data(contentsOf: destino, options: [.mappedIfSafe])
            if Int64(datos.count) != entrada.bytes {
                inconsistencias.append(.init(
                    destinoPath: entrada.destinoPath,
                    tipo: .tamanoDistinto))
            } else if Self.sha256(datos) != entrada.sha256 {
                inconsistencias.append(.init(
                    destinoPath: entrada.destinoPath,
                    tipo: .hashDistinto))
            }
            verificados += 1
        }

        return .init(
            documentoId: documentoId,
            revisionId: revision.revisionId,
            archivosVerificados: verificados,
            inconsistencias: inconsistencias)
    }

    func url(_ documentoId: DocumentoPersistenteID) -> URL {
        base.appendingPathComponent("\(documentoId.description).json")
    }

    private static func sha256(_ datos: Data) -> String {
        SHA256.hash(data: datos).map { String(format: "%02x", $0) }.joined()
    }
}
