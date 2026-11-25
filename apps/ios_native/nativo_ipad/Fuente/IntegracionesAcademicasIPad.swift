import CryptoKit
import EventKit
import Foundation
import PDFKit
import UIKit
import UniformTypeIdentifiers

// MARK: - Persistencia de integraciones e importaciones

enum FuenteAcademicaExterna: String, Codable, CaseIterable {
    case calendarioIPad
    case ics
    case canvas
    case moodle
    case teams
    case classroom
    case csv
    case json
    case archivoProfesor
    case generica
}

struct RegistroImportacionAcademica: Codable, Identifiable, Equatable {
    var id = UUID()
    var huella: String
    var fuente: FuenteAcademicaExterna
    var nombre: String
    var fecha = Date()
    var tareasNuevas = 0
    var eventosNuevos = 0
    var materialesNuevos = 0
    var duplicadosOmitidos = 0
    var advertencias: [String] = []
}

struct ResumenCambioMaterial: Codable, Equatable {
    var bytesAntes: Int64 = 0
    var bytesDespues: Int64 = 0
    var paginasAntes: Int?
    var paginasDespues: Int?
    var lineasAgregadas: [String] = []
    var lineasRetiradas: [String] = []
    var descripcion: String
}

struct VersionMaterialProfesorIPad: Codable, Identifiable, Equatable {
    var id = UUID()
    var hash: String
    var hashRecurso: String
    var fecha = Date()
    var bytes: Int64
    var cambio: ResumenCambioMaterial?
}

struct MaterialProfesorIPad: Codable, Identifiable, Equatable {
    var id = UUID()
    var identidadExterna: String
    var nombre: String
    var cursoId: UUID?
    var bookmark: Data?
    var hashActual: String
    var hashRecursoActual: String
    var fechaRevision = Date()
    var versiones: [VersionMaterialProfesorIPad] = []
    var referenciaUnica = true
}

struct VinculoTareaExterna: Codable, Identifiable, Equatable {
    var id: String { claveExterna }
    var claveExterna: String
    var tareaId: UUID
    var fuente: FuenteAcademicaExterna
    var fecha = Date()
}

private struct EstadoIntegracionesAcademicas: Codable {
    var version = 1
    var importaciones: [RegistroImportacionAcademica] = []
    var huellasImportadas: Set<String> = []
    var materiales: [MaterialProfesorIPad] = []
    var tareasExternas: [VinculoTareaExterna] = []
    var eventosCalendarioImportados: Set<String> = []
    var calendariosSeleccionados: Set<String> = []

    enum CodingKeys: String, CodingKey {
        case version, importaciones, huellasImportadas, materiales
        case tareasExternas, eventosCalendarioImportados, calendariosSeleccionados
    }

    init() {}

    init(from decoder: Decoder) throws {
        let c = try decoder.container(keyedBy: CodingKeys.self)
        version = try c.decodeIfPresent(Int.self, forKey: .version) ?? 1
        importaciones = try c.decodeIfPresent([RegistroImportacionAcademica].self,
                                              forKey: .importaciones) ?? []
        huellasImportadas = try c.decodeIfPresent(Set<String>.self,
                                                  forKey: .huellasImportadas) ?? []
        materiales = try c.decodeIfPresent([MaterialProfesorIPad].self,
                                           forKey: .materiales) ?? []
        tareasExternas = try c.decodeIfPresent([VinculoTareaExterna].self,
                                               forKey: .tareasExternas) ?? []
        eventosCalendarioImportados = try c.decodeIfPresent(
            Set<String>.self, forKey: .eventosCalendarioImportados) ?? []
        calendariosSeleccionados = try c.decodeIfPresent(
            Set<String>.self, forKey: .calendariosSeleccionados) ?? []
    }
}

struct InstantaneaIntegracionesAcademicas {
    var importaciones: [RegistroImportacionAcademica]
    var materiales: [MaterialProfesorIPad]
    var tareasExternas: [VinculoTareaExterna]
    var eventosCalendarioImportados: Set<String>
    var calendariosSeleccionados: Set<String>
}

final class CentroIntegracionesAcademicas {
    static let compartido = CentroIntegracionesAcademicas()

    private let bloqueo = NSRecursiveLock()
    private let url: URL
    private var estado: EstadoIntegracionesAcademicas

    private init() {
        let documentos = FileManager.default.urls(for: .documentDirectory,
                                                   in: .userDomainMask).first!
        let carpeta = documentos.appendingPathComponent("Academico", isDirectory: true)
        try? FileManager.default.createDirectory(at: carpeta,
                                                 withIntermediateDirectories: true)
        url = carpeta.appendingPathComponent("integraciones-ipad.json")
        let decoder = JSONDecoder()
        decoder.dateDecodingStrategy = .iso8601
        if let datos = try? Data(contentsOf: url),
           let leido = try? decoder.decode(EstadoIntegracionesAcademicas.self,
                                           from: datos) {
            estado = leido
        } else {
            estado = EstadoIntegracionesAcademicas()
        }
    }

    func instantanea() -> InstantaneaIntegracionesAcademicas {
        bloqueo.lock(); defer { bloqueo.unlock() }
        return InstantaneaIntegracionesAcademicas(
            importaciones: estado.importaciones,
            materiales: estado.materiales,
            tareasExternas: estado.tareasExternas,
            eventosCalendarioImportados: estado.eventosCalendarioImportados,
            calendariosSeleccionados: estado.calendariosSeleccionados)
    }

    @discardableResult
    private func cambiar<T>(_ cuerpo: (inout EstadoIntegracionesAcademicas) -> T) -> T {
        bloqueo.lock(); defer { bloqueo.unlock() }
        let resultado = cuerpo(&estado)
        guardarSinBloqueo()
        return resultado
    }

    private func guardarSinBloqueo() {
        let encoder = JSONEncoder()
        encoder.dateEncodingStrategy = .iso8601
        encoder.outputFormatting = [.prettyPrinted, .sortedKeys]
        guard let datos = try? encoder.encode(estado) else { return }
        try? datos.write(to: url, options: [.atomic, .completeFileProtection])
    }

    func yaImportada(_ huella: String) -> Bool {
        bloqueo.lock(); defer { bloqueo.unlock() }
        return estado.huellasImportadas.contains(huella)
    }

