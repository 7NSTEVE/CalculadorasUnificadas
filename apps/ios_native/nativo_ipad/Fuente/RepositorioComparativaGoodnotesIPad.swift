import Foundation

enum ProductoMuestraComparativaIPad {
    case cuadernU
    case goodnotes

    var clavePersistente: String {
        switch self {
        case .cuadernU: return "cuadernu"
        case .goodnotes: return "goodnotes"
        }
    }
}

enum ErrorRepositorioComparativaGoodnotesIPad: LocalizedError, Equatable {
    case versionNoSoportada(Int)
    case demasiadasSeries(Int)
    case demasiadasFunciones(Int)
    case demasiadasMuestras(String)
    case demasiadasProveniencias(Int)
    case serieInvalida(String)
    case funcionInvalida(String)
    case idDuplicado(String)
    case provenienciaInvalida(String)

    var errorDescription: String? {
        switch self {
        case .versionNoSoportada(let version):
            return "La evidencia comparativa usa una versión no soportada (\(version))."
        case .demasiadasSeries(let cantidad):
            return "La evidencia contiene demasiadas series de rendimiento (\(cantidad))."
        case .demasiadasFunciones(let cantidad):
            return "La evidencia contiene demasiadas funciones (\(cantidad))."
        case .demasiadasMuestras(let id):
            return "La serie \(id) supera el límite seguro de muestras."
        case .demasiadasProveniencias(let cantidad):
            return "La evidencia contiene demasiados identificadores de muestra (\(cantidad))."
        case .serieInvalida(let id):
            return "La serie comparativa \(id) contiene datos inválidos."
        case .funcionInvalida(let id):
            return "La evidencia funcional \(id) contiene datos inválidos."
        case .idDuplicado(let id):
            return "La evidencia contiene un identificador duplicado: \(id)."
        case .provenienciaInvalida(let id):
            return "La proveniencia comparativa no es válida: \(id)."
        }
    }
}

struct ArchivoComparativaGoodnotesIPad: Codable, Equatable {
    var version = 1
    var politica = PoliticaSuperioridadGoodnotesIPad()
    var rendimiento: [SerieComparativaRendimientoIPad] = []
    var funciones: [EvidenciaFuncionComparativaIPad] = []
    /// Campo opcional para mantener compatibilidad con todos los JSON v1 ya
    /// generados. Cada entrada identifica una muestra automática ya aceptada.
    var provenienciaMuestras: [String]?
    var actualizado = Date()
}

struct ResumenComparativaGoodnotesIPad: Equatable {
    var seriesTotales: Int
    var seriesComparables: Int
    var muestrasCuadernU: Int
    var muestrasGoodnotes: Int
    var funcionesGoodnotes: Int
    var funcionesCuadernUVerificadas: Int
    var diferenciadoresExclusivosVerificados: Int
    var veredicto: VeredictoSuperioridadGoodnotesIPad
}

/// Fuente de verdad local para demostrar —o refutar— la afirmación de que
/// CuadernU supera Goodnotes. No mezcla benchmarks de hardware/OS distintos y
/// nunca genera muestras sintéticas para completar el mínimo del gate.
final class RepositorioComparativaGoodnotesIPad {
    static let compartido = RepositorioComparativaGoodnotesIPad()

    static let maximoSeries = 500
    static let maximoFunciones = 1_000
    static let maximoMuestrasPorProducto = 5_000
    static let maximoProveniencias = 100_000

    private let bloqueo = NSRecursiveLock()
    private let url: URL
    private var archivo: ArchivoComparativaGoodnotesIPad

    init(base: URL? = nil, archivos: FileManager = .default) {
        let carpeta = base ?? archivos.urls(
            for: .documentDirectory, in: .userDomainMask)[0]
            .appendingPathComponent("QA/ComparativaGoodnotes", isDirectory: true)
        try? archivos.createDirectory(at: carpeta,
                                      withIntermediateDirectories: true)
        url = carpeta.appendingPathComponent("evidencia-goodnotes.json")
        let decoder = JSONDecoder()
        decoder.dateDecodingStrategy = .iso8601
        if let datos = try? Data(contentsOf: url),
           let leido = try? decoder.decode(
                ArchivoComparativaGoodnotesIPad.self, from: datos),
           (try? Self.validar(leido)) != nil {
            archivo = leido
        } else {
            archivo = ArchivoComparativaGoodnotesIPad()
        }
    }

