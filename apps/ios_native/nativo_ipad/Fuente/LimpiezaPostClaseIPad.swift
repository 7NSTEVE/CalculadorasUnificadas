import Foundation
import UIKit

struct CambioMarcoMedioLimpiezaIPad: Equatable {
    var elementoId: String
    var origen: CGRect
    var destino: CGRect
}

struct PerspectivaSugeridaLimpiezaIPad: Equatable {
    var elementoId: String
    var puntos: [PuntoNormalizado]
}

struct EncabezadoSugeridoLimpiezaIPad: Equatable {
    var elementoId: String
    var titulo: String
    var region: CGRect
}

struct PlanLimpiezaPostClaseIPad {
    var transformacionesTinta: [TintaTransformacionTrazo]
    var marcosMedios: [CambioMarcoMedioLimpiezaIPad]
    var perspectivas: [PerspectivaSugeridaLimpiezaIPad]
    var encabezados: [EncabezadoSugeridoLimpiezaIPad]
    var secciones: [FrameApunteIPad]

    var estaVacio: Bool {
        transformacionesTinta.isEmpty && marcosMedios.isEmpty
            && perspectivas.isEmpty && encabezados.isEmpty && secciones.isEmpty
    }

    var descripcion: String {
        [
            "\(transformacionesTinta.count) trazos con espacio más compacto",
            "\(marcosMedios.count) fotos/PDF alineados o separados",
            "\(perspectivas.count) perspectivas corregibles",
            "\(encabezados.count) encabezados detectados",
            "\(secciones.count) secciones espaciales sugeridas",
        ].joined(separator: "\n")
    }
}

struct ResultadoLimpiezaPostClaseIPad {
    var trazos = 0
    var medios = 0
    var perspectivas = 0
    var encabezados = 0
    var secciones = 0
}

/// Heurísticas completamente locales. El plan nunca se aplica durante el
/// análisis: primero se entrega al panel y se dibuja una vista previa morada.
enum MotorLimpiezaPostClaseIPad {
    static func preparar(sesion: Sesion,
                         completado: @escaping (PlanLimpiezaPostClaseIPad) -> Void) {
        let documento = sesion.documentoPersistenteId
        let pagina = CGRect(x: 0, y: 0,
                            width: max(CGFloat(sesion.anchoPagina), 64),
                            height: max(CGFloat(sesion.altoPagina), 64))
        let tinta = sesion.planReflujo(region: pagina, factor: 0.88, modo: 1)
            .filter { hypot($0.delta_x, $0.delta_y) >= 8 }
        let marcos = planMarcos(sesion: sesion, documento: documento,
                                pagina: pagina)
        let encabezados = detectarEncabezados(sesion: sesion,
                                               documento: documento)
        let secciones = sugerirSecciones(encabezados: encabezados,
                                          pagina: pagina)
        let candidatosPerspectiva = sesion.medios.filter { medio in
            guard medio.tipo == .imagen,
                  RepositorioEdicionesImagenDocumentoIPad.compartido
                    .ajustes(documento: documento, id: medio.id)
                    .perspectiva.isEmpty else { return false }
            let fondo = RepositorioFondosObjetoIPad.compartido.ajustes(
                documento: documento, elemento: medio.id)
            return !fondo.oculto
        }
        guard !candidatosPerspectiva.isEmpty else {
            completado(PlanLimpiezaPostClaseIPad(
                transformacionesTinta: tinta, marcosMedios: marcos,
                perspectivas: [], encabezados: encabezados,
                secciones: secciones))
            return
        }

        let grupo = DispatchGroup()
        let bloqueo = NSLock()
        var perspectivas: [PerspectivaSugeridaLimpiezaIPad] = []
        for medio in candidatosPerspectiva.prefix(20) {
            guard let url = RepositorioAdjuntos.compartido.url(hash: medio.hash),
                  let imagen = UIImage(contentsOfFile: url.path) else { continue }
            grupo.enter()
            AnalizadorImagenLocal.detectarPerspectiva(en: imagen) { resultado in
                defer { grupo.leave() }
                guard case .success(let puntos) = resultado,
                      mereceCorreccion(puntos) else { return }
                bloqueo.lock()
                perspectivas.append(PerspectivaSugeridaLimpiezaIPad(
                    elementoId: medio.id, puntos: puntos))
                bloqueo.unlock()
            }
        }
        grupo.notify(queue: .main) {
            completado(PlanLimpiezaPostClaseIPad(
                transformacionesTinta: tinta, marcosMedios: marcos,
                perspectivas: perspectivas.sorted { $0.elementoId < $1.elementoId },
                encabezados: encabezados, secciones: secciones))
        }
    }

