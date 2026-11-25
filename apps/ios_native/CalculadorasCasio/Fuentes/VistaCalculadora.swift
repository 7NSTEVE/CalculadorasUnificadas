import QuartzCore
import SwiftUI
import UIKit

/// La calculadora completa: su carátula real, el LCD encima, las zonas táctiles y
/// la capa de estudio alrededor.
///
/// La carátula es la imagen auténtica extraída del emulador de Casio, así que no hay
/// que redibujar ninguna tecla: bastan áreas invisibles encima, en las coordenadas
/// que sacó `Tools/analysis/key_layouts.py` de la tabla oficial de Casio.
struct VistaCalculadora: View {
    let modelo: ModeloCalculadora

    @StateObject private var emulador: EmuladorCalculadora
    @StateObject private var sesion: SesionEstudio

    @State private var mostrarDiagnostico = false
    @State private var mostrarCinta = false
    @State private var mostrarEnviar = false
    @State private var mostrarProcedimientos = false
    @State private var mostrarFormulas = false
    @State private var mostrarExportar = false
    @State private var mostrarMePerdi = false
    @State private var mostrarRebobinado = false
    @State private var mostrarPlano = false
    @State private var mostrarFicha = false
    @State private var mostrarGrafica = false
    @State private var mostrarArbol = false
    @State private var mostrarPaleta = false
    @State private var mostrarMaterias = false
    @State private var mostrarAprender = false
    @AppStorage("modoAprendizaje") private var modoCrudo = ModoAprendizaje.asistido.rawValue
    @State private var guion = GuionEjercicio()
    @AppStorage("soloUnPaso") private var soloUnPaso = false

    /// Ramas de intentos. Vive aquí y no en la sesión porque son de la máquina: sus
    /// volcados solo valen para esta ROM y este modelo.
    @StateObject private var arbol = ArbolIntentos()
    @StateObject private var errores = ContadorErrores()
    @StateObject private var registro = RegistroTeclas()
    @StateObject private var atencion = VigilanteAtencion()
    @StateObject private var indicadores = VigilanteIndicadores()
    @StateObject private var cuaderno = Cuaderno()
    @StateObject private var examen = SesionExamen()
    @StateObject private var preferencias = Preferencias()

    /// Modo de ayuda. En examen la app no teclea ni señala nada: es lo que se está
    /// practicando, y una ayuda que se cuela hace que la práctica no valga.
    private var modoAprendizaje: ModoAprendizaje {
        ModoAprendizaje(rawValue: modoCrudo) ?? .asistido
    }

    /// Última tecla tocada, para descartar el rebote del dedo.
    @State private var ultimoToque: (codigo: UInt8, instante: Double)?
    @State private var avisoRapido: String?

    /// Segundos que se retiene un `AC` para poder arrepentirse. Cero lo desactiva,
    /// que es lo que quiere quien ya tiene el dedo hecho.
    @AppStorage("retardoBorrado") private var retardoBorrado = 0.0

    /// Teclas señaladas sobre la carátula, en orden, sin pulsarlas.
    ///
    /// Señalar y pulsar no son lo mismo: si la app lo hace todo, nunca aprendes
    /// dónde está la tecla, y en el examen la calculadora no teclea sola.
    @State private var guiaTeclado: [String] = []
    @AppStorage("hapticaActiva") private var hapticaActiva = true

    @Environment(\.scenePhase) private var fase

    init(modelo: ModeloCalculadora) {
        self.modelo = modelo
        _emulador = StateObject(wrappedValue: EmuladorCalculadora(modelo: modelo))
        _sesion = StateObject(wrappedValue: SesionEstudio(modelo: modelo.id))
    }

