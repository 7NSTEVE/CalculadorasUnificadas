import SwiftUI

/// Gráfica de una expresión, con cursor y puntos notables.
///
/// Los valores los da la Casio, uno a uno. Por eso hay barra de progreso y por eso se
/// avisa de cuánto va a tardar: es el precio de que la curva y el resultado de pulsar
/// `=` no puedan discrepar nunca.
struct VistaGrafica: View {
    let modelo: ModeloCalculadora
    @ObservedObject var emulador: EmuladorCalculadora
    @Environment(\.dismiss) private var cerrar

    @StateObject private var muestreador = MuestreadorCasio()

    @State private var expresion = "x^2-1"
    @State private var desde = "-5"
    @State private var hasta = "5"
    @State private var cantidad: Double = 40
    @State private var cursorX: Double?

    private var rango: (Double, Double)? {
        guard let a = LectorContenido.numero(desde),
              let b = LectorContenido.numero(hasta), b > a else { return nil }
        return (a, b)
    }

    var body: some View {
        NavigationView {
            VStack(spacing: 0) {
                controles
                Divider().overlay(Color.white.opacity(0.15))
                lienzo
                if !muestreador.muestras.isEmpty { panelInferior }
            }
            .background(Color(white: 0.13))
            .foregroundStyle(.white)
            .navigationTitle("Gráfica")
            .navigationBarTitleDisplayMode(.inline)
            .toolbar {
                ToolbarItem(placement: .confirmationAction) {
                    Button("Listo") { cerrar() }
                }
            }
        }
    }

    // MARK: - Controles

    private var controles: some View {
        VStack(spacing: 8) {
            HStack(spacing: 8) {
                Text("y =").font(.system(.body, design: .monospaced))
                TextField("x^2-1", text: $expresion)
                    .textFieldStyle(.roundedBorder)
                    .autocorrectionDisabled()
                    .textInputAutocapitalization(.never)
            }
            HStack(spacing: 8) {
                Text("x de").font(.caption)
                TextField("-5", text: $desde).textFieldStyle(.roundedBorder).frame(width: 70)
                Text("a").font(.caption)
                TextField("5", text: $hasta).textFieldStyle(.roundedBorder).frame(width: 70)
                Spacer()
                Text("\(Int(cantidad)) pts").font(.caption).foregroundStyle(.secondary)
            }
            Slider(value: $cantidad, in: 10...100, step: 5)
                .tint(.orange)

            if muestreador.muestreando {
                VStack(spacing: 4) {
                    ProgressView(value: muestreador.progreso).tint(.orange)
                    HStack {
                        Text(muestreador.estado).font(.caption2).foregroundStyle(.secondary)
                        Spacer()
                        Button("Cancelar") { muestreador.cancelar() }.font(.caption2)
                    }
                }
            } else {
                Button {
                    dibujar()
                } label: {
                    Label("Calcular en la Casio (~\(Int(Grafica.segundosEstimados(muestras: Int(cantidad)))) s)",
                          systemImage: "waveform.path.ecg")
                        .frame(maxWidth: .infinity)
                }
                .buttonStyle(.borderedProminent)
                .tint(.orange)
                .disabled(rango == nil || expresion.isEmpty)
            }
        }
        .padding(12)
    }

    private func dibujar() {
        guard let (a, b) = rango else { return }
        cursorX = nil
        Task {
            await muestreador.muestrear(expresion: expresion, desde: a, hasta: b,
                                        cantidad: Int(cantidad), modelo: modelo,
                                        emulador: emulador)
        }
    }

    // MARK: - Lienzo

    private var lienzo: some View {
        GeometryReader { geometria in
            let tamano = geometria.size
            Canvas { contexto, _ in
                dibujarCurva(contexto, tamano: tamano)
            }
            .contentShape(Rectangle())
            .gesture(
                DragGesture(minimumDistance: 0)
                    .onChanged { toque in
                        cursorX = xEnGrafica(toque.location.x, ancho: tamano.width)
                    }
            )
        }
        .frame(maxHeight: .infinity)
    }

    private var muestrasValidas: [MuestraCurva] { muestreador.muestras }

