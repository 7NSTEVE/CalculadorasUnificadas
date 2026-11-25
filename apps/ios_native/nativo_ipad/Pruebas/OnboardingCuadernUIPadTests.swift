import XCTest
@testable import CuadernUEstudio

final class OnboardingCuadernUIPadTests: XCTestCase {
    private func base() throws -> URL {
        let url = FileManager.default.temporaryDirectory
            .appendingPathComponent("onboarding-\(UUID().uuidString)", isDirectory: true)
        try FileManager.default.createDirectory(at: url, withIntermediateDirectories: true)
        return url
    }

    func testSoloCompletaCuandoTodosLosPasosSonReales() throws {
        let repo = RepositorioOnboardingCuadernUIPad(base: try base())
        XCTAssertTrue(repo.debeMostrarse)
        for paso in PasoOnboardingCuadernUIPad.allCases.dropLast() { repo.marcar(paso) }
        XCTAssertFalse(repo.completado)
        repo.marcar(PasoOnboardingCuadernUIPad.allCases.last!)
        XCTAssertTrue(repo.completado)
        XCTAssertFalse(repo.debeMostrarse)
    }

    func testPersistenciaDeProgreso() throws {
        let base = try base()
        do {
            let repo = RepositorioOnboardingCuadernUIPad(base: base)
            repo.marcar(.buscar); repo.marcar(.foco)
        }
        let reabierto = RepositorioOnboardingCuadernUIPad(base: base)
        XCTAssertEqual(reabierto.completados(), [.buscar, .foco])
        XCTAssertTrue(reabierto.debeMostrarse)
    }

    func testOmitirNoFingePasosCompletados() throws {
        let base = try base()
        let repo = RepositorioOnboardingCuadernUIPad(base: base)
        repo.descartar()
        XCTAssertFalse(repo.debeMostrarse)
        XCTAssertFalse(repo.completado)
        XCTAssertTrue(repo.completados().isEmpty)
    }

    func testReiniciarBorraProgresoYOmitido() throws {
        let repo = RepositorioOnboardingCuadernUIPad(base: try base())
        repo.marcar(.importar); repo.descartar(); repo.reiniciar()
        XCTAssertTrue(repo.debeMostrarse)
        XCTAssertTrue(repo.completados().isEmpty)
    }
}