    var body: some View {
        GeometryReader { geometria in
            // En iPad sobra sitio a los lados de la calculadora: en vez de dejar
            // barras negras, ahí va la cinta de cálculo. Si no cabe —en Slide Over
            // junto a OneNote, que es el caso importante— el panel desaparece
            // entero y queda una sola línea de contexto.
            let cabePanel = geometria.size.width > 720

            HStack(spacing: 0) {
                VStack(spacing: 0) {
                    if !cabePanel { barraCompacta }
                    zonaCalculadora(disponible: CGSize(
                        width: cabePanel ? geometria.size.width - 320 : geometria.size.width,
                        height: cabePanel ? geometria.size.height : geometria.size.height - 34))
                }

                if cabePanel {
                    PanelEstudio(sesion: sesion, emulador: emulador,
                                 nombreCalculadora: modelo.nombre)
                        .frame(width: 320)
                }
            }
        }
        .background(Color(white: 0.08))
        .navigationTitle(modelo.nombre)
        .navigationBarTitleDisplayMode(.inline)
        .toolbar { barraHerramientas }
        .sheet(isPresented: $mostrarCinta) {
            NavigationStack {
                PanelEstudio(sesion: sesion, emulador: emulador,
                             nombreCalculadora: modelo.nombre)
                    .navigationTitle("Cinta de cálculo")
                    .navigationBarTitleDisplayMode(.inline)
                    .toolbar {
                        ToolbarItem(placement: .navigationBarTrailing) {
                            Button("Listo") { mostrarCinta = false }
                        }
                    }
            }
        }
        .sheet(isPresented: $mostrarEnviar) {
            VistaEnviarExpresion(modelo: modelo, emulador: emulador)
        }
        .sheet(isPresented: $mostrarFormulas) {
            VistaFormulas(modelo: modelo.modeloTraductor) { expresion in
                // La fórmula no calcula nada: genera la expresión y la teclea la
                // Casio, que es lo que hace que el número sea de fiar.
                if let traduccion = try? TraductorExpresion.traducir(
                        expresion, para: modelo.modeloTraductor),
                   let codigos = modelo.disposicion.codigos(para: traduccion.teclas) {
                    emulador.teclear(codigos, velocidad: .normal)
                } else {
                    avisar("Esa fórmula no se pudo teclear en este modelo")
                }
            }
        }
        .sheet(isPresented: $mostrarPaleta) {
            PaletaComandos { clave in ejecutar(clave) }
        }
        .sheet(isPresented: $mostrarMaterias) {
            VistaMaterias(cuaderno: cuaderno)
        }
        .sheet(isPresented: $mostrarAprender) {
            VistaEntrenamiento(
                modelo: modelo,
                modo: Binding(get: { modoAprendizaje },
                              set: { modoCrudo = $0.rawValue }),
                examen: examen,
                alSenalar: { guiaTeclado = $0 })
        }
        .sheet(isPresented: $mostrarGrafica) {
            VistaGrafica(modelo: modelo, emulador: emulador)
        }
        .sheet(isPresented: $mostrarArbol) {
            VistaArbol(arbol: arbol, emulador: emulador)
        }
        .sheet(isPresented: $mostrarPlano) {
            VistaVisualizador(emulador: emulador, sesion: sesion)
        }
        .sheet(isPresented: $mostrarFicha) {
            VistaDiagnostico(modelo: modelo, emulador: emulador)
        }
        .sheet(isPresented: $mostrarExportar) {
            VistaExportar(sesion: sesion, nombreModelo: modelo.nombre)
        }
        .sheet(isPresented: $mostrarMePerdi) {
            VistaMePerdi(sesion: sesion)
        }
        .sheet(isPresented: $mostrarRebobinado) {
            NavigationStack {
                ScrollView {
                    VistaRebobinado(emulador: emulador).padding()
                }
                .background(Color(white: 0.13))
                .foregroundStyle(.white)
                .navigationTitle("Rebobinar")
                .navigationBarTitleDisplayMode(.inline)
                .toolbar {
                    ToolbarItem(placement: .confirmationAction) {
                        Button("Listo") { mostrarRebobinado = false }
                    }
                }
            }
        }
        .sheet(isPresented: $mostrarProcedimientos) {
            VistaProcedimientos(
                modelo: modelo,
                alSenalarEnTeclado: { guiaTeclado = $0 },
                alEjecutar: { nombres in
                    guiaTeclado = []
                    if let codigos = modelo.disposicion.codigos(para: nombres) {
                        emulador.teclear(codigos, velocidad: .normal)
                    }
                })
        }
        .onAppear {
            emulador.arrancar()
            emulador.retardoBorradoSegundos = retardoBorrado
        }
        .onReceive(emulador.$indicadores) { crudos in
            // El modo angular se lee del LCD, igual que lo lee el usuario. Cambiar de
            // DEG a RAD sin darse cuenta arruina un examen entero de trigonometría.
            let tabla = modelo.hardwareId == 4 ? IndicadorEstado.classWiz
                                               : IndicadorEstado.esPlus
            indicadores.observar(indicadores:
                IndicadorEstado.activos(en: crudos, tabla: tabla).map(\.etiqueta))
        }
        .onChange(of: retardoBorrado) { nuevo in
            emulador.retardoBorradoSegundos = nuevo
        }
        .onDisappear {
            emulador.parar()
            sesion.salioDeLaApp(teclas: emulador.historial, estado: emulador.guardarEstado())
        }
        .onChange(of: fase) { nueva in
            switch nueva {
            case .active: sesion.volvioALaApp()
            case .background, .inactive:
                sesion.salioDeLaApp(teclas: emulador.historial, estado: emulador.guardarEstado())
            @unknown default: break
            }
        }
        .overlay(alignment: .top) {
            VStack(spacing: 8) {
                AvisoBorradoPendiente(emulador: emulador)
                avisoCambioDeModo
                avisoRecordatorio
            }
        }
        .overlay(alignment: .bottom) {
            VStack(spacing: 8) {
                avisoBreve
                avisoEnvio
                avisoGuia
                ofertaReanudar
            }
        }
        .overlay { avisoRebobinando }
        .overlay { avisoSinRom }
        .overlay(alignment: .bottom) {
            VStack(spacing: 8) {
                BarraEsenciales(modelo: modelo, preferencias: preferencias) { tecla in
                    // Manda exactamente el mismo código que la tecla de la carátula:
                    // por debajo, la Casio no distingue de dónde vino el toque.
                    emulador.pulsar(tecla.codigo)
                    sesion.registrar(tecla: tecla.nombre)
                    registro.registrar(tecla.nombre)
                    atencion.registrarTecla()
                    if hapticaActiva { vibrar(por: tecla.nombre) }
                    DispatchQueue.main.asyncAfter(deadline: .now() + 0.05) {
                        emulador.soltar()
                        if tecla.nombre == "IGUAL" { cerrarOperacion() }
                    }
                }
                TiraTeclas(registro: registro)
            }
        }
        .background(atajos)
    }

