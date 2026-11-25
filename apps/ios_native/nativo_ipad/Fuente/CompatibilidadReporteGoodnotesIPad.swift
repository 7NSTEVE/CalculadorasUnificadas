import Foundation

extension ReporteSuperioridadGoodnotesIPad {
    /// Única traducción autorizada de un reporte a una afirmación binaria.
    /// Evita que UI/marketing inventen una regla paralela a la del gate.
    var permiteAfirmarSuperioridad: Bool {
        veredicto == .supera
    }
}
