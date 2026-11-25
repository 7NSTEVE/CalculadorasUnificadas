import Foundation
import Metal
import QuartzCore
import simd

/// Renderer vectorial Metal para página completa o para un viewport/tile.
///
/// Una textura del tamaño total de un lienzo largo escala con el documento y
/// puede superar tanto la memoria disponible como el límite 2D del GPU. El
/// renderer conserva la ruta rápida de página normal, pero rechaza de forma
/// segura superficies gigantes y expone una variante con `regionDocumento` que
/// renderiza solamente el viewport visible sin alterar las coordenadas de tinta.
final class RenderizadorMetal {

    var mostrarTinta = true
    var regionesFondo: [RegionFondoRender] = []

    private struct Uniformes {
        var tamano: SIMD2<Float>
        var origen: SIMD2<Float>
        var color: SIMD4<Float>
    }

    private let dispositivo: MTLDevice
    private let cola: MTLCommandQueue
    private let pipeline: MTLRenderPipelineState
    private let pipelineTextura: MTLRenderPipelineState
    private let capa: CAMetalLayer

    private let semaforoFrames = DispatchSemaphore(value: 3)
    private var indiceEnVuelo = 0

    private var texturaSeca: MTLTexture?
    private var texturaSecaAlterna: MTLTexture?
    private var texturaSecaMSAA: MTLTexture?
    private var tamanoTextura: CGSize = .zero
    private var texturaSecaValida = false

    private var texturaVistaMSAA: MTLTexture?
    private var tamanoVistaMSAA: CGSize = .zero

    private var bufferVertices: [MTLBuffer?] = [nil, nil, nil]
    private var capacidadVertices: [Int] = [0, 0, 0]
    private(set) var deshabilitadoPorTamano = false

    /// Un tile de 14 Mpx ocupa ~56 MB por textura BGRA. El renderer mantiene
    /// dos texturas secas más MSAA memoryless; superar esto aumenta demasiado
    /// el pico aunque el dispositivo soporte una dimensión 2D mayor.
    static let maximoPixelesPorSuperficie: CGFloat = 14_000_000
    static let maximoDimensionFisica: CGFloat = 6_144
    static let escalaMinimaRegional: CGFloat = 0.125

    var colorPapel = SIMD4<Float>(0.99, 0.99, 0.98, 1.0)

    private var colorLimpieza: MTLClearColor {
        MTLClearColor(red: 0, green: 0, blue: 0, alpha: 0)
    }

    static func requiereVirtualizacion(
        tamanoDocumento: CGSize,
        escala: CGFloat
    ) -> Bool {
        let s = max(escala, 1)
        let ancho = max(tamanoDocumento.width * s, 0)
        let alto = max(tamanoDocumento.height * s, 0)
        guard ancho.isFinite, alto.isFinite, ancho > 0, alto > 0 else {
            return true
        }
        return ancho > maximoDimensionFisica
            || alto > maximoDimensionFisica
            || ancho * alto > maximoPixelesPorSuperficie
    }

    init?(capa: CAMetalLayer) {
        guard let dispositivo = MTLCreateSystemDefaultDevice(),
              let cola = dispositivo.makeCommandQueue(),
              let biblioteca = dispositivo.makeDefaultLibrary()
        else { return nil }

        self.dispositivo = dispositivo
        self.cola = cola
        self.capa = capa

        capa.device = dispositivo
        capa.pixelFormat = .bgra8Unorm
        capa.framebufferOnly = false
        capa.isOpaque = false
        capa.presentsWithTransaction = false
        capa.maximumDrawableCount = 3

        let descriptor = MTLRenderPipelineDescriptor()
        descriptor.vertexFunction = biblioteca.makeFunction(name: "vertice_tinta")
        descriptor.fragmentFunction = biblioteca.makeFunction(name: "fragmento_tinta")
        descriptor.colorAttachments[0].pixelFormat = .bgra8Unorm
        descriptor.colorAttachments[0].isBlendingEnabled = true
        descriptor.colorAttachments[0].sourceRGBBlendFactor = .one
        descriptor.colorAttachments[0].destinationRGBBlendFactor = .oneMinusSourceAlpha
        descriptor.colorAttachments[0].sourceAlphaBlendFactor = .one
        descriptor.colorAttachments[0].destinationAlphaBlendFactor = .oneMinusSourceAlpha
        descriptor.rasterSampleCount = 4

        let descTextura = MTLRenderPipelineDescriptor()
        descTextura.vertexFunction = biblioteca.makeFunction(name: "vertice_textura")
        descTextura.fragmentFunction = biblioteca.makeFunction(name: "fragmento_textura")
        descTextura.colorAttachments[0].pixelFormat = .bgra8Unorm
        descTextura.rasterSampleCount = 4
        descTextura.colorAttachments[0].isBlendingEnabled = false

        guard let p1 = try? dispositivo.makeRenderPipelineState(descriptor: descriptor),
              let p2 = try? dispositivo.makeRenderPipelineState(descriptor: descTextura)
        else { return nil }
        pipeline = p1
        pipelineTextura = p2
    }

