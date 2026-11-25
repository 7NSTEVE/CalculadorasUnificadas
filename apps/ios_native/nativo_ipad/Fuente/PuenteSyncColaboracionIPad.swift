import Foundation

enum PuenteSyncColaboracionIPad {
    static func operacion(documentoId: DocumentoPersistenteID,
                          sesionId: UUID,
                          tipo: TipoOperacionColaborativa,
                          objetoId: String,
                          carga: Data? = nil,
                          tumba: Bool = false,
                          rol: RolColaboracion,
                          reloj: RelojLamportColaboracion = .compartido)
        -> OperacionColaborativa {
        OperacionColaborativa(
            sesionId: sesionId,
            documentoId: documentoId.claveCompatibilidadUInt64,
            marca: reloj.siguiente(),
            tipo: tipo,
            objetoId: objetoId,
            carga: carga,
            tumba: tumba,
            rolAutor: rol)
    }
}