    var politica: PoliticaSuperioridadGoodnotesIPad {
        get {
            bloqueo.lock(); defer { bloqueo.unlock() }
            return archivo.politica
        }
        set {
            bloqueo.lock(); defer { bloqueo.unlock() }
            var candidato = archivo
            candidato.politica = newValue
            candidato.actualizado = Date()
            guard (try? Self.validar(candidato)) != nil,
                  (try? persistir(candidato)) != nil else { return }
            archivo = candidato
        }
    }

    @discardableResult
    func registrarMuestra(
        producto: ProductoMuestraComparativaIPad,
        metrica: String,
        escenario: String,
        dispositivo: String,
        sistema: String,
        direccion: DireccionMetricaComparativaIPad,
        valor: Double,
        fecha: Date = Date(),
        idMuestra: String? = nil
    ) throws -> SerieComparativaRendimientoIPad {
        let metrica = metrica.trimmingCharacters(in: .whitespacesAndNewlines)
        let escenario = escenario.trimmingCharacters(in: .whitespacesAndNewlines)
        let dispositivo = dispositivo.trimmingCharacters(in: .whitespacesAndNewlines)
        let sistema = sistema.trimmingCharacters(in: .whitespacesAndNewlines)
        let idMuestraLimpio = idMuestra?.trimmingCharacters(
            in: .whitespacesAndNewlines)
        guard valor.isFinite, valor >= 0,
              Self.textoValido(metrica, maximo: 160),
              Self.textoValido(escenario, maximo: 320),
              Self.textoValido(dispositivo, maximo: 120),
              Self.textoValido(sistema, maximo: 120) else {
            throw ErrorRepositorioComparativaGoodnotesIPad.serieInvalida(
                "\(metrica)|\(escenario)")
        }
        if let idMuestraLimpio,
           !Self.textoValido(idMuestraLimpio, maximo: 240) {
            throw ErrorRepositorioComparativaGoodnotesIPad
                .provenienciaInvalida(idMuestraLimpio)
        }

        bloqueo.lock(); defer { bloqueo.unlock() }
        let id = "\(metrica)|\(escenario)|\(dispositivo)|\(sistema)"
        let claveProveniencia = idMuestraLimpio.map {
            "\(producto.clavePersistente)|\($0)"
        }
        if let claveProveniencia,
           Set(archivo.provenienciaMuestras ?? []).contains(claveProveniencia) {
            guard let existente = archivo.rendimiento.first(where: { $0.id == id }) else {
                throw ErrorRepositorioComparativaGoodnotesIPad
                    .provenienciaInvalida(claveProveniencia)
            }
            return existente
        }

        var candidato = archivo
        var serie: SerieComparativaRendimientoIPad
        if let indice = candidato.rendimiento.firstIndex(where: { $0.id == id }) {
            serie = candidato.rendimiento[indice]
            guard serie.direccion == direccion else {
                throw ErrorRepositorioComparativaGoodnotesIPad.serieInvalida(id)
            }
            switch producto {
            case .cuadernU:
                guard serie.cuadernU.count < Self.maximoMuestrasPorProducto else {
                    throw ErrorRepositorioComparativaGoodnotesIPad.demasiadasMuestras(id)
                }
                serie.cuadernU.append(valor)
            case .goodnotes:
                guard serie.goodnotes.count < Self.maximoMuestrasPorProducto else {
                    throw ErrorRepositorioComparativaGoodnotesIPad.demasiadasMuestras(id)
                }
                serie.goodnotes.append(valor)
            }
            serie.fecha = max(serie.fecha, fecha)
            candidato.rendimiento[indice] = serie
        } else {
            guard candidato.rendimiento.count < Self.maximoSeries else {
                throw ErrorRepositorioComparativaGoodnotesIPad.demasiadasSeries(
                    candidato.rendimiento.count + 1)
            }
            serie = SerieComparativaRendimientoIPad(
                metrica: metrica,
                escenario: escenario,
                dispositivo: dispositivo,
                sistema: sistema,
                direccion: direccion,
                cuadernU: producto == .cuadernU ? [valor] : [],
                goodnotes: producto == .goodnotes ? [valor] : [],
                fecha: fecha)
            candidato.rendimiento.append(serie)
        }

        if let claveProveniencia {
            var proveniencias = candidato.provenienciaMuestras ?? []
            guard proveniencias.count < Self.maximoProveniencias else {
                throw ErrorRepositorioComparativaGoodnotesIPad
                    .demasiadasProveniencias(proveniencias.count + 1)
            }
            proveniencias.append(claveProveniencia)
            candidato.provenienciaMuestras = proveniencias
        }
        candidato.actualizado = Date()
        try Self.validar(candidato)
        try persistir(candidato)
        archivo = candidato
        return serie
    }

