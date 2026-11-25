import Foundation

enum AdvertenciaIntegracionAudioResilienteIPad: LocalizedError {
    case noSePudoPreparar(String)
    case noSePudoVincular(String)
    case noSePudoActualizar(String)
    case grabacionConservadaPeroSidecarFallo(String)

    var errorDescription: String? {
        switch self {
        case .noSePudoPreparar(let detalle):
            return "La grabación puede continuar, pero no se pudo preparar su journal de recovery: \(detalle)"
        case .noSePudoVincular(let detalle):
            return "La grabación comenzó, pero su archivo en curso no pudo vincularse al journal de recovery: \(detalle)"
        case .noSePudoActualizar(let detalle):
            return "La grabación continúa, pero el estado auxiliar de recovery no pudo actualizarse: \(detalle)"
        case .grabacionConservadaPeroSidecarFallo(let detalle):
            return "El archivo principal se conservó, pero no pudo confirmarse en el store resiliente: \(detalle)"
        }
    }
}

/// Adaptador entre la máquina de estados real de grabación y los sidecars de
/// recovery. Nunca captura audio por sí mismo y nunca sustituye a GrabadoraAudio
/// ni a GrabadoraNotaViva.
final class IntegradorAudioResilienteGrabacionIPad {
    private let audio: RepositorioAudioResilienteIPad
    private let vinculos: RepositorioVinculosCapturaAudioIPad
    private(set) var activo: VinculoCapturaAudioIPad?

    init(audio: RepositorioAudioResilienteIPad = .compartido,
         vinculos: RepositorioVinculosCapturaAudioIPad = .compartido) {
        self.audio = audio
        self.vinculos = vinculos
    }

    @discardableResult
    func preparar(documentoId: UInt64,
                  modo: ModoCaptura,
                  microfono: Bool) -> Error? {
        activo = nil
        guard microfono else { return nil }
        let modoResiliente: ModoCapturaAudioResilienteIPad
        switch modo {
        case .soloAudio: modoResiliente = .soloAudio
        case .notaViva: modoResiliente = .notaViva
        case .pantallaConAudio: return nil
        }
        do {
            activo = try vinculos.crear(
                documentoId: documentoId,
                modo: modoResiliente,
                repositorioAudio: audio)
            return nil
        } catch {
            return AdvertenciaIntegracionAudioResilienteIPad
                .noSePudoPreparar(error.localizedDescription)
        }
    }

    /// Se llama sólo después de que el capturador real confirmó inicio.
    @discardableResult
    func vincularFuente(_ fuente: URL?, modo: ModoCaptura) -> Error? {
        guard let id = activo?.sesionAudioId else { return nil }
        guard let fuente else {
            return AdvertenciaIntegracionAudioResilienteIPad
                .noSePudoVincular("el capturador no publicó una ruta")
        }
        do {
            activo = try vinculos.vincularFuente(
                fuente,
                sesion: id,
                esManifiestoNotaViva: modo == .notaViva)
            return nil
        } catch {
            try? vinculos.marcarRecovery(
                id, motivo: "No se pudo vincular la fuente al comenzar: \(error.localizedDescription)")
            return AdvertenciaIntegracionAudioResilienteIPad
                .noSePudoVincular(error.localizedDescription)
        }
    }

    @discardableResult
    func pausar() -> Error? {
        guard let id = activo?.sesionAudioId else { return nil }
        do {
            try audio.pausar(id)
            activo = try vinculos.marcarPausa(id)
            return nil
        } catch {
            return AdvertenciaIntegracionAudioResilienteIPad
                .noSePudoActualizar(error.localizedDescription)
        }
    }

    @discardableResult
    func reanudar() -> Error? {
        guard let id = activo?.sesionAudioId else { return nil }
        do {
            try audio.reanudar(id)
            activo = try vinculos.marcarReanudacion(id)
            return nil
        } catch {
            return AdvertenciaIntegracionAudioResilienteIPad
                .noSePudoActualizar(error.localizedDescription)
        }
    }

    /// Una preparación que nunca llegó a grabar no debe reaparecer como crash.
    func cancelarPreparacion(motivo: String) {
        guard let id = activo?.sesionAudioId else { return }
        try? vinculos.marcarCancelado(id, motivo: motivo)
        // El manifest sin segmentos es íntegro y puede cerrarse de forma segura.
        try? audio.finalizar(id)
        activo = nil
    }

    /// Confirma únicamente el archivo que el capturador principal ya declaró
    /// exitoso. El resultado principal nunca se borra si este sidecar falla.
    @discardableResult
    func confirmarFinal(
        resultadoPrincipal: URL,
        modo: ModoCaptura,
        duracion: Double,
        ajustes: TintaAjustesGrabacion
    ) -> Error? {
        guard let id = activo?.sesionAudioId else { return nil }
        do {
            let fuente: URL
            switch modo {
            case .soloAudio:
                fuente = resultadoPrincipal
            case .notaViva:
                guard let resuelta = try ResolutorAudioFinalNotaVivaIPad
                    .resolver(manifiestoURL: resultadoPrincipal) else {
                    throw ErrorVinculoCapturaAudioIPad.archivoFinalNoDisponible
                }
                fuente = resuelta
            case .pantallaConAudio:
                activo = nil
                return nil
            }

            let ext = fuente.pathExtension.isEmpty ? "m4a" : fuente.pathExtension
            let codec = ajustes.canalesElegidos == .espacial
                ? "audio-espacial-foa" : "aac"
            _ = try audio.confirmarSegmento(
                sesion: id,
                archivoTemporal: fuente,
                inicioSegundos: 0,
                duracionSegundos: max(duracion, 0.001),
                codec: codec,
                extensionArchivo: ext,
                sampleRate: Double(ajustes.frecuencia_hz),
                canales: max(ajustes.canalesDeAudio, 1))
            try audio.finalizar(id)
            activo = try vinculos.marcarConfirmado(id)
            activo = nil
            return nil
        } catch {
            try? vinculos.marcarRecovery(
                id,
                motivo: "El archivo principal terminó, pero su copia resiliente no pudo confirmarse: \(error.localizedDescription)")
            activo = nil
            return AdvertenciaIntegracionAudioResilienteIPad
                .grabacionConservadaPeroSidecarFallo(error.localizedDescription)
        }
    }

    func registrarFalloFinal(_ error: Error) {
        guard let id = activo?.sesionAudioId else { return }
        try? vinculos.marcarRecovery(
            id,
            motivo: "El capturador principal terminó con error: \(error.localizedDescription)")
        activo = nil
    }
}
