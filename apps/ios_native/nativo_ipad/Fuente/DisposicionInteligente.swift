import UIKit

enum TipoFondoRegion:String,Codable,CaseIterable { case blanco="Blanco",crema="Crema",oscuro="Oscuro",rayado="Rayado",cuadricula="Cuadriculado" }
struct RegionFondoDocumento:Codable,Identifiable,Equatable {
    var id=UUID();var nombre:String;var marco:CGRect;var tipo:TipoFondoRegion
    var colorARGB:UInt32;var colorPautaARGB:UInt32=0x338E8E93;var separacion:CGFloat=24
}
struct RegionFondoRender:Equatable {
    var marco:CGRect;var tipo:TipoFondoRegion;var colorARGB:UInt32
    var colorPautaARGB:UInt32;var separacion:CGFloat
}

final class RepositorioRegionesFondo {
    static let compartido=RepositorioRegionesFondo();private let url:URL;private var documentos:[String:[RegionFondoDocumento]]=[:]
    private init(){let base=FileManager.default.urls(for:.documentDirectory,in:.userDomainMask).first!;let carpeta=base.appendingPathComponent("RegionesFondo",isDirectory:true);try? FileManager.default.createDirectory(at:carpeta,withIntermediateDirectories:true);url=carpeta.appendingPathComponent("regiones.json");if let d=try? Data(contentsOf:url),let r=try? JSONDecoder().decode([String:[RegionFondoDocumento]].self,from:d){documentos=r}}
    func regiones(_ documento:UInt64)->[RegionFondoDocumento]{documentos[String(documento)] ?? []}
    func guardar(_ regiones:[RegionFondoDocumento],documento:UInt64){documentos[String(documento)]=regiones;if let d=try? JSONEncoder().encode(documentos){try? d.write(to:url,options:.atomic)}}
    static func render(_ regiones:[RegionFondoDocumento])->[RegionFondoRender]{regiones.map{RegionFondoRender(marco:$0.marco,tipo:$0.tipo,colorARGB:$0.colorARGB,colorPautaARGB:$0.colorPautaARGB,separacion:$0.separacion)}}
}

final class PanelDisposicionInteligente: UITableViewController {
    var alAceptarTitulo: ((String) -> Void)?
    var alCerrarClase: (() -> Void)?
    var alAplicarCambios: (() -> Void)?

    private weak var navegador: VistaLienzoNavegable?
    private let sesion: Sesion
    private let tituloActual: () -> String
    private let clase: SesionClase?
    private var regiones: [RegionFondoDocumento]

    init(navegador: VistaLienzoNavegable, sesion: Sesion,
         tituloActual: @escaping () -> String = { "" },
         clase: SesionClase? = nil) {
        self.navegador = navegador
        self.sesion = sesion
        self.tituloActual = tituloActual
        self.clase = clase
        regiones = RepositorioRegionesFondo.compartido
            .regiones(sesion.documentoPersistenteId)
        super.init(style: .insetGrouped)
    }

    required init?(coder: NSCoder) { fatalError("init(coder:) no está implementado") }

