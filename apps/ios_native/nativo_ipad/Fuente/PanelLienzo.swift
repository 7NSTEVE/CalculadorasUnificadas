import UIKit

struct ConfiguracionLienzo {
    var modo: ModoPaginaLienzo
    var ancho: Float
    var alto: Float
    var unidad: UnidadLienzo
    var reacomodo: ReacomodoLienzo
    var tipoPapel: TipoPapel
    var papel: AjustesPapel
    var mostrarTiradoresTamano: Bool
    var mostrarCortes: Bool
    var altoHoja: Float
    var margenSeguro: Float
}

struct PoliticaModoLienzoIPad: Equatable {
    var anchoInfinito: Bool
    var altoInfinito: Bool
    var tituloAncho: String
    var tituloAlto: String
    var detalle: String

    var tieneEjeFinito: Bool { !anchoInfinito || !altoInfinito }

    static func para(_ modo: ModoPaginaLienzo) -> PoliticaModoLienzoIPad {
        switch modo {
        case .libro, .paginadoFijo:
            return PoliticaModoLienzoIPad(
                anchoInfinito: false,
                altoInfinito: false,
                tituloAncho: "Ancho fijo",
                tituloAlto: "Alto fijo",
                detalle: "Ancho fijo · alto fijo. El documento conserva bordes físicos en ambos ejes.")
        case .hibrido:
            return PoliticaModoLienzoIPad(
                anchoInfinito: false,
                altoInfinito: true,
                tituloAncho: "Ancho fijo",
                tituloAlto: "Alto inicial",
                detalle: "Ancho fijo · alto infinito. La altura indicada es solo el área inicial y crecerá automáticamente al escribir hacia abajo.")
        case .presentacion:
            return PoliticaModoLienzoIPad(
                anchoInfinito: false,
                altoInfinito: true,
                tituloAncho: "Ancho fijo",
                tituloAlto: "Alto inicial",
                detalle: "Ancho fijo · recorrido vertical extensible. Sirve para diapositivas y apuntes que continúan hacia abajo.")
        case .horizontalInfinito:
            return PoliticaModoLienzoIPad(
                anchoInfinito: true,
                altoInfinito: false,
                tituloAncho: "Ancho inicial",
                tituloAlto: "Alto fijo",
                detalle: "Ancho infinito · alto fijo. El ancho indicado es solo el área inicial y crecerá al trabajar hacia los lados.")
        case .infinitoXY, .pizarra:
            return PoliticaModoLienzoIPad(
                anchoInfinito: true,
                altoInfinito: true,
                tituloAncho: "Ancho inicial",
                tituloAlto: "Alto inicial",
                detalle: "Ancho infinito · alto infinito. Los dos valores definen únicamente el área inicial de trabajo; el lienzo puede crecer en cualquier dirección.")
        }
    }
}

enum ConversorUnidadLienzoIPad {
    static let ppp: Float = 150
    private static let milimetrosPorPulgada: Float = 25.4

    static func convertir(_ valor: Float, de origen: UnidadLienzo,
                          a destino: UnidadLienzo) -> Float {
        guard valor.isFinite else { return valor }
        let pixeles: Float
        switch origen {
        case .pixeles:
            pixeles = valor
        case .milimetros:
            pixeles = valor * ppp / milimetrosPorPulgada
        case .centimetros:
            pixeles = valor * 10 * ppp / milimetrosPorPulgada
        case .pulgadas:
            pixeles = valor * ppp
        }

        switch destino {
        case .pixeles:
            return pixeles
        case .milimetros:
            return pixeles * milimetrosPorPulgada / ppp
        case .centimetros:
            return pixeles * milimetrosPorPulgada / (ppp * 10)
        case .pulgadas:
            return pixeles / ppp
        }
    }
}

enum EspacioRapidoLienzoIPad: Int, CaseIterable {
    case cm1
    case cm3
    case cm5
    case cm10

    var centimetros: Float {
        switch self {
        case .cm1: return 1
        case .cm3: return 3
        case .cm5: return 5
        case .cm10: return 10
        }
    }

