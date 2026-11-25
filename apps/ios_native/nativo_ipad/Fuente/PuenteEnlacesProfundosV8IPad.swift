import UIKit

/// Adopta el codec UUID v1 en la ruta de entrada sin romper los observadores
/// legacy. `CentroEnlacesProfundos` sigue recibiendo todos los URLs; este puente
/// únicamente consume los que puede validar con el contrato nuevo.
@MainActor
final class PuenteEnlacesProfundosV8IPad {
    static let compartido = PuenteEnlacesProfundosV8IPad()

    private var observador: NSObjectProtocol?
    private var instalado = false

    private init() {}

    func instalarSiHaceFalta() {
        guard !instalado else { return }
        instalado = true
        observador = NotificationCenter.default.addObserver(
            forName: .enlaceProfundoTinta,
            object: nil,
            queue: .main) { [weak self] notificacion in
                guard let url = notificacion.object as? URL else { return }
                Task { @MainActor [weak self] in
                    self?.procesarSiEsV1(url)
                }
            }

        if let pendiente = CentroEnlacesProfundos.compartido.pendiente {
            procesarSiEsV1(pendiente)
        }
    }

    @discardableResult
    func procesarSiEsV1(_ url: URL) -> Bool {
        guard (try? CodecEnlacesInternosIPad.decodificar(url)) != nil,
              let escena = escenaPreferida() else { return false }
        _ = CoordinadorNavegacionEnlacesIPad.compartido.abrir(url, desde: escena)
        return true
    }

    private func escenaPreferida() -> UIWindowScene? {
        let escenas = UIApplication.shared.connectedScenes
            .compactMap { $0 as? UIWindowScene }
            .filter { $0.activationState == .foregroundActive }

        if let escenaClave = escenas.first(where: { escena in
            escena.windows.contains(where: \.isKeyWindow)
        }) {
            return escenaClave
        }
        return escenas.first
    }

    deinit {
        if let observador {
            NotificationCenter.default.removeObserver(observador)
        }
    }
}
