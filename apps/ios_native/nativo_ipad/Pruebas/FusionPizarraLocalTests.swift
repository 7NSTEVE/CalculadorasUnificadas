import XCTest
@testable import CuadernUEstudio

final class FusionPizarraLocalTests: XCTestCase {
    func testMedianaDeDosConservaLaSemanticaSuperior() {
        XCTAssertEqual(FusionPizarraLocal.mediana2(2, 9), 9)
        XCTAssertEqual(FusionPizarraLocal.mediana2(9, 2), 9)
        XCTAssertEqual(FusionPizarraLocal.mediana2(7, 7), 7)
    }

    func testRedDeTresParaTodasLasPermutaciones() {
        verificarTodas([1, 4, 9]) { valores in
            FusionPizarraLocal.mediana3(valores[0], valores[1], valores[2])
        }
    }

    func testRedDeCuatroParaTodasLasPermutaciones() {
        verificarTodas([1, 4, 9, 16]) { valores in
            FusionPizarraLocal.mediana4(
                valores[0], valores[1], valores[2], valores[3])
        }
    }

    func testRedDeCincoParaTodasLasPermutaciones() {
        verificarTodas([1, 4, 9, 16, 25]) { valores in
            FusionPizarraLocal.mediana5(
                valores[0], valores[1], valores[2], valores[3], valores[4])
        }
    }

    private func verificarTodas(
        _ valores: [UInt8],
        archivo: StaticString = #filePath,
        linea: UInt = #line,
        mediana: ([UInt8]) -> UInt8
    ) {
        let esperada = valores.sorted()[valores.count / 2]
        let permutaciones = permutar(valores)
        XCTAssertEqual(permutaciones.count, factorial(valores.count),
                       file: archivo, line: linea)
        for permutacion in permutaciones {
            XCTAssertEqual(mediana(permutacion), esperada,
                           "Falló para \(permutacion)",
                           file: archivo, line: linea)
        }
    }

    private func permutar(_ valores: [UInt8]) -> [[UInt8]] {
        guard valores.count > 1 else { return [valores] }
        return valores.indices.flatMap { indice -> [[UInt8]] in
            var restantes = valores
            let primero = restantes.remove(at: indice)
            return permutar(restantes).map { [primero] + $0 }
        }
    }

    private func factorial(_ valor: Int) -> Int {
        valor < 2 ? 1 : (2...valor).reduce(1, *)
    }
}
