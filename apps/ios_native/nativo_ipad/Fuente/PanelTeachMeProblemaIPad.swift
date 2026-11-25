import UIKit

final class PanelTeachMeProblemaIPad:UITableViewController{
    private let problemaId:UUID,servicio:ServicioTeachMeProblemasIPad,estudio:CentroEstudioTecnicoIPad
    private var problema:ProblemaBancoIPad?,niveles:[NivelPistaProblemaIPad]=[],pistas:[NivelPistaProblemaIPad:PistaGraduadaProblemaIPad]=[:]
    init(problemaId:UUID,servicio:ServicioTeachMeProblemasIPad = .init(),estudio:CentroEstudioTecnicoIPad = .compartido){self.problemaId=problemaId;self.servicio=servicio;self.estudio=estudio;super.init(style:.insetGrouped)}
    required init?(coder:NSCoder){fatalError("init(coder:) no implementado")}
    override func viewDidLoad(){super.viewDidLoad();title="Teach Me";navigationItem.leftBarButtonItem=UIBarButtonItem(barButtonSystemItem:.close,target:self,action:#selector(cerrar));recargar()}
    private func recargar(){problema=estudio.instantanea().problemas.first{$0.id==problemaId};niveles=problema.map(GeneradorPistasGraduadasIPad.disponibles) ?? [];tableView.reloadData()}
    override func numberOfSections(in tableView:UITableView)->Int{2}
    override func tableView(_ tableView:UITableView,titleForHeaderInSection section:Int)->String?{section==0 ? "Problema":"Pistas graduadas"}
    override func tableView(_ tableView:UITableView,numberOfRowsInSection section:Int)->Int{section==0 ? 1:niveles.count}
    override func tableView(_ tableView:UITableView,cellForRowAt indexPath:IndexPath)->UITableViewCell{
        let c=UITableViewCell(style:.subtitle,reuseIdentifier:nil)
        if indexPath.section==0{c.textLabel?.text=problema?.enunciado ?? "Problema no disponible";c.textLabel?.numberOfLines=0;c.detailTextLabel?.text=problema.map{"\($0.tema) · \($0.dificultad.nombre)"};c.selectionStyle = .none;return c}
        let n=niveles[indexPath.row];c.textLabel?.text="\(n.rawValue). \(n.nombre)"
        if let p=pistas[n]{c.detailTextLabel?.text=p.texto;c.detailTextLabel?.numberOfLines=8;c.imageView?.image=UIImage(systemName:p.revelaSolucion ? "eye.fill":"lightbulb.fill")}else{c.detailTextLabel?.text=n == .reveladoParcial ? "Muestra únicamente el inicio de la solución vinculada.":"Toque para solicitar esta pista sin revelar la solución.";c.imageView?.image=UIImage(systemName:"lightbulb");c.accessoryType = .disclosureIndicator}
        c.accessibilityLabel="Pista nivel \(n.rawValue), \(n.nombre)";c.accessibilityValue=pistas[n]?.texto ?? "No solicitada";return c
    }
    override func tableView(_ tableView:UITableView,didSelectRowAt indexPath:IndexPath){tableView.deselectRow(at:indexPath,animated:true);guard indexPath.section==1 else{return};let n=niveles[indexPath.row];guard pistas[n]==nil else{return};do{let p=try servicio.pista(problemaId:problemaId,nivel:n);pistas[n]=p;tableView.reloadRows(at:[indexPath],with:.automatic);AnunciadorAccesibilidadIPad.anunciar("Pista \(n.rawValue): \(p.texto)")}catch{let a=UIAlertController(title:"Pista no disponible",message:error.localizedDescription,preferredStyle:.alert);a.addAction(UIAlertAction(title:"Cerrar",style:.cancel));present(a,animated:true)}}
    @objc private func cerrar(){dismiss(animated:true)}
}