    // MARK: - Modo compacto

    /// Todo el contexto reducido a una línea, para cuando la app comparte pantalla
    /// con OneNote y cada punto de ancho cuenta.
    private var barraCompacta: some View {
        Button { mostrarCinta = true } label: {
            HStack(spacing: 8) {
                Image(systemName: sesion.objetivo.isEmpty ? "target" : "flag.fill")
                    .font(.system(size: 10))
                    // Ambas ramas tienen que ser del mismo tipo: `.secondary` es
                    // HierarchicalShapeStyle y `.orange` es Color, y Swift no los unifica.
                    .foregroundStyle(sesion.objetivo.isEmpty
                                     ? Color.secondary : Color.orange)

                VStack(alignment: .leading, spacing: 0) {
                    Text(sesion.objetivo.isEmpty ? "Sin objetivo — toca para escribirlo"
                                                 : sesion.objetivo)
                        .font(.caption.weight(.semibold))
                        .lineLimit(1)
                    if !sesion.siguientePaso.isEmpty {
                        Text(sesion.siguientePaso)
                            .font(.system(size: 10))
                            .foregroundStyle(.secondary)
                            .lineLimit(1)
                    }
                }

                Spacer(minLength: 4)

                if let ultimo = sesion.cinta.first, !ultimo.resultado.isEmpty {
                    Text(ultimo.resultado)
                        .font(.system(size: 11, design: .monospaced).weight(.semibold))
                        .lineLimit(1)
                        .padding(.horizontal, 6).padding(.vertical, 2)
                        .background(Color.white.opacity(0.10), in: RoundedRectangle(cornerRadius: 5))
                        // Arrastrar el último resultado hasta la hoja de OneNote
                        // sin abrir nada más.
                        .onDrag { NSItemProvider(object: ultimo.resultado as NSString) }
                }
            }
            .padding(.horizontal, 12)
            .frame(height: 34)
            .contentShape(Rectangle())
        }
        .buttonStyle(.plain)
        .foregroundStyle(.white)
        .background(Color(white: 0.13))
    }

    // MARK: - Calculadora

    private func zonaCalculadora(disponible: CGSize) -> some View {
        let disposicion = modelo.disposicion
        let marco = encajar(en: disponible, proporcion: disposicion.proporcion)

        return ZStack(alignment: .topLeading) {
            if let caratula = UIImage.caratula(disposicion.caratula) {
                Image(uiImage: caratula)
                    .resizable()
                    .interpolation(.high)
                    .frame(width: marco.width, height: marco.height)
            }

            // Los indicadores van justo encima de la matriz, dentro del cristal,
            // como en la calculadora física. La carátula extraída no los trae
            // dibujados porque allí sólo se encienden.
            barraIndicadores
                .frame(width: disposicion.areaPantalla.width * marco.width,
                       alignment: .leading)
                .offset(x: disposicion.areaPantalla.minX * marco.width,
                        y: max(0, disposicion.areaPantalla.minY * marco.height - 16))

            PantallaLCD(pixeles: emulador.pixeles,
                        ancho: emulador.ancho, alto: emulador.alto)
                .frame(width: disposicion.areaPantalla.width * marco.width,
                       height: disposicion.areaPantalla.height * marco.height)
                .offset(x: disposicion.areaPantalla.minX * marco.width,
                        y: disposicion.areaPantalla.minY * marco.height)

            ForEach(disposicion.teclas, id: \.codigo) { tecla in
                zonaTactil(tecla: tecla, marco: marco)
            }

            if !guiaTeclado.isEmpty {
                capaGuia(disposicion: disposicion, marco: marco)
            }

            if mostrarDiagnostico {
                capaDiagnostico(disposicion: disposicion, marco: marco)
            }
        }
        .frame(width: marco.width, height: marco.height)
        .frame(maxWidth: .infinity, maxHeight: .infinity)
    }

