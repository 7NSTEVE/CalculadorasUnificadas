import UIKit

/// Actualiza únicamente estado de vista. No persiste nada ni modifica tinta.
/// Se engancha al mismo canal de foco que ya usa el enunciado y se apaga cuando
/// cambia el frame, termina la sesión o el usuario quita el foco.
final class ActualizadorHUDSimulacroIPad {
    static let compartido = ActualizadorHUDSimulacroIPad()

    private var temporizador: Timer?
    private var trabajoInicio: DispatchWorkItem?
    private var documento: UInt64?
    private var frameId: UUID?
    private var region: CGRect?
    private var objetivo = ""

    private init() {}

    func programarSiCorresponde(
        documento: UInt64,
        region: CGRect,
        demora: TimeInterval
    ) {
        trabajoInicio?.cancel()
        let trabajo = DispatchWorkItem { [weak self] in
            self?.iniciarSiCorresponde(documento: documento, region: region)
        }
        trabajoInicio = trabajo
        DispatchQueue.main.asyncAfter(deadline: .now() + max(demora, 0), execute: trabajo)
    }

    func detener(documento: UInt64? = nil) {
        if let documento, self.documento != nil, self.documento != documento { return }
        trabajoInicio?.cancel()
        trabajoInicio = nil
        temporizador?.invalidate()
        temporizador = nil
        self.documento = nil
        frameId = nil
        region = nil
        objetivo = ""
    }

    private func iniciarSiCorresponde(documento: UInt64, region: CGRect) {
        guard let sesion = RepositorioSimulacroDocumentoIPad.compartido
            .cargar(documento: documento),
              let actualId = sesion.frameActualId,
              let frame = RepositorioNavegacionEspacialIPad.compartido
                .instantanea(documento).frames.first(where: {
                    $0.id == actualId && $0.region == region
                }),
              let estudio = frame.estudio else {
            detener(documento: documento)
            return
        }

        detener()
        self.documento = documento
        frameId = frame.id
        self.region = region
        objetivo = estudio.objetivo.isEmpty ? frame.titulo : estudio.objetivo
        emitir()

        let timer = Timer(timeInterval: 1, repeats: true) { [weak self] _ in
            self?.emitir()
        }
        RunLoop.main.add(timer, forMode: .common)
        temporizador = timer
    }

    private func emitir() {
        guard let documento, let frameId, let region else {
            detener(); return
        }
        let ahora = Date()
        guard let sesion = RepositorioSimulacroDocumentoIPad.compartido
            .cargar(documento: documento, ahora: ahora) else {
            detener(documento: documento)
            return
        }

        guard sesion.frameActualId == frameId else {
            if sesion.frameActualId == nil {
                publicar(
                    estado: "Simulacro terminado · 00:00",
                    documento: documento,
                    region: region)
            }
            detener(documento: documento)
            return
        }

        let posicion = (sesion.plan.frameIds.firstIndex(of: frameId) ?? 0) + 1
        let restantes = max(Int(ceil(sesion.segundosRestantes(ahora: ahora))), 0)
        let reloj = String(format: "%02d:%02d", restantes / 60, restantes % 60)
        publicar(
            estado: "Simulacro \(posicion)/\(sesion.plan.cantidad) · \(reloj)",
            documento: documento,
            region: region)
    }

    private func publicar(estado: String, documento: UInt64, region: CGRect) {
        CanalFocoPaginaActualIPad.activar(
            documento: documento,
            region: region,
            guia: ContenidoGuiaFocoIPad.codificar(
                enunciado: objetivo,
                estado: estado))
    }

    deinit {
        trabajoInicio?.cancel()
        temporizador?.invalidate()
    }
}
