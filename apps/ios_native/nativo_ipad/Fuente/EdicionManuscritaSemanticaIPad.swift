import UIKit

enum NivelEdicionManuscritaIPad: String, Codable, CaseIterable {
    case palabra
    case linea
}

struct TrazoManuscritoIPad: Equatable {
    var indice: Int
    var marco: CGRect
}

struct GrupoManuscritoIPad: Equatable, Identifiable {
    var nivel: NivelEdicionManuscritaIPad
    var trazos: [Int]
    var marco: CGRect
    var id: String {
        "\(nivel.rawValue):" + trazos.map(String.init).joined(separator: ",")
    }
}

struct PoliticaAgrupacionManuscritaIPad: Equatable {
    var toleranciaLineaFactor: CGFloat = 0.62
    var espacioPalabraFactor: CGFloat = 0.48
    var espacioPalabraMinimo: CGFloat = 8
    var expansionHitTestFactor: CGFloat = 0.28
}

enum AnalizadorEstructuraManuscritaIPad {
    static func grupos(
        _ trazos: [TrazoManuscritoIPad],
        nivel: NivelEdicionManuscritaIPad,
        politica: PoliticaAgrupacionManuscritaIPad = .init()
    ) -> [GrupoManuscritoIPad] {
        let validos = trazos.filter {
            $0.indice >= 0 && $0.marco.width.isFinite && $0.marco.height.isFinite
                && $0.marco.minX.isFinite && $0.marco.minY.isFinite
                && !$0.marco.isNull && !$0.marco.isInfinite
                && $0.marco.width > 0 && $0.marco.height > 0
        }
        guard !validos.isEmpty else { return [] }
        let lineas = agruparLineas(validos, politica: politica)
        if nivel == .linea {
            return lineas.map {
                GrupoManuscritoIPad(
                    nivel: .linea,
                    trazos: $0.map(\.indice).sorted(),
                    marco: union($0.map(\.marco)))
            }
        }
        return lineas.flatMap { palabras(en: $0, politica: politica) }
    }

    static func grupo(
        cerca punto: CGPoint,
        trazos: [TrazoManuscritoIPad],
        nivel: NivelEdicionManuscritaIPad,
        politica: PoliticaAgrupacionManuscritaIPad = .init()
    ) -> GrupoManuscritoIPad? {
        let candidatos = grupos(trazos, nivel: nivel, politica: politica)
        guard !candidatos.isEmpty else { return nil }
        let tocados = candidatos.filter { grupo in
            let margen = max(grupo.marco.height * politica.expansionHitTestFactor, 6)
            return grupo.marco.insetBy(dx: -margen, dy: -margen).contains(punto)
        }
        if let mejor = tocados.min(by: {
            distanciaCuadrada(punto, a: $0.marco) < distanciaCuadrada(punto, a: $1.marco)
        }) {
            return mejor
        }
        let mejor = candidatos.min {
            distanciaCuadrada(punto, a: $0.marco) < distanciaCuadrada(punto, a: $1.marco)
        }
        guard let mejor else { return nil }
        let distancia = sqrt(distanciaCuadrada(punto, a: mejor.marco))
        return distancia <= max(mejor.marco.height * 1.25, 28) ? mejor : nil
    }

    private static func agruparLineas(
        _ trazos: [TrazoManuscritoIPad],
        politica: PoliticaAgrupacionManuscritaIPad
    ) -> [[TrazoManuscritoIPad]] {
        let ordenados = trazos.sorted {
            if abs($0.marco.midY - $1.marco.midY) > 0.5 {
                return $0.marco.midY < $1.marco.midY
            }
            return $0.marco.minX < $1.marco.minX
        }
        var lineas: [[TrazoManuscritoIPad]] = []
        var marcos: [CGRect] = []
        for trazo in ordenados {
            var mejorIndice: Int?
            var mejorPuntuacion = -CGFloat.greatestFiniteMagnitude
            // Las líneas están ordenadas verticalmente. Normalmente sólo las
            // últimas dos pueden competir; limitar la ventana evita O(n²).
            let inicio = max(0, lineas.count - 3)
            if inicio < lineas.count {
                for indice in inicio..<lineas.count {
                    let linea = marcos[indice]
                    let altura = max(min(linea.height, trazo.marco.height), 1)
                    let solape = max(0, min(linea.maxY, trazo.marco.maxY)
                                     - max(linea.minY, trazo.marco.minY)) / altura
                    let tolerancia = max(linea.height, trazo.marco.height)
                        * politica.toleranciaLineaFactor
                    let distanciaY = abs(linea.midY - trazo.marco.midY)
                    guard solape >= 0.22 || distanciaY <= tolerancia else { continue }
                    let puntuacion = solape * 3 - distanciaY / max(tolerancia, 1)
                    if puntuacion > mejorPuntuacion {
                        mejorPuntuacion = puntuacion
                        mejorIndice = indice
                    }
                }
            }
            if let indice = mejorIndice {
                lineas[indice].append(trazo)
                marcos[indice] = marcos[indice].union(trazo.marco)
            } else {
                lineas.append([trazo])
                marcos.append(trazo.marco)
            }
        }
        return lineas.sorted { union($0.map(\.marco)).minY < union($1.map(\.marco)).minY }
    }

