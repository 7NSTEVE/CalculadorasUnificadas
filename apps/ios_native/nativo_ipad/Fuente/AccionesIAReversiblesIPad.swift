import CryptoKit
import Foundation
import UIKit

enum TipoObjetivoAccionIAIPad: String, Codable {
    case documentoTexto
    case bloqueLienzo
    case seleccionTexto
    case comentario
    case formula
    case metadata
}

enum EstadoAccionIAIPad: String, Codable { case propuesta, aplicada, revertida, descartada }

struct ObjetivoAccionIAIPad: Codable, Equatable {
    var documentoId: DocumentoPersistenteID
    var tipo: TipoObjetivoAccionIAIPad
    var objetoId: String
}

struct AccionIAReversibleIPad: Codable, Equatable, Identifiable {
    var id = UUID()
    var objetivo: ObjetivoAccionIAIPad
    var titulo: String
    var explicacion: String
    var antes: Data
    var despues: Data
    var hashAntes: String
    var hashDespues: String
    var fuentes: [String] = []
    var estado: EstadoAccionIAIPad = .propuesta
    var creada = Date()
    var actualizada = Date()

    static func propuesta(objetivo: ObjetivoAccionIAIPad,
                          titulo: String,
                          explicacion: String,
                          antes: Data,
                          despues: Data,
                          fuentes: [String] = []) -> AccionIAReversibleIPad {
        .init(objetivo: objetivo,
              titulo: titulo,
              explicacion: explicacion,
              antes: antes,
              despues: despues,
              hashAntes: HashAccionIAIPad.sha256(antes),
              hashDespues: HashAccionIAIPad.sha256(despues),
              fuentes: fuentes)
    }
}

enum ErrorAccionIAReversibleIPad: LocalizedError, Equatable {
    case accionNoEncontrada
    case estadoInvalido
    case objetivoCambioDesdePropuesta
    case contenidoAplicadoCambio
    case hashInternoInvalido

    var errorDescription: String? {
        switch self {
        case .accionNoEncontrada: return "La propuesta de IA ya no existe."
        case .estadoInvalido: return "La acción de IA no está en un estado compatible con esta operación."
        case .objetivoCambioDesdePropuesta: return "El contenido cambió después de generar la propuesta. Revise una propuesta nueva antes de aplicarla."
        case .contenidoAplicadoCambio: return "El contenido aplicado fue editado después; no se puede deshacer automáticamente sin pisar cambios nuevos."
        case .hashInternoInvalido: return "La propuesta de IA no supera su comprobación interna de integridad."
        }
    }
}

protocol AdaptadorObjetivoAccionIAIPad: AnyObject {
    func leer(_ objetivo: ObjetivoAccionIAIPad) throws -> Data
    func escribir(_ datos: Data, en objetivo: ObjetivoAccionIAIPad) throws
}

private struct ArchivoAccionesIAIPad: Codable {
    var version = 1
    var acciones: [AccionIAReversibleIPad] = []
}

final class RepositorioAccionesIAReversiblesIPad {
    static let compartido = RepositorioAccionesIAReversiblesIPad()
    private let bloqueo = NSRecursiveLock(); private let url: URL
    private var archivo: ArchivoAccionesIAIPad

    init(base: URL? = nil, archivos: FileManager = .default) {
        let carpeta = base ?? archivos.urls(for: .applicationSupportDirectory, in: .userDomainMask)[0]
            .appendingPathComponent("IA/AccionesReversibles", isDirectory: true)
        try? archivos.createDirectory(at: carpeta, withIntermediateDirectories: true)
        url = carpeta.appendingPathComponent("acciones-v1.json")
        let d=JSONDecoder();d.dateDecodingStrategy = .iso8601
        if let datos=try? Data(contentsOf:url),let leido=try? d.decode(ArchivoAccionesIAIPad.self,from:datos){archivo=leido}else{archivo=ArchivoAccionesIAIPad()}
    }

