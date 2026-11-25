import Foundation

enum TipoEntradaSistemaIPad: String, Codable {
    case texto
    case enlace
    case archivo
    case imagen
    case audio
    case nuevaNota
    case continuar
    case capturarFoto
    case capturarVoz
}

struct EntradaSistemaIPad: Codable, Identifiable {
    var id = UUID()
    var fecha = Date()
    var tipo: TipoEntradaSistemaIPad
    var texto: String?
    var archivo: String?
    var nombreOriginal: String?
    var tipoUTI: String?
    var destino = "Inbox"
    var cursoSugerido: String?
}

struct EstadoWidgetTintaIPad: Codable {
    var actualizado = Date()
    var proximaClase = "Sin próxima clase confirmada"
    var continuar = "Continuar donde estaba"
    var proximaEvaluacion = "Sin evaluación próxima confirmada"
    var accion = "Abrir Tinta"
}

struct EntradaPendienteSistemaIPad {
    let entrada: EntradaSistemaIPad
    let adjunto: URL?
    fileprivate let manifiesto: URL
}

enum BuzonCompartidoSistemaIPad {
    static let grupo = "group.com.calculadorasunificadas.tinta"

    private static var raiz: URL? {
        FileManager.default.containerURL(forSecurityApplicationGroupIdentifier: grupo)
    }

    private static var pendientes: URL? {
        raiz?.appendingPathComponent("Entradas/Pendientes", isDirectory: true)
    }

    private static var procesadas: URL? {
        raiz?.appendingPathComponent("Entradas/Procesadas", isDirectory: true)
    }

    static func publicar(_ entrada: EntradaSistemaIPad, datosArchivo: Data? = nil) throws {
        guard let pendientes, let raiz else {
            throw NSError(domain: "TintaCompartido", code: 1,
                          userInfo: [NSLocalizedDescriptionKey:
                            "El contenedor compartido de Tinta no está disponible."])
        }
        try FileManager.default.createDirectory(at: pendientes,
                                                withIntermediateDirectories: true)
        var final = entrada
        if let datosArchivo {
            let carpeta = raiz.appendingPathComponent("Entradas/Archivos", isDirectory: true)
            try FileManager.default.createDirectory(at: carpeta,
                                                    withIntermediateDirectories: true)
            let extensionArchivo = entrada.nombreOriginal.map {
                URL(fileURLWithPath: $0).pathExtension
            }.flatMap { $0.isEmpty ? nil : $0 }
            let nombre = entrada.id.uuidString
                + (extensionArchivo.map { ".\($0)" } ?? "")
            let destino = carpeta.appendingPathComponent(nombre)
            try datosArchivo.write(to: destino, options: .atomic)
            final.archivo = "Entradas/Archivos/\(nombre)"
        }
        let datos = try JSONEncoder().encode(final)
        try datos.write(to: pendientes.appendingPathComponent(
            "\(final.id.uuidString).json"), options: .atomic)
    }

    /// Lee sin retirar. El manifiesto solo pasa a Procesadas cuando el receptor
    /// confirma que ya guardó el texto o copió el adjunto; así un fallo de
    /// almacenamiento no hace desaparecer una captura compartida.
    static func pendientesSinConfirmar() -> [EntradaPendienteSistemaIPad] {
        guard let pendientes, let raiz else { return [] }
        let archivos = (try? FileManager.default.contentsOfDirectory(
            at: pendientes, includingPropertiesForKeys: nil,
            options: [.skipsHiddenFiles])) ?? []
        var salida: [EntradaPendienteSistemaIPad] = []
        for manifiesto in archivos where manifiesto.pathExtension == "json" {
            guard let datos = try? Data(contentsOf: manifiesto),
                  let entrada = try? JSONDecoder().decode(EntradaSistemaIPad.self,
                                                          from: datos) else { continue }
            let adjunto = entrada.archivo.map { raiz.appendingPathComponent($0) }
            salida.append(EntradaPendienteSistemaIPad(
                entrada: entrada, adjunto: adjunto, manifiesto: manifiesto))
        }
        return salida.sorted { $0.entrada.fecha < $1.entrada.fecha }
    }

    @discardableResult
    static func confirmar(_ pendiente: EntradaPendienteSistemaIPad) -> Bool {
        guard let procesadas else { return false }
        do {
            try FileManager.default.createDirectory(at: procesadas,
                                                    withIntermediateDirectories: true)
            let destino = procesadas.appendingPathComponent(
                pendiente.manifiesto.lastPathComponent)
            try FileManager.default.moveItem(at: pendiente.manifiesto, to: destino)
            return true
        } catch {
            return false
        }
    }

    static func guardarEstadoWidget(_ estado: EstadoWidgetTintaIPad) {
        guard let raiz, let datos = try? JSONEncoder().encode(estado) else { return }
        try? datos.write(to: raiz.appendingPathComponent("estado-widget.json"),
                         options: .atomic)
    }

    static func estadoWidget() -> EstadoWidgetTintaIPad {
        guard let raiz, let datos = try? Data(contentsOf: raiz.appendingPathComponent(
            "estado-widget.json")),
              let estado = try? JSONDecoder().decode(EstadoWidgetTintaIPad.self,
                                                      from: datos) else {
            return EstadoWidgetTintaIPad()
        }
        return estado
    }
}
