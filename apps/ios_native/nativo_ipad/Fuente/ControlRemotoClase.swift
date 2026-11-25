import Foundation

enum TipoCapturaRemota: String, Codable {
    case pizarra
    case diapositiva
    case documento
    case fotoLibre
}

enum ComandoRemoto: String, Codable {
    case mePerdi
    case importante
    case examen
    case siguienteSlide
    case anteriorSlide
    case deshacer
    case foto
}

struct MetadatosCapturaRemota: Codable {
    var id = UUID()
    var tipo: TipoCapturaRemota
    var fecha = Date()
    var nombre: String
    var corregirPizarra: Bool
}

struct CapturaRemotaRecibida {
    let metadatos: MetadatosCapturaRemota
    let adjunto: AdjuntoImportado
}

extension Notification.Name {
    static let capturaRemotaRecibida = Notification.Name(
        "Tinta.capturaRemotaRecibida")
    static let comandoRemotoRecibido = Notification.Name(
        "Tinta.comandoRemotoRecibido")
}
