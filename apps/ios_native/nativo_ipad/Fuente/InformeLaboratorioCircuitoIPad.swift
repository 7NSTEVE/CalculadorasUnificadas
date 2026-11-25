import Foundation
import UIKit

struct MedicionComponenteDCIPad: Equatable, Identifiable {
    var id: UUID
    var referencia: String
    var tipo: TipoComponenteElectrico
    var nodoA: Int
    var nodoB: Int
    var voltajeAB: Double
    var corrienteAB: Double?
    var potenciaAbsorbida: Double?
    var nota: String?
}

struct VerificacionKCLNodoIPad: Equatable, Identifiable {
    var id: Int { nodo }
    var nodo: Int
    var residuoAmperios: Double?
    var completa: Bool
    var detalle: String
}

struct InformeLaboratorioCircuitoIPad {
    var titulo: String
    var diagnostico: DiagnosticoCircuitoIPad
    var errorSimulacion: String?
    var voltajes: [Int: Double]
    var componentes: [MedicionComponenteDCIPad]
    var kcl: [VerificacionKCLNodoIPad]
    var sumaPotencias: Double?
    var balancePotenciaCompleto: Bool
    var advertencias: [String]
    var spice: String

    var aprobadoNumericamente: Bool {
        guard errorSimulacion == nil,
              diagnostico.hallazgos.allSatisfy({ $0.severidad != .bloqueo }) else {
            return false
        }
        let kclCompletas = kcl.filter(\.completa)
        guard !kclCompletas.isEmpty else { return false }
        return kclCompletas.allSatisfy {
            guard let r = $0.residuoAmperios else { return false }
            return abs(r) <= 1e-7
        }
    }

    func texto() -> String {
        var salida: [String] = [
            "# Informe de laboratorio · \(titulo)",
            "",
            "## Diagnóstico topológico",
            diagnostico.texto,
            "",
            "## Punto de operación DC",
        ]
        if let errorSimulacion {
            salida.append("No calculado: \(errorSimulacion)")
        } else {
            for nodo in voltajes.keys.sorted() {
                let valor = NotacionIngenieria.formatear(voltajes[nodo] ?? 0, unidad: "V")
                salida.append("V(\(nodo)) = \(valor)")
            }
        }

        salida += ["", "## Componentes · referencia A→B"]
        for medicion in componentes {
            let voltaje = NotacionIngenieria.formatear(medicion.voltajeAB, unidad: "V")
            var linea = "• \(medicion.referencia) [\(medicion.tipo.nombre)] "
                + "\(medicion.nodoA)→\(medicion.nodoB): Vab = \(voltaje)"
            if let corriente = medicion.corrienteAB {
                linea += ", Iab = " + NotacionIngenieria.formatear(corriente, unidad: "A")
            } else {
                linea += ", Iab = no determinada"
            }
            if let potencia = medicion.potenciaAbsorbida {
                linea += ", Pabs = " + NotacionIngenieria.formatear(potencia, unidad: "W")
            }
            if let nota = medicion.nota { linea += " · \(nota)" }
            salida.append(linea)
        }

        salida += ["", "## Comprobación KCL"]
        if kcl.isEmpty {
            salida.append("Sin nodos no-GND comprobables.")
        } else {
            for nodo in kcl {
                if let residuo = nodo.residuoAmperios, nodo.completa {
                    let valor = NotacionIngenieria.formatear(residuo, unidad: "A")
                    let estado = abs(residuo) <= 1e-7 ? "OK" : "REVISAR"
                    salida.append("Nodo \(nodo.nodo): ΣI_saliendo = \(valor) · \(estado)")
                } else {
                    salida.append("Nodo \(nodo.nodo): comprobación parcial · \(nodo.detalle)")
                }
            }
        }

        salida += ["", "## Balance de potencia"]
        if let sumaPotencias, balancePotenciaCompleto {
            salida.append("ΣPabs = " + NotacionIngenieria.formatear(sumaPotencias, unidad: "W"))
            salida.append(abs(sumaPotencias) <= 1e-7
                          ? "Balance cerrado dentro de tolerancia numérica."
                          : "El balance excede la tolerancia; revise modelo, signos o valores.")
        } else {
            salida.append("Balance incompleto: no se inventaron corrientes que no pudieron determinarse.")
        }

        if !advertencias.isEmpty {
            salida += ["", "## Advertencias"]
            salida += advertencias.map { "• \($0)" }
        }
        salida += ["", "## Netlist SPICE", "```spice", spice, "```"]
        return salida.joined(separator: "\n")
    }
}