    func registrar(_ registro: RegistroImportacionAcademica) {
        cambiar { estado in
            estado.huellasImportadas.insert(registro.huella)
            estado.importaciones.insert(registro, at: 0)
            if estado.importaciones.count > 250 {
                estado.importaciones.removeLast(estado.importaciones.count - 250)
            }
        }
    }

    func vinculoTarea(clave: String) -> VinculoTareaExterna? {
        bloqueo.lock(); defer { bloqueo.unlock() }
        return estado.tareasExternas.first { $0.claveExterna == clave }
    }

    func registrarVinculoTarea(_ vinculo: VinculoTareaExterna) {
        cambiar { estado in
            guard !estado.tareasExternas.contains(where: {
                $0.claveExterna == vinculo.claveExterna
            }) else { return }
            estado.tareasExternas.append(vinculo)
        }
    }

    func seleccionarCalendario(_ id: String, seleccionado: Bool) {
        cambiar { estado in
            if seleccionado {
                estado.calendariosSeleccionados.insert(id)
            } else {
                estado.calendariosSeleccionados.remove(id)
            }
        }
    }

    func marcarEventoCalendario(_ id: String) {
        cambiar { $0.eventosCalendarioImportados.insert(id) }
    }

    func material(identidad: String) -> MaterialProfesorIPad? {
        bloqueo.lock(); defer { bloqueo.unlock() }
        return estado.materiales.first { $0.identidadExterna == identidad }
    }

    func guardarMaterial(_ material: MaterialProfesorIPad) {
        cambiar { estado in
            if let indice = estado.materiales.firstIndex(where: {
                $0.identidadExterna == material.identidadExterna
            }) {
                estado.materiales[indice] = material
            } else {
                estado.materiales.append(material)
            }
        }
    }
}

// MARK: - Archivos del profesor y versiones inmutables

enum ResultadoRegistroMaterial {
    case nuevo(MaterialProfesorIPad)
    case sinCambios(MaterialProfesorIPad)
    case actualizado(MaterialProfesorIPad, ResumenCambioMaterial)
    case referenciaExistente(MaterialProfesorIPad)
}

enum ErrorIntegracionAcademica: LocalizedError {
    case accesoDenegado
    case formatoNoCompatible(String)
    case archivoInvalido(String)
    case calendarioSinPermiso

    var errorDescription: String? {
        switch self {
        case .accesoDenegado: return "iPadOS no permitió leer el archivo seleccionado."
        case .formatoNoCompatible(let extensionArchivo):
            return "El formato .\(extensionArchivo) se conserva como material, pero no tiene un importador académico estructurado."
        case .archivoInvalido(let detalle): return "Archivo inválido: \(detalle)"
        case .calendarioSinPermiso: return "No hay permiso completo para leer el calendario."
        }
    }
}

final class GestorMaterialProfesorIPad {
    static let compartido = GestorMaterialProfesorIPad()

    private init() {}

    func registrar(_ origen: URL, cursoId: UUID?) throws -> ResultadoRegistroMaterial {
        let acceso = origen.startAccessingSecurityScopedResource()
        defer { if acceso { origen.stopAccessingSecurityScopedResource() } }

        guard FileManager.default.fileExists(atPath: origen.path) else {
            throw ErrorIntegracionAcademica.accesoDenegado
        }
        let adjunto = try RepositorioAdjuntos.compartido.importar(url: origen)
        let identidad = Self.identidad(nombre: origen.lastPathComponent,
                                       cursoId: cursoId)
        let bookmark = try? origen.bookmarkData(options: .minimalBookmark,
                                                includingResourceValuesForKeys: nil,
                                                relativeTo: nil)
        let centro = CentroIntegracionesAcademicas.compartido

        if var existente = centro.material(identidad: identidad) {
            if existente.hashActual == adjunto.hash {
                existente.bookmark = bookmark ?? existente.bookmark
                existente.fechaRevision = Date()
                centro.guardarMaterial(existente)
                return .sinCambios(existente)
            }
            let anteriorURL = RepositorioAdjuntos.compartido.url(
                hash: existente.hashRecursoActual)
            let cambio = Self.comparar(anterior: anteriorURL, nuevo: adjunto.url,
                                       bytesAntes: existente.versiones.last?.bytes ?? 0,
                                       bytesDespues: adjunto.bytes)
            let version = VersionMaterialProfesorIPad(
                hash: adjunto.hash, hashRecurso: adjunto.hash,
                bytes: adjunto.bytes, cambio: cambio)
            existente.versiones.append(version)
            existente.hashActual = adjunto.hash
            existente.hashRecursoActual = adjunto.hash
            existente.bookmark = bookmark ?? existente.bookmark
            existente.fechaRevision = Date()
            centro.guardarMaterial(existente)
            reflejarEnCentroAcademico(existente, cambio: cambio)
            return .actualizado(existente, cambio)
        }

        if let porContenido = centro.instantanea().materiales.first(where: {
            $0.hashActual == adjunto.hash
        }) {
            CentroExperienciaIPad.compartido.relacionar(
                origen: "material:\(porContenido.id.uuidString)",
                destino: "curso:\(cursoId?.uuidString ?? "sin-curso")",
                tipo: .perteneceA, cursoId: cursoId,
                nota: "Referencia al mismo contenido, sin duplicar bytes")
            return .referenciaExistente(porContenido)
        }

        let inicial = VersionMaterialProfesorIPad(
            hash: adjunto.hash, hashRecurso: adjunto.hash, bytes: adjunto.bytes)
        let material = MaterialProfesorIPad(
            identidadExterna: identidad, nombre: origen.lastPathComponent,
            cursoId: cursoId, bookmark: bookmark,
            hashActual: adjunto.hash, hashRecursoActual: adjunto.hash,
            versiones: [inicial])
        centro.guardarMaterial(material)
        reflejarEnCentroAcademico(material, cambio: nil)
        return .nuevo(material)
    }

    func revisarCambios() -> [ResultadoRegistroMaterial] {
        var resultados: [ResultadoRegistroMaterial] = []
        for material in CentroIntegracionesAcademicas.compartido.instantanea().materiales {
            guard let datos = material.bookmark else { continue }
            var obsoleto = false
            guard let url = try? URL(resolvingBookmarkData: datos,
                                     options: [.withoutUI], relativeTo: nil,
                                     bookmarkDataIsStale: &obsoleto), !obsoleto else { continue }
            if let resultado = try? registrar(url, cursoId: material.cursoId) {
                resultados.append(resultado)
            }
        }
        return resultados
    }