    // MARK: - Presupuesto de memoria

    private func superficieSegura(_ tamano: CGSize) -> Bool {
        guard tamano.width.isFinite, tamano.height.isFinite,
              tamano.width >= 1, tamano.height >= 1 else { return false }
        return tamano.width <= Self.maximoDimensionFisica
            && tamano.height <= Self.maximoDimensionFisica
            && tamano.width * tamano.height <= Self.maximoPixelesPorSuperficie
    }

    @discardableResult
    private func prepararSuperficie(
        regionDocumento: CGRect,
        escala: CGFloat
    ) -> CGSize? {
        // La página normal llega con escala Retina y no cambia. El piso menor
        // sólo importa al alejar mucho un viewport regional: permite seguir
        // mostrando el documento completo sin pedir una textura >6144 px.
        let s = min(max(escala, Self.escalaMinimaRegional), 4)
        let fisico = CGSize(
            width: ceil(max(regionDocumento.width * s, 1)),
            height: ceil(max(regionDocumento.height * s, 1)))
        guard superficieSegura(fisico) else {
            deshabilitarPorTamano()
            return nil
        }
        deshabilitadoPorTamano = false
        capa.drawableSize = fisico
        capa.contentsScale = s
        return fisico
    }

    private func deshabilitarPorTamano() {
        // Un layout posterior de LienzoCuadernU puede volver a asignar el
        // drawableSize del documento completo. Lo reducimos SIEMPRE antes de
        // consultar el flag, evitando que CAMetalLayer conserve un tamaño
        // gigantesco aunque ya se hubiera deshabilitado en un frame anterior.
        capa.drawableSize = CGSize(width: 1, height: 1)
        capa.contentsScale = 1
        guard !deshabilitadoPorTamano else { return }
        deshabilitadoPorTamano = true
        invalidarTextura()
        texturaVistaMSAA = nil
        tamanoVistaMSAA = .zero
        bufferVertices = [nil, nil, nil]
        capacidadVertices = [0, 0, 0]
        // Sustituye cualquier último drawable válido por transparente. No se
        // toca opacity: los sublayers de selección/puntero del lienzo deben
        // seguir visibles aunque la tinta pase al renderer virtualizado.
        guard let drawable = capa.nextDrawable(),
              let comandos = cola.makeCommandBuffer() else { return }
        let paso = MTLRenderPassDescriptor()
        paso.colorAttachments[0].texture = drawable.texture
        paso.colorAttachments[0].loadAction = .clear
        paso.colorAttachments[0].storeAction = .store
        paso.colorAttachments[0].clearColor = colorLimpieza
        if let encoder = comandos.makeRenderCommandEncoder(descriptor: paso) {
            encoder.endEncoding()
        }
        comandos.present(drawable)
        comandos.commit()
    }

    private func asegurarTextura(tamano: CGSize) -> Bool {
        guard superficieSegura(tamano) else {
            deshabilitarPorTamano(); return false
        }
        if texturaSeca != nil, texturaSecaAlterna != nil,
           texturaSecaMSAA != nil, tamanoTextura == tamano { return true }

        let descriptor = MTLTextureDescriptor.texture2DDescriptor(
            pixelFormat: .bgra8Unorm,
            width: Int(tamano.width), height: Int(tamano.height),
            mipmapped: false)
        descriptor.usage = [.renderTarget, .shaderRead]
        descriptor.storageMode = .private
        texturaSeca = dispositivo.makeTexture(descriptor: descriptor)
        texturaSecaAlterna = dispositivo.makeTexture(descriptor: descriptor)

        let descMSAA = MTLTextureDescriptor.texture2DDescriptor(
            pixelFormat: .bgra8Unorm,
            width: Int(tamano.width), height: Int(tamano.height),
            mipmapped: false)
        descMSAA.textureType = .type2DMultisample
        descMSAA.sampleCount = 4
        descMSAA.storageMode = .memoryless
        descMSAA.usage = [.renderTarget]
        texturaSecaMSAA = dispositivo.makeTexture(descriptor: descMSAA)

        guard texturaSeca != nil, texturaSecaAlterna != nil,
              texturaSecaMSAA != nil else {
            invalidarTextura()
            return false
        }
        tamanoTextura = tamano
        texturaSecaValida = false
        return true
    }

