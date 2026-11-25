import Foundation

/// Categorías normalizadas que permiten mezclar resultados de fuentes distintas
/// sin perder el tipo de contenido que debe abrir la UI al navegar.
enum CategoriaBusquedaUniversalIPad: String, Codable, CaseIterable, Hashable {
    case manuscrito
    case texto
    case pdf
    case ocr
    case transcripcion
    case formula
    case problema
    case error
    case etiqueta
    case fuente
    case objeto

    var nombre: String {
        switch self {
        case .manuscrito: return "Escritura"
        case .texto: return "Texto"
        case .pdf: return "PDF"
        case .ocr: return "OCR"
        case .transcripcion: return "Transcripción"
        case .formula: return "Fórmula"
        case .problema: return "Problema"
        case .error: return "Error"
        case .etiqueta: return "Etiqueta"
        case .fuente: return "Fuente"
        case .objeto: return "Objeto"
        }
    }
}

/// Una localización puede ser parcial: no todos los proveedores conocen página,
/// frame o segundo de audio. La UI debe usar el locator más específico disponible.
struct LocalizacionBusquedaUniversalIPad: Codable, Equatable, Hashable {
    var documentoId: UInt64?
    var pagina: Int?
    var frameId: UUID?
    var problemaId: UUID?
    var segundoAudio: Double?
    var contexto: String?

    init(
        documentoId: UInt64? = nil,
        pagina: Int? = nil,
        frameId: UUID? = nil,
        problemaId: UUID? = nil,
        segundoAudio: Double? = nil,
        contexto: String? = nil
    ) {
        self.documentoId = documentoId
        self.pagina = pagina
        self.frameId = frameId
        self.problemaId = problemaId
        self.segundoAudio = segundoAudio
        self.contexto = contexto
    }
}

/// Contrato común para cualquier fragmento indexable. `claveEstable` debe ser
/// estable entre reaperturas para que un reindexado reemplace en vez de duplicar.
struct EntradaBusquedaUniversalIPad: Codable, Equatable, Hashable, Identifiable {
    var id: String { claveEstable }
    var claveEstable: String
    var categoria: CategoriaBusquedaUniversalIPad
    var titulo: String
    var contenido: String
    var palabrasClave: [String]
    var cursoId: UUID?
    var fuente: String?
    var fecha: Date
    var localizacion: LocalizacionBusquedaUniversalIPad

    init(
        claveEstable: String,
        categoria: CategoriaBusquedaUniversalIPad,
        titulo: String,
        contenido: String,
        palabrasClave: [String] = [],
        cursoId: UUID? = nil,
        fuente: String? = nil,
        fecha: Date = Date(),
        localizacion: LocalizacionBusquedaUniversalIPad = .init()
    ) {
        self.claveEstable = claveEstable
        self.categoria = categoria
        self.titulo = titulo
        self.contenido = contenido
        self.palabrasClave = palabrasClave
        self.cursoId = cursoId
        self.fuente = fuente
        self.fecha = fecha
        self.localizacion = localizacion
    }
}

struct ConsultaBusquedaUniversalIPad: Equatable {
    var texto: String
    var categorias: Set<CategoriaBusquedaUniversalIPad> = []
    var cursoId: UUID?
    var limite = 50

    var vacia: Bool {
        Normalizador.texto(texto).isEmpty && categorias.isEmpty && cursoId == nil
    }
}

struct ResultadoBusquedaUniversalIPad: Equatable, Identifiable {
    var id: String { entrada.claveEstable }
    var entrada: EntradaBusquedaUniversalIPad
    var puntuacion: Double
    var coincidencias: [String]
}

/// Índice en memoria, thread-safe y determinista. Los subsistemas (tinta, PDF,
/// audio, estudio, etc.) aportan entradas; este repositorio no intenta poseer ni
/// duplicar sus datos fuente.
final class IndiceBusquedaUniversalIPad {
    static let compartido = IndiceBusquedaUniversalIPad()

    private let bloqueo = NSRecursiveLock()
    private var entradas: [String: EntradaBusquedaUniversalIPad] = [:]

    init() {}

    func reemplazar(_ nuevas: [EntradaBusquedaUniversalIPad]) {
        bloqueo.lock(); defer { bloqueo.unlock() }
        entradas = Dictionary(uniqueKeysWithValues: nuevas.map { ($0.claveEstable, $0) })
    }

    func upsert(_ nuevas: [EntradaBusquedaUniversalIPad]) {
        bloqueo.lock(); defer { bloqueo.unlock() }
        for entrada in nuevas { entradas[entrada.claveEstable] = entrada }
    }

    func eliminar(claves: some Sequence<String>) {
        bloqueo.lock(); defer { bloqueo.unlock() }
        for clave in claves { entradas.removeValue(forKey: clave) }
    }

    func eliminar(prefijo: String) {
        bloqueo.lock(); defer { bloqueo.unlock() }
        for clave in entradas.keys where clave.hasPrefix(prefijo) {
            entradas.removeValue(forKey: clave)
        }
    }

