import AVFoundation
import UIKit

/// La pantalla de ajustes de la grabación.
///
/// Cada control cambia un número y vuelve a preguntarle al núcleo qué sale de
/// ahí: tamaño, kbps, megabytes por minuto y cuánto cabe en el iPad. El resumen
/// de abajo se refresca en cada toque, así que se ve el coste **antes** de
/// grabar dos horas de clase, no después.
final class PanelGrabacion: UIViewController {

    var ajustes: TintaAjustesGrabacion
    var pantalla: AjustesPantalla
    var seleccionMicrofono: SeleccionMicrofono?
    private let capacidades: TintaCapacidades

    var alAceptar: ((TintaAjustesGrabacion, AjustesPantalla,
                     SeleccionMicrofono?) -> Void)?

    private let pila = UIStackView()
    private let resumen = UILabel()
    private let aviso = UILabel()
    private var botonMicrofono = UIButton(type: .system)
    private var fuentes: [Microfonos.Fuente] = []
    private var controlesVideo: [UIView] = []
    private weak var controlAudioApp: UISwitch?
    private weak var controlCanales: UISegmentedControl?
    private weak var controlBitrateAudioAutomatico: UISwitch?
    /// Los deslizadores, por etiqueta, para poder encenderlos y apagarlos desde
    /// su interruptor de «automático».
    private var deslizadores: [Int: UISlider] = [:]

    private let opcionesFps = [15, 24, 30, 60, 120]

    init(ajustes: TintaAjustesGrabacion, pantalla: AjustesPantalla,
         seleccionMicrofono: SeleccionMicrofono? = nil,
         capacidades: TintaCapacidades) {
        var compatibles = ajustes.ajustado(a: capacidades)
        if compatibles.modoElegido == .pantallaConAudio,
           compatibles.canalesElegidos == .espacial {
            compatibles.canalesElegidos = .estereo
        }
        if compatibles.canalesElegidos == .espacial,
           !Microfonos.seleccionPermiteAudioEspacial(seleccionMicrofono) {
            compatibles.canalesElegidos = .estereo
        }
        self.ajustes = compatibles
        self.pantalla = pantalla
        self.seleccionMicrofono = seleccionMicrofono
        self.capacidades = capacidades
        super.init(nibName: nil, bundle: nil)
    }

    required init?(coder: NSCoder) { fatalError("sin storyboard") }

    // MARK: - Montaje

