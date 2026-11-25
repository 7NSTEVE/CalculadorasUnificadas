import Foundation

/// Motor nativo C++17 v47 que impulsa cada calculadora en CuadernU iOS/iPadOS.
enum MotorCalculadoraV47: String, Hashable {
    case cg50       // core/casio_cg50 (SH7305 + R61524 384x216 RGB color)
    case cw2        // core/casio_cw2 (fx-991CW 192x63 4-level grayscale)
    case nx         // core/casio_nx (fx-570LA X & fx-570VN PLUS)
}

/// Una de las 4 calculadoras Casio nativas integradas en CuadernU.
struct ModeloCalculadora: Identifiable, Hashable {
    let id: String
    let nombre: String
    let descripcion: String
    let recursoRom: String
    let disposicion: DisposicionCalculadora
    let motor: MotorCalculadoraV47

    let hardwareId: Int32
    let realHardware: Int32
    let csrMask: Int32
    let pdValue: Int32

    static let todos: [ModeloCalculadora] = [cg50, cw2, classwiz, esplus]

    static let cg50 = ModeloCalculadora(
        id: "cg50",
        nombre: "fx-CG50",
        descripcion: "Gráfica a color",
        recursoRom: "SystemROM",
        disposicion: Disposiciones.cg50,
        motor: .cg50,
        hardwareId: 10,
        realHardware: 1,
        csrMask: 0,
        pdValue: 0
    )

    static let cw2 = ModeloCalculadora(
        id: "cw2",
        nombre: "fx-991CW",
        descripcion: "ClassWiz II",
        recursoRom: "fx991cw_rom",
        disposicion: Disposiciones.cw2,
        motor: .cw2,
        hardwareId: 5,
        realHardware: 0,
        csrMask: 0,
        pdValue: 0
    )

    static let classwiz = ModeloCalculadora(
        id: "classwiz",
        nombre: "fx-570 / 991 LA X",
        descripcion: "ClassWiz",
        recursoRom: "fx991lax",
        disposicion: Disposiciones.classwiz,
        motor: .nx,
        hardwareId: 4,
        realHardware: 0,
        csrMask: 0x0003,
        pdValue: 0x00
    )

    static let esplus = ModeloCalculadora(
        id: "esplus",
        nombre: "fx-570VN PLUS",
        descripcion: "Natural-V.P.A.M.",
        recursoRom: "fx570vnplus",
        disposicion: Disposiciones.esplus,
        motor: .nx,
        hardwareId: 3,
        realHardware: 0,
        csrMask: 0x0001,
        pdValue: 0x08
    )

    var modeloTraductor: TraductorExpresion.Modelo {
        hardwareId == 4 ? .classWiz : .esPlus
    }

    static func == (lhs: ModeloCalculadora, rhs: ModeloCalculadora) -> Bool {
        lhs.id == rhs.id
    }

    func hash(into hasher: inout Hasher) {
        hasher.combine(id)
    }
}