    func instantanea() -> [EntradaBusquedaUniversalIPad] {
        bloqueo.lock(); defer { bloqueo.unlock() }
        return Array(entradas.values)
    }

    func buscar(_ consulta: ConsultaBusquedaUniversalIPad) -> [ResultadoBusquedaUniversalIPad] {
        let configuracion = RepositorioBusquedaTecnicaIPad.compartido.instantanea()
        let terminos = Self.terminosConsulta(consulta.texto)
        let variantes = Self.expandir(terminos: terminos, alias: configuracion.alias)
        let limite = min(max(consulta.limite, 1), 500)

        return instantanea().compactMap { entrada -> ResultadoBusquedaUniversalIPad? in
            if !consulta.categorias.isEmpty,
               !consulta.categorias.contains(entrada.categoria) { return nil }
            if let cursoId = consulta.cursoId, entrada.cursoId != cursoId { return nil }

            if terminos.isEmpty {
                return ResultadoBusquedaUniversalIPad(
                    entrada: entrada,
                    puntuacion: 0.1,
                    coincidencias: [])
            }

            let evaluacion = Self.evaluar(
                entrada: entrada,
                terminos: terminos,
                variantes: variantes,
                tolerancia: configuracion.toleranciaManuscrita)
            guard evaluacion.puntuacion > 0 else { return nil }
            return ResultadoBusquedaUniversalIPad(
                entrada: entrada,
                puntuacion: evaluacion.puntuacion,
                coincidencias: evaluacion.coincidencias)
        }
        .sorted { izquierda, derecha in
            if abs(izquierda.puntuacion - derecha.puntuacion) > 0.000_001 {
                return izquierda.puntuacion > derecha.puntuacion
            }
            if izquierda.entrada.fecha != derecha.entrada.fecha {
                return izquierda.entrada.fecha > derecha.entrada.fecha
            }
            return izquierda.entrada.claveEstable < derecha.entrada.claveEstable
        }
        .prefix(limite)
        .map { $0 }
    }

    private static func terminosConsulta(_ texto: String) -> [String] {
        let normal = Normalizador.texto(texto)
        guard !normal.isEmpty else { return [] }
        return normal.split(whereSeparator: { $0.isWhitespace })
            .map(String.init)
            .filter { !$0.isEmpty }
    }

    private static func expandir(
        terminos: [String],
        alias: [String: [String]]
    ) -> [String: Set<String>] {
        var salida: [String: Set<String>] = [:]
        for termino in terminos {
            var grupo: Set<String> = [termino]
            for (canonico, valores) in alias {
                let familia = Set([Normalizador.texto(canonico)] + valores.map(Normalizador.texto))
                if familia.contains(termino) { grupo.formUnion(familia) }
            }
            salida[termino] = grupo
        }
        return salida
    }

    private static func evaluar(
        entrada: EntradaBusquedaUniversalIPad,
        terminos: [String],
        variantes: [String: Set<String>],
        tolerancia: Bool
    ) -> (puntuacion: Double, coincidencias: [String]) {
        let titulo = Normalizador.texto(entrada.titulo)
        let contenido = Normalizador.texto(entrada.contenido)
        let keywords = entrada.palabrasClave.map(Normalizador.texto)
        let fuente = Normalizador.texto(entrada.fuente ?? "")

        var total = 0.0
        var halladas: [String] = []

        for termino in terminos {
            let familia = variantes[termino] ?? [termino]
            var mejor = 0.0

            for variante in familia {
                if titulo == variante { mejor = max(mejor, 8.0) }
                if titulo.contains(variante) { mejor = max(mejor, 6.0) }
                if keywords.contains(variante) { mejor = max(mejor, 5.5) }
                if keywords.contains(where: { $0.contains(variante) }) {
                    mejor = max(mejor, 4.5)
                }
                if contenido.contains(variante) { mejor = max(mejor, 3.5) }
                if fuente.contains(variante) { mejor = max(mejor, 2.5) }

                if tolerancia && mejor == 0 {
                    let candidatos = Self.tokens(titulo)
                        + Self.tokens(contenido)
                        + keywords.flatMap(Self.tokens)
                    if candidatos.contains(where: {
                        Self.coincidenciaAproximada(variante, $0)
                    }) {
                        mejor = max(mejor, 1.5)
                    }
                }
            }

            guard mejor > 0 else { return (0, []) }
            total += mejor
            halladas.append(termino)
        }

        // Bonificaciones explicables: coincidencia de frase y categorías de
        // conocimiento estructurado. No hay ranking ML oculto.
        let frase = Normalizador.texto(terminos.joined(separator: " "))
        if terminos.count > 1 && (titulo.contains(frase) || contenido.contains(frase)) {
            total += 4
        }
        switch entrada.categoria {
        case .problema, .formula, .error: total += 0.4
        case .transcripcion, .manuscrito, .ocr: total += 0.2
        default: break
        }

        return (total, halladas)
    }

    private static func tokens(_ texto: String) -> [String] {
        Normalizador.texto(texto)
            .split(whereSeparator: { !$0.isLetter && !$0.isNumber })
            .map(String.init)
    }

