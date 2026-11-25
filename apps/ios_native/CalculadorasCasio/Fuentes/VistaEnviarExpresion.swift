import SwiftUI
import UIKit

extension DisposicionCalculadora {
    /// Traduce nombres lógicos de tecla a códigos de matriz.
    ///
    /// Devuelve `nil` si alguno no existe en este modelo, en vez de saltárselo: una
    /// secuencia a la que le falta una tecla no da un resultado peor, da **otro**
    /// resultado, y eso es mucho más difícil de detectar.
    func codigos(para nombres: [String]) -> [UInt8]? {
        var codigos: [UInt8] = []
        for nombre in nombres {
            guard let tecla = teclas.first(where: { $0.nombre == nombre }) else { return nil }
            codigos.append(tecla.codigo)
        }
        return codigos
    }
}

/// Pegar una expresión de OneNote y mandarla a la Casio.
///
/// La cuenta la sigue haciendo el firmware original: esto solo decide qué teclas se
/// pulsan. Y las enseña antes de pulsarlas, porque interpretar lo que alguien
/// escribió es justo donde se puede fallar en silencio.
struct VistaEnviarExpresion: View {
    let modelo: ModeloCalculadora
    @ObservedObject var emulador: EmuladorCalculadora
    @Environment(\.dismiss) private var cerrar

    @State private var expresion: String = ""
    @State private var teclas: [String] = []
    @State private var avisos: [String] = []
    @State private var error: String?

    /// Se recuerda entre envíos: quien quiere aprender dónde están las teclas lo
    /// quiere siempre, y quien no, tampoco quiere elegirlo cada vez.
    @AppStorage("velocidadEnvio") private var velocidadCruda: Double = 0

    private var traductor: TraductorExpresion.Modelo {
        modelo.modeloTraductor
    }

    private var velocidad: VelocidadEnvio {
        VelocidadEnvio(rawValue: velocidadCruda) ?? .instantaneo
    }

    var body: some View {
        NavigationStack {
            Form {
                Section {
                    TextField("p. ej. sqrt((12^2)+(5^2))", text: $expresion, axis: .vertical)
                        .font(.system(.body, design: .monospaced))
                        .lineLimit(1...4)
                        .autocorrectionDisabled()
                        .textInputAutocapitalization(.never)
                        .onChange(of: expresion) { _ in interpretar() }

                    Button {
                        expresion = UIPasteboard.general.string ?? ""
                        interpretar()
                    } label: {
                        Label("Pegar del portapapeles", systemImage: "doc.on.clipboard")
                    }
                } header: {
                    Text("Expresión")
                } footer: {
                    Text("Entiende + − × ÷, paréntesis, ^, sqrt, sin, cos, tan, ln, log y π. "
                         + "Las letras sueltas hay que sustituirlas por su valor.")
                }

                if let error {
                    Section {
                        Label(error, systemImage: "exclamationmark.triangle")
                            .foregroundStyle(.orange)
                    }
                }

                if !avisos.isEmpty {
                    Section("Ojo") {
                        ForEach(avisos, id: \.self) { aviso in
                            Text(aviso).font(.footnote)
                        }
                    }
                }

                if !teclas.isEmpty {
                    Section {
                        Text(teclas.map(etiqueta).joined(separator: "  "))
                            .font(.system(.callout, design: .monospaced))
                    } header: {
                        Text("Se van a pulsar \(teclas.count) teclas")
                    } footer: {
                        Text("Míralo antes de enviarlo. Es lo mismo que teclearías tú "
                             + "en la calculadora, en ese orden.")
                    }

                    Section {
                        Picker("Velocidad", selection: $velocidadCruda) {
                            ForEach(VelocidadEnvio.allCases) { velocidad in
                                Text(velocidad.etiqueta).tag(velocidad.rawValue)
                            }
                        }
                    } header: {
                        Text("Cómo teclearlo")
                    } footer: {
                        Text(velocidad.animada
                             ? "Se irán encendiendo las teclas en la carátula. Sirve para "
                               + "aprender dónde están: en el examen tecleas tú."
                             : "Se pulsa todo de golpe.")
                    }
                }
            }
            .navigationTitle("Enviar a la Casio")
            .navigationBarTitleDisplayMode(.inline)
            .toolbar {
                ToolbarItem(placement: .navigationBarLeading) {
                    Button("Cancelar") { cerrar() }
                }
                ToolbarItem(placement: .navigationBarTrailing) {
                    Button("Enviar") { enviar() }
                        .disabled(teclas.isEmpty || emulador.rebobinando)
                }
            }
            .onAppear {
                // Si vienes de copiar algo en OneNote, lo normal es que sea eso.
                if let pegado = UIPasteboard.general.string, expresion.isEmpty {
                    expresion = pegado
                    interpretar()
                }
            }
        }
    }

    private func interpretar() {
        let texto = expresion.trimmingCharacters(in: .whitespacesAndNewlines)
        guard !texto.isEmpty else {
            teclas = []; avisos = []; error = nil
            return
        }
        do {
            let resultado = try TraductorExpresion.traducir(texto, para: traductor)
            guard modelo.disposicion.codigos(para: resultado.teclas) != nil else {
                teclas = []; avisos = resultado.avisos
                error = "hay teclas que no existen en la \(modelo.nombre)"
                return
            }
            teclas = resultado.teclas
            avisos = resultado.avisos
            error = nil
        } catch let fallo as TraductorExpresion.Fallo {
            teclas = []
            error = fallo.description
        } catch {
            teclas = []
            self.error = "no se pudo interpretar la expresión"
        }
    }

    private func enviar() {
        guard let codigos = modelo.disposicion.codigos(para: teclas) else { return }
        emulador.teclear(codigos, velocidad: velocidad)
        cerrar()
    }

    /// El nombre interno no se le enseña a nadie: en la vista previa va el símbolo.
    private func etiqueta(_ nombre: String) -> String {
        switch nombre {
        case "MAS": return "+"
        case "MENOS": return "−"
        case "POR": return "×"
        case "ENTRE": return "÷"
        case "PUNTO": return "."
        case "COMA": return ","
        case "PAREN_ABRE": return "("
        case "PAREN_CIERRA": return ")"
        case "NEGATIVO": return "(−)"
        case "RAIZ": return "√"
        case "CUADRADO": return "x²"
        case "POTENCIA": return "^"
        case "DERECHA": return "▶"
        case "SIN": return "sin"
        case "COS": return "cos"
        case "TAN": return "tan"
        case "LN": return "ln"
        case "LOG": return "log"
        case "LOG_B": return "log▫"
        case "EXP": return "×10ˣ"
        case "SHIFT": return "SHIFT"
        default: return nombre
        }
    }
}
