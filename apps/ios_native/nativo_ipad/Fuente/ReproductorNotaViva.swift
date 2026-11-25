import AVFoundation
import AVKit
import QuartzCore
import UIKit

/// Reproductor autocontenido de un paquete `.tintaviva`. Carga una copia de la
/// tinta en su propio lienzo, por lo que revisar una clase antigua no modifica
/// el cuaderno que está abierto para escribir.
final class ReproductorNotaViva: UIViewController {

    enum Fallo: LocalizedError {
        case manifiestoInvalido
        case tintaInvalida
        case audioInvalido

        var errorDescription: String? {
            switch self {
            case .manifiestoInvalido: return "El manifiesto de Nota Viva no es válido."
            case .tintaInvalida: return "La copia de tinta de Nota Viva no se pudo abrir."
            case .audioInvalido: return "La pista de audio de Nota Viva está vacía o dañada."
            }
        }
    }

    private let manifiestoURL: URL
    private let lienzo: LienzoCuadernU
    private lazy var navegador = VistaLienzoNavegable(lienzo: lienzo)
    private let botonPlay = UIButton(type: .system)
    private let botonRetroceso = UIButton(type: .system)
    private let botonAvance = UIButton(type: .system)
    private let progreso = UISlider()
    private let tiempo = UILabel()
    private let velocidad = UISegmentedControl(items: ["0,5×", "0,75×", "1×", "1,25×",
                                                 "1,5×", "1,75×", "2×", "3×"])
    private let botonMarcadores = UIButton(type: .system)
    private let botonTranscripcion = UIButton(type: .system)
    private let botonAudio = UIButton(type: .system)
    private let botonVideo = UIButton(type: .system)
    private let bucle = UISwitch()
    private let saltarSilencios = UISwitch()
    private let contextoActual = UILabel()

    private var manifiesto: ManifiestoNotaViva?
    private var audio: AVAudioPlayer?
    private var audioOriginalURL: URL?
    private var audioMejoradoURL: URL?
    private var audioActualURL: URL?
    private var videoClase: ReproductorVideoClasePiP?
    private var displayLink: CADisplayLink?
    private var reproduciendo = false
    private var inicioPared = 0.0
    private var inicioMedio = 0.0
    private let velocidades: [Double] = [0.5, 0.75, 1, 1.25, 1.5, 1.75, 2, 3]
    private var rangoBucle: ClosedRange<Double>?
    private var ultimoEventoEnfocado = -1
    private var analizandoSilencios = false
    private var advertenciasCarga: [String] = []
    private var usaCronologiaVectorial = false

    init(manifiestoURL: URL) {
        self.manifiestoURL = manifiestoURL
        self.lienzo = LienzoCuadernU(
            idPersistente: DocumentoPersistenteID())
        super.init(nibName: nil, bundle: nil)
    }

    required init?(coder: NSCoder) { fatalError("sin storyboard") }

