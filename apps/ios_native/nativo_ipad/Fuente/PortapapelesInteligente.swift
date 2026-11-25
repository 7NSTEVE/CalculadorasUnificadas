import CryptoKit
import Photos
import UIKit
import UniformTypeIdentifiers

enum TipoEntradaPortapapeles:String,Codable { case texto,imagen,archivo,enlace,codigo,ecuacion }
enum ModoPegadoInteligenteIPad:String,CaseIterable {
    case original="Original",imagen="Imagen",texto="Texto",latex="LaTeX"
    case enlace="Link",fuenteCitada="Fuente citada",ocr="OCR local"
    case circuito="Detectar circuito",ecuacion="Detectar ecuación"
    var simbolo:String{switch self{case .original:return"doc.on.clipboard";case .imagen:return"photo";case .texto:return"textformat";case .latex,.ecuacion:return"function";case .enlace:return"link";case .fuenteCitada:return"quote.opening";case .ocr:return"viewfinder";case .circuito:return"resistor"}}
}
struct EntradaPortapapeles:Codable,Identifiable,Equatable {
    var id=UUID();var tipo:TipoEntradaPortapapeles;var texto:String?;var hashAdjunto:String?
    var nombre:String;var fecha=Date();var favorita=false;var temporal=false;var vence:Date?
}

final class HistorialPortapapeles {
    static let compartido=HistorialPortapapeles();private let url:URL;private(set)var entradas:[EntradaPortapapeles]=[]
    var hayCambioPendiente=false
    private init(){let base=FileManager.default.urls(for:.documentDirectory,in:.userDomainMask).first!;let carpeta=base.appendingPathComponent("Portapapeles",isDirectory:true);try? FileManager.default.createDirectory(at:carpeta,withIntermediateDirectories:true);url=carpeta.appendingPathComponent("historial.json");let d=JSONDecoder();d.dateDecodingStrategy = .iso8601;if let datos=try? Data(contentsOf:url),let e=try? d.decode([EntradaPortapapeles].self,from:datos){entradas=e};purgar();NotificationCenter.default.addObserver(forName:UIPasteboard.changedNotification,object:UIPasteboard.general,queue:.main){[weak self]_ in self?.hayCambioPendiente=true}}
    func capturarActual()->EntradaPortapapeles?{let p=UIPasteboard.general;let entrada:EntradaPortapapeles?
        if let imagen=p.image,let adjunto=try? RepositorioAdjuntos.compartido.importar(imagen:imagen,nombre:"Recorte del portapapeles.png"){entrada=EntradaPortapapeles(tipo:.imagen,hashAdjunto:adjunto.hash,nombre:"Imagen \(Date().formatted(date:.omitted,time:.shortened))")}
        else if let url=p.url { if url.isFileURL,let adjunto=try? RepositorioAdjuntos.compartido.importar(url:url){entrada=EntradaPortapapeles(tipo:.archivo,hashAdjunto:adjunto.hash,nombre:adjunto.nombreOriginal)}else{entrada=EntradaPortapapeles(tipo:.enlace,texto:url.absoluteString,nombre:url.host ?? "Enlace")}}
        else if let texto=p.string,!texto.isEmpty{let tipo=clasificar(texto);entrada=EntradaPortapapeles(tipo:tipo,texto:texto,nombre:String(texto.prefix(60)))}else{entrada=nil}
        guard let entrada else{return nil};agregar(entrada);hayCambioPendiente=false;return entrada}
    func agregar(_ entrada:EntradaPortapapeles){let clave=entrada.hashAdjunto ?? entrada.texto.map(hash) ?? entrada.id.uuidString;entradas.removeAll{($0.hashAdjunto ?? $0.texto.map(hash) ?? $0.id.uuidString)==clave};entradas.insert(entrada,at:0);if entradas.count>300{entradas=Array(entradas.prefix(300))};persistir()}
    func alternarFavorita(_ id:UUID){guard let i=entradas.firstIndex(where:{$0.id==id})else{return};entradas[i].favorita.toggle();persistir()}
    func hacerTemporal(_ id:UUID,dias:Int=7){guard let i=entradas.firstIndex(where:{$0.id==id})else{return};entradas[i].temporal=true;entradas[i].vence=Calendar.current.date(byAdding:.day,value:dias,to:Date());persistir()}
    func eliminar(_ id:UUID){entradas.removeAll{$0.id==id};persistir()}
    func purgar(){let ahora=Date();entradas.removeAll{$0.temporal && !$0.favorita && ($0.vence ?? .distantFuture)<ahora};persistir()}
    private func clasificar(_ texto:String)->TipoEntradaPortapapeles{if texto.contains("\n#include")||texto.contains("int main")||texto.contains("func "){return .codigo};if texto.range(of:"[=+*/^]",options:.regularExpression) != nil && texto.count<500{return .ecuacion};if URL(string:texto)?.scheme != nil{return .enlace};return .texto}
    private func hash(_ texto:String)->String{SHA256.hash(data:Data(texto.utf8)).map{String(format:"%02x",$0)}.joined()}
    private func persistir(){let e=JSONEncoder();e.dateEncodingStrategy = .iso8601;e.outputFormatting=[.sortedKeys];if let d=try? e.encode(entradas){try? d.write(to:url,options:.atomic)}}
}

