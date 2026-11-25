import UIKit

struct DescriptorCircuitoIPad {
    var caracteristicas: [Float]
    var terminalA: CGPoint
    var terminalB: CGPoint
}

struct EjemploCircuitoIPad: Codable, Identifiable {
    var id = UUID()
    var tipo: TipoComponenteElectrico
    var caracteristicas: [Float]
    var fecha = Date()
}

struct CandidatoPlantillaCircuitoIPad {
    var tipo: TipoComponenteElectrico
    var confianza: Float
}

final class RepositorioPlantillasCircuitoIPad {
    static let compartido = RepositorioPlantillasCircuitoIPad()
    private let url: URL
    private var ejemplos: [EjemploCircuitoIPad] = []
    private let cola = DispatchQueue(label: "tinta.circuitos.plantillas")

    private init() {
        let documentos = FileManager.default.urls(for: .documentDirectory,
                                                   in: .userDomainMask).first!
        let carpeta = documentos.appendingPathComponent("Ingenieria", isDirectory: true)
        try? FileManager.default.createDirectory(at: carpeta,
                                                 withIntermediateDirectories: true)
        url = carpeta.appendingPathComponent("plantillas-circuitos.json")
        if let datos = try? Data(contentsOf: url),
           let leidos = try? JSONDecoder().decode([EjemploCircuitoIPad].self,
                                                  from: datos) {
            ejemplos = leidos.filter { !$0.caracteristicas.isEmpty }
        }
    }

    func agregar(tipo: TipoComponenteElectrico, caracteristicas: [Float]) {
        guard caracteristicas.allSatisfy(\.isFinite), !caracteristicas.isEmpty else { return }
        cola.sync {
            let repetido = ejemplos.contains {
                $0.tipo == tipo && distancia($0.caracteristicas, caracteristicas) < 0.012
            }
            guard !repetido else { return }
            ejemplos.append(EjemploCircuitoIPad(tipo: tipo,
                                                 caracteristicas: caracteristicas))
            let indicesTipo = ejemplos.indices.filter { ejemplos[$0].tipo == tipo }
            if indicesTipo.count > 24 {
                let quitar = indicesTipo.prefix(indicesTipo.count - 24).reversed()
                for indice in quitar { ejemplos.remove(at: indice) }
            }
            persistir()
        }
    }

    func reconocer(_ caracteristicas: [Float]) -> CandidatoPlantillaCircuitoIPad? {
        cola.sync {
            guard !caracteristicas.isEmpty else { return nil }
            let candidatos = ejemplos.compactMap { ejemplo
                -> (TipoComponenteElectrico, Float)? in
                guard ejemplo.caracteristicas.count == caracteristicas.count else { return nil }
                return (ejemplo.tipo,
                        Float(distancia(ejemplo.caracteristicas, caracteristicas)))
            }
            guard let mejor = candidatos.min(by: { $0.1 < $1.1 }) else { return nil }
            // Distancia RMS normalizada. Por encima de 0.16 no se etiqueta:
            // solicitar otro ejemplo es preferible a inventar un símbolo.
            guard mejor.1 <= 0.16 else { return nil }
            let confianza = max(0.55, min(0.98, 1 - mejor.1 / 0.32))
            return CandidatoPlantillaCircuitoIPad(tipo: mejor.0,
                                                   confianza: confianza)
        }
    }

    func cantidadEjemplos(tipo: TipoComponenteElectrico) -> Int {
        cola.sync { ejemplos.filter { $0.tipo == tipo }.count }
    }

    func eliminarEjemplos(tipo: TipoComponenteElectrico) {
        cola.sync {
            ejemplos.removeAll { $0.tipo == tipo }
            persistir()
        }
    }

    private func distancia(_ a: [Float], _ b: [Float]) -> Double {
        guard a.count == b.count, !a.isEmpty else { return .infinity }
        let suma = zip(a, b).reduce(0.0) {
            $0 + pow(Double($1.0 - $1.1), 2)
        }
        return sqrt(suma / Double(a.count))
    }

