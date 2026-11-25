import SwiftUI

/// Punto de entrada de la calculadora **cuando se compila ella sola**.
///
/// Desde el 2026-08-10 las calculadoras no son una app aparte: viven dentro del
/// cuaderno de tinta como panel, y el `@main` de la app fusionada es el
/// `AppDelegate` de `nativo/ipad/Fuente/AppPrincipal.swift`. Un objetivo de
/// Xcode no admite dos `@main`, asi que este queda detras de una bandera.
///
/// No se borra por dos razones: deja la calculadora arrancable por su cuenta
/// para depurarla sin el lienzo delante, y documenta cual era su raiz.
///
/// Para compilarla sola: `SWIFT_ACTIVE_COMPILATION_CONDITIONS = CALCULADORA_SOLA`
/// y devolver `INFOPLIST_FILE` a `CalculadorasCasio/Info.plist`.
#if CALCULADORA_SOLA
@main
#endif
struct CalculadorasCasioApp: App {
    var body: some Scene {
        WindowGroup {
            VistaSelector()
                .preferredColorScheme(.dark)
        }
    }
}
