import Foundation
import UIKit

enum TipoIncidenciaAccesibilidadIPad: String, Codable, CaseIterable {
    case etiquetaFaltante, objetivoTactilPequeno, controlSinAccion, textoNoEscalable, contrastePersonalizado
}
struct IncidenciaAccesibilidadIPad: Equatable, Identifiable {
    var id:String{"\(tipo.rawValue):\(ruta)"}; var tipo:TipoIncidenciaAccesibilidadIPad; var ruta:String; var detalle:String
}
struct EstadoAccesibilidadSistemaIPad: Equatable {
    var voiceOver:Bool;var reducirMovimiento:Bool;var reducirTransparencia:Bool;var aumentarContraste:Bool;var textoNegrita:Bool
    static var actual:Self{.init(voiceOver:UIAccessibility.isVoiceOverRunning,reducirMovimiento:UIAccessibility.isReduceMotionEnabled,reducirTransparencia:UIAccessibility.isReduceTransparencyEnabled,aumentarContraste:UIAccessibility.isDarkerSystemColorsEnabled,textoNegrita:UIAccessibility.isBoldTextEnabled)}
}
enum AnunciadorAccesibilidadIPad {
    static func anunciar(_ texto:String){guard !texto.isEmpty else{return};UIAccessibility.post(notification:.announcement,argument:texto)}
    static func foco(en vista:UIView){UIAccessibility.post(notification:.layoutChanged,argument:vista)}
}
enum AplicadorAccesibilidadIPad {
    @discardableResult static func control(_ vista:UIView,etiqueta:String,ayuda:String?=nil,valor:String?=nil,traits:UIAccessibilityTraits?=nil,objetivoMinimo:CGFloat=44)->UIView{
        vista.isAccessibilityElement=true;vista.accessibilityLabel=etiqueta;vista.accessibilityHint=ayuda;vista.accessibilityValue=valor;if let traits{vista.accessibilityTraits=traits}
        if objetivoMinimo>0{
            if vista.constraints.first(where:{$0.firstAttribute == .height && $0.relation == .greaterThanOrEqual})==nil{vista.heightAnchor.constraint(greaterThanOrEqualToConstant:objetivoMinimo).isActive=true}
            if vista.constraints.first(where:{$0.firstAttribute == .width && $0.relation == .greaterThanOrEqual})==nil{vista.widthAnchor.constraint(greaterThanOrEqualToConstant:objetivoMinimo).isActive=true}
        };return vista
    }
    static func textoDinamico(_ label:UILabel,estilo:UIFont.TextStyle){label.font = .preferredFont(forTextStyle:estilo);label.adjustsFontForContentSizeCategory=true}
    static func textoDinamico(_ textView:UITextView,estilo:UIFont.TextStyle){textView.font = .preferredFont(forTextStyle:estilo);textView.adjustsFontForContentSizeCategory=true}
    static func animar(duracion:TimeInterval=0.25,cambios:@escaping()->Void,completion:((Bool)->Void)?=nil){if UIAccessibility.isReduceMotionEnabled{UIView.performWithoutAnimation{cambios();completion?(true)}}else{UIView.animate(withDuration:duracion,animations:cambios,completion:completion)}}
}
enum AuditorAccesibilidadIPad {
    static func auditar(_ raiz:UIView)->[IncidenciaAccesibilidadIPad]{var s:[IncidenciaAccesibilidadIPad]=[];recorrer(raiz,ruta:nombre(raiz),salida:&s);return s}
    private static func recorrer(_ vista:UIView,ruta:String,salida:inout[IncidenciaAccesibilidadIPad]){
        if esControlInteractivo(vista),!vista.isHidden,vista.alpha>0.01{
            let e=vista.accessibilityLabel?.trimmingCharacters(in:.whitespacesAndNewlines) ?? "";if e.isEmpty{salida.append(.init(tipo:.etiquetaFaltante,ruta:ruta,detalle:"El control interactivo no tiene accessibilityLabel."))}
            let t=vista.bounds.size;if t.width>0,t.height>0,(t.width<44 || t.height<44){salida.append(.init(tipo:.objetivoTactilPequeno,ruta:ruta,detalle:"Área táctil \(Int(t.width))×\(Int(t.height)); objetivo mínimo 44×44 pt."))}
        }
        if let l=vista as? UILabel,l.font != nil,!l.adjustsFontForContentSizeCategory,l.numberOfLines != 0{salida.append(.init(tipo:.textoNoEscalable,ruta:ruta,detalle:"UILabel no está configurado para Dynamic Type."))}
        if let t=vista as? UITextView,!t.adjustsFontForContentSizeCategory{salida.append(.init(tipo:.textoNoEscalable,ruta:ruta,detalle:"UITextView no está configurado para Dynamic Type."))}
        for(i,s)in vista.subviews.enumerated(){recorrer(s,ruta:"\(ruta)/\(nombre(s))[\(i)]",salida:&salida)}
    }
    private static func esControlInteractivo(_ v:UIView)->Bool{if let c=v as? UIControl{return c.isEnabled && c.isUserInteractionEnabled};return v.isUserInteractionEnabled && (v.gestureRecognizers?.contains(where:{$0.isEnabled}) == true)}
    private static func nombre(_ v:UIView)->String{if let id=v.accessibilityIdentifier,!id.isEmpty{return id};return String(describing:type(of:v))}
}

struct ColisionAtajoIPad: Equatable {
    var entrada:String;var modificadores:UInt;var acciones:[AccionIPad]
}

/// Audita la fuente de verdad YA existente (`AtajoIPad`). No mantiene otra tabla
/// de shortcuts; sólo detecta combinaciones duplicadas antes de construir UIKeyCommand.
enum AuditorAtajosIPad {
    static func colisiones(_ atajos:[AtajoIPad]=AtajoIPad.predeterminados)->[ColisionAtajoIPad]{
        let grupos=Dictionary(grouping:atajos){"\($0.entrada.lowercased())|\($0.modificadores)"}
        return grupos.values.filter{$0.count>1}.map{g in
            .init(entrada:g[0].entrada.lowercased(),modificadores:g[0].modificadores,acciones:g.map(\.accion).sorted{$0.rawValue<$1.rawValue})
        }.sorted{($0.entrada,$0.modificadores)<($1.entrada,$1.modificadores)}
    }
}
