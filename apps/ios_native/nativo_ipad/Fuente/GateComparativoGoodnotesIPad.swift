import Foundation

enum DireccionMetricaComparativaIPad: String, Codable {
    case menorEsMejor
    case mayorEsMejor
}

struct SerieComparativaRendimientoIPad: Codable, Equatable, Identifiable {
    var id: String { "\(metrica)|\(escenario)|\(dispositivo)|\(sistema)" }
    var metrica: String
    var escenario: String
    var dispositivo: String
    var sistema: String
    var direccion: DireccionMetricaComparativaIPad
    var cuadernU: [Double]
    var goodnotes: [Double]
    var fecha = Date()
}

enum EstadoFuncionComparativaIPad: String, Codable, Comparable {
    case inexistente
    case parcial
    case implementada
    case verificada

    private var orden: Int {
        switch self {
        case .inexistente: return 0
        case .parcial: return 1
        case .implementada: return 2
        case .verificada: return 3
        }
    }

    static func < (lhs: Self, rhs: Self) -> Bool { lhs.orden < rhs.orden }
}

struct EvidenciaFuncionComparativaIPad: Codable, Equatable, Identifiable {
    var id: String
    var nombre: String
    var categoria: String
    var disponibleEnGoodnotes: Bool
    var estadoCuadernU: EstadoFuncionComparativaIPad
    var diferenciadorCuadernU: Bool
    var evidencia: String
    var fecha = Date()
}

struct PoliticaSuperioridadGoodnotesIPad: Codable, Equatable {
    /// Un benchmark de una sola pasada es demasiado ruidoso para una afirmación
    /// de producto. Cinco es el piso; hardware QA puede elevarlo por escenario.
    var minimoMuestrasPorProducto = 5
    /// Margen requerido para contar una métrica como ventaja real y no como
    /// ruido de medición. El gate también exige que ninguna métrica comparable
    /// quede por debajo de la tolerancia de no-regresión.
    var ventajaRendimientoMinimaPorcentaje = 5.0
    var regresionRendimientoMaximaPorcentaje = 2.0
    var minimoMetricasRendimiento = 3
    var minimoMetricasConVentaja = 1
    /// Evita declarar superioridad con una matriz elegida a conveniencia. El
    /// benchmark maestro ya contiene decenas de capacidades de Goodnotes; una
    /// ejecución real debe cubrir una porción sustancial de ellas.
    var minimoFuncionesGoodnotesComparadas = 20
    var minimoDiferenciadoresVerificados = 2
    var antiguedadMaximaDias = 90
}

enum VeredictoSuperioridadGoodnotesIPad: String, Codable {
    case evidenciaInsuficiente
    case noSupera
    case supera
}

struct ComparacionMetricaGoodnotesIPad: Equatable, Identifiable {
    var id: String
    var medianaCuadernU: Double
    var medianaGoodnotes: Double
    /// Positivo = CuadernU mejor; negativo = Goodnotes mejor.
    var ventajaCuadernUPorcentaje: Double
    var cuentaComoVentaja: Bool
    var regresionInaceptable: Bool
}

struct ReporteSuperioridadGoodnotesIPad: Equatable {
    var veredicto: VeredictoSuperioridadGoodnotesIPad
    var comparaciones: [ComparacionMetricaGoodnotesIPad]
    var funcionesGoodnotesNoIgualadas: [EvidenciaFuncionComparativaIPad]
    var diferenciadoresVerificados: [EvidenciaFuncionComparativaIPad]
    var faltantes: [String]

    var permiteAfirmarSuperioridad: Bool { veredicto == .supera }
}