    private func xEnGrafica(_ pantalla: CGFloat, ancho: CGFloat) -> Double? {
        guard let primera = muestrasValidas.first, let ultima = muestrasValidas.last,
              ancho > 0 else { return nil }
        let proporcion = max(0, min(1, Double(pantalla / ancho)))
        return primera.x + proporcion * (ultima.x - primera.x)
    }

    private func dibujarCurva(_ contexto: GraphicsContext, tamano: CGSize) {
        let muestras = muestrasValidas
        guard muestras.count >= 2,
              let primera = muestras.first, let ultima = muestras.last,
              let vertical = Grafica.rangoVertical(muestras),
              ultima.x > primera.x, vertical.maximo > vertical.minimo else { return }

        func puntoEn(_ x: Double, _ y: Double) -> CGPoint {
            let px = (x - primera.x) / (ultima.x - primera.x) * Double(tamano.width)
            let py = (1 - (y - vertical.minimo) / (vertical.maximo - vertical.minimo))
                     * Double(tamano.height)
            return CGPoint(x: px, y: py)
        }

        // Ejes, si el cero cae dentro de la ventana.
        var ejes = Path()
        if vertical.minimo <= 0 && vertical.maximo >= 0 {
            let y = puntoEn(primera.x, 0).y
            ejes.move(to: CGPoint(x: 0, y: y))
            ejes.addLine(to: CGPoint(x: tamano.width, y: y))
        }
        if primera.x <= 0 && ultima.x >= 0 {
            let x = puntoEn(0, vertical.minimo).x
            ejes.move(to: CGPoint(x: x, y: 0))
            ejes.addLine(to: CGPoint(x: x, y: tamano.height))
        }
        contexto.stroke(ejes, with: .color(.white.opacity(0.25)), lineWidth: 1)

        // La curva. Los huecos (errores de la calculadora) parten el trazo en vez de
        // unirse con una recta que no existe.
        var trazo = Path()
        var dibujando = false
        for muestra in muestras {
            guard let y = muestra.y, y.isFinite else { dibujando = false; continue }
            let punto = puntoEn(muestra.x, y)
            if dibujando {
                trazo.addLine(to: punto)
            } else {
                trazo.move(to: punto)
                dibujando = true
            }
        }
        contexto.stroke(trazo, with: .color(.orange), lineWidth: 2)

        // Puntos notables.
        for notable in Grafica.raices(muestras) + Grafica.extremos(muestras) {
            let punto = puntoEn(notable.x, notable.y)
            let marca = Path(ellipseIn: CGRect(x: punto.x - 4, y: punto.y - 4,
                                               width: 8, height: 8))
            contexto.fill(marca, with: .color(notable.clase == .raiz ? .cyan : .yellow))
        }

        // Cursor.
        if let cursorX, let y = Grafica.interpolar(muestras, en: cursorX) {
            let punto = puntoEn(cursorX, y)
            var linea = Path()
            linea.move(to: CGPoint(x: punto.x, y: 0))
            linea.addLine(to: CGPoint(x: punto.x, y: tamano.height))
            contexto.stroke(linea, with: .color(.white.opacity(0.4)),
                            style: StrokeStyle(lineWidth: 1, dash: [4, 4]))
            let bola = Path(ellipseIn: CGRect(x: punto.x - 5, y: punto.y - 5,
                                              width: 10, height: 10))
            contexto.fill(bola, with: .color(.white))
        }
    }

    // MARK: - Panel inferior