    private func asegurarTexturaVistaMSAA(ancho: Int, alto: Int) -> Bool {
        guard ancho > 0, alto > 0,
              superficieSegura(CGSize(width: ancho, height: alto)) else {
            texturaVistaMSAA = nil
            tamanoVistaMSAA = .zero
            return false
        }
        let tamano = CGSize(width: ancho, height: alto)
        if texturaVistaMSAA != nil, tamanoVistaMSAA == tamano { return true }
        let desc = MTLTextureDescriptor.texture2DDescriptor(
            pixelFormat: .bgra8Unorm, width: ancho, height: alto,
            mipmapped: false)
        desc.textureType = .type2DMultisample
        desc.sampleCount = 4
        desc.storageMode = .memoryless
        desc.usage = [.renderTarget]
        texturaVistaMSAA = dispositivo.makeTexture(descriptor: desc)
        tamanoVistaMSAA = tamano
        return texturaVistaMSAA != nil
    }

    // MARK: - Buffers y geometría

    private func reservarFrame() -> Bool {
        semaforoFrames.wait(timeout: .now()) == .success
    }

    private func liberarFrame() { semaforoFrames.signal() }

    private func bufferCopia(vertices: [SIMD2<Float>]) -> MTLBuffer? {
        guard !vertices.isEmpty else { return nil }
        return vertices.withUnsafeBytes { bytes in
            guard let base = bytes.baseAddress else { return nil }
            return dispositivo.makeBuffer(bytes: base, length: bytes.count,
                                           options: .storageModeShared)
        }
    }

    private func bufferCopia(datos: UnsafePointer<Float>, vertices: Int) -> MTLBuffer? {
        guard vertices > 0 else { return nil }
        return dispositivo.makeBuffer(
            bytes: datos,
            length: vertices * MemoryLayout<SIMD2<Float>>.stride,
            options: .storageModeShared)
    }

    private func asegurarBuffer(vertices: Int, indice: Int) -> MTLBuffer? {
        guard vertices > 0 else { return nil }
        if let buffer = bufferVertices[indice], capacidadVertices[indice] >= vertices {
            return buffer
        }
        let capacidad = max(vertices * 2, 4096)
        let buffer = dispositivo.makeBuffer(
            length: capacidad * MemoryLayout<SIMD2<Float>>.stride,
            options: .storageModeShared)
        bufferVertices[indice] = buffer
        capacidadVertices[indice] = capacidad
        return buffer
    }

    private func componentes(_ argb: UInt32) -> SIMD4<Float> {
        let a = Float((argb >> 24) & 0xFF) / 255
        let r = Float((argb >> 16) & 0xFF) / 255
        let g = Float((argb >> 8) & 0xFF) / 255
        let b = Float(argb & 0xFF) / 255
        return SIMD4<Float>(r * a, g * a, b * a, a)
    }

    private func uniformes(
        region: CGRect,
        color: UInt32
    ) -> Uniformes {
        Uniformes(
            tamano: SIMD2<Float>(Float(region.width), Float(region.height)),
            origen: SIMD2<Float>(Float(region.minX), Float(region.minY)),
            color: componentes(color))
    }

    private func verticesDePauta(_ segmentos: [SegmentoPauta]) -> [SIMD2<Float>] {
        var resultado: [SIMD2<Float>] = []
        resultado.reserveCapacity(segmentos.count * 6)
        for segmento in segmentos {
            let a = SIMD2<Float>(Float(segmento.desde.x), Float(segmento.desde.y))
            let b = SIMD2<Float>(Float(segmento.hasta.x), Float(segmento.hasta.y))
            let delta = b - a
            let largo = simd_length(delta)
            guard largo > 0.001 else { continue }
            let mitad: Float = segmento.destacado ? 1.15 : 0.55
            let normal = SIMD2<Float>(-delta.y, delta.x) / largo * mitad
            resultado.append(contentsOf: [
                a - normal, a + normal, b - normal,
                b - normal, a + normal, b + normal,
            ])
        }
        return resultado
    }

