import UIKit

/// Importar imágenes como hojas del documento.
enum DocumentosImagen {
    
    struct ImagenImportada {
        let tamano: CGSize
        let imagen: UIImage
    }
    
    static func importar(_ url: URL) -> ImagenImportada? {
        let necesitaPermiso = url.startAccessingSecurityScopedResource()
        defer { if necesitaPermiso { url.stopAccessingSecurityScopedResource() } }
        
        guard let data = try? Data(contentsOf: url),
              let imagen = UIImage(data: data) else {
            return nil
        }
        
        return ImagenImportada(
            tamano: imagen.size,
            imagen: imagen
        )
    }
}
