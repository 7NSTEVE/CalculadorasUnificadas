import Foundation

// Estos repositorios son fuentes DERIVADAS y sincronizan todo su estado mutable
// mediante NSRecursiveLock/NSLock. Declarar Sendable aquí evita que Swift 6
// convierta los cruces intencionales hacia colas utility/userInitiated en una
// cascada de closures @MainActor que terminarían ejecutando búsqueda/OCR en UI.
extension RepositorioOCRManuscritoIPad: @unchecked Sendable {}
extension RepositorioComparativaGoodnotesIPad: @unchecked Sendable {}
extension RepositorioQAPencilFisicoIPad: @unchecked Sendable {}

// Son snapshots inmutables una vez entregados a una cola. CGRect/Date/Data son
// value types; no contienen referencias UIKit mutables.
extension RegistroOCRManuscritoIPad: @unchecked Sendable {}
extension ResultadoBusquedaManuscritaIPad: @unchecked Sendable {}
extension SesionQAPencilFisicoIPad: @unchecked Sendable {}
extension SnapshotQAPencilFisicoIPad: @unchecked Sendable {}
