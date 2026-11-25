import Combine
import Foundation
import QuartzCore
import SwiftUI
import UIKit

/// Comandos que el lienzo de CuadernU puede despachar a la calculadora activa.
enum ComandoCasioTinta: Equatable {
    case mostrarCinta
    case guardarPunto
    case deshacerTecla
    case mostrarRebobinado
    case mostrarFormulas
    case mostrarProcedimientos
    case mostrarPlano
    case mostrarGrafica
    case mostrarIntentos
    case mostrarExportacion
    case nuevoEjercicio
    case capturarLCD
}

/// Puente reactivo compartido entre el editor de tinta de CuadernU y las 4
/// calculadoras Casio nativas v47 (fx-CG50, fx-991CW, fx-570LA X, fx-570VN PLUS).
final class CentroCasioCuadernU: ObservableObject {
    let modoSoloCalculadora: Bool
    @Published var modeloActivo: ModeloCalculadora? = ModeloCalculadora.cg50
    @Published var ultimoComando: ComandoCasioTinta?

    /// Invocado cuando el usuario captura la pantalla LCD para insertarla en el lienzo.
    var alEntregarCaptura: ((UIImage, String) -> Void)?

    init(modoSoloCalculadora: Bool = false) {
        self.modoSoloCalculadora = modoSoloCalculadora
    }

    func seleccionarModelo(_ id: String) {
        if let encontrado = ModeloCalculadora.todos.first(where: { $0.id == id }) {
            modeloActivo = encontrado
        }
    }

    func enviar(_ comando: ComandoCasioTinta) {
        ultimoComando = comando
    }
}

/// Controlador nativo para los motores v47 modernos (`casio_cg50_*` y `casio_cw2_*`)
/// expuestos mediante `casio_ffi.h`.
final class EmuladorCasioV47: ObservableObject {
    let modelo: ModeloCalculadora

    @Published private(set) var imagenLCD: CGImage?
    @Published private(set) var indicadoresMask: UInt32 = 0
    @Published private(set) var teclaPulsada: UInt16?
    @Published private(set) var cargada: Bool = false

    private var handle: OpaquePointer?
    private var displayLink: CADisplayLink?
    private var ultimoInstante: CFTimeInterval = 0
    private var bufferRgba: [UInt8] = []
    private(set) var ancho: Int = 384
    private(set) var alto: Int = 216

    init(modelo: ModeloCalculadora) {
        self.modelo = modelo
        inicializarMotor()
    }

    deinit {
        detener()
        destruirMotor()
    }

    private func inicializarMotor() {
        switch modelo.motor {
        case .cg50:
            guard let dir = Paquete.directorioRoms() else { return }
            let path = dir.path
            handle = path.withCString { casio_cg50_create_from_dir($0) }
            guard let h = handle else { return }
            if let warm = Paquete.url("cg50_warm_state", extension: "bin") {
                _ = warm.path.withCString { casio_cg50_import_state_file(h, $0) }
            }
            _ = casio_cg50_set_rtc_epoch(h, Int64(Date().timeIntervalSince1970))
            ancho = 384
            alto = 216
            bufferRgba = [UInt8](repeating: 255, count: ancho * alto * 4)
            cargada = true
            renderizar(forzar: true)

        case .cw2:
            guard let rom = Paquete.url("fx991cw_rom", extension: "bin") else { return }
            let ram = Paquete.url("fx991cw_default_en", extension: "ram")
            handle = rom.path.withCString { romPtr in
                if let ramPath = ram?.path {
                    return ramPath.withCString { casio_cw2_create_from_files(romPtr, $0) }
                }
                return casio_cw2_create_from_files(romPtr, nil)
            }
            guard let h = handle else { return }
            casio_cw2_power_on(h)
            casio_cw2_advance(h, 250)
            ancho = Int(casio_cw2_screen_width(h))
            alto = Int(casio_cw2_screen_height(h))
            bufferRgba = [UInt8](repeating: 255, count: max(1, ancho * alto * 4))
            cargada = true
            renderizar(forzar: true)

        case .nx:
            break
        }
    }

    private func destruirMotor() {
        guard let h = handle else { return }
        switch modelo.motor {
        case .cg50: casio_cg50_destroy(h)
        case .cw2: casio_cw2_destroy(h)
        case .nx: break
        }
        handle = nil
    }