    @discardableResult
    func guardarPropuesta(_ accion: AccionIAReversibleIPad) throws -> AccionIAReversibleIPad {
        guard accion.estado == .propuesta,
              accion.hashAntes == HashAccionIAIPad.sha256(accion.antes),
              accion.hashDespues == HashAccionIAIPad.sha256(accion.despues) else {
            throw ErrorAccionIAReversibleIPad.hashInternoInvalido
        }
        bloqueo.lock();defer{bloqueo.unlock()}
        if let existente=archivo.acciones.first(where:{$0.id==accion.id}){return existente}
        archivo.acciones.append(accion);persistir();return accion
    }

    func acciones(documentoId: DocumentoPersistenteID? = nil,
                  incluirDescartadas: Bool = false) -> [AccionIAReversibleIPad] {
        bloqueo.lock();defer{bloqueo.unlock()}
        return archivo.acciones.filter{
            (documentoId == nil || $0.objetivo.documentoId == documentoId) &&
            (incluirDescartadas || $0.estado != .descartada)
        }.sorted{$0.actualizada>$1.actualizada}
    }

    @discardableResult
    func aplicar(_ id: UUID, adaptador: AdaptadorObjetivoAccionIAIPad) throws -> AccionIAReversibleIPad {
        bloqueo.lock();defer{bloqueo.unlock()}
        guard let i=archivo.acciones.firstIndex(where:{$0.id==id})else{throw ErrorAccionIAReversibleIPad.accionNoEncontrada}
        guard archivo.acciones[i].estado == .propuesta else{throw ErrorAccionIAReversibleIPad.estadoInvalido}
        let actual=try adaptador.leer(archivo.acciones[i].objetivo)
        guard HashAccionIAIPad.sha256(actual)==archivo.acciones[i].hashAntes else{throw ErrorAccionIAReversibleIPad.objetivoCambioDesdePropuesta}
        try adaptador.escribir(archivo.acciones[i].despues,en:archivo.acciones[i].objetivo)
        let comprobacion=try adaptador.leer(archivo.acciones[i].objetivo)
        guard HashAccionIAIPad.sha256(comprobacion)==archivo.acciones[i].hashDespues else{throw ErrorAccionIAReversibleIPad.hashInternoInvalido}
        archivo.acciones[i].estado = .aplicada;archivo.acciones[i].actualizada=Date();persistir();return archivo.acciones[i]
    }

    @discardableResult
    func revertir(_ id: UUID, adaptador: AdaptadorObjetivoAccionIAIPad) throws -> AccionIAReversibleIPad {
        bloqueo.lock();defer{bloqueo.unlock()}
        guard let i=archivo.acciones.firstIndex(where:{$0.id==id})else{throw ErrorAccionIAReversibleIPad.accionNoEncontrada}
        guard archivo.acciones[i].estado == .aplicada else{throw ErrorAccionIAReversibleIPad.estadoInvalido}
        let actual=try adaptador.leer(archivo.acciones[i].objetivo)
        guard HashAccionIAIPad.sha256(actual)==archivo.acciones[i].hashDespues else{throw ErrorAccionIAReversibleIPad.contenidoAplicadoCambio}
        try adaptador.escribir(archivo.acciones[i].antes,en:archivo.acciones[i].objetivo)
        archivo.acciones[i].estado = .revertida;archivo.acciones[i].actualizada=Date();persistir();return archivo.acciones[i]
    }

    func descartar(_ id: UUID) throws {
        bloqueo.lock();defer{bloqueo.unlock()}
        guard let i=archivo.acciones.firstIndex(where:{$0.id==id})else{throw ErrorAccionIAReversibleIPad.accionNoEncontrada}
        guard archivo.acciones[i].estado == .propuesta else{throw ErrorAccionIAReversibleIPad.estadoInvalido}
        archivo.acciones[i].estado = .descartada;archivo.acciones[i].actualizada=Date();persistir()
    }

