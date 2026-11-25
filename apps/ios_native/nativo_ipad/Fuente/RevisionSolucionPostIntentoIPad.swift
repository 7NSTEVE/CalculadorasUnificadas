import Foundation

enum EstadoAccesoSolucionPostIntentoIPad: Equatable {
    case permitido(intentoId: UUID)
    case sinVinculoProblemBank
    case problemaNoDisponible
    case sinSolucion
    case intentoActivo
    case sinIntentoTerminado
}

enum PoliticaAccesoSolucionPostIntentoIPad {
    static func evaluar(
        estudio: EstadoEstudioFrameIPad,
        problema: ProblemaBancoIPad?
    ) -> EstadoAccesoSolucionPostIntentoIPad {
        guard let intento = estudio.intentosTerminados.last else {
            if estudio.intentoActivo != nil { return .intentoActivo }
            return estudio.problemaId == nil ? .sinVinculoProblemBank : .sinIntentoTerminado
        }
        return evaluar(
            estudio: estudio,
            problema: problema,
            intentoId: intento.id)
    }

    /// Variante histórica: permite revisar el intento exacto de un simulacro
    /// anterior aunque el frame tenga intentos terminados más recientes. Un
    /// intento actualmente activo sigue bloqueando cualquier revelado.
    static func evaluar(
        estudio: EstadoEstudioFrameIPad,
        problema: ProblemaBancoIPad?,
        intentoId: UUID
    ) -> EstadoAccesoSolucionPostIntentoIPad {
        guard estudio.problemaId != nil else { return .sinVinculoProblemBank }
        guard let problema else { return .problemaNoDisponible }
        guard estudio.intentoActivo == nil else { return .intentoActivo }
        guard estudio.intentosTerminados.contains(where: { $0.id == intentoId }) else {
            return .sinIntentoTerminado
        }
        guard let solucion = problema.solucion,
              solucionDisponible(solucion) else { return .sinSolucion }
        return .permitido(intentoId: intentoId)
    }

    static func solucionDisponible(_ solucion: SolucionVinculadaIPad) -> Bool {
        if !solucion.texto.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty {
            return true
        }
        if let hash = solucion.hashAdjunto?.trimmingCharacters(in: .whitespacesAndNewlines),
           !hash.isEmpty { return true }
        if let hash = solucion.fuente?.hashArchivo?.trimmingCharacters(in: .whitespacesAndNewlines),
           !hash.isEmpty { return true }
        return false
    }
}

enum ResultadoRevisionSolucionIPad: String, Codable, CaseIterable {
    case coincide
    case diferencia
    case duda

    var titulo: String {
        switch self {
        case .coincide: return "Mi procedimiento coincide"
        case .diferencia: return "Encontré una diferencia"
        case .duda: return "Me quedó una duda"
        }
    }
}

struct RevisionSolucionPostIntentoIPad: Codable, Identifiable, Equatable {
    var id = UUID()
    var documentoId: UInt64
    var frameId: UUID
    var problemaId: UUID
    var intentoId: UUID
    var fecha = Date()
    var resultado: ResultadoRevisionSolucionIPad
    var paso = ""
    var correccion = ""
    var nota = ""

    mutating func normalizar() {
        paso = paso.trimmingCharacters(in: .whitespacesAndNewlines)
        correccion = correccion.trimmingCharacters(in: .whitespacesAndNewlines)
        nota = nota.trimmingCharacters(in: .whitespacesAndNewlines)
    }
}

/// Sidecar separado de la tinta y del Problem Bank. Guarda qué aprendió el
/// usuario al comparar su intento ya terminado con la solución. La clave lógica
/// es `frame + intento`: volver a guardar la misma revisión actualiza esa fila,
/// no duplica el historial.
final class RepositorioRevisionesSolucionIPad {
    static let compartido = RepositorioRevisionesSolucionIPad()

    private let base: URL
    private let archivos: FileManager
    private let cola = DispatchQueue(label: "cuadernu.revision-solucion")

