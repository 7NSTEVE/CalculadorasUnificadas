import SwiftUI
import UIKit

/// El panel de las Casio dentro del cuaderno.
///
/// Es lo que hace que esto sea **una app** y no dos pegadas: la calculadora no
/// se abre aparte ni hay que salir del apunte para usarla. Cubre los puntos 62
/// y 115 de `MODULO-NOTAS-240-FUNCIONES.md`.
///
/// Aquí no se decide nada: el reparto de la pantalla lo calcula el núcleo C++
/// (`tinta_panel_reparto`), que se prueba en Windows en segundos. Este archivo
/// solo coloca dos vistas en los rectángulos que le dan y traduce el arrastre
/// del divisor. Esa división es deliberada: todo lo que viva aquí es código que
/// no se puede verificar sin un Mac.
///
/// La calculadora es SwiftUI y el lienzo es UIKit + Metal —tiene que serlo, por
/// la latencia—, así que se embebe con `UIHostingController`, que es la forma
/// normal de meter SwiftUI dentro de UIKit.
enum ModoPanelCasio: String, Codable {
    case acoplado
    case flotante
}

struct AjustesPanelCasio {
    var visible = false
    /// La petición principal es usar la calculadora sobre el apunte. El modo
    /// acoplado se conserva como alternativa, pero la primera apertura flota.
    var modo: ModoPanelCasio = .flotante
    var centroFlotanteNormalizado = CGPoint(x: 0.5, y: 0.5)
    var tamanoFlotante = CGSize(width: 320, height: 600)
    
    /// `false` = derecha. Es el lado por defecto: la mano que escribe suele
    /// tapar el lado donde está el lápiz.
    var aLaIzquierda = false
    var fraccion: CGFloat = 0.38
    /// La ClassWiz con su carátula no se lee por debajo de esto.
    var minPt: CGFloat = 320
    var maxFraccion: CGFloat = 2.0 / 3.0
    var anchoMinimoParaDividir: CGFloat = 700

    /// Los cuatro números del reparto, tal como los da el núcleo.
    func reparto(anchoTotal: CGFloat) -> (lienzo: (x: CGFloat, ancho: CGFloat),
                                          panel: (x: CGFloat, ancho: CGFloat)) {
        var ol: Float = 0, al: Float = 0, op: Float = 0, ap: Float = 0
        tinta_panel_reparto(Float(anchoTotal), visible, aLaIzquierda ? 1 : 0,
                            Float(fraccion), Float(minPt), Float(maxFraccion),
                            Float(anchoMinimoParaDividir), &ol, &al, &op, &ap)
        return ((CGFloat(ol), CGFloat(al)), (CGFloat(op), CGFloat(ap)))
    }

    /// En iPhone vertical no se reparte: el panel tapa la pantalla entera.
    func cubreTodo(anchoTotal: CGFloat) -> Bool {
        tinta_panel_cubre_todo(Float(anchoTotal), Float(minPt),
                               Float(maxFraccion), Float(anchoMinimoParaDividir))
    }

    /// Al soltar el divisor. Devuelve 0 si hay que cerrar el panel.
    func fraccionAlSoltar(_ cruda: CGFloat) -> CGFloat {
        CGFloat(tinta_panel_fraccion_al_soltar(Float(cruda), Float(maxFraccion)))
    }
}

/// El contenedor del panel: la calculadora más el divisor que se arrastra.
final class PanelCasio: UIViewController {

    /// Ancho del divisor. Menos de 44 pt y no se agarra con el dedo, que es la
    /// medida mínima de área táctil que recomienda iPadOS.
    private static let anchoDivisor: CGFloat = 44
    private static let altoBarraFlotante: CGFloat = 36

    private let calculadora: UIHostingController<VistaSelector>
    private let divisor = UIView()
    private let agarradero = UIView()
    private let barraFlotante = UIView()
    private let tituloFlotante = UILabel()
    private let botonCerrar = UIButton(type: .system)

    init(centro: CentroCasioCuadernU) {
        self.calculadora = UIHostingController(rootView: VistaSelector(centro: centro))
        super.init(nibName: nil, bundle: nil)
    }

    required init?(coder: NSCoder) {
        fatalError("init(coder:) has not been implemented")
    }

    /// Se llama al soltar el divisor con la fracción ya pegada por el núcleo.
    /// Un 0 significa cerrar.
    var alCambiarFraccion: ((CGFloat) -> Void)?

    /// Cerrar pertenece al contenedor, no a la calculadora. Así el selector y
    /// ambas carátulas comparten exactamente el mismo botón de cierre.
    var alCerrar: (() -> Void)?

    /// El gesto para mover un panel flotante vive únicamente en su barra
    /// superior. Instalarlo sobre toda la calculadora competía con los
    /// `DragGesture` de las teclas y podía convertir un toque rápido en arrastre.
    var alMoverFlotante: ((UIPanGestureRecognizer) -> Void)?

    var modo: ModoPanelCasio = .acoplado {
        didSet {
            guard isViewLoaded else { return }
            actualizarApariencia()
            view.setNeedsLayout()
        }
    }

    /// Lado en el que está el panel; decide de qué borde cuelga el divisor.
    var aLaIzquierda = false { didSet { view.setNeedsLayout() } }