    override func viewDidLoad() {
        super.viewDidLoad()
        view.backgroundColor = .systemGroupedBackground
        title = "Grabación de clase"

        navigationItem.rightBarButtonItem = UIBarButtonItem(
            barButtonSystemItem: .done, target: self, action: #selector(aceptar))
        navigationItem.leftBarButtonItem = UIBarButtonItem(
            barButtonSystemItem: .cancel, target: self, action: #selector(cancelar))

        fuentes = Microfonos.disponibles()

        let scroll = UIScrollView()
        scroll.translatesAutoresizingMaskIntoConstraints = false
        view.addSubview(scroll)

        pila.axis = .vertical
        pila.spacing = 14
        pila.isLayoutMarginsRelativeArrangement = true
        pila.layoutMargins = UIEdgeInsets(top: 16, left: 20, bottom: 24, right: 20)
        pila.translatesAutoresizingMaskIntoConstraints = false
        scroll.addSubview(pila)

        NSLayoutConstraint.activate([
            scroll.topAnchor.constraint(equalTo: view.safeAreaLayoutGuide.topAnchor),
            scroll.leadingAnchor.constraint(equalTo: view.leadingAnchor),
            scroll.trailingAnchor.constraint(equalTo: view.trailingAnchor),
            scroll.bottomAnchor.constraint(equalTo: view.bottomAnchor),

            pila.topAnchor.constraint(equalTo: scroll.contentLayoutGuide.topAnchor),
            pila.bottomAnchor.constraint(equalTo: scroll.contentLayoutGuide.bottomAnchor),
            pila.leadingAnchor.constraint(equalTo: scroll.contentLayoutGuide.leadingAnchor),
            pila.trailingAnchor.constraint(equalTo: scroll.contentLayoutGuide.trailingAnchor),
            pila.widthAnchor.constraint(equalTo: scroll.frameLayoutGuide.widthAnchor),
        ])

        construir()
        refrescar()
    }

    private func construir() {
        // --- Preajustes ---
        let preajustes = PreajusteGrabacion.todos
        let selectorPreajuste = UISegmentedControl(items: preajustes.map(\.nombre))
        selectorPreajuste.addTarget(self, action: #selector(elegirPreajuste(_:)),
                                    for: .valueChanged)
        pila.addArrangedSubview(titulo("Para qué es esta grabación"))
        pila.addArrangedSubview(selectorPreajuste)
        if let primero = preajustes.first {
            pila.addArrangedSubview(nota(primero.paraQue))
        }

        let modo = UISegmentedControl(items: ModoCaptura.allCases.map(\.nombre))
        modo.selectedSegmentIndex = Int(ajustes.modo)
        modo.addTarget(self, action: #selector(cambiarModoCaptura(_:)),
                       for: .valueChanged)
        pila.addArrangedSubview(titulo("Qué se guarda"))
        pila.addArrangedSubview(modo)
        pila.addArrangedSubview(nota(ajustes.modoElegido.explicacion))

        // --- Vídeo ---
        agregarVideo(titulo("Vídeo"))

        let resolucion = UISegmentedControl(items: ResolucionGrabacion.allCases.map(\.nombre))
        resolucion.selectedSegmentIndex = Int(ajustes.resolucion)
        resolucion.addTarget(self, action: #selector(cambiarResolucion(_:)), for: .valueChanged)
        agregarVideo(fila("Resolución", resolucion))

        let fps = UISegmentedControl(items: fpsDisponibles().map { "\($0)" })
        fps.selectedSegmentIndex = indiceFps()
        fps.addTarget(self, action: #selector(cambiarFps(_:)), for: .valueChanged)
        agregarVideo(fila("FPS", fps))

        let calidad = UISegmentedControl(items: CalidadVideo.allCases.map(\.nombre))
        calidad.selectedSegmentIndex = Int(ajustes.calidad)
        calidad.addTarget(self, action: #selector(cambiarCalidad(_:)), for: .valueChanged)
        agregarVideo(fila("Calidad", calidad))

        let codec = UISegmentedControl(items: CodecVideo.allCases.map(\.nombre))
        codec.selectedSegmentIndex = Int(ajustes.codec)
        codec.isEnabled = capacidades.hevc
        codec.addTarget(self, action: #selector(cambiarCodec(_:)), for: .valueChanged)
        agregarVideo(fila("Códec", codec))

        agregarVideo(deslizador(etiqueta: "Bitrate del vídeo", unidad: "kbps",
                                minimo: 500, maximo: 60000,
                                valor: ajustes.kbps_video, tag: Tag.kbpsVideo))
        agregarVideo(nota("En automático lo calcula el núcleo con el tamaño, los FPS y la calidad."))

        // --- Audio ---
        pila.addArrangedSubview(titulo("Audio"))

        let micro = UISwitch()
        micro.isOn = ajustes.microfono
        micro.tag = Tag.microfono
        micro.addTarget(self, action: #selector(cambiarInterruptor(_:)), for: .valueChanged)
        pila.addArrangedSubview(fila("Grabar el micrófono", micro))

        let audioApp = UISwitch()
        audioApp.isOn = ajustes.audio_de_la_app
        audioApp.tag = Tag.audioApp
        audioApp.addTarget(self, action: #selector(cambiarInterruptor(_:)), for: .valueChanged)
        controlAudioApp = audioApp
        pila.addArrangedSubview(fila("Grabar el sonido de la app", audioApp))

        botonMicrofono.showsMenuAsPrimaryAction = true
        botonMicrofono.changesSelectionAsPrimaryAction = false
        botonMicrofono.menu = menuDeMicrofonos()
        pila.addArrangedSubview(fila("Micrófono", botonMicrofono))
        pila.addArrangedSubview(nota(
            "Los 4–5 micrófonos del iPad no se pueden grabar por separado: iOS no lo "
            + "permite. Lo que sí se elige es de qué lado capta y con qué forma, que "
            + "es un micrófono virtual apuntado a donde uno quiera."))

        let patron = UISegmentedControl(items: PatronPolar.allCases.map(\.nombre))
        patron.selectedSegmentIndex = Int(ajustes.patron_polar)
        patron.addTarget(self, action: #selector(cambiarPatron(_:)), for: .valueChanged)
        pila.addArrangedSubview(fila("Forma de captación", patron))

        let canales = UISegmentedControl(items: ModoCanales.allCases.map(\.nombre))
        canales.selectedSegmentIndex = Int(ajustes.canales)
        canales.addTarget(self, action: #selector(cambiarCanales(_:)), for: .valueChanged)
        canales.setEnabled(capacidades.estereo, forSegmentAt: Int(ModoCanales.estereo.rawValue))
        controlCanales = canales
        pila.addArrangedSubview(fila("Canales", canales))
        pila.addArrangedSubview(nota("La ruta actual reporta hasta \(capacidades.canales_entrada_max) canal(es) de entrada."))
        if !capacidades.espacial {
            pila.addArrangedSubview(nota(
                "Este iPad o su versión de iPadOS no expone captura FOA real; "
                + "3D queda desactivado y no se sustituye por un estéreo rotulado como espacial."))
        }

        let frecuencia = UISegmentedControl(items: ["44,1 kHz", "48 kHz"])
        frecuencia.selectedSegmentIndex = ajustes.frecuencia_hz == 44100 ? 0 : 1
        frecuencia.addTarget(self, action: #selector(cambiarFrecuencia(_:)), for: .valueChanged)
        pila.addArrangedSubview(fila("Frecuencia", frecuencia))

        pila.addArrangedSubview(
            deslizador(etiqueta: "Bitrate del audio", unidad: "kbps",
                       minimo: 24, maximo: 320,
                       valor: ajustes.kbps_audio, tag: Tag.kbpsAudio))

        // --- Pantalla ---
        pila.addArrangedSubview(titulo("Pantalla"))

        let siempre = UISwitch()
        siempre.isOn = pantalla.siempreActiva
        siempre.tag = Tag.pantallaSiempre
        siempre.addTarget(self, action: #selector(cambiarInterruptor(_:)), for: .valueChanged)
        pila.addArrangedSubview(fila("Pantalla siempre activa", siempre))

        let alGrabar = UISwitch()
        alGrabar.isOn = pantalla.siempreActivaAlGrabar
        alGrabar.tag = Tag.pantallaAlGrabar
        alGrabar.addTarget(self, action: #selector(cambiarInterruptor(_:)), for: .valueChanged)
        pila.addArrangedSubview(fila("…al menos mientras graba", alGrabar))

        let alEscribir = UISwitch()
        alEscribir.isOn = pantalla.siempreActivaAlEscribir
        alEscribir.tag = Tag.pantallaAlEscribir
        alEscribir.addTarget(self, action: #selector(cambiarInterruptor(_:)), for: .valueChanged)
        pila.addArrangedSubview(fila("…y mientras se escribe", alEscribir))

        let puntero = UISwitch()
        puntero.isOn = ajustes.mostrar_puntero
        puntero.tag = Tag.mostrarPuntero
        puntero.addTarget(self, action: #selector(cambiarInterruptor(_:)), for: .valueChanged)
        pila.addArrangedSubview(fila("Ver el puntero en el vídeo", puntero))

        // --- Resumen ---
        resumen.numberOfLines = 0
        resumen.font = .monospacedSystemFont(ofSize: 13, weight: .regular)
        resumen.textColor = .secondaryLabel
        pila.addArrangedSubview(titulo("Lo que va a salir"))
        pila.addArrangedSubview(resumen)

        aviso.numberOfLines = 0
        aviso.font = .preferredFont(forTextStyle: .footnote)
        aviso.textColor = .systemOrange
        pila.addArrangedSubview(aviso)
    }

    // MARK: - Cambios

    private enum Tag {
        static let kbpsVideo = 100
        static let kbpsAudio = 101
        static let microfono = 200
        static let audioApp = 201
        static let pantallaSiempre = 300
        static let pantallaAlGrabar = 301
        static let pantallaAlEscribir = 302
        static let mostrarPuntero = 303
    }

    @objc private func elegirPreajuste(_ control: UISegmentedControl) {
        let lista = PreajusteGrabacion.todos
        guard control.selectedSegmentIndex >= 0,
              control.selectedSegmentIndex < lista.count else { return }
        ajustes = lista[control.selectedSegmentIndex].ajustes.ajustado(a: capacidades)
        normalizarAudioParaModoYFuente()
        // Un preajuste cambia media pantalla: se reconstruye entera en vez de ir
        // control por control, que es donde se olvida uno.
        pila.arrangedSubviews.forEach { $0.removeFromSuperview() }
        deslizadores.removeAll()
        controlesVideo.removeAll()
        construir()
        refrescar()
    }

    @objc private func cambiarModoCaptura(_ c: UISegmentedControl) {
        ajustes.modo = Int32(c.selectedSegmentIndex)
        normalizarAudioParaModoYFuente()
        if ajustes.modoElegido != .pantallaConAudio {
            // AVAudioRecorder capta el micrófono. El audio interno de otras apps
            // solo llega por ReplayKit, que pertenece al modo vídeo.
            ajustes.audio_de_la_app = false
        }
        pila.arrangedSubviews.forEach { $0.removeFromSuperview() }
        deslizadores.removeAll()
        controlesVideo.removeAll()
        construir()
        refrescar()
    }

    @objc private func cambiarResolucion(_ c: UISegmentedControl) {
        ajustes.resolucion = Int32(c.selectedSegmentIndex)
        refrescar()
    }

    @objc private func cambiarFps(_ c: UISegmentedControl) {
        let lista = fpsDisponibles()
        guard c.selectedSegmentIndex >= 0, c.selectedSegmentIndex < lista.count else { return }
        ajustes.fps = Int32(lista[c.selectedSegmentIndex])
        refrescar()
    }

    @objc private func cambiarCalidad(_ c: UISegmentedControl) {
        ajustes.calidad = Int32(c.selectedSegmentIndex)
        refrescar()
    }

    @objc private func cambiarCodec(_ c: UISegmentedControl) {
        ajustes.codec = Int32(c.selectedSegmentIndex)
        refrescar()
    }

    @objc private func cambiarPatron(_ c: UISegmentedControl) {
        ajustes.patron_polar = Int32(c.selectedSegmentIndex)
        refrescar()
    }

    @objc private func cambiarCanales(_ c: UISegmentedControl) {
        ajustes.canales = Int32(c.selectedSegmentIndex)
        normalizarAudioParaModoYFuente()
        c.selectedSegmentIndex = Int(ajustes.canales)
        refrescar()
    }

    @objc private func cambiarFrecuencia(_ c: UISegmentedControl) {
        ajustes.frecuencia_hz = c.selectedSegmentIndex == 0 ? 44100 : 48000
        refrescar()
    }

    @objc private func cambiarInterruptor(_ s: UISwitch) {
        switch s.tag {
        case Tag.microfono: ajustes.microfono = s.isOn
        case Tag.audioApp: ajustes.audio_de_la_app = s.isOn
        case Tag.mostrarPuntero: ajustes.mostrar_puntero = s.isOn
        case Tag.pantallaSiempre: pantalla.siempreActiva = s.isOn
        case Tag.pantallaAlGrabar: pantalla.siempreActivaAlGrabar = s.isOn
        case Tag.pantallaAlEscribir: pantalla.siempreActivaAlEscribir = s.isOn
        default: break
        }
        refrescar()
    }

    @objc private func cambiarDeslizador(_ s: UISlider) {
        let valor = Int32(s.value.rounded())
        switch s.tag {
        case Tag.kbpsVideo: ajustes.kbps_video = valor
        case Tag.kbpsAudio: ajustes.kbps_audio = valor
        default: break
        }
        refrescar()
    }

    @objc private func cambiarAutomatico(_ s: UISwitch) {
        // El 0 es el que significa «automático» para el núcleo. Al apagarlo se
        // parte del valor que el núcleo estaba usando, que es el que se ve en el
        // resumen: así el deslizador no salta al mínimo al tocarlo.
        var valor: Int32 = 0
        switch s.tag {
        case Tag.kbpsVideo:
            valor = s.isOn ? 0 : Int32(ajustes.kbpsVideo(capacidades))
            ajustes.kbps_video = valor
        case Tag.kbpsAudio:
            valor = s.isOn ? 0 : Int32(ajustes.kbpsAudio(capacidades))
            ajustes.kbps_audio = valor
        default: break
        }
        if let barra = deslizadores[s.tag] {
            barra.isEnabled = !s.isOn
            if valor > 0 {
                barra.value = min(max(Float(valor), barra.minimumValue), barra.maximumValue)
            }
        }
        refrescar()
    }

    @objc private func elegirMicrofono(_ indice: Int) {
        guard indice >= 0, indice < fuentes.count else { return }
        ajustes.posicion_microfono = fuentes[indice].posicion.rawValue
        seleccionMicrofono = fuentes[indice].seleccion
        normalizarAudioParaModoYFuente()
        botonMicrofono.menu = menuDeMicrofonos()
        refrescar()
    }

    private func elegirMicrofonoAutomatico() {
        seleccionMicrofono = nil
        ajustes.posicion_microfono = PosicionMicrofono.automatica.rawValue
        normalizarAudioParaModoYFuente()
        botonMicrofono.menu = menuDeMicrofonos()
        refrescar()
    }

    @objc private func aceptar() {
        alAceptar?(ajustes.ajustado(a: capacidades), pantalla, seleccionMicrofono)
        dismiss(animated: true)
    }

    @objc private func cancelar() { dismiss(animated: true) }

    // MARK: - Refresco

    private func refrescar() {
        let tamano = ajustes.tamano(capacidades)
        let kv = ajustes.kbpsVideo(capacidades)
        let ka = ajustes.kbpsAudio(capacidades)
        let esEspacial = ajustes.canalesElegidos == .espacial
        let permiteEspacial = capacidades.espacial
            && ajustes.modoElegido != .pantallaConAudio
            && Microfonos.seleccionPermiteAudioEspacial(seleccionMicrofono)
        controlCanales?.setEnabled(permiteEspacial,
                                  forSegmentAt: Int(ModoCanales.espacial.rawValue))
        controlCanales?.selectedSegmentIndex = Int(ajustes.canales)

        controlBitrateAudioAutomatico?.isEnabled = !esEspacial
        if let barra = deslizadores[Tag.kbpsAudio] {
            barra.isEnabled = !esEspacial && ajustes.kbps_audio > 0
        }

        let mb = esEspacial ? 0 : ajustes.megabytesPorMinuto(capacidades)
        let libres = megabytesLibres()
        let minutos = esEspacial
            ? .infinity
            : ajustes.minutosQueCaben(capacidades, megabytesLibres: libres)

        var lineas = ["modo        \(ajustes.modoElegido.nombre)"]
        if ajustes.modoElegido.incluyeVideo {
            lineas.append("tamaño      \(Int(tamano.width)) × \(Int(tamano.height))")
            lineas.append("vídeo       \(ajustes.fps) FPS · \(kv) kbps · \(ajustes.codecElegido.nombre)")
        }
        if esEspacial {
            lineas.append("audio       3D FOA (4 componentes + estéreo compatible)")
            lineas.append("codificación administrada por iPadOS · QuickTime")
            lineas.append("tamaño      se medirá sobre el archivo real")
        } else {
            lineas.append("audio       \(ajustes.canalesElegidos.nombre) · \(ka) kbps · \(ajustes.frecuencia_hz / 1000) kHz")
            lineas.append("tamaño/min  \(String(format: "%.1f", mb)) MB")
            lineas.append("caben       \(duracion(minutos)) con \(Int(libres / 1024)) GB libres")
        }
        resumen.text = lineas.joined(separator: "\n")

        var textos: [String] = []
        if let problema = ajustes.problema(capacidades) { textos.append(problema) }
        let microfonoSeleccionadoDisponible = seleccionMicrofono.map {
            Microfonos.fuenteExacta($0) != nil
        } ?? true
        if ajustes.microfono, !microfonoSeleccionadoDisponible {
            textos.append("El micrófono seleccionado ya no está conectado. Elija otra entrada o Automático.")
        }
        let corregido = ajustes.ajustado(a: capacidades)
        if corregido.canales != ajustes.canales {
            textos.append("Los canales se han ajustado a lo que da este iPad.")
        }
        if esEspacial {
            textos.append("En 3D, el deslizador de kbps AAC no aplica: AVFoundation administra las pistas FOA y estéreo de compatibilidad.")
        } else if minutos < 30 {
            textos.append("Con estos ajustes no cabe ni media hora de clase.")
        }
        aviso.text = textos.joined(separator: " ")
        aviso.isHidden = textos.isEmpty
        navigationItem.rightBarButtonItem?.isEnabled = !ajustes.microfono
            || microfonoSeleccionadoDisponible

        botonMicrofono.setTitle(nombreDelMicrofono(), for: .normal)
        controlesVideo.forEach { $0.isHidden = !ajustes.modoElegido.incluyeVideo }
        controlAudioApp?.isEnabled = ajustes.modoElegido == .pantallaConAudio
    }

    /// ReplayKit no entrega FOA y una entrada externa ignora
    /// `multichannelAudioMode`. Se corrige la selección antes de aceptarla para
    /// que una configuración persistida nunca produzca un archivo mal rotulado.
    private func normalizarAudioParaModoYFuente() {
        guard ajustes.canalesElegidos == .espacial else { return }
        if ajustes.modoElegido == .pantallaConAudio
            || !capacidades.espacial
            || !Microfonos.seleccionPermiteAudioEspacial(seleccionMicrofono) {
            ajustes.canalesElegidos = .estereo
        }
    }

    private func duracion(_ minutos: Double) -> String {
        guard minutos.isFinite, minutos > 0 else { return "nada" }
        let horas = Int(minutos) / 60
        let resto = Int(minutos) % 60
        return horas > 0 ? "\(horas) h \(resto) min" : "\(resto) min"
    }

    private func megabytesLibres() -> Double {
        let url = URL(fileURLWithPath: NSHomeDirectory())
        let valores = try? url.resourceValues(
            forKeys: [.volumeAvailableCapacityForImportantUsageKey])
        let bytes = valores?.volumeAvailableCapacityForImportantUsage ?? 0
        return Double(bytes) / 1_000_000.0
    }

    private func fpsDisponibles() -> [Int] {
        opcionesFps.filter { $0 <= Int(capacidades.fps_maximo) }
    }

    private func indiceFps() -> Int {
        let lista = fpsDisponibles()
        return lista.firstIndex(of: Int(ajustes.fps)) ?? max(0, lista.count - 1)
    }

    private func nombreDelMicrofono() -> String {
        if seleccionMicrofono == nil { return "Automático" }
        if let seleccionMicrofono,
           let fuente = Microfonos.fuenteExacta(seleccionMicrofono) {
            return fuente.descripcion
        }
        return "\(seleccionMicrofono?.nombre ?? ajustes.posicionElegida.nombre) — no disponible"
    }

    private func menuDeMicrofonos() -> UIMenu {
        let automatico = UIAction(title: "Automático",
                                  state: seleccionMicrofono == nil ? .on : .off) { [weak self] _ in
            self?.elegirMicrofonoAutomatico()
        }
        let acciones = fuentes.enumerated().map { indice, fuente in
            UIAction(title: fuente.descripcion,
                     state: fuente.seleccion == seleccionMicrofono ? .on : .off) { [weak self] _ in
                self?.elegirMicrofono(indice)
            }
        }
        var hijos = [automatico] + acciones
        if let seleccionMicrofono,
           Microfonos.fuenteExacta(seleccionMicrofono) == nil {
            hijos.append(UIAction(
                title: "No disponible: \(seleccionMicrofono.nombre)",
                attributes: [.disabled], state: .on) { _ in })
        }
        if acciones.isEmpty {
            hijos.append(UIAction(title: "No hay micrófonos",
                                  attributes: [.disabled]) { _ in })
        }
        return UIMenu(children: hijos)
    }

    private func agregarVideo(_ vista: UIView) {
        controlesVideo.append(vista)
        pila.addArrangedSubview(vista)
    }

    // MARK: - Piezas de la interfaz

    private func titulo(_ texto: String) -> UILabel {
        let etiqueta = UILabel()
        etiqueta.text = texto
        etiqueta.font = .preferredFont(forTextStyle: .headline)
        return etiqueta
    }

    private func nota(_ texto: String) -> UILabel {
        let etiqueta = UILabel()
        etiqueta.text = texto
        etiqueta.numberOfLines = 0
        etiqueta.font = .preferredFont(forTextStyle: .footnote)
        etiqueta.textColor = .secondaryLabel
        return etiqueta
    }

    private func fila(_ texto: String, _ control: UIView) -> UIView {
        let etiqueta = UILabel()
        etiqueta.text = texto
        etiqueta.font = .preferredFont(forTextStyle: .body)
        etiqueta.setContentHuggingPriority(.defaultLow, for: .horizontal)

        let fila = UIStackView(arrangedSubviews: [etiqueta, control])
        fila.axis = .horizontal
        fila.spacing = 12
        fila.alignment = .center
        return fila
    }

    /// Un deslizador con su interruptor de «automático» al lado. El 0 significa
    /// automático para el núcleo, así que el interruptor no es adorno: es lo que
    /// distingue «déjalo tú» de «yo mando».
    private func deslizador(etiqueta texto: String, unidad: String,
                            minimo: Float, maximo: Float,
                            valor: Int32, tag: Int) -> UIView {
        let automatico = UISwitch()
        automatico.isOn = valor == 0
        automatico.tag = tag
        automatico.addTarget(self, action: #selector(cambiarAutomatico(_:)), for: .valueChanged)
        if tag == Tag.kbpsAudio { controlBitrateAudioAutomatico = automatico }

        let barra = UISlider()
        barra.minimumValue = minimo
        barra.maximumValue = maximo
        barra.value = valor > 0 ? Float(valor) : minimo
        barra.isEnabled = valor > 0
        barra.tag = tag
        barra.addTarget(self, action: #selector(cambiarDeslizador(_:)), for: .valueChanged)
        deslizadores[tag] = barra

        let cabecera = fila("\(texto) (\(unidad))", automatico)
        let columna = UIStackView(arrangedSubviews: [cabecera, barra])
        columna.axis = .vertical
        columna.spacing = 6
        return columna
    }
}