    override func viewDidLoad() {
        super.viewDidLoad(); title = "Disposición inteligente"
        navigationItem.leftBarButtonItem = UIBarButtonItem(
            barButtonSystemItem: .close, target: self,
            action: #selector(cerrarPanel))
        aplicarRegiones()
    }

    override func numberOfSections(in tableView: UITableView) -> Int { 4 }
    override func tableView(_ tableView: UITableView,
                            titleForHeaderInSection section: Int) -> String? {
        ["Cierre de clase", "Escritura y espacio",
         "Reacomodo y edición manuscrita", "Fondos por región"][section]
    }
    override func tableView(_ tableView: UITableView,
                            numberOfRowsInSection section: Int) -> Int {
        switch section {
        case 0: return 3
        case 1: return 5
        case 2: return 6
        default: return max(regiones.count + 1, 1)
        }
    }
    override func tableView(_ tableView: UITableView,
                            cellForRowAt indexPath: IndexPath) -> UITableViewCell {
        let celda = UITableViewCell(style: .subtitle, reuseIdentifier: nil)
        switch indexPath.section {
        case 0:
            let titulos = ["Sugerir título de esta nota", "Ordenar esta página",
                           "Cerrar clase y preparar repaso"]
            let detalles = [
                "Nunca reemplaza el nombre sin que usted elija uno.",
                "Alinea, compacta, corrige perspectiva y sugiere secciones con vista previa.",
                clase == nil ? "No hay una sesión de clase activa."
                    : "Guarda, resume y crea cinco preguntas de recuperación.",
            ]
            celda.textLabel?.text = titulos[indexPath.row]
            celda.detailTextLabel?.text = detalles[indexPath.row]
            celda.detailTextLabel?.numberOfLines = 0
            celda.imageView?.image = UIImage(systemName: [
                "text.badge.plus", "wand.and.rays", "checkmark.circle"
            ][indexPath.row])
            if indexPath.row == 2, clase == nil {
                celda.selectionStyle = .none
                celda.textLabel?.textColor = .secondaryLabel
            }
        case 1:
            let titulos = ["Activar Smart Zoom / Zoom Box",
                           "Desactivar Smart Zoom", "Insertar espacio vertical",
                           "Insertar espacio horizontal", "Gestos naturales de Pencil"]
            celda.textLabel?.text = titulos[indexPath.row]
            if indexPath.row == 0 {
                celda.detailTextLabel?.text = "Avanza al borde sin saltos y detecta el siguiente renglón"
            } else if indexPath.row == 4 {
                let p = PreferenciasGestosTintaIPad.actual
                let activos = [p.circleToLasso ? "Circle→Lasso" : nil,
                               p.scribbleToErase ? "Scribble→Erase" : nil]
                    .compactMap { $0 }
                celda.detailTextLabel?.text = activos.isEmpty
                    ? "Desactivados" : activos.joined(separator: " · ")
                celda.imageView?.image = UIImage(systemName: "pencil.and.scribble")
                celda.accessoryType = .disclosureIndicator
            }
        case 2:
            let titulos = [
                "Reflow parcial con vista previa",
                "Limpiar región: alinear y espaciar",
                "Distribuir objetos manuscritos",
                "Editar palabra manuscrita",
                "Editar línea manuscrita",
                "Copiar región visible como ancla fija",
            ]
            let detalles = [
                "Reacomoda trazos con vista previa y versión previa.",
                "Conserva la tinta original en una versión previa.",
                "Distribuye grupos sin rasterizar la escritura.",
                "Toque una palabra y muévala, cópiela, enlácela o edítela.",
                "Toque un renglón completo y edítelo como una unidad vectorial.",
                "Copia coordenadas exactas de la zona visible para enlazar diagramas, PDFs o espacio del lienzo aunque no exista un objeto único.",
            ]
            celda.textLabel?.text = titulos[indexPath.row]
            celda.detailTextLabel?.text = detalles[indexPath.row]
            celda.detailTextLabel?.numberOfLines = 0
            if indexPath.row >= 3 {
                let simbolos = [
                    "text.cursor",
                    "text.line.first.and.arrowtriangle.forward",
                    "link.badge.plus",
                ]
                celda.imageView?.image = UIImage(
                    systemName: simbolos[indexPath.row - 3])
            }
        default:
            if indexPath.row == 0 {
                celda.textLabel?.text = "Añadir región con fondo distinto"
                celda.imageView?.image = UIImage(systemName: "rectangle.3.group")
            } else {
                let region = regiones[indexPath.row - 1]
                celda.textLabel?.text = region.nombre
                celda.detailTextLabel?.text = "\(region.tipo.rawValue) · \(Int(region.marco.width))×\(Int(region.marco.height))"
                celda.accessoryType = .disclosureIndicator
            }
        }
        return celda
    }

    override func tableView(_ tableView: UITableView,
                            didSelectRowAt indexPath: IndexPath) {
        tableView.deselectRow(at: indexPath, animated: true)
        switch indexPath.section {
        case 0:
            if indexPath.row == 0 { mostrarTitulos() }
            else if indexPath.row == 1 { prepararLimpieza() }
            else if clase != nil {
                dismiss(animated: true) { [weak self] in self?.alCerrarClase?() }
            }
        case 1:
            if indexPath.row == 4 { abrirPreferenciasGestos() }
            else { accionEspacio(indexPath.row) }
        case 2:
            if indexPath.row < 3 {
                pedirReflujo(modo: indexPath.row)
            } else if indexPath.row < 5 {
                iniciarEdicionManuscrita(
                    nivel: indexPath.row == 3 ? .palabra : .linea)
            } else {
                copiarRegionVisibleComoAncla()
            }
        default:
            if indexPath.row == 0 { nuevaRegion() }
            else { editarRegion(indexPath.row - 1) }
        }
    }

    private func abrirPreferenciasGestos() {
        let p = PreferenciasGestosTintaIPad.actual
        let hoja = UIAlertController(
            title: "Gestos naturales de Pencil",
            message: "Se aplican sólo mientras usa la pluma. Puede desactivar cada gesto si interfiere con su forma de escribir.",
            preferredStyle: .actionSheet)
        hoja.addAction(UIAlertAction(
            title: "\(p.circleToLasso ? "✓ " : "")Circle-to-Lasso",
            style: .default) { [weak self] _ in
                var nuevo = PreferenciasGestosTintaIPad.actual
                nuevo.circleToLasso.toggle(); PreferenciasGestosTintaIPad.actual = nuevo
                self?.tableView.reloadData()
            })
        hoja.addAction(UIAlertAction(
            title: "\(p.scribbleToErase ? "✓ " : "")Scribble-to-Erase",
            style: .default) { [weak self] _ in
                var nuevo = PreferenciasGestosTintaIPad.actual
                nuevo.scribbleToErase.toggle(); PreferenciasGestosTintaIPad.actual = nuevo
                self?.tableView.reloadData()
            })
        hoja.addAction(UIAlertAction(title: "Restaurar ambos", style: .default) {
            [weak self] _ in
            PreferenciasGestosTintaIPad.restaurar(); self?.tableView.reloadData()
        })
        hoja.addAction(UIAlertAction(title: "Cerrar", style: .cancel))
        hoja.popoverPresentationController?.sourceView = view
        present(hoja, animated: true)
    }

    private func iniciarEdicionManuscrita(nivel: NivelEdicionManuscritaIPad) {
        guard let navegador,
              let presentador = navigationController?.presentingViewController
                ?? presentingViewController else {
            avisar("No se pudo abrir el editor manuscrito en esta ventana.")
            return
        }
        let alCambiar = alAplicarCambios
        navegador.quitarPreviaReflujo()
        dismiss(animated: true) {
            CapturadorEdicionManuscritaIPad.instalar(
                sobre: navegador,
                nivel: nivel,
                presentador: presentador,
                alCambiar: { alCambiar?() })
        }
    }

    private func copiarRegionVisibleComoAncla() {
        guard let navegador else {
            avisar("No se pudo leer la región visible de esta ventana.")
            return
        }
        let documento = CGRect(
            x: 0, y: 0,
            width: max(CGFloat(sesion.anchoPagina), 1),
            height: max(CGFloat(sesion.altoPagina), 1))
        let region = navegador.regionVisibleDocumento.intersection(documento)
        let curso = CentroExperienciaIPad.compartido.instantanea()
            .espacioActivo?.cursoId
        guard !region.isNull, !region.isEmpty,
              PortapapelesAnclasDirectasIPad.compartido.copiarRegion(
                region,
                sesion: sesion,
                titulo: "Región fija del lienzo",
                cursoId: curso) != nil else {
            avisar("La zona visible no produjo una región válida para enlazar.")
            return
        }
        let mensaje = "Ancla fija copiada: \(Int(region.width))×\(Int(region.height)) pt. Ahora seleccione handwriting y elija «Enlazar aquí al ancla copiada»."
        dismiss(animated: true) {
            navegador.lienzo.alNotificarGestoRegion?(mensaje)
            UIAccessibility.post(notification: .announcement, argument: mensaje)
        }
    }

    private func mostrarTitulos() {
        let sugerencias = GeneradorTitulosAutomaticosIPad.sugerencias(
            sesion: sesion, clase: clase)
        let actual = tituloActual()
        let mensaje = GeneradorTitulosAutomaticosIPad.esGenerico(actual)
            ? "El nombre actual parece genérico. Elija uno solo si desea reemplazarlo."
            : "Nombre actual: \(actual). Ninguna sugerencia se aplicará automáticamente."
        let hoja = UIAlertController(title: "Títulos sugeridos", message: mensaje,
                                     preferredStyle: .actionSheet)
        for titulo in sugerencias {
            hoja.addAction(UIAlertAction(title: titulo, style: .default) {
                [weak self] _ in
                guard let self else { return }
                RepositorioTitulosAutomaticosIPad.compartido.aceptar(
                    titulo, documento: self.sesion.documentoPersistenteId)
                self.alAceptarTitulo?(titulo)
            })
        }
        hoja.addAction(UIAlertAction(title: "Conservar nombre actual", style: .cancel))
        hoja.popoverPresentationController?.sourceView = view
        present(hoja, animated: true)
    }

    private func prepararLimpieza() {
        navigationItem.prompt = "Analizando localmente; todavía no se aplica nada…"
        view.isUserInteractionEnabled = false
        MotorLimpiezaPostClaseIPad.preparar(sesion: sesion) { [weak self] plan in
            guard let self else { return }
            self.navigationItem.prompt = nil
            self.view.isUserInteractionEnabled = true
            guard !plan.estaVacio, let navegador = self.navegador else {
                self.avisar("No se detectaron cambios útiles para ordenar esta página.")
                return
            }
            navegador.previsualizarLimpieza(plan)
            let hoja = UIAlertController(
                title: "Vista previa de Ordenar esta página",
                message: plan.descripcion
                    + "\n\nMorado muestra destinos y secciones. La tinta original se respalda antes de aplicar.",
                preferredStyle: .actionSheet)
            hoja.addAction(UIAlertAction(title: "Aplicar cambios reversibles",
                                          style: .default) { [weak self] _ in
                guard let self, let navegador = self.navegador else { return }
                _ = GestorVersionesDocumento.compartido.crear(
                    nombre: "Antes de ordenar la página",
                    motivo: "respaldo automático de tinta y objetos",
                    sesion: self.sesion, bloques: navegador.capaBloques.bloques,
                    tecnicos: navegador.capaTecnica.objetos,
                    capas: RepositorioCapas.compartido.estado(
                        self.sesion.documentoPersistenteId))
                let resultado = MotorLimpiezaPostClaseIPad.aplicar(
                    plan, sesion: self.sesion, navegador: navegador)
                self.alAplicarCambios?()
                self.avisar("Orden aplicado: \(resultado.trazos) trazos, \(resultado.medios) objetos, \(resultado.perspectivas) perspectivas, \(resultado.encabezados) encabezados y \(resultado.secciones) secciones.")
            })
            hoja.addAction(UIAlertAction(title: "Cancelar", style: .cancel) {
                [weak self] _ in self?.navegador?.quitarPreviaReflujo()
            })
            hoja.popoverPresentationController?.sourceView = self.view
            self.present(hoja, animated: true)
        }
    }

    private func accionEspacio(_ fila: Int) {
        guard let navegador else { return }
        switch fila {
        case 0:
            let visible = navegador.regionVisibleDocumento
            let region = CGRect(x: visible.minX, y: visible.minY,
                                width: max(visible.width * 0.56, 240),
                                height: max(visible.height * 0.26, 100))
            navegador.activarZoomEscritura(region: region, altoLinea: 36,
                                            avance: 0.68)
            dismiss(animated: true)
        case 1:
            navegador.desactivarZoomEscritura(); dismiss(animated: true)
        default:
            let vertical = fila == 2
            let alerta = UIAlertController(
                title: vertical ? "Espacio vertical" : "Espacio horizontal",
                message: "Cantidad en puntos del documento. 3 cm impresos ≈ 85 pt.",
                preferredStyle: .alert)
            alerta.addTextField { $0.text = "85"; $0.keyboardType = .decimalPad }
            alerta.addAction(UIAlertAction(title: "Insertar", style: .default) {
                [weak self] _ in
                guard let self, let navegador = self.navegador,
                      let texto = alerta.textFields?.first?.text?
                        .replacingOccurrences(of: ",", with: "."),
                      let valor = Float(texto) else { return }
                let punto = navegador.centroVisibleDocumento
                navegador.insertarEspacio(
                    eje: vertical ? .vertical : .horizontal,
                    desde: Float(vertical ? punto.y : punto.x), cantidad: valor)
                self.alAplicarCambios?()
            })
            alerta.addAction(UIAlertAction(title: "Cancelar", style: .cancel))
            present(alerta, animated: true)
        }
    }

    private func pedirReflujo(modo: Int) {
        let alerta = UIAlertController(
            title: "Reacomodar región visible",
            message: "Factor de espacio entre líneas. Se mostrará una vista previa antes de mover.",
            preferredStyle: .alert)
        alerta.addTextField {
            $0.text = modo == 0 ? "1.25" : "1.15"; $0.keyboardType = .decimalPad
        }
        alerta.addAction(UIAlertAction(title: "Previsualizar", style: .default) {
            [weak self] _ in
            guard let self, let navegador = self.navegador,
                  let texto = alerta.textFields?.first?.text?
                    .replacingOccurrences(of: ",", with: "."),
                  let factor = Float(texto) else { return }
            let plan = self.sesion.planReflujo(
                region: navegador.regionVisibleDocumento, factor: factor, modo: modo)
            guard !plan.isEmpty else {
                self.avisar("No hay trazos dentro de la región visible."); return
            }
            navegador.previsualizarReflujo(plan); self.confirmarReflujo(plan)
        })
        alerta.addAction(UIAlertAction(title: "Cancelar", style: .cancel))
        present(alerta, animated: true)
    }

    private func confirmarReflujo(_ plan: [TintaTransformacionTrazo]) {
        let maximo = plan.map { hypot($0.delta_x, $0.delta_y) }.max() ?? 0
        let hoja = UIAlertController(
            title: "Vista previa activa",
            message: "\(plan.count) trazos se moverán; desplazamiento máximo \(Int(maximo)) pt. Morado indica destino.",
            preferredStyle: .actionSheet)
        hoja.addAction(UIAlertAction(title: "Aplicar", style: .default) {
            [weak self] _ in
            guard let self, let navegador = self.navegador else { return }
            _ = GestorVersionesDocumento.compartido.crear(
                nombre: "Antes de reflow", motivo: "respaldo automático de geometría",
                sesion: self.sesion, bloques: navegador.capaBloques.bloques,
                tecnicos: navegador.capaTecnica.objetos,
                capas: RepositorioCapas.compartido.estado(
                    self.sesion.documentoPersistenteId))
            _ = self.sesion.aplicarReflujo(plan)
            navegador.quitarPreviaReflujo(); navegador.lienzo.rehornearYDibujar()
            self.alAplicarCambios?()
        })
        hoja.addAction(UIAlertAction(title: "Cancelar", style: .cancel) {
            [weak self] _ in self?.navegador?.quitarPreviaReflujo()
        })
        hoja.popoverPresentationController?.sourceView = view
        present(hoja, animated: true)
    }

    private func nuevaRegion() {
        let visible = navegador?.regionVisibleDocumento
            ?? CGRect(x: 0, y: 0, width: 600, height: 400)
        let alerta = UIAlertController(
            title: "Nueva región de fondo",
            message: "Se crea sobre la región visible; después puede editar sus coordenadas.",
            preferredStyle: .alert)
        alerta.addTextField { $0.placeholder = "nombre"; $0.text = "Zona cuadriculada" }
        alerta.addTextField {
            $0.placeholder = "tipo: blanco, crema, oscuro, rayado, cuadriculado"
            $0.text = "cuadriculado"
        }
        alerta.addAction(UIAlertAction(title: "Crear", style: .default) {
            [weak self] _ in
            guard let self else { return }
            let tipo = self.tipo(alerta.textFields?[1].text ?? "") ?? .cuadricula
            self.regiones.append(RegionFondoDocumento(
                nombre: alerta.textFields?[0].text ?? tipo.rawValue,
                marco: visible, tipo: tipo, colorARGB: self.color(tipo)))
            self.guardarRegiones()
        })
        alerta.addAction(UIAlertAction(title: "Cancelar", style: .cancel))
        present(alerta, animated: true)
    }

    private func editarRegion(_ indice: Int) {
        let region = regiones[indice]
        let alerta = UIAlertController(title: region.nombre,
                                       message: "x,y,ancho,alto,separación",
                                       preferredStyle: .alert)
        alerta.addTextField {
            $0.text = "\(region.marco.minX),\(region.marco.minY),\(region.marco.width),\(region.marco.height),\(region.separacion)"
        }
        alerta.addAction(UIAlertAction(title: "Guardar", style: .default) {
            [weak self] _ in
            guard let self else { return }
            let valores = alerta.textFields?.first?.text?.split(separator: ",")
                .compactMap { Double($0.trimmingCharacters(in: .whitespaces)) } ?? []
            guard valores.count == 5 else { return }
            self.regiones[indice].marco = CGRect(
                x: CGFloat(valores[0]), y: CGFloat(valores[1]),
                width: CGFloat(max(valores[2], 20)),
                height: CGFloat(max(valores[3], 20)))
            self.regiones[indice].separacion = CGFloat(max(valores[4], 3))
            self.guardarRegiones()
        })
        alerta.addAction(UIAlertAction(title: "Eliminar", style: .destructive) {
            [weak self] _ in self?.regiones.remove(at: indice); self?.guardarRegiones()
        })
        alerta.addAction(UIAlertAction(title: "Cancelar", style: .cancel))
        present(alerta, animated: true)
    }

    private func tipo(_ texto: String) -> TipoFondoRegion? {
        let normal = Normalizador.texto(texto)
        if normal.contains("cuad") { return .cuadricula }
        if normal.contains("raya") { return .rayado }
        if normal.contains("oscur") { return .oscuro }
        if normal.contains("crema") { return .crema }
        if normal.contains("blanc") { return .blanco }
        return nil
    }
    private func color(_ tipo: TipoFondoRegion) -> UInt32 {
        switch tipo {
        case .blanco, .rayado, .cuadricula: return 0xFFFFFFFF
        case .crema: return 0xFFFFF7E6
        case .oscuro: return 0xFF202124
        }
    }
    private func guardarRegiones() {
        RepositorioRegionesFondo.compartido.guardar(
            regiones, documento: sesion.documentoPersistenteId)
        aplicarRegiones(); tableView.reloadData(); alAplicarCambios?()
    }
    private func aplicarRegiones() {
        navegador?.lienzo.regionesFondo = RepositorioRegionesFondo.render(regiones)
    }
    private func avisar(_ texto: String) {
        let alerta = UIAlertController(title: nil, message: texto,
                                       preferredStyle: .alert)
        alerta.addAction(UIAlertAction(title: "Aceptar", style: .default))
        present(alerta, animated: true)
    }
    @objc private func cerrarPanel() {
        navegador?.quitarPreviaReflujo(); dismiss(animated: true)
    }
}