    override func viewDidLoad() {
        super.viewDidLoad()
        title = "Nota Viva"
        view.backgroundColor = .systemBackground
        navigationItem.rightBarButtonItem = UIBarButtonItem(
            barButtonSystemItem: .done, target: self, action: #selector(cerrar))
        montarInterfaz()
        do {
            try cargarPaquete()
            mostrar(segundo: 0)
            let advertencias = (manifiesto?.advertencias ?? []) + advertenciasCarga
            if !advertencias.isEmpty {
                DispatchQueue.main.async { [weak self] in
                    self?.mostrarAviso(advertencias.joined(separator: "\n\n"))
                }
            }
        } catch {
            mostrarFallo(error.localizedDescription)
        }
    }

    override func viewWillDisappear(_ animated: Bool) {
        super.viewWillDisappear(animated)
        detenerReloj()
        audio?.stop()
        lienzo.finalizarReproduccion()
    }

    private func montarInterfaz() {
        navegador.translatesAutoresizingMaskIntoConstraints = false
        view.addSubview(navegador)

        botonPlay.setImage(UIImage(systemName: "play.fill"), for: .normal)
        botonPlay.addTarget(self, action: #selector(alternarPlay), for: .touchUpInside)

        configurarSaltos()

        progreso.minimumValue = 0
        progreso.addTarget(self, action: #selector(buscar(_:)), for: .valueChanged)

        tiempo.font = .monospacedDigitSystemFont(ofSize: 13, weight: .regular)
        tiempo.text = "00:00 / 00:00"

        velocidad.selectedSegmentIndex = 2
        velocidad.addTarget(self, action: #selector(cambiarVelocidad),
                            for: .valueChanged)

        botonMarcadores.setTitle("Marcadores", for: .normal)
        botonMarcadores.showsMenuAsPrimaryAction = true
        botonTranscripcion.setTitle("Transcripción", for: .normal)
        botonTranscripcion.addTarget(self, action: #selector(abrirTranscripcion),
                                     for: .touchUpInside)

        botonAudio.setTitle("Audio", for: .normal)
        botonAudio.showsMenuAsPrimaryAction = true
        botonVideo.setTitle("Vídeo PiP", for: .normal)
        botonVideo.addTarget(self, action: #selector(abrirVideo), for: .touchUpInside)

        saltarSilencios.addTarget(self, action: #selector(cambiarSaltoSilencios),
                                  for: .valueChanged)
        contextoActual.font = .preferredFont(forTextStyle: .caption1)
        contextoActual.textColor = .secondaryLabel
        contextoActual.numberOfLines = 1
        contextoActual.setContentCompressionResistancePriority(.defaultLow,
                                                                for: .horizontal)

        let filaSuperior = UIStackView(arrangedSubviews: [botonPlay, botonRetroceso,
                                                           botonAvance, progreso, tiempo])
        filaSuperior.axis = .horizontal
        filaSuperior.alignment = .center
        filaSuperior.spacing = 12

        let etiquetaBucle = UILabel(); etiquetaBucle.text = "Repetir"
        let etiquetaSilencios = UILabel(); etiquetaSilencios.text = "Saltar silencios"
        let filaInferior = UIStackView(arrangedSubviews: [velocidad, botonMarcadores,
                                                          botonTranscripcion, botonAudio,
                                                          botonVideo])
        filaInferior.axis = .horizontal
        filaInferior.alignment = .center
        filaInferior.spacing = 12

        let filaContexto = UIStackView(arrangedSubviews: [etiquetaBucle, bucle,
                                                           etiquetaSilencios,
                                                           saltarSilencios,
                                                           contextoActual])
        filaContexto.axis = .horizontal
        filaContexto.alignment = .center
        filaContexto.spacing = 10

        let controles = UIStackView(arrangedSubviews: [filaSuperior, filaInferior,
                                                        filaContexto])
        controles.axis = .vertical
        controles.spacing = 10
        controles.isLayoutMarginsRelativeArrangement = true
        controles.layoutMargins = UIEdgeInsets(top: 10, left: 16, bottom: 12, right: 16)
        controles.translatesAutoresizingMaskIntoConstraints = false
        view.addSubview(controles)

        NSLayoutConstraint.activate([
            navegador.topAnchor.constraint(equalTo: view.safeAreaLayoutGuide.topAnchor),
            navegador.leadingAnchor.constraint(equalTo: view.leadingAnchor),
            navegador.trailingAnchor.constraint(equalTo: view.trailingAnchor),
            navegador.bottomAnchor.constraint(equalTo: controles.topAnchor),
            controles.leadingAnchor.constraint(equalTo: view.leadingAnchor),
            controles.trailingAnchor.constraint(equalTo: view.trailingAnchor),
            controles.bottomAnchor.constraint(equalTo: view.safeAreaLayoutGuide.bottomAnchor),
        ])
    }

    private func configurarSaltos() {
        botonRetroceso.setImage(UIImage(systemName: "gobackward.15"), for: .normal)
        botonAvance.setImage(UIImage(systemName: "goforward.15"), for: .normal)
        botonRetroceso.showsMenuAsPrimaryAction = true
        botonAvance.showsMenuAsPrimaryAction = true
        botonRetroceso.menu = menuSaltos(signo: -1)
        botonAvance.menu = menuSaltos(signo: 1)
    }

    private func menuSaltos(signo: Double) -> UIMenu {
        UIMenu(children: [5, 10, 15, 30].map { cantidad in
            UIAction(title: "\(signo < 0 ? "Retroceder" : "Avanzar") \(cantidad) s") {
                [weak self] _ in self?.saltar(signo * Double(cantidad))
            }
        })
    }

    private func cargarPaquete() throws {
        guard let manifiesto = try? RepositorioManifiestoNotaViva.cargar(
            manifiestoURL) else {
            throw Fallo.manifiestoInvalido
        }
        let carpeta = manifiestoURL.deletingLastPathComponent()
        var cronologiaCargada = false
        if let nombreCronologia = manifiesto.archivoCronologia {
            do {
                let datos = try Data(contentsOf: carpeta.appendingPathComponent(
                    nombreCronologia))
                if lienzo.sesion.cargarCronologiaNotaViva(datos) {
                    cronologiaCargada = true
                    usaCronologiaVectorial = true
                } else {
                    registrarAdvertenciaCronologia(lienzo.sesion.ultimoError)
                }
            } catch {
                registrarAdvertenciaCronologia(error.localizedDescription)
            }
        }
        if !cronologiaCargada {
            guard let tinta = try? Data(contentsOf: carpeta.appendingPathComponent(
                manifiesto.archivoTinta)),
                  lienzo.sesion.cargarComoCopia(tinta) else {
                throw Fallo.tintaInvalida
            }
            usaCronologiaVectorial = false
        }
        navegador.recargarElementos()
        navegador.sincronizarTamanoDocumento(conservarCentro: false)
        // Reproducir nunca debe editar la copia mediante menús contextuales o
        // gestos de sus objetos; zoom y desplazamiento del scroll siguen vivos.
        navegador.capaElementos.isUserInteractionEnabled = false

        if let nombreAudio = manifiesto.archivoAudio {
            let url = carpeta.appendingPathComponent(nombreAudio)
            audioOriginalURL = url
            cargarAudio(url, avisarSiFalla: false)
        }
        if let nombreMejorado = manifiesto.archivoAudioMejorado {
            let url = carpeta.appendingPathComponent(nombreMejorado)
            if FileManager.default.fileExists(atPath: url.path) { audioMejoradoURL = url }
        }
        self.manifiesto = manifiesto
        progreso.maximumValue = Float(max(0.001, manifiesto.duracion))
        botonMarcadores.menu = menuMarcadores(manifiesto.eventos,
                                              capitulos: manifiesto.capitulos ?? [])
        botonVideo.isHidden = manifiesto.videoAutorizadoConfirmado != true
            || manifiesto.archivoVideoAutorizado == nil
        saltarSilencios.isEnabled = audio != nil
        actualizarMenuAudio()
    }

    private func registrarAdvertenciaCronologia(_ detalle: String) {
        var mensaje = "La reproducción paso a paso de esta Nota Viva no está disponible. Se abrió la copia final de la nota."
        let limpio = detalle.trimmingCharacters(in: .whitespacesAndNewlines)
        if !limpio.isEmpty { mensaje += "\n\nDetalle: \(limpio)" }
        if !advertenciasCarga.contains(mensaje) { advertenciasCarga.append(mensaje) }
    }

    @discardableResult
    private func cargarAudio(
        _ url: URL,
        conservarSegundo: Double? = nil,
        avisarSiFalla: Bool = true
    ) -> Bool {
        let segundo = conservarSegundo ?? Double(progreso.value)
        let estabaReproduciendo = reproduciendo
        audio?.stop()
        do {
            let nuevo = try AVAudioPlayer(contentsOf: url)
            guard nuevo.duration.isFinite, nuevo.duration > 0,
                  nuevo.prepareToPlay() else { throw Fallo.audioInvalido }
            audio = nuevo
            audioActualURL = url
        } catch {
            audio = nil
            audioActualURL = nil
            let mensaje = error.localizedDescription
            if !advertenciasCarga.contains(mensaje) { advertenciasCarga.append(mensaje) }
            actualizarMenuAudio()
            if avisarSiFalla { mostrarAviso(mensaje) }
            return false
        }
        audio?.enableRate = true
        audio?.currentTime = min(max(segundo, 0), audio?.duration ?? segundo)
        if estabaReproduciendo {
            audio?.rate = Float(velocidadActual)
            audio?.play()
        }
        actualizarMenuAudio()
        return true
    }

    @objc private func alternarPlay() {
        reproduciendo ? pausar() : reproducir()
    }

    private func reproducir() {
        guard let manifiesto else { return }
        if Double(progreso.value) >= manifiesto.duracion - 0.01 {
            progreso.value = 0
        }
        reproduciendo = true
        inicioMedio = Double(progreso.value)
        inicioPared = CACurrentMediaTime()
        if let audio {
            audio.currentTime = inicioMedio
            audio.rate = Float(velocidadActual)
            audio.play()
        }
        botonPlay.setImage(UIImage(systemName: "pause.fill"), for: .normal)
        iniciarReloj()
    }

    private func pausar() {
        guard reproduciendo else { return }
        let actual = segundoActual
        reproduciendo = false
        audio?.pause()
        mostrar(segundo: actual)
        botonPlay.setImage(UIImage(systemName: "play.fill"), for: .normal)
        detenerReloj()
    }

    @objc private func buscar(_ sender: UISlider) {
        let continuar = reproduciendo
        if continuar { pausar() }
        ir(a: Double(sender.value), enfocarContexto: true)
        if continuar { reproducir() }
    }

    private func saltar(_ delta: Double) {
        let continuar = reproduciendo
        if continuar { pausar() }
        ir(a: segundoActual + delta, enfocarContexto: true)
        if continuar { reproducir() }
    }

    private func ir(a segundo: Double, enfocarContexto: Bool) {
        guard let manifiesto else { return }
        let destino = min(max(segundo, 0), manifiesto.duracion)
        progreso.value = Float(destino)
        audio?.currentTime = destino
        if enfocarContexto { ultimoEventoEnfocado = -1 }
        mostrar(segundo: destino)
    }

    @objc private func cambiarVelocidad() {
        guard reproduciendo else { return }
        let actual = segundoActual
        inicioMedio = actual
        inicioPared = CACurrentMediaTime()
        audio?.rate = Float(velocidadActual)
    }

    private var velocidadActual: Double {
        let indice = max(0, min(velocidad.selectedSegmentIndex,
                                velocidades.count - 1))
        return velocidades[indice]
    }

    private var segundoActual: Double {
        if let audio, audio.isPlaying { return audio.currentTime }
        guard reproduciendo else { return Double(progreso.value) }
        return inicioMedio + (CACurrentMediaTime() - inicioPared) * velocidadActual
    }

    private func iniciarReloj() {
        detenerReloj()
        let link = CADisplayLink(target: self, selector: #selector(actualizarReloj))
        link.preferredFrameRateRange = CAFrameRateRange(minimum: 15, maximum: 30,
                                                        preferred: 30)
        link.add(to: .main, forMode: .common)
        displayLink = link
    }

    private func detenerReloj() {
        displayLink?.invalidate()
        displayLink = nil
    }

    @objc private func actualizarReloj() {
        guard let manifiesto else { return }
        var segundo = segundoActual
        if saltarSilencios.isOn,
           let silencio = manifiesto.silencios?.first(where: {
               segundo >= $0.inicio && segundo < $0.fin
           }), silencio.duracion >= 0.35 {
            segundo = min(silencio.fin, manifiesto.duracion)
            inicioMedio = segundo
            inicioPared = CACurrentMediaTime()
            audio?.currentTime = segundo
            if reproduciendo { audio?.play() }
        }
        if bucle.isOn, let rangoBucle, segundo >= rangoBucle.upperBound {
            segundo = rangoBucle.lowerBound
            progreso.value = Float(segundo)
            inicioMedio = segundo
            inicioPared = CACurrentMediaTime()
            audio?.currentTime = segundo
            audio?.play()
        }
        if segundo >= manifiesto.duracion {
            if bucle.isOn {
                segundo = 0
                progreso.value = 0
                inicioMedio = 0
                inicioPared = CACurrentMediaTime()
                audio?.currentTime = 0
                audio?.play()
            } else {
                segundo = manifiesto.duracion
                reproduciendo = false
                audio?.pause()
                botonPlay.setImage(UIImage(systemName: "play.fill"), for: .normal)
                detenerReloj()
                mostrar(segundo: segundo)
                return
            }
        }
        mostrar(segundo: segundo)
    }

    private func mostrar(segundo: Double) {
        guard let manifiesto else { return }
        let acotado = max(0, min(segundo, manifiesto.duracion))
        progreso.value = Float(acotado)
        tiempo.text = "\(formato(acotado)) / \(formato(manifiesto.duracion))"
        let limite = monotono(para: acotado, manifiesto: manifiesto)
        if usaCronologiaVectorial,
           lienzo.sesion.mostrarCronologiaNotaViva(hasta: limite) {
            navegador.recargarElementos()
            navegador.sincronizarTamanoDocumento(conservarCentro: true)
        }
        lienzo.reproducirTinta(hasta: limite)
        sincronizarContexto(segundo: acotado, manifiesto: manifiesto)
    }

    private func sincronizarContexto(segundo: Double, manifiesto: ManifiestoNotaViva) {
        let indice = manifiesto.eventos.lastIndex { $0.segundo <= segundo } ?? -1
        guard indice >= 0 else { contextoActual.text = nil; return }
        let evento = manifiesto.eventos[indice]
        contextoActual.text = "\(nombre(evento.tipo)) · \(evento.titulo ?? formato(evento.segundo))"
        guard indice != ultimoEventoEnfocado else { return }
        ultimoEventoEnfocado = indice
        if let x = evento.x, let y = evento.y {
            navegador.mostrarRegion(
                CGRect(x: CGFloat(x) - 260, y: CGFloat(y) - 210,
                       width: 520, height: 420), animado: false)
        }
    }

    private func monotono(para segundo: Double,
                          manifiesto: ManifiestoNotaViva) -> Double {
        guard !manifiesto.segmentos.isEmpty else {
            return manifiesto.inicioMonotono + segundo
        }
        for segmento in manifiesto.segmentos {
            let fin = segmento.monotonoFin ?? (segmento.monotonoInicio
                                                + manifiesto.duracion)
            let duracion = max(0, fin - segmento.monotonoInicio)
            let finMedio = segmento.segundoInicio + duracion
            if segundo <= finMedio {
                return segmento.monotonoInicio + max(0, segundo - segmento.segundoInicio)
            }
        }
        let ultimo = manifiesto.segmentos.last!
        return ultimo.monotonoFin ?? (ultimo.monotonoInicio + manifiesto.duracion)
    }

    private func menuMarcadores(_ eventos: [EventoNotaViva],
                                capitulos: [CapituloAudio]) -> UIMenu {
        let visibles = eventos.filter {
            [.mePerdi, .importante, .examen, .marcador, .cambioDiapositiva,
             .fotoPizarra].contains($0.tipo)
        }
        let accionesCapitulos = capitulos.map { capitulo in
            UIAction(title: capitulo.titulo.isEmpty ? "Capítulo" : capitulo.titulo,
                     subtitle: formato(capitulo.inicio)) { [weak self] _ in
                self?.progreso.value = Float(capitulo.inicio)
                self?.ir(a: capitulo.inicio, enfocarContexto: true)
            }
        }
        let acciones = visibles.map { evento in
            UIAction(title: evento.titulo ?? nombre(evento.tipo),
                     subtitle: formato(evento.segundo)) { [weak self] _ in
                self?.progreso.value = Float(evento.segundo)
                self?.ir(a: evento.segundo, enfocarContexto: true)
            }
        }
        let todas = accionesCapitulos + acciones
        return UIMenu(children: todas.isEmpty
                      ? [UIAction(title: "Sin marcadores") { _ in }]
                      : todas)
    }

    @objc private func abrirTranscripcion() {
        guard let actualizado = try? RepositorioManifiestoNotaViva.cargar(
                manifiestoURL),
              let segmentos = actualizado.transcripcion, !segmentos.isEmpty else {
            mostrarFallo("La transcripción local todavía no está disponible o falta el modelo del idioma.")
            return
        }
        manifiesto = actualizado
        let controlador = ControladorTranscripcion(segmentos: segmentos)
        controlador.alIr = { [weak self] segundo in
            self?.ir(a: segundo, enfocarContexto: true)
        }
        controlador.alRepetir = { [weak self] rango in
            self?.rangoBucle = rango
            self?.bucle.isOn = true
            self?.ir(a: rango.lowerBound, enfocarContexto: true)
        }
        navigationController?.pushViewController(controlador, animated: true)
    }

    @objc private func cambiarSaltoSilencios() {
        guard saltarSilencios.isOn,
              manifiesto?.silencios == nil else { return }
        guard let audioOriginalURL, !analizandoSilencios else {
            saltarSilencios.isOn = false
            return
        }
        analizandoSilencios = true
        let espera = alertaEspera(
            titulo: "Analizando silencios",
            mensaje: "El análisis ocurre localmente y no modifica el audio.")
        present(espera, animated: true)
        DispatchQueue.global(qos: .userInitiated).async { [weak self] in
            do {
                let segmentos = try ProcesadorAudioLocal.detectarSilencios(en: audioOriginalURL)
                DispatchQueue.main.async {
                    guard let self else { return }
                    self.analizandoSilencios = false
                    espera.dismiss(animated: true)
                    self.actualizarManifiesto { $0.silencios = segmentos }
                    if segmentos.isEmpty {
                        self.saltarSilencios.isOn = false
                        self.mostrarAviso("No se detectaron silencios suficientemente largos con el umbral conservador.")
                    }
                }
            } catch {
                DispatchQueue.main.async {
                    self?.analizandoSilencios = false
                    self?.saltarSilencios.isOn = false
                    espera.dismiss(animated: true) {
                        self?.mostrarAviso(error.localizedDescription)
                    }
                }
            }
        }
    }

    private func actualizarMenuAudio() {
        var acciones: [UIMenuElement] = []
        if let original = audioOriginalURL {
            acciones.append(UIAction(
                title: "Original inmutable",
                image: UIImage(systemName: "waveform"),
                state: audioActualURL == original ? .on : .off) { [weak self] _ in
                    self?.cargarAudio(original, conservarSegundo: self?.segundoActual)
                })
        }
        if let mejorado = audioMejoradoURL {
            acciones.append(UIAction(
                title: "Copia mejorada local",
                image: UIImage(systemName: "waveform.badge.plus"),
                state: audioActualURL == mejorado ? .on : .off) { [weak self] _ in
                    self?.cargarAudio(mejorado, conservarSegundo: self?.segundoActual)
                })
        }
        acciones.append(UIAction(
            title: "Crear otra copia mejorada…",
            image: UIImage(systemName: "slider.horizontal.3")) { [weak self] _ in
                self?.configurarMejoraAudio()
            })
        botonAudio.menu = UIMenu(children: acciones)
    }

    private func configurarMejoraAudio() {
        guard audioOriginalURL != nil else {
            mostrarAviso("Esta Nota Viva no contiene una pista de audio original.")
            return
        }
        let panel = PanelMejoraAudio()
        panel.alAplicar = { [weak self, weak panel] ajustes in
            panel?.dismiss(animated: true) { self?.crearAudioMejorado(ajustes) }
        }
        present(UINavigationController(rootViewController: panel), animated: true)
    }

    private func crearAudioMejorado(_ ajustes: AjustesMejoraAudio) {
        guard let original = audioOriginalURL else { return }
        let destino = ProcesadorAudioLocal.destinoUnico(juntoA: original)
        let espera = alertaEspera(
            titulo: "Creando copia mejorada",
            mensaje: "Reducción, voz, zumbido y canales se procesan en el iPad. El original se conserva.")
        present(espera, animated: true)
        DispatchQueue.global(qos: .userInitiated).async { [weak self] in
            do {
                _ = try ProcesadorAudioLocal.mejorar(
                    original: original, destino: destino, ajustes: ajustes)
                DispatchQueue.main.async {
                    guard let self else { return }
                    self.audioMejoradoURL = destino
                    self.actualizarManifiesto {
                        $0.archivoAudioMejorado = destino.lastPathComponent
                        $0.ajustesAudioMejorado = ajustes
                    }
                    espera.dismiss(animated: true) {
                        self.cargarAudio(destino, conservarSegundo: self.segundoActual)
                    }
                }
            } catch {
                DispatchQueue.main.async {
                    espera.dismiss(animated: true) {
                        self?.mostrarAviso(error.localizedDescription)
                    }
                }
            }
        }
    }

    @objc private func abrirVideo() {
        guard let manifiesto,
              manifiesto.videoAutorizadoConfirmado == true,
              let nombre = manifiesto.archivoVideoAutorizado else {
            mostrarAviso("No hay un vídeo autorizado vinculado a esta Nota Viva.")
            return
        }
        let url = manifiestoURL.deletingLastPathComponent().appendingPathComponent(nombre)
        guard FileManager.default.fileExists(atPath: url.path) else {
            mostrarAviso("El vídeo vinculado ya no está dentro del paquete.")
            return
        }
        let reproductor = ReproductorVideoClasePiP(url: url, segundo: segundoActual)
        videoClase = reproductor
        present(UINavigationController(rootViewController: reproductor), animated: true)
    }

    private func actualizarManifiesto(_ cambio: (inout ManifiestoNotaViva) -> Void) {
        guard manifiesto != nil else { return }
        do {
            self.manifiesto = try RepositorioManifiestoNotaViva.actualizar(
                manifiestoURL, cambio: cambio)
        } catch {
            mostrarAviso("No se pudo actualizar el manifiesto: \(error.localizedDescription)")
        }
    }

    private func alertaEspera(titulo: String, mensaje: String) -> UIAlertController {
        let alerta = UIAlertController(title: titulo, message: "\n\(mensaje)",
                                       preferredStyle: .alert)
        let indicador = UIActivityIndicatorView(style: .medium)
        indicador.startAnimating()
        indicador.translatesAutoresizingMaskIntoConstraints = false
        alerta.view.addSubview(indicador)
        NSLayoutConstraint.activate([
            indicador.centerXAnchor.constraint(equalTo: alerta.view.centerXAnchor),
            indicador.topAnchor.constraint(equalTo: alerta.view.topAnchor, constant: 58),
        ])
        return alerta
    }

    private func mostrarAviso(_ texto: String) {
        let alerta = UIAlertController(title: nil, message: texto, preferredStyle: .alert)
        alerta.addAction(UIAlertAction(title: "Aceptar", style: .default))
        present(alerta, animated: true)
    }

    private func nombre(_ tipo: TipoEventoNotaViva) -> String {
        switch tipo {
        case .mePerdi: return "Me perdí"
        case .importante: return "Importante"
        case .examen: return "Examen"
        case .marcador: return "Marcador"
        case .cambioDiapositiva: return "Cambio de diapositiva"
        case .fotoPizarra: return "Foto de pizarra"
        case .videoClase: return "Vídeo de clase"
        case .explicacionOral: return "Explicación oral"
        default: return tipo.rawValue
        }
    }

    private func formato(_ segundos: Double) -> String {
        let total = max(0, Int(segundos))
        return String(format: "%02d:%02d", total / 60, total % 60)
    }

    private func mostrarFallo(_ texto: String) {
        let alerta = UIAlertController(title: "No se pudo abrir Nota Viva",
                                       message: texto, preferredStyle: .alert)
        alerta.addAction(UIAlertAction(title: "Cerrar", style: .cancel) { [weak self] _ in
            self?.dismiss(animated: true)
        })
        present(alerta, animated: true)
    }

    @objc private func cerrar() { dismiss(animated: true) }
}

final class ControladorTranscripcion: UITableViewController {
    var alIr: ((Double) -> Void)?
    var alRepetir: ((ClosedRange<Double>) -> Void)?
    private let segmentos: [SegmentoTranscripcion]

    init(segmentos: [SegmentoTranscripcion]) {
        self.segmentos = segmentos
        super.init(style: .insetGrouped)
    }

    required init?(coder: NSCoder) { fatalError("init(coder:) no está implementado") }

    override func viewDidLoad() {
        super.viewDidLoad()
        title = "Transcripción vinculada"
        tableView.register(UITableViewCell.self, forCellReuseIdentifier: "segmento")
    }

    override func tableView(_ tableView: UITableView,
                            numberOfRowsInSection section: Int) -> Int { segmentos.count }

    override func tableView(_ tableView: UITableView, cellForRowAt indexPath: IndexPath)
        -> UITableViewCell {
        let celda = UITableViewCell(style: .subtitle, reuseIdentifier: nil)
        let segmento = segmentos[indexPath.row]
        celda.textLabel?.text = segmento.texto
        celda.textLabel?.numberOfLines = 0
        celda.detailTextLabel?.text = String(format: "%02d:%02d · confianza %d%%",
            Int(segmento.inicio) / 60, Int(segmento.inicio) % 60,
            Int(segmento.confianza * 100))
        celda.accessoryType = .disclosureIndicator
        return celda
    }

    override func tableView(_ tableView: UITableView, didSelectRowAt indexPath: IndexPath) {
        alIr?(segmentos[indexPath.row].inicio)
    }

    override func tableView(_ tableView: UITableView,
                            trailingSwipeActionsConfigurationForRowAt indexPath: IndexPath)
        -> UISwipeActionsConfiguration? {
        let segmento = segmentos[indexPath.row]
        let repetir = UIContextualAction(style: .normal, title: "Repetir") { [weak self] _, _, fin in
            self?.alRepetir?(segmento.inicio...(segmento.inicio
                + max(segmento.duracion, 3)))
            fin(true)
        }
        repetir.backgroundColor = .systemIndigo
        return UISwipeActionsConfiguration(actions: [repetir])
    }
}

final class PanelMejoraAudio: UITableViewController {
    var alAplicar: ((AjustesMejoraAudio) -> Void)?
    private let ruido = UISwitch()
    private let voz = UISwitch()
    private let normalizar = UISwitch()
    private let zumbido = UISwitch()
    private let frecuencia = UISegmentedControl(items: ["50 Hz", "60 Hz"])
    private let canales = UISegmentedControl(
        items: ModoCanalesMejoraAudio.allCases.map(\.nombre))

    init() { super.init(style: .insetGrouped) }
    required init?(coder: NSCoder) { fatalError("init(coder:) no está implementado") }

    override func viewDidLoad() {
        super.viewDidLoad()
        title = "Mejorar copia de audio"
        navigationItem.leftBarButtonItem = UIBarButtonItem(
            barButtonSystemItem: .cancel, target: self, action: #selector(cancelar))
        navigationItem.rightBarButtonItem = UIBarButtonItem(
            title: "Crear copia", style: .done, target: self, action: #selector(aplicar))
        [ruido, voz, normalizar, zumbido].forEach { $0.isOn = true }
        frecuencia.selectedSegmentIndex = 1
        canales.selectedSegmentIndex = 0
        tableView.isScrollEnabled = true
    }

    override func numberOfSections(in tableView: UITableView) -> Int { 3 }
    override func tableView(_ tableView: UITableView,
                            numberOfRowsInSection section: Int) -> Int {
        [4, 1, 1][section]
    }

    override func tableView(_ tableView: UITableView,
                            titleForHeaderInSection section: Int) -> String? {
        ["Procesamiento local", "Frecuencia de red", "Canales de la copia"][section]
    }

    override func tableView(_ tableView: UITableView,
                            titleForFooterInSection section: Int) -> String? {
        guard section == 2 else { return nil }
        return "La salida es una copia PCM separada. El M4A original nunca se sobrescribe. Mono mezcla los canales; las otras opciones aíslan uno."
    }

    override func tableView(_ tableView: UITableView,
                            cellForRowAt indexPath: IndexPath) -> UITableViewCell {
        let celda = UITableViewCell(style: .default, reuseIdentifier: nil)
        if indexPath.section == 0 {
            let filas: [(String, UISwitch)] = [
                ("Reducción conservadora de ruido", ruido),
                ("Realce de voz", voz),
                ("Normalización", normalizar),
                ("Eliminar zumbido eléctrico", zumbido),
            ]
            celda.textLabel?.text = filas[indexPath.row].0
            celda.accessoryView = filas[indexPath.row].1
        } else if indexPath.section == 1 {
            celda.contentView.addSubview(frecuencia)
            frecuencia.translatesAutoresizingMaskIntoConstraints = false
            NSLayoutConstraint.activate([
                frecuencia.leadingAnchor.constraint(equalTo: celda.contentView.leadingAnchor,
                                                     constant: 16),
                frecuencia.trailingAnchor.constraint(equalTo: celda.contentView.trailingAnchor,
                                                      constant: -16),
                frecuencia.centerYAnchor.constraint(equalTo: celda.contentView.centerYAnchor),
            ])
        } else {
            celda.contentView.addSubview(canales)
            canales.translatesAutoresizingMaskIntoConstraints = false
            canales.apportionsSegmentWidthsByContent = true
            NSLayoutConstraint.activate([
                canales.leadingAnchor.constraint(equalTo: celda.contentView.leadingAnchor,
                                                  constant: 12),
                canales.trailingAnchor.constraint(equalTo: celda.contentView.trailingAnchor,
                                                   constant: -12),
                canales.centerYAnchor.constraint(equalTo: celda.contentView.centerYAnchor),
            ])
        }
        return celda
    }

    override func tableView(_ tableView: UITableView,
                            heightForRowAt indexPath: IndexPath) -> CGFloat {
        indexPath.section == 2 ? 58 : 48
    }

    @objc private func aplicar() {
        let lista = ModoCanalesMejoraAudio.allCases
        let indice = max(0, min(canales.selectedSegmentIndex, lista.count - 1))
        alAplicar?(AjustesMejoraAudio(
            reduccionRuido: ruido.isOn,
            realceVoz: voz.isOn,
            normalizar: normalizar.isOn,
            eliminarZumbido: zumbido.isOn,
            frecuenciaZumbidoHz: frecuencia.selectedSegmentIndex == 0 ? 50 : 60,
            canales: lista[indice]))
    }

    @objc private func cancelar() { dismiss(animated: true) }
}

private final class VistaVideoClase: UIView {
    override class var layerClass: AnyClass { AVPlayerLayer.self }
    var capa: AVPlayerLayer { layer as! AVPlayerLayer }
}

/// Reproductor de vídeo expresamente vinculado como autorizado. PiP usa la API
/// pública de AVKit y se inicia únicamente al tocar el botón visible.
final class ReproductorVideoClasePiP: UIViewController,
                                      AVPictureInPictureControllerDelegate {
    private let url: URL
    private let segundoInicial: Double
    private let vistaVideo = VistaVideoClase()
    private let reproductor: AVPlayer
    private var pip: AVPictureInPictureController?

    init(url: URL, segundo: Double) {
        self.url = url
        self.segundoInicial = max(0, segundo)
        self.reproductor = AVPlayer(url: url)
        super.init(nibName: nil, bundle: nil)
    }

    required init?(coder: NSCoder) { fatalError("init(coder:) no está implementado") }

    override func viewDidLoad() {
        super.viewDidLoad()
        title = "Vídeo autorizado de la clase"
        view.backgroundColor = .black
        navigationItem.leftBarButtonItem = UIBarButtonItem(
            barButtonSystemItem: .close, target: self, action: #selector(cerrar))
        navigationItem.rightBarButtonItem = UIBarButtonItem(
            title: "Abrir PiP", style: .done, target: self, action: #selector(iniciarPiP))

        vistaVideo.translatesAutoresizingMaskIntoConstraints = false
        vistaVideo.capa.player = reproductor
        vistaVideo.capa.videoGravity = .resizeAspect
        view.addSubview(vistaVideo)
        NSLayoutConstraint.activate([
            vistaVideo.topAnchor.constraint(equalTo: view.safeAreaLayoutGuide.topAnchor),
            vistaVideo.leadingAnchor.constraint(equalTo: view.leadingAnchor),
            vistaVideo.trailingAnchor.constraint(equalTo: view.trailingAnchor),
            vistaVideo.bottomAnchor.constraint(equalTo: view.bottomAnchor),
        ])

        if AVPictureInPictureController.isPictureInPictureSupported() {
            pip = AVPictureInPictureController(playerLayer: vistaVideo.capa)
            pip?.delegate = self
            pip?.canStartPictureInPictureAutomaticallyFromInline = false
        } else {
            navigationItem.rightBarButtonItem?.isEnabled = false
        }
        try? AVAudioSession.sharedInstance().setCategory(.playback, mode: .moviePlayback)
        try? AVAudioSession.sharedInstance().setActive(true)
        reproductor.seek(to: CMTime(seconds: segundoInicial, preferredTimescale: 600),
                         toleranceBefore: .zero, toleranceAfter: .zero)
        reproductor.play()
    }

    @objc private func iniciarPiP() {
        guard let pip, pip.isPictureInPicturePossible else {
            let alerta = UIAlertController(
                title: "PiP todavía no está disponible",
                message: "Espere a que el vídeo comience o use los controles de reproducción del sistema.",
                preferredStyle: .alert)
            alerta.addAction(UIAlertAction(title: "Aceptar", style: .default))
            present(alerta, animated: true)
            return
        }
        pip.startPictureInPicture()
    }

    func pictureInPictureControllerDidStartPictureInPicture(
        _ pictureInPictureController: AVPictureInPictureController
    ) {
        dismiss(animated: true)
    }

    @objc private func cerrar() {
        reproductor.pause()
        dismiss(animated: true)
    }
}