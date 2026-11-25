import AVFoundation
import Foundation
import UIKit

struct BrechaExplicacionIPad: Identifiable {
    var id = UUID()
    var termino: String
    var motivo: String
    var fuentes: [ReferenciaFuente]
}

struct InformeExplicacionIPad {
    var coberturaEstimada: Double
    var terminosMencionados: [String]
    var brechasPosibles: [BrechaExplicacionIPad]
    var fuentesComparadas: [ReferenciaFuente]
    var advertencia: String
}

/// Comparación extractiva contra el material local. No inventa una explicación
/// modelo ni afirma que una respuesta sea correcta por mera similitud textual.
enum EvaluadorExplicacionLocalIPad {
    static func evaluar(
        explicacion: String,
        tema: String,
        cursoId: UUID?
    ) -> InformeExplicacionIPad {
        let resultados = CentroAcademico.compartido.buscar(
            tema.isEmpty ? explicacion : tema, cursoId: cursoId, limite: 12)
        let fuentes = resultados.map(\.fuente)
        let textoMaterial = resultados.map(\.texto).joined(separator: "\n")
        let frecuencia = frecuencias(textoMaterial)
        let clavesEtiquetadas = resultados.flatMap {
            $0.fuente.titulo.split(separator: " ").map(String.init)
                + EtiquetadorLocal.etiquetas($0.texto)
        }
        let candidatos = Array(Set(
            clavesEtiquetadas.map(normalizar).filter { $0.count >= 3 }
                + frecuencia.sorted { $0.value > $1.value }.prefix(30).map(\.key)))
            .filter { !palabrasVacias.contains($0) }
            .sorted { frecuencia[$0, default: 0] > frecuencia[$1, default: 0] }
        let explicada = Set(tokenizar(explicacion))
        let mencionados = candidatos.filter { explicada.contains($0) }
        let faltantes = candidatos.filter { !explicada.contains($0) }.prefix(12)
        let brechas = faltantes.map { termino in
            let relacionadas = resultados.filter {
                Set(tokenizar($0.texto + " " + $0.fuente.titulo)).contains(termino)
            }.map(\.fuente)
            return BrechaExplicacionIPad(
                termino: termino,
                motivo: "Aparece en el material recuperado, pero no en la explicación transcrita.",
                fuentes: Array(relacionadas.prefix(4)))
        }
        let cobertura = candidatos.isEmpty ? 0
            : Double(mencionados.count) / Double(candidatos.count)
        return InformeExplicacionIPad(
            coberturaEstimada: cobertura,
            terminosMencionados: mencionados,
            brechasPosibles: brechas,
            fuentesComparadas: Array(fuentes.prefix(12)),
            advertencia: resultados.isEmpty
                ? "No hubo material local suficiente; no se emitió una evaluación."
                : "La cobertura léxica solo señala posibles huecos. No demuestra comprensión ni corrección; revise las fuentes citadas.")
    }

    static func coincidenciaRespuesta(_ respuesta: String, esperada: String) -> Double {
        let a = Set(tokenizar(respuesta)); let b = Set(tokenizar(esperada))
        guard !a.isEmpty, !b.isEmpty else { return 0 }
        return Double(a.intersection(b).count) / Double(a.union(b).count)
    }

    private static let palabrasVacias: Set<String> = [
        "para", "como", "este", "esta", "estos", "estas", "porque", "pero",
        "desde", "entre", "sobre", "donde", "cuando", "tambien", "tiene",
        "puede", "cada", "segun", "the", "and", "that", "with", "from",
    ]

    private static func frecuencias(_ texto: String) -> [String: Int] {
        Dictionary(grouping: tokenizar(texto), by: { $0 }).mapValues(\.count)
    }
    private static func tokenizar(_ texto: String) -> [String] {
        texto.components(separatedBy: .alphanumerics.inverted)
            .map(normalizar).filter { $0.count >= 3 && !palabrasVacias.contains($0) }
    }
    private static func normalizar(_ texto: String) -> String {
        texto.folding(options: [.diacriticInsensitive, .caseInsensitive],
                      locale: Locale(identifier: "es")).lowercased()
    }
}

enum ModoEstudioOralIPad: Int, CaseIterable {
    case ensenar
    case preguntas
    case movimiento