    func arrancar() {
        guard displayLink == nil, cargada else { return }
        ultimoInstante = CACurrentMediaTime()
        let link = CADisplayLink(target: self, selector: #selector(latido))
        link.preferredFrameRateRange = CAFrameRateRange(minimum: 24, maximum: 60, preferred: 30)
        link.add(to: .main, forMode: .common)
        displayLink = link
    }

    func detener() {
        displayLink?.invalidate()
        displayLink = nil
    }

    @objc private func latido() {
        guard let h = handle else { return }
        let ahora = CACurrentMediaTime()
        let dtMs = min(80.0, max(4.0, (ahora - ultimoInstante) * 1000.0))
        ultimoInstante = ahora

        switch modelo.motor {
        case .cg50:
            _ = casio_cg50_step(h, 350_000, 120_000)
            renderizar(forzar: false)
        case .cw2:
            casio_cw2_advance(h, UInt32(dtMs))
            renderizar(forzar: false)
        case .nx:
            break
        }
    }

    func pulsar(_ codigo: UInt16) {
        guard let h = handle else { return }
        teclaPulsada = codigo
        switch modelo.motor {
        case .cg50:
            _ = casio_cg50_key_down(h, codigo)
        case .cw2:
            casio_cw2_key_down(h, UInt32(codigo))
        case .nx:
            break
        }
    }

    func soltar(_ codigo: UInt16) {
        guard let h = handle else { return }
        switch modelo.motor {
        case .cg50:
            casio_cg50_key_up(h, codigo)
        case .cw2:
            casio_cw2_key_up(h)
        case .nx:
            break
        }
        teclaPulsada = nil
    }

    func capturarImagenUIImage() -> UIImage? {
        renderizar(forzar: true)
        guard let cg = imagenLCD else { return nil }
        return UIImage(cgImage: cg)
    }

    private func renderizar(forzar: Bool) {
        guard let h = handle, ancho > 0, alto > 0 else { return }
        var cambiado: Int32 = 0
        var mask: UInt32 = 0
        bufferRgba.withUnsafeMutableBufferPointer { dst in
            guard let base = dst.baseAddress else { return }
            switch modelo.motor {
            case .cg50:
                cambiado = casio_cg50_render_rgba_ex(h, base, Int32(dst.count), forzar ? 1 : 0)
            case .cw2:
                cambiado = casio_cw2_render_rgba(h, base, Int32(dst.count), &mask, forzar ? 1 : 0)
            case .nx:
                break
            }
        }
        if modelo.motor == .cw2 {
            indicadoresMask = mask
        }
        guard cambiado != 0 || imagenLCD == nil else { return }
        guard let provider = CGDataProvider(data: Data(bufferRgba) as CFData) else { return }
        imagenLCD = CGImage(
            width: ancho,
            height: alto,
            bitsPerComponent: 8,
            bitsPerPixel: 32,
            bytesPerRow: ancho * 4,
            space: CGColorSpaceCreateDeviceRGB(),
            bitmapInfo: CGBitmapInfo(rawValue: CGImageAlphaInfo.last.rawValue),
            provider: provider,
            decode: nil,
            shouldInterpolate: false,
            intent: .defaultIntent
        )
    }
}

/// Vista completa para fx-CG50 (384x216 color) y fx-991CW (192x63 4 grises) en iOS/iPadOS.
struct VistaCalculadoraV47: View {
    let modelo: ModeloCalculadora
    var centro: CentroCasioCuadernU?

    @StateObject private var emulador: EmuladorCasioV47

    init(modelo: ModeloCalculadora, centro: CentroCasioCuadernU? = nil) {
        self.modelo = modelo
        self.centro = centro
        _emulador = StateObject(wrappedValue: EmuladorCasioV47(modelo: modelo))
    }

    var body: some View {
        GeometryReader { geo in
            let disp = modelo.disposicion
            let marco = encajar(en: geo.size, proporcion: disp.proporcion)

            ZStack(alignment: .topLeading) {
                if let caratula = UIImage.caratula(disp.caratula) {
                    Image(uiImage: caratula)
                        .resizable()
                        .interpolation(.high)
                        .frame(width: marco.width, height: marco.height)
                }

                if let cg = emulador.imagenLCD {
                    Image(decorative: cg, scale: 1.0)
                        .interpolation(.none)
                        .resizable()
                        .frame(
                            width: disp.areaPantalla.width * marco.width,
                            height: disp.areaPantalla.height * marco.height
                        )
                        .offset(
                            x: disp.areaPantalla.minX * marco.width,
                            y: disp.areaPantalla.minY * marco.height
                        )
                }

                ForEach(disp.teclas, id: \.codigoMatriz16) { tecla in
                    let rect = CGRect(
                        x: tecla.area.minX * marco.width,
                        y: tecla.area.minY * marco.height,
                        width: tecla.area.width * marco.width,
                        height: tecla.area.height * marco.height
                    )
                    let activa = emulador.teclaPulsada == tecla.codigoMatriz16
                    Rectangle()
                        .fill(Color.accentColor.opacity(activa ? 0.25 : 0.001))
                        .frame(width: rect.width, height: rect.height)
                        .offset(x: rect.minX, y: rect.minY)
                        .gesture(
                            DragGesture(minimumDistance: 0)
                                .onChanged { _ in
                                    if emulador.teclaPulsada != tecla.codigoMatriz16 {
                                        emulador.pulsar(tecla.codigoMatriz16)
                                        UIImpactFeedbackGenerator(style: .light).impactOccurred()
                                    }
                                }
                                .onEnded { _ in
                                    emulador.soltar(tecla.codigoMatriz16)
                                }
                        )
                }
            }
            .frame(width: marco.width, height: marco.height)
            .frame(maxWidth: .infinity, maxHeight: .infinity)
        }
        .background(Color(.systemGroupedBackground))
        .navigationTitle(modelo.nombre)
        .navigationBarTitleDisplayMode(.inline)
        .toolbar {
            if centro?.alEntregarCaptura != nil {
                ToolbarItem(placement: .navigationBarTrailing) {
                    Button {
                        if let img = emulador.capturarImagenUIImage() {
                            centro?.alEntregarCaptura?(img, modelo.id)
                        }
                    } label: {
                        Label("Insertar en CuadernU", systemImage: "camera.viewfinder")
                    }
                }
            }
        }
        .onAppear { emulador.arrancar() }
        .onDisappear { emulador.detener() }
        .onChange(of: centro?.ultimoComando) { cmd in
            if cmd == .capturarLCD, let img = emulador.capturarImagenUIImage() {
                centro?.alEntregarCaptura?(img, modelo.id)
                centro?.ultimoComando = nil
            }
        }
    }

    private func encajar(en disponible: CGSize, proporcion: CGFloat) -> CGSize {
        let anchoPorAltura = disponible.height * proporcion
        if anchoPorAltura <= disponible.width {
            return CGSize(width: anchoPorAltura, height: disponible.height)
        }
        return CGSize(width: disponible.width, height: disponible.width / proporcion)
    }
}