    private func reflejarEnCentroAcademico(_ material: MaterialProfesorIPad,
                                            cambio: ResumenCambioMaterial?) {
        CentroAcademico.compartido.transaccion { base in
            if let indice = base.archivosProfesor.firstIndex(where: {
                $0.nombre == material.nombre && $0.cursoId == material.cursoId
            }) {
                guard base.archivosProfesor[indice].hashActual != material.hashActual else {
                    return
                }
                base.archivosProfesor[indice].hashActual = material.hashActual
                base.archivosProfesor[indice].fechaRevision = Date()
                base.archivosProfesor[indice].versiones.append(VersionArchivo(
                    hash: material.hashActual,
                    notasCambio: cambio?.descripcion ?? "Versión nueva"))
            } else {
                base.archivosProfesor.append(ArchivoProfesor(
                    nombre: material.nombre, cursoId: material.cursoId,
                    hashActual: material.hashActual,
                    versiones: [VersionArchivo(hash: material.hashActual,
                                               notasCambio: "Versión inicial")]))
            }
            base.timeline.append(EventoAcademico(
                tipo: .archivo, titulo: material.nombre,
                detalle: cambio?.descripcion ?? "Material importado",
                cursoId: material.cursoId))
        }
    }

    private static func identidad(nombre: String, cursoId: UUID?) -> String {
        "\(cursoId?.uuidString ?? "sin-curso")|\(normalizar(nombre))"
    }

    private static func comparar(anterior: URL?, nuevo: URL,
                                 bytesAntes: Int64, bytesDespues: Int64)
        -> ResumenCambioMaterial {
        var resumen = ResumenCambioMaterial(
            bytesAntes: bytesAntes, bytesDespues: bytesDespues,
            descripcion: "El archivo cambió de contenido.")
        guard nuevo.pathExtension.lowercased() == "pdf",
              let nuevoPDF = PDFDocument(url: nuevo) else {
            resumen.descripcion = "Contenido distinto; tamaño \(bytesAntes) → \(bytesDespues) bytes. Se conservaron ambas versiones."
            return resumen
        }
        let anteriorPDF = anterior.flatMap { PDFDocument(url: $0) }
        resumen.paginasAntes = anteriorPDF?.pageCount
        resumen.paginasDespues = nuevoPDF.pageCount
        let lineasAntes = Set(lineas(pdf: anteriorPDF))
        let lineasDespues = Set(lineas(pdf: nuevoPDF))
        resumen.lineasAgregadas = Array(lineasDespues.subtracting(lineasAntes))
            .sorted().prefix(20).map { String($0) }
        resumen.lineasRetiradas = Array(lineasAntes.subtracting(lineasDespues))
            .sorted().prefix(20).map { String($0) }
        resumen.descripcion = "PDF \(resumen.paginasAntes ?? 0) → \(nuevoPDF.pageCount) páginas; \(resumen.lineasAgregadas.count) líneas nuevas y \(resumen.lineasRetiradas.count) retiradas en la vista resumida."
        return resumen
    }

    private static func lineas(pdf: PDFDocument?) -> [String] {
        guard let pdf else { return [] }
        var salida: [String] = []
        for indice in 0..<pdf.pageCount {
            guard let texto = pdf.page(at: indice)?.string else { continue }
            salida.append(contentsOf: texto.components(separatedBy: .newlines)
                .map(normalizar).filter { $0.count >= 4 })
        }
        return salida
    }

    private static func normalizar(_ texto: String) -> String {
        texto.folding(options: [.diacriticInsensitive, .caseInsensitive],
                      locale: .current)
            .lowercased().split(whereSeparator: \.isWhitespace)
            .joined(separator: " ")
    }
}

// MARK: - Importador LMS/local idempotente

struct EventoAcademicoExterno {
    var idExterno: String
    var titulo: String
    var detalle = ""
    var curso = ""
    var fechaInicio: Date?
    var fechaLimite: Date?
    var url: URL?
    var tipo = "tarea"
}

struct ResultadoImportacionLocal {
    var registro: RegistroImportacionAcademica
    var tareas: [TareaAcademica]
    var materiales: [MaterialProfesorIPad]
}

final class ImportadorAcademicoLocal {
    static let compartido = ImportadorAcademicoLocal()
    private init() {}

    func importar(_ url: URL, cursoIdForzado: UUID? = nil) throws
        -> ResultadoImportacionLocal {
        let acceso = url.startAccessingSecurityScopedResource()
        defer { if acceso { url.stopAccessingSecurityScopedResource() } }
        let datos = try Data(contentsOf: url)
        let huella = Self.sha(datos)
        let extensionArchivo = url.pathExtension.lowercased()
        let fuente = Self.fuente(nombre: url.lastPathComponent,
                                 extensionArchivo: extensionArchivo)
        var registro = RegistroImportacionAcademica(
            huella: huella, fuente: fuente, nombre: url.lastPathComponent)
        if CentroIntegracionesAcademicas.compartido.yaImportada(huella) {
            registro.duplicadosOmitidos = 1
            return ResultadoImportacionLocal(registro: registro,
                                             tareas: [], materiales: [])
        }

        let eventos: [EventoAcademicoExterno]
        switch extensionArchivo {
        case "ics": eventos = try Self.eventosICS(datos)
        case "csv", "tsv": eventos = try Self.eventosCSV(
            datos, separadorPreferido: extensionArchivo == "tsv" ? "\t" : nil)
        case "json": eventos = try Self.eventosJSON(datos)
        case "pdf", "ppt", "pptx", "doc", "docx", "key", "pages":
            let resultado = try GestorMaterialProfesorIPad.compartido.registrar(
                url, cursoId: cursoIdForzado)
            let material: MaterialProfesorIPad
            switch resultado {
            case .nuevo(let valor): material = valor; registro.materialesNuevos = 1
            case .actualizado(let valor, _): material = valor; registro.materialesNuevos = 1
            case .sinCambios(let valor), .referenciaExistente(let valor):
                material = valor; registro.duplicadosOmitidos = 1
            }
            CentroIntegracionesAcademicas.compartido.registrar(registro)
            return ResultadoImportacionLocal(registro: registro,
                                             tareas: [], materiales: [material])
        default:
            throw ErrorIntegracionAcademica.formatoNoCompatible(extensionArchivo)
        }

        let tareas = registrar(eventos, fuente: fuente,
                               cursoIdForzado: cursoIdForzado,
                               registro: &registro)
        CentroIntegracionesAcademicas.compartido.registrar(registro)
        return ResultadoImportacionLocal(registro: registro,
                                         tareas: tareas, materiales: [])
    }