    private static func palabras(
        en linea: [TrazoManuscritoIPad],
        politica: PoliticaAgrupacionManuscritaIPad
    ) -> [GrupoManuscritoIPad] {
        let ordenados = linea.sorted { $0.marco.minX < $1.marco.minX }
        guard let primero = ordenados.first else { return [] }
        let alturas = ordenados.map(\.marco.height).sorted()
        let medianaAltura = alturas[alturas.count / 2]
        let umbral = max(
            politica.espacioPalabraMinimo,
            medianaAltura * politica.espacioPalabraFactor)

        var salida: [GrupoManuscritoIPad] = []
        var actual = [primero]
        var marcoActual = primero.marco
        for trazo in ordenados.dropFirst() {
            let gap = trazo.marco.minX - marcoActual.maxX
            if gap > umbral {
                salida.append(GrupoManuscritoIPad(
                    nivel: .palabra,
                    trazos: actual.map(\.indice).sorted(),
                    marco: marcoActual))
                actual = [trazo]
                marcoActual = trazo.marco
            } else {
                actual.append(trazo)
                marcoActual = marcoActual.union(trazo.marco)
            }
        }
        salida.append(GrupoManuscritoIPad(
            nivel: .palabra,
            trazos: actual.map(\.indice).sorted(),
            marco: marcoActual))
        return salida
    }

    private static func union(_ marcos: [CGRect]) -> CGRect {
        marcos.dropFirst().reduce(marcos.first ?? .null) { $0.union($1) }
    }

    private static func distanciaCuadrada(_ punto: CGPoint, a rect: CGRect) -> CGFloat {
        let x = min(max(punto.x, rect.minX), rect.maxX)
        let y = min(max(punto.y, rect.minY), rect.maxY)
        let dx = punto.x - x
        let dy = punto.y - y
        return dx * dx + dy * dy
    }
}

enum MotorEdicionManuscritaIPad {
    static func descriptores(sesion: Sesion) -> [TrazoManuscritoIPad] {
        (0..<sesion.cantidadTrazos).compactMap { indice in
            sesion.marcoTrazo(indice).map {
                TrazoManuscritoIPad(indice: indice, marco: $0)
            }
        }
    }

    static func grupo(
        cerca punto: CGPoint,
        nivel: NivelEdicionManuscritaIPad,
        sesion: Sesion
    ) -> GrupoManuscritoIPad? {
        AnalizadorEstructuraManuscritaIPad.grupo(
            cerca: punto,
            trazos: descriptores(sesion: sesion),
            nivel: nivel)
    }

    @discardableResult
    static func mover(_ grupo: GrupoManuscritoIPad,
                      delta: CGSize,
                      sesion: Sesion) -> Bool {
        sesion.transformarTrazos(
            grupo.trazos, desplazamiento: delta,
            pivote: CGPoint(x: grupo.marco.midX, y: grupo.marco.midY))
    }

    @discardableResult
    static func eliminar(_ grupo: GrupoManuscritoIPad,
                         sesion: Sesion) -> Bool {
        sesion.eliminarTrazos(grupo.trazos)
    }

    static func duplicar(_ grupo: GrupoManuscritoIPad,
                         sesion: Sesion,
                         desplazamiento: CGSize = CGSize(width: 28, height: 28))
        -> SeleccionLienzo {
        sesion.duplicarSeleccion(
            SeleccionLienzo(trazos: grupo.trazos),
            desplazamiento: desplazamiento)
    }
}
