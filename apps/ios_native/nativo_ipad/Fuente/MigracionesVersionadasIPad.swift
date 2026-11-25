import CryptoKit
import Foundation

struct MigracionDatosVersionadaIPad {
    var identificador: String
    var desdeVersion: Int
    var hastaVersion: Int
    var transformar: (Data) throws -> Data

    init(identificador: String,
         desdeVersion: Int,
         hastaVersion: Int,
         transformar: @escaping (Data) throws -> Data) {
        self.identificador = identificador
        self.desdeVersion = desdeVersion
        self.hastaVersion = hastaVersion
        self.transformar = transformar
    }
}

struct PasoMigracionAplicadoIPad: Equatable {
    var identificador: String
    var desdeVersion: Int
    var hastaVersion: Int
    var sha256Antes: String
    var sha256Despues: String
    var bytesAntes: Int
    var bytesDespues: Int
}

struct ResultadoTransformacionVersionadaIPad {
    var datos: Data
    var versionFinal: Int
    var pasos: [PasoMigracionAplicadoIPad]
}

enum ErrorMigracionVersionadaIPad: LocalizedError, Equatable {
    case versionInvalida(Int)
    case destinoAnterior(actual: Int, destino: Int)
    case saltoFaltante(Int)
    case pasoNoAvanza(String)
    case pasosDuplicados(Int)
    case cicloDetectado
    case salidaDemasiadoGrande(Int)
    case archivoAusente(String)

    var errorDescription: String? {
        switch self {
        case .versionInvalida(let v): return "Versión de formato inválida: \(v)."
        case .destinoAnterior(let actual, let destino):
            return "No se hará downgrade automático de \(actual) a \(destino)."
        case .saltoFaltante(let v): return "Falta una migración que parta de la versión \(v)."
        case .pasoNoAvanza(let id): return "La migración \(id) no incrementa la versión."
        case .pasosDuplicados(let v): return "Hay más de una migración candidata desde la versión \(v)."
        case .cicloDetectado: return "La cadena de migraciones contiene un ciclo."
        case .salidaDemasiadoGrande(let bytes): return "La migración produjo \(bytes) bytes y excede el límite preventivo."
        case .archivoAusente(let ruta): return "No existe el archivo que se intentó migrar: \(ruta)."
        }
    }
}

enum MotorMigracionesVersionadasIPad {
    static let maximoSalidaPredeterminado = 256 * 1_024 * 1_024

    static func aplicar(datos iniciales: Data,
                        versionActual: Int,
                        versionDestino: Int,
                        migraciones: [MigracionDatosVersionadaIPad],
                        maximoSalida: Int = maximoSalidaPredeterminado) throws
        -> ResultadoTransformacionVersionadaIPad {
        guard versionActual >= 0, versionDestino >= 0 else {
            throw ErrorMigracionVersionadaIPad.versionInvalida(
                min(versionActual, versionDestino))
        }
        guard versionDestino >= versionActual else {
            throw ErrorMigracionVersionadaIPad.destinoAnterior(
                actual: versionActual, destino: versionDestino)
        }
        guard versionDestino != versionActual else {
            return .init(datos: iniciales,
                         versionFinal: versionActual,
                         pasos: [])
        }

        let agrupadas = Dictionary(grouping: migraciones, by: \.desdeVersion)
        if let duplicada = agrupadas.first(where: { $0.value.count > 1 })?.key {
            throw ErrorMigracionVersionadaIPad.pasosDuplicados(duplicada)
        }

        var datos = iniciales
        var version = versionActual
        var vistos = Set<Int>()
        var pasos: [PasoMigracionAplicadoIPad] = []

        while version < versionDestino {
            guard vistos.insert(version).inserted else {
                throw ErrorMigracionVersionadaIPad.cicloDetectado
            }
            guard let migracion = agrupadas[version]?.first else {
                throw ErrorMigracionVersionadaIPad.saltoFaltante(version)
            }
            guard migracion.hastaVersion > migracion.desdeVersion,
                  migracion.hastaVersion <= versionDestino else {
                throw ErrorMigracionVersionadaIPad.pasoNoAvanza(
                    migracion.identificador)
            }

            let antes = datos
            let despues = try migracion.transformar(antes)
            guard despues.count <= max(maximoSalida, 1) else {
                throw ErrorMigracionVersionadaIPad.salidaDemasiadoGrande(
                    despues.count)
            }
            pasos.append(.init(
                identificador: migracion.identificador,
                desdeVersion: migracion.desdeVersion,
                hastaVersion: migracion.hastaVersion,
                sha256Antes: sha256(antes),
                sha256Despues: sha256(despues),
                bytesAntes: antes.count,
                bytesDespues: despues.count))
            datos = despues
            version = migracion.hastaVersion
        }

        guard version == versionDestino else {
            throw ErrorMigracionVersionadaIPad.saltoFaltante(version)
        }
        return .init(datos: datos, versionFinal: version, pasos: pasos)
    }