    func registrarFuncion(_ evidencia: EvidenciaFuncionComparativaIPad) throws {
        guard Self.textoValido(evidencia.id, maximo: 160),
              Self.textoValido(evidencia.nombre, maximo: 240),
              Self.textoValido(evidencia.categoria, maximo: 120),
              !evidencia.evidencia.isEmpty,
              evidencia.evidencia.count <= 4_000 else {
            throw ErrorRepositorioComparativaGoodnotesIPad.funcionInvalida(
                evidencia.id)
        }
        bloqueo.lock(); defer { bloqueo.unlock() }
        var candidato = archivo
        if let indice = candidato.funciones.firstIndex(where: { $0.id == evidencia.id }) {
            candidato.funciones[indice] = evidencia
        } else {
            guard candidato.funciones.count < Self.maximoFunciones else {
                throw ErrorRepositorioComparativaGoodnotesIPad.demasiadasFunciones(
                    candidato.funciones.count + 1)
            }
            candidato.funciones.append(evidencia)
        }
        candidato.actualizado = Date()
        try Self.validar(candidato)
        try persistir(candidato)
        archivo = candidato
    }

    func snapshot() -> ArchivoComparativaGoodnotesIPad {
        bloqueo.lock(); defer { bloqueo.unlock() }
        return archivo
    }

    func reporte(ahora: Date = Date()) -> ReporteSuperioridadGoodnotesIPad {
        bloqueo.lock(); defer { bloqueo.unlock() }
        return GateSuperioridadGoodnotesIPad.evaluar(
            rendimiento: archivo.rendimiento,
            funciones: archivo.funciones,
            politica: archivo.politica,
            ahora: ahora)
    }

    func resumen(ahora: Date = Date()) -> ResumenComparativaGoodnotesIPad {
        bloqueo.lock(); defer { bloqueo.unlock() }
        let reporte = GateSuperioridadGoodnotesIPad.evaluar(
            rendimiento: archivo.rendimiento,
            funciones: archivo.funciones,
            politica: archivo.politica,
            ahora: ahora)
        let minimo = max(archivo.politica.minimoMuestrasPorProducto, 1)
        return ResumenComparativaGoodnotesIPad(
            seriesTotales: archivo.rendimiento.count,
            seriesComparables: archivo.rendimiento.filter {
                $0.cuadernU.count >= minimo && $0.goodnotes.count >= minimo
            }.count,
            muestrasCuadernU: archivo.rendimiento.reduce(0) { $0 + $1.cuadernU.count },
            muestrasGoodnotes: archivo.rendimiento.reduce(0) { $0 + $1.goodnotes.count },
            funcionesGoodnotes: archivo.funciones.filter(\.disponibleEnGoodnotes).count,
            funcionesCuadernUVerificadas: archivo.funciones.filter {
                $0.estadoCuadernU == .verificada
            }.count,
            diferenciadoresExclusivosVerificados: archivo.funciones.filter {
                $0.diferenciadorCuadernU && !$0.disponibleEnGoodnotes
                    && $0.estadoCuadernU == .verificada
            }.count,
            veredicto: reporte.veredicto)
    }

    func exportar() throws -> Data {
        bloqueo.lock(); defer { bloqueo.unlock() }
        try Self.validar(archivo)
        return try Self.codificar(archivo)
    }

    func importar(_ datos: Data) throws {
        let decoder = JSONDecoder()
        decoder.dateDecodingStrategy = .iso8601
        let candidato = try decoder.decode(
            ArchivoComparativaGoodnotesIPad.self, from: datos)
        try Self.validar(candidato)
        bloqueo.lock(); defer { bloqueo.unlock() }
        try persistir(candidato)
        archivo = candidato
    }

