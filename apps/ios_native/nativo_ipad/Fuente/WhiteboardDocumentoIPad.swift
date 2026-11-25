import Foundation
import UIKit

struct BoardWhiteboardIPad: Codable, Equatable, Identifiable {
    var id: UUID = UUID()
    var titulo: String
    var plantillaId: UUID?
    var colorFondoARGB: UInt32 = 0xFFFFFFFF
    var orden: Int
    var creada = Date()
    var actualizada = Date()

    var documentoCanvasId: DocumentoPersistenteID { DocumentoPersistenteID(id) }
}

struct DocumentoWhiteboardIPad: Codable, Equatable, Identifiable {
    var id: DocumentoPersistenteID
    var titulo: String
    var boards: [BoardWhiteboardIPad]
    var creado = Date()
    var actualizado = Date()
    var revision: UInt64 = 0
}

enum ErrorWhiteboardDocumentoIPad: LocalizedError, Equatable {
    case tituloVacio
    case boardInexistente
    case minimoUnBoard
    case indiceInvalido

    var errorDescription: String? {
        switch self {
        case .tituloVacio: return "El Whiteboard necesita un título."
        case .boardInexistente: return "El board ya no existe."
        case .minimoUnBoard: return "Un Whiteboard debe conservar al menos un board."
        case .indiceInvalido: return "La posición solicitada no existe."
        }
    }
}

final class RepositorioWhiteboardIPad {
    static let compartido = RepositorioWhiteboardIPad()
    private let bloqueo = NSRecursiveLock()
    private let base: URL

    init(base: URL? = nil, archivos: FileManager = .default) {
        self.base = base ?? archivos.urls(for: .applicationSupportDirectory, in: .userDomainMask)[0]
            .appendingPathComponent("Whiteboards", isDirectory: true)
        try? archivos.createDirectory(at: self.base, withIntermediateDirectories: true)
    }

    @discardableResult
    func crear(titulo: String,
               plantillaInicial: UUID? = nil,
               colorARGB: UInt32 = 0xFFFFFFFF) throws -> DocumentoWhiteboardIPad {
        let limpio = titulo.trimmingCharacters(in: .whitespacesAndNewlines)
        guard !limpio.isEmpty else { throw ErrorWhiteboardDocumentoIPad.tituloVacio }
        bloqueo.lock(); defer { bloqueo.unlock() }
        let docId = DocumentoPersistenteID()
        let board = BoardWhiteboardIPad(
            titulo: "Board 1", plantillaId: plantillaInicial,
            colorFondoARGB: colorARGB, orden: 0)
        let documento = DocumentoWhiteboardIPad(id: docId, titulo: limpio, boards: [board])
        try persistirSinBloqueo(documento)
        return documento
    }

    func cargar(_ id: DocumentoPersistenteID) throws -> DocumentoWhiteboardIPad? {
        bloqueo.lock(); defer { bloqueo.unlock() }
        let url = archivo(id)
        guard FileManager.default.fileExists(atPath: url.path) else { return nil }
        let d = JSONDecoder(); d.dateDecodingStrategy = .iso8601
        let doc = try d.decode(DocumentoWhiteboardIPad.self, from: Data(contentsOf: url))
        return normalizado(doc)
    }

    func listar() -> [DocumentoWhiteboardIPad] {
        bloqueo.lock(); defer { bloqueo.unlock() }
        let urls = (try? FileManager.default.contentsOfDirectory(
            at: base, includingPropertiesForKeys: nil,
            options: [.skipsHiddenFiles])) ?? []
        let d=JSONDecoder();d.dateDecodingStrategy = .iso8601
        return urls.filter{$0.pathExtension=="json"}.compactMap{try? d.decode(DocumentoWhiteboardIPad.self,from:Data(contentsOf:$0))}
            .map(normalizado).sorted{$0.actualizado>$1.actualizado}
    }

