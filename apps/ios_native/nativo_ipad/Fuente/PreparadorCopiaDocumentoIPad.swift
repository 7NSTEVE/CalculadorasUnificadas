import Foundation

/// Construye una copia completa antes de crear su UI y coordina la publicación
/// del blob `.tinta` con el agregado Swift.
///
/// La ruta histórica construía primero un `ControladorLienzo`. Eso hacía que
/// capas como bloques, objetos técnicos y regiones de fondo cachearan un destino
/// vacío en sus inicializadores; el agregado se copiaba después y la pestaña
/// recién creada podía verse incompleta hasta reabrirla. Además, si la escritura
/// canónica del `.tinta` fallaba después de copiar sidecars, quedaban archivos
/// huérfanos bajo un UUID que nunca entró a la biblioteca.
///
/// Esta frontera trabaja únicamente con `Sesion`: primero produce los bytes
/// finales y el agregado, después publica el archivo canónico y solo entonces el
/// llamador debe construir `ControladorLienzo`/`VistaLienzoNavegable`.
enum PreparadorCopiaDocumentoIPad {
    enum Fallo: LocalizedError {
        case noCreaSesion
        case cargaRechazada
        case noSerializa

        var errorDescription: String? {
            switch self {
            case .noCreaSesion:
                return "No se pudo crear la sesión temporal de la copia."
            case .cargaRechazada:
                return "El documento no pudo cargarse como una copia aislada."
            case .noSerializa:
                return "La copia no pudo serializarse antes de publicarse."
            }
        }
    }

    /// Prepara bytes + sidecars usando una sesión sin UI. Si falla cualquier
    /// etapa posterior a haber tocado el agregado, intenta retirarlo antes de
    /// devolver el error.
    static func preparar(_ origen: Data,
                         destino: DocumentoPersistenteID) throws -> Data {
        guard let sesion = Sesion(idPersistente: destino) else {
            throw Fallo.noCreaSesion
        }
        guard sesion.cargarComoCopia(origen) else {
            _ = CoordinadorDuplicacionAgregadoIPad.compartido
                .descartarCopia(documento: destino.claveCompatibilidadUInt64)
            throw Fallo.cargaRechazada
        }
        guard let datos = sesion.guardar() else {
            _ = CoordinadorDuplicacionAgregadoIPad.compartido
                .descartarCopia(documento: destino.claveCompatibilidadUInt64)
            throw Fallo.noSerializa
        }
        return datos
    }

    /// Publica la copia como una transacción superior. El closure representa la
    /// única escritura canónica que decide si ese UUID llega a existir para la
    /// biblioteca. Un error del closure conserva su tipo original, pero antes se
    /// compensa el agregado documental del destino.
    @discardableResult
    static func publicar(
        _ origen: Data,
        destino: DocumentoPersistenteID,
        escrituraCanonica: (Data) throws -> Void
    ) throws -> Data {
        let datos = try preparar(origen, destino: destino)
        do {
            try escrituraCanonica(datos)
            return datos
        } catch {
            _ = CoordinadorDuplicacionAgregadoIPad.compartido
                .descartarCopia(documento: destino.claveCompatibilidadUInt64)
            throw error
        }
    }
}
