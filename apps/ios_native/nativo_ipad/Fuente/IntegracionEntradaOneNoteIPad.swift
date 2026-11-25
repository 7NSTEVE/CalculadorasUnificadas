import UIKit

/// Puente UI para que el importador no sea sólo infraestructura: instala una acción visible en
/// Biblioteca y recibe documentos que iPadOS entrega mediante "Abrir con CuadernU".
@MainActor
final class CentroEntradaOneNoteIPad: NSObject {
    static let compartido = CentroEntradaOneNoteIPad()

    private var coordinadores: [String: CoordinadorImportacionOneNoteIPad] = [:]

    func instalarBotonImportar(en escena: UIWindowScene) {
        guard let biblioteca = encontrarBiblioteca(en: escena) else { return }
        if biblioteca.navigationItem.leftBarButtonItems?.contains(where: {
            $0.accessibilityIdentifier == "biblioteca.importar.onenote"
        }) == true { return }

        let boton = UIBarButtonItem(
            image: UIImage(systemName: "square.and.arrow.down"),
            style: .plain,
            target: self,
            action: #selector(importarDesdeBiblioteca)
        )
        boton.accessibilityLabel = "Importar OneNote"
        boton.accessibilityIdentifier = "biblioteca.importar.onenote"

        var actuales = biblioteca.navigationItem.leftBarButtonItems ?? []
        actuales.append(boton)
        biblioteca.navigationItem.leftBarButtonItems = actuales
    }

    func recibir(urls: [URL], en escena: UIWindowScene) {
        let onenote = urls.filter { FormatoImportacionOneNote(url: $0) != .desconocido }
        guard !onenote.isEmpty, let presentador = presentadorVisible(en: escena) else { return }
        coordinador(para: escena).importarURLsExternas(onenote, desde: presentador)
    }

    @objc private func importarDesdeBiblioteca() {
        guard let escena = UIApplication.shared.connectedScenes
            .compactMap({ $0 as? UIWindowScene })
            .filter({ $0.activationState == .foregroundActive })
            .first(where: { encontrarBiblioteca(en: $0) != nil }),
              let biblioteca = encontrarBiblioteca(en: escena) else { return }

        coordinador(para: escena).presentarSelector(desde: biblioteca)
    }

    private func coordinador(para escena: UIWindowScene) -> CoordinadorImportacionOneNoteIPad {
        let id = escena.session.persistentIdentifier
        if let existente = coordinadores[id] { return existente }
        let nuevo = CoordinadorImportacionOneNoteIPad()
        coordinadores[id] = nuevo
        return nuevo
    }

    private func encontrarBiblioteca(en escena: UIWindowScene) -> BibliotecaCuadernosIPad? {
        for ventana in escena.windows where !ventana.isHidden {
            if let nav = ventana.rootViewController as? UINavigationController,
               let biblioteca = nav.viewControllers.first(where: { $0 is BibliotecaCuadernosIPad })
                as? BibliotecaCuadernosIPad {
                return biblioteca
            }
            if let biblioteca = ventana.rootViewController as? BibliotecaCuadernosIPad {
                return biblioteca
            }
        }
        return nil
    }

    private func presentadorVisible(en escena: UIWindowScene) -> UIViewController? {
        guard let raiz = escena.windows
            .filter({ !$0.isHidden })
            .sorted(by: { $0.windowLevel.rawValue > $1.windowLevel.rawValue })
            .first?.rootViewController else { return nil }
        return masVisible(desde: raiz)
    }

    private func masVisible(desde controlador: UIViewController) -> UIViewController {
        if let presentado = controlador.presentedViewController,
           !presentado.isBeingDismissed {
            return masVisible(desde: presentado)
        }
        if let nav = controlador as? UINavigationController,
           let visible = nav.visibleViewController {
            return masVisible(desde: visible)
        }
        if let tab = controlador as? UITabBarController,
           let seleccionado = tab.selectedViewController {
            return masVisible(desde: seleccionado)
        }
        return controlador
    }
}

// Los hooks de escena (openURLContexts / sceneWillEnterForeground) se invocan
// directamente desde EscenaPrincipal en AppPrincipal.swift para evitar
// redeclaración de métodos del protocolo UIWindowSceneDelegate.
