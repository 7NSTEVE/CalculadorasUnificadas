import XCTest
@testable import CuadernUEstudio

final class PlanMigracionSidecarsConocidosIPadTests: XCTestCase {
    func testPlanDetectaYRenumeraRutasRealesConUUID() throws {
        let fm = FileManager.default, doc = DocumentoPersistenteID()
        let soporte = fm.urls(for: .applicationSupportDirectory, in: .userDomainMask)[0]
        let documentos = fm.urls(for: .documentDirectory, in: .userDomainMask)[0]
        let notasBase = soporte.appendingPathComponent("Tinta/NotasAdhesivas", isDirectory: true)
        let legacyNotas = notasBase.appendingPathComponent(
            "doc-\(doc.claveCompatibilidadUInt64).json")
        let legacyRecovery = documentos.appendingPathComponent(
            "Recuperacion/\(doc.claveCompatibilidadUInt64)", isDirectory: true)
        defer {
            try? fm.removeItem(at: legacyNotas)
            try? fm.removeItem(at: notasBase.appendingPathComponent("doc-\(doc.description).json"))
            try? fm.removeItem(at: legacyRecovery)
            try? fm.removeItem(at: documentos.appendingPathComponent("Recuperacion/\(doc.description)"))
        }

        try fm.createDirectory(at: notasBase, withIntermediateDirectories: true)
        try Data("{}".utf8).write(to: legacyNotas)
        try fm.createDirectory(at: legacyRecovery, withIntermediateDirectories: true)
        try Data("journal".utf8).write(to: legacyRecovery.appendingPathComponent("r.bin"))

        let plan = PlanMigracionSidecarsConocidosIPad.construir(documentoId: doc)
        XCTAssertEqual(plan.legacyEncontrados, 2)
        XCTAssertEqual(plan.pasos.count, 2)
        XCTAssertTrue(plan.pasos.allSatisfy(\.existeLegacy))
        XCTAssertTrue(plan.pasos.contains {
            $0.nombre == "Notas adhesivas" && $0.destino.lastPathComponent.contains(doc.description)
        })
        XCTAssertTrue(plan.pasos.contains {
            $0.nombre == "Recovery / autoguardado" && $0.destino.lastPathComponent == doc.description
        })
    }

    func testEjecucionMigraAmbosPatronesConElMismoUUID() throws {
        let fm = FileManager.default, doc = DocumentoPersistenteID()
        let soporte = fm.urls(for: .applicationSupportDirectory, in: .userDomainMask)[0]
        let documentos = fm.urls(for: .documentDirectory, in: .userDomainMask)[0]
        let notasBase = soporte.appendingPathComponent("Tinta/NotasAdhesivas", isDirectory: true)
        let legacyNotas = notasBase.appendingPathComponent("doc-\(doc.claveCompatibilidadUInt64).json")
        let durableNotas = notasBase.appendingPathComponent("doc-\(doc.description).json")
        let recoveryBase = documentos.appendingPathComponent("Recuperacion", isDirectory: true)
        let legacyRecovery = recoveryBase.appendingPathComponent(String(doc.claveCompatibilidadUInt64), isDirectory: true)
        let durableRecovery = recoveryBase.appendingPathComponent(doc.description, isDirectory: true)
        defer {
            for u in [legacyNotas, durableNotas, legacyRecovery, durableRecovery] { try? fm.removeItem(at: u) }
        }

        try fm.createDirectory(at: notasBase, withIntermediateDirectories: true)
        try Data("{\"texto\":\"nota\"}".utf8).write(to: legacyNotas)
        try fm.createDirectory(at: legacyRecovery, withIntermediateDirectories: true)
        try Data("recovery".utf8).write(to: legacyRecovery.appendingPathComponent("snap.bin"))

        let txBase = fm.temporaryDirectory.appendingPathComponent("plan-known-\(UUID())", isDirectory: true)
        defer { try? fm.removeItem(at: txBase) }
        let resultado = try PlanMigracionSidecarsConocidosIPad.ejecutar(
            documentoId: doc,
            transacciones: .init(base: txBase))

        XCTAssertEqual(resultado.notasAdhesivas?.estado, .migrado)
        XCTAssertEqual(resultado.recovery?.archivosMigrados, 1)
        XCTAssertFalse(fm.fileExists(atPath: legacyNotas.path))
        XCTAssertFalse(fm.fileExists(atPath: legacyRecovery.path))
        XCTAssertEqual(try Data(contentsOf: durableNotas), Data("{\"texto\":\"nota\"}".utf8))
        XCTAssertEqual(try Data(contentsOf: durableRecovery.appendingPathComponent("snap.bin")),
                       Data("recovery".utf8))
    }
}