    private static func coincidenciaAproximada(_ a: String, _ b: String) -> Bool {
        guard !a.isEmpty, !b.isEmpty else { return false }
        if a == b { return true }
        let largo = max(a.count, b.count)
        let tolerancia = largo <= 4 ? 1 : (largo <= 8 ? 2 : 3)
        return distanciaDamerau(a, b) <= tolerancia
    }

    private static func distanciaDamerau(_ a: String, _ b: String) -> Int {
        let izquierda = Array(a), derecha = Array(b)
        if izquierda.isEmpty { return derecha.count }
        if derecha.isEmpty { return izquierda.count }

        var matriz = Array(
            repeating: Array(repeating: 0, count: derecha.count + 1),
            count: izquierda.count + 1)
        for i in 0...izquierda.count { matriz[i][0] = i }
        for j in 0...derecha.count { matriz[0][j] = j }

        for i in 1...izquierda.count {
            for j in 1...derecha.count {
                let costo = izquierda[i - 1] == derecha[j - 1] ? 0 : 1
                matriz[i][j] = min(
                    matriz[i - 1][j] + 1,
                    matriz[i][j - 1] + 1,
                    matriz[i - 1][j - 1] + costo)
                if i > 1, j > 1,
                   izquierda[i - 1] == derecha[j - 2],
                   izquierda[i - 2] == derecha[j - 1] {
                    matriz[i][j] = min(matriz[i][j], matriz[i - 2][j - 2] + 1)
                }
            }
        }
        return matriz[izquierda.count][derecha.count]
    }
}

/// Primer proveedor real: hace que Problem Bank, fórmulas, errores y dudas
/// entren al mismo índice. Los agentes propietarios de tinta/PDF/audio pueden
/// añadir sus proveedores sin modificar ni duplicar este motor.
enum ProveedorBusquedaEstudioTecnicoIPad {
    static func construir(
        desde instantanea: InstantaneaEstudioTecnicoIPad
    ) -> [EntradaBusquedaUniversalIPad] {
        var salida: [EntradaBusquedaUniversalIPad] = []

        for problema in instantanea.problemas where problema.estado != .archivado {
            salida.append(EntradaBusquedaUniversalIPad(
                claveEstable: "problem-bank:\(problema.id.uuidString)",
                categoria: .problema,
                titulo: problema.tema,
                contenido: problema.enunciado,
                palabrasClave: problema.etiquetas,
                cursoId: problema.cursoId,
                fuente: problema.fuente?.titulo,
                fecha: problema.ultimoUso ?? problema.fecha,
                localizacion: .init(problemaId: problema.id, contexto: problema.tema)))

            for error in problema.erroresDePaso {
                salida.append(EntradaBusquedaUniversalIPad(
                    claveEstable: "problem-error:\(problema.id.uuidString):\(error.id.uuidString)",
                    categoria: .error,
                    titulo: error.descripcion,
                    contenido: [error.respuestaDada, error.correccion]
                        .filter { !$0.isEmpty }.joined(separator: "\n"),
                    palabrasClave: problema.etiquetas,
                    cursoId: problema.cursoId,
                    fuente: problema.fuente?.titulo,
                    fecha: error.fecha,
                    localizacion: .init(
                        problemaId: problema.id,
                        segundoAudio: error.segundoAudio,
                        contexto: error.descripcion)))
            }
        }

        for formula in instantanea.formulas {
            salida.append(EntradaBusquedaUniversalIPad(
                claveEstable: "formula:\(formula.id.uuidString)",
                categoria: .formula,
                titulo: formula.nombre.isEmpty ? formula.expresion : formula.nombre,
                contenido: [formula.expresion, formula.parcial, formula.tema]
                    .filter { !$0.isEmpty }.joined(separator: "\n"),
                palabrasClave: [formula.tema, formula.parcial].filter { !$0.isEmpty },
                cursoId: formula.cursoId,
                fuente: formula.fuente?.titulo,
                fecha: formula.fecha,
                localizacion: .init(contexto: formula.expresion)))
        }

        for duda in instantanea.dudas {
            salida.append(EntradaBusquedaUniversalIPad(
                claveEstable: "duda:\(duda.id.uuidString)",
                categoria: .texto,
                titulo: duda.pregunta,
                contenido: [duda.contexto, duda.respuesta]
                    .filter { !$0.isEmpty }.joined(separator: "\n"),
                cursoId: duda.cursoId,
                fuente: duda.fuente?.titulo,
                fecha: duda.fecha,
                localizacion: .init(contexto: "Duda para profesor")))
        }

        return salida
    }

    static func reindexar(
        centro: CentroEstudioTecnicoIPad = .compartido,
        indice: IndiceBusquedaUniversalIPad = .compartido
    ) {
        indice.eliminar(prefijo: "problem-bank:")
        indice.eliminar(prefijo: "problem-error:")
        indice.eliminar(prefijo: "formula:")
        indice.eliminar(prefijo: "duda:")
        indice.upsert(construir(desde: centro.instantanea()))
    }
}