    @discardableResult
    func agregarBoard(documentoId: DocumentoPersistenteID,
                      titulo: String? = nil,
                      plantillaId: UUID? = nil,
                      colorARGB: UInt32 = 0xFFFFFFFF) throws -> BoardWhiteboardIPad {
        try mutar(documentoId) { doc in
            let orden = (doc.boards.map(\.orden).max() ?? -1) + 1
            let board = BoardWhiteboardIPad(
                titulo: titulo?.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty == false
                    ? titulo!.trimmingCharacters(in: .whitespacesAndNewlines)
                    : "Board \(doc.boards.count + 1)",
                plantillaId: plantillaId, colorFondoARGB: colorARGB, orden: orden)
            doc.boards.append(board)
            return board
        }
    }

    func renombrarBoard(documentoId: DocumentoPersistenteID,
                        boardId: UUID, titulo: String) throws {
        let limpio=titulo.trimmingCharacters(in:.whitespacesAndNewlines)
        guard !limpio.isEmpty else{throw ErrorWhiteboardDocumentoIPad.tituloVacio}
        _ = try mutar(documentoId) { doc -> Void in
            guard let i=doc.boards.firstIndex(where:{$0.id==boardId}) else{throw ErrorWhiteboardDocumentoIPad.boardInexistente}
            doc.boards[i].titulo=limpio;doc.boards[i].actualizada=Date()
        }
    }

    func configurarBoard(documentoId: DocumentoPersistenteID,
                         boardId: UUID,
                         plantillaId: UUID?, colorARGB: UInt32) throws {
        _ = try mutar(documentoId) { doc -> Void in
            guard let i=doc.boards.firstIndex(where:{$0.id==boardId}) else{throw ErrorWhiteboardDocumentoIPad.boardInexistente}
            doc.boards[i].plantillaId=plantillaId;doc.boards[i].colorFondoARGB=colorARGB;doc.boards[i].actualizada=Date()
        }
    }

    func moverBoard(documentoId: DocumentoPersistenteID,
                    desde: Int, hasta: Int) throws {
        _ = try mutar(documentoId) { doc -> Void in
            var ordenados=doc.boards.sorted{$0.orden<$1.orden}
            guard ordenados.indices.contains(desde),hasta>=0,hasta<ordenados.count else{throw ErrorWhiteboardDocumentoIPad.indiceInvalido}
            let item=ordenados.remove(at:desde);ordenados.insert(item,at:hasta)
            for i in ordenados.indices{ordenados[i].orden=i;ordenados[i].actualizada=Date()}
            doc.boards=ordenados
        }
    }

    func eliminarBoard(documentoId: DocumentoPersistenteID, boardId: UUID) throws {
        _ = try mutar(documentoId) { doc -> Void in
            guard doc.boards.count>1 else{throw ErrorWhiteboardDocumentoIPad.minimoUnBoard}
            guard doc.boards.contains(where:{$0.id==boardId}) else{throw ErrorWhiteboardDocumentoIPad.boardInexistente}
            doc.boards.removeAll{$0.id==boardId}
            for i in doc.boards.indices{doc.boards[i].orden=i}
        }
    }

    func eliminarDocumento(_ id: DocumentoPersistenteID) throws {
        bloqueo.lock();defer{bloqueo.unlock()}
        let url=archivo(id);if FileManager.default.fileExists(atPath:url.path){try FileManager.default.removeItem(at:url)}
    }

    private func mutar<T>(_ id: DocumentoPersistenteID,
                          _ cambios: (inout DocumentoWhiteboardIPad) throws -> T) throws -> T {
        bloqueo.lock();defer{bloqueo.unlock()}
        guard var doc=try cargarSinBloqueo(id) else{throw ErrorWhiteboardDocumentoIPad.boardInexistente}
        let resultado=try cambios(&doc);doc.revision &+= 1;doc.actualizado=Date();doc=normalizado(doc);try persistirSinBloqueo(doc);return resultado
    }