    @discardableResult
    func registrar(_ eventos: [EventoAcademicoExterno],
                   fuente: FuenteAcademicaExterna,
                   cursoIdForzado: UUID? = nil,
                   registro: inout RegistroImportacionAcademica)
        -> [TareaAcademica] {
        var nuevas: [TareaAcademica] = []
        for evento in eventos {
            let clave = "\(fuente.rawValue)|\(evento.idExterno)"
            if CentroIntegracionesAcademicas.compartido.vinculoTarea(clave: clave) != nil {
                registro.duplicadosOmitidos += 1
                continue
            }
            let cursoId = cursoIdForzado ?? Self.cursoId(nombre: evento.curso)
            let normal = Self.normalizar("\(evento.tipo) \(evento.titulo)")
            if normal.contains("anuncio") || normal.contains("announcement") {
                _ = CentroAcademico.compartido.capturar(
                    evento.titulo + (evento.detalle.isEmpty ? "" : "\n\(evento.detalle)"),
                    tipo: .texto, cursoSugerido: cursoId)
                registro.eventosNuevos += 1
                continue
            }
            if normal.contains("clase") || normal.contains("lecture") {
                CentroAcademico.compartido.transaccion { base in
                    base.timeline.append(EventoAcademico(
                        fecha: evento.fechaInicio ?? Date(), tipo: .clase,
                        titulo: evento.titulo, detalle: evento.detalle,
                        cursoId: cursoId))
                }
                registro.eventosNuevos += 1
                continue
            }
            var tarea = TareaAcademica(
                titulo: evento.titulo, detalle: evento.detalle,
                cursoId: cursoId, fechaLimite: evento.fechaLimite ?? evento.fechaInicio)
            tarea.referencia = ReferenciaFuente(
                tipo: .web, titulo: evento.titulo, cursoId: cursoId)
            tarea.prioridad = normal.contains("examen") || normal.contains("exam")
                ? .urgente : .normal
            tarea.pasos = DescomponedorTareas.pasos(
                para: tarea.titulo, minutos: tarea.estimadoMinutos)
            CentroAcademico.compartido.transaccion { base in
                base.tareas.append(tarea)
                base.timeline.append(EventoAcademico(
                    tipo: tarea.prioridad == .urgente ? .examen : .tarea,
                    titulo: tarea.titulo, detalle: tarea.detalle,
                    cursoId: cursoId, referencia: tarea.referencia))
            }
            CentroIntegracionesAcademicas.compartido.registrarVinculoTarea(
                VinculoTareaExterna(claveExterna: clave, tareaId: tarea.id,
                                    fuente: fuente))
            nuevas.append(tarea)
            registro.tareasNuevas += 1
        }
        return nuevas
    }

    private static func eventosICS(_ datos: Data) throws -> [EventoAcademicoExterno] {
        guard let texto = String(data: datos, encoding: .utf8) else {
            throw ErrorIntegracionAcademica.archivoInvalido("ICS no es UTF-8")
        }
        var desplegadas: [String] = []
        for linea in texto.components(separatedBy: .newlines) {
            if (linea.hasPrefix(" ") || linea.hasPrefix("\t")), !desplegadas.isEmpty {
                desplegadas[desplegadas.count - 1] += linea.dropFirst()
            } else {
                desplegadas.append(linea.trimmingCharacters(in: .newlines))
            }
        }
        var salida: [EventoAcademicoExterno] = []
        var campos: [String: String]? = nil
        for linea in desplegadas {
            if linea == "BEGIN:VEVENT" { campos = [:]; continue }
            if linea == "END:VEVENT", let valor = campos {
                let titulo = desescapar(valor["SUMMARY"] ?? "Evento académico")
                let identidad = valor["UID"]
                    ?? sha(Data("\(titulo)|\(valor["DTSTART"] ?? "")|\(valor["DTEND"] ?? "")".utf8))
                salida.append(EventoAcademicoExterno(
                    idExterno: identidad,
                    titulo: titulo,
                    detalle: desescapar(valor["DESCRIPTION"] ?? ""),
                    curso: desescapar(valor["CATEGORIES"] ?? ""),
                    fechaInicio: fechaICS(valor["DTSTART"]),
                    fechaLimite: fechaICS(valor["DUE"] ?? valor["DTEND"]),
                    url: valor["URL"].flatMap(URL.init(string:)),
                    tipo: titulo.lowercased().contains("exam")
                        || titulo.lowercased().contains("examen") ? "examen" : "tarea"))
                campos = nil
                continue
            }
            guard campos != nil, let separador = linea.firstIndex(of: ":") else { continue }
            let claveCruda = String(linea[..<separador])
            let clave = claveCruda.components(separatedBy: ";").first ?? claveCruda
            campos?[clave.uppercased()] = String(linea[linea.index(after: separador)...])
        }
        return salida
    }

    private static func eventosCSV(_ datos: Data, separadorPreferido: Character?) throws
        -> [EventoAcademicoExterno] {
        guard let texto = String(data: datos, encoding: .utf8) else {
            throw ErrorIntegracionAcademica.archivoInvalido("CSV no es UTF-8")
        }
        let separador = separadorPreferido ?? (texto.first(where: { $0 == ";" }) != nil
            && texto.filter { $0 == ";" }.count > texto.filter { $0 == "," }.count ? ";" : ",")
        let filas = parsearCSV(texto, separador: separador)
        guard let cabecera = filas.first else { return [] }
        let claves = cabecera.map(normalizar)
        func valor(_ fila: [String], _ nombres: [String]) -> String? {
            guard let indice = claves.firstIndex(where: nombres.contains),
                  indice < fila.count else { return nil }
            return fila[indice].trimmingCharacters(in: .whitespacesAndNewlines)
        }
        return filas.dropFirst().enumerated().compactMap { entrada in
            let (indice, fila) = entrada
            guard let titulo = valor(fila, ["title", "titulo", "name", "nombre",
                                                 "assignment", "tarea"]),
                  !titulo.isEmpty else { return nil }
            let id = valor(fila, ["id", "uid", "external id", "external_id"])
                ?? "fila-\(indice)-\(sha(Data(titulo.utf8)))"
            return EventoAcademicoExterno(
                idExterno: id, titulo: titulo,
                detalle: valor(fila, ["description", "descripcion", "detalle"]) ?? "",
                curso: valor(fila, ["course", "curso", "class", "materia"]) ?? "",
                fechaInicio: parsearFecha(valor(fila, ["start", "inicio"])),
                fechaLimite: parsearFecha(valor(fila, ["due", "due at", "due_at",
                                                              "fecha limite", "fecha_limite", "date"])),
                url: valor(fila, ["url", "link", "enlace"]).flatMap(URL.init(string:)),
                tipo: valor(fila, ["type", "tipo"]) ?? "tarea")
        }
    }