    private var barraIndicadores: some View {
        let tabla = modelo.hardwareId == 4 ? IndicadorEstado.classWiz : IndicadorEstado.esPlus
        let activos = IndicadorEstado.activos(en: emulador.indicadores, tabla: tabla)
        return HStack(spacing: 5) {
            ForEach(activos, id: \.etiqueta) { indicador in
                Text(indicador.etiqueta)
                    .font(.system(size: 9,
                                  weight: indicador.destacado ? .heavy : .regular,
                                  design: .monospaced))
                    .foregroundStyle(indicador.destacado
                                     ? Color(red: 0.85, green: 0.45, blue: 0.1)
                                     : Color(white: 0.25))
            }
        }
    }

    private func zonaTactil(tecla: TeclaFisica, marco: CGSize) -> some View {
        let pulsada = emulador.teclaPulsada == tecla.codigo

        // Lo que se DIBUJA es exactamente la tecla de la carátula. Lo que se puede
        // TOCAR es algo mayor, porque en un cristal no hay relieve que guíe el dedo.
        // Agrandar el dibujo haría la carátula menos parecida a la de verdad; agrandar
        // la zona sensible no se ve y es lo que quita los fallos.
        let dibujada = CGRect(x: tecla.area.minX * marco.width,
                              y: tecla.area.minY * marco.height,
                              width: tecla.area.width * marco.width,
                              height: tecla.area.height * marco.height)
        let sensible = Ergonomia.zonaSensible(dibujada,
                                              extra: errores.margenExtra(tecla.codigo))

        return Rectangle()
            .fill(Color.black.opacity(pulsada ? 0.28 : 0.001))
            .frame(width: dibujada.width, height: dibujada.height)
            .offset(x: dibujada.minX, y: dibujada.minY)
            .contentShape(
                Rectangle()
                    .size(width: sensible.width, height: sensible.height)
                    .offset(x: (dibujada.width - sensible.width) / 2,
                            y: (dibujada.height - sensible.height) / 2)
            )
            .gesture(
                DragGesture(minimumDistance: 0)
                    .onChanged { _ in
                        guard emulador.teclaPulsada != tecla.codigo else { return }
                        // Un segundo toque en la misma tecla a los pocos milisegundos
                        // es el dedo rebotando, no una pulsación.
                        if Ergonomia.esRebote(codigo: tecla.codigo, anterior: ultimoToque,
                                              ahora: CACurrentMediaTime()) { return }
                        ultimoToque = (tecla.codigo, CACurrentMediaTime())

                        // Si estabas siguiendo una guía y ya pulsaste esa tecla,
                        // sobra: se quita sola sin que haya que cerrarla.
                        if guiaTeclado.first == tecla.nombre { guiaTeclado.removeFirst() }
                        emulador.pulsar(tecla.codigo)
                        sesion.registrar(tecla: tecla.nombre)
                        registro.registrar(tecla.nombre)
                        atencion.registrarTecla()
                        errores.registrar(tecla.codigo, codigoDel: codigoDel)
                        if tecla.nombre == "MODE" { indicadores.registrarEntradaASetup() }
                        if hapticaActiva { vibrar(por: tecla.nombre) }
                    }
                    .onEnded { _ in
                        emulador.soltar()
                        if tecla.nombre == "IGUAL" { cerrarOperacion() }
                    }
            )
    }

    /// Código de `DEL` en este modelo, para saber cuándo se borró lo recién escrito.
    private var codigoDel: UInt8 {
        modelo.disposicion.teclas.first { $0.nombre == "DEL" }?.codigo ?? 0xFF
    }

    /// Las teclas que cambian el rumbo se notan distinto en el dedo.
    ///
    /// `=` cierra un cálculo, `AC` se lo lleva por delante y SHIFT/ALPHA cambian lo
    /// que hará la siguiente tecla: son las tres cosas que conviene saber sin tener
    /// que mirar la pantalla.
    private func vibrar(por nombre: String) {
        let estilo: UIImpactFeedbackGenerator.FeedbackStyle
        switch nombre {
        case "IGUAL": estilo = .heavy
        case "AC", "DEL": estilo = .soft
        case "SHIFT", "ALPHA": estilo = .light
        default: estilo = .rigid
        }
        UIImpactFeedbackGenerator(style: estilo).impactOccurred()
    }