/// Reconstruye magnitudes derivables del mismo modelo DC que usa el solver.
/// Para ramas ideales el motor usa 1e9 S; el informe replica exactamente esa
/// aproximación en lugar de presentar un cortocircuito matemático distinto.
enum GeneradorInformeLaboratorioCircuitoIPad {
    private static let conductanciaCorto = 1e9

    static func generar(_ esquema: EsquemaEditable) -> InformeLaboratorioCircuitoIPad {
        let diagnostico = AnalizadorTopologiaCircuitoIPad.analizar(esquema)
        let solucion = MotorCircuitoLocal.dc(esquema)
        let mapa = esquema.mapaNodosElectricos()
        let activos = esquema.componentes.filter(\.activo)
        var advertencias: [String] = []

        guard solucion.error == nil else {
            return InformeLaboratorioCircuitoIPad(
                titulo: esquema.titulo,
                diagnostico: diagnostico,
                errorSimulacion: solucion.error,
                voltajes: [:], componentes: [], kcl: [],
                sumaPotencias: nil, balancePotenciaCompleto: false,
                advertencias: [
                    "El informe numérico se detuvo junto con el solver; no se generó un resultado parcial."
                ],
                spice: esquema.spice())
        }

        var voltajes: [Int: Double] = [0: 0]
        for nodo in Set(esquema.nodos.map { mapa[$0.numero] ?? $0.numero }).sorted()
        where nodo >= 0 && nodo < solucion.voltajes.count {
            voltajes[nodo] = solucion.voltajes[nodo]
        }

        func v(_ nodoOriginal: Int) -> Double? {
            let nodo = mapa[nodoOriginal] ?? nodoOriginal
            if nodo == 0 { return 0 }
            guard nodo >= 0, nodo < solucion.voltajes.count else { return nil }
            return solucion.voltajes[nodo]
        }

        var corrientes: [UUID: Double] = [:]
        var fuentesVoltaje: [ComponenteEsquema] = []
        for c in activos {
            guard let va = v(c.nodoA), let vb = v(c.nodoB) else { continue }
            let i: Double?
            switch c.tipo {
            case .resistor:
                i = c.valor > 0 ? (va - vb) / c.valor : nil
            case .capacitor, .capacitorPolarizado:
                i = 0
            case .inductor, .cable, .tierra:
                i = (va - vb) * conductanciaCorto
            case .interruptor, .pulsador:
                i = c.auxiliar > 0.5 ? (va - vb) * conductanciaCorto : 0
            case .fuenteCorriente:
                i = c.valor
            case .probeVoltaje, .probeCorriente, .voltimetro, .osciloscopio:
                i = 0
            case .fuenteVoltaje, .bateria:
                fuentesVoltaje.append(c)
                i = nil
            default:
                i = nil
            }
            if let i, i.isFinite { corrientes[c.id] = i }
        }

        // Corrientes de fuentes ideales de voltaje. Se deducen por KCL sólo
        // cuando un nodo deja una única fuente aún desconocida. El algoritmo se
        // propaga y resuelve redes en árbol/serie sin inventar una solución en
        // lazos de fuentes ideales que sean indeterminados.
        var pendientes = Dictionary(uniqueKeysWithValues: fuentesVoltaje.map { ($0.id, $0) })
        var progreso = true
        while progreso && !pendientes.isEmpty {
            progreso = false
            let nodos = Set(activos.flatMap {
                [mapa[$0.nodoA] ?? $0.nodoA, mapa[$0.nodoB] ?? $0.nodoB]
            }).filter { $0 > 0 }
            for nodo in nodos {
                let incidentesPendientes = pendientes.values.filter {
                    (mapa[$0.nodoA] ?? $0.nodoA) == nodo ||
                    (mapa[$0.nodoB] ?? $0.nodoB) == nodo
                }
                guard incidentesPendientes.count == 1,
                      let fuente = incidentesPendientes.first else { continue }
                var conocido = 0.0
                var completo = true
                for c in activos where c.id != fuente.id {
                    let a = mapa[c.nodoA] ?? c.nodoA
                    let b = mapa[c.nodoB] ?? c.nodoB
                    guard a == nodo || b == nodo else { continue }
                    guard let i = corrientes[c.id] else {
                        if pendientes[c.id] != nil { continue }
                        completo = false
                        break
                    }
                    conocido += a == nodo ? i : -i
                }
                guard completo else { continue }
                let aFuente = mapa[fuente.nodoA] ?? fuente.nodoA
                let corriente = aFuente == nodo ? -conocido : conocido
                guard corriente.isFinite else { continue }
                corrientes[fuente.id] = corriente
                pendientes.removeValue(forKey: fuente.id)
                progreso = true
            }
        }
        if !pendientes.isEmpty {
            advertencias.append(
                "No se pudieron deducir por KCL las corrientes de: "
                + pendientes.values.map(\.referencia).sorted().joined(separator: ", ")
                + ". Se dejaron explícitamente como no determinadas.")
        }

        var mediciones: [MedicionComponenteDCIPad] = []
        var sumaPotencia = 0.0
        var potenciaCompleta = true
        for c in activos {
            let a = mapa[c.nodoA] ?? c.nodoA
            let b = mapa[c.nodoB] ?? c.nodoB
            let va = v(c.nodoA) ?? .nan
            let vb = v(c.nodoB) ?? .nan
            let vab = va - vb
            let i = corrientes[c.id]
            let p = i.flatMap { corriente -> Double? in
                let potencia = vab * corriente
                return potencia.isFinite ? potencia : nil
            }
            if let p {
                sumaPotencia += p
            } else if participaEnBalance(c.tipo) {
                potenciaCompleta = false
            }
            mediciones.append(.init(
                id: c.id, referencia: c.referencia, tipo: c.tipo,
                nodoA: a, nodoB: b, voltajeAB: vab,
                corrienteAB: i, potenciaAbsorbida: p,
                nota: notaModelo(c)))
        }

        let nodosNoGND = Set(activos.flatMap {
            [mapa[$0.nodoA] ?? $0.nodoA, mapa[$0.nodoB] ?? $0.nodoB]
        }).filter { $0 > 0 }.sorted()
        let kcl: [VerificacionKCLNodoIPad] = nodosNoGND.map { nodo in
            var suma = 0.0
            var faltantes: [String] = []
            for c in activos {
                let a = mapa[c.nodoA] ?? c.nodoA
                let b = mapa[c.nodoB] ?? c.nodoB
                guard a == nodo || b == nodo else { continue }
                guard let i = corrientes[c.id] else {
                    faltantes.append(c.referencia)
                    continue
                }
                suma += a == nodo ? i : -i
            }
            let completa = faltantes.isEmpty
            return VerificacionKCLNodoIPad(
                nodo: nodo,
                residuoAmperios: completa && suma.isFinite ? suma : nil,
                completa: completa,
                detalle: completa ? "Todas las ramas tienen corriente derivable."
                    : "Faltan corrientes: \(faltantes.sorted().joined(separator: ", "))")
        }

        if potenciaCompleta, !sumaPotencia.isFinite {
            potenciaCompleta = false
        }
        if potenciaCompleta && abs(sumaPotencia) > 1e-7 {
            advertencias.append(
                "El balance de potencia no cerró dentro de 1e-7 W; revise aproximaciones de ramas ideales y signos.")
        }
        let kclMal = kcl.filter {
            $0.completa && abs($0.residuoAmperios ?? 0) > 1e-7
        }
        if !kclMal.isEmpty {
            advertencias.append(
                "KCL excede tolerancia en nodo(s): "
                + kclMal.map { String($0.nodo) }.joined(separator: ", "))
        }

        return InformeLaboratorioCircuitoIPad(
            titulo: esquema.titulo,
            diagnostico: diagnostico,
            errorSimulacion: nil,
            voltajes: voltajes,
            componentes: mediciones,
            kcl: kcl,
            sumaPotencias: potenciaCompleta ? sumaPotencia : nil,
            balancePotenciaCompleto: potenciaCompleta,
            advertencias: advertencias,
            spice: esquema.spice())
    }