    private var panelInferior: some View {
        VStack(alignment: .leading, spacing: 6) {
            if let cursorX, let y = Grafica.interpolar(muestrasValidas, en: cursorX) {
                Text(String(format: "x = %.4g    y = %.4g", cursorX, y))
                    .font(.system(.callout, design: .monospaced))
            }

            let notables = Grafica.raices(muestrasValidas) + Grafica.extremos(muestrasValidas)
            if notables.isEmpty {
                Text("No se encontraron cortes ni extremos en este rango.")
                    .font(.caption).foregroundStyle(.secondary)
            } else {
                ScrollView(.horizontal, showsIndicators: false) {
                    HStack(spacing: 8) {
                        ForEach(notables) { notable in
                            VStack(spacing: 1) {
                                Text(notable.clase.rawValue)
                                    .font(.system(size: 9)).foregroundStyle(.secondary)
                                Text(String(format: "%.4g", notable.x))
                                    .font(.system(size: 11, design: .monospaced))
                            }
                            .padding(.horizontal, 8).padding(.vertical, 4)
                            .background(Color(white: 0.20), in: RoundedRectangle(cornerRadius: 6))
                        }
                    }
                }
            }

            // Honestidad sobre la precisión: los extremos son de las muestras, no de
            // la función. Con 40 puntos en un rango de 10, el pico puede estar a
            // 0,25 de donde se marca, y eso hay que decirlo.
            Text("Calculado en la Casio con \(muestrasValidas.count) puntos. "
                 + "Los cortes se interpolan entre muestras; con más puntos, más fino.")
                .font(.system(size: 9))
                .foregroundStyle(.secondary)
        }
        .padding(12)
    }
}

/// Árbol de intentos: probar un camino, verlo fallar y volver sin perderlo.
struct VistaArbol: View {
    @ObservedObject var arbol: ArbolIntentos
    @ObservedObject var emulador: EmuladorCalculadora
    @Environment(\.dismiss) private var cerrar

    @State private var nombreNuevo = ""
    @State private var pidiendoNombre = false

    var body: some View {
        NavigationView {
            List {
                Section {
                    Button {
                        pidiendoNombre = true
                    } label: {
                        Label("Empezar un intento nuevo aquí", systemImage: "arrow.triangle.branch")
                    }
                } footer: {
                    Text("Se guarda el estado de la calculadora tal cual está. "
                         + "Si el intento no sale, se marca y se vuelve, pero no se "
                         + "borra: poder comparar es lo que enseña por qué no salía.")
                }

                if arbol.ramas.isEmpty {
                    Text("Todavía no hay intentos.")
                        .foregroundStyle(.secondary)
                } else {
                    ForEach(arbol.ramas) { rama in
                        fila(rama)
                    }
                }
            }
            .navigationTitle("Intentos")
            .navigationBarTitleDisplayMode(.inline)
            .toolbar {
                ToolbarItem(placement: .confirmationAction) {
                    Button("Listo") { cerrar() }
                }
            }
            .alert("Nombre del intento", isPresented: $pidiendoNombre) {
                TextField("p. ej. con Thévenin", text: $nombreNuevo)
                Button("Empezar") {
                    if let estado = emulador.guardarEstado() {
                        arbol.bifurcar(nombre: nombreNuevo.isEmpty ? "Intento" : nombreNuevo,
                                       estado: estado, teclas: emulador.historial.count)
                    }
                    nombreNuevo = ""
                }
                Button("Cancelar", role: .cancel) { nombreNuevo = "" }
            }
        }
    }

    private func fila(_ rama: ArbolIntentos.Rama) -> some View {
        HStack(spacing: 10) {
            Image(systemName: rama.descartada ? "xmark.circle.fill"
                  : (rama.id == arbol.actual ? "arrow.right.circle.fill" : "circle"))
                .foregroundStyle(rama.descartada ? Color.red
                                 : (rama.id == arbol.actual ? Color.orange : Color.secondary))
            VStack(alignment: .leading, spacing: 2) {
                Text(rama.nombre)
                    .strikethrough(rama.descartada)
                Text("desde la tecla \(rama.teclasAlEmpezar)")
                    .font(.caption).foregroundStyle(.secondary)
            }
            Spacer()
            if !rama.descartada {
                Button("Estaba mal") {
                    if let volver = arbol.descartar(rama.id,
                                                    estadoActual: emulador.guardarEstado()) {
                        emulador.cargarEstado(volver)
                    }
                }
                .font(.caption)
                .buttonStyle(.bordered)
            } else {
                Button("Ver") { emulador.cargarEstado(rama.estadoInicial) }
                    .font(.caption)
                    .buttonStyle(.bordered)
            }
        }
    }
}