    private static func sha256(_ datos: Data) -> String {
        SHA256.hash(data: datos).map { String(format: "%02x", $0) }.joined()
    }
}

struct SolicitudMigracionArchivoVersionadoIPad {
    var nombre: String
    var archivo: URL
    var versionActual: Int
    var versionDestino: Int
    var migraciones: [MigracionDatosVersionadaIPad]
    var maximoSalida = MotorMigracionesVersionadasIPad.maximoSalidaPredeterminado
}

struct ArchivoMigradoVersionadoIPad: Equatable {
    var nombre: String
    var archivo: URL
    var versionInicial: Int
    var versionFinal: Int
    var pasos: [PasoMigracionAplicadoIPad]
}

struct ResultadoMigracionConjuntoIPad: Equatable {
    var documentoId: DocumentoPersistenteID
    var transaccionId: UUID?
    var archivos: [ArchivoMigradoVersionadoIPad]
    var huboCambios: Bool { transaccionId != nil }
}

/// Transforma todos los archivos primero. Sólo si TODA la cadena es válida crea
/// una única transacción multiarchivo con precondición SHA de cada original.
final class MigradorConjuntoVersionadoIPad {
    private let transacciones: CoordinadorPersistenciaTransaccionalIPad
    private let archivos: FileManager

    init(transacciones: CoordinadorPersistenciaTransaccionalIPad = .compartido,
         archivos: FileManager = .default) {
        self.transacciones = transacciones
        self.archivos = archivos
    }

    func migrar(documentoId: DocumentoPersistenteID,
                 solicitudes: [SolicitudMigracionArchivoVersionadoIPad]) throws
        -> ResultadoMigracionConjuntoIPad {
        var cambios: [CambioArchivoTransaccionalIPad] = []
        var resultados: [ArchivoMigradoVersionadoIPad] = []

        for solicitud in solicitudes {
            let url = solicitud.archivo.standardizedFileURL
            guard archivos.fileExists(atPath: url.path) else {
                throw ErrorMigracionVersionadaIPad.archivoAusente(url.path)
            }
            let originales = try Data(contentsOf: url, options: [.mappedIfSafe])
            let transformacion = try MotorMigracionesVersionadasIPad.aplicar(
                datos: originales,
                versionActual: solicitud.versionActual,
                versionDestino: solicitud.versionDestino,
                migraciones: solicitud.migraciones,
                maximoSalida: solicitud.maximoSalida)
            resultados.append(.init(
                nombre: solicitud.nombre,
                archivo: url,
                versionInicial: solicitud.versionActual,
                versionFinal: transformacion.versionFinal,
                pasos: transformacion.pasos))

            if !transformacion.pasos.isEmpty {
                cambios.append(.init(
                    destino: url,
                    mutacion: .escribir(transformacion.datos),
                    precondicion: .sha256(Self.sha256(originales))))
            }
        }

        guard !cambios.isEmpty else {
            return .init(documentoId: documentoId,
                         transaccionId: nil,
                         archivos: resultados)
        }
        let preparada = try transacciones.ejecutar(
            documentoId: documentoId,
            cambios: cambios)
        return .init(documentoId: documentoId,
                     transaccionId: preparada.id,
                     archivos: resultados)
    }

    private static func sha256(_ datos: Data) -> String {
        SHA256.hash(data: datos).map { String(format: "%02x", $0) }.joined()
    }
}