    private func normalizado(_ doc: DocumentoWhiteboardIPad) -> DocumentoWhiteboardIPad {
        var d=doc;d.boards=d.boards.sorted{if $0.orden != $1.orden{return $0.orden<$1.orden};return $0.creada<$1.creada}
        for i in d.boards.indices{d.boards[i].orden=i};return d
    }
    private func cargarSinBloqueo(_ id: DocumentoPersistenteID)throws->DocumentoWhiteboardIPad?{
        let u=archivo(id);guard FileManager.default.fileExists(atPath:u.path)else{return nil};let d=JSONDecoder();d.dateDecodingStrategy = .iso8601;return try d.decode(DocumentoWhiteboardIPad.self,from:Data(contentsOf:u))
    }
    private func archivo(_ id:DocumentoPersistenteID)->URL{base.appendingPathComponent("\(id.description).json")}
    private func persistirSinBloqueo(_ doc:DocumentoWhiteboardIPad)throws{let e=JSONEncoder();e.dateEncodingStrategy = .iso8601;e.outputFormatting=[.prettyPrinted,.sortedKeys];try e.encode(doc).write(to:archivo(doc.id),options:[.atomic,.completeFileProtection])}
}

final class PanelBoardsWhiteboardIPad:UITableViewController{
    var alAbrirBoard:((BoardWhiteboardIPad)->Void)?
    private let documentoId:DocumentoPersistenteID,repo:RepositorioWhiteboardIPad
    private var documento:DocumentoWhiteboardIPad?
    init(documentoId:DocumentoPersistenteID,repo:RepositorioWhiteboardIPad = .compartido){self.documentoId=documentoId;self.repo=repo;super.init(style:.insetGrouped)}
    required init?(coder:NSCoder){fatalError("init(coder:) no implementado")}
    override func viewDidLoad(){super.viewDidLoad();title="Boards";navigationItem.rightBarButtonItem=UIBarButtonItem(barButtonSystemItem:.add,target:self,action:#selector(agregar));tableView.setEditing(true,animated:false);recargar()}
    private func recargar(){documento=try? repo.cargar(documentoId);tableView.reloadData()}
    override func tableView(_ tableView:UITableView,numberOfRowsInSection section:Int)->Int{documento?.boards.count ?? 0}
    override func tableView(_ tableView:UITableView,cellForRowAt indexPath:IndexPath)->UITableViewCell{let c=UITableViewCell(style:.subtitle,reuseIdentifier:nil);guard let b=documento?.boards[indexPath.row]else{return c};c.textLabel?.text=b.titulo;c.detailTextLabel?.text="Board \(indexPath.row+1)";c.imageView?.image=UIImage(systemName:"rectangle.on.rectangle");c.accessoryType = .disclosureIndicator;c.accessibilityLabel=b.titulo;return c}
    override func tableView(_ tableView:UITableView,didSelectRowAt indexPath:IndexPath){tableView.deselectRow(at:indexPath,animated:true);if let b=documento?.boards[indexPath.row]{alAbrirBoard?(b)}}
    override func tableView(_ tableView:UITableView,canMoveRowAt indexPath:IndexPath)->Bool{true}
    override func tableView(_ tableView:UITableView,moveRowAt sourceIndexPath:IndexPath,to destinationIndexPath:IndexPath){try? repo.moverBoard(documentoId:documentoId,desde:sourceIndexPath.row,hasta:destinationIndexPath.row);recargar()}
    override func tableView(_ tableView:UITableView,commit editingStyle:UITableViewCell.EditingStyle,forRowAt indexPath:IndexPath){if editingStyle == .delete,let b=documento?.boards[indexPath.row]{do{try repo.eliminarBoard(documentoId:documentoId,boardId:b.id);recargar()}catch{let a=UIAlertController(title:"No se puede eliminar",message:error.localizedDescription,preferredStyle:.alert);a.addAction(UIAlertAction(title:"Cerrar",style:.cancel));present(a,animated:true)}}}
    @objc private func agregar(){_ = try? repo.agregarBoard(documentoId:documentoId);recargar()}
}
