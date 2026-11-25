import UIKit

enum AlineacionSeleccionIPad: String, CaseIterable {
    case izquierda = "Izquierda"
    case centroHorizontal = "Centro horizontal"
    case derecha = "Derecha"
    case arriba = "Arriba"
    case centroVertical = "Centro vertical"
    case abajo = "Abajo"
}

enum DistribucionSeleccionIPad: String, CaseIterable {
    case horizontal = "Distribuir horizontalmente"
    case vertical = "Distribuir verticalmente"
}

private struct MetadatoSeleccionPersistente: Codable, Equatable {
    var grupo: String?
    var bloqueado = false
    var etiquetas: [String] = []
    var descripcionAccesible: String?
}

private struct ArchivoMetadatosSeleccion: Codable {
    var formato = 1
    var elementos: [String: MetadatoSeleccionPersistente] = [:]
}

/// Metadatos que no alteran el formato abierto de tinta: grupos, bloqueos,
/// etiquetas y descripciones accesibles se asocian a las identidades estables
/// de cada elemento y sobreviven a reordenamientos de capa.
final class RepositorioMetadatosSeleccionIPad {
    static let compartido = RepositorioMetadatosSeleccionIPad()

    private let cola = DispatchQueue(label: "tinta.seleccion.metadatos")
    private let base: URL
    private var cache: [UInt64: ArchivoMetadatosSeleccion] = [:]

    private init() {
        let soporte = FileManager.default.urls(for: .applicationSupportDirectory,
                                               in: .userDomainMask)[0]
        base = soporte.appendingPathComponent("Tinta/MetadatosSeleccion",
                                              isDirectory: true)
        try? FileManager.default.createDirectory(at: base,
                                                 withIntermediateDirectories: true)
    }

    private func url(_ documento: UInt64) -> URL {
        base.appendingPathComponent("\(documento).json")
    }

    private func cargar(_ documento: UInt64) -> ArchivoMetadatosSeleccion {
        if let valor = cache[documento] { return valor }
        let valor = (try? Data(contentsOf: url(documento)))
            .flatMap { try? JSONDecoder().decode(ArchivoMetadatosSeleccion.self,
                                                  from: $0) }
            ?? ArchivoMetadatosSeleccion()
        cache[documento] = valor
        return valor
    }

    private func guardar(_ archivo: ArchivoMetadatosSeleccion,
                         documento: UInt64) {
        cache[documento] = archivo
        guard let datos = try? JSONEncoder().encode(archivo) else { return }
        try? datos.write(to: url(documento), options: .atomic)
    }

    func claves(de seleccion: SeleccionLienzo, sesion: Sesion) -> [String] {
        var salida: [String] = []
        for indice in seleccion.trazos where indice >= 0 && indice < sesion.cantidadTrazos {
            if let id = sesion.asegurarIdTrazo(indice) { salida.append("t:\(id)") }
        }
        let textos = sesion.textos
        salida += seleccion.textos.compactMap {
            textos.indices.contains($0) ? "x:\(textos[$0].id)" : nil
        }
        let medios = sesion.medios
        salida += seleccion.medios.compactMap {
            medios.indices.contains($0) ? "m:\(medios[$0].id)" : nil
        }
        return salida
    }

    func seleccion(de claves: Set<String>, sesion: Sesion) -> SeleccionLienzo {
        var salida = SeleccionLienzo()
        for indice in 0..<sesion.cantidadTrazos {
            if let id = sesion.idTrazo(indice), claves.contains("t:\(id)") {
                salida.trazos.append(indice)
            }
        }
        for (indice, texto) in sesion.textos.enumerated()
            where claves.contains("x:\(texto.id)") {
            salida.textos.append(indice)
        }
        for (indice, medio) in sesion.medios.enumerated()
            where claves.contains("m:\(medio.id)") {
            salida.medios.append(indice)
        }
        return salida
    }

    func expandirGrupos(de seleccion: SeleccionLienzo, sesion: Sesion) -> SeleccionLienzo {
        let documento = sesion.documentoPersistenteId
        return cola.sync {
            let archivo = cargar(documento)
            let actuales = Set(claves(de: seleccion, sesion: sesion))
            let grupos = Set(actuales.compactMap { archivo.elementos[$0]?.grupo })
            guard !grupos.isEmpty else { return seleccion }
            let ampliadas = Set(archivo.elementos.compactMap { clave, meta in
                meta.grupo.map(grupos.contains) == true ? clave : nil
            })
            return self.seleccion(de: actuales.union(ampliadas), sesion: sesion)
        }
    }

