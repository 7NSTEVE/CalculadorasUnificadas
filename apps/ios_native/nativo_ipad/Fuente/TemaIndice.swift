import Foundation
import CoreGraphics

public struct TemaIndice: Identifiable {
    public let id = UUID()
    public let titulo: String
    public let pagina: Int
    public let yOffset: CGFloat
}
