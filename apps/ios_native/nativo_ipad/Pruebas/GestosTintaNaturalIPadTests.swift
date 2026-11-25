import XCTest

final class GestosTintaNaturalIPadTests: XCTestCase {
    func testCirculoCerradoSeClasificaComoCirculo() {
        let puntos = elipse(
            centro: CGPoint(x: 120, y: 90),
            radioX: 70, radioY: 48, muestras: 72)
        let analisis = DetectorGestoTintaNaturalIPad.analizar(puntos)
        XCTAssertEqual(analisis.tipo, .circulo)
        XCTAssertLessThan(analisis.cierreRelativo, 0.1)
    }

    func testElipseAnchaSigueSirviendoParaCircleToLasso() {
        let puntos = elipse(
            centro: CGPoint(x: 180, y: 100),
            radioX: 120, radioY: 45, muestras: 90)
        XCTAssertEqual(
            DetectorGestoTintaNaturalIPad.analizar(puntos).tipo,
            .circulo)
    }

    func testGarabatoConCrucesSeClasificaComoBorrado() {
        var puntos: [CGPoint] = []
        for fila in 0..<7 {
            let y = CGFloat(30 + fila * 5)
            if fila.isMultiple(of: 2) {
                puntos += [CGPoint(x: 20, y: y), CGPoint(x: 150, y: y + 10)]
            } else {
                puntos += [CGPoint(x: 150, y: y), CGPoint(x: 20, y: y + 10)]
            }
        }
        // Interpolar para parecer una captura real de Pencil.
        let denso = interpolar(puntos, pasos: 8)
        let analisis = DetectorGestoTintaNaturalIPad.analizar(denso)
        XCTAssertEqual(analisis.tipo, .garabato)
        XCTAssertGreaterThanOrEqual(max(analisis.reversionesX, analisis.reversionesY), 4)
    }

    func testLineaNormalNoSeInterpretaComoGesto() {
        let puntos = (0..<60).map {
            CGPoint(x: CGFloat($0 * 3), y: 40 + sin(CGFloat($0) / 9) * 3)
        }
        XCTAssertEqual(
            DetectorGestoTintaNaturalIPad.analizar(puntos).tipo,
            .ninguno)
    }

    func testOndaCursivaConPocasReversionesNoSeBorra() {
        let puntos = (0..<100).map { indice -> CGPoint in
            let x = CGFloat(indice) * 2.2
            let y = 70 + sin(CGFloat(indice) * 0.23) * 10
            return CGPoint(x: x, y: y)
        }
        let analisis = DetectorGestoTintaNaturalIPad.analizar(puntos)
        XCTAssertNotEqual(analisis.tipo, .garabato)
    }

    func testTrazoDemasiadoPequenoSeIgnora() {
        let puntos = [
            CGPoint(x: 1, y: 1), CGPoint(x: 2, y: 2),
            CGPoint(x: 3, y: 1), CGPoint(x: 2, y: 2),
        ]
        XCTAssertEqual(
            DetectorGestoTintaNaturalIPad.analizar(puntos).tipo,
            .ninguno)
    }

    private func elipse(centro: CGPoint, radioX: CGFloat,
                        radioY: CGFloat, muestras: Int) -> [CGPoint] {
        (0...muestras).map { indice in
            let angulo = CGFloat(indice) / CGFloat(muestras) * .pi * 2
            return CGPoint(
                x: centro.x + cos(angulo) * radioX,
                y: centro.y + sin(angulo) * radioY)
        }
    }

    private func interpolar(_ vertices: [CGPoint], pasos: Int) -> [CGPoint] {
        guard vertices.count >= 2 else { return vertices }
        var salida: [CGPoint] = []
        for (a, b) in zip(vertices, vertices.dropFirst()) {
            for paso in 0..<pasos {
                let t = CGFloat(paso) / CGFloat(pasos)
                salida.append(CGPoint(
                    x: a.x + (b.x - a.x) * t,
                    y: a.y + (b.y - a.y) * t))
            }
        }
        salida.append(vertices.last!)
        return salida
    }
}