    func filtrarDesbloqueados(_ seleccion: SeleccionLienzo,
                              sesion: Sesion) -> SeleccionLienzo {
        let documento = sesion.documentoPersistenteId
        return cola.sync {
            let archivo = cargar(documento)
            let editables = Set(claves(de: seleccion, sesion: sesion).filter {
                archivo.elementos[$0]?.bloqueado != true
            })
            return self.seleccion(de: editables, sesion: sesion)
        }
    }

    func agrupar(_ seleccion: SeleccionLienzo, sesion: Sesion) {
        let documento = sesion.documentoPersistenteId
        cola.sync {
            var archivo = cargar(documento)
            let grupo = UUID().uuidString
            for clave in claves(de: seleccion, sesion: sesion) {
                var meta = archivo.elementos[clave] ?? MetadatoSeleccionPersistente()
                meta.grupo = grupo
                archivo.elementos[clave] = meta
            }
            guardar(archivo, documento: documento)
        }
    }

    func desagrupar(_ seleccion: SeleccionLienzo, sesion: Sesion) {
        let documento = sesion.documentoPersistenteId
        cola.sync {
            var archivo = cargar(documento)
            let elegidas = claves(de: seleccion, sesion: sesion)
            let grupos = Set(elegidas.compactMap { archivo.elementos[$0]?.grupo })
            for clave in Array(archivo.elementos.keys) {
                guard let grupo = archivo.elementos[clave]?.grupo,
                      grupos.contains(grupo) else { continue }
                archivo.elementos[clave]?.grupo = nil
            }
            guardar(archivo, documento: documento)
        }
    }

    func bloquear(_ seleccion: SeleccionLienzo, sesion: Sesion, valor: Bool) {
        let documento = sesion.documentoPersistenteId
        cola.sync {
            var archivo = cargar(documento)
            for clave in claves(de: seleccion, sesion: sesion) {
                var meta = archivo.elementos[clave] ?? MetadatoSeleccionPersistente()
                meta.bloqueado = valor
                archivo.elementos[clave] = meta
            }
            guardar(archivo, documento: documento)
        }
    }

    func actualizarEtiquetas(_ etiquetas: [String], descripcion: String?,
                             seleccion: SeleccionLienzo, sesion: Sesion) {
        let documento = sesion.documentoPersistenteId
        cola.sync {
            var archivo = cargar(documento)
            let limpias = Array(Set(etiquetas.map {
                $0.trimmingCharacters(in: .whitespacesAndNewlines)
            }.filter { !$0.isEmpty })).sorted()
            for clave in claves(de: seleccion, sesion: sesion) {
                var meta = archivo.elementos[clave] ?? MetadatoSeleccionPersistente()
                meta.etiquetas = limpias
                meta.descripcionAccesible = descripcion?.trimmingCharacters(
                    in: .whitespacesAndNewlines)
                archivo.elementos[clave] = meta
            }
            guardar(archivo, documento: documento)
        }
    }
}

final class PanelAjustesSeleccionIPad: UIViewController {
    private let lienzo: LienzoCuadernU
    private let colores: [UInt32]
    private let forma = UISegmentedControl(items: FormaSeleccionLienzo.allCases.map(\.nombre))
    private let filtro = UIButton(type: .system)
    private let combinacion = UISegmentedControl(
        items: CombinacionSeleccionLienzo.allCases.map(\.nombre))
    private let completo = UISwitch()
    private let filtroColor = UIButton(type: .system)
    private let radio = UISlider()
    private let cuadricula = UISwitch()
    private let paso = UISlider()
    private let etiquetaRadio = UILabel()
    private let etiquetaPaso = UILabel()

    init(lienzo: LienzoCuadernU, colores: [UInt32]) {
        self.lienzo = lienzo
        self.colores = colores
        super.init(nibName: nil, bundle: nil)
        preferredContentSize = CGSize(width: 500, height: 560)
    }

    required init?(coder: NSCoder) { fatalError("init(coder:) no está implementado") }