    private func verticesRectangulo(_ rect: CGRect) -> [SIMD2<Float>] {
        let a = SIMD2<Float>(Float(rect.minX), Float(rect.minY))
        let b = SIMD2<Float>(Float(rect.maxX), Float(rect.minY))
        let c = SIMD2<Float>(Float(rect.maxX), Float(rect.maxY))
        let d = SIMD2<Float>(Float(rect.minX), Float(rect.maxY))
        return [a, b, d, b, c, d]
    }

    private func segmentosRegion(_ region: RegionFondoRender) -> [SegmentoPauta] {
        guard region.separacion > 2 else { return [] }
        var salida: [SegmentoPauta] = []
        if region.tipo == .rayado || region.tipo == .cuadricula {
            var y = region.marco.minY
            while y <= region.marco.maxY {
                salida.append(SegmentoPauta(
                    desde: CGPoint(x: region.marco.minX, y: y),
                    hasta: CGPoint(x: region.marco.maxX, y: y), destacado: false))
                y += region.separacion
            }
        }
        if region.tipo == .cuadricula {
            var x = region.marco.minX
            while x <= region.marco.maxX {
                salida.append(SegmentoPauta(
                    desde: CGPoint(x: x, y: region.marco.minY),
                    hasta: CGPoint(x: x, y: region.marco.maxY), destacado: false))
                x += region.separacion
            }
        }
        return salida
    }

    // MARK: - Tinta seca

    func hornearTintaSeca(
        sesion: Sesion,
        tamano: CGSize,
        escala: CGFloat,
        hasta tiempo: Double? = nil
    ) {
        hornearTintaSeca(
            sesion: sesion,
            regionDocumento: CGRect(origin: .zero, size: tamano),
            escala: escala,
            hasta: tiempo)
    }

    func hornearTintaSeca(
        sesion: Sesion,
        regionDocumento region: CGRect,
        escala: CGFloat,
        hasta tiempo: Double? = nil
    ) {
        guard region.width > 0, region.height > 0,
              let fisico = prepararSuperficie(
                regionDocumento: region, escala: escala),
              asegurarTextura(tamano: fisico),
              let textura = texturaSeca,
              let msaa = texturaSecaMSAA,
              let comandos = cola.makeCommandBuffer()
        else { return }

        let paso = MTLRenderPassDescriptor()
        paso.colorAttachments[0].texture = msaa
        paso.colorAttachments[0].resolveTexture = textura
        paso.colorAttachments[0].loadAction = .clear
        paso.colorAttachments[0].storeAction = .multisampleResolve
        paso.colorAttachments[0].clearColor = colorLimpieza
        guard let codificador = comandos.makeRenderCommandEncoder(
            descriptor: paso) else { return }
        codificador.setRenderPipelineState(pipeline)

        for fondo in regionesFondo
        where !fondo.marco.isEmpty && fondo.marco.intersects(region) {
            let relleno = verticesRectangulo(fondo.marco)
            if let buffer = bufferCopia(vertices: relleno) {
                var u = uniformes(region: region, color: fondo.colorARGB)
                codificador.setVertexBuffer(buffer, offset: 0, index: 0)
                codificador.setVertexBytes(
                    &u, length: MemoryLayout<Uniformes>.stride, index: 1)
                codificador.setFragmentBytes(
                    &u, length: MemoryLayout<Uniformes>.stride, index: 1)
                codificador.drawPrimitives(
                    type: .triangle, vertexStart: 0, vertexCount: relleno.count)
            }
            let lineas = verticesDePauta(segmentosRegion(fondo))
            if let buffer = bufferCopia(vertices: lineas) {
                var u = uniformes(region: region, color: fondo.colorPautaARGB)
                codificador.setVertexBuffer(buffer, offset: 0, index: 0)
                codificador.setVertexBytes(
                    &u, length: MemoryLayout<Uniformes>.stride, index: 1)
                codificador.setFragmentBytes(
                    &u, length: MemoryLayout<Uniformes>.stride, index: 1)
                codificador.drawPrimitives(
                    type: .triangle, vertexStart: 0, vertexCount: lineas.count)
            }
        }

        let pauta = sesion.pautaVisibleAvanzada(
            x: Float(region.minX), y: Float(region.minY),
            ancho: Float(region.width), alto: Float(region.height))
        let verticesPauta = verticesDePauta(pauta)
        if !verticesPauta.isEmpty,
           let buffer = bufferCopia(vertices: verticesPauta) {
            var u = uniformes(region: region, color: sesion.colorPauta)
            codificador.setVertexBuffer(buffer, offset: 0, index: 0)
            codificador.setVertexBytes(
                &u, length: MemoryLayout<Uniformes>.stride, index: 1)
            codificador.setFragmentBytes(
                &u, length: MemoryLayout<Uniformes>.stride, index: 1)
            codificador.drawPrimitives(
                type: .triangle, vertexStart: 0,
                vertexCount: verticesPauta.count)
        }

        if mostrarTinta {
            let regionFiltro = region.insetBy(dx: -12, dy: -12)
            for indice in 0..<sesion.cantidadTrazos {
                if let marco = sesion.marcoTrazo(indice),
                   !marco.intersects(regionFiltro) { continue }
                let geometria: (
                    datos: UnsafePointer<Float>, cantidad: Int, color: UInt32)?
                if let tiempo {
                    geometria = sesion.verticesDelTrazo(indice, hasta: tiempo)
                } else {
                    geometria = sesion.verticesDelTrazo(indice)
                }
                guard let trazo = geometria,
                      let buffer = bufferCopia(
                        datos: trazo.datos, vertices: trazo.cantidad)
                else { continue }
                var u = uniformes(region: region, color: trazo.color)
                codificador.setVertexBuffer(buffer, offset: 0, index: 0)
                codificador.setVertexBytes(
                    &u, length: MemoryLayout<Uniformes>.stride, index: 1)
                codificador.setFragmentBytes(
                    &u, length: MemoryLayout<Uniformes>.stride, index: 1)
                codificador.drawPrimitives(
                    type: .triangle, vertexStart: 0,
                    vertexCount: trazo.cantidad)
            }
        }

        codificador.endEncoding()
        comandos.commit()
        texturaSecaValida = true
    }