final class PanelPortapapeles:UITableViewController {
    var alInsertarTexto:((String)->Void)?;var alInsertarAdjunto:((AdjuntoImportado,TipoMedio)->Void)?
    var alPegarComo:((EntradaPortapapeles,ModoPegadoInteligenteIPad)->Void)?
    var alCerrar:(()->Void)?
    private var entradas:[EntradaPortapapeles]{HistorialPortapapeles.compartido.entradas}
    override func viewDidLoad(){super.viewDidLoad();title="Shelf · recortes";navigationItem.leftBarButtonItem=UIBarButtonItem(barButtonSystemItem:.close,target:self,action:#selector(cerrar));navigationItem.rightBarButtonItems=[UIBarButtonItem(title:"Capturar actual",style:.done,target:self,action:#selector(capturar)),UIBarButtonItem(title:"Screenshots",style:.plain,target:self,action:#selector(screenshots))];tableView.dragDelegate=self;tableView.dragInteractionEnabled=true}
    override func tableView(_ tableView:UITableView,numberOfRowsInSection section:Int)->Int{entradas.count}
    override func tableView(_ tableView:UITableView,cellForRowAt indexPath:IndexPath)->UITableViewCell{let e=entradas[indexPath.row],c=UITableViewCell(style:.subtitle,reuseIdentifier:nil);c.textLabel?.text="\(e.favorita ? "★ ":"")\(e.nombre)";c.detailTextLabel?.text="\(e.tipo.rawValue) · \(e.fecha.formatted(date:.abbreviated,time:.shortened))\(e.temporal ? " · temporal":"")";c.imageView?.image=UIImage(systemName:simbolo(e.tipo));return c}
    override func tableView(_ tableView:UITableView,didSelectRowAt indexPath:IndexPath){elegirModo(entradas[indexPath.row],origen:tableView.cellForRow(at:indexPath))}
    override func tableView(_ tableView: UITableView, trailingSwipeActionsConfigurationForRowAt indexPath: IndexPath) -> UISwipeActionsConfiguration? {
        let e = entradas[indexPath.row]
        let favorita = UIContextualAction(style: .normal, title: e.favorita ? "Quitar ★" : "Favorita") { _, _, fin in
            HistorialPortapapeles.compartido.alternarFavorita(e.id)
            tableView.reloadData()
            fin(true)
        }
        favorita.backgroundColor = .systemYellow
        
        let temporal = UIContextualAction(style: .normal, title: "7 días") { _, _, fin in
            HistorialPortapapeles.compartido.hacerTemporal(e.id)
            tableView.reloadData()
            fin(true)
        }
        temporal.backgroundColor = .systemBlue
        
        let borrar = UIContextualAction(style: .destructive, title: "Borrar") { _, _, fin in
            HistorialPortapapeles.compartido.eliminar(e.id)
            tableView.reloadData()
            fin(true)
        }
        
        return UISwipeActionsConfiguration(actions: [borrar, temporal, favorita])
    }
    private func insertar(_ e:EntradaPortapapeles){if let texto=e.texto{alInsertarTexto?(texto);return};guard let hash=e.hashAdjunto,let url=RepositorioAdjuntos.compartido.url(hash:hash)else{return};let bytes=Int64((try? url.resourceValues(forKeys:[.fileSizeKey]).fileSize) ?? 0);let adjunto=AdjuntoImportado(hash:hash,url:url,nombreOriginal:e.nombre,bytes:bytes);let tipo:TipoMedio=e.tipo == .imagen ? .imagen:(url.pathExtension.lowercased()=="pdf" ? .pdf:.documento);alInsertarAdjunto?(adjunto,tipo)}
    private func elegirModo(_ entrada: EntradaPortapapeles, origen: UIView?) {
        guard alPegarComo != nil else{insertar(entrada);return}
        let hoja=UIAlertController(title:"Pegar como",message:entrada.nombre,preferredStyle:.actionSheet)
        for modo in modos(entrada){hoja.addAction(UIAlertAction(title:modo.rawValue,style:.default){[weak self]_ in self?.alPegarComo?(entrada,modo)})}
        hoja.addAction(UIAlertAction(title:"Cancelar",style:.cancel));hoja.popoverPresentationController?.sourceView=origen ?? view;hoja.popoverPresentationController?.sourceRect=origen?.bounds ?? CGRect(x:view.bounds.midX,y:view.bounds.midY,width:1,height:1);present(hoja,animated:true)
    }
    private func modos(_ entrada:EntradaPortapapeles)->[ModoPegadoInteligenteIPad]{
        switch entrada.tipo{
        case .imagen:return[.original,.imagen,.ocr,.circuito,.ecuacion,.fuenteCitada]
        case .archivo:return[.original,.imagen,.texto,.fuenteCitada]
        case .enlace:return[.original,.enlace,.texto,.fuenteCitada]
        case .codigo:return[.original,.texto,.imagen,.fuenteCitada]
        case .ecuacion:return[.original,.latex,.texto,.imagen,.fuenteCitada]
        case .texto:return[.original,.texto,.imagen,.latex,.enlace,.fuenteCitada]
        }
    }
    @objc private func capturar(){_ = HistorialPortapapeles.compartido.capturarActual();tableView.reloadData()}
    @objc private func screenshots(){ImportadorScreenshotsRecientes.cargar(limite:20){[weak self] imagenes in DispatchQueue.main.async{for (i,imagen) in imagenes.enumerated(){guard let a=try? RepositorioAdjuntos.compartido.importar(imagen:imagen,nombre:"Screenshot reciente \(i+1).png")else{continue};HistorialPortapapeles.compartido.agregar(EntradaPortapapeles(tipo:.imagen,hashAdjunto:a.hash,nombre:a.nombreOriginal))};self?.tableView.reloadData()}}}
    private func simbolo(_ tipo:TipoEntradaPortapapeles)->String{switch tipo{case .texto:return"text.alignleft";case .imagen:return"photo";case .archivo:return"doc";case .enlace:return"link";case .codigo:return"chevron.left.forwardslash.chevron.right";case .ecuacion:return"function"}}
    @objc private func cerrar(){if let alCerrar{alCerrar()}else{dismiss(animated:true)}}
}

extension PanelPortapapeles:UITableViewDragDelegate {
    func tableView(_ tableView:UITableView,itemsForBeginning session:UIDragSession,at indexPath:IndexPath)->[UIDragItem]{guard entradas.indices.contains(indexPath.row)else{return[]};let entrada=entradas[indexPath.row];if let texto=entrada.texto{return[UIDragItem(itemProvider:NSItemProvider(object:texto as NSString))]};guard let hash=entrada.hashAdjunto,let url=RepositorioAdjuntos.compartido.url(hash:hash),let proveedor=NSItemProvider(contentsOf:url)else{return[]};let item=UIDragItem(itemProvider:proveedor);item.localObject=entrada;return[item]}
}

enum ImportadorScreenshotsRecientes {
    static func cargar(limite:Int,completado:@escaping([UIImage])->Void){PHPhotoLibrary.requestAuthorization(for:.readWrite){estado in guard estado == .authorized || estado == .limited else{completado([]);return};let colecciones=PHAssetCollection.fetchAssetCollections(with:.smartAlbum,subtype:.smartAlbumScreenshots,options:nil);guard let album=colecciones.firstObject else{completado([]);return};let opciones=PHFetchOptions();opciones.sortDescriptors=[NSSortDescriptor(key:"creationDate",ascending:false)];opciones.fetchLimit=limite;let activos=PHAsset.fetchAssets(in:album,options:opciones);let gestor=PHImageManager.default(),solicitud=PHImageRequestOptions();solicitud.deliveryMode = .highQualityFormat;solicitud.isNetworkAccessAllowed=false;let grupo=DispatchGroup();var salida=[UIImage]();let cola=DispatchQueue(label:"tinta.screenshots.resultado");activos.enumerateObjects{activo,_,_ in grupo.enter();gestor.requestImage(for:activo,targetSize:CGSize(width:2048,height:2048),contentMode:.aspectFit,options:solicitud){imagen,_ in if let imagen{cola.sync{salida.append(imagen)}};grupo.leave()}};grupo.notify(queue:.global(qos:.userInitiated)){completado(salida)}}}
}
