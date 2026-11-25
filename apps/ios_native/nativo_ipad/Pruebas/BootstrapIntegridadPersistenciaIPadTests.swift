import XCTest
@testable import CuadernUEstudio

final class BootstrapIntegridadPersistenciaIPadTests: XCTestCase {
    private func conflicto(_ id: UUID = UUID()) -> ConflictoRecoveryDocumentoIPad {
        ConflictoRecoveryDocumentoIPad(
            id: id,
            documentoId: DocumentoPersistenteID(),
            rutas: ["/tmp/documento.tinta", "/tmp/sidecar.json"],
            actualizado: Date(timeIntervalSince1970: 1_700_000_000))
    }

    func testMapeaRecoveryLimpioYRecuperadoSinBloqueo() {
        var siguiente: EstadoGateIntegridadIPad = .limpio
        let coordinador = CoordinadorBootstrapIntegridadIPad(
            ejecutarGate: { siguiente },
            resolverConservandoActuales: { _, _ in },
            destinoDiagnostico: { _ in URL(fileURLWithPath: "/tmp/diag") })

        XCTAssertEqual(coordinador.estado, .noEjecutado)
        XCTAssertEqual(coordinador.ejecutar(), .listo(recovery: 0))

        siguiente = .recuperado(transacciones: 3)
        XCTAssertEqual(coordinador.ejecutar(), .listo(recovery: 3))
    }

    func testConflictoPermaneceBloqueadoHastaExportarYResolver() throws {
        let primero = conflicto(), segundo = conflicto()
        var pendientes = [primero, segundo]
        var resueltos: [UUID] = []
        let raiz = FileManager.default.temporaryDirectory
            .appendingPathComponent("bootstrap-integridad-\(UUID().uuidString)",
                                    isDirectory: true)
        defer { try? FileManager.default.removeItem(at: raiz) }

        let coordinador = CoordinadorBootstrapIntegridadIPad(
            ejecutarGate: {
                pendientes.isEmpty ? .limpio : .bloqueado(conflictos: pendientes)
            },
            resolverConservandoActuales: { id, destino in
                XCTAssertFalse(FileManager.default.fileExists(atPath: destino.path))
                try FileManager.default.createDirectory(
                    at: destino, withIntermediateDirectories: true)
                resueltos.append(id)
                pendientes.removeAll { $0.id == id }
            },
            destinoDiagnostico: { id in
                raiz.appendingPathComponent("conflicto-\(id.uuidString)",
                                            isDirectory: true)
            })

        XCTAssertEqual(coordinador.ejecutar(),
                       .bloqueado(conflictos: [primero, segundo]))
        let resultado = try coordinador.resolverTodosConservandoActuales()

        XCTAssertEqual(Set(resueltos), Set([primero.id, segundo.id]))
        XCTAssertEqual(resultado.diagnosticos.count, 2)
        XCTAssertEqual(resultado.estadoFinal, .listo(recovery: 0))
        XCTAssertEqual(coordinador.estado, .listo(recovery: 0))
    }

    func testDiagnosticoExistenteNuncaSeSobrescribe() throws {
        let actual = conflicto()
        var pendientes = [actual]
        var destinoUsado: URL?
        let raiz = FileManager.default.temporaryDirectory
            .appendingPathComponent("bootstrap-no-overwrite-\(UUID().uuidString)",
                                    isDirectory: true)
        let preferido = raiz.appendingPathComponent("conflicto", isDirectory: true)
        try FileManager.default.createDirectory(
            at: preferido, withIntermediateDirectories: true)
        defer { try? FileManager.default.removeItem(at: raiz) }

        let coordinador = CoordinadorBootstrapIntegridadIPad(
            ejecutarGate: {
                pendientes.isEmpty ? .limpio : .bloqueado(conflictos: pendientes)
            },
            resolverConservandoActuales: { id, destino in
                destinoUsado = destino
                pendientes.removeAll { $0.id == id }
            },
            destinoDiagnostico: { _ in preferido })

        _ = coordinador.ejecutar()
        let resultado = try coordinador.resolverTodosConservandoActuales()
        let usado = try XCTUnwrap(destinoUsado)

        XCTAssertNotEqual(usado, preferido)
        XCTAssertEqual(usado.deletingLastPathComponent(),
                       preferido.deletingLastPathComponent())
        XCTAssertTrue(FileManager.default.fileExists(atPath: preferido.path),
                      "El paquete previo debe conservarse intacto.")
        XCTAssertEqual(resultado.diagnosticos, [usado])
    }

    func testFalloDeExportacionNoDesbloqueaElProceso() {
        let actual = conflicto()
        var llamadasGate = 0
        let coordinador = CoordinadorBootstrapIntegridadIPad(
            ejecutarGate: {
                llamadasGate += 1
                return .bloqueado(conflictos: [actual])
            },
            resolverConservandoActuales: { _, _ in
                throw CocoaError(.fileWriteNoPermission)
            },
            destinoDiagnostico: { _ in URL(fileURLWithPath: "/tmp/diag") })

        _ = coordinador.ejecutar()
        XCTAssertThrowsError(try coordinador.resolverTodosConservandoActuales())
        XCTAssertEqual(coordinador.estado, .bloqueado(conflictos: [actual]))
        XCTAssertGreaterThanOrEqual(llamadasGate, 2,
                                    "Tras el fallo debe reevaluarse el gate.")
    }

    func testResolverSinConflictosEsNoOpYReevalua() throws {
        var gates = 0
        let coordinador = CoordinadorBootstrapIntegridadIPad(
            ejecutarGate: {
                gates += 1
                return .limpio
            },
            resolverConservandoActuales: { _, _ in
                XCTFail("No debe invocarse un resolver sin conflictos.")
            },
            destinoDiagnostico: { _ in URL(fileURLWithPath: "/tmp/noop") })

        _ = coordinador.ejecutar()
        let resultado = try coordinador.resolverTodosConservandoActuales()
        XCTAssertTrue(resultado.diagnosticos.isEmpty)
        XCTAssertEqual(resultado.estadoFinal, .listo(recovery: 0))
        XCTAssertEqual(gates, 2)
    }
}
