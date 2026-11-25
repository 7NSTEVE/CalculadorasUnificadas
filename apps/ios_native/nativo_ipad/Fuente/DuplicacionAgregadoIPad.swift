import Foundation

/// Duplica el estado documental que vive fuera del blob `.tinta`.
///
/// El núcleo C++ ya crea una identidad nueva para la página al usar
/// `cargarComoCopia`. Este coordinador completa la operación con los sidecars
/// que pertenecen al cuaderno. No copia estados efímeros como undo, sesiones de
/// colaboración, cachés ni el progreso momentáneo de revelado de Active Recall.
final class CoordinadorDuplicacionAgregadoIPad {
    static let compartido = CoordinadorDuplicacionAgregadoIPad()

    enum Paso: Equatable, CaseIterable {
        case privacidad
        case notas
        case semantica
        case navegacion
        case capas
        case recall
        case fondos
        case regionesFondo
        case edicionesImagen
        case clipsPDF
        case metadatosSeleccion
        case ajustesTexto
        case bloques
        case tecnicos
        case esquemaCircuito
        case procedenciaPPTX
    }

    struct Resumen: Equatable {
        var privacidad = 0
        var capas = 0
        var frames = 0
        var escenas = 0
        var favoritos = 0
        var cubiertasRecall = 0
        var ajustesFondos = 0
        var regionesFondo = 0
        var edicionesImagen = 0
        var clipsPDF = 0
        var metadatosSeleccion = 0
        var ajustesTexto = 0
        var bloques = 0
        var objetosTecnicos = 0
        var esquemaCircuito = 0
        var objetosPPTX = 0
        var notasAdhesivas = 0
        var conexionesNotas = 0
        var marcacionesSemanticas = 0
        var referenciasSemanticas = 0
    }

    enum Fallo: LocalizedError {
        case mismoDocumento

        var errorDescription: String? {
            switch self {
            case .mismoDocumento:
                return "La copia debe tener una identidad documental distinta."
            }
        }
    }

    private init() {}

    @discardableResult
    func duplicar(
        desde origen: UInt64,
        hacia destino: UInt64,
        despuesDePaso: ((Paso) throws -> Void)? = nil
    ) throws -> Resumen {
        guard origen != destino else { throw Fallo.mismoDocumento }
        var resumen = Resumen()
        var completados: [Paso] = []

        do {
            // La política de protección viaja primero. No se copia ninguna clave
            // del Keychain: el nuevo documento generará su propia clave aleatoria
            // cuando cifre por primera vez.
            resumen.privacidad = try DuplicadorPrivacidadCuadernoIPad.compartido
                .duplicar(desde: origen, hacia: destino)
            try registrar(.privacidad, en: &completados,
                          despuesDePaso: despuesDePaso)

            // Cada etapa confirma su escritura antes de publicar memoria. Un
            // destino con contenido real se rechaza en vez de sobrescribirse.
            let notas = try RepositorioNotasAdhesivasIPad.compartido
                .duplicar(desde: origen, hacia: destino)
            resumen.notasAdhesivas = notas.notas
            resumen.conexionesNotas = notas.conexiones
            try registrar(.notas, en: &completados, despuesDePaso: despuesDePaso)

            let semantica = try RepositorioSemanticaIPad.compartido
                .duplicar(desde: origen, hacia: destino)
            resumen.marcacionesSemanticas = semantica.marcaciones
            resumen.referenciasSemanticas = semantica.referencias
            try registrar(.semantica, en: &completados, despuesDePaso: despuesDePaso)

            let navegacion = try RepositorioNavegacionEspacialIPad.compartido
                .duplicar(desde: origen, hacia: destino)
            resumen.frames = navegacion.frames
            resumen.escenas = navegacion.escenas
            resumen.favoritos = navegacion.favoritos
            try registrar(.navegacion, en: &completados, despuesDePaso: despuesDePaso)

            resumen.capas = try RepositorioCapas.compartido
                .duplicar(desde: origen, hacia: destino)
            try registrar(.capas, en: &completados, despuesDePaso: despuesDePaso)

            resumen.cubiertasRecall = try RepositorioRecallIPad.compartido
                .duplicar(desde: origen, hacia: destino)
            try registrar(.recall, en: &completados, despuesDePaso: despuesDePaso)

            resumen.ajustesFondos = try RepositorioFondosObjetoIPad.compartido
                .duplicar(desde: origen, hacia: destino)
            try registrar(.fondos, en: &completados, despuesDePaso: despuesDePaso)

            resumen.regionesFondo = try DuplicadorRegionesFondoIPad.compartido
                .duplicar(desde: origen, hacia: destino)
            try registrar(.regionesFondo, en: &completados,
                          despuesDePaso: despuesDePaso)

            resumen.edicionesImagen = try RepositorioEdicionesImagenDocumentoIPad
                .compartido.duplicar(desde: origen, hacia: destino)
            try registrar(.edicionesImagen, en: &completados,
                          despuesDePaso: despuesDePaso)

            resumen.clipsPDF = try RepositorioClipsPDFDocumentoIPad.compartido
                .duplicar(desde: origen, hacia: destino)
            try registrar(.clipsPDF, en: &completados,
                          despuesDePaso: despuesDePaso)

            resumen.metadatosSeleccion = try DuplicadorMetadatosSeleccionIPad
                .compartido.duplicar(desde: origen, hacia: destino)
            try registrar(.metadatosSeleccion, en: &completados,
                          despuesDePaso: despuesDePaso)

            resumen.ajustesTexto = try RepositorioAjustesTextoDocumentoIPad
                .compartido.duplicar(desde: origen, hacia: destino)
            try registrar(.ajustesTexto, en: &completados,
                          despuesDePaso: despuesDePaso)

            resumen.bloques = try DuplicadorObjetosExternosTintaIPad.compartido
                .duplicarBloques(desde: origen, hacia: destino)
            try registrar(.bloques, en: &completados,
                          despuesDePaso: despuesDePaso)

            resumen.objetosTecnicos = try DuplicadorObjetosExternosTintaIPad
                .compartido.duplicarTecnicos(desde: origen, hacia: destino)
            try registrar(.tecnicos, en: &completados,
                          despuesDePaso: despuesDePaso)

            resumen.esquemaCircuito = try DuplicadorEsquemaCircuitoIPad.compartido
                .duplicar(desde: origen, hacia: destino)
            try registrar(.esquemaCircuito, en: &completados,
                          despuesDePaso: despuesDePaso)

            resumen.objetosPPTX = try RepositorioProcedenciaPPTXCopiaIPad
                .compartido.duplicar(desde: origen, hacia: destino)
            try registrar(.procedenciaPPTX, en: &completados,
                          despuesDePaso: despuesDePaso)

            return resumen
        } catch {
            revertir(completados.reversed(), documento: destino)
            throw error
        }
    }