    private static func participaEnBalance(_ tipo: TipoComponenteElectrico) -> Bool {
        switch tipo {
        case .probeVoltaje, .probeCorriente, .voltimetro, .osciloscopio:
            return false
        default:
            return true
        }
    }

    private static func notaModelo(_ c: ComponenteEsquema) -> String? {
        switch c.tipo {
        case .capacitor, .capacitorPolarizado:
            return "abierto en estado estacionario DC"
        case .inductor, .cable, .tierra:
            return "rama ideal aproximada por el solver con 1e9 S"
        case .interruptor, .pulsador:
            return c.auxiliar > 0.5 ? "cerrado · 1e9 S" : "abierto"
        case .probeVoltaje, .probeCorriente, .voltimetro, .osciloscopio:
            return "instrumento ideal · no carga el circuito"
        default:
            return nil
        }
    }
}

final class PanelInformeLaboratorioCircuitoIPad: UIViewController {
    var alInsertarTexto: ((String) -> Void)?
    private let informe: InformeLaboratorioCircuitoIPad
    private let texto = UITextView()

    init(esquema: EsquemaEditable) {
        informe = GeneradorInformeLaboratorioCircuitoIPad.generar(esquema)
        super.init(nibName: nil, bundle: nil)
    }

    required init?(coder: NSCoder) { fatalError("init(coder:) no implementado") }