    private func persistir(){let e=JSONEncoder();e.dateEncodingStrategy = .iso8601;e.outputFormatting=[.sortedKeys];if let datos=try? e.encode(archivo){try? datos.write(to:url,options:[.atomic,.completeFileProtection])}}
}

enum HashAccionIAIPad {
    static func sha256(_ datos:Data)->String{SHA256.hash(data:datos).map{String(format:"%02x",$0)}.joined()}
}

final class PanelPreviewAccionIAIPad: UIViewController {
    var alAplicar: (() -> Void)?;var alDescartar:(()->Void)?
    private let accion:AccionIAReversibleIPad
    init(accion:AccionIAReversibleIPad){self.accion=accion;super.init(nibName:nil,bundle:nil)}
    required init?(coder:NSCoder){fatalError("init(coder:) no implementado")}
    override func viewDidLoad(){
        super.viewDidLoad();title="Revisar cambio de IA";view.backgroundColor = .systemBackground
        let titulo=UILabel();titulo.text=accion.titulo;titulo.font=.preferredFont(forTextStyle:.title2);titulo.adjustsFontForContentSizeCategory=true;titulo.numberOfLines=0
        let explicacion=UILabel();explicacion.text=accion.explicacion;explicacion.font=.preferredFont(forTextStyle:.body);explicacion.adjustsFontForContentSizeCategory=true;explicacion.numberOfLines=0
        let antes=UITextView();antes.isEditable=false;antes.text=String(data:accion.antes,encoding:.utf8) ?? "Contenido binario";antes.font=.preferredFont(forTextStyle:.body);antes.adjustsFontForContentSizeCategory=true
        let despues=UITextView();despues.isEditable=false;despues.text=String(data:accion.despues,encoding:.utf8) ?? "Contenido binario";despues.font=.preferredFont(forTextStyle:.body);despues.adjustsFontForContentSizeCategory=true
        let columnas=UIStackView(arrangedSubviews:[grupo("Antes",antes),grupo("Propuesta",despues)]);columnas.axis = .horizontal;columnas.distribution = .fillEqually;columnas.spacing=12
        let aplicar=UIButton(type:.system);aplicar.configuration = .filled();aplicar.configuration?.title="Aplicar cambio";aplicar.addTarget(self,action:#selector(aplicarCambio),for:.touchUpInside);aplicar.accessibilityLabel="Aplicar cambio propuesto por IA"
        let descartar=UIButton(type:.system);descartar.configuration = .bordered();descartar.configuration?.title="Descartar";descartar.addTarget(self,action:#selector(descartarCambio),for:.touchUpInside)
        let acciones=UIStackView(arrangedSubviews:[descartar,aplicar]);acciones.axis = .horizontal;acciones.distribution = .fillEqually;acciones.spacing=12
        let pila=UIStackView(arrangedSubviews:[titulo,explicacion,columnas,acciones]);pila.axis = .vertical;pila.spacing=12;pila.translatesAutoresizingMaskIntoConstraints=false;view.addSubview(pila)
        NSLayoutConstraint.activate([pila.topAnchor.constraint(equalTo:view.safeAreaLayoutGuide.topAnchor,constant:16),pila.leadingAnchor.constraint(equalTo:view.leadingAnchor,constant:16),pila.trailingAnchor.constraint(equalTo:view.trailingAnchor,constant:-16),pila.bottomAnchor.constraint(equalTo:view.safeAreaLayoutGuide.bottomAnchor,constant:-16),columnas.heightAnchor.constraint(greaterThanOrEqualToConstant:260)])
    }
    private func grupo(_ nombre:String,_ vista:UIView)->UIView{let l=UILabel();l.text=nombre;l.font=.preferredFont(forTextStyle:.headline);let p=UIStackView(arrangedSubviews:[l,vista]);p.axis = .vertical;p.spacing=6;return p}
    @objc private func aplicarCambio(){alAplicar?()};@objc private func descartarCambio(){alDescartar?()}
}