    private func persistir() {
        guard let datos = try? JSONEncoder().encode(ejemplos) else { return }
        try? datos.write(to: url, options: [.atomic, .completeFileProtection])
    }
}

extension Sesion {
    func descriptorCircuitoUltimos(_ cantidad: Int) -> DescriptorCircuitoIPad? {
        guard cantidad > 0 else { return nil }
        var geometria = TintaReconocimientoCircuito()
        _ = tinta_circuito_reconocer_ultimos(puntero, cantidad, &geometria)
        var valores = [Float](repeating: 0, count: 32)
        let total = valores.withUnsafeMutableBufferPointer {
            tinta_circuito_caracteristicas_ultimos(
                puntero, cantidad, $0.baseAddress, $0.count)
        }
        guard total > 0 else { return nil }
        return DescriptorCircuitoIPad(
            caracteristicas: Array(valores.prefix(total)),
            terminalA: CGPoint(x: CGFloat(geometria.terminal_ax),
                               y: CGFloat(geometria.terminal_ay)),
            terminalB: CGPoint(x: CGFloat(geometria.terminal_bx),
                               y: CGFloat(geometria.terminal_by)))
    }

    func descriptorCircuitoSeleccion(_ indices: [Int]) -> DescriptorCircuitoIPad? {
        let indicesValidos = indices.filter { $0 >= 0 }
        guard !indicesValidos.isEmpty else { return nil }
        var geometria = TintaReconocimientoCircuito()
        var valores = [Float](repeating: 0, count: 32)
        let total = indicesValidos.withUnsafeBufferPointer { buffer -> Int in
            guard let base = buffer.baseAddress else { return 0 }
            _ = tinta_circuito_reconocer_seleccion(
                puntero, base, buffer.count, &geometria)
            return valores.withUnsafeMutableBufferPointer {
                tinta_circuito_caracteristicas_seleccion(
                    puntero, base, buffer.count, $0.baseAddress, $0.count)
            }
        }
        guard total > 0 else { return nil }
        return DescriptorCircuitoIPad(
            caracteristicas: Array(valores.prefix(total)),
            terminalA: CGPoint(x: CGFloat(geometria.terminal_ax),
                               y: CGFloat(geometria.terminal_ay)),
            terminalB: CGPoint(x: CGFloat(geometria.terminal_bx),
                               y: CGFloat(geometria.terminal_by)))
    }

    func reconocerCircuitoEntrenableUltimos(_ cantidad: Int)
        -> (ReconocimientoCircuito, DescriptorCircuitoIPad)? {
        guard let descriptor = descriptorCircuitoUltimos(cantidad) else { return nil }
        let geometrico = reconocerUltimosTrazosComoCircuito(cantidad)
        let aprendido = RepositorioPlantillasCircuitoIPad.compartido
            .reconocer(descriptor.caracteristicas)
        return combinar(geometrico: geometrico, aprendido: aprendido,
                        descriptor: descriptor)
    }

    func reconocerCircuitoEntrenableSeleccion(_ indices: [Int])
        -> (ReconocimientoCircuito, DescriptorCircuitoIPad)? {
        guard let descriptor = descriptorCircuitoSeleccion(indices) else { return nil }
        let geometrico = reconocerSeleccionComoCircuito(indices)
        let aprendido = RepositorioPlantillasCircuitoIPad.compartido
            .reconocer(descriptor.caracteristicas)
        return combinar(geometrico: geometrico, aprendido: aprendido,
                        descriptor: descriptor)
    }

    private func combinar(geometrico: ReconocimientoCircuito?,
                          aprendido: CandidatoPlantillaCircuitoIPad?,
                          descriptor: DescriptorCircuitoIPad)
        -> (ReconocimientoCircuito, DescriptorCircuitoIPad)? {
        if let aprendido,
           geometrico == nil || aprendido.confianza > (geometrico?.confianza ?? 0) {
            return (ReconocimientoCircuito(
                tipo: aprendido.tipo, confianza: aprendido.confianza,
                terminalA: descriptor.terminalA, terminalB: descriptor.terminalB),
                    descriptor)
        }
        return geometrico.map { ($0, descriptor) }
    }
}