    override func viewDidLoad() {
        super.viewDidLoad()
        title = "Informe de laboratorio"
        view.backgroundColor = .systemBackground
        texto.text = informe.texto()
        texto.isEditable = false
        texto.font = .monospacedSystemFont(ofSize: 13, weight: .regular)
        texto.translatesAutoresizingMaskIntoConstraints = false
        texto.accessibilityIdentifier = "circuito.informe.texto"
        view.addSubview(texto)
        navigationItem.rightBarButtonItem = UIBarButtonItem(
            title: "Insertar", style: .plain, target: self,
            action: #selector(insertar(_:)))
        NSLayoutConstraint.activate([
            texto.topAnchor.constraint(equalTo: view.safeAreaLayoutGuide.topAnchor, constant: 8),
            texto.leadingAnchor.constraint(equalTo: view.leadingAnchor, constant: 12),
            texto.trailingAnchor.constraint(equalTo: view.trailingAnchor, constant: -12),
            texto.bottomAnchor.constraint(equalTo: view.safeAreaLayoutGuide.bottomAnchor, constant: -8),
        ])
    }

    @objc private func insertar(_ remitente: UIBarButtonItem) {
        let menu = UIAlertController(title: "Informe", message: nil,
                                     preferredStyle: .actionSheet)
        menu.addAction(UIAlertAction(title: "Insertar en el apunte", style: .default) {
            [weak self] _ in
            guard let self else { return }
            self.alInsertarTexto?(self.informe.texto())
        })
        menu.addAction(UIAlertAction(title: "Copiar", style: .default) {
            [weak self] _ in UIPasteboard.general.string = self?.informe.texto()
        })
        menu.addAction(UIAlertAction(title: "Cancelar", style: .cancel))
        menu.popoverPresentationController?.barButtonItem = remitente
        present(menu, animated: true)
    }
}