    /// Tras pulsar `=` hay que dar tiempo al firmware a calcular antes de leer la
    /// pantalla; el resultado no aparece en el mismo fotograma.
    private func cerrarOperacion() {
        DispatchQueue.main.asyncAfter(deadline: .now() + 1.2) {
            let resultado = emulador.lineasPantalla.last(where: { !$0.isEmpty }) ?? ""
            sesion.cerrar(resultado: resultado, modo: modoActual,
                          pixeles: emulador.pixeles,
                          ancho: emulador.ancho, alto: emulador.alto)
        }
    }

    private var modoActual: String {
        let tabla = modelo.hardwareId == 4 ? IndicadorEstado.classWiz : IndicadorEstado.esPlus
        return IndicadorEstado.activos(en: emulador.indicadores, tabla: tabla)
            .filter(\.destacado)
            .map(\.etiqueta)
            .joined(separator: " · ")
    }

    /// Marca las teclas de un procedimiento sobre la carátula, numeradas en el orden
    /// en que hay que pulsarlas. No las pulsa: las señala.
    private func capaGuia(disposicion: DisposicionCalculadora, marco: CGSize) -> some View {
        ZStack(alignment: .topLeading) {
            ForEach(Array(guiaTeclado.enumerated()), id: \.offset) { indice, nombre in
                if let tecla = disposicion.teclas.first(where: { $0.nombre == nombre }) {
                    RoundedRectangle(cornerRadius: 4)
                        .strokeBorder(Color.orange, lineWidth: 3)
                        .background(RoundedRectangle(cornerRadius: 4)
                            .fill(Color.orange.opacity(0.22)))
                        .frame(width: tecla.area.width * marco.width,
                               height: tecla.area.height * marco.height)
                        .overlay(alignment: .topLeading) {
                            Text("\(indice + 1)")
                                .font(.system(size: 11, weight: .bold))
                                .foregroundStyle(.white)
                                .padding(3)
                                .background(Circle().fill(Color.orange))
                                .offset(x: -6, y: -6)
                        }
                        .offset(x: tecla.area.minX * marco.width,
                                y: tecla.area.minY * marco.height)
                }
            }
        }
        .allowsHitTesting(false)
        .transition(.opacity)
    }

    // MARK: - Diagnóstico

    /// Superpone las zonas táctiles y el marco del LCD. Sirve para corregir
    /// coordenadas a partir de una captura, en vez de a ojo.
    private func capaDiagnostico(disposicion: DisposicionCalculadora, marco: CGSize) -> some View {
        ZStack(alignment: .topLeading) {
            Rectangle()
                .strokeBorder(Color.green, lineWidth: 2)
                .frame(width: disposicion.areaPantalla.width * marco.width,
                       height: disposicion.areaPantalla.height * marco.height)
                .offset(x: disposicion.areaPantalla.minX * marco.width,
                        y: disposicion.areaPantalla.minY * marco.height)

            ForEach(disposicion.teclas, id: \.codigo) { tecla in
                Rectangle()
                    .strokeBorder(Color.red.opacity(0.9), lineWidth: 1)
                    .frame(width: tecla.area.width * marco.width,
                           height: tecla.area.height * marco.height)
                    .offset(x: tecla.area.minX * marco.width,
                            y: tecla.area.minY * marco.height)
            }
        }
        .allowsHitTesting(false)
    }

    // MARK: - Avisos

    @ViewBuilder private var avisoRecordatorio: some View {
        if let recordatorio = sesion.recordatorio {
            VStack(alignment: .leading, spacing: 4) {
                Text("Ibas por aquí · \(textoAusencia(recordatorio.ausencia)) fuera")
                    .font(.caption.weight(.semibold))
                    .foregroundStyle(.secondary)
                if !recordatorio.objetivo.isEmpty {
                    Text(recordatorio.objetivo).font(.callout.weight(.medium))
                }
                if !recordatorio.siguientePaso.isEmpty {
                    Label(recordatorio.siguientePaso, systemImage: "arrow.turn.down.right")
                        .font(.footnote)
                        .foregroundStyle(.secondary)
                }
                if let ultimo = recordatorio.ultimo {
                    Text("\(ultimo.teclas)  =  \(ultimo.resultado)")
                        .font(.system(.callout, design: .monospaced))
                }
            }
            .padding(12)
            .frame(maxWidth: 420, alignment: .leading)
            .background(.ultraThinMaterial, in: RoundedRectangle(cornerRadius: 14))
            .padding(.top, 8)
            .transition(.move(edge: .top).combined(with: .opacity))
            .onTapGesture { sesion.recordatorio = nil }
            .task {
                try? await Task.sleep(nanoseconds: 8_000_000_000)
                sesion.recordatorio = nil
            }
        }
    }