    private static func eventosJSON(_ datos: Data) throws -> [EventoAcademicoExterno] {
        let objeto = try JSONSerialization.jsonObject(with: datos)
        var salida: [EventoAcademicoExterno] = []
        var contador = 0
        func recorrer(_ valor: Any, cursoHeredado: String = "") {
            if let arreglo = valor as? [Any] {
                arreglo.forEach { recorrer($0, cursoHeredado: cursoHeredado) }
                return
            }
            guard let diccionario = valor as? [String: Any] else { return }
            let curso = texto(diccionario, claves: ["course", "course_name", "curso",
                                                    "context_name"]) ?? cursoHeredado
            if let titulo = texto(diccionario, claves: ["title", "name", "titulo",
                                                        "assignment_name", "subject"]) {
                contador += 1
                let id = texto(diccionario, claves: ["id", "uid", "external_id",
                                                     "html_url"]) ?? "json-\(contador)-\(sha(Data(titulo.utf8)))"
                salida.append(EventoAcademicoExterno(
                    idExterno: id, titulo: titulo,
                    detalle: texto(diccionario, claves: ["description", "body", "detalle",
                                                          "message"]) ?? "",
                    curso: curso,
                    fechaInicio: fecha(diccionario, claves: ["start_at", "start", "inicio"]),
                    fechaLimite: fecha(diccionario, claves: ["due_at", "due", "fecha_limite",
                                                             "lock_at"]),
                    url: texto(diccionario, claves: ["html_url", "url", "link"])
                        .flatMap(URL.init(string:)),
                    tipo: texto(diccionario, claves: ["type", "tipo", "workflow_state"])
                        ?? "tarea"))
            }
            for (clave, hijo) in diccionario where hijo is [Any] || hijo is [String: Any] {
                if !["rubric", "metadata"].contains(clave.lowercased()) {
                    recorrer(hijo, cursoHeredado: curso)
                }
            }
        }
        recorrer(objeto)
        return salida
    }

    private static func parsearCSV(_ texto: String, separador: Character) -> [[String]] {
        var filas: [[String]] = []
        var fila: [String] = []
        var campo = ""
        var entreComillas = false
        var indice = texto.startIndex
        while indice < texto.endIndex {
            let caracter = texto[indice]
            if caracter == "\"" {
                let siguiente = texto.index(after: indice)
                if entreComillas, siguiente < texto.endIndex, texto[siguiente] == "\"" {
                    campo.append("\""); indice = siguiente
                } else {
                    entreComillas.toggle()
                }
            } else if caracter == separador, !entreComillas {
                fila.append(campo); campo = ""
            } else if (caracter == "\n" || caracter == "\r"), !entreComillas {
                if caracter == "\r" {
                    let siguiente = texto.index(after: indice)
                    if siguiente < texto.endIndex, texto[siguiente] == "\n" { indice = siguiente }
                }
                fila.append(campo); campo = ""
                if fila.contains(where: { !$0.isEmpty }) { filas.append(fila) }
                fila = []
            } else {
                campo.append(caracter)
            }
            indice = texto.index(after: indice)
        }
        fila.append(campo)
        if fila.contains(where: { !$0.isEmpty }) { filas.append(fila) }
        return filas
    }

    private static func cursoId(nombre: String) -> UUID? {
        let buscado = normalizar(nombre)
        guard !buscado.isEmpty else { return nil }
        let cursos = CentroAcademico.compartido.instantanea().cursos
        return cursos.first {
            let nombre = normalizar($0.nombre)
            let codigo = normalizar($0.codigo)
            return (!nombre.isEmpty && (nombre == buscado || buscado.contains(nombre)))
                || (!codigo.isEmpty && buscado.contains(codigo))
        }?.id
    }

    private static func fuente(nombre: String, extensionArchivo: String)
        -> FuenteAcademicaExterna {
        let normal = normalizar(nombre)
        if normal.contains("canvas") { return .canvas }
        if normal.contains("moodle") { return .moodle }
        if normal.contains("teams") { return .teams }
        if normal.contains("classroom") { return .classroom }
        switch extensionArchivo {
        case "ics": return .ics
        case "csv", "tsv": return .csv
        case "json": return .json
        case "pdf", "ppt", "pptx", "doc", "docx": return .archivoProfesor
        default: return .generica
        }
    }

    private static func texto(_ diccionario: [String: Any], claves: [String]) -> String? {
        for clave in claves {
            if let valor = diccionario[clave] as? String, !valor.isEmpty { return valor }
            if let numero = diccionario[clave] as? NSNumber { return numero.stringValue }
        }
        return nil
    }

    private static func fecha(_ diccionario: [String: Any], claves: [String]) -> Date? {
        for clave in claves {
            if let valor = diccionario[clave] as? String,
               let fecha = parsearFecha(valor) { return fecha }
            if let numero = diccionario[clave] as? NSNumber {
                let segundos = numero.doubleValue > 4_000_000_000
                    ? numero.doubleValue / 1_000 : numero.doubleValue
                return Date(timeIntervalSince1970: segundos)
            }
        }
        return nil
    }

    private static func parsearFecha(_ texto: String?) -> Date? {
        guard let texto, !texto.isEmpty else { return nil }
        if let fecha = ISO8601DateFormatter().date(from: texto) { return fecha }
        for formato in ["yyyy-MM-dd HH:mm:ss", "yyyy-MM-dd", "dd/MM/yyyy HH:mm",
                        "dd/MM/yyyy", "MM/dd/yyyy"] {
            let df = DateFormatter(); df.locale = Locale(identifier: "en_US_POSIX")
            df.dateFormat = formato
            if let fecha = df.date(from: texto) { return fecha }
        }
        return nil
    }

