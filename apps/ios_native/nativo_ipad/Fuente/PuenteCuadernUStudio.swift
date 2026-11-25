import Foundation
import SwiftUI

/// Puente Swift sobre `cuadernu_studio_ffi.h`: comparte el motor C++17 portátil
/// de Windows y Android dentro de CuadernU para iOS y iPadOS:
/// - Interoperabilidad binaria `.cudoc` (abrir/guardar cuadernos de Windows/Android)
/// - Exportación vectorial PDF / Markdown / CAD DXF e importación DOCX/PPTX/CSV/XLSX
/// - CAS de ingeniería (`#varsub`, unidades SI, ecuaciones, SEL, matrices, gráficas 2D)
/// - Balanceador de química de 118 elementos
/// - Simulador de circuitos MNA (DC, AC, Bode, Transitorio, Laplace) y Mapa de Karnaugh
/// - Grafo de conocimiento 2D, repetición espaciada SM-2 y Horarios TEC.
final class CuadernUStudioBridge {
    static let compartido = CuadernUStudioBridge()

    private init() {}

    private func invocarCadena(_ fn: (UnsafeMutablePointer<CChar>?, Int32) -> Int32) -> String {
        var cap: Int32 = 16384
        for _ in 0..<2 {
            var buf = [CChar](repeating: 0, count: Int(cap))
            let n = buf.withUnsafeMutableBufferPointer { ptr in
                fn(ptr.baseAddress, cap)
            }
            if n < 0 { return "" }
            if n < cap {
                return String(cString: buf)
            }
            cap = n + 32
        }
        return ""
    }

    private func parsearJson(_ raw: String) -> [String: Any] {
        guard let data = raw.data(using: .utf8),
              let obj = try? JSONSerialization.jsonObject(with: data) as? [String: Any] else {
            return [:]
        }
        return obj
    }

    func capacidades() -> [String: Any] {
        parsearJson(invocarCadena { cuadernu_ffi_capabilities_json($0, $1) })
    }

    func evaluarCas(_ expresion: String, grados: Bool = true) -> [String: Any] {
        let raw = expresion.withCString { ePtr in
            "{}".withCString { envPtr in
                invocarCadena { cuadernu_cas_eval_json(ePtr, envPtr, grados ? 1 : 0, $0, $1) }
            }
        }
        return parsearJson(raw)
    }

    func resolverEcuacion(_ ecuacion: String, variable: String = "x") -> [String: Any] {
        let raw = ecuacion.withCString { eqPtr in
            variable.withCString { vPtr in
                invocarCadena { cuadernu_cas_solve_equation_json(eqPtr, vPtr, $0, $1) }
            }
        }
        return parsearJson(raw)
    }

    func resolverSistemaLineal(_ ecuaciones: String) -> [String: Any] {
        let raw = ecuaciones.withCString { ptr in
            invocarCadena { cuadernu_cas_solve_linear_system_json(ptr, $0, $1) }
        }
        return parsearJson(raw)
    }

    func evaluarMatriz(_ expresion: String) -> [String: Any] {
        let raw = expresion.withCString { ptr in
            invocarCadena { cuadernu_cas_matrix_eval_json(ptr, $0, $1) }
        }
        return parsearJson(raw)
    }

    func analizarQuimica(_ formulaOReaccion: String) -> [String: Any] {
        let raw = formulaOReaccion.withCString { ptr in
            invocarCadena { cuadernu_chem_analyze_json(ptr, $0, $1) }
        }
        return parsearJson(raw)
    }

    func resolverCircuito(netlist: String,
                          modo: String = "dc",
                          param1: Double = 1000.0,
                          param2: Double = 0.0,
                          nodoSonda: Int32 = 2) -> [String: Any] {
        let raw = netlist.withCString { nPtr in
            modo.withCString { mPtr in
                invocarCadena {
                    cuadernu_circuit_solve_json(nPtr, mPtr, param1, param2, nodoSonda, $0, $1)
                }
            }
        }
        return parsearJson(raw)
    }

    func simplificarKMap(variables: Int32, mintermsCsv: String, dontCaresCsv: String = "") -> [String: Any] {
        let raw = mintermsCsv.withCString { mPtr in
            dontCaresCsv.withCString { dPtr in
                invocarCadena { cuadernu_circuit_kmap_json(variables, mPtr, dPtr, $0, $1) }
            }
        }
        return parsearJson(raw)
    }

    func revisarSm2(repeticiones: Int32,
                    intervaloDias: Int32,
                    factorFacilidad: Double,
                    calidad: Int32) -> [String: Any] {
        let ahora = UInt64(Date().timeIntervalSince1970)
        return parsearJson(invocarCadena {
            cuadernu_sm2_review_json(repeticiones, intervaloDias, factorFacilidad, calidad, ahora, $0, $1)
        })
    }

    /// Lee un archivo `.cudoc` creado en Windows o Android y devuelve su instantánea JSON completa.
    func inspeccionarCudoc(ruta: String) -> [String: Any]? {
        guard let doc = cuadernu_doc_create(0) else { return nil }
        defer { cuadernu_doc_destroy(doc) }
        let ok = ruta.withCString { cuadernu_doc_load_cudoc(doc, $0) }
        guard ok != 0 else { return nil }
        return parsearJson(invocarCadena { cuadernu_doc_get_snapshot_json(doc, $0, $1) })
    }

    /// Crea y guarda un `.cudoc` binario compatible con Windows y Android.
    @discardableResult
    func exportarResumenCudoc(titulo: String, bloquesTexto: [String], rutaDestino: String) -> Bool {
        guard let doc = cuadernu_doc_create(0) else { return false }
        defer { cuadernu_doc_destroy(doc) }
        titulo.withCString { _ = cuadernu_doc_set_title(doc, $0) }
        var y = 40.0
        for texto in bloquesTexto {
            texto.withCString { tPtr in
                _ = cuadernu_doc_add_text_box(doc, 0, 0, 36.0, y, 520.0, 64.0, tPtr, 13.5, 0x1D1D1FFF)
            }
            y += 74.0
        }
        return rutaDestino.withCString { cuadernu_doc_save_cudoc(doc, $0) != 0 }
    }
}