    func reiniciar() {
        bloqueo.lock(); defer { bloqueo.unlock() }
        let candidato = ArchivoComparativaGoodnotesIPad()
        guard (try? persistir(candidato)) != nil else { return }
        archivo = candidato
    }

    private func persistir(_ valor: ArchivoComparativaGoodnotesIPad) throws {
        try Self.codificar(valor).write(
            to: url, options: [.atomic, .completeFileProtection])
    }

    private static func codificar(
        _ valor: ArchivoComparativaGoodnotesIPad
    ) throws -> Data {
        let encoder = JSONEncoder()
        encoder.dateEncodingStrategy = .iso8601
        encoder.outputFormatting = [.prettyPrinted, .sortedKeys]
        return try encoder.encode(valor)
    }

    private static func textoValido(_ texto: String, maximo: Int) -> Bool {
        !texto.isEmpty && texto.count <= maximo
            && !texto.unicodeScalars.contains(where: { $0.value == 0 })
    }

    static func validar(_ valor: ArchivoComparativaGoodnotesIPad) throws {
        guard valor.version == 1 else {
            throw ErrorRepositorioComparativaGoodnotesIPad
                .versionNoSoportada(valor.version)
        }
        guard valor.rendimiento.count <= maximoSeries else {
            throw ErrorRepositorioComparativaGoodnotesIPad
                .demasiadasSeries(valor.rendimiento.count)
        }
        guard valor.funciones.count <= maximoFunciones else {
            throw ErrorRepositorioComparativaGoodnotesIPad
                .demasiadasFunciones(valor.funciones.count)
        }
        let proveniencias = valor.provenienciaMuestras ?? []
        guard proveniencias.count <= maximoProveniencias else {
            throw ErrorRepositorioComparativaGoodnotesIPad
                .demasiadasProveniencias(proveniencias.count)
        }
        guard Set(proveniencias).count == proveniencias.count else {
            throw ErrorRepositorioComparativaGoodnotesIPad
                .idDuplicado("proveniencia")
        }
        for clave in proveniencias where !textoValido(clave, maximo: 260) {
            throw ErrorRepositorioComparativaGoodnotesIPad
                .provenienciaInvalida(clave)
        }

        var idsSeries = Set<String>()
        for serie in valor.rendimiento {
            guard idsSeries.insert(serie.id).inserted else {
                throw ErrorRepositorioComparativaGoodnotesIPad
                    .idDuplicado(serie.id)
            }
            guard textoValido(serie.metrica, maximo: 160),
                  textoValido(serie.escenario, maximo: 320),
                  textoValido(serie.dispositivo, maximo: 120),
                  textoValido(serie.sistema, maximo: 120),
                  serie.cuadernU.count <= maximoMuestrasPorProducto,
                  serie.goodnotes.count <= maximoMuestrasPorProducto,
                  serie.cuadernU.allSatisfy({ $0.isFinite && $0 >= 0 }),
                  serie.goodnotes.allSatisfy({ $0.isFinite && $0 >= 0 }) else {
                if serie.cuadernU.count > maximoMuestrasPorProducto
                    || serie.goodnotes.count > maximoMuestrasPorProducto {
                    throw ErrorRepositorioComparativaGoodnotesIPad
                        .demasiadasMuestras(serie.id)
                }
                throw ErrorRepositorioComparativaGoodnotesIPad
                    .serieInvalida(serie.id)
            }
        }

        var idsFunciones = Set<String>()
        for funcion in valor.funciones {
            guard idsFunciones.insert(funcion.id).inserted else {
                throw ErrorRepositorioComparativaGoodnotesIPad
                    .idDuplicado(funcion.id)
            }
            guard textoValido(funcion.id, maximo: 160),
                  textoValido(funcion.nombre, maximo: 240),
                  textoValido(funcion.categoria, maximo: 120),
                  !funcion.evidencia.isEmpty,
                  funcion.evidencia.count <= 4_000 else {
                throw ErrorRepositorioComparativaGoodnotesIPad
                    .funcionInvalida(funcion.id)
            }
        }
    }
}