    static func aplicar(_ plan: PlanLimpiezaPostClaseIPad,
                        sesion: Sesion,
                        navegador: VistaLienzoNavegable) -> ResultadoLimpiezaPostClaseIPad {
        var resultado = ResultadoLimpiezaPostClaseIPad()
        if !plan.transformacionesTinta.isEmpty,
           sesion.aplicarReflujo(plan.transformacionesTinta) {
            resultado.trazos = plan.transformacionesTinta.count
        }
        for cambio in plan.marcosMedios {
            guard var medio = sesion.medios.first(where: {
                $0.id == cambio.elementoId
            }) else { continue }
            medio.marco = cambio.destino
            if sesion.actualizarMedio(medio) { resultado.medios += 1 }
        }
        for sugerencia in plan.perspectivas {
            var ajustes = RepositorioEdicionesImagenDocumentoIPad.compartido
                .ajustes(documento: sesion.documentoPersistenteId,
                         id: sugerencia.elementoId)
            ajustes.perspectiva = sugerencia.puntos
            _ = RepositorioEdicionesImagenDocumentoIPad.compartido.guardar(
                ajustes, documento: sesion.documentoPersistenteId,
                id: sugerencia.elementoId)
            resultado.perspectivas += 1
        }
        let documento = sesion.documentoPersistenteId
        let cursoId = CentroExperienciaIPad.compartido.instantanea()
            .espacioActivo?.cursoId
        for encabezado in plan.encabezados {
            if RepositorioSemanticaIPad.compartido.marcar(
                documento: documento, claves: ["x:\(encabezado.elementoId)"],
                tipo: .encabezado, titulo: encabezado.titulo,
                region: encabezado.region, cursoId: cursoId) != nil {
                resultado.encabezados += 1
            }
        }
        let existentes = RepositorioNavegacionEspacialIPad.compartido
            .instantanea(documento).frames
        for seccion in plan.secciones where !existentes.contains(where: {
            Normalizador.texto($0.titulo) == Normalizador.texto(seccion.titulo)
                && abs($0.region.midY - seccion.region.midY) < 24
        }) {
            RepositorioNavegacionEspacialIPad.compartido.agregar(
                seccion, documento: documento)
            resultado.secciones += 1
        }
        navegador.quitarPreviaReflujo()
        navegador.recargarElementos()
        navegador.mostrarFrames(
            RepositorioNavegacionEspacialIPad.compartido
                .instantanea(documento).frames)
        navegador.lienzo.rehornearYDibujar()
        return resultado
    }

