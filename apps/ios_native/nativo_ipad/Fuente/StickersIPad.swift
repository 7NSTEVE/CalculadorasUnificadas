import Foundation
import UIKit
import UniformTypeIdentifiers

struct ResultadoImportacionStickerIPad {
    var elemento: ElementoReutilizableIPad
    var coleccion: ColeccionElementosReutilizablesIPad
}

enum ErrorStickerIPad: LocalizedError {
    case imagenInvalida
    case archivoDemasiadoGrande(Int)

    var errorDescription: String? {
        switch self {
        case .imagenInvalida: return "El archivo no contiene una imagen utilizable como sticker."
        case .archivoDemasiadoGrande(let bytes):
            return "El sticker ocupa \(ByteCountFormatter.string(fromByteCount: Int64(bytes), countStyle: .file)); reduzca su tamaño antes de importarlo."
        }
    }
}

final class GestorStickersIPad {
    static let compartido = GestorStickersIPad()
    static let etiquetaSistema = "cuadernu:stickers"
    static let maximoSticker = 20 * 1_024 * 1_024

    private let biblioteca: BibliotecaElementosReutilizablesIPad

    init(biblioteca: BibliotecaElementosReutilizablesIPad = .compartido) {
        self.biblioteca = biblioteca
    }

    func colecciones() -> [ColeccionElementosReutilizablesIPad] {
        biblioteca.instantanea().colecciones.filter {
            !$0.archivada && $0.etiquetas.contains(Self.etiquetaSistema)
        }
    }

    func stickers(consulta: String = "", limite: Int = 200) -> [ElementoReutilizableIPad] {
        let ids = Set(colecciones().map(\.id))
        return biblioteca.buscar(consulta, limite: limite).filter {
            ids.contains($0.coleccionId) && $0.tipo == .imagen
        }
    }

    @discardableResult
    func crearColeccion(nombre: String) throws -> ColeccionElementosReutilizablesIPad {
        try biblioteca.crearColeccion(nombre: nombre,
                                      etiquetas: [Self.etiquetaSistema, "sticker"])
    }

    @discardableResult
    func importar(datos: Data,
                  nombre: String,
                  extensionArchivo: String,
                  coleccionId: UUID? = nil,
                  etiquetas: [String] = []) throws -> ResultadoImportacionStickerIPad {
        guard datos.count <= Self.maximoSticker else {
            throw ErrorStickerIPad.archivoDemasiadoGrande(datos.count)
        }
        guard let imagen = UIImage(data: datos),
              imagen.size.width > 0, imagen.size.height > 0 else {
            throw ErrorStickerIPad.imagenInvalida
        }
        let coleccion: ColeccionElementosReutilizablesIPad
        if let coleccionId,
           let existente = colecciones().first(where: { $0.id == coleccionId }) {
            coleccion = existente
        } else if let existente = colecciones().first {
            coleccion = existente
        } else {
            coleccion = try crearColeccion(nombre: "Mis stickers")
        }
        let preview = Self.preview(imagen)
        let elemento = try biblioteca.guardarElemento(
            coleccionId: coleccion.id,
            tipo: .imagen,
            nombre: nombre.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty
                ? "Sticker" : nombre,
            detalle: "Sticker reutilizable",
            etiquetas: Array(Set(etiquetas + ["sticker"])),
            payload: datos,
            extensionArchivo: extensionArchivo,
            preview: preview,
            extensionPreview: "png",
            anchoNatural: Double(imagen.size.width),
            altoNatural: Double(imagen.size.height))
        return .init(elemento: elemento, coleccion: coleccion)
    }

    func importar(url: URL, coleccionId: UUID? = nil) throws -> ResultadoImportacionStickerIPad {
        let acceso = url.startAccessingSecurityScopedResource()
        defer { if acceso { url.stopAccessingSecurityScopedResource() } }
        let datos = try Data(contentsOf: url, options: [.mappedIfSafe])
        let nombre = url.deletingPathExtension().lastPathComponent
        return try importar(datos: datos, nombre: nombre,
                            extensionArchivo: url.pathExtension,
                            coleccionId: coleccionId)
    }

    func favorito(_ id: UUID, valor: Bool) { biblioteca.favorito(elementoId: id, valor: valor) }
    func registrarUso(_ id: UUID) { biblioteca.registrarUso(elementoId: id) }
    func payload(_ id: UUID) throws -> Data? { try biblioteca.payload(elementoId: id) }

