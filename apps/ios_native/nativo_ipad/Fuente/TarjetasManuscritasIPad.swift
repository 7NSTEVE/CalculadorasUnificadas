import Foundation
import UIKit

struct CaraTarjetaManuscritaIPad: Codable, Equatable {
    var hashImagen: String
    var claves: [String]
    var region: CGRect
    var fuente: ReferenciaFuente
}

struct TarjetaManuscritaIPad: Codable, Identifiable, Equatable {
    var id = UUID()
    var flashcardId: UUID
    var frente: CaraTarjetaManuscritaIPad
    var reverso: CaraTarjetaManuscritaIPad
    var cubiertasRecall: [UUID] = []
    var creada = Date()
}

private struct ArchivoTarjetasManuscritasIPad: Codable {
    var formato = 1
    var tarjetas: [TarjetaManuscritaIPad] = []
}

final class RepositorioTarjetasManuscritasIPad {
    static let compartido = RepositorioTarjetasManuscritasIPad()
    private let bloqueo = NSRecursiveLock()
    private let url: URL
    private var archivo: ArchivoTarjetasManuscritasIPad

    private init() {
        let soporte = FileManager.default.urls(for: .applicationSupportDirectory,
                                               in: .userDomainMask)[0]
        let carpeta = soporte.appendingPathComponent("Tinta/TarjetasManuscritas",
                                                     isDirectory: true)
        try? FileManager.default.createDirectory(at: carpeta,
                                                 withIntermediateDirectories: true)
        url = carpeta.appendingPathComponent("tarjetas.json")
        let decoder = JSONDecoder(); decoder.dateDecodingStrategy = .iso8601
        archivo = (try? Data(contentsOf: url)).flatMap {
            try? decoder.decode(ArchivoTarjetasManuscritasIPad.self, from: $0)
        } ?? ArchivoTarjetasManuscritasIPad()
    }

    func guardar(_ tarjeta: TarjetaManuscritaIPad) {
        bloqueo.lock(); defer { bloqueo.unlock() }
        archivo.tarjetas.removeAll { $0.flashcardId == tarjeta.flashcardId }
        archivo.tarjetas.append(tarjeta); persistir()
    }

    func tarjeta(flashcardId: UUID) -> TarjetaManuscritaIPad? {
        bloqueo.lock(); defer { bloqueo.unlock() }
        return archivo.tarjetas.first { $0.flashcardId == flashcardId }
    }

    func imagen(cara: CaraTarjetaManuscritaIPad) -> UIImage? {
        guard let url = RepositorioAdjuntos.compartido.url(hash: cara.hashImagen) else {
            return nil
        }
        return UIImage(contentsOfFile: url.path)
    }

    private func persistir() {
        let encoder = JSONEncoder(); encoder.dateEncodingStrategy = .iso8601
        encoder.outputFormatting = [.sortedKeys]
        guard let datos = try? encoder.encode(archivo) else { return }
        try? datos.write(to: url, options: [.atomic, .completeFileProtection])
    }
}

final class VistaCaraTarjetaManuscritaIPad: UIView {
    private let imagen = UIImageView()
    private let etiqueta = UILabel()

    init(imagen: UIImage?, titulo: String) {
        super.init(frame: .zero)
        layer.cornerRadius = 12; layer.borderWidth = 1
        layer.borderColor = UIColor.separator.cgColor; clipsToBounds = true
        imagen.map { self.imagen.image = $0 }
        self.imagen.contentMode = .scaleAspectFit
        self.imagen.translatesAutoresizingMaskIntoConstraints = false
        etiqueta.text = titulo; etiqueta.textAlignment = .center
        etiqueta.backgroundColor = UIColor.systemBackground.withAlphaComponent(0.86)
        etiqueta.translatesAutoresizingMaskIntoConstraints = false
        addSubview(self.imagen); addSubview(etiqueta)
        NSLayoutConstraint.activate([
            self.imagen.leadingAnchor.constraint(equalTo: leadingAnchor),
            self.imagen.trailingAnchor.constraint(equalTo: trailingAnchor),
            self.imagen.topAnchor.constraint(equalTo: topAnchor),
            self.imagen.bottomAnchor.constraint(equalTo: bottomAnchor),
            etiqueta.leadingAnchor.constraint(equalTo: leadingAnchor),
            etiqueta.trailingAnchor.constraint(equalTo: trailingAnchor),
            etiqueta.bottomAnchor.constraint(equalTo: bottomAnchor),
            etiqueta.heightAnchor.constraint(equalToConstant: 28),
            heightAnchor.constraint(greaterThanOrEqualToConstant: 220),
        ])
    }
    required init?(coder: NSCoder) { fatalError("init(coder:) no implementado") }
}
