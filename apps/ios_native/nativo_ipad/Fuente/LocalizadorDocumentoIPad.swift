import Foundation

struct UbicacionDocumentoIPad: Equatable {
    var documentoId: UInt64
    var contextoCuaderno: String
    var panel: PanelPestanasIPad
    var pestanaId: UUID
    var titulo: String
    var abierta: Bool
}

enum LocalizadorDocumentoIPad {
    /// Resuelve la identidad durable de `Sesion` contra los manifiestos de
    /// pestañas sin abrir controladores ni materializar canvas. Incluye pestañas
    /// cerradas para que un deep link antiguo pueda reabrir su destino.
    static func localizar(
        documentoId: UInt64,
        repositorio: RepositorioPestanasIPad = .compartido,
        contextos: Set<String>? = nil
    ) -> UbicacionDocumentoIPad? {
        guard documentoId != 0 else { return nil }
        let candidatos: Set<String>
        if let contextos {
            candidatos = contextos
        } else {
            candidatos = (try? repositorio.contextosPersistidos()) ?? []
        }
        for contexto in candidatos.sorted() {
            for panel in [PanelPestanasIPad.principal, .secundario] {
                guard case .cargado(let estado) = repositorio.leer(
                    panel, contexto: contexto) else { continue }
                if let registro = estado.abiertas.first(where: {
                    clave($0.id) == documentoId
                }) {
                    return UbicacionDocumentoIPad(
                        documentoId: documentoId,
                        contextoCuaderno: contexto,
                        panel: panel,
                        pestanaId: registro.id,
                        titulo: registro.titulo,
                        abierta: true)
                }
                if let registro = estado.cerradas.first(where: {
                    clave($0.id) == documentoId
                }) {
                    return UbicacionDocumentoIPad(
                        documentoId: documentoId,
                        contextoCuaderno: contexto,
                        panel: panel,
                        pestanaId: registro.id,
                        titulo: registro.titulo,
                        abierta: false)
                }
            }
        }
        return nil
    }

    private static func clave(_ id: UUID) -> UInt64 {
        DocumentoPersistenteID(id).claveCompatibilidadUInt64
    }
}
