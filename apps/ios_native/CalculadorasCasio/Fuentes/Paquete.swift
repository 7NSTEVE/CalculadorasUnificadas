import Foundation

/// Localiza recursos dentro del paquete de la app (carátulas y ROMs de los 4 modelos v47).
///
/// Busca tanto en la raíz del bundle como en las referencias de carpeta montadas
/// por XcodeGen (`Recursos`, `skins`, `roms`) para garantizar fuente única sin
/// duplicar activos en el monorepo.
enum Paquete {
    static let subcarpetas = ["Recursos", "skins", "roms"]

    static func url(_ nombre: String, extension extensionArchivo: String) -> URL? {
        if let raiz = Bundle.main.url(forResource: nombre, withExtension: extensionArchivo) {
            return raiz
        }
        for sub in subcarpetas {
            if let encontrada = Bundle.main.url(forResource: nombre,
                                                withExtension: extensionArchivo,
                                                subdirectory: sub) {
                return encontrada
            }
        }
        return nil
    }

    static func directorioRoms() -> URL? {
        if let cg50 = url("SystemROM", extension: "raw") {
            return cg50.deletingLastPathComponent()
        }
        if let cw2 = url("fx991cw_rom", extension: "bin") {
            return cw2.deletingLastPathComponent()
        }
        return Bundle.main.resourceURL
    }

    static func datos(_ nombre: String, extension extensionArchivo: String) -> Data? {
        guard let url = url(nombre, extension: extensionArchivo) else { return nil }
        return try? Data(contentsOf: url)
    }
}