    private static func fechaICS(_ texto: String?) -> Date? {
        guard let texto else { return nil }
        for formato in ["yyyyMMdd'T'HHmmss'Z'", "yyyyMMdd'T'HHmmss", "yyyyMMdd"] {
            let df = DateFormatter(); df.locale = Locale(identifier: "en_US_POSIX")
            df.timeZone = formato.contains("'Z'") ? TimeZone(secondsFromGMT: 0) : .current
            df.dateFormat = formato
            if let fecha = df.date(from: texto) { return fecha }
        }
        return nil
    }

    private static func desescapar(_ texto: String) -> String {
        texto.replacingOccurrences(of: "\\n", with: "\n")
            .replacingOccurrences(of: "\\,", with: ",")
            .replacingOccurrences(of: "\\;", with: ";")
            .replacingOccurrences(of: "\\\\", with: "\\")
    }

    private static func normalizar(_ texto: String) -> String {
        texto.folding(options: [.diacriticInsensitive, .caseInsensitive],
                      locale: .current).lowercased()
            .split(whereSeparator: \.isWhitespace).joined(separator: " ")
    }

    private static func sha(_ datos: Data) -> String {
        SHA256.hash(data: datos).map { String(format: "%02x", $0) }.joined()
    }
}

// MARK: - Calendario académico del iPad

struct ResultadoSincronizacionCalendario {
    var eventosLeidos = 0
    var tareasCreadas = 0
    var duplicados = 0
    var ignorados = 0
}

final class GestorCalendarioAcademicoIPad {
    static let compartido = GestorCalendarioAcademicoIPad()
    let almacen = EKEventStore()

    private init() {}

    var estadoAutorizacion: EKAuthorizationStatus {
        EKEventStore.authorizationStatus(for: .event)
    }

    func pedirAcceso(_ terminar: @escaping (Result<Bool, Error>) -> Void) {
        almacen.requestFullAccessToEvents { concedido, error in
            DispatchQueue.main.async {
                if let error { terminar(.failure(error)) }
                else { terminar(.success(concedido)) }
            }
        }
    }

    func calendarios() -> [EKCalendar] {
        almacen.calendars(for: .event).sorted {
            $0.title.localizedCaseInsensitiveCompare($1.title) == .orderedAscending
        }
    }

    func sincronizar(desde: Date = Calendar.current.date(byAdding: .day, value: -45,
                                                          to: Date())!,
                     hasta: Date = Calendar.current.date(byAdding: .day, value: 365,
                                                          to: Date())!) throws
        -> ResultadoSincronizacionCalendario {
        guard estadoAutorizacion == .fullAccess else {
            throw ErrorIntegracionAcademica.calendarioSinPermiso
        }
        let integraciones = CentroIntegracionesAcademicas.compartido
        let instantanea = integraciones.instantanea()
        let calendarios = calendarios().filter {
            instantanea.calendariosSeleccionados.contains($0.calendarIdentifier)
        }
        guard !calendarios.isEmpty else { return ResultadoSincronizacionCalendario() }
        let predicado = almacen.predicateForEvents(
            withStart: desde, end: hasta, calendars: calendarios)
        let eventos = almacen.events(matching: predicado)
        var resultado = ResultadoSincronizacionCalendario(eventosLeidos: eventos.count)
        var registro = RegistroImportacionAcademica(
            huella: "calendario-\(Int(Date().timeIntervalSince1970))",
            fuente: .calendarioIPad, nombre: "Calendarios del iPad")
        var externos: [EventoAcademicoExterno] = []
        for evento in eventos {
            let id = evento.eventIdentifier ?? "\(evento.calendarItemIdentifier)|\(evento.startDate.timeIntervalSince1970)"
            if instantanea.eventosCalendarioImportados.contains(id) {
                resultado.duplicados += 1
                continue
            }
            let texto = Self.normalizar("\(evento.title ?? "") \(evento.notes ?? "")")
            let curso = Self.cursoCoincidente(
                texto: "\(evento.calendar.title) \(evento.title ?? "")")
            let esAcademico = curso != nil || Self.indiciosAcademicos.contains(where: texto.contains)
            guard esAcademico else { resultado.ignorados += 1; continue }
            externos.append(EventoAcademicoExterno(
                idExterno: id, titulo: evento.title ?? "Evento académico",
                detalle: evento.notes ?? "", curso: curso?.nombre ?? evento.calendar.title,
                fechaInicio: evento.startDate, fechaLimite: evento.endDate,
                url: evento.url,
                tipo: texto.contains("examen") || texto.contains("exam")
                    ? "examen" : (texto.contains("clase") ? "clase" : "tarea")))
            integraciones.marcarEventoCalendario(id)
        }
        let tareas = ImportadorAcademicoLocal.compartido.registrar(
            externos, fuente: .calendarioIPad, registro: &registro)
        resultado.tareasCreadas = tareas.count
        registro.duplicadosOmitidos += resultado.duplicados
        integraciones.registrar(registro)
        return resultado
    }

    @discardableResult
    func crearEvento(titulo: String, inicio: Date, fin: Date,
                     notas: String = "", documentoId: UInt64?,
                     calendario: EKCalendar? = nil, alarmaMinutos: Int? = 30) throws
        -> String {
        guard estadoAutorizacion == .fullAccess else {
            throw ErrorIntegracionAcademica.calendarioSinPermiso
        }
        let evento = EKEvent(eventStore: almacen)
        evento.title = titulo
        evento.startDate = inicio
        evento.endDate = max(fin, inicio.addingTimeInterval(15 * 60))
        evento.notes = notas
        evento.calendar = calendario ?? almacen.defaultCalendarForNewEvents
        if let documentoId {
            evento.url = URL(string: "tinta://abrir?documento=\(documentoId)")
        }
        if let alarmaMinutos {
            evento.addAlarm(EKAlarm(relativeOffset: TimeInterval(-alarmaMinutos * 60)))
        }
        try almacen.save(evento, span: .thisEvent, commit: true)
        return evento.eventIdentifier
    }