    /// Con el panel cubriendo toda la pantalla no hay nada que repartir, así
    /// que el divisor sobra y se esconde.
    var mostrarDivisor = true {
        didSet {
            guard isViewLoaded else { return }
            actualizarApariencia()
        }
    }

    /// Lo pone quien monta el panel, desde `AjustesPanelCasio`. No se escribe a
    /// mano aquí: tenerlo en dos sitios es garantía de que un día dejen de
    /// coincidir y el divisor se pegue a un sitio distinto del que permite el
    /// reparto.
    var maxFraccion: CGFloat = 2.0 / 3.0

    override func viewDidLoad() {
        super.viewDidLoad()
        view.backgroundColor = .systemBackground

        addChild(calculadora)
        calculadora.view.frame = view.bounds
        calculadora.view.backgroundColor = .clear
        view.addSubview(calculadora.view)
        calculadora.didMove(toParent: self)

        barraFlotante.backgroundColor = .secondarySystemBackground
        barraFlotante.accessibilityLabel = "Mover calculadora flotante"
        barraFlotante.accessibilityHint = "Arrastre esta barra para mover la calculadora"
        barraFlotante.addGestureRecognizer(
            UIPanGestureRecognizer(target: self,
                                   action: #selector(arrastrarFlotante(_:))))
        view.addSubview(barraFlotante)

        tituloFlotante.text = "Calculadora"
        tituloFlotante.font = .systemFont(ofSize: 13, weight: .semibold)
        tituloFlotante.textColor = .secondaryLabel
        tituloFlotante.textAlignment = .center
        barraFlotante.addSubview(tituloFlotante)

        botonCerrar.setImage(UIImage(systemName: "xmark.circle.fill"), for: .normal)
        botonCerrar.tintColor = .secondaryLabel
        botonCerrar.accessibilityLabel = "Cerrar calculadora"
        botonCerrar.addTarget(self, action: #selector(cerrar), for: .touchUpInside)
        barraFlotante.addSubview(botonCerrar)

        divisor.backgroundColor = .clear
        view.addSubview(divisor)

        // La marca visible del divisor: una barrita, como la de las ventanas
        // divididas del sistema.
        agarradero.backgroundColor = .separator
        agarradero.layer.cornerRadius = 2.5
        divisor.addSubview(agarradero)

        divisor.addGestureRecognizer(
            UIPanGestureRecognizer(target: self, action: #selector(arrastrar(_:))))

        actualizarApariencia()
    }

    override func viewDidLayoutSubviews() {
        super.viewDidLayoutSubviews()
        let altoBarra = modo == .flotante ? Self.altoBarraFlotante : 0
        barraFlotante.frame = CGRect(x: 0, y: 0,
                                     width: view.bounds.width,
                                     height: altoBarra)
        tituloFlotante.frame = CGRect(x: 44, y: 0,
                                      width: max(0, barraFlotante.bounds.width - 88),
                                      height: altoBarra)
        botonCerrar.frame = CGRect(x: max(0, barraFlotante.bounds.width - 44),
                                   y: 0, width: 44, height: altoBarra)
        calculadora.view.frame = CGRect(x: 0, y: altoBarra,
                                        width: view.bounds.width,
                                        height: max(0, view.bounds.height - altoBarra))

        let x = aLaIzquierda ? view.bounds.width - Self.anchoDivisor : 0
        divisor.frame = CGRect(x: x, y: 0,
                               width: Self.anchoDivisor, height: view.bounds.height)
        agarradero.frame = CGRect(x: divisor.bounds.midX - 2.5,
                                  y: divisor.bounds.midY - 22,
                                  width: 5, height: 44)
    }

    private func actualizarApariencia() {
        let flotante = modo == .flotante
        barraFlotante.isHidden = !flotante
        view.layer.cornerRadius = flotante ? 14 : 0
        view.layer.cornerCurve = .continuous
        view.layer.borderWidth = flotante ? 1 : 0
        view.layer.borderColor = UIColor.separator.cgColor
        view.clipsToBounds = flotante
        divisor.isHidden = flotante || !mostrarDivisor
    }

    @objc private func cerrar() {
        alCerrar?()
    }

    @objc private func arrastrarFlotante(_ gesto: UIPanGestureRecognizer) {
        guard modo == .flotante else { return }
        alMoverFlotante?(gesto)
    }

    // MARK: - Arrastrar el divisor

    @objc private func arrastrar(_ gesto: UIPanGestureRecognizer) {
        guard let padre = parent?.view else { return }
        let anchoTotal = padre.bounds.width
        guard anchoTotal > 0 else { return }

        let x = gesto.location(in: padre).x
        // Con el panel a la derecha, su ancho es lo que queda del punto al
        // borde; a la izquierda, es el punto mismo.
        let anchoPanel = aLaIzquierda ? x : anchoTotal - x
        let cruda = max(0, min(1, anchoPanel / anchoTotal))

        switch gesto.state {
        case .changed:
            // Mientras se arrastra manda el dedo, sin pegarse: pegarse en vivo
            // hace que el panel salte bajo el dedo y se siente roto.
            alCambiarFraccion?(cruda)
        case .ended, .cancelled:
            alCambiarFraccion?(CGFloat(
                tinta_panel_fraccion_al_soltar(Float(cruda), Float(maxFraccion))))
        default:
            break
        }
    }
}