    @discardableResult
    func hornearTrazoTerminado(
        sesion: Sesion,
        indice: Int,
        tamano: CGSize,
        escala: CGFloat
    ) -> Bool {
        hornearTrazoTerminado(
            sesion: sesion, indice: indice,
            regionDocumento: CGRect(origin: .zero, size: tamano),
            escala: escala)
    }

    @discardableResult
    func hornearTrazoTerminado(
        sesion: Sesion,
        indice: Int,
        regionDocumento region: CGRect,
        escala: CGFloat
    ) -> Bool {
        if let marco = sesion.marcoTrazo(indice),
           !marco.intersects(region.insetBy(dx: -12, dy: -12)) {
            return true
        }
        guard let fisico = prepararSuperficie(
                regionDocumento: region, escala: escala),
              asegurarTextura(tamano: fisico),
              texturaSecaValida,
              let origen = texturaSeca,
              let destino = texturaSecaAlterna,
              let msaa = texturaSecaMSAA else { return false }
        guard mostrarTinta else { return true }
        guard let trazo = sesion.verticesDelTrazo(indice),
              let buffer = bufferCopia(
                datos: trazo.datos, vertices: trazo.cantidad),
              let comandos = cola.makeCommandBuffer() else { return false }

        let paso = MTLRenderPassDescriptor()
        paso.colorAttachments[0].texture = msaa
        paso.colorAttachments[0].resolveTexture = destino
        paso.colorAttachments[0].loadAction = .clear
        paso.colorAttachments[0].storeAction = .multisampleResolve
        paso.colorAttachments[0].clearColor = colorLimpieza
        guard let codificador = comandos.makeRenderCommandEncoder(
            descriptor: paso) else { return false }

        codificador.setRenderPipelineState(pipelineTextura)
        codificador.setFragmentTexture(origen, index: 0)
        codificador.drawPrimitives(
            type: .triangleStrip, vertexStart: 0, vertexCount: 4)

        var u = uniformes(region: region, color: trazo.color)
        codificador.setRenderPipelineState(pipeline)
        codificador.setVertexBuffer(buffer, offset: 0, index: 0)
        codificador.setVertexBytes(
            &u, length: MemoryLayout<Uniformes>.stride, index: 1)
        codificador.setFragmentBytes(
            &u, length: MemoryLayout<Uniformes>.stride, index: 1)
        codificador.drawPrimitives(
            type: .triangle, vertexStart: 0, vertexCount: trazo.cantidad)
        codificador.endEncoding()
        comandos.commit()
        texturaSeca = destino
        texturaSecaAlterna = origen
        return true
    }