    var titulo: String {
        let valor = Int(centimetros)
        return "\(valor) cm"
    }

    var pixeles: Float {
        ConversorUnidadLienzoIPad.convertir(
            centimetros, de: .centimetros, a: .pixeles)
    }
}

enum PreajusteLienzo: CaseIterable {
    case a4, a3, a5, carta, oficio, dieciseisNueve, dieciseisDiez
    case cuatroTres, ipad, pantallaDividida, ingenieria, pdfNotas, pptNotas
    case a4InfinitoVertical, cartaInfinitaVertical, pizarraInfinita
    case circuitosPanoramicos

    var nombre: String {
        switch self {
        case .a4: return "A4"
        case .a3: return "A3"
        case .a5: return "A5"
        case .carta: return "Carta"
        case .oficio: return "Oficio / Legal"
        case .dieciseisNueve: return "16:9"
        case .dieciseisDiez: return "16:10"
        case .cuatroTres: return "4:3"
        case .ipad: return "iPad completo"
        case .pantallaDividida: return "Pantalla dividida"
        case .ingenieria: return "Engineering notebook"
        case .pdfNotas: return "PDF + notas laterales"
        case .pptNotas: return "PowerPoint + apuntes"
        case .a4InfinitoVertical: return "A4 · infinito vertical"
        case .cartaInfinitaVertical: return "Carta · infinito vertical"
        case .pizarraInfinita: return "Pizarra · infinito XY"
        case .circuitosPanoramicos: return "Circuitos · infinito horizontal"
        }
    }

    var modoSugerido: ModoPaginaLienzo? {
        switch self {
        case .a4InfinitoVertical, .cartaInfinitaVertical:
            return .hibrido
        case .pizarraInfinita:
            return .infinitoXY
        case .circuitosPanoramicos:
            return .horizontalInfinito
        default:
            return nil
        }
    }

    func valores(en pantalla: CGSize) -> (Float, Float, UnidadLienzo) {
        switch self {
        case .a4, .a4InfinitoVertical: return (210, 297, .milimetros)
        case .a3: return (297, 420, .milimetros)
        case .a5: return (148, 210, .milimetros)
        case .carta, .cartaInfinitaVertical: return (216, 279, .milimetros)
        case .oficio: return (216, 356, .milimetros)
        case .dieciseisNueve: return (1920, 1080, .pixeles)
        case .dieciseisDiez: return (1920, 1200, .pixeles)
        case .cuatroTres, .pizarraInfinita: return (1600, 1200, .pixeles)
        case .ipad: return (Float(pantalla.width), Float(pantalla.height), .pixeles)
        case .pantallaDividida:
            return (Float(max(pantalla.width / 2, 320)), Float(pantalla.height), .pixeles)
        case .ingenieria: return (237, 297, .milimetros)
        case .pdfNotas: return (297, 297, .milimetros)
        case .pptNotas: return (1920, 1620, .pixeles)
        case .circuitosPanoramicos: return (1600, 900, .pixeles)
        }
    }
}

/// Ajustes de la página híbrida y el papel. Nada de este panel rasteriza o
/// destruye contenido: el usuario elige explícitamente conservar o escalar.
final class PanelLienzo: UIViewController {
    var alAplicar: ((ConfiguracionLienzo) -> Void)?
    var alAjustarContenido: (() -> Void)?
    var alInsertarEspacio: ((EjeLienzo, Float, Float) -> Void)?
    var alAjustarAncho: (() -> Void)?
    var alAjustarPagina: (() -> Void)?