/// Gate conservador para el objetivo de producto "superar Goodnotes 6".
///
/// No conoce una lista hardcodeada de funciones del competidor: la matriz se
/// alimenta con evidencia fechada. Eso obliga a actualizar el benchmark cuando
/// Goodnotes cambie y evita convertir documentación vieja en una verdad eterna.
enum GateSuperioridadGoodnotesIPad {
    static func evaluar(
        rendimiento: [SerieComparativaRendimientoIPad],
        funciones: [EvidenciaFuncionComparativaIPad],
        politica: PoliticaSuperioridadGoodnotesIPad = .init(),
        ahora: Date = Date()
    ) -> ReporteSuperioridadGoodnotesIPad {
        let dias = max(politica.antiguedadMaximaDias, 1)
        let minimoMuestras = max(politica.minimoMuestrasPorProducto, 1)
        let minimoMetricas = max(politica.minimoMetricasRendimiento, 1)
        let minimoVentajas = max(politica.minimoMetricasConVentaja, 1)
        let minimoFunciones = max(politica.minimoFuncionesGoodnotesComparadas, 1)
        let minimoDiferenciadores = max(politica.minimoDiferenciadoresVerificados, 1)
        let ventajaMinima = max(politica.ventajaRendimientoMinimaPorcentaje, 0)
        let regresionMaxima = max(politica.regresionRendimientoMaximaPorcentaje, 0)
        let limite = ahora.addingTimeInterval(-Double(dias) * 86_400)
        var faltantes: [String] = []
        var comparaciones: [ComparacionMetricaGoodnotesIPad] = []

        let seriesRecientes = rendimiento.filter { $0.fecha >= limite }
        for serie in seriesRecientes {
            let a = serie.cuadernU.filter { $0.isFinite && $0 >= 0 }
            let b = serie.goodnotes.filter { $0.isFinite && $0 >= 0 }
            guard a.count >= minimoMuestras, b.count >= minimoMuestras else {
                faltantes.append("\(serie.id): faltan muestras comparables")
                continue
            }
            let cu = mediana(a), gn = mediana(b)
            guard gn > 0 else {
                faltantes.append("\(serie.id): baseline Goodnotes inválido")
                continue
            }
            let ventaja: Double
            switch serie.direccion {
            case .menorEsMejor:
                ventaja = (gn - cu) / gn * 100
            case .mayorEsMejor:
                ventaja = (cu - gn) / gn * 100
            }
            comparaciones.append(.init(
                id: serie.id,
                medianaCuadernU: cu,
                medianaGoodnotes: gn,
                ventajaCuadernUPorcentaje: ventaja,
                cuentaComoVentaja: ventaja >= ventajaMinima,
                regresionInaceptable: ventaja < -regresionMaxima))
        }

        if rendimiento.contains(where: { $0.fecha < limite }) {
            faltantes.append("Hay benchmarks comparativos vencidos (>\(dias) días)")
        }
        if comparaciones.count < minimoMetricas {
            faltantes.append(
                "Rendimiento: sólo \(comparaciones.count)/\(minimoMetricas) métricas comparables")
        }

        let funcionesRecientes = funciones.filter { $0.fecha >= limite }
        let funcionesGoodnotes = funcionesRecientes.filter(\.disponibleEnGoodnotes)
        let noIgualadas = funcionesGoodnotes.filter {
            $0.estadoCuadernU < .verificada
        }.sorted { ($0.categoria, $0.nombre) < ($1.categoria, $1.nombre) }
        let diferenciadores = funcionesRecientes.filter {
            $0.diferenciadorCuadernU
                && !$0.disponibleEnGoodnotes
                && $0.estadoCuadernU == .verificada
        }.sorted { ($0.categoria, $0.nombre) < ($1.categoria, $1.nombre) }

        if funcionesRecientes.isEmpty {
            faltantes.append("Funcionalidad: no hay matriz comparativa reciente")
        }
        if funcionesGoodnotes.count < minimoFunciones {
            faltantes.append(
                "Funcionalidad: sólo \(funcionesGoodnotes.count)/\(minimoFunciones) capacidades Goodnotes comparadas")
        }
        if funciones.contains(where: { $0.fecha < limite }) {
            faltantes.append("Hay evidencia funcional vencida (>\(dias) días)")
        }
        if !noIgualadas.isEmpty {
            faltantes.append("Funcionalidad: \(noIgualadas.count) capacidades de Goodnotes no están verificadas end-to-end")
        }
        if diferenciadores.count < minimoDiferenciadores {
            faltantes.append(
                "Características: sólo \(diferenciadores.count)/\(minimoDiferenciadores) diferenciadores exclusivos verificados")
        }

        let suficientes = comparaciones.count >= minimoMetricas
            && funcionesGoodnotes.count >= minimoFunciones
            && diferenciadores.count >= minimoDiferenciadores
        let ventajaSuficiente = comparaciones.filter(\.cuentaComoVentaja).count
            >= minimoVentajas
        let sinRegresiones = !comparaciones.contains(where: \.regresionInaceptable)
        let paridadCompleta = noIgualadas.isEmpty

        let veredicto: VeredictoSuperioridadGoodnotesIPad
        if !suficientes {
            veredicto = .evidenciaInsuficiente
        } else if ventajaSuficiente && sinRegresiones && paridadCompleta {
            veredicto = .supera
        } else {
            veredicto = .noSupera
        }

        return .init(
            veredicto: veredicto,
            comparaciones: comparaciones.sorted { $0.id < $1.id },
            funcionesGoodnotesNoIgualadas: noIgualadas,
            diferenciadoresVerificados: diferenciadores,
            faltantes: Array(Set(faltantes)).sorted())
    }

    private static func mediana(_ valores: [Double]) -> Double {
        let v = valores.sorted()
        let i = v.count / 2
        return v.count.isMultiple(of: 2) ? (v[i - 1] + v[i]) / 2 : v[i]
    }
}