    init(base: URL? = nil, archivos: FileManager = .default) {
        self.archivos = archivos
        if let base {
            self.base = base
        } else {
            let soporte = archivos.urls(
                for: .applicationSupportDirectory,
                in: .userDomainMask)[0]
            self.base = soporte.appendingPathComponent(
                "Tinta/RevisionesSolucion", isDirectory: true)
        }
        try? archivos.createDirectory(at: self.base,
                                      withIntermediateDirectories: true)
    }

    func revisiones(documento: UInt64) -> [RevisionSolucionPostIntentoIPad] {
        cola.sync { cargarSinBloqueo(documento) }
    }

    func revision(
        documento: UInt64,
        frameId: UUID,
        intentoId: UUID
    ) -> RevisionSolucionPostIntentoIPad? {
        revisiones(documento: documento).last {
            $0.frameId == frameId && $0.intentoId == intentoId
        }
    }

    @discardableResult
    func guardar(_ entrada: RevisionSolucionPostIntentoIPad) -> Bool {
        cola.sync {
            var nueva = entrada
            nueva.normalizar()
            var filas = cargarSinBloqueo(nueva.documentoId)
            if let indice = filas.firstIndex(where: {
                $0.frameId == nueva.frameId && $0.intentoId == nueva.intentoId
            }) {
                nueva.id = filas[indice].id
                filas[indice] = nueva
            } else {
                filas.append(nueva)
            }
            return persistirSinBloqueo(filas, documento: nueva.documentoId)
        }
    }

    private func url(_ documento: UInt64) -> URL {
        base.appendingPathComponent("\(documento).json")
    }

    private func cargarSinBloqueo(_ documento: UInt64)
        -> [RevisionSolucionPostIntentoIPad] {
        guard let datos = try? Data(contentsOf: url(documento)),
              let filas = try? JSONDecoder().decode(
                [RevisionSolucionPostIntentoIPad].self, from: datos) else { return [] }
        return filas.sorted { $0.fecha < $1.fecha }
    }

    private func persistirSinBloqueo(
        _ filas: [RevisionSolucionPostIntentoIPad],
        documento: UInt64
    ) -> Bool {
        do {
            let encoder = JSONEncoder()
            encoder.outputFormatting = [.sortedKeys]
            let datos = try encoder.encode(filas)
            try datos.write(to: url(documento), options: .atomic)
            return true
        } catch {
            return false
        }
    }
}

enum ResultadoRegistroRevisionSolucionIPad: Equatable {
    case guardada
    case guardadaYReflejadaComoError
    case noSePudoGuardar
}

enum ServicioRevisionSolucionPostIntentoIPad {
    static func marcador(intentoId: UUID) -> String {
        "[revision-solucion:\(intentoId.uuidString)]"
    }

    @discardableResult
    static func guardar(
        _ revision: RevisionSolucionPostIntentoIPad,
        repositorio: RepositorioRevisionesSolucionIPad = .compartido
    ) -> ResultadoRegistroRevisionSolucionIPad {
        guard repositorio.guardar(revision) else { return .noSePudoGuardar }
        guard revision.resultado == .diferencia,
              !revision.paso.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty else {
            return .guardada
        }

        let centro = CentroEstudioTecnicoIPad.compartido
        guard let problema = centro.instantanea().problemas.first(where: {
            $0.id == revision.problemaId
        }) else { return .guardada }

        let marca = marcador(intentoId: revision.intentoId)
        guard !problema.erroresDePaso.contains(where: { $0.elementoId == marca }) else {
            return .guardadaYReflejadaComoError
        }

        centro.registrarError(
            PasoErrorProblemaIPad(
                fecha: revision.fecha,
                descripcion: revision.paso,
                respuestaDada: "",
                correccion: revision.correccion,
                elementoId: marca),
            problemaId: revision.problemaId)
        return .guardadaYReflejadaComoError
    }
}