    override func viewDidLoad() {
        super.viewDidLoad()
        title = "Selección avanzada"
        view.backgroundColor = .systemBackground
        navigationItem.leftBarButtonItem = UIBarButtonItem(
            barButtonSystemItem: .close, target: self, action: #selector(cerrar))

        let ajustes = lienzo.ajustesSeleccion
        forma.selectedSegmentIndex = ajustes.forma.rawValue
        combinacion.selectedSegmentIndex = ajustes.combinacion.rawValue
        completo.isOn = ajustes.requerirCompleto
        cuadricula.isOn = ajustes.ajustarCuadricula
        radio.minimumValue = 4; radio.maximumValue = 160
        radio.value = Float(ajustes.radioPincel)
        paso.minimumValue = 2; paso.maximumValue = 128
        paso.value = Float(ajustes.pasoCuadricula)

        forma.addTarget(self, action: #selector(cambio), for: .valueChanged)
        combinacion.addTarget(self, action: #selector(cambio), for: .valueChanged)
        completo.addTarget(self, action: #selector(cambio), for: .valueChanged)
        cuadricula.addTarget(self, action: #selector(cambio), for: .valueChanged)
        radio.addTarget(self, action: #selector(cambio), for: .valueChanged)
        paso.addTarget(self, action: #selector(cambio), for: .valueChanged)
        filtro.showsMenuAsPrimaryAction = true
        filtroColor.showsMenuAsPrimaryAction = true

        let pila = UIStackView(arrangedSubviews: [
            etiqueta("Forma"), forma,
            etiqueta("Qué seleccionar"), filtro,
            etiqueta("Combinar con la selección actual"), combinacion,
            fila("Exigir elemento completo", completo),
            etiqueta("Color de tinta"), filtroColor,
            etiquetaRadio, radio,
            fila("Ajustar movimiento a cuadrícula", cuadricula),
            etiquetaPaso, paso,
            nota("El filtro de color solo afecta a la tinta. Texto, imagen y PDF conservan su tipo real. Un elemento bloqueado puede seleccionarse para desbloquearlo, pero no se transforma ni se elimina."),
        ])
        pila.axis = .vertical
        pila.spacing = 12
        pila.translatesAutoresizingMaskIntoConstraints = false
        let scroll = UIScrollView()
        scroll.translatesAutoresizingMaskIntoConstraints = false
        view.addSubview(scroll); scroll.addSubview(pila)
        NSLayoutConstraint.activate([
            scroll.leadingAnchor.constraint(equalTo: view.leadingAnchor),
            scroll.trailingAnchor.constraint(equalTo: view.trailingAnchor),
            scroll.topAnchor.constraint(equalTo: view.safeAreaLayoutGuide.topAnchor),
            scroll.bottomAnchor.constraint(equalTo: view.bottomAnchor),
            pila.leadingAnchor.constraint(equalTo: scroll.contentLayoutGuide.leadingAnchor,
                                          constant: 20),
            pila.trailingAnchor.constraint(equalTo: scroll.contentLayoutGuide.trailingAnchor,
                                           constant: -20),
            pila.topAnchor.constraint(equalTo: scroll.contentLayoutGuide.topAnchor,
                                      constant: 20),
            pila.bottomAnchor.constraint(equalTo: scroll.contentLayoutGuide.bottomAnchor,
                                         constant: -20),
            pila.widthAnchor.constraint(equalTo: scroll.frameLayoutGuide.widthAnchor,
                                        constant: -40),
        ])
        reconstruirMenus()
        refrescarEtiquetas()
    }

    private func etiqueta(_ texto: String) -> UILabel {
        let label = UILabel()
        label.text = texto
        label.font = .preferredFont(forTextStyle: .headline)
        return label
    }

    private func nota(_ texto: String) -> UILabel {
        let label = UILabel()
        label.text = texto
        label.numberOfLines = 0
        label.font = .preferredFont(forTextStyle: .footnote)
        label.textColor = .secondaryLabel
        return label
    }

    private func fila(_ texto: String, _ control: UIView) -> UIStackView {
        let label = UILabel(); label.text = texto
        let fila = UIStackView(arrangedSubviews: [label, UIView(), control])
        fila.axis = .horizontal; fila.alignment = .center
        return fila
    }

    private func reconstruirMenus() {
        filtro.menu = UIMenu(children: FiltroSeleccionLienzo.allCases.map { valor in
            UIAction(title: valor.nombre,
                     state: lienzo.ajustesSeleccion.filtro == valor ? .on : .off) {
                [weak self] _ in
                self?.lienzo.ajustesSeleccion.filtro = valor
                self?.reconstruirMenus()
            }
        })
        filtro.setTitle(lienzo.ajustesSeleccion.filtro.nombre, for: .normal)

        var acciones: [UIAction] = [UIAction(
            title: "Cualquier color",
            state: lienzo.ajustesSeleccion.colorARGB == 0 ? .on : .off) {
                [weak self] _ in
                self?.lienzo.ajustesSeleccion.colorARGB = 0
                self?.reconstruirMenus()
            }]
        acciones += colores.enumerated().map { indice, color in
            UIAction(title: "Color \(indice + 1)",
                     image: UIImage(systemName: "circle.fill"),
                     state: lienzo.ajustesSeleccion.colorARGB == color ? .on : .off) {
                [weak self] _ in
                self?.lienzo.ajustesSeleccion.colorARGB = color
                self?.reconstruirMenus()
            }
        }
        filtroColor.menu = UIMenu(children: acciones)
        filtroColor.setTitle(lienzo.ajustesSeleccion.colorARGB == 0
                             ? "Cualquier color" : "Color exacto", for: .normal)
    }

    @objc private func cambio() {
        guard let nuevaForma = FormaSeleccionLienzo(rawValue: forma.selectedSegmentIndex),
              let nuevaCombinacion = CombinacionSeleccionLienzo(
                rawValue: combinacion.selectedSegmentIndex) else { return }
        lienzo.ajustesSeleccion.forma = nuevaForma
        lienzo.ajustesSeleccion.combinacion = nuevaCombinacion
        lienzo.ajustesSeleccion.requerirCompleto = completo.isOn
        lienzo.ajustesSeleccion.radioPincel = CGFloat(radio.value)
        lienzo.ajustesSeleccion.ajustarCuadricula = cuadricula.isOn
        lienzo.ajustesSeleccion.pasoCuadricula = CGFloat(paso.value)
        refrescarEtiquetas()
    }

    private func refrescarEtiquetas() {
        etiquetaRadio.text = "Radio del pincel: \(Int(radio.value.rounded())) pt"
        etiquetaPaso.text = "Paso de cuadrícula: \(Int(paso.value.rounded())) pt"
        radio.isEnabled = forma.selectedSegmentIndex == FormaSeleccionLienzo.pincel.rawValue
        paso.isEnabled = cuadricula.isOn
    }

    @objc private func cerrar() { dismiss(animated: true) }
}

final class PanelEtiquetasSeleccionIPad: UIViewController {
    private let lienzo: LienzoCuadernU
    private let etiquetas = UITextField()
    private let descripcion = UITextView()

    init(lienzo: LienzoCuadernU) {
        self.lienzo = lienzo
        super.init(nibName: nil, bundle: nil)
        preferredContentSize = CGSize(width: 480, height: 360)
    }

    required init?(coder: NSCoder) { fatalError("init(coder:) no está implementado") }

    override func viewDidLoad() {
        super.viewDidLoad()
        title = "Etiquetas y accesibilidad"
        view.backgroundColor = .systemBackground
        etiquetas.placeholder = "Etiquetas separadas por comas"
        etiquetas.borderStyle = .roundedRect
        descripcion.layer.borderWidth = 1
        descripcion.layer.borderColor = UIColor.separator.cgColor
        descripcion.layer.cornerRadius = 8
        descripcion.font = .preferredFont(forTextStyle: .body)
        descripcion.text = "Descripción accesible del contenido seleccionado"
        let guardar = UIButton(type: .system)
        guardar.configuration = .filled()
        guardar.configuration?.title = "Guardar en la selección"
        guardar.addTarget(self, action: #selector(aplicar), for: .touchUpInside)
        let pila = UIStackView(arrangedSubviews: [etiquetas, descripcion, guardar])
        pila.axis = .vertical; pila.spacing = 14
        pila.translatesAutoresizingMaskIntoConstraints = false
        view.addSubview(pila)
        NSLayoutConstraint.activate([
            pila.leadingAnchor.constraint(equalTo: view.leadingAnchor, constant: 20),
            pila.trailingAnchor.constraint(equalTo: view.trailingAnchor, constant: -20),
            pila.topAnchor.constraint(equalTo: view.safeAreaLayoutGuide.topAnchor,
                                      constant: 20),
            pila.bottomAnchor.constraint(equalTo: view.safeAreaLayoutGuide.bottomAnchor,
                                         constant: -20),
            descripcion.heightAnchor.constraint(greaterThanOrEqualToConstant: 160),
        ])
    }

    @objc private func aplicar() {
        let valores = etiquetas.text?.split(separator: ",").map(String.init) ?? []
        RepositorioMetadatosSeleccionIPad.compartido.actualizarEtiquetas(
            valores, descripcion: descripcion.text,
            seleccion: lienzo.seleccionActual, sesion: lienzo.sesion)
        dismiss(animated: true)
    }
}