    // MARK: - Frame interactivo

    @discardableResult
    func dibujar(
        sesion: Sesion,
        tamano: CGSize,
        alCompletarGPU: ((CFTimeInterval) -> Void)? = nil
    ) -> Bool {
        dibujar(
            sesion: sesion,
            regionDocumento: CGRect(origin: .zero, size: tamano),
            escala: capa.contentsScale,
            alCompletarGPU: alCompletarGPU)
    }

    @discardableResult
    func dibujar(
        sesion: Sesion,
        regionDocumento region: CGRect,
        escala: CGFloat,
        alCompletarGPU: ((CFTimeInterval) -> Void)? = nil
    ) -> Bool {
        // Para el backing layer del lienzo gigante devolvemos "consumido" sin
        // encolar frames. El renderer virtualizado pinta el viewport; devolver
        // false aquí haría que PlanificadorFrames reintentara indefinidamente.
        guard prepararSuperficie(
            regionDocumento: region, escala: escala) != nil else { return true }
        guard reservarFrame() else { return false }
        let indiceActual = indiceEnVuelo
        indiceEnVuelo = (indiceEnVuelo + 1) % 3

        guard let drawable = capa.nextDrawable(),
              let comandos = cola.makeCommandBuffer(),
              asegurarTexturaVistaMSAA(
                ancho: drawable.texture.width,
                alto: drawable.texture.height),
              let msaa = texturaVistaMSAA else {
            liberarFrame()
            return false
        }

        let paso = MTLRenderPassDescriptor()
        paso.colorAttachments[0].texture = msaa
        paso.colorAttachments[0].resolveTexture = drawable.texture
        paso.colorAttachments[0].loadAction = .clear
        paso.colorAttachments[0].storeAction = .multisampleResolve
        paso.colorAttachments[0].clearColor = colorLimpieza

        if let codificador = comandos.makeRenderCommandEncoder(descriptor: paso) {
            if let seca = texturaSeca,
               seca.width == drawable.texture.width,
               seca.height == drawable.texture.height {
                codificador.setRenderPipelineState(pipelineTextura)
                codificador.setFragmentTexture(seca, index: 0)
                codificador.drawPrimitives(
                    type: .triangleStrip, vertexStart: 0, vertexCount: 4)
            }

            codificador.setRenderPipelineState(pipeline)
            if mostrarTinta,
               let trazo = sesion.verticesTrazoEnCurso(),
               let buffer = asegurarBuffer(
                vertices: trazo.cantidad, indice: indiceActual) {
                memcpy(buffer.contents(), trazo.datos,
                       trazo.cantidad * MemoryLayout<SIMD2<Float>>.stride)
                var u = uniformes(
                    region: region, color: sesion.colorTrazoEnCurso)
                codificador.setVertexBuffer(buffer, offset: 0, index: 0)
                codificador.setVertexBytes(
                    &u, length: MemoryLayout<Uniformes>.stride, index: 1)
                codificador.setFragmentBytes(
                    &u, length: MemoryLayout<Uniformes>.stride, index: 1)
                codificador.drawPrimitives(
                    type: .triangle, vertexStart: 0,
                    vertexCount: trazo.cantidad)
            }
            codificador.endEncoding()
        }

        comandos.addCompletedHandler { [self] buffer in
            liberarFrame()
            if let alCompletarGPU {
                let tiempo = buffer.gpuEndTime > 0
                    ? buffer.gpuEndTime : CACurrentMediaTime()
                alCompletarGPU(tiempo)
            }
        }
        comandos.present(drawable)
        comandos.commit()
        return true
    }

    func invalidarTextura() {
        texturaSeca = nil
        texturaSecaAlterna = nil
        texturaSecaMSAA = nil
        tamanoTextura = .zero
        texturaSecaValida = false
    }

    func liberarRecursosVolatiles() {
        invalidarTextura()
        texturaVistaMSAA = nil
        tamanoVistaMSAA = .zero
        bufferVertices = [nil, nil, nil]
        capacidadVertices = [0, 0, 0]
    }
}