    private let inicial: ConfiguracionLienzo
    private let botonModo = UIButton(type: .system)
    private let detalleModo = UILabel()
    private let unidad = UISegmentedControl(items: UnidadLienzo.allCases.map(\.simbolo))
    private let campoAncho = UITextField.campoNumerico()
    private let campoAlto = UITextField.campoNumerico()
    private let etiquetaAncho = UILabel()
    private let etiquetaAlto = UILabel()
    private let escalar = UISwitch()
    private let tiradoresTamano = UISwitch()
    private let notaTiradores = UILabel()
    private let selectorEspacio = UISegmentedControl(
        items: EspacioRapidoLienzoIPad.allCases.map(\.titulo))
    private let cortes = UISwitch()
    private let campoAltoHoja = UITextField.campoNumerico()
    private let campoMargenSeguro = UITextField.campoNumerico()
    private let botonPreset = UIButton(type: .system)
    private let botonTipoPapel = UIButton(type: .system)
    private let botonPauta = UIButton(type: .system)
    private let campoSeparacion = UITextField.campoNumerico()
    private let campoOpacidad = UITextField.campoNumerico()
    private let campoGrosor = UITextField.campoNumerico()
    private let margenIzquierdo = UITextField.campoNumerico()
    private let margenDerecho = UITextField.campoNumerico()
    private let margenSuperior = UITextField.campoNumerico()
    private let margenInferior = UITextField.campoNumerico()
    private let ejes = UISwitch()
    private let exportarPauta = UISwitch()
    private let selectorColor = UIColorWell()
    private var tipoPapelElegido: TipoPapel
    private var pautaElegida: Pauta
    private var modoElegido: ModoPaginaLienzo
    private var unidadElegida: UnidadLienzo

    init(configuracion: ConfiguracionLienzo) {
        inicial = configuracion
        tipoPapelElegido = configuracion.tipoPapel
        pautaElegida = configuracion.papel.pauta
        modoElegido = configuracion.modo
        unidadElegida = configuracion.unidad
        super.init(nibName: nil, bundle: nil)
    }

    required init?(coder: NSCoder) { fatalError("init(coder:) no está implementado") }