    private static func preview(_ imagen: UIImage) -> Data? {
        let limite: CGFloat = 320
        let escala = min(1, limite / max(imagen.size.width, imagen.size.height))
        let tamano = CGSize(width: max(1, imagen.size.width * escala),
                            height: max(1, imagen.size.height * escala))
        let renderer = UIGraphicsImageRenderer(size: tamano)
        return renderer.pngData { _ in imagen.draw(in: CGRect(origin: .zero, size: tamano)) }
    }
}

final class PanelStickersIPad: UICollectionViewController, UIDocumentPickerDelegate, UISearchResultsUpdating {
    var alInsertar: ((ElementoReutilizableIPad, Data) -> Void)?
    private let gestor: GestorStickersIPad
    private let buscador = UISearchController(searchResultsController: nil)
    private var stickers: [ElementoReutilizableIPad] = []

    init(gestor: GestorStickersIPad = .compartido) {
        self.gestor = gestor
        let layout = UICollectionViewFlowLayout()
        layout.itemSize = CGSize(width: 112, height: 132)
        layout.minimumInteritemSpacing = 12; layout.minimumLineSpacing = 14
        super.init(collectionViewLayout: layout)
    }
    required init?(coder: NSCoder) { fatalError("init(coder:) no implementado") }

    override func viewDidLoad() {
        super.viewDidLoad()
        title = "Stickers"
        collectionView.backgroundColor = .systemBackground
        collectionView.register(UICollectionViewCell.self, forCellWithReuseIdentifier: "sticker")
        buscador.searchResultsUpdater = self
        buscador.searchBar.placeholder = "Buscar stickers"
        navigationItem.searchController = buscador
        navigationItem.rightBarButtonItem = UIBarButtonItem(
            barButtonSystemItem: .add, target: self, action: #selector(importar))
        recargar()
    }

    func updateSearchResults(for searchController: UISearchController) { recargar() }
    private func recargar() {
        stickers = gestor.stickers(consulta: buscador.searchBar.text ?? "")
        collectionView.reloadData()
    }

    override func collectionView(_ collectionView: UICollectionView,
                                 numberOfItemsInSection section: Int) -> Int { stickers.count }

    override func collectionView(_ collectionView: UICollectionView,
                                 cellForItemAt indexPath: IndexPath) -> UICollectionViewCell {
        let cell = collectionView.dequeueReusableCell(withReuseIdentifier: "sticker", for: indexPath)
        cell.contentView.subviews.forEach { $0.removeFromSuperview() }
        let item = stickers[indexPath.item]
        let imagen = UIImageView(frame: CGRect(x: 6, y: 6, width: 100, height: 96))
        imagen.contentMode = .scaleAspectFit
        if let datos = try? gestor.payload(item.id) {
            imagen.image = UIImage(data: datos)
        }
        let titulo = UILabel(frame: CGRect(x: 4, y: 104, width: 104, height: 24))
        titulo.text = item.nombre; titulo.textAlignment = .center
        titulo.font = .preferredFont(forTextStyle: .caption1)
        titulo.adjustsFontForContentSizeCategory = true
        titulo.lineBreakMode = .byTruncatingTail
        cell.contentView.addSubview(imagen); cell.contentView.addSubview(titulo)
        cell.contentView.layer.cornerRadius = 12
        cell.contentView.backgroundColor = .secondarySystemBackground
        cell.isAccessibilityElement = true
        cell.accessibilityLabel = "Sticker \(item.nombre)"
        cell.accessibilityHint = "Doble toque para insertar"
        return cell
    }

    override func collectionView(_ collectionView: UICollectionView, didSelectItemAt indexPath: IndexPath) {
        let item = stickers[indexPath.item]
        guard let datos = try? gestor.payload(item.id) else { return }
        gestor.registrarUso(item.id)
        alInsertar?(item, datos)
    }

    @objc private func importar() {
        let tipos: [UTType] = [.png, .jpeg, .gif, .webP, .image]
        let picker = UIDocumentPickerViewController(forOpeningContentTypes: tipos, asCopy: true)
        picker.allowsMultipleSelection = true; picker.delegate = self
        present(picker, animated: true)
    }

    func documentPicker(_ controller: UIDocumentPickerViewController, didPickDocumentsAt urls: [URL]) {
        for url in urls { _ = try? gestor.importar(url: url) }
        recargar()
    }
}