    /// Al abrir la app la calculadora está recién encendida, aunque la libreta de
    /// al lado siga como estaba. Devolverla a su sitio cuesta segundos, así que se
    /// ofrece con el precio delante en vez de imponer la espera al arrancar.
    @ViewBuilder private var ofertaReanudar: some View {
        if let punto = sesion.reanudacion, emulador.pasosDeshacer == 0,
           !punto.teclas.isEmpty, !emulador.rebobinando {
            HStack(spacing: 12) {
                VStack(alignment: .leading, spacing: 2) {
                    Text("Dejaste la calculadora a medias")
                        .font(.footnote.weight(.semibold))
                    Text("Recuperarla tal cual tarda "
                         + "\(max(1, Int(punto.segundosEstimados.rounded()))) s")
                        .font(.caption2)
                        .foregroundStyle(.secondary)
                }
                Spacer(minLength: 8)
                Button("Recuperar") {
                    emulador.restaurar(secuencia: punto.teclas, estado: punto.estado)
                    sesion.descartarReanudacion()
                }
                .buttonStyle(.borderedProminent)
                .controlSize(.small)
                Button {
                    sesion.descartarReanudacion()
                } label: {
                    Image(systemName: "xmark")
                }
                .buttonStyle(.plain)
                .foregroundStyle(.secondary)
            }
            .padding(12)
            .frame(maxWidth: 460)
            .background(.ultraThinMaterial, in: RoundedRectangle(cornerRadius: 14))
            .padding(.bottom, 16)
            .transition(.move(edge: .bottom).combined(with: .opacity))
        }
    }

    private func textoAusencia(_ segundos: TimeInterval) -> String {
        let minutos = Int(segundos / 60)
        if minutos < 1 { return "un momento" }
        if minutos < 60 { return "\(minutos) min" }
        return "\(minutos / 60) h \(minutos % 60) min"
    }

    /// Mientras rebobina no se puede teclear: el emulador está siendo reproducido
    /// desde cero en otro hilo. Con sesiones largas eso son segundos, así que hay
    /// que decir que está pasando algo.
    @ViewBuilder private var avisoRebobinando: some View {
        // Un envío animado también bloquea el teclado, pero ahí tapar la pantalla
        // sería absurdo: lo que se quiere ver es precisamente la carátula.
        if emulador.rebobinando && !emulador.envioAnimado {
            ZStack {
                Color.black.opacity(0.45).ignoresSafeArea()
                VStack(spacing: 12) {
                    ProgressView(value: emulador.progresoRebobinado)
                        .frame(width: 190)
                    Text("Rebobinando la calculadora…")
                        .font(.footnote)
                        .foregroundStyle(.secondary)
                }
                .padding(22)
                .background(.ultraThinMaterial, in: RoundedRectangle(cornerRadius: 16))
            }
            .transition(.opacity)
        }
    }

    /// Mientras teclea sola, una barra abajo que no tapa nada y se puede cortar.
    ///
    /// Nada de diálogo modal: interrumpir con un pop-up mientras estás mirando dónde
    /// cae cada tecla es justo lo contrario de lo que hace falta.
    @ViewBuilder private var avisoEnvio: some View {
        if emulador.envioAnimado {
            HStack(spacing: 12) {
                ProgressView(value: emulador.progresoRebobinado)
                    .frame(width: 120)
                Text("Tecleando…")
                    .font(.footnote)
                    .foregroundStyle(.secondary)
                Button("Parar") { emulador.cancelarEnvio() }
                    .font(.footnote.weight(.semibold))
                    .buttonStyle(.borderless)
            }
            .padding(.horizontal, 16)
            .padding(.vertical, 10)
            .background(.ultraThinMaterial, in: Capsule())
            .padding(.bottom, 18)
            .transition(.move(edge: .bottom).combined(with: .opacity))
        }
    }

    /// Mientras hay teclas señaladas, una barra que dice qué se está enseñando.
    @ViewBuilder private var avisoGuia: some View {
        if !guiaTeclado.isEmpty {
            HStack(spacing: 12) {
                Image(systemName: "hand.point.up.left.fill")
                    .foregroundStyle(.orange)
                Text("Pulsa las teclas marcadas")
                    .font(.footnote)
                Button("Quitar") { guiaTeclado = [] }
                    .font(.footnote.weight(.semibold))
                    .buttonStyle(.borderless)
            }
            .padding(.horizontal, 16)
            .padding(.vertical, 10)
            .background(.ultraThinMaterial, in: Capsule())
            .padding(.bottom, 18)
            .transition(.move(edge: .bottom).combined(with: .opacity))
        }
    }