    private static let indiciosAcademicos = [
        "clase", "curso", "tarea", "entrega", "examen", "quiz", "laboratorio",
        "proyecto", "assignment", "lecture", "exam", "deadline", "campus"
    ]

    private static func cursoCoincidente(texto: String) -> CursoAcademico? {
        let normal = normalizar(texto)
        return CentroAcademico.compartido.instantanea().cursos.first {
            (!normalizar($0.nombre).isEmpty && normal.contains(normalizar($0.nombre)))
                || (!$0.codigo.isEmpty && normal.contains(normalizar($0.codigo)))
        }
    }

    private static func normalizar(_ texto: String) -> String {
        texto.folding(options: [.diacriticInsensitive, .caseInsensitive],
                      locale: .current).lowercased()
    }
}

// MARK: - Interfaz de calendario, LMS y materiales

final class PanelIntegracionesAcademicasIPad: UITableViewController,
                                               UIDocumentPickerDelegate {
    private var instantanea = CentroIntegracionesAcademicas.compartido.instantanea()
    private var calendarios: [EKCalendar] = []
    private var trabajando = false

    init() { super.init(style: .insetGrouped) }
    required init?(coder: NSCoder) { fatalError("init(coder:) no implementado") }

    override func viewDidLoad() {
        super.viewDidLoad()
        title = "Calendario y Campus"
        navigationItem.leftBarButtonItem = UIBarButtonItem(
            barButtonSystemItem: .close, target: self, action: #selector(cerrar))
        navigationItem.rightBarButtonItem = UIBarButtonItem(
            image: UIImage(systemName: "arrow.clockwise"), style: .plain,
            target: self, action: #selector(revisar))
        refrescar()
    }

    private func refrescar() {
        instantanea = CentroIntegracionesAcademicas.compartido.instantanea()
        if GestorCalendarioAcademicoIPad.compartido.estadoAutorizacion == .fullAccess {
            calendarios = GestorCalendarioAcademicoIPad.compartido.calendarios()
        } else {
            calendarios = []
        }
        tableView.reloadData()
    }

    override func numberOfSections(in tableView: UITableView) -> Int { 4 }

    override func tableView(_ tableView: UITableView,
                            numberOfRowsInSection section: Int) -> Int {
        switch section {
        case 0: return 2 + calendarios.count
        case 1: return 2
        case 2: return max(1, instantanea.materiales.count)
        default: return max(1, min(instantanea.importaciones.count, 20))
        }
    }

    override func tableView(_ tableView: UITableView,
                            titleForHeaderInSection section: Int) -> String? {
        ["Calendario académico", "Importación local LMS/Campus",
         "Archivos del profesor", "Historial idempotente"][section]
    }

    override func tableView(_ tableView: UITableView,
                            titleForFooterInSection section: Int) -> String? {
        switch section {
        case 0: return "Solo se leen los calendarios que usted marque. Los demás eventos personales se ignoran."
        case 1: return "Admite ICS, CSV/TSV, JSON y materiales PDF/Office exportados legalmente. No requiere credenciales ni API de pago."
        case 2: return "Cada hash se almacena una vez; una actualización conserva la versión anterior y las anotaciones."
        default: return "Reimportar el mismo contenido no crea tareas ni archivos duplicados."
        }
    }

    override func tableView(_ tableView: UITableView,
                            cellForRowAt indexPath: IndexPath) -> UITableViewCell {
        let celda = UITableViewCell(style: .subtitle, reuseIdentifier: nil)
        switch indexPath.section {
        case 0 where indexPath.row == 0:
            celda.textLabel?.text = "Acceso al calendario"
            celda.detailTextLabel?.text = descripcionAutorizacion()
            celda.imageView?.image = UIImage(systemName: "calendar.badge.checkmark")
        case 0 where indexPath.row == 1:
            celda.textLabel?.text = "Sincronizar calendarios marcados"
            celda.detailTextLabel?.text = "Importar clases, exámenes y entregas sin duplicar"
            celda.imageView?.image = UIImage(systemName: "arrow.triangle.2.circlepath")
        case 0:
            let calendario = calendarios[indexPath.row - 2]
            celda.textLabel?.text = calendario.title
            celda.detailTextLabel?.text = calendario.source.title
            celda.accessoryType = instantanea.calendariosSeleccionados.contains(
                calendario.calendarIdentifier) ? .checkmark : .none
        case 1 where indexPath.row == 0:
            celda.textLabel?.text = "Importar exportación o material"
            celda.detailTextLabel?.text = "ICS · CSV · TSV · JSON · PDF · PPTX · DOCX"
            celda.imageView?.image = UIImage(systemName: "square.and.arrow.down")
        case 1:
            celda.textLabel?.text = "Crear evento desde una nota"
            celda.detailTextLabel?.text = "El evento conserva un enlace profundo a Tinta"
            celda.imageView?.image = UIImage(systemName: "calendar.badge.plus")
        case 2:
            guard !instantanea.materiales.isEmpty else {
                celda.textLabel?.text = "No hay materiales registrados"
                celda.selectionStyle = .none
                return celda
            }
            let material = instantanea.materiales[indexPath.row]
            celda.textLabel?.text = material.nombre
            let ultima = material.versiones.last?.cambio?.descripcion
            celda.detailTextLabel?.text = ultima
                ?? "\(material.versiones.count) versión\(material.versiones.count == 1 ? "" : "es")"
            celda.imageView?.image = UIImage(systemName:
                material.versiones.count > 1 ? "doc.badge.clock" : "doc")
        default:
            guard !instantanea.importaciones.isEmpty else {
                celda.textLabel?.text = "Todavía no hay importaciones"
                celda.selectionStyle = .none
                return celda
            }
            let item = instantanea.importaciones[indexPath.row]
            celda.textLabel?.text = item.nombre
            celda.detailTextLabel?.text = "\(item.tareasNuevas) tareas · \(item.materialesNuevos) materiales · \(item.duplicadosOmitidos) duplicados omitidos"
            celda.imageView?.image = UIImage(systemName: "checkmark.circle")
        }
        return celda
    }

    override func tableView(_ tableView: UITableView,
                            didSelectRowAt indexPath: IndexPath) {
        defer { tableView.deselectRow(at: indexPath, animated: true) }
        guard !trabajando else { return }
        switch indexPath.section {
        case 0 where indexPath.row == 0: pedirCalendario()
        case 0 where indexPath.row == 1: sincronizarCalendario()
        case 0:
            let calendario = calendarios[indexPath.row - 2]
            let estaba = instantanea.calendariosSeleccionados.contains(
                calendario.calendarIdentifier)
            CentroIntegracionesAcademicas.compartido.seleccionarCalendario(
                calendario.calendarIdentifier, seleccionado: !estaba)
            refrescar()
        case 1 where indexPath.row == 0: elegirArchivos()
        case 1: crearEventoManual()
        case 2 where !instantanea.materiales.isEmpty:
            mostrarVersiones(instantanea.materiales[indexPath.row])
        default: break
        }
    }

    private func pedirCalendario() {
        GestorCalendarioAcademicoIPad.compartido.pedirAcceso { [weak self] resultado in
            switch resultado {
            case .success: self?.refrescar()
            case .failure(let error): self?.avisar(error.localizedDescription)
            }
        }
    }

    private func sincronizarCalendario() {
        trabajando = true
        DispatchQueue.global(qos: .utility).async { [weak self] in
            let resultado = Result {
                try GestorCalendarioAcademicoIPad.compartido.sincronizar()
            }
            DispatchQueue.main.async {
                self?.trabajando = false
                switch resultado {
                case .success(let resumen):
                    self?.avisar("Se leyeron \(resumen.eventosLeidos) eventos y se crearon \(resumen.tareasCreadas) tareas. \(resumen.duplicados) ya existían; \(resumen.ignorados) no eran académicos.")
                    self?.refrescar()
                case .failure(let error): self?.avisar(error.localizedDescription)
                }
            }
        }
    }

    private func elegirArchivos() {
        var tipos = ["ics", "csv", "tsv", "json", "pdf", "ppt", "pptx",
                     "doc", "docx", "key", "pages"]
            .compactMap { UTType(filenameExtension: $0) }
        tipos.append(.data)
        let selector = UIDocumentPickerViewController(
            forOpeningContentTypes: tipos, asCopy: false)
        selector.allowsMultipleSelection = true
        selector.delegate = self
        present(selector, animated: true)
    }

    func documentPicker(_ controller: UIDocumentPickerViewController,
                        didPickDocumentsAt urls: [URL]) {
        guard !urls.isEmpty else { return }
        trabajando = true
        let curso = CentroExperienciaIPad.compartido.instantanea().espacioActivo?.cursoId
        DispatchQueue.global(qos: .utility).async { [weak self] in
            var mensajes: [String] = []
            for url in urls {
                do {
                    let resultado = try ImportadorAcademicoLocal.compartido.importar(
                        url, cursoIdForzado: curso)
                    mensajes.append("\(url.lastPathComponent): \(resultado.registro.tareasNuevas) tareas, \(resultado.registro.materialesNuevos) materiales, \(resultado.registro.duplicadosOmitidos) duplicados.")
                } catch {
                    mensajes.append("\(url.lastPathComponent): \(error.localizedDescription)")
                }
            }
            DispatchQueue.main.async {
                self?.trabajando = false
                self?.avisar(mensajes.joined(separator: "\n"))
                self?.refrescar()
            }
        }
    }

    private func crearEventoManual() {
        let alerta = UIAlertController(title: "Crear evento académico", message: nil,
                                       preferredStyle: .alert)
        alerta.addTextField { $0.placeholder = "Título" }
        alerta.addTextField { $0.placeholder = "Dentro de cuántos días"; $0.keyboardType = .numberPad }
        alerta.addAction(UIAlertAction(title: "Cancelar", style: .cancel))
        alerta.addAction(UIAlertAction(title: "Crear", style: .default) { [weak self] _ in
            guard let titulo = alerta.textFields?.first?.text,
                  !titulo.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty else { return }
            let dias = Int(alerta.textFields?[1].text ?? "") ?? 1
            let inicio = Calendar.current.date(byAdding: .day, value: dias, to: Date())!
            do {
                _ = try GestorCalendarioAcademicoIPad.compartido.crearEvento(
                    titulo: titulo, inicio: inicio,
                    fin: inicio.addingTimeInterval(60 * 60),
                    documentoId: CentroExperienciaIPad.compartido.instantanea()
                        .espacioActivo?.documentoActivo)
                self?.avisar("Evento creado con enlace a la nota.")
            } catch { self?.avisar(error.localizedDescription) }
        })
        present(alerta, animated: true)
    }

    private func mostrarVersiones(_ material: MaterialProfesorIPad) {
        let lineas = material.versiones.reversed().map { version in
            let fecha = Self.fecha.string(from: version.fecha)
            return "\(fecha) · \(version.hash.prefix(10))\n\(version.cambio?.descripcion ?? "Versión conservada")"
        }
        avisar(lineas.joined(separator: "\n\n"))
    }

    @objc private func revisar() {
        trabajando = true
        DispatchQueue.global(qos: .utility).async { [weak self] in
            let resultados = GestorMaterialProfesorIPad.compartido.revisarCambios()
            let cambios = resultados.filter {
                if case .actualizado = $0 { return true }
                return false
            }.count
            DispatchQueue.main.async {
                self?.trabajando = false
                self?.avisar(cambios == 0
                    ? "No se detectaron cambios en los archivos referenciados accesibles."
                    : "Se conservaron \(cambios) versiones nuevas sin borrar las anteriores.")
                self?.refrescar()
            }
        }
    }

    private func descripcionAutorizacion() -> String {
        switch GestorCalendarioAcademicoIPad.compartido.estadoAutorizacion {
        case .fullAccess: return "Acceso completo concedido"
        case .writeOnly: return "Solo escritura; la sincronización de lectura está desactivada"
        case .denied, .restricted: return "Acceso denegado o restringido"
        case .notDetermined: return "Toque para elegir permiso"
        @unknown default: return "Estado no reconocido"
        }
    }

    private func avisar(_ texto: String) {
        let alerta = UIAlertController(title: nil, message: texto, preferredStyle: .alert)
        alerta.addAction(UIAlertAction(title: "Aceptar", style: .default))
        present(alerta, animated: true)
    }

    @objc private func cerrar() { dismiss(animated: true) }

    private static let fecha: DateFormatter = {
        let df = DateFormatter(); df.dateStyle = .medium; df.timeStyle = .short
        return df
    }()
}