    override func viewDidLoad() {
        super.viewDidLoad()
        title = "Lienzo y papel"
        view.backgroundColor = .systemGroupedBackground
        navigationItem.rightBarButtonItem = UIBarButtonItem(
            title: "Aplicar", style: .done, target: self, action: #selector(aplicarCambios))
        navigationItem.leftBarButtonItem = UIBarButtonItem(
            barButtonSystemItem: .cancel, target: self, action: #selector(cancelar))

        let desplazable = UIScrollView()
        desplazable.translatesAutoresizingMaskIntoConstraints = false
        view.addSubview(desplazable)
        let pila = UIStackView()
        pila.axis = .vertical
        pila.spacing = 12
        pila.isLayoutMarginsRelativeArrangement = true
        pila.layoutMargins = UIEdgeInsets(top: 20, left: 20, bottom: 28, right: 20)
        pila.translatesAutoresizingMaskIntoConstraints = false
        desplazable.addSubview(pila)
        NSLayoutConstraint.activate([
            desplazable.topAnchor.constraint(equalTo: view.topAnchor),
            desplazable.leadingAnchor.constraint(equalTo: view.leadingAnchor),
            desplazable.trailingAnchor.constraint(equalTo: view.trailingAnchor),
            desplazable.bottomAnchor.constraint(equalTo: view.bottomAnchor),
            pila.topAnchor.constraint(equalTo: desplazable.contentLayoutGuide.topAnchor),
            pila.leadingAnchor.constraint(equalTo: desplazable.contentLayoutGuide.leadingAnchor),
            pila.trailingAnchor.constraint(equalTo: desplazable.contentLayoutGuide.trailingAnchor),
            pila.bottomAnchor.constraint(equalTo: desplazable.contentLayoutGuide.bottomAnchor),
            pila.widthAnchor.constraint(equalTo: desplazable.frameLayoutGuide.widthAnchor),
        ])

        pila.addArrangedSubview(tituloSeccion("Comportamiento de página"))
        configurarMenuModo()
        pila.addArrangedSubview(fila("Modo", botonModo))
        configurarNotaSecundaria(detalleModo)
        pila.addArrangedSubview(detalleModo)

        botonPreset.configuration = .tinted()
        botonPreset.configuration?.title = "Elegir tamaño o comportamiento predefinido"
        botonPreset.showsMenuAsPrimaryAction = true
        botonPreset.menu = UIMenu(children: PreajusteLienzo.allCases.map { preset in
            UIAction(title: preset.nombre) { [weak self] _ in self?.aplicar(preset) }
        })
        pila.addArrangedSubview(botonPreset)

        unidad.selectedSegmentIndex = Int(unidadElegida.rawValue)
        unidad.addTarget(self, action: #selector(cambiarUnidad), for: .valueChanged)
        pila.addArrangedSubview(fila("Unidad", unidad))
        campoAncho.text = texto(inicial.ancho)
        campoAlto.text = texto(inicial.alto)
        pila.addArrangedSubview(filaDimensiones())
        escalar.isOn = inicial.reacomodo == .escalar
        pila.addArrangedSubview(fila("Escalar contenido proporcionalmente", escalar))
        pila.addArrangedSubview(nota("Si se desactiva, cambiar el papel no mueve ni deforma lo ya escrito."))
        tiradoresTamano.isOn = inicial.mostrarTiradoresTamano
        pila.addArrangedSubview(fila("Editar tamaño arrastrando bordes",
                                     tiradoresTamano))
        configurarNotaSecundaria(notaTiradores)
        pila.addArrangedSubview(notaTiradores)
        actualizarPresentacionModo()

        let accionesVista = UIStackView()
        accionesVista.axis = .horizontal
        accionesVista.distribution = .fillEqually
        accionesVista.spacing = 8
        accionesVista.addArrangedSubview(boton("Ajustar al ancho", #selector(ajustarAncho)))
        accionesVista.addArrangedSubview(boton("Página completa", #selector(ajustarPagina)))
        pila.addArrangedSubview(accionesVista)
        pila.addArrangedSubview(boton("Recortar papel al contenido", #selector(ajustarContenido)))

        pila.addArrangedSubview(tituloSeccion("Insertar espacio sin reescalar"))
        selectorEspacio.selectedSegmentIndex = EspacioRapidoLienzoIPad.cm3.rawValue
        pila.addArrangedSubview(fila("Cantidad", selectorEspacio))
        let insertar = UIStackView()
        insertar.axis = .horizontal
        insertar.distribution = .fillEqually
        insertar.spacing = 8
        insertar.addArrangedSubview(boton("Horizontal", #selector(insertarHorizontal)))
        insertar.addArrangedSubview(boton("Vertical", #selector(insertarVertical)))
        pila.addArrangedSubview(insertar)
        pila.addArrangedSubview(nota(
            "El hueco se inserta en el centro geométrico del documento actual sin escalar lo ya escrito."))

        pila.addArrangedSubview(tituloSeccion("Impresión"))
        cortes.isOn = inicial.mostrarCortes
        campoAltoHoja.text = texto(inicial.altoHoja)
        campoMargenSeguro.text = texto(inicial.margenSeguro)
        pila.addArrangedSubview(fila("Mostrar cortes seguros", cortes))
        pila.addArrangedSubview(filaDosCampos("Alto de hoja (px)", campoAltoHoja,
                                             "Margen seguro", campoMargenSeguro))

        pila.addArrangedSubview(tituloSeccion("Papel y pauta"))
        configurarMenusPapel()
        pila.addArrangedSubview(fila("Fondo", botonTipoPapel))
        pila.addArrangedSubview(fila("Pauta", botonPauta))
        campoSeparacion.text = texto(inicial.papel.separacion)
        campoOpacidad.text = texto(inicial.papel.opacidad)
        campoGrosor.text = texto(inicial.papel.grosor)
        pila.addArrangedSubview(filaDosCampos("Separación", campoSeparacion,
                                             "Grosor", campoGrosor))
        pila.addArrangedSubview(fila("Opacidad 0–1", campoOpacidad))
        selectorColor.selectedColor = UIColor(argb: inicial.papel.colorARGB == 0
                                               ? 0x5520_3040 : inicial.papel.colorARGB)
        pila.addArrangedSubview(fila("Color de pauta", selectorColor))

        margenIzquierdo.text = texto(inicial.papel.margenIzquierdo)
        margenDerecho.text = texto(inicial.papel.margenDerecho)
        margenSuperior.text = texto(inicial.papel.margenSuperior)
        margenInferior.text = texto(inicial.papel.margenInferior)
        pila.addArrangedSubview(filaDosCampos("Margen izquierdo", margenIzquierdo,
                                             "Margen derecho", margenDerecho))
        pila.addArrangedSubview(filaDosCampos("Margen superior", margenSuperior,
                                             "Margen inferior", margenInferior))
        ejes.isOn = inicial.papel.mostrarEjes
        exportarPauta.isOn = inicial.papel.incluirAlExportar
        pila.addArrangedSubview(fila("Mostrar ejes principales", ejes))
        pila.addArrangedSubview(fila("Incluir pauta al exportar", exportarPauta))
    }

    private func configurarMenusPapel() {
        botonTipoPapel.configuration = .bordered()
        botonTipoPapel.configuration?.title = tipoPapelElegido.nombre
        botonTipoPapel.showsMenuAsPrimaryAction = true
        botonTipoPapel.menu = UIMenu(children: TipoPapel.allCases.map { tipo in
            UIAction(title: tipo.nombre) { [weak self] _ in
                self?.tipoPapelElegido = tipo
                self?.botonTipoPapel.configuration?.title = tipo.nombre
            }
        })
        botonPauta.configuration = .bordered()
        botonPauta.configuration?.title = pautaElegida.nombre
        botonPauta.showsMenuAsPrimaryAction = true
        botonPauta.menu = UIMenu(children: Pauta.allCases.map { pauta in
            UIAction(title: pauta.nombre) { [weak self] _ in
                self?.pautaElegida = pauta
                self?.botonPauta.configuration?.title = pauta.nombre
            }
        })
    }

    private func configurarMenuModo() {
        botonModo.configuration = .bordered()
        botonModo.configuration?.title = modoElegido.nombre
        botonModo.configuration?.image = UIImage(systemName: modoElegido.simbolo)
        botonModo.configuration?.imagePadding = 6
        botonModo.showsMenuAsPrimaryAction = true
        botonModo.menu = UIMenu(children: ModoPaginaLienzo.allCases.map { modo in
            UIAction(title: modo.nombre,
                     image: UIImage(systemName: modo.simbolo),
                     state: modo == modoElegido ? .on : .off) { [weak self] _ in
                self?.modoElegido = modo
                self?.configurarMenuModo()
                self?.actualizarPresentacionModo()
            }
        })
    }

    private func actualizarPresentacionModo() {
        let politica = PoliticaModoLienzoIPad.para(modoElegido)
        detalleModo.text = politica.detalle
        etiquetaAncho.text = politica.tituloAncho
        etiquetaAlto.text = politica.tituloAlto
        campoAncho.accessibilityLabel = politica.tituloAncho
        campoAlto.accessibilityLabel = politica.tituloAlto
        tiradoresTamano.isEnabled = politica.tieneEjeFinito
        if !politica.tieneEjeFinito {
            notaTiradores.text = "Este modo no tiene bordes finitos. Los tiradores se desactivan porque el lienzo crece en ambos ejes."
        } else if politica.anchoInfinito || politica.altoInfinito {
            notaTiradores.text = "Solo aparecerán tiradores en los ejes que siguen siendo finitos; el eje infinito crece automáticamente."
        } else {
            notaTiradores.text = "Los tiradores azules aparecen únicamente mientras esta opción esté activa."
        }
    }

    private func aplicar(_ preset: PreajusteLienzo) {
        let valores = preset.valores(en: view.window?.screen.bounds.size
                                     ?? UIScreen.main.bounds.size)
        campoAncho.text = texto(valores.0)
        campoAlto.text = texto(valores.1)
        unidadElegida = valores.2
        unidad.selectedSegmentIndex = Int(valores.2.rawValue)
        if let modo = preset.modoSugerido {
            modoElegido = modo
            configurarMenuModo()
            actualizarPresentacionModo()
        }
        botonPreset.configuration?.title = preset.nombre
    }

    @objc private func aplicarCambios() {
        guard let ancho = numero(campoAncho.text), let alto = numero(campoAlto.text),
              ancho > 0, alto > 0 else {
            mostrarError("El ancho y el alto iniciales deben ser números mayores que cero.")
            return
        }
        var papel = inicial.papel
        papel.pauta = pautaElegida
        papel.separacion = max(numero(campoSeparacion.text) ?? 32, 1)
        papel.opacidad = min(max(numero(campoOpacidad.text) ?? 0.45, 0), 1)
        papel.grosor = min(max(numero(campoGrosor.text) ?? 1, 0.25), 12)
        papel.colorARGB = selectorColor.selectedColor?.argb ?? 0
        papel.margenIzquierdo = max(numero(margenIzquierdo.text) ?? 0, 0)
        papel.margenDerecho = max(numero(margenDerecho.text) ?? 0, 0)
        papel.margenSuperior = max(numero(margenSuperior.text) ?? 0, 0)
        papel.margenInferior = max(numero(margenInferior.text) ?? 0, 0)
        papel.mostrarEjes = ejes.isOn
        papel.ejeX = ancho / 2
        papel.ejeY = alto / 2
        papel.incluirAlExportar = exportarPauta.isOn
        let politica = PoliticaModoLienzoIPad.para(modoElegido)
        let configuracion = ConfiguracionLienzo(
            modo: modoElegido,
            ancho: ancho, alto: alto,
            unidad: unidadElegida,
            reacomodo: escalar.isOn ? .escalar : .conservar,
            tipoPapel: tipoPapelElegido, papel: papel,
            mostrarTiradoresTamano: politica.tieneEjeFinito && tiradoresTamano.isOn,
            mostrarCortes: cortes.isOn,
            altoHoja: max(numero(campoAltoHoja.text) ?? 1754, 64),
            margenSeguro: max(numero(campoMargenSeguro.text) ?? 36, 0))
        alAplicar?(configuracion)
        dismiss(animated: true)
    }

    @objc private func cancelar() { dismiss(animated: true) }

    @objc private func cambiarUnidad() {
        guard let nueva = UnidadLienzo(
            rawValue: Int32(unidad.selectedSegmentIndex)),
              nueva != unidadElegida else { return }
        if let ancho = numero(campoAncho.text) {
            campoAncho.text = texto(ConversorUnidadLienzoIPad.convertir(
                ancho, de: unidadElegida, a: nueva))
        }
        if let alto = numero(campoAlto.text) {
            campoAlto.text = texto(ConversorUnidadLienzoIPad.convertir(
                alto, de: unidadElegida, a: nueva))
        }
        unidadElegida = nueva
    }

    @objc private func ajustarContenido() { alAjustarContenido?() }
    @objc private func ajustarAncho() { alAjustarAncho?() }
    @objc private func ajustarPagina() { alAjustarPagina?() }

    private var espacioSeleccionado: EspacioRapidoLienzoIPad {
        EspacioRapidoLienzoIPad(rawValue: selectorEspacio.selectedSegmentIndex) ?? .cm3
    }

    @objc private func insertarHorizontal() {
        let anchoPixeles = ConversorUnidadLienzoIPad.convertir(
            inicial.ancho, de: inicial.unidad, a: .pixeles)
        alInsertarEspacio?(.horizontal, anchoPixeles / 2,
                           espacioSeleccionado.pixeles)
    }

    @objc private func insertarVertical() {
        let altoPixeles = ConversorUnidadLienzoIPad.convertir(
            inicial.alto, de: inicial.unidad, a: .pixeles)
        alInsertarEspacio?(.vertical, altoPixeles / 2,
                           espacioSeleccionado.pixeles)
    }

    private func tituloSeccion(_ texto: String) -> UILabel {
        let etiqueta = UILabel()
        etiqueta.text = texto
        etiqueta.font = .preferredFont(forTextStyle: .headline)
        etiqueta.textColor = .label
        etiqueta.numberOfLines = 0
        etiqueta.layoutMargins = UIEdgeInsets(top: 12, left: 0, bottom: 0, right: 0)
        return etiqueta
    }

    private func nota(_ texto: String) -> UILabel {
        let etiqueta = UILabel()
        configurarNotaSecundaria(etiqueta)
        return etiqueta
    }

    private func configurarNotaSecundaria(_ etiqueta: UILabel) {
        etiqueta.font = .preferredFont(forTextStyle: .footnote)
        etiqueta.textColor = .secondaryLabel
        etiqueta.numberOfLines = 0
    }

    private func fila(_ titulo: String, _ control: UIView) -> UIView {
        let etiqueta = UILabel()
        etiqueta.text = titulo
        etiqueta.numberOfLines = 0
        let fila = UIStackView(arrangedSubviews: [etiqueta, control])
        fila.axis = .horizontal
        fila.spacing = 12
        fila.alignment = .center
        etiqueta.setContentCompressionResistancePriority(.defaultLow, for: .horizontal)
        control.setContentCompressionResistancePriority(.required, for: .horizontal)
        return fila
    }

    private func filaDimensiones() -> UIView {
        for etiqueta in [etiquetaAncho, etiquetaAlto] {
            etiqueta.font = .preferredFont(forTextStyle: .caption1)
            etiqueta.textColor = .secondaryLabel
        }
        let a = UIStackView(arrangedSubviews: [etiquetaAncho, campoAncho])
        let b = UIStackView(arrangedSubviews: [etiquetaAlto, campoAlto])
        a.axis = .vertical; a.spacing = 4
        b.axis = .vertical; b.spacing = 4
        let fila = UIStackView(arrangedSubviews: [a, b])
        fila.axis = .horizontal
        fila.distribution = .fillEqually
        fila.spacing = 12
        return fila
    }

    private func filaDosCampos(_ tituloA: String, _ campoA: UITextField,
                               _ tituloB: String, _ campoB: UITextField) -> UIView {
        let a = UIStackView(arrangedSubviews: [UILabel(texto: tituloA), campoA])
        let b = UIStackView(arrangedSubviews: [UILabel(texto: tituloB), campoB])
        a.axis = .vertical; a.spacing = 4
        b.axis = .vertical; b.spacing = 4
        let fila = UIStackView(arrangedSubviews: [a, b])
        fila.axis = .horizontal
        fila.distribution = .fillEqually
        fila.spacing = 12
        return fila
    }

    private func boton(_ titulo: String, _ accion: Selector) -> UIButton {
        let resultado = UIButton(type: .system)
        resultado.configuration = .bordered()
        resultado.configuration?.title = titulo
        resultado.addTarget(self, action: accion, for: .touchUpInside)
        return resultado
    }

    private func numero(_ texto: String?) -> Float? {
        guard let texto else { return nil }
        return Float(texto.replacingOccurrences(of: ",", with: "."))
    }

    private func texto(_ numero: Float) -> String { String(format: "%.2f", numero) }

    private func mostrarError(_ mensaje: String) {
        let alerta = UIAlertController(title: "Valor inválido", message: mensaje,
                                       preferredStyle: .alert)
        alerta.addAction(UIAlertAction(title: "Corregir", style: .default))
        present(alerta, animated: true)
    }
}

private extension UITextField {
    static func campoNumerico() -> UITextField {
        let campo = UITextField()
        campo.borderStyle = .roundedRect
        campo.keyboardType = .decimalPad
        campo.textAlignment = .right
        campo.widthAnchor.constraint(greaterThanOrEqualToConstant: 92).isActive = true
        return campo
    }
}

private extension UILabel {
    convenience init(texto: String) {
        self.init()
        text = texto
        font = .preferredFont(forTextStyle: .caption1)
        textColor = .secondaryLabel
    }
}