    var nombre: String {
        switch self {
        case .ensenar: return "Enseñar"
        case .preguntas: return "Preguntas"
        case .movimiento: return "Auto/Bus"
        }
    }
}

final class PanelEstudioOralIPad: UIViewController {
    private let modo = UISegmentedControl(
        items: ModoEstudioOralIPad.allCases.map(\.nombre))
    private let tema = UITextField()
    private let pregunta = UILabel()
    private let transcripcion = UITextView()
    private let boton = UIButton(type: .system)
    private let siguiente = UIButton(type: .system)
    private let confianza = UISlider()
    private let estado = UILabel()
    private let dictado = DictadoEnVivo()
    private let voz = AVSpeechSynthesizer()
    private var textoActual = ""
    private var tarjetaActual: FlashcardAcademica?
    private var inicioRespuesta = Date()
    private var cursoId: UUID?

    override func viewDidLoad() {
        super.viewDidLoad()
        title = "Estudio oral local"
        view.backgroundColor = .systemBackground
        navigationItem.leftBarButtonItem = UIBarButtonItem(
            barButtonSystemItem: .close, target: self, action: #selector(cerrar))
        cursoId = CentroExperienciaIPad.compartido.instantanea().espacioActivo?.cursoId

        modo.selectedSegmentIndex = 0
        modo.addTarget(self, action: #selector(cambiarModo), for: .valueChanged)
        tema.placeholder = "Tema: KCL, superposición, Thévenin…"
        tema.borderStyle = .roundedRect
        tema.autocorrectionType = .no
        pregunta.text = "Explique el concepto con sus propias palabras."
        pregunta.font = .preferredFont(forTextStyle: .title2)
        pregunta.numberOfLines = 0
        pregunta.textAlignment = .center

        transcripcion.font = .preferredFont(forTextStyle: .body)
        transcripcion.backgroundColor = .secondarySystemBackground
        transcripcion.layer.cornerRadius = 12
        transcripcion.isEditable = false
        transcripcion.text = "La transcripción aparecerá aquí."

        boton.configuration = .borderedProminent()
        boton.configuration?.title = "Comenzar explicación"
        boton.configuration?.image = UIImage(systemName: "mic.fill")
        boton.addTarget(self, action: #selector(alternar), for: .touchUpInside)
        siguiente.configuration = .bordered()
        siguiente.configuration?.title = "Siguiente pregunta"
        siguiente.addTarget(self, action: #selector(siguientePregunta), for: .touchUpInside)
        siguiente.isHidden = true

        confianza.minimumValue = 0; confianza.maximumValue = 1; confianza.value = 0.5
        let etiquetaConfianza = UILabel(); etiquetaConfianza.text = "Confianza al responder"
        estado.font = .preferredFont(forTextStyle: .footnote)
        estado.textColor = .secondaryLabel; estado.numberOfLines = 0
        estado.text = "El reconocimiento oral se exige en el dispositivo. No se usa una API de IA de pago."

        let pila = UIStackView(arrangedSubviews: [modo, tema, pregunta,
                                                   transcripcion, etiquetaConfianza,
                                                   confianza, boton, siguiente, estado])
        pila.axis = .vertical; pila.spacing = 14
        pila.translatesAutoresizingMaskIntoConstraints = false
        view.addSubview(pila)
        NSLayoutConstraint.activate([
            pila.topAnchor.constraint(equalTo: view.safeAreaLayoutGuide.topAnchor,
                                      constant: 18),
            pila.leadingAnchor.constraint(equalTo: view.leadingAnchor, constant: 24),
            pila.trailingAnchor.constraint(equalTo: view.trailingAnchor, constant: -24),
            pila.bottomAnchor.constraint(lessThanOrEqualTo: view.safeAreaLayoutGuide.bottomAnchor,
                                         constant: -18),
            transcripcion.heightAnchor.constraint(greaterThanOrEqualToConstant: 220),
            boton.heightAnchor.constraint(equalToConstant: 52),
        ])

        dictado.exigirEnDispositivo = true
        dictado.alTranscribir = { [weak self] texto, final in
            DispatchQueue.main.async {
                self?.textoActual = texto
                self?.transcripcion.text = texto
                if final { self?.detenerYAnalizar() }
            }
        }
        dictado.alFallar = { [weak self] error in
            DispatchQueue.main.async { self?.fallo(error.localizedDescription) }
        }
    }

    @objc private func cambiarModo() {
        let elegido = ModoEstudioOralIPad(rawValue: modo.selectedSegmentIndex) ?? .ensenar
        siguiente.isHidden = elegido == .ensenar
        tema.isHidden = elegido != .ensenar
        if elegido == .ensenar {
            pregunta.text = "Explique el concepto con sus propias palabras."
            boton.configuration?.title = "Comenzar explicación"
        } else {
            prepararPregunta()
            boton.configuration?.title = "Responder hablando"
            aplicarInterfazMovimiento(elegido == .movimiento)
        }
    }

    private func aplicarInterfazMovimiento(_ activo: Bool) {
        navigationController?.setNavigationBarHidden(activo, animated: true)
        transcripcion.isHidden = activo
        modo.isHidden = activo
        confianza.isHidden = activo
        pregunta.font = .preferredFont(forTextStyle: activo ? .largeTitle : .title2)
    }

    @objc private func alternar() {
        if dictado.escuchando { detenerYAnalizar(); return }
        DictadoEnVivo.pedirPermisos { [weak self] concedido in
            guard let self else { return }
            guard concedido else { self.fallo("No se concedieron micrófono y reconocimiento de voz."); return }
            self.textoActual = ""; self.transcripcion.text = "Escuchando…"
            self.inicioRespuesta = Date()
            do {
                try self.dictado.comenzar()
                self.boton.configuration?.title = "Terminar respuesta"
                self.boton.configuration?.image = UIImage(systemName: "stop.fill")
                self.estado.text = self.dictado.enDispositivo
                    ? "Reconocimiento local activo."
                    : "El modelo local no está disponible."
            } catch { self.fallo(error.localizedDescription) }
        }
    }

    private func detenerYAnalizar() {
        guard dictado.escuchando || !textoActual.isEmpty else { return }
        dictado.detener()
        boton.configuration?.title = "Comenzar otra respuesta"
        boton.configuration?.image = UIImage(systemName: "mic.fill")
        let elegido = ModoEstudioOralIPad(rawValue: modo.selectedSegmentIndex) ?? .ensenar
        if elegido == .ensenar { mostrarInforme() }
        else { mostrarComparacionRespuesta() }
    }

    private func mostrarInforme() {
        let informe = EvaluadorExplicacionLocalIPad.evaluar(
            explicacion: textoActual, tema: tema.text ?? "", cursoId: cursoId)
        let huecos = informe.brechasPosibles.prefix(10).map {
            "• \($0.termino): \($0.motivo)"
        }.joined(separator: "\n")
        let texto = String(format: "Cobertura léxica estimada: %.0f%%\n\n%@\n\n%@",
                           informe.coberturaEstimada * 100,
                           huecos.isEmpty ? "No se aislaron huecos léxicos claros." : huecos,
                           informe.advertencia)
        let controlador = ControladorInformeOralIPad(
            titulo: "Comparación con sus materiales", texto: texto,
            fuentes: informe.fuentesComparadas)
        navigationController?.pushViewController(controlador, animated: true)
    }

    private func prepararPregunta() {
        let base = CentroAcademico.compartido.instantanea()
        let candidatas = base.flashcards.filter {
            !$0.suspendida && (cursoId == nil || $0.cursoId == cursoId)
        }.sorted { $0.proximoRepaso < $1.proximoRepaso }
        tarjetaActual = candidatas.first
        if let tarjetaActual {
            pregunta.text = tarjetaActual.pregunta
            hablar(tarjetaActual.pregunta)
        } else {
            let respuesta = CentroAcademico.compartido.modoSocratico(
                tema.text ?? "conceptos principales", cursoId: cursoId)
            let primera = respuesta.texto.components(separatedBy: "\n\n").first
                ?? "Explique el concepto principal de la última clase."
            pregunta.text = primera; hablar(primera)
        }
    }

    @objc private func siguientePregunta() {
        textoActual = ""; transcripcion.text = ""
        prepararPregunta()
    }

    private func mostrarComparacionRespuesta() {
        guard let tarjeta = tarjetaActual else {
            estado.text = "La respuesta quedó registrada como práctica oral, pero no había una respuesta fuente para comparar."
            return
        }
        let similitud = EvaluadorExplicacionLocalIPad.coincidenciaRespuesta(
            textoActual, esperada: tarjeta.respuesta)
        let alerta = UIAlertController(
            title: "Revise y califique su respuesta",
            message: String(
                format: "Su respuesta:\n%@\n\nFuente esperada:\n%@\n\nSolapamiento léxico orientativo: %.0f%%. No es una calificación automática.",
                textoActual, tarjeta.respuesta, similitud * 100),
            preferredStyle: .actionSheet)
        for calidad in 0...5 {
            alerta.addAction(UIAlertAction(title: "\(calidad) / 5", style: .default) {
                [weak self] _ in
                guard let self else { return }
                CentroAcademico.compartido.registrarRepaso(
                    tarjetaId: tarjeta.id, calidad: calidad,
                    confianza: Double(self.confianza.value),
                    segundos: Date().timeIntervalSince(self.inicioRespuesta))
                if calidad < 3 {
                    _ = CentroAcademico.compartido.registrarError(
                        cursoId: tarjeta.cursoId, conceptoId: tarjeta.conceptoId,
                        enunciado: tarjeta.pregunta,
                        respuestaDada: self.textoActual,
                        correcta: tarjeta.respuesta,
                        causa: "Respuesta oral marcada para repetir",
                        referencia: tarjeta.referencia)
                }
                self.prepararPregunta()
            })
        }
        alerta.addAction(UIAlertAction(title: "Cancelar", style: .cancel))
        alerta.popoverPresentationController?.sourceView = view
        present(alerta, animated: true)
    }

    private func hablar(_ texto: String) {
        voz.stopSpeaking(at: .immediate)
        let frase = AVSpeechUtterance(string: texto)
        frase.voice = AVSpeechSynthesisVoice(language: "es-CR")
            ?? AVSpeechSynthesisVoice(language: "es-MX")
        frase.rate = 0.46
        voz.speak(frase)
    }

    private func fallo(_ texto: String) {
        dictado.detener()
        boton.configuration?.title = "Intentar de nuevo"
        let alerta = UIAlertController(title: "Estudio oral no disponible",
                                       message: texto, preferredStyle: .alert)
        alerta.addAction(UIAlertAction(title: "Aceptar", style: .default))
        present(alerta, animated: true)
    }

    @objc private func cerrar() {
        dictado.detener(); voz.stopSpeaking(at: .immediate); dismiss(animated: true)
    }
}

final class ControladorInformeOralIPad: UITableViewController {
    private let cuerpo: String
    private let fuentes: [ReferenciaFuente]
    init(titulo: String, texto: String, fuentes: [ReferenciaFuente]) {
        self.cuerpo = texto; self.fuentes = fuentes
        super.init(style: .insetGrouped); self.title = titulo
    }
    required init?(coder: NSCoder) { fatalError("init(coder:) no está implementado") }
    override func numberOfSections(in tableView: UITableView) -> Int { 2 }
    override func tableView(_ tableView: UITableView,
                            numberOfRowsInSection section: Int) -> Int {
        section == 0 ? 1 : fuentes.count
    }
    override func tableView(_ tableView: UITableView,
                            titleForHeaderInSection section: Int) -> String? {
        section == 0 ? "Informe" : "Fuentes exactas comparadas"
    }
    override func tableView(_ tableView: UITableView,
                            cellForRowAt indexPath: IndexPath) -> UITableViewCell {
        let celda = UITableViewCell(style: .subtitle, reuseIdentifier: nil)
        if indexPath.section == 0 {
            celda.textLabel?.text = cuerpo; celda.textLabel?.numberOfLines = 0
        } else {
            let fuente = fuentes[indexPath.row]
            celda.textLabel?.text = fuente.titulo
            celda.detailTextLabel?.text = [fuente.tipo.rawValue,
                fuente.pagina.map { "p. \($0)" },
                fuente.segundoAudio.map { String(format: "%02d:%02d", Int($0) / 60, Int($0) % 60) }]
                .compactMap { $0 }.joined(separator: " · ")
        }
        return celda
    }
}