    @ViewBuilder private var avisoBreve: some View {
        if let aviso = avisoRapido {
            Text(aviso)
                .font(.footnote.weight(.medium))
                .padding(.horizontal, 14).padding(.vertical, 8)
                .background(.ultraThinMaterial, in: Capsule())
                .transition(.opacity)
        }
    }

    @ViewBuilder private var avisoSinRom: some View {
        if !emulador.cargada {
            Text("No se encontró la ROM \(modelo.recursoRom).bin en el paquete.")
                .font(.callout)
                .padding()
                .background(.thinMaterial, in: RoundedRectangle(cornerRadius: 12))
        }
    }

    // MARK: - Barra de herramientas

    @ToolbarContentBuilder private var barraHerramientas: some ToolbarContent {
        ToolbarItemGroup(placement: .navigationBarTrailing) {
            Button {
                sesion.anotarPunto(nota: sesion.objetivo, teclas: emulador.historial,
                                   estado: emulador.guardarEstado())
                avisar("Punto guardado")
            } label: {
                Image(systemName: "pin.fill")
            }
            .disabled(emulador.rebobinando)

            // Tocar deshace una tecla; mantener abre el resto del rebobinado.
            Menu {
                Button("Deshacer 5 teclas") { emulador.rebobinar(pasos: 5) }
                    .disabled(emulador.pasosDeshacer < 5)
                Button("Deshacer 20 teclas") { emulador.rebobinar(pasos: 20) }
                    .disabled(emulador.pasosDeshacer < 20)
                if let pasos = pasosHastaAntesDelUltimoAC {
                    Button("Volver a antes del último AC") {
                        emulador.rebobinar(pasos: pasos)
                    }
                }
                Divider()
                Button("Volver al encendido", role: .destructive) {
                    emulador.rebobinar(pasos: emulador.pasosDeshacer)
                }
            } label: {
                Image(systemName: "arrow.uturn.backward")
            } primaryAction: {
                emulador.deshacer()
                sesion.deshacerUltimaTecla()
            }
            .disabled(emulador.pasosDeshacer == 0 || emulador.rebobinando)

            Button { mostrarCinta = true } label: {
                Image(systemName: "list.bullet.rectangle")
            }

            Menu {
                Button {
                    mostrarProcedimientos = true
                } label: {
                    Label("¿Cómo hago…?", systemImage: "questionmark.circle")
                }
                Button {
                    mostrarEnviar = true
                } label: {
                    Label("Pegar una expresión", systemImage: "doc.on.clipboard")
                }
                Button {
                    mostrarFormulas = true
                } label: {
                    Label("Fórmulas", systemImage: "function")
                }
                Divider()
                Button {
                    mostrarRebobinado = true
                } label: {
                    Label("Rebobinar", systemImage: "backward.end.alt")
                }
                Button {
                    mostrarMePerdi = true
                } label: {
                    Label("Me perdí", systemImage: "questionmark.bubble")
                }
                Button {
                    mostrarPlano = true
                } label: {
                    Label("Plano complejo", systemImage: "chart.dots.scatter")
                }
                Button {
                    mostrarGrafica = true
                } label: {
                    Label("Graficar", systemImage: "waveform.path.ecg")
                }
                Button {
                    mostrarArbol = true
                } label: {
                    Label("Intentos", systemImage: "arrow.triangle.branch")
                }
                Button {
                    mostrarExportar = true
                } label: {
                    Label("Exportar", systemImage: "square.and.arrow.up")
                }
                Divider()
                Toggle("Vibración al pulsar", isOn: $hapticaActiva)
                Toggle("Modo diagnóstico", isOn: $mostrarDiagnostico)
                Button {
                    mostrarFicha = true
                } label: {
                    Label("Qué máquina es esta", systemImage: "checkmark.seal")
                }
                Button {
                    mostrarMaterias = true
                } label: {
                    Label("Materias", systemImage: "books.vertical")
                }
                Button {
                    mostrarAprender = true
                } label: {
                    Label("Aprender / examen", systemImage: "graduationcap")
                }
                Toggle("Barra de teclas rápidas", isOn: $preferencias.barraEsenciales)
                Toggle("Ver teclas pulsadas", isOn: $registro.visible)
                // Retener el AC un instante da margen a cancelarlo. La tecla llega
                // a la Casio igual, solo que un momento después.
                Picker("Retardo del AC", selection: $retardoBorrado) {
                    Text("Sin retardo").tag(0.0)
                    Text("1 segundo").tag(1.0)
                    Text("2 segundos").tag(2.0)
                }
                Button {
                    sesion.nuevoEjercicio(teclas: emulador.historial)
                    avisar("Ejercicio anterior guardado como punto")
                } label: {
                    Label("Nuevo ejercicio", systemImage: "square.and.pencil")
                }
            } label: {
                Image(systemName: "ellipsis.circle")
            }
        }
    }

