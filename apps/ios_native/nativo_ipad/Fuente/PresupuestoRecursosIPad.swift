import Foundation
import UIKit

struct PresupuestoMemoriaCuadernUIPad: Equatable {
    var porDocumento: Int64
    var global: Int64
    var reserva: Int64

    static func recomendado(memoriaFisica: UInt64 = ProcessInfo.processInfo.physicalMemory) -> PresupuestoMemoriaCuadernUIPad {
        let fisica = Int64(min(memoriaFisica, UInt64(Int64.max)))
        let global = min(max(fisica / 4, 384 * 1_024 * 1_024), 2 * 1_024 * 1_024 * 1_024)
        let porDocumento = min(max(fisica / 12, 128 * 1_024 * 1_024), 512 * 1_024 * 1_024)
        let reserva = max(fisica / 12, 192 * 1_024 * 1_024)
        return .init(porDocumento: porDocumento, global: global, reserva: reserva)
    }
}

protocol RecursoSuspendibleDocumentoIPad: AnyObject {
    var documentoPersistenteId: DocumentoPersistenteID { get }
    var bytesResidentesEstimados: Int64 { get }
    var bytesLiberablesEstimados: Int64 { get }
    var tieneCambiosSinGuardar: Bool { get }
    var fijadoEnMemoria: Bool { get }
    var ultimoUso: Date { get }
    func persistirAntesDeSuspender() throws
    func liberarRecursosPesados()
}

private final class CajaRecursoSuspendibleIPad {
    weak var valor: RecursoSuspendibleDocumentoIPad?
    init(_ valor: RecursoSuspendibleDocumentoIPad) { self.valor = valor }
}

struct ReporteLiberacionRecursosIPad: Equatable {
    var solicitados: Int64
    var liberadosEstimados: Int64
    var documentosSuspendidos: [DocumentoPersistenteID]
    var documentosOmitidosPorError: [DocumentoPersistenteID]
}

final class CentroPresupuestoRecursosIPad {
    static let compartido = CentroPresupuestoRecursosIPad()
    static let cambio = Notification.Name("CuadernU.PresupuestoRecursos.Cambio")

    private let bloqueo = NSRecursiveLock()
    private var recursos: [DocumentoPersistenteID: CajaRecursoSuspendibleIPad] = [:]
    var presupuesto: PresupuestoMemoriaCuadernUIPad

    init(presupuesto: PresupuestoMemoriaCuadernUIPad = .recomendado(),
         observarMemoria: Bool = true) {
        self.presupuesto = presupuesto
        if observarMemoria {
            NotificationCenter.default.addObserver(
                self, selector: #selector(memoriaBaja),
                name: UIApplication.didReceiveMemoryWarningNotification, object: nil)
        }
    }

    deinit { NotificationCenter.default.removeObserver(self) }

    func registrar(_ recurso: RecursoSuspendibleDocumentoIPad) {
        bloqueo.lock(); recursos[recurso.documentoPersistenteId] = CajaRecursoSuspendibleIPad(recurso)
        depurarSinBloqueo(); bloqueo.unlock(); publicar()
    }

    func retirar(documentoId: DocumentoPersistenteID) {
        bloqueo.lock(); recursos.removeValue(forKey: documentoId); bloqueo.unlock(); publicar()
    }

    func bytesResidentesEstimados() -> Int64 {
        bloqueo.lock(); defer { bloqueo.unlock() }; depurarSinBloqueo()
        return recursos.values.compactMap(\.valor).reduce(0) { $0 + max($1.bytesResidentesEstimados, 0) }
    }

    func excedePresupuesto(documentoId: DocumentoPersistenteID) -> Bool {
        bloqueo.lock(); defer { bloqueo.unlock() }
        guard let recurso = recursos[documentoId]?.valor else { return false }
        return recurso.bytesResidentesEstimados > presupuesto.porDocumento
    }

    @discardableResult
    func asegurarCapacidad(bytesAdicionales: Int64,
                           proteger: Set<DocumentoPersistenteID> = []) -> ReporteLiberacionRecursosIPad {
        bloqueo.lock(); defer { bloqueo.unlock() }; depurarSinBloqueo()
        let residentes = recursos.values.compactMap(\.valor)
            .reduce(Int64(0)) { $0 + max($1.bytesResidentesEstimados, 0) }
        let exceso = max(0, residentes + max(bytesAdicionales, 0) - presupuesto.global)
        return liberarSinBloqueo(objetivo: exceso, proteger: proteger)
    }

    @discardableResult
    func liberar(objetivo: Int64,
                 proteger: Set<DocumentoPersistenteID> = []) -> ReporteLiberacionRecursosIPad {
        bloqueo.lock(); defer { bloqueo.unlock() }; depurarSinBloqueo()
        return liberarSinBloqueo(objetivo: max(objetivo, 0), proteger: proteger)
    }

    @objc private func memoriaBaja() {
        let objetivo = max(presupuesto.global / 4, 128 * 1_024 * 1_024)
        _ = liberar(objetivo: objetivo)
    }

    private func liberarSinBloqueo(objetivo: Int64,
                                   proteger: Set<DocumentoPersistenteID>) -> ReporteLiberacionRecursosIPad {
        guard objetivo > 0 else {
            return .init(solicitados: 0, liberadosEstimados: 0,
                         documentosSuspendidos: [], documentosOmitidosPorError: [])
        }
        let candidatos = recursos.values.compactMap(\.valor).filter {
            !proteger.contains($0.documentoPersistenteId) && !$0.fijadoEnMemoria && $0.bytesLiberablesEstimados > 0
        }.sorted {
            if $0.ultimoUso != $1.ultimoUso { return $0.ultimoUso < $1.ultimoUso }
            return $0.bytesLiberablesEstimados > $1.bytesLiberablesEstimados
        }
        var liberados: Int64 = 0, suspendidos: [DocumentoPersistenteID] = [], omitidos: [DocumentoPersistenteID] = []
        for recurso in candidatos where liberados < objetivo {
            do {
                if recurso.tieneCambiosSinGuardar { try recurso.persistirAntesDeSuspender() }
                let estimado = max(recurso.bytesLiberablesEstimados, 0)
                recurso.liberarRecursosPesados()
                liberados += estimado; suspendidos.append(recurso.documentoPersistenteId)
            } catch { omitidos.append(recurso.documentoPersistenteId) }
        }
        publicar()
        return .init(solicitados: objetivo, liberadosEstimados: liberados,
                     documentosSuspendidos: suspendidos, documentosOmitidosPorError: omitidos)
    }

    private func depurarSinBloqueo() { recursos = recursos.filter { $0.value.valor != nil } }
    private func publicar() {
        DispatchQueue.main.async { NotificationCenter.default.post(name: Self.cambio, object: self) }
    }
}

/// Contador ligero para que cada subsistema declare su huella sin que el gestor
/// de presupuesto conozca Metal/PDFKit/audio internamente.
struct HuellaRecursosDocumentoIPad: Equatable {
    var tinta: Int64 = 0
    var imagenes: Int64 = 0
    var pdfCache: Int64 = 0
    var audio: Int64 = 0
    var otros: Int64 = 0
    var total: Int64 { [tinta, imagenes, pdfCache, audio, otros].reduce(0) { $0 + max($1, 0) } }
}