    private static func planMarcos(sesion: Sesion, documento: UInt64,
                                   pagina: CGRect)
        -> [CambioMarcoMedioLimpiezaIPad] {
        let candidatos = sesion.medios.filter { medio in
            let fondo = RepositorioFondosObjetoIPad.compartido.ajustes(
                documento: documento, elemento: medio.id)
            return !fondo.esFondo && !fondo.bloqueado && !fondo.oculto
                && (medio.tipo == .imagen || medio.tipo == .pdf)
        }.sorted {
            $0.marco.minY == $1.marco.minY
                ? $0.marco.minX < $1.marco.minX : $0.marco.minY < $1.marco.minY
        }
        var destinos: [CGRect] = []
        var cambios: [CambioMarcoMedioLimpiezaIPad] = []
        for medio in candidatos {
            var destino = medio.marco
            destino.origin.x = (destino.minX / 12).rounded() * 12
            destino.origin.y = (destino.minY / 12).rounded() * 12
            destino.origin.x = min(max(destino.minX, pagina.minX + 12),
                                   max(pagina.maxX - destino.width - 12,
                                       pagina.minX + 12))
            var intentos = 0
            while let conflicto = destinos.first(where: {
                $0.insetBy(dx: -12, dy: -12).intersects(destino)
            }), intentos < 40 {
                destino.origin.y = conflicto.maxY + 18
                intentos += 1
            }
            if abs(destino.minX - medio.marco.minX) > 2
                || abs(destino.minY - medio.marco.minY) > 2 {
                cambios.append(CambioMarcoMedioLimpiezaIPad(
                    elementoId: medio.id, origen: medio.marco, destino: destino))
            }
            destinos.append(destino)
        }
        return cambios
    }

    private static func detectarEncabezados(sesion: Sesion, documento: UInt64)
        -> [EncabezadoSugeridoLimpiezaIPad] {
        let existentes = Set(RepositorioSemanticaIPad.compartido
            .marcaciones(documento: documento)
            .filter { $0.tipo == .encabezado }
            .flatMap(\.clavesElemento))
        return sesion.textos.compactMap { texto in
            guard !existentes.contains("x:\(texto.id)") else { return nil }
            let lineas = texto.texto.components(separatedBy: .newlines)
                .map { $0.trimmingCharacters(in: .whitespacesAndNewlines) }
                .filter { !$0.isEmpty }
            guard let primera = lineas.first, lineas.count <= 2,
                  (4...72).contains(primera.count), texto.marco.height <= 180,
                  primera.range(of: #"[.!?;:]$"#,
                                options: .regularExpression) == nil else { return nil }
            return EncabezadoSugeridoLimpiezaIPad(
                elementoId: texto.id, titulo: primera, region: texto.marco)
        }.sorted { $0.region.minY < $1.region.minY }
    }

    private static func sugerirSecciones(
        encabezados: [EncabezadoSugeridoLimpiezaIPad], pagina: CGRect)
        -> [FrameApunteIPad] {
        guard !encabezados.isEmpty else { return [] }
        var salida: [FrameApunteIPad] = []
        for (indice, encabezado) in encabezados.prefix(12).enumerated() {
            let inicio = max(pagina.minY, encabezado.region.minY - 24)
            let fin: CGFloat
            if indice + 1 < encabezados.count {
                fin = min(pagina.maxY,
                          encabezados[indice + 1].region.minY - 24)
            } else {
                fin = min(pagina.maxY,
                          max(encabezado.region.maxY + 480, inicio + 180))
            }
            guard fin - inicio >= 160 else { continue }
            salida.append(FrameApunteIPad(
                titulo: encabezado.titulo,
                region: CGRect(x: pagina.minX + 12, y: inicio,
                               width: max(pagina.width - 24, 64),
                               height: fin - inicio),
                tipo: .visible, colorARGB: 0xFFBF5AF2))
        }
        return salida
    }

    private static func mereceCorreccion(_ puntos: [PuntoNormalizado]) -> Bool {
        guard puntos.count == 4 else { return false }
        let ideales = [CGPoint(x: 0, y: 0), CGPoint(x: 1, y: 0),
                       CGPoint(x: 1, y: 1), CGPoint(x: 0, y: 1)]
        let desviacion = zip(puntos, ideales).reduce(CGFloat.zero) { total, par in
            total + hypot(par.0.x - par.1.x, par.0.y - par.1.y)
        } / 4
        return desviacion >= 0.035
    }
}
