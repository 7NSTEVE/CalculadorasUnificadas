import UIKit

final class PanelIngenieria: UIViewController {
    var alInsertarTexto: ((String) -> Void)?
    var alInsertarImagen: ((UIImage, String) -> Void)?
    private let documentoId: UInt64
    private let sesionTinta: Sesion
    private var esquema: EsquemaEditable
    private let vista: VistaEsquemaEditable

    init(documentoId: UInt64, sesion: Sesion) {
        self.documentoId = documentoId; self.sesionTinta = sesion
        esquema = RepositorioEsquemas.compartido.esquema(documento: documentoId)
        vista = VistaEsquemaEditable(esquema: esquema)
        super.init(nibName: nil, bundle: nil)
    }
    required init?(coder: NSCoder) { fatalError("init(coder:) no está implementado") }

    override func viewDidLoad() {
        super.viewDidLoad(); title = "Engineering Toolbox"; view.backgroundColor = .systemBackground
        navigationItem.leftBarButtonItem = UIBarButtonItem(
            barButtonSystemItem: .close, target: self, action: #selector(cerrar))
        let agregar = UIBarButtonItem(title: "Componente", style: .plain, target: nil, action: nil)
        agregar.menu = UIMenu(children: TipoComponenteElectrico.allCases.map { tipo in
            UIAction(title: tipo.nombre) { [weak self] _ in self?.agregar(tipo) }
        })
        let cablear = UIBarButtonItem(title: "Cablear", image: nil,
                                     primaryAction: nil, menu: menuCableado())
        let ensenar = UIBarButtonItem(title: "Enseñar", image: nil,
                                     primaryAction: nil, menu: menuEnsenanza())
        navigationItem.rightBarButtonItems = [agregar, cablear, ensenar, UIBarButtonItem(
            title: "Reconocer tinta", style: .plain, target: self,
            action: #selector(reconocer))]

        vista.translatesAutoresizingMaskIntoConstraints = false
        vista.alCambiar = { [weak self] nuevo in self?.guardar(nuevo) }
        view.addSubview(vista)
        let acciones = UIStackView()
        acciones.axis = .horizontal; acciones.spacing = 8; acciones.distribution = .fillEqually
        acciones.addArrangedSubview(boton("Componentes", #selector(lista)))
        acciones.addArrangedSubview(boton("Análisis", #selector(menuAnalisis(_:))))
        acciones.addArrangedSubview(boton("Thévenin/Norton", #selector(equivalente)))
        acciones.addArrangedSubview(boton("Calculadoras", #selector(toolbox)))
        acciones.addArrangedSubview(boton("Waveform", #selector(ondas)))
        acciones.addArrangedSubview(boton("SPICE", #selector(spice)))
        acciones.translatesAutoresizingMaskIntoConstraints = false
        view.addSubview(acciones)
        NSLayoutConstraint.activate([
            acciones.leadingAnchor.constraint(equalTo: view.leadingAnchor, constant: 12),
            acciones.trailingAnchor.constraint(equalTo: view.trailingAnchor, constant: -12),
            acciones.bottomAnchor.constraint(equalTo: view.safeAreaLayoutGuide.bottomAnchor,
                                              constant: -10),
            acciones.heightAnchor.constraint(equalToConstant: 46),
            vista.topAnchor.constraint(equalTo: view.safeAreaLayoutGuide.topAnchor),
            vista.leadingAnchor.constraint(equalTo: view.leadingAnchor),
            vista.trailingAnchor.constraint(equalTo: view.trailingAnchor),
            vista.bottomAnchor.constraint(equalTo: acciones.topAnchor, constant: -8),
        ])
    }

    private func menuCableado() -> UIMenu {
        let tipos = TipoConexionEsquema.allCases.map { tipo in
            UIAction(title: tipo.rawValue) { [weak self] _ in
                self?.vista.tipoConexion = tipo
                self?.vista.modoCableado = true
            }
        }
        let terminar = UIAction(title: "Terminar cableado",
                                image: UIImage(systemName: "checkmark")) {
            [weak self] _ in self?.vista.modoCableado = false
        }
        let unirCruces = UIAction(title: "Detectar y unir cruces…",
                                  image: UIImage(systemName: "point.3.connected.trianglepath.dotted")) {
            [weak self] _ in self?.confirmarUnionesEnCruces()
        }
        return UIMenu(children: tipos + [unirCruces, terminar])
    }

    private func menuEnsenanza() -> UIMenu {
        let tipos = TipoComponenteElectrico.allCases
        let grupos = stride(from: 0, to: tipos.count, by: 12).map { inicio -> UIMenu in
            let fin = min(inicio + 12, tipos.count)
            let acciones = tipos[inicio..<fin].map { tipo in
                UIAction(title: tipo.nombre) { [weak self] _ in self?.ensenar(tipo) }
            }
            return UIMenu(title: "\(inicio + 1)–\(fin)", children: acciones)
        }
        let administrar = UIAction(
            title: "Administrar ejemplos aprendidos…",
            image: UIImage(systemName: "square.stack.3d.up")) { [weak self] _ in
                self?.abrirPlantillasAprendidas()
            }
        var elementos: [UIMenuElement] = grupos
        elementos.append(administrar)
        return UIMenu(title: "Enseñar símbolo local", children: elementos)
    }

    private func abrirPlantillasAprendidas() {
        navigationController?.pushViewController(
            PanelPlantillasCircuitoIPad(), animated: true)
    }

    private func ensenar(_ tipo: TipoComponenteElectrico) {
        let alerta = UIAlertController(
            title: "Enseñar \(tipo.nombre)",
            message: "Indique cuántos de los últimos trazos forman el símbolo. Solo se guarda un descriptor geométrico normalizado; la tinta original no cambia.",
            preferredStyle: .alert)
        alerta.addTextField { $0.keyboardType = .numberPad; $0.text = "1" }
        alerta.addAction(UIAlertAction(title: "Guardar ejemplo", style: .default) {
            [weak self] _ in
            guard let self,
                  let cantidad = Int(alerta.textFields?.first?.text ?? ""),
                  let descriptor = self.sesionTinta.descriptorCircuitoUltimos(cantidad) else {
                self?.avisar("No se pudo extraer un descriptor de esos trazos."); return
            }
            RepositorioPlantillasCircuitoIPad.compartido.agregar(
                tipo: tipo, caracteristicas: descriptor.caracteristicas)
            let total = RepositorioPlantillasCircuitoIPad.compartido
                .cantidadEjemplos(tipo: tipo)
            DispatchQueue.main.async {
                self.avisar("Ejemplo local guardado para \(tipo.nombre). Total: \(total).")
            }
        })
        alerta.addAction(UIAlertAction(title: "Cancelar", style: .cancel))
        present(alerta, animated: true)
    }

    private func confirmarUnionesEnCruces() {
        var vistaPrevia = esquema
        let cantidad = vistaPrevia.crearUnionesEnCruces()
        guard cantidad > 0 else {
            avisar("No se detectaron cruces unibles. Los puentes y cables curvos no se convierten automáticamente.")
            return
        }
        let alerta = UIAlertController(
            title: "Crear uniones",
            message: "Se detectaron \(cantidad) cruces. Se dividirán los cables y se creará un nodo compartido en cada cruce. El cambio se aplicará solo al confirmar.",
            preferredStyle: .alert)
        alerta.addAction(UIAlertAction(title: "Crear \(cantidad) uniones", style: .default) {
            [weak self] _ in
            guard let self else { return }
            var actualizado = self.esquema
            _ = actualizado.crearUnionesEnCruces()
            self.guardar(actualizado)
        })
        alerta.addAction(UIAlertAction(title: "Cancelar", style: .cancel))
        present(alerta, animated: true)
    }

    private func boton(_ titulo: String, _ selector: Selector) -> UIButton {
        let b = UIButton(type: .system); b.configuration = .bordered()
        b.configuration?.title = titulo; b.titleLabel?.numberOfLines = 2
        b.addTarget(self, action: selector, for: .touchUpInside); return b
    }

    private func agregar(_ tipo: TipoComponenteElectrico) {
        let posicion = CGPoint(x: vista.bounds.midX, y: vista.bounds.midY)
        MotorCircuito.compartido.delegado = self
        MotorCircuito.compartido.insertar(tipo, en: esquema, posicion: posicion) { [weak self] modificado in
            self?.guardar(modificado)
        }
    }

    @objc private func reconocer() {
        let alerta = UIAlertController(title: "Trazos del símbolo",
                                       message: "Indique cuántos de los últimos trazos forman un solo componente. El original queda intacto.",
                                       preferredStyle: .alert)
        alerta.addTextField { $0.keyboardType = .numberPad; $0.text = "1" }
        alerta.addAction(UIAlertAction(title: "Reconocer", style: .default) { [weak self] _ in
            guard let self, let texto = alerta.textFields?.first?.text,
                  let cantidad = Int(texto),
                  let (r, descriptor) = self.sesionTinta
                    .reconocerCircuitoEntrenableUltimos(cantidad) else {
                self?.avisar("No hay un candidato fiable. Use Enseñar para guardar un ejemplo confirmado.")
                return
            }
            let confirmacion = UIAlertController(
                title: "Candidato: \(r.tipo.nombre)",
                message: "Confianza \(Int(r.confianza * 100)) %. El manuscrito original permanece intacto. Solo se crea el objeto si usted confirma.",
                preferredStyle: .alert)
            confirmacion.addAction(UIAlertAction(title: "Crear objeto", style: .default) {
                [weak self] _ in self?.crear(r)
            })
            confirmacion.addAction(UIAlertAction(title: "Crear y aprender este ejemplo",
                                                  style: .default) { [weak self] _ in
                RepositorioPlantillasCircuitoIPad.compartido.agregar(
                    tipo: r.tipo, caracteristicas: descriptor.caracteristicas)
                self?.crear(r)
            })
            confirmacion.addAction(UIAlertAction(title: "No es correcto", style: .cancel))
            DispatchQueue.main.async { [weak self] in
                self?.present(confirmacion, animated: true)
            }
        })
        alerta.addAction(UIAlertAction(title: "Cancelar", style: .cancel))
        present(alerta, animated: true)
    }

    private func crear(_ reconocimiento: ReconocimientoCircuito) {
        let centro = CGPoint(x: (reconocimiento.terminalA.x + reconocimiento.terminalB.x) / 2,
                             y: (reconocimiento.terminalA.y + reconocimiento.terminalB.y) / 2)
        esquema.agregar(reconocimiento.tipo, posicion: centro,
                        terminalA: reconocimiento.terminalA,
                        terminalB: reconocimiento.terminalB,
                        confianza: reconocimiento.confianza)
        guardar(esquema)
        avisar("Objeto creado y reconocimiento congelado. Confirme valor, nodos y orientación.")
    }

    @objc private func lista() {
        let lista = ListaComponentes(esquema: esquema)
        lista.alCambiar = { [weak self] nuevo in self?.guardar(nuevo) }
        navigationController?.pushViewController(lista, animated: true)
    }

    @objc private func menuAnalisis(_ remitente: UIButton) {
        let menu = UIAlertController(
            title: "Análisis del circuito",
            message: "Los dispositivos sin modelo detienen el cálculo; nunca se omiten para producir un resultado parcial.",
            preferredStyle: .actionSheet)
        menu.addAction(UIAlertAction(title: "Punto de operación DC", style: .default) {
            [weak self] _ in self?.simularDC()
        })
        menu.addAction(UIAlertAction(title: "Punto AC", style: .default) {
            [weak self] _ in self?.simularAC()
        })
        menu.addAction(UIAlertAction(title: "Barrido DC", style: .default) {
            [weak self] _ in self?.configurarBarridoDC()
        })
        menu.addAction(UIAlertAction(title: "Transitorio", style: .default) {
            [weak self] _ in self?.configurarTransitorio(escalon: false)
        })
        menu.addAction(UIAlertAction(title: "Respuesta escalón", style: .default) {
            [weak self] _ in self?.configurarTransitorio(escalon: true)
        })
        menu.addAction(UIAlertAction(title: "Barrido AC · magnitud y fase", style: .default) {
            [weak self] _ in self?.configurarBarridoAC(bode: false)
        })
        menu.addAction(UIAlertAction(title: "Diagrama de Bode · dB y fase", style: .default) {
            [weak self] _ in self?.configurarBarridoAC(bode: true)
        })
        menu.addAction(UIAlertAction(title: "Topología y diagnóstico", style: .default) {
            [weak self] _ in self?.abrirDiagnostico()
        })
        menu.addAction(UIAlertAction(title: "Solve Mode · proceso guiado", style: .default) {
            [weak self] _ in self?.abrirModoResolucion()
        })
        menu.addAction(UIAlertAction(title: "Cancelar", style: .cancel))
        menu.popoverPresentationController?.sourceView = remitente
        menu.popoverPresentationController?.sourceRect = remitente.bounds
        present(menu, animated: true)
    }

    private func abrirDiagnostico() {
        let panel = PanelDiagnosticoCircuitoIPad(esquema: esquema)
        panel.alInsertarTexto = alInsertarTexto
        navigationController?.pushViewController(panel, animated: true)
    }

    private func abrirModoResolucion() {
        let panel = PanelResolucionCircuitoIPad(
            documentoId: documentoId, esquema: esquema)
        panel.alInsertarTexto = alInsertarTexto
        navigationController?.pushViewController(panel, animated: true)
    }

    @objc private func simularDC() {
        let r = MotorCircuitoLocal.dc(esquema)
        guard let error = r.error else {
            avisar(r.voltajes.enumerated().map { "V(\($0.offset)) = \(NotacionIngenieria.formatear($0.element, unidad: "V"))" }
                .joined(separator: "\n")); return
        }
        avisar("No se simuló: \(error)")
    }

    @objc private func simularAC() {
        pedirNumero(titulo: "Frecuencia AC", valor: "1000") { [weak self] frecuencia in
            guard let self else { return }
            let r = MotorCircuitoLocal.ac(self.esquema, frecuencia: frecuencia)
            if let error = r.error { self.avisar("No se simuló: \(error)"); return }
            self.avisar(r.voltajes.enumerated().map {
                "V(\($0.offset)) = \(NotacionIngenieria.formatear($0.element.magnitud, unidad: "V")) ∠ \(String(format: "%.2f", $0.element.fase))°"
            }.joined(separator: "\n"))
        }
    }

    private func configurarBarridoDC() {
        let fuentes = esquema.componentes.enumerated().filter {
            $0.element.activo && ($0.element.tipo == .fuenteVoltaje ||
                                  $0.element.tipo == .fuenteCorriente)
        }
        guard !fuentes.isEmpty else {
            avisar("Agregue una fuente independiente de voltaje o corriente con valor confirmado.")
            return
        }
        let referencias = fuentes.map { $0.element.referencia }.joined(separator: ", ")
        let alerta = UIAlertController(
            title: "Barrido DC",
            message: "Fuentes disponibles: \(referencias)", preferredStyle: .alert)
        agregarCampos(alerta, [fuentes[0].element.referencia, "0", "5", "51", "1"])
        alerta.addAction(UIAlertAction(title: "Calcular", style: .default) {
            [weak self] _ in
            guard let self,
                  let indice = self.indiceFuente(alerta.textFields?[0].text,
                                                 candidatos: fuentes.map { $0.offset }),
                  let inicio = self.numero(alerta.textFields?[1].text),
                  let fin = self.numero(alerta.textFields?[2].text),
                  let puntos = Int(alerta.textFields?[3].text ?? ""),
                  let nodo = Int(alerta.textFields?[4].text ?? "") else {
                self?.avisar("Fuente o parámetros no válidos."); return
            }
            let resultado = MotorCircuitoLocal.barridoDC(
                self.esquema, indiceFuente: indice, inicio: inicio, fin: fin,
                puntos: puntos, nodo: nodo)
            self.mostrar(resultado, titulo: "Barrido DC · V(\(nodo))",
                         modo: .lineal(unidadX: "entrada", unidadY: "V"))
        })
        alerta.addAction(UIAlertAction(title: "Cancelar", style: .cancel))
        present(alerta, animated: true)
    }

    private func configurarBarridoAC(bode: Bool) {
        let alerta = UIAlertController(
            title: bode ? "Diagrama de Bode" : "Barrido AC",
            message: bode
                ? "Frecuencia inicial, final, puntos por década, nodo de salida y amplitud Vin usada como referencia."
                : "Frecuencia inicial, final, puntos por década y nodo de salida.",
            preferredStyle: .alert)
        agregarCampos(alerta, bode
                      ? ["10 Hz", "1Meg Hz", "24", "1", "1 V"]
                      : ["10 Hz", "1Meg Hz", "24", "1"])
        alerta.addAction(UIAlertAction(title: "Calcular", style: .default) {
            [weak self] _ in
            guard let self,
                  let inicio = self.numero(alerta.textFields?[0].text),
                  let fin = self.numero(alerta.textFields?[1].text),
                  let ppd = Int(alerta.textFields?[2].text ?? ""),
                  let nodo = Int(alerta.textFields?[3].text ?? "") else {
                self?.avisar("Parámetros no válidos."); return
            }
            let referencia: Double
            if bode {
                guard let valor = self.numero(alerta.textFields?[4].text), valor > 0 else {
                    self.avisar("Vin de referencia debe ser positivo."); return
                }
                referencia = valor
            } else {
                referencia = 1
            }
            let resultado = MotorCircuitoLocal.barridoAC(
                self.esquema, inicioHz: inicio, finHz: fin,
                puntosPorDecada: ppd, nodo: nodo)
            self.mostrar(resultado,
                         titulo: "\(bode ? "Bode" : "Barrido AC") · V(\(nodo))",
                         modo: bode ? .bode(referencia: referencia) : .magnitudFase)
        })
        alerta.addAction(UIAlertAction(title: "Cancelar", style: .cancel))
        present(alerta, animated: true)
    }

    private func configurarTransitorio(escalon: Bool) {
        let fuentes = esquema.componentes.enumerated().filter {
            $0.element.activo && ($0.element.tipo == .fuenteVoltaje ||
                                  $0.element.tipo == .fuenteCorriente ||
                                  $0.element.tipo == .bateria)
        }
        if escalon && fuentes.isEmpty {
            avisar("La respuesta escalón requiere una fuente DC o batería.")
            return
        }
        let alerta = UIAlertController(
            title: escalon ? "Respuesta escalón" : "Análisis transitorio",
            message: escalon
                ? "Fuente, amplitud final, duración, paso y nodo de salida. La energía inicial de C/L es cero."
                : "Duración, paso y nodo de salida. La energía inicial de C/L es cero.",
            preferredStyle: .alert)
        if escalon {
            agregarCampos(alerta, [fuentes[0].element.referencia, "5 V", "100 ms",
                                    "0.5 ms", "1"])
        } else {
            agregarCampos(alerta, ["100 ms", "0.5 ms", "1"])
        }
        alerta.addAction(UIAlertAction(title: "Calcular", style: .default) {
            [weak self] _ in
            guard let self else { return }
            var circuito = self.esquema
            let desplazamiento = escalon ? 2 : 0
            if escalon {
                guard let indice = self.indiceFuente(
                        alerta.textFields?[0].text,
                        candidatos: fuentes.map { $0.offset }),
                      let amplitud = self.numero(alerta.textFields?[1].text),
                      amplitud > 0 else {
                    self.avisar("Fuente o amplitud no válida."); return
                }
                circuito.componentes[indice].valor = amplitud
            }
            guard let duracion = self.numero(alerta.textFields?[desplazamiento].text),
                  let paso = self.numero(alerta.textFields?[desplazamiento + 1].text),
                  let nodo = Int(alerta.textFields?[desplazamiento + 2].text ?? "") else {
                self.avisar("Duración, paso o nodo no válido."); return
            }
            let resultado = MotorCircuitoLocal.transitorio(
                circuito, duracion: duracion, paso: paso, nodo: nodo)
            self.mostrar(resultado,
                         titulo: "\(escalon ? "Respuesta escalón" : "Transitorio") · V(\(nodo))",
                         modo: .lineal(unidadX: "s", unidadY: "V"))
        })
        alerta.addAction(UIAlertAction(title: "Cancelar", style: .cancel))
        present(alerta, animated: true)
    }

    private func mostrar(_ resultado: ResultadoAnalisisCircuitoIPad, titulo: String,
                         modo: ModoGraficaCircuitoIPad) {
        if let error = resultado.error { avisar("No se simuló: \(error)"); return }
        guard !resultado.muestras.isEmpty else {
            avisar("El análisis no produjo muestras."); return
        }
        let panel = ControladorResultadoCircuitoIPad(
            titulo: titulo, muestras: resultado.muestras, modo: modo)
        panel.alInsertarImagen = alInsertarImagen
        panel.alInsertarTexto = alInsertarTexto
        navigationController?.pushViewController(panel, animated: true)
    }

    private func agregarCampos(_ alerta: UIAlertController, _ valores: [String]) {
        for valor in valores {
            alerta.addTextField { campo in
                campo.text = valor
                campo.keyboardType = .numbersAndPunctuation
            }
        }
    }

    private func numero(_ texto: String?) -> Double? {
        NotacionIngenieria.interpretar(texto ?? "")
    }

    private func indiceFuente(_ texto: String?, candidatos: [Int]) -> Int? {
        let buscado = (texto ?? "").trimmingCharacters(in: .whitespacesAndNewlines)
        if let indice = Int(buscado), candidatos.contains(indice) { return indice }
        return candidatos.first {
            esquema.componentes[$0].referencia.caseInsensitiveCompare(buscado) == .orderedSame
        }
    }

    @objc private func equivalente() {
        let a = UIAlertController(title: "Puerto Thévenin/Norton",
                                  message: "Nodos A y B separados por coma", preferredStyle: .alert)
        a.addTextField { $0.text = "1,0" }
        a.addAction(UIAlertAction(title: "Calcular", style: .default) { [weak self] _ in
            guard let self else { return }
            let valores = a.textFields?.first?.text?.split(separator: ",").compactMap {
                Int($0.trimmingCharacters(in: .whitespaces))
            } ?? []
            guard valores.count == 2 else { return }
            let r = MotorCircuitoLocal.equivalente(self.esquema, a: valores[0], b: valores[1])
            if let error = r.error { self.avisar(error) }
            else { self.avisar("Vth = \(NotacionIngenieria.formatear(r.v, unidad: "V"))\nRth = \(NotacionIngenieria.formatear(r.r, unidad: "Ω"))\nIn = \(NotacionIngenieria.formatear(r.i, unidad: "A"))") }
        })
        a.addAction(UIAlertAction(title: "Cancelar", style: .cancel)); present(a, animated: true)
    }

    @objc private func toolbox() {
        let controlador = ControladorToolbox()
        controlador.alInsertar = alInsertarTexto
        navigationController?.pushViewController(controlador, animated: true)
    }

    @objc private func ondas() {
        let controlador = ControladorFormaOnda()
        controlador.alInsertar = alInsertarImagen
        navigationController?.pushViewController(controlador, animated: true)
    }

    @objc private func spice() {
        let texto = esquema.spice()
        let hoja = UIAlertController(title: "Netlist SPICE", message: texto,
                                     preferredStyle: .actionSheet)
        hoja.addAction(UIAlertAction(title: "Insertar en el apunte", style: .default) {
            [weak self] _ in self?.alInsertarTexto?(texto)
        })
        hoja.addAction(UIAlertAction(title: "Copiar", style: .default) { _ in
            UIPasteboard.general.string = texto
        })
        hoja.addAction(UIAlertAction(title: "Cancelar", style: .cancel))
        hoja.popoverPresentationController?.sourceView = view; present(hoja, animated: true)
    }

    private func guardar(_ nuevo: EsquemaEditable) {
        esquema = nuevo; vista.esquema = nuevo
        RepositorioEsquemas.compartido.guardar(nuevo, documento: documentoId)
    }

    private func pedirNumero(titulo: String, valor: String,
                             completado: @escaping (Double) -> Void) {
        let a = UIAlertController(title: titulo, message: nil, preferredStyle: .alert)
        a.addTextField { $0.text = valor; $0.keyboardType = .decimalPad }
        a.addAction(UIAlertAction(title: "Aceptar", style: .default) { _ in
            if let texto = a.textFields?.first?.text?.replacingOccurrences(of: ",", with: "."),
               let numero = Double(texto) { completado(numero) }
        }); a.addAction(UIAlertAction(title: "Cancelar", style: .cancel)); present(a, animated: true)
    }

    private func avisar(_ texto: String) {
        let a = UIAlertController(title: nil, message: texto, preferredStyle: .alert)
        a.addAction(UIAlertAction(title: "Aceptar", style: .default)); present(a, animated: true)
    }
    @objc private func cerrar() { dismiss(animated: true) }
}

extension PanelIngenieria: DelegadoInteraccionMotor {
    public func solicitarTexto(titulo: String, mensaje: String, placeholder: String, completado: @escaping (String?) -> Void) {
        let alerta = UIAlertController(title: titulo, message: mensaje, preferredStyle: .alert)
        alerta.addTextField {
            $0.placeholder = placeholder
            $0.keyboardType = .numbersAndPunctuation
        }
        alerta.addAction(UIAlertAction(title: "Crear", style: .default) { _ in
            completado(alerta.textFields?.first?.text)
        })
        alerta.addAction(UIAlertAction(title: "Cancelar", style: .cancel) { _ in
            completado(nil)
        })
        present(alerta, animated: true)
    }
    
    public func mostrarMensaje(_ mensaje: String) {
        avisar(mensaje)
    }
}

final class PanelPlantillasCircuitoIPad: UITableViewController {
    private let tipos = TipoComponenteElectrico.allCases
    private let repositorio = RepositorioPlantillasCircuitoIPad.compartido

    init() { super.init(style: .insetGrouped) }
    required init?(coder: NSCoder) { fatalError("init(coder:) no está implementado") }

    override func viewDidLoad() {
        super.viewDidLoad()
        title = "Símbolos aprendidos"
    }

    override func numberOfSections(in tableView: UITableView) -> Int { 1 }

    override func tableView(_ tableView: UITableView,
                            numberOfRowsInSection section: Int) -> Int {
        tipos.count
    }

    override func tableView(_ tableView: UITableView,
                            titleForHeaderInSection section: Int) -> String? {
        "Ejemplos locales confirmados"
    }

    override func tableView(_ tableView: UITableView,
                            titleForFooterInSection section: Int) -> String? {
        "Cada símbolo admite hasta 24 ejemplos. Eliminar ejemplos no modifica la tinta ni los objetos ya creados."
    }

    override func tableView(_ tableView: UITableView,
                            cellForRowAt indexPath: IndexPath) -> UITableViewCell {
        let celda = UITableViewCell(style: .value1, reuseIdentifier: nil)
        let tipo = tipos[indexPath.row]
        let cantidad = repositorio.cantidadEjemplos(tipo: tipo)
        celda.textLabel?.text = tipo.nombre
        celda.detailTextLabel?.text = cantidad == 1 ? "1 ejemplo" : "\(cantidad) ejemplos"
        celda.accessoryType = cantidad > 0 ? .disclosureIndicator : .none
        celda.selectionStyle = cantidad > 0 ? .default : .none
        return celda
    }

    override func tableView(_ tableView: UITableView, didSelectRowAt indexPath: IndexPath) {
        tableView.deselectRow(at: indexPath, animated: true)
        let tipo = tipos[indexPath.row]
        let cantidad = repositorio.cantidadEjemplos(tipo: tipo)
        guard cantidad > 0 else { return }
        let alerta = UIAlertController(
            title: "Eliminar ejemplos de \(tipo.nombre)",
            message: "Se eliminarán los \(cantidad) ejemplos aprendidos localmente para este símbolo. Esta acción no borra tinta ni componentes.",
            preferredStyle: .alert)
        alerta.addAction(UIAlertAction(title: "Eliminar ejemplos", style: .destructive) {
            [weak self] _ in
            self?.repositorio.eliminarEjemplos(tipo: tipo)
            self?.tableView.reloadRows(at: [indexPath], with: .automatic)
        })
        alerta.addAction(UIAlertAction(title: "Cancelar", style: .cancel))
        present(alerta, animated: true)
    }
}

final class ListaComponentes: UITableViewController {
    var alCambiar: ((EsquemaEditable) -> Void)?
    private var esquema: EsquemaEditable
    init(esquema: EsquemaEditable) { self.esquema = esquema; super.init(style: .insetGrouped) }
    required init?(coder: NSCoder) { fatalError("init(coder:) no está implementado") }
    override func viewDidLoad() { super.viewDidLoad(); title = "Componentes y valores" }
    override func numberOfSections(in tableView: UITableView) -> Int { 2 }
    override func tableView(_ tableView: UITableView,
                            titleForHeaderInSection section: Int) -> String? {
        section == 0 ? "Componentes" : "Cables, buses y puentes"
    }
    override func tableView(_ tableView: UITableView, numberOfRowsInSection section: Int) -> Int {
        section == 0 ? esquema.componentes.count : (esquema.conexiones ?? []).count
    }
    override func tableView(_ tableView: UITableView, cellForRowAt indexPath: IndexPath)
        -> UITableViewCell {
        let celda = UITableViewCell(style: .subtitle, reuseIdentifier: nil)
        if indexPath.section == 0 {
            let c = esquema.componentes[indexPath.row]
            celda.textLabel?.text = "\(c.referencia) · \(c.tipo.nombre)"
            let valor = c.valor > 0 ? NotacionIngenieria.formatear(c.valor)
                : "sin valor confirmado"
            let orientacion = Int((c.rotacion * 180 / .pi).rounded())
            celda.detailTextLabel?.text = "nodos \(c.nodoA)–\(c.nodoB) · \(valor) · "
                + "\(c.estandarSimbolo?.rawValue ?? "ANSI") · \(orientacion)°"
                + (c.espejado == true ? " · volteado" : "")
                + (c.activo ? "" : " · inactivo")
        } else {
            let conexion = (esquema.conexiones ?? [])[indexPath.row]
            celda.textLabel?.text = "\(conexion.tipo.rawValue) · nodos \(conexion.nodoA)–\(conexion.nodoB)"
            celda.detailTextLabel?.text = [conexion.etiqueta, conexion.corriente,
                                           conexion.polaridadA]
                .filter { !$0.isEmpty }.joined(separator: " · ")
        }
        celda.accessoryType = .disclosureIndicator; return celda
    }
    override func tableView(_ tableView: UITableView, didSelectRowAt indexPath: IndexPath) {
        if indexPath.section == 1 { editarConexion(indexPath.row); return }
        let c = esquema.componentes[indexPath.row]
        let a = UIAlertController(title: c.referencia, message: c.tipo.nombre, preferredStyle: .alert)
        a.addTextField { $0.text = String(c.valor); $0.keyboardType = .numbersAndPunctuation }
        a.addTextField { $0.text = "\(c.nodoA),\(c.nodoB)" }
        a.addTextField { $0.text = c.referencia; $0.placeholder = "Referencia manual" }
        a.addTextField { $0.text = c.estandarSimbolo?.rawValue ?? "ANSI"
            $0.placeholder = "ANSI o IEC" }
        a.addAction(UIAlertAction(title: "Guardar", style: .default) { [weak self] _ in
            guard let self else { return }
            if let valor = NotacionIngenieria.interpretar(a.textFields?[0].text ?? "") {
                self.esquema.componentes[indexPath.row].valor = valor
            }
            let nodos = a.textFields?[1].text?.split(separator: ",").compactMap { Int($0) } ?? []
            if nodos.count == 2 { self.esquema.componentes[indexPath.row].nodoA = nodos[0]
                self.esquema.componentes[indexPath.row].nodoB = nodos[1] }
            let referencia = a.textFields?[2].text?.trimmingCharacters(in: .whitespacesAndNewlines)
                ?? ""
            if !referencia.isEmpty { self.esquema.componentes[indexPath.row].referencia = referencia }
            let estandar = (a.textFields?[3].text ?? "").uppercased()
            self.esquema.componentes[indexPath.row].estandarSimbolo =
                estandar.contains("IEC") ? .iec : .ansi
            self.alCambiar?(self.esquema); self.tableView.reloadData()
        })
        a.addAction(UIAlertAction(title: "Rotar 90°", style: .default) { [weak self] _ in
            guard let self else { return }
            let actual = self.esquema.componentes[indexPath.row].rotacion
            self.esquema.componentes[indexPath.row].rotacion =
                (actual + .pi / 2).truncatingRemainder(dividingBy: 2 * .pi)
            self.alCambiar?(self.esquema); self.tableView.reloadData()
        })
        a.addAction(UIAlertAction(title: "Voltear horizontalmente", style: .default) {
            [weak self] _ in
            guard let self else { return }
            let actual = self.esquema.componentes[indexPath.row].espejado == true
            self.esquema.componentes[indexPath.row].espejado = !actual
            self.alCambiar?(self.esquema); self.tableView.reloadData()
        })
        a.addAction(UIAlertAction(title: c.activo ? "Desactivar" : "Activar",
                                  style: .default) { [weak self] _ in
            guard let self else { return }
            self.esquema.componentes[indexPath.row].activo.toggle()
            self.alCambiar?(self.esquema); self.tableView.reloadData()
        })
        a.addAction(UIAlertAction(title: "Eliminar", style: .destructive) { [weak self] _ in
            self?.esquema.componentes.remove(at: indexPath.row)
            if let esquema = self?.esquema { self?.alCambiar?(esquema) }
            self?.tableView.reloadData()
        })
        a.addAction(UIAlertAction(title: "Cancelar", style: .cancel)); present(a, animated: true)
    }

    private func editarConexion(_ indice: Int) {
        guard var lista = esquema.conexiones, lista.indices.contains(indice) else { return }
        let actual = lista[indice]
        let alerta = UIAlertController(
            title: "Nodos \(actual.nodoA)–\(actual.nodoB)",
            message: "Tipo: cable ortogonal, libre, curvo, puente o bus. Corriente dibuja una flecha; la polaridad es solo una etiqueta.",
            preferredStyle: .alert)
        alerta.addTextField { $0.placeholder = "Tipo"; $0.text = actual.tipo.rawValue }
        alerta.addTextField { $0.placeholder = "Etiqueta de nodo/bus"; $0.text = actual.etiqueta }
        alerta.addTextField { $0.placeholder = "Corriente, ej. I1"; $0.text = actual.corriente }
        alerta.addTextField { $0.placeholder = "Polaridad, ej. + / −"; $0.text = actual.polaridadA }
        alerta.addAction(UIAlertAction(title: "Guardar", style: .default) {
            [weak self] _ in
            let tipoTexto = Normalizador.texto(alerta.textFields?[0].text ?? "")
            if tipoTexto.contains("puente") { lista[indice].tipo = .puente }
            else if tipoTexto.contains("bus") { lista[indice].tipo = .bus }
            else if tipoTexto.contains("curv") { lista[indice].tipo = .curva }
            else if tipoTexto.contains("libre") { lista[indice].tipo = .libre }
            else { lista[indice].tipo = .conductor }
            lista[indice].etiqueta = alerta.textFields?[1].text ?? ""
            lista[indice].corriente = alerta.textFields?[2].text ?? ""
            lista[indice].polaridadA = alerta.textFields?[3].text ?? ""
            self?.esquema.conexiones = lista
            if let esquema = self?.esquema { self?.alCambiar?(esquema) }
            self?.tableView.reloadData()
        })
        alerta.addAction(UIAlertAction(title: "Eliminar", style: .destructive) {
            [weak self] _ in
            lista.remove(at: indice); self?.esquema.conexiones = lista
            if let esquema = self?.esquema { self?.alCambiar?(esquema) }
            self?.tableView.reloadData()
        })
        alerta.addAction(UIAlertAction(title: "Cancelar", style: .cancel))
        present(alerta, animated: true)
    }
}

enum HerramientaIngenieria: String, CaseIterable {
    case leyOhm = "Ley de Ohm V=IR", potencia = "Potencia P=VI"
    case divisor = "Divisor de voltaje", opampNoInv = "Op-amp no inversor"
    case opampInv = "Op-amp inversor", corteRC = "Frecuencia de corte RC"
    case resonanciaLC = "Resonancia LC", constanteRC = "Constante de tiempo RC"
    case dbVoltaje = "Relación a dB", desdeDB = "dB a relación"
    case resistorLed = "Resistor para LED", astable555 = "555 astable"
    case mono555 = "555 monoestable", adc = "Resolución ADC"
    case bateria = "Energía de batería", reactanciaC = "Reactancia capacitiva"
    case reactanciaL = "Reactancia inductiva", paralelo = "Resistencias en paralelo"
    case divisorCorriente = "Divisor de corriente"
    case resistoresSerie = "Resistencias en serie"
    case capacitoresSerie = "Capacitores en serie"
    case capacitoresParalelo = "Capacitores en paralelo"
    case inductoresSerie = "Inductores en serie"
    case inductoresParalelo = "Inductores en paralelo"
    case corteRL = "Frecuencia de corte RL"
    case impedanciaRLC = "Impedancia serie RLC · magnitud"
    case faseRLC = "Impedancia serie RLC · fase"
    case gananciaVoltaje = "Ganancia de voltaje"
    case dbPotencia = "Relación de potencia a dB"
    case rmsSeno = "Pico de seno a RMS"
    case factorPotencia = "Factor de potencia"
    case dutyPWM = "Duty cycle PWM"
    case promedioPWM = "Voltaje promedio PWM"
    case polarizacionBJT = "Corriente BJT por polarización de base"
    case energiaCapacitor = "Energía de capacitor"
    case energiaInductor = "Energía de inductor"
    case relacionTransformador = "Relación de transformador"
    case inductanciaDesdeReactancia = "Inductancia desde reactancia"
    case capacitanciaDesdeReactancia = "Capacitancia desde reactancia"
    case nortonDesdeThevenin = "Norton desde Thévenin"
    case maximaPotencia = "Máxima transferencia de potencia"
    case superposicionTres = "Sumar tres contribuciones"

    var entradas: [String] {
        switch self {
        case .leyOhm: return ["Corriente A", "Resistencia Ω"]
        case .potencia: return ["Voltaje V", "Corriente A"]
        case .divisor: return ["Vin V", "R superior Ω", "R inferior Ω"]
        case .opampNoInv: return ["Rf Ω", "Rg Ω"]
        case .opampInv: return ["Rin Ω", "Rf Ω"]
        case .corteRC: return ["R Ω", "C F"]
        case .resonanciaLC: return ["L H", "C F"]
        case .constanteRC: return ["R Ω", "C F"]
        case .dbVoltaje: return ["Relación"]
        case .desdeDB: return ["dB"]
        case .resistorLed: return ["Fuente V", "Vf LED", "Corriente A"]
        case .astable555: return ["R1 Ω", "R2 Ω", "C F"]
        case .mono555: return ["R Ω", "C F"]
        case .adc: return ["Vref V", "Bits"]
        case .bateria: return ["Voltaje V", "Capacidad Ah"]
        case .reactanciaC: return ["Frecuencia Hz", "C F"]
        case .reactanciaL: return ["Frecuencia Hz", "L H"]
        case .paralelo: return ["R1 Ω", "R2 Ω"]
        case .divisorCorriente: return ["Corriente total A", "Rama propia Ω", "Otra rama Ω"]
        case .resistoresSerie: return ["R1 Ω", "R2 Ω"]
        case .capacitoresSerie: return ["C1 F", "C2 F"]
        case .capacitoresParalelo: return ["C1 F", "C2 F"]
        case .inductoresSerie: return ["L1 H", "L2 H"]
        case .inductoresParalelo: return ["L1 H", "L2 H"]
        case .corteRL: return ["R Ω", "L H"]
        case .impedanciaRLC, .faseRLC:
            return ["R Ω", "L H", "C F", "Frecuencia Hz"]
        case .gananciaVoltaje: return ["Vout", "Vin"]
        case .dbPotencia: return ["Relación de potencias"]
        case .rmsSeno: return ["Valor pico"]
        case .factorPotencia: return ["Potencia real W", "Vrms V", "Irms A"]
        case .dutyPWM: return ["Tiempo encendido s", "Período s"]
        case .promedioPWM: return ["Voltaje alto V", "Duty %"]
        case .polarizacionBJT: return ["Vcc V", "Vbe V", "Rbase Ω", "Beta"]
        case .energiaCapacitor: return ["C F", "Voltaje V"]
        case .energiaInductor: return ["L H", "Corriente A"]
        case .relacionTransformador: return ["V primario", "Vueltas primario", "Vueltas secundario"]
        case .inductanciaDesdeReactancia: return ["Reactancia Ω", "Frecuencia Hz"]
        case .capacitanciaDesdeReactancia: return ["Reactancia Ω", "Frecuencia Hz"]
        case .nortonDesdeThevenin: return ["Vth V", "Rth Ω"]
        case .maximaPotencia: return ["Vth V", "Rth Ω"]
        case .superposicionTres: return ["Contribución 1", "Contribución 2", "Contribución 3"]
        }
    }
    func calcular(_ x: [Double]) -> (Double, String) {
        switch self {
        case .leyOhm: return (x[0] * x[1], "V")
        case .potencia: return (x[0] * x[1], "W")
        case .divisor: return (tinta_toolbox_divisor(x[0], x[1], x[2]), "V")
        case .opampNoInv: return (tinta_toolbox_opamp_no_inversor(x[0], x[1]), "V/V")
        case .opampInv: return (tinta_toolbox_opamp_inversor(x[0], x[1]), "V/V")
        case .corteRC: return (tinta_toolbox_corte_rc(x[0], x[1]), "Hz")
        case .resonanciaLC: return (tinta_toolbox_resonancia_lc(x[0], x[1]), "Hz")
        case .constanteRC: return (tinta_toolbox_constante_rc(x[0], x[1]), "s")
        case .dbVoltaje: return (tinta_toolbox_a_db(x[0], false), "dB")
        case .desdeDB: return (tinta_toolbox_desde_db(x[0], false), "")
        case .resistorLed: return ((x[0] - x[1]) / x[2], "Ω")
        case .astable555: return (1.44 / ((x[0] + 2 * x[1]) * x[2]), "Hz")
        case .mono555: return (1.1 * x[0] * x[1], "s")
        case .adc: return (x[0] / pow(2, x[1]), "V/LSB")
        case .bateria: return (x[0] * x[1], "Wh")
        case .reactanciaC: return (1 / (2 * .pi * x[0] * x[1]), "Ω")
        case .reactanciaL: return (2 * .pi * x[0] * x[1], "Ω")
        case .paralelo: return (x[0] * x[1] / (x[0] + x[1]), "Ω")
        case .divisorCorriente:
            return (x[0] * x[2] / (x[1] + x[2]), "A")
        case .resistoresSerie: return (x[0] + x[1], "Ω")
        case .capacitoresSerie: return (x[0] * x[1] / (x[0] + x[1]), "F")
        case .capacitoresParalelo: return (x[0] + x[1], "F")
        case .inductoresSerie: return (x[0] + x[1], "H")
        case .inductoresParalelo: return (x[0] * x[1] / (x[0] + x[1]), "H")
        case .corteRL: return (x[0] / (2 * .pi * x[1]), "Hz")
        case .impedanciaRLC:
            let xl = 2 * Double.pi * x[3] * x[1]
            let xc = 1 / (2 * Double.pi * x[3] * x[2])
            return (hypot(x[0], xl - xc), "Ω")
        case .faseRLC:
            let xl = 2 * Double.pi * x[3] * x[1]
            let xc = 1 / (2 * Double.pi * x[3] * x[2])
            return (atan2(xl - xc, x[0]) * 180 / .pi, "°")
        case .gananciaVoltaje: return (x[0] / x[1], "V/V")
        case .dbPotencia: return (10 * log10(abs(x[0])), "dB")
        case .rmsSeno: return (x[0] / sqrt(2), "RMS")
        case .factorPotencia: return (x[0] / (x[1] * x[2]), "")
        case .dutyPWM: return (100 * x[0] / x[1], "%")
        case .promedioPWM: return (x[0] * x[1] / 100, "V")
        case .polarizacionBJT: return (x[3] * (x[0] - x[1]) / x[2], "A")
        case .energiaCapacitor: return (0.5 * x[0] * x[1] * x[1], "J")
        case .energiaInductor: return (0.5 * x[0] * x[1] * x[1], "J")
        case .relacionTransformador: return (x[0] * x[2] / x[1], "V")
        case .inductanciaDesdeReactancia: return (x[0] / (2 * .pi * x[1]), "H")
        case .capacitanciaDesdeReactancia: return (1 / (2 * .pi * x[1] * x[0]), "F")
        case .nortonDesdeThevenin: return (x[0] / x[1], "A")
        case .maximaPotencia: return (x[0] * x[0] / (4 * x[1]), "W")
        case .superposicionTres: return (x[0] + x[1] + x[2], "")
        }
    }
}

final class ControladorToolbox: UITableViewController {
    var alInsertar: ((String) -> Void)?
    init() { super.init(style: .insetGrouped) }
    required init?(coder: NSCoder) { fatalError("init(coder:) no está implementado") }
    override func viewDidLoad() {
        super.viewDidLoad(); title = "Calculadoras de ingeniería"
        navigationItem.rightBarButtonItem = UIBarButtonItem(
            title: "Códigos", style: .plain, target: self,
            action: #selector(abrirCodigos))
    }
    override func tableView(_ tableView: UITableView, numberOfRowsInSection section: Int) -> Int {
        HerramientaIngenieria.allCases.count
    }
    override func tableView(_ tableView: UITableView, cellForRowAt indexPath: IndexPath)
        -> UITableViewCell {
        let c = UITableViewCell(style: .default, reuseIdentifier: nil)
        c.textLabel?.text = HerramientaIngenieria.allCases[indexPath.row].rawValue
        c.accessoryType = .disclosureIndicator; return c
    }
    override func tableView(_ tableView: UITableView, didSelectRowAt indexPath: IndexPath) {
        let herramienta = HerramientaIngenieria.allCases[indexPath.row]
        let a = UIAlertController(title: herramienta.rawValue, message: nil, preferredStyle: .alert)
        for nombre in herramienta.entradas { a.addTextField { $0.placeholder = nombre
            $0.keyboardType = .numbersAndPunctuation } }
        a.addAction(UIAlertAction(title: "Calcular", style: .default) { [weak self] _ in
            let valores = a.textFields?.compactMap {
                Double(($0.text ?? "").replacingOccurrences(of: ",", with: "."))
            } ?? []
            guard valores.count == herramienta.entradas.count else { return }
            let r = herramienta.calcular(valores)
            guard r.0.isFinite else {
                let error = UIAlertController(
                    title: "Datos no válidos",
                    message: "Revise divisores, frecuencia y magnitudes. No se insertó un resultado infinito o indefinido.",
                    preferredStyle: .alert)
                error.addAction(UIAlertAction(title: "Aceptar", style: .default))
                self?.present(error, animated: true); return
            }
            let texto = "\(herramienta.rawValue): \(NotacionIngenieria.formatear(r.0, unidad: r.1))"
            let resultado = UIAlertController(title: "Resultado", message: texto,
                                              preferredStyle: .actionSheet)
            resultado.addAction(UIAlertAction(title: "Insertar en el apunte", style: .default) {
                _ in self?.alInsertar?(texto)
            }); resultado.addAction(UIAlertAction(title: "Cerrar", style: .cancel))
            resultado.popoverPresentationController?.sourceView = self?.view
            self?.present(resultado, animated: true)
        }); a.addAction(UIAlertAction(title: "Cancelar", style: .cancel)); present(a, animated: true)
    }
    @objc private func abrirCodigos() {
        let panel = PanelCodigosComponentesIPad()
        panel.alInsertar = alInsertar
        navigationController?.pushViewController(panel, animated: true)
    }
}

enum FormaOnda: String, CaseIterable { case seno, cuadrada, triangular, pwm, exponencial }

final class ControladorFormaOnda: UIViewController {
    var alInsertar: ((UIImage, String) -> Void)?
    private let vista = VistaFormaOnda()
    private let forma = UISegmentedControl(items: FormaOnda.allCases.map(\.rawValue))
    private let frecuencia = UISlider(); private let duty = UISlider()
    override func viewDidLoad() {
        super.viewDidLoad(); title = "Waveform tool"; view.backgroundColor = .systemBackground
        forma.selectedSegmentIndex = 0; forma.addTarget(self, action: #selector(actualizar), for: .valueChanged)
        frecuencia.minimumValue = 1; frecuencia.maximumValue = 10; frecuencia.value = 2
        duty.minimumValue = 0.05; duty.maximumValue = 0.95; duty.value = 0.5
        frecuencia.addTarget(self, action: #selector(actualizar), for: .valueChanged)
        duty.addTarget(self, action: #selector(actualizar), for: .valueChanged)
        let insertar = UIButton(type: .system); insertar.configuration = .borderedProminent()
        insertar.configuration?.title = "Insertar gráfica en la nota"
        insertar.addTarget(self, action: #selector(insertarOnda), for: .touchUpInside)
        let pila = UIStackView(arrangedSubviews: [vista, forma, frecuencia, duty, insertar])
        pila.axis = .vertical; pila.spacing = 14; pila.translatesAutoresizingMaskIntoConstraints = false
        view.addSubview(pila); vista.heightAnchor.constraint(equalToConstant: 360).isActive = true
        NSLayoutConstraint.activate([
            pila.topAnchor.constraint(equalTo: view.safeAreaLayoutGuide.topAnchor, constant: 20),
            pila.leadingAnchor.constraint(equalTo: view.leadingAnchor, constant: 20),
            pila.trailingAnchor.constraint(equalTo: view.trailingAnchor, constant: -20),
        ]); actualizar()
    }
    @objc private func actualizar() {
        vista.forma = FormaOnda.allCases[max(forma.selectedSegmentIndex, 0)]
        vista.ciclos = CGFloat(frecuencia.value); vista.duty = CGFloat(duty.value); vista.setNeedsDisplay()
    }
    @objc private func insertarOnda() {
        let imagen = UIGraphicsImageRenderer(bounds: vista.bounds).image { contexto in
            vista.layer.render(in: contexto.cgContext)
        }
        alInsertar?(imagen, "Onda \(vista.forma.rawValue)")
    }
}

final class VistaFormaOnda: UIView {
    var forma: FormaOnda = .seno; var ciclos: CGFloat = 2; var duty: CGFloat = 0.5
    override init(frame: CGRect) { super.init(frame: frame); backgroundColor = .secondarySystemBackground }
    required init?(coder: NSCoder) { super.init(coder: coder) }
    override func draw(_ rect: CGRect) {
        let eje = UIBezierPath(); eje.move(to: CGPoint(x: 0, y: bounds.midY))
        eje.addLine(to: CGPoint(x: bounds.maxX, y: bounds.midY)); UIColor.secondaryLabel.setStroke()
        eje.lineWidth = 1; eje.stroke()
        let onda = UIBezierPath()
        for px in 0...Int(bounds.width) {
            let x = CGFloat(px) / max(bounds.width, 1)
            let fase = x * ciclos
            let fraccion = fase - floor(fase)
            let y: CGFloat
            switch forma {
            case .seno: y = sin(fase * 2 * .pi)
            case .cuadrada: y = fraccion < 0.5 ? 1 : -1
            case .triangular: y = 1 - 4 * abs(fraccion - 0.5)
            case .pwm: y = fraccion < duty ? 1 : -1
            case .exponencial: y = 1 - 2 * exp(-fraccion * 5)
            }
            let p = CGPoint(x: CGFloat(px), y: bounds.midY - y * bounds.height * 0.38)
            if px == 0 { onda.move(to: p) } else { onda.addLine(to: p) }
        }
        UIColor.systemGreen.setStroke(); onda.lineWidth = 2.5; onda.stroke()
    }
}