    /// El blob `.tinta` de una copia se publica después de construir su agregado.
    /// Si esa escritura canónica falla, el UUID destino nunca llega a formar parte
    /// de la biblioteca y sus sidecars deben retirarse explícitamente. Este método
    /// también sirve para abortos de una transacción superior de pestañas.
    @discardableResult
    func descartarCopia(documento: UInt64) -> Bool {
        var completa = true
        for paso in Paso.allCases.reversed() {
            do {
                try revertir(paso, documento: documento)
            } catch {
                completa = false
                print("Limpieza de copia incompleta en \(paso): \(error.localizedDescription)")
            }
        }
        return completa
    }

    /// El cierre se invoca también desde las pruebas para simular un fallo justo
    /// después de una escritura confirmada. Registrar el paso antes del hook es
    /// esencial: si el hook falla, esa etapa ya existe en disco y debe revertirse.
    private func registrar(
        _ paso: Paso,
        en completados: inout [Paso],
        despuesDePaso: ((Paso) throws -> Void)?
    ) throws {
        completados.append(paso)
        try despuesDePaso?(paso)
    }

    private func revertir<S: Sequence>(_ pasos: S, documento: UInt64)
    where S.Element == Paso {
        for paso in pasos {
            do {
                try revertir(paso, documento: documento)
            } catch {
                // El error original de la duplicación es el que debe volver al
                // llamador. Un fallo de compensación se registra para diagnóstico
                // y no puede ocultar la causa que hizo abortar la copia.
                print("Rollback de duplicación incompleto en \(paso): \(error.localizedDescription)")
            }
        }
    }

    private func revertir(_ paso: Paso, documento: UInt64) throws {
        switch paso {
        case .privacidad:
            try DuplicadorPrivacidadCuadernoIPad.compartido
                .eliminarDocumento(documento)
        case .notas:
            let repo = RepositorioNotasAdhesivasIPad.compartido
            for nota in repo.notas(documento) {
                repo.eliminar(nota.id, documento: documento)
            }
        case .semantica:
            let repo = RepositorioSemanticaIPad.compartido
            for marca in repo.marcaciones(documento: documento) {
                repo.eliminarMarcacion(marca.id)
            }
            for referencia in repo.referencias(documento: documento) {
                repo.eliminarReferencia(referencia.id)
            }
        case .navegacion:
            let repo = RepositorioNavegacionEspacialIPad.compartido
            let datos = repo.instantanea(documento)
            let ids = datos.frames.map(\.id)
                + datos.escenas.map(\.id)
                + datos.favoritos.map(\.id)
            for id in ids { repo.eliminar(id, documento: documento) }
        case .capas:
            try RepositorioCapas.compartido.eliminarDocumento(documento)
        case .recall:
            try RepositorioRecallIPad.compartido.eliminarDocumento(documento)
        case .fondos:
            try RepositorioFondosObjetoIPad.compartido.eliminarDocumento(documento)
        case .regionesFondo:
            try DuplicadorRegionesFondoIPad.compartido
                .eliminarDocumento(documento)
        case .edicionesImagen:
            try RepositorioEdicionesImagenDocumentoIPad.compartido
                .eliminarDocumento(documento)
        case .clipsPDF:
            try RepositorioClipsPDFDocumentoIPad.compartido
                .eliminarDocumento(documento)
        case .metadatosSeleccion:
            try DuplicadorMetadatosSeleccionIPad.compartido
                .eliminarDocumento(documento)
        case .ajustesTexto:
            try RepositorioAjustesTextoDocumentoIPad.compartido
                .eliminarDocumento(documento)
        case .bloques:
            try DuplicadorObjetosExternosTintaIPad.compartido
                .eliminarBloques(documento: documento)
        case .tecnicos:
            try DuplicadorObjetosExternosTintaIPad.compartido
                .eliminarTecnicos(documento: documento)
        case .esquemaCircuito:
            try DuplicadorEsquemaCircuitoIPad.compartido
                .eliminarDocumento(documento)
        case .procedenciaPPTX:
            try RepositorioProcedenciaPPTXCopiaIPad.compartido
                .eliminarDocumento(documento)
        }
    }
}