    /// Atajos del teclado físico.
    ///
    /// Salen del mismo catálogo que la paleta `⌘K`, a propósito: mantener dos listas
    /// paralelas es exactamente cómo acaban diciendo cosas distintas.
    @ViewBuilder private var atajos: some View {
        ZStack {
            Button("") { mostrarPaleta = true }
                .keyboardShortcut("k", modifiers: .command)
                .hidden()
            // `Character(...)` revienta con una cadena vacía, así que se filtran
            // antes los comandos sin atajo en vez de confiar en que siempre lo tengan.
            ForEach(Comandos.todos.filter { $0.atajo.count == 1 }) { comando in
                Button("") { ejecutar(comando.clave) }
                    .keyboardShortcut(
                        KeyEquivalent(Character(comando.atajo.lowercased())),
                        modifiers: .command)
                    .hidden()
            }
        }
    }

    /// Ejecuta un comando por su clave. Es el único sitio donde se decide qué hace
    /// cada uno, así que el menú, el atajo y la paleta no pueden discrepar.
    private func ejecutar(_ clave: String) {
        switch clave {
        case "grafica": mostrarGrafica = true
        case "formulas": mostrarFormulas = true
        case "exportar": mostrarExportar = true
        case "rebobinar": mostrarRebobinado = true
        case "punto":
            sesion.anotarPunto(nota: sesion.objetivo, teclas: emulador.historial,
                               estado: emulador.guardarEstado())
            avisar("Punto guardado")
        case "meperdi": mostrarMePerdi = true
        case "cinta": mostrarCinta = true
        case "procedimientos": mostrarProcedimientos = true
        case "plano": mostrarPlano = true
        case "intentos": mostrarArbol = true
        case "diagnostico": mostrarFicha = true
        case "nuevo":
            sesion.nuevoEjercicio(teclas: emulador.historial)
            avisar("Ejercicio anterior guardado como punto")
        default: break
        }
    }

    /// Aviso de que el modo angular cambió sin pasar por el menú.
    ///
    /// Es una barra pasiva: no bloquea nada y se quita al tocarla. Un modal aquí
    /// interrumpiría justo cuando estás a mitad de escribir.
    @ViewBuilder private var avisoCambioDeModo: some View {
        if let texto = indicadores.cambioSospechoso {
            HStack(spacing: 8) {
                Image(systemName: "exclamationmark.triangle.fill")
                Text(texto).font(.footnote.weight(.medium))
                Text("¿A propósito?").font(.footnote).foregroundStyle(.secondary)
            }
            .padding(.horizontal, 14).padding(.vertical, 8)
            .background(.ultraThinMaterial, in: Capsule())
            .onTapGesture { indicadores.descartarAviso() }
            .transition(.move(edge: .top).combined(with: .opacity))
        }
    }

    /// Cuántas teclas hay que rebobinar para deshacer el último `AC`.
    ///
    /// Es el accidente que más cuesta: un `AC` de más se lleva por delante el
    /// cálculo entero y en la calculadora física no hay vuelta atrás.
    private var pasosHastaAntesDelUltimoAC: Int? {
        guard let ac = modelo.disposicion.teclas.first(where: { $0.nombre == "AC" })?.codigo,
              let indice = emulador.historial.lastIndex(of: ac) else { return nil }
        return emulador.historial.count - indice
    }

    private func avisar(_ mensaje: String) {
        avisoRapido = mensaje
        DispatchQueue.main.asyncAfter(deadline: .now() + 1.6) {
            if avisoRapido == mensaje { avisoRapido = nil }
        }
    }

    // MARK: - Apoyo

    /// Encaja la carátula dentro del espacio disponible sin deformarla.
    private func encajar(en disponible: CGSize, proporcion: CGFloat) -> CGSize {
        let anchoPorAltura = disponible.height * proporcion
        if anchoPorAltura <= disponible.width {
            return CGSize(width: anchoPorAltura, height: disponible.height)
        }
        return CGSize(width: disponible.width, height: disponible.width / proporcion)
    }
}
