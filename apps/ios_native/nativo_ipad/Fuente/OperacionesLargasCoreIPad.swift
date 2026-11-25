import Foundation

enum EstadoOperacionLargaIPad: String, Codable { case preparando, ejecutando, cancelando, completada, cancelada, fallida }
struct InstantaneaOperacionLargaIPad: Identifiable, Equatable { var id: UUID; var titulo: String; var fase: String; var progreso: Double; var bytesProcesados: Int64; var bytesTotales: Int64?; var estado: EstadoOperacionLargaIPad; var puedeCancelar: Bool; var mensajeError: String?; var inicio: Date; var actualizado: Date }
enum ErrorOperacionLargaIPad: LocalizedError, Equatable {
    case cancelada, archivoDemasiadoGrande(Int64, maximo: Int64), espacioInsuficiente(requerido: Int64, disponible: Int64)
    var errorDescription: String? { switch self { case .cancelada: return "La operación fue cancelada."; case .archivoDemasiadoGrande(let b, let m): return "Archivo de \(b) bytes excede el límite preventivo de \(m) bytes."; case .espacioInsuficiente(let r, let d): return "Se requieren \(r) bytes y hay \(d) disponibles." } }
}
final class TokenCancelacionOperacionIPad { private let bloqueo=NSLock(); private var cancelado=false; func cancelar(){bloqueo.lock();cancelado=true;bloqueo.unlock()}; var estaCancelado:Bool{bloqueo.lock();defer{bloqueo.unlock()};return cancelado}; func verificar()throws{if estaCancelado{throw ErrorOperacionLargaIPad.cancelada}} }
final class ContextoOperacionLargaIPad {
    let token:TokenCancelacionOperacionIPad; private let progreso:(String,Double,Int64,Int64?)->Void; private let bloqueo=NSLock(); private var rollbacks:[()->Void]=[]; private var limpiezas:[()->Void]=[]
    init(token:TokenCancelacionOperacionIPad, progreso:@escaping(String,Double,Int64,Int64?)->Void){self.token=token;self.progreso=progreso}
    func reportar(fase:String,progreso valor:Double,bytesProcesados:Int64=0,bytesTotales:Int64?=nil)throws{try token.verificar();progreso(fase,min(max(valor,0),1),max(bytesProcesados,0),bytesTotales)}
    func registrarRollback(_ a:@escaping()->Void){bloqueo.lock();rollbacks.append(a);bloqueo.unlock()}; func registrarLimpieza(_ a:@escaping()->Void){bloqueo.lock();limpiezas.append(a);bloqueo.unlock()}
    fileprivate func rollback(){bloqueo.lock();let a=Array(rollbacks.reversed());rollbacks.removeAll();bloqueo.unlock();a.forEach{$0()}}; fileprivate func limpiar(){bloqueo.lock();let a=Array(limpiezas.reversed());limpiezas.removeAll();bloqueo.unlock();a.forEach{$0()}}
}
final class CoordinadorOperacionesLargasIPad {
    static let compartido=CoordinadorOperacionesLargasIPad(); static let cambio=Notification.Name("CuadernU.OperacionLarga.Cambio"); private struct Registro{var instantanea:InstantaneaOperacionLargaIPad;var token:TokenCancelacionOperacionIPad}; private let bloqueo=NSRecursiveLock(); private let cola=DispatchQueue(label:"cuadernu.operaciones-largas",qos:.utility,attributes:.concurrent); private var registros:[UUID:Registro]=[:]
    func instantaneas(incluirTerminadas:Bool=false)->[InstantaneaOperacionLargaIPad]{bloqueo.lock();defer{bloqueo.unlock()};return registros.values.map(\.instantanea).filter{incluirTerminadas || [.preparando,.ejecutando,.cancelando].contains($0.estado)}.sorted{$0.inicio<$1.inicio}}
    @discardableResult func ejecutar(titulo:String,bytesTotales:Int64?=nil,trabajo:@escaping(ContextoOperacionLargaIPad)throws->Void,completion:((Result<Void,Error>)->Void)?=nil)->UUID{
        let id=UUID(),token=TokenCancelacionOperacionIPad(),ahora=Date();let inicial=InstantaneaOperacionLargaIPad(id:id,titulo:titulo,fase:"Preparando",progreso:0,bytesProcesados:0,bytesTotales:bytesTotales,estado:.preparando,puedeCancelar:true,mensajeError:nil,inicio:ahora,actualizado:ahora);bloqueo.lock();registros[id]=Registro(instantanea:inicial,token:token);bloqueo.unlock();publicar(id)
        cola.async{[weak self] in guard let self else{return};self.actualizar(id){$0.estado = .ejecutando;$0.fase="En curso"};let contexto=ContextoOperacionLargaIPad(token:token){fase,valor,hechos,total in self.actualizar(id){$0.fase=fase;$0.progreso=valor;$0.bytesProcesados=hechos;if let total{$0.bytesTotales=total}}};let resultado:Result<Void,Error>;do{try token.verificar();try trabajo(contexto);try token.verificar();self.actualizar(id){$0.estado = .completada;$0.fase="Completada";$0.progreso=1;$0.puedeCancelar=false};resultado = .success(())}catch{contexto.rollback();let c=token.estaCancelado || ((error as? ErrorOperacionLargaIPad) == .cancelada);self.actualizar(id){$0.estado=c ? .cancelada:.fallida;$0.fase=c ? "Cancelada":"Falló";$0.puedeCancelar=false;$0.mensajeError=c ? nil:error.localizedDescription};resultado = .failure(error)};contexto.limpiar();if let completion{DispatchQueue.main.async{completion(resultado)}}};return id
    }
    func cancelar(_ id:UUID){bloqueo.lock();guard var r=registros[id],[.preparando,.ejecutando].contains(r.instantanea.estado)else{bloqueo.unlock();return};r.token.cancelar();r.instantanea.estado = .cancelando;r.instantanea.fase="Cancelando…";r.instantanea.actualizado=Date();registros[id]=r;bloqueo.unlock();publicar(id)}
    func descartarTerminadas(){bloqueo.lock();registros=registros.filter{[.preparando,.ejecutando,.cancelando].contains($0.value.instantanea.estado)};bloqueo.unlock();publicar(nil)}
    private func actualizar(_ id:UUID,_ cambios:(inout InstantaneaOperacionLargaIPad)->Void){bloqueo.lock();guard var r=registros[id]else{bloqueo.unlock();return};cambios(&r.instantanea);r.instantanea.actualizado=Date();registros[id]=r;bloqueo.unlock();publicar(id)}
    private func publicar(_ id:UUID?){DispatchQueue.main.async{NotificationCenter.default.post(name:Self.cambio,object:self,userInfo:id.map{["id":$0]})}}
}
enum PreflightOperacionLargaIPad {
    static func validarArchivo(_ url:URL,maximoBytes:Int64,multiplicadorEspacio:Double=2,archivos:FileManager = .default)throws->Int64{
        let bytes=Int64(try url.resourceValues(forKeys:[.fileSizeKey]).fileSize ?? 0);guard bytes<=maximoBytes else{throw ErrorOperacionLargaIPad.archivoDemasiadoGrande(bytes,maximo:maximoBytes)};let requerido=Int64(Double(bytes)*max(multiplicadorEspacio,1));let base=archivos.urls(for:.documentDirectory,in:.userDomainMask).first ?? url;let capacidades=try? base.resourceValues(forKeys:[.volumeAvailableCapacityForImportantUsageKey]);if let disponible=capacidades?.volumeAvailableCapacityForImportantUsage,disponible<requerido{throw ErrorOperacionLargaIPad.espacioInsuficiente(requerido:requerido,disponible:disponible)};return bytes
    }
}
