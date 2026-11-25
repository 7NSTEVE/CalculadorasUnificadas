import Foundation
import UIKit

// MARK: - Acciones y atajos del iPad

/// Acciones que pueden vivir en la barra estable, la paleta de comandos o un
/// teclado externo. Es una lista semántica: la interfaz decide después si la
/// presenta como botón, comando o gesto del lápiz del iPad.
enum AccionIPad: String, Codable, CaseIterable, Identifiable {
    case paletaComandos
    case espaciosCurso
    case pluma
    case resaltador
    case borradorObjeto
    case borradorParcial
    case borradorAvanzado
    case lazo
    case mano
    case teclado
    case voz
    case deshacer
    case rehacer
    case limpiar
    case papel
    case lienzo
    case navegacionEspacial
    case ultimaEscritura
    case vistaAnterior
    case marcador
    case centroAcademico
    case calendarioCampus
    case evaluacionEntrega
    case multimediaClase
    case bibliotecaFuentes
    case estudioOral
    case estudioTecnico
    case asistenteLocal
    case matematicas
    case ingenieria
    case herramientasTecnicas
    case plumaAvanzada
    case bloques
    case capas
    case versiones
    case portapapeles
    case disposicion
    case conocimiento
    case colaboracion
    case controlRemotoWeb
    case seguridad
    case foco
    case modosEntrada
    case metricasInterrupcion
    case proximaAccion
    case recuperarCincoMinutos
    case scratchGlobal
    case importar
    case escanearPizarra
    case exportar
    case grabar
    case pausarGrabacion
    case marcarMePerdi
    case marcarImportante
    case marcarExamen
    case reproducirNotaViva
    case calculadora
    case ajustes
    case ventanaDividida
    case menuRadial
    case notasAdhesivas
    case carpetasInteligentes
    case herramientasCasio

    var id: String { rawValue }


    var nombre: String {
        switch self {
        case .paletaComandos: return "Paleta de comandos"
        case .espaciosCurso: return "Espacios y perfiles"
        case .pluma: return "Pluma"
        case .resaltador: return "Resaltador"
        case .borradorObjeto: return "Borrador por objeto"
        case .borradorParcial: return "Borrador parcial"
        case .borradorAvanzado: return "Configurar borrador avanzado"
        case .lazo: return "Selección por lazo"
        case .mano: return "Mano para navegar"
        case .teclado: return "Insertar texto"
        case .voz: return "Dictado"
        case .deshacer: return "Deshacer"
        case .rehacer: return "Rehacer"
        case .limpiar: return "Limpiar toda la tinta"
        case .papel: return "Papel y pauta"
        case .lienzo: return "Tamaño y modo del lienzo"
        case .navegacionEspacial: return "Escenas, frames y favoritos espaciales"
        case .ultimaEscritura: return "Volver a la última escritura"
        case .vistaAnterior: return "Volver a la vista anterior"
        case .marcador: return "Agregar marcador"
        case .centroAcademico: return "Centro académico"
        case .calendarioCampus: return "Calendario y Campus"
        case .evaluacionEntrega: return "Entregas, rúbricas y exámenes"
        case .multimediaClase: return "Slides, fotos, audio y vídeo"
        case .bibliotecaFuentes: return "Biblioteca, fuentes y migración"
        case .estudioOral: return "Estudio oral y técnica Feynman"
        case .estudioTecnico: return "Estudio técnico y Problem Bank"
        case .asistenteLocal: return "Asistente local"
        case .matematicas: return "Matemáticas"
        case .ingenieria: return "Ingeniería"
        case .herramientasTecnicas: return "Herramientas técnicas"
        case .plumaAvanzada: return "Pluma avanzada"
        case .bloques: return "Bloques estructurados"
        case .capas: return "Capas"
        case .versiones: return "Versiones"
        case .portapapeles: return "Portapapeles inteligente"
        case .disposicion: return "Disposición inteligente"
        case .conocimiento: return "Grafo de conocimiento"
        case .colaboracion: return "Colaboración local"
        case .controlRemotoWeb: return "Teléfono como cámara y control"
        case .seguridad: return "Seguridad y recuperación"
        case .foco: return "Modo de enfoque"
        case .modosEntrada: return "Entrada, scroll y gestos"
        case .metricasInterrupcion: return "Interrupciones y reentrada"
        case .proximaAccion: return "¿Qué hago ahora?"
        case .recuperarCincoMinutos: return "Recuperar los últimos cinco minutos"
        case .scratchGlobal: return "Scratchpad global"
        case .importar: return "Importar"
        case .escanearPizarra: return "Escanear pizarra"
        case .exportar: return "Exportar"
        case .grabar: return "Iniciar o detener grabación"
        case .pausarGrabacion: return "Pausar o reanudar grabación"
        case .marcarMePerdi: return "Marcar: me perdí"
        case .marcarImportante: return "Marcar: importante"
        case .marcarExamen: return "Marcar: entra en examen"
        case .reproducirNotaViva: return "Reproducir la última Nota Viva"
        case .calculadora: return "Calculadora dentro del apunte"
        case .ajustes: return "Ajustes de captura y lápiz del iPad"
        case .ventanaDividida: return "Ventana dividida"
        case .menuRadial: return "Menú radial"
        case .notasAdhesivas: return "Notas adhesivas"
        case .carpetasInteligentes: return "Carpetas inteligentes"
        case .herramientasCasio: return "Herramientas de calculadora Casio"
        }
    }

    var simbolo: String {
        switch self {
        case .paletaComandos: return "command"
        case .espaciosCurso: return "rectangle.3.group"
        case .pluma: return "pencil.tip"
        case .resaltador: return "highlighter"
        case .borradorObjeto: return "eraser"
        case .borradorParcial: return "eraser.line.dashed"
        case .borradorAvanzado: return "eraser.fill"
        case .lazo: return "lasso"
        case .mano: return "hand.draw"
        case .teclado: return "keyboard"
        case .voz: return "mic"
        case .deshacer: return "arrow.uturn.backward"
        case .rehacer: return "arrow.uturn.forward"
        case .limpiar: return "trash"
        case .papel: return "doc.text"
        case .lienzo: return "rectangle.expand.vertical"
        case .navegacionEspacial: return "viewfinder.rectangular"
        case .ultimaEscritura: return "location"
        case .vistaAnterior: return "arrowshape.turn.up.backward"
        case .marcador: return "bookmark"
        case .centroAcademico: return "graduationcap"
        case .calendarioCampus: return "calendar.badge.clock"
        case .evaluacionEntrega: return "doc.badge.arrow.up"
        case .multimediaClase: return "rectangle.stack.badge.play"
        case .bibliotecaFuentes: return "books.vertical"
        case .estudioOral: return "person.wave.2"
        case .estudioTecnico: return "square.stack.3d.up"
        case .asistenteLocal: return "brain.head.profile"
        case .matematicas: return "chart.xyaxis.line"
        case .ingenieria: return "wrench.and.screwdriver"
        case .herramientasTecnicas: return "ruler"
        case .plumaAvanzada: return "pencil.tip.crop.circle"
        case .bloques: return "square.grid.3x3.square"
        case .capas: return "square.3.layers.3d"
        case .versiones: return "clock.arrow.2.circlepath"
        case .portapapeles: return "list.clipboard"
        case .disposicion: return "wand.and.rays"
        case .conocimiento: return "point.3.connected.trianglepath.dotted"
        case .colaboracion: return "person.2.wave.2"
        case .controlRemotoWeb: return "iphone.gen3.radiowaves.left.and.right"
        case .seguridad: return "lock.shield"
        case .foco: return "scope"
        case .modosEntrada: return "hand.tap"
        case .metricasInterrupcion: return "arrowshape.turn.up.backward.2"
        case .proximaAccion: return "scope"
        case .recuperarCincoMinutos: return "clock.arrow.circlepath"
        case .scratchGlobal: return "scribble.variable"
        case .importar: return "square.and.arrow.down"
        case .escanearPizarra: return "camera.viewfinder"
        case .exportar: return "square.and.arrow.up"
        case .grabar: return "record.circle"
        case .pausarGrabacion: return "pause.circle"
        case .marcarMePerdi: return "questionmark.circle"
        case .marcarImportante: return "exclamationmark.circle"
        case .marcarExamen: return "checkmark.seal"
        case .reproducirNotaViva: return "play.rectangle"
        case .calculadora: return "x.squareroot"
        case .ajustes: return "slider.horizontal.3"
        case .ventanaDividida: return "square.split.2x1"
        case .menuRadial: return "circle.grid.cross"
        case .notasAdhesivas: return "note.text"
        case .carpetasInteligentes: return "folder.badge.gearshape"
        case .herramientasCasio: return "function"
        }
    }

    var palabrasClave: String {
        switch self {
        case .espaciosCurso: return "curso semestre universidad workspace perfil interfaz"
        case .borradorObjeto, .borradorParcial, .borradorAvanzado:
            return "eliminar borrar goma pixel trazo objeto resaltador color capa"
        case .lazo: return "seleccionar mover transformar tinta"
        case .mano: return "pan navegar desplazar zoom"
        case .navegacionEspacial:
            return "escenas frames pestañas minimapa navegación espacial favorito temporal"
        case .foco, .metricasInterrupcion:
            return "concentración temporizador estudiar clase distracciones reentrada"
        case .modosEntrada:
            return "lectura escritura selección scroll vertical horizontal gestos radial estrella"
        case .scratchGlobal: return "borrador rápido pensamiento capturar bandeja"
        case .grabar, .pausarGrabacion, .reproducirNotaViva, .multimediaClase:
            return "audio video micrófono replay paso a paso live nota"
        case .estudioOral:
            return "feynman enseñar explicación preguntas oral caminar bus auto"
        case .estudioTecnico:
            return "problemas práctica fórmula dependencias laboratorio assignment office hours examen"
        case .bibliotecaFuentes:
            return "biblioteca carpeta curso libro fuente cita PDF comparar migrar Formato Infinito Formato Paginado"
        case .controlRemotoWeb:
            return "android iphone teléfono remoto cámara pizarra QR navegador paleta"
        case .ventanaDividida:
            return "ventana split dividir workspace vista dual pantalla"
        case .menuRadial:
            return "menu radial pie circular atajos lápiz squeeze"
        case .notasAdhesivas:
            return "nota adhesiva post-it sticky callout comentario mapa mental"
        case .carpetasInteligentes:
            return "carpeta inteligente smart folder filtro huérfanos sin revisar"
        default: return nombre
        }
    }
}

struct AtajoIPad: Codable, Equatable, Identifiable {
    var accion: AccionIPad
    var entrada: String
    var modificadores: UInt
    var id: String { accion.rawValue }

    var flags: UIKeyModifierFlags {
        UIKeyModifierFlags(rawValue: Int(modificadores))
    }

    static let predeterminados: [AtajoIPad] = [
        AtajoIPad(accion: .paletaComandos, entrada: "k",
                  modificadores: UInt(UIKeyModifierFlags.command.rawValue)),
        AtajoIPad(accion: .pluma, entrada: "p", modificadores: 0),
        AtajoIPad(accion: .resaltador, entrada: "h", modificadores: 0),
        AtajoIPad(accion: .borradorObjeto, entrada: "e", modificadores: 0),
        AtajoIPad(accion: .lazo, entrada: "l", modificadores: 0),
        AtajoIPad(accion: .mano, entrada: "m", modificadores: 0),
        AtajoIPad(accion: .foco, entrada: "f",
                  modificadores: UInt(UIKeyModifierFlags.command.rawValue)),
        AtajoIPad(accion: .deshacer, entrada: "z",
                  modificadores: UInt(UIKeyModifierFlags.command.rawValue)),
        AtajoIPad(accion: .rehacer, entrada: "z",
                  modificadores: UInt((UIKeyModifierFlags.command.union(.shift)).rawValue)),
        AtajoIPad(accion: .importar, entrada: "o",
                  modificadores: UInt(UIKeyModifierFlags.command.rawValue)),
        AtajoIPad(accion: .exportar, entrada: "e",
                  modificadores: UInt((UIKeyModifierFlags.command.union(.shift)).rawValue))
    ]
}

// MARK: - Espacios por curso y perfiles de interfaz

enum PerfilInterfazIPad: String, Codable, CaseIterable, Identifiable {
    case clase
    case estudio
    case tarea
    case lectura
    case circuitos
    case examen
    case sinDistracciones

    var id: String { rawValue }

    var nombre: String {
        switch self {
        case .clase: return "Clase"
        case .estudio: return "Estudio"
        case .tarea: return "Tarea"
        case .lectura: return "Lectura"
        case .circuitos: return "Circuitos"
        case .examen: return "Examen"
        case .sinDistracciones: return "Sin distracciones"
        }
    }

    var detalle: String {
        switch self {
        case .clase: return "Captura rápida, Nota Viva y marcas de clase."
        case .estudio: return "Repaso, preguntas y relaciones entre conceptos."
        case .tarea: return "Enunciado, pasos, cálculo y entrega."
        case .lectura: return "PDF, subrayado y referencias sin ruido visual."
        case .circuitos: return "Cuadrícula técnica, símbolos y simulación local."
        case .examen: return "Cronómetro discreto y herramientas restringibles."
        case .sinDistracciones: return "Solo el contenido y la herramienta activa."
        }
    }
}

enum NivelInterfazIPad: String, Codable, CaseIterable {
    case simple
    case avanzado

    var nombre: String { self == .simple ? "Simple" : "Avanzado" }
}

enum UbicacionBarraIPad: String, Codable, CaseIterable, Identifiable {
    case superior
    case inferior
    case izquierda
    case derecha
    case flotante

    var id: String { rawValue }

    var nombre: String {
        switch self {
        case .superior: return "Arriba"
        case .inferior: return "Abajo"
        case .izquierda: return "Izquierda"
        case .derecha: return "Derecha"
        case .flotante: return "Flotante"
        }
    }

    var simbolo: String {
        switch self {
        case .superior: return "rectangle.topthird.inset.filled"
        case .inferior: return "rectangle.bottomthird.inset.filled"
        case .izquierda: return "rectangle.leftthird.inset.filled"
        case .derecha: return "rectangle.rightthird.inset.filled"
        case .flotante: return "platter.filled.top.and.arrow.up.iphone"
        }
    }
}

struct HerramientaSateliteIPad: Codable, Equatable, Identifiable {
    var id = UUID()
    var accion: AccionIPad
    /// Centro normalizado respecto del área segura. Se guarda entre 0 y 1 para
    /// sobrevivir a rotación, Stage Manager y tamaños distintos de iPad.
    var x: Double
    var y: Double
}

struct ConfiguracionBarraIPad: Codable, Equatable {
    var ubicacion: UbicacionBarraIPad = .superior
    var centroFlotanteX = 0.5
    var centroFlotanteY = 0.12
    var satelites: [HerramientaSateliteIPad] = []

    mutating func normalizar() {
        centroFlotanteX = min(max(centroFlotanteX, 0.08), 0.92)
        centroFlotanteY = min(max(centroFlotanteY, 0.08), 0.92)
        var acciones = Set<AccionIPad>()
        satelites = satelites.filter { satelite in
            guard acciones.insert(satelite.accion).inserted else { return false }
            return satelite.x.isFinite && satelite.y.isFinite
        }.map { satelite in
            var copia = satelite
            copia.x = min(max(copia.x, 0.04), 0.96)
            copia.y = min(max(copia.y, 0.04), 0.96)
            return copia
        }
    }
}

struct ConfiguracionInterfazIPad: Codable, Equatable {
    var perfil: PerfilInterfazIPad = .clase
    var nivel: NivelInterfazIPad = .simple
    var mostrarBarra = true
    var mostrarEtiquetasTecnicas = false
    var mostrarIndicadores = false
    var reducirMovimiento = false
    var ocultarSugerencias = true
    var barraCompacta = true

    mutating func aplicar(_ perfil: PerfilInterfazIPad) {
        self.perfil = perfil
        mostrarBarra = true
        mostrarEtiquetasTecnicas = false
        mostrarIndicadores = true
        reducirMovimiento = false
        ocultarSugerencias = true
        barraCompacta = false
        switch perfil {
        case .clase:
            mostrarBarra = true
            mostrarEtiquetasTecnicas = false
            mostrarIndicadores = false
            barraCompacta = true
        case .estudio:
            mostrarBarra = true
            mostrarEtiquetasTecnicas = false
            mostrarIndicadores = true
            barraCompacta = false
        case .tarea:
            mostrarBarra = true
            mostrarEtiquetasTecnicas = false
            mostrarIndicadores = true
            barraCompacta = false
        case .lectura:
            mostrarBarra = true
            mostrarEtiquetasTecnicas = false
            mostrarIndicadores = false
            barraCompacta = true
        case .circuitos:
            mostrarBarra = true
            mostrarEtiquetasTecnicas = true
            mostrarIndicadores = true
            barraCompacta = false
        case .examen:
            mostrarBarra = false
            mostrarEtiquetasTecnicas = false
            mostrarIndicadores = false
            reducirMovimiento = true
            barraCompacta = true
        case .sinDistracciones:
            mostrarBarra = false
            mostrarEtiquetasTecnicas = false
            mostrarIndicadores = false
            reducirMovimiento = true
            ocultarSugerencias = true
            barraCompacta = true
        }
    }
}

struct PerfilPapelCurso: Codable, Equatable {
    var tipoPapel: Int32 = TipoPapel.blanco.rawValue
    var pauta: Int32 = Pauta.ninguna.rawValue
    var separacion: Float = 32
    var margen: Float = 0
}

struct EspacioCursoIPad: Codable, Identifiable, Equatable {
    var id = UUID()
    var cursoId: UUID?
    var universidad = ""
    var semestre = ""
    var nombre: String
    var colorARGB: UInt32 = 0xFF0A_84FF
    var perfil: PerfilInterfazIPad = .clase
    var papel = PerfilPapelCurso()
    var perfilPlumaId: String?
    var calculadoraPreferida = "científica"
    var herramientasFijas: [AccionIPad] = [
        .pluma, .resaltador, .borradorObjeto, .lazo, .papel, .grabar
    ]
    var documentoActivo: UInt64?
    var fechaAcceso = Date()
    var archivado = false
}

enum TipoRelacionAcademica: String, Codable, CaseIterable {
    case perteneceA
    case continua
    case prerrequisito
    case explica
    case contradice
    case derivaDe
    case versionDe
    case entregaDe
}

struct RelacionAcademicaIPad: Codable, Identifiable, Equatable {
    var id = UUID()
    var origen: String
    var destino: String
    var tipo: TipoRelacionAcademica
    var cursoId: UUID?
    var fecha = Date()
    var nota = ""
}

struct DocumentoRecienteIPad: Codable, Identifiable, Equatable {
    var documentoId: UInt64
    var titulo: String
    var cursoId: UUID?
    var fecha = Date()
    var favoritoHasta: Date?
    var id: UInt64 { documentoId }
}

struct ReentradaIPad: Codable, Equatable {
    var fecha = Date()
    var documentoId: UInt64
    var titulo: String
    var cursoId: UUID?
    var objetivo: String?
    var accionPendiente: String?
    var vista: EstadoVistaLienzo
}

struct SugerenciaBarraIPad: Codable, Identifiable, Equatable {
    var id = UUID()
    var accion: AccionIPad
    var reemplazar: AccionIPad?
    var razon: String
    var fecha = Date()
}

private struct EstadoExperienciaIPad: Codable {
    private static let ordenBarraVersion2: [AccionIPad] = [
        .espaciosCurso, .deshacer, .papel,
        .centroAcademico, .calendarioCampus, .evaluacionEntrega, .multimediaClase,
        .estudioOral, .foco,
        .importar, .exportar, .grabar, .ajustes,
    ]
    private static let ordenBarraPredeterminado: [AccionIPad] = [
        .deshacer, .rehacer, .lienzo, .importar,
        .exportar, .calculadora, .papel, .ajustes,
        .espaciosCurso, .centroAcademico, .multimediaClase, .foco,
    ]

    var version = 3
    var espacios: [EspacioCursoIPad] = []
    var espacioActivoId: UUID?
    var interfaz = ConfiguracionInterfazIPad()
    var barra = ConfiguracionBarraIPad()
    var atajos = AtajoIPad.predeterminados
    var ordenBarra = EstadoExperienciaIPad.ordenBarraPredeterminado
    var usoAcciones: [String: Int] = [:]
    var sugerencias: [SugerenciaBarraIPad] = []
    var relaciones: [RelacionAcademicaIPad] = []
    var recientes: [DocumentoRecienteIPad] = []
    var reentrada: ReentradaIPad?
    var scratchGlobalId: UUID?

    enum CodingKeys: String, CodingKey {
        case version, espacios, espacioActivoId, interfaz, barra, atajos, ordenBarra
        case usoAcciones, sugerencias, relaciones, recientes, reentrada
        case scratchGlobalId
    }

    init() {}

    init(from decoder: Decoder) throws {
        let c = try decoder.container(keyedBy: CodingKeys.self)
        version = try c.decodeIfPresent(Int.self, forKey: .version) ?? 1
        espacios = try c.decodeIfPresent([EspacioCursoIPad].self,
                                         forKey: .espacios) ?? []
        espacioActivoId = try c.decodeIfPresent(UUID.self, forKey: .espacioActivoId)
        interfaz = try c.decodeIfPresent(ConfiguracionInterfazIPad.self,
                                         forKey: .interfaz) ?? ConfiguracionInterfazIPad()
        barra = try c.decodeIfPresent(ConfiguracionBarraIPad.self,
                                      forKey: .barra) ?? ConfiguracionBarraIPad()
        atajos = try c.decodeIfPresent([AtajoIPad].self,
                                       forKey: .atajos) ?? AtajoIPad.predeterminados
        ordenBarra = try c.decodeIfPresent([AccionIPad].self, forKey: .ordenBarra)
            ?? EstadoExperienciaIPad().ordenBarra
        usoAcciones = try c.decodeIfPresent([String: Int].self,
                                            forKey: .usoAcciones) ?? [:]
        sugerencias = try c.decodeIfPresent([SugerenciaBarraIPad].self,
                                            forKey: .sugerencias) ?? []
        relaciones = try c.decodeIfPresent([RelacionAcademicaIPad].self,
                                           forKey: .relaciones) ?? []
        recientes = try c.decodeIfPresent([DocumentoRecienteIPad].self,
                                          forKey: .recientes) ?? []
        reentrada = try c.decodeIfPresent(ReentradaIPad.self, forKey: .reentrada)
        scratchGlobalId = try c.decodeIfPresent(UUID.self, forKey: .scratchGlobalId)
        if version < 2 {
            // La primera build física arrancaba en modo avanzado, con toda la
            // telemetría visible. La migración conserva documentos y perfiles,
            // pero devuelve la superficie principal a un modo comprensible.
            interfaz.nivel = .simple
            interfaz.mostrarIndicadores = false
            interfaz.barraCompacta = true
            version = 2
        }
        if version < 3 {
            // La ubicación superior reproduce exactamente el comportamiento de
            // archivos anteriores. La nueva configuración agrega movimiento y
            // satélites sin alterar la barra que ya conocía la persona.
            barra = ConfiguracionBarraIPad()
            if ordenBarra == EstadoExperienciaIPad.ordenBarraVersion2 {
                ordenBarra = EstadoExperienciaIPad.ordenBarraPredeterminado
            }
            version = 3
        }
        barra.normalizar()
        var unicas = Set<AccionIPad>()
        ordenBarra = ordenBarra.filter { unicas.insert($0).inserted }
    }
}

struct InstantaneaExperienciaIPad {
    var espacios: [EspacioCursoIPad]
    var espacioActivo: EspacioCursoIPad?
    var interfaz: ConfiguracionInterfazIPad
    var barra: ConfiguracionBarraIPad
    var atajos: [AtajoIPad]
    var ordenBarra: [AccionIPad]
    var sugerencias: [SugerenciaBarraIPad]
    var relaciones: [RelacionAcademicaIPad]
    var recientes: [DocumentoRecienteIPad]
    var reentrada: ReentradaIPad?
}

/// Persistencia exclusivamente local de la experiencia del iPad. Está separada
/// del archivo académico para poder evolucionar perfiles y atajos sin volver
/// ilegible una base creada por una versión anterior.
final class CentroExperienciaIPad {
    static let compartido = CentroExperienciaIPad()

    private let bloqueo = NSRecursiveLock()
    private let url: URL
    private var estado: EstadoExperienciaIPad

    private init() {
        let documentos = FileManager.default.urls(for: .documentDirectory,
                                                   in: .userDomainMask).first!
        let carpeta = documentos.appendingPathComponent("Academico", isDirectory: true)
        try? FileManager.default.createDirectory(at: carpeta,
                                                 withIntermediateDirectories: true)
        url = carpeta.appendingPathComponent("experiencia-ipad.json")
        let decodificador = JSONDecoder()
        decodificador.dateDecodingStrategy = .iso8601
        if let datos = try? Data(contentsOf: url),
           let leido = try? decodificador.decode(EstadoExperienciaIPad.self,
                                                 from: datos) {
            estado = leido
        } else {
            estado = EstadoExperienciaIPad()
        }
    }

    func instantanea() -> InstantaneaExperienciaIPad {
        bloqueo.lock(); defer { bloqueo.unlock() }
        return InstantaneaExperienciaIPad(
            espacios: estado.espacios,
            espacioActivo: estado.espacios.first { $0.id == estado.espacioActivoId },
            interfaz: estado.interfaz,
            barra: estado.barra,
            atajos: estado.atajos,
            ordenBarra: estado.ordenBarra,
            sugerencias: estado.sugerencias,
            relaciones: estado.relaciones,
            recientes: estado.recientes,
            reentrada: estado.reentrada)
    }

    @discardableResult
    private func cambiar<T>(_ cuerpo: (inout EstadoExperienciaIPad) -> T) -> T {
        bloqueo.lock(); defer { bloqueo.unlock() }
        let resultado = cuerpo(&estado)
        guardarSinBloqueo()
        return resultado
    }

    private func guardarSinBloqueo() {
        let codificador = JSONEncoder()
        codificador.dateEncodingStrategy = .iso8601
        codificador.outputFormatting = [.prettyPrinted, .sortedKeys]
        guard let datos = try? codificador.encode(estado) else { return }
        try? datos.write(to: url, options: [.atomic, .completeFileProtection])
    }

    @discardableResult
    func asegurarEspacio(para curso: CursoAcademico) -> EspacioCursoIPad {
        cambiar { estado in
            if let indice = estado.espacios.firstIndex(where: {
                $0.cursoId == curso.id
            }) {
                estado.espacios[indice].nombre = curso.nombre
                estado.espacios[indice].semestre = curso.semestre
                estado.espacios[indice].colorARGB = curso.colorARGB
                return estado.espacios[indice]
            }
            let nuevo = EspacioCursoIPad(
                cursoId: curso.id, semestre: curso.semestre,
                nombre: curso.nombre, colorARGB: curso.colorARGB)
            estado.espacios.append(nuevo)
            if estado.espacioActivoId == nil { estado.espacioActivoId = nuevo.id }
            return nuevo
        }
    }

    @discardableResult
    func crearEspacioLibre(nombre: String, universidad: String = "",
                           semestre: String = "") -> EspacioCursoIPad {
        cambiar { estado in
            let nuevo = EspacioCursoIPad(
                universidad: universidad, semestre: semestre, nombre: nombre)
            estado.espacios.append(nuevo)
            estado.espacioActivoId = nuevo.id
            return nuevo
        }
    }

    func activarEspacio(_ id: UUID, documentoId: UInt64?) {
        cambiar { estado in
            guard let indice = estado.espacios.firstIndex(where: { $0.id == id }) else {
                return
            }
            estado.espacioActivoId = id
            estado.espacios[indice].fechaAcceso = Date()
            if let documentoId { estado.espacios[indice].documentoActivo = documentoId }
            estado.interfaz.aplicar(estado.espacios[indice].perfil)
        }
    }

    func actualizarPapelActivo(tipo: TipoPapel, pauta: Pauta,
                               separacion: Float, margen: Float) {
        cambiar { estado in
            guard let id = estado.espacioActivoId,
                  let indice = estado.espacios.firstIndex(where: { $0.id == id }) else {
                return
            }
            estado.espacios[indice].papel = PerfilPapelCurso(
                tipoPapel: tipo.rawValue, pauta: pauta.rawValue,
                separacion: separacion, margen: margen)
        }
    }

    func actualizarPlumaActiva(_ id: UUID?) {
        cambiar { estado in
            guard let espacioId = estado.espacioActivoId,
                  let indice = estado.espacios.firstIndex(where: {
                      $0.id == espacioId
                  }) else { return }
            estado.espacios[indice].perfilPlumaId = id?.uuidString
        }
    }

    func actualizarInterfaz(_ configuracion: ConfiguracionInterfazIPad) {
        cambiar { $0.interfaz = configuracion }
    }

    func actualizarBarra(_ configuracion: ConfiguracionBarraIPad) {
        cambiar { estado in
            var segura = configuracion
            segura.normalizar()
            estado.barra = segura
        }
    }

    func guardarOrdenBarra(_ acciones: [AccionIPad]) {
        cambiar { estado in
            var unicas = Set<AccionIPad>()
            estado.ordenBarra = acciones.filter { unicas.insert($0).inserted }
        }
    }

    func moverSatelite(_ id: UUID, x: Double, y: Double) {
        cambiar { estado in
            guard let indice = estado.barra.satelites.firstIndex(where: {
                $0.id == id
            }) else { return }
            estado.barra.satelites[indice].x = min(max(x, 0.04), 0.96)
            estado.barra.satelites[indice].y = min(max(y, 0.04), 0.96)
        }
    }

    func moverBarraFlotante(x: Double, y: Double) {
        cambiar { estado in
            estado.barra.centroFlotanteX = min(max(x, 0.08), 0.92)
            estado.barra.centroFlotanteY = min(max(y, 0.08), 0.92)
        }
    }

    @discardableResult
    func restaurarBarra() -> InstantaneaExperienciaIPad {
        cambiar { estado in
            let predeterminado = EstadoExperienciaIPad()
            estado.interfaz.mostrarBarra = true
            estado.interfaz.barraCompacta = true
            estado.barra = predeterminado.barra
            estado.ordenBarra = predeterminado.ordenBarra
        }
        return instantanea()
    }

    func aplicarPerfil(_ perfil: PerfilInterfazIPad) -> ConfiguracionInterfazIPad {
        cambiar { estado in
            estado.interfaz.aplicar(perfil)
            if let id = estado.espacioActivoId,
               let indice = estado.espacios.firstIndex(where: { $0.id == id }) {
                estado.espacios[indice].perfil = perfil
            }
            return estado.interfaz
        }
    }

    func fijarNivel(_ nivel: NivelInterfazIPad) -> ConfiguracionInterfazIPad {
        cambiar { estado in
            estado.interfaz.nivel = nivel
            return estado.interfaz
        }
    }

    func registrarUso(_ accion: AccionIPad) {
        cambiar { estado in
            estado.usoAcciones[accion.rawValue, default: 0] += 1
            guard !estado.interfaz.ocultarSugerencias,
                  !estado.ordenBarra.contains(accion),
                  !estado.sugerencias.contains(where: { $0.accion == accion }),
                  estado.usoAcciones[accion.rawValue, default: 0] >= 8 else { return }
            let menosUsada = estado.ordenBarra.min {
                estado.usoAcciones[$0.rawValue, default: 0]
                    < estado.usoAcciones[$1.rawValue, default: 0]
            }
            estado.sugerencias.append(SugerenciaBarraIPad(
                accion: accion, reemplazar: menosUsada,
                razon: "Se usó \(estado.usoAcciones[accion.rawValue, default: 0]) veces. La barra no cambiará hasta que usted acepte."))
        }
    }

    func aceptarSugerencia(_ id: UUID) {
        cambiar { estado in
            guard let sugerencia = estado.sugerencias.first(where: { $0.id == id }) else {
                return
            }
            if let reemplazo = sugerencia.reemplazar,
               let indice = estado.ordenBarra.firstIndex(of: reemplazo) {
                estado.ordenBarra[indice] = sugerencia.accion
            } else if !estado.ordenBarra.contains(sugerencia.accion) {
                estado.ordenBarra.append(sugerencia.accion)
            }
            estado.sugerencias.removeAll { $0.id == id }
        }
    }

    func descartarSugerencia(_ id: UUID) {
        cambiar { $0.sugerencias.removeAll { $0.id == id } }
    }

    func guardarAtajos(_ atajos: [AtajoIPad]) {
        cambiar { $0.atajos = atajos }
    }

    func registrarDocumento(_ documentoId: UInt64, titulo: String,
                            cursoId: UUID?) {
        cambiar { estado in
            estado.recientes.removeAll { $0.documentoId == documentoId }
            estado.recientes.insert(DocumentoRecienteIPad(
                documentoId: documentoId, titulo: titulo,
                cursoId: cursoId), at: 0)
            if estado.recientes.count > 40 {
                estado.recientes.removeLast(estado.recientes.count - 40)
            }
        }
    }

    func favoritoTemporal(documentoId: UInt64, hasta: Date?) {
        cambiar { estado in
            guard let indice = estado.recientes.firstIndex(where: {
                $0.documentoId == documentoId
            }) else { return }
            estado.recientes[indice].favoritoHasta = hasta
        }
    }

    func relacionar(origen: String, destino: String, tipo: TipoRelacionAcademica,
                    cursoId: UUID?, nota: String = "") {
        cambiar { estado in
            guard !estado.relaciones.contains(where: {
                $0.origen == origen && $0.destino == destino && $0.tipo == tipo
            }) else { return }
            estado.relaciones.append(RelacionAcademicaIPad(
                origen: origen, destino: destino, tipo: tipo,
                cursoId: cursoId, nota: nota))
        }
    }

    func archivarEspacios(semestre: String, valor: Bool) {
        let buscado = Normalizador.texto(semestre)
        cambiar { estado in
            for indice in estado.espacios.indices
            where Normalizador.texto(estado.espacios[indice].semestre) == buscado {
                let idEspacio = estado.espacios[indice].id
                estado.espacios[indice].archivado = valor
                if valor && estado.espacioActivoId == idEspacio {
                    estado.espacioActivoId = estado.espacios.first {
                        !$0.archivado && $0.id != idEspacio
                    }?.id
                }
            }
        }
    }

    @discardableResult
    func duplicarEspacio(origenCursoId: UUID,
                         para curso: CursoAcademico) -> EspacioCursoIPad {
        cambiar { estado in
            if let existente = estado.espacios.first(where: {
                $0.cursoId == curso.id
            }) { return existente }
            let origen = estado.espacios.first { $0.cursoId == origenCursoId }
            var copia = origen ?? EspacioCursoIPad(nombre: curso.nombre)
            copia.id = UUID()
            copia.cursoId = curso.id
            copia.nombre = curso.nombre
            copia.semestre = curso.semestre
            copia.colorARGB = curso.colorARGB
            copia.documentoActivo = nil
            copia.fechaAcceso = Date()
            copia.archivado = false
            estado.espacios.append(copia)
            return copia
        }
    }

    func guardarReentrada(_ reentrada: ReentradaIPad) {
        cambiar { $0.reentrada = reentrada }
    }

    @discardableResult
    func asegurarScratchGlobal() -> ScratchSpace {
        let idExistente: UUID? = {
            bloqueo.lock(); defer { bloqueo.unlock() }
            return estado.scratchGlobalId
        }()
        if let idExistente,
           let encontrado = CentroAcademico.compartido.instantanea().scratch.first(where: {
               $0.id == idExistente
           }) {
            return encontrado
        }
        let nuevo = ScratchSpace(titulo: "Scratchpad global", global: true)
        CentroAcademico.compartido.transaccion { $0.scratch.append(nuevo) }
        cambiar { $0.scratchGlobalId = nuevo.id }
        return nuevo
    }
}

// MARK: - Paleta de comandos

final class PaletaComandosIPad: UITableViewController, UISearchResultsUpdating {
    var alEjecutar: ((AccionIPad) -> Void)?

    private let buscador = UISearchController(searchResultsController: nil)
    private var resultados = AccionIPad.allCases

    override func viewDidLoad() {
        super.viewDidLoad()
        title = "Comandos"
        navigationItem.searchController = buscador
        navigationItem.hidesSearchBarWhenScrolling = false
        buscador.obscuresBackgroundDuringPresentation = false
        buscador.searchResultsUpdater = self
        buscador.searchBar.placeholder = "Buscar una acción"
        navigationItem.rightBarButtonItem = UIBarButtonItem(
            barButtonSystemItem: .close, target: self, action: #selector(cerrar))
        tableView.register(UITableViewCell.self, forCellReuseIdentifier: "comando")
        DispatchQueue.main.async { [weak self] in
            self?.buscador.searchBar.becomeFirstResponder()
        }
    }

    func updateSearchResults(for searchController: UISearchController) {
        let consulta = normalizar(searchController.searchBar.text ?? "")
        if consulta.isEmpty {
            resultados = AccionIPad.allCases
        } else {
            let terminos = consulta.split(separator: " ").map(String.init)
            resultados = AccionIPad.allCases.filter { accion in
                let indice = normalizar("\(accion.nombre) \(accion.palabrasClave)")
                return terminos.allSatisfy(indice.contains)
            }
        }
        tableView.reloadData()
    }

    override func tableView(_ tableView: UITableView,
                            numberOfRowsInSection section: Int) -> Int {
        resultados.count
    }

    override func tableView(_ tableView: UITableView,
                            cellForRowAt indexPath: IndexPath) -> UITableViewCell {
        let accion = resultados[indexPath.row]
        let celda = tableView.dequeueReusableCell(withIdentifier: "comando",
                                                  for: indexPath)
        var contenido = celda.defaultContentConfiguration()
        contenido.text = accion.nombre
        contenido.image = UIImage(systemName: accion.simbolo)
        celda.contentConfiguration = contenido
        return celda
    }

    override func tableView(_ tableView: UITableView,
                            didSelectRowAt indexPath: IndexPath) {
        let accion = resultados[indexPath.row]
        CentroExperienciaIPad.compartido.registrarUso(accion)
        dismiss(animated: true) { [weak self] in self?.alEjecutar?(accion) }
    }

    private func normalizar(_ texto: String) -> String {
        texto.folding(options: [.diacriticInsensitive, .caseInsensitive],
                      locale: .current).lowercased()
    }

    @objc private func cerrar() { dismiss(animated: true) }
}

// MARK: - Foco visual

enum ModoFocoVisualIPad: String, Codable, CaseIterable, Identifiable {
    case escrituraLibre
    case renglon
    case columna
    case lectura
    case formula
    case ejercicio
    case diapositiva

    var id: String { rawValue }

    var nombre: String {
        switch self {
        case .escrituraLibre: return "Escritura libre"
        case .renglon: return "Renglón actual"
        case .columna: return "Columna"
        case .lectura: return "Ventana de lectura"
        case .formula: return "Fórmula"
        case .ejercicio: return "Ejercicio"
        case .diapositiva: return "Diapositiva"
        }
    }
}

enum AparienciaTemporizadorIPad: String, Codable, CaseIterable, Identifiable {
    case grande
    case discreto
    case barra
    case oculto

    var id: String { rawValue }
    var nombre: String {
        switch self {
        case .grande: return "Grande"
        case .discreto: return "Discreto"
        case .barra: return "Barra de progreso"
        case .oculto: return "Oculto"
        }
    }
}

struct ConfiguracionFocoIPad: Codable, Equatable {
    var objetivo = ""
    var minutos = 25
    var modo: ModoFocoVisualIPad = .escrituraLibre
    var temporizador: AparienciaTemporizadorIPad = .discreto
    var ocultarBarra = true
    var bloquearCambioCurso = true
}

/// Máscara no interactiva: oscurece el exterior pero deja que Pencil y los
/// dedos sigan llegando al lienzo y al scroll que hay debajo.
final class MascaraFocoIPad: UIView {
    var modo: ModoFocoVisualIPad = .escrituraLibre { didSet { setNeedsLayout() } }
    var intensidad: CGFloat = 0.48 { didSet { setNeedsLayout() } }

    private let mascara = CAShapeLayer()

    override init(frame: CGRect) {
        super.init(frame: frame)
        isUserInteractionEnabled = false
        isAccessibilityElement = false
        layer.addSublayer(mascara)
    }

    required init?(coder: NSCoder) {
        super.init(coder: coder)
        isUserInteractionEnabled = false
        layer.addSublayer(mascara)
    }

    override func layoutSubviews() {
        super.layoutSubviews()
        mascara.frame = bounds
        guard modo != .escrituraLibre else {
            mascara.path = nil
            return
        }
        let exterior = UIBezierPath(rect: bounds)
        exterior.append(UIBezierPath(roundedRect: regionClara(), cornerRadius: 18))
        exterior.usesEvenOddFillRule = true
        mascara.path = exterior.cgPath
        mascara.fillRule = .evenOdd
        mascara.fillColor = UIColor.black.withAlphaComponent(intensidad).cgColor
    }

    private func regionClara() -> CGRect {
        let seguro = bounds.insetBy(dx: max(22, bounds.width * 0.04),
                                   dy: max(22, bounds.height * 0.04))
        switch modo {
        case .escrituraLibre:
            return bounds
        case .renglon:
            return CGRect(x: seguro.minX, y: seguro.midY - 64,
                          width: seguro.width, height: 128)
        case .columna:
            return CGRect(x: seguro.midX - min(300, seguro.width * 0.34),
                          y: seguro.minY,
                          width: min(600, seguro.width * 0.68),
                          height: seguro.height)
        case .lectura:
            return seguro.insetBy(dx: seguro.width * 0.10,
                                  dy: seguro.height * 0.06)
        case .formula:
            return CGRect(x: seguro.midX - min(360, seguro.width * 0.42),
                          y: seguro.midY - 150,
                          width: min(720, seguro.width * 0.84), height: 300)
        case .ejercicio:
            let ancho = min(760, seguro.width * 0.88)
            return CGRect(x: seguro.midX - ancho / 2,
                          y: seguro.minY + seguro.height * 0.08,
                          width: ancho, height: seguro.height * 0.72)
        case .diapositiva:
            let ancho = min(seguro.width, seguro.height * 4 / 3)
            return CGRect(x: seguro.midX - ancho / 2,
                          y: seguro.midY - ancho * 3 / 8,
                          width: ancho, height: ancho * 3 / 4)
        }
    }
}

final class ControlFocoFlotanteIPad: UIVisualEffectView {
    var alAparcar: (() -> Void)?
    var alInterrupcion: (() -> Void)?
    var alTerminar: (() -> Void)?

    private let objetivo = UILabel()
    private let tiempo = UILabel()
    private let progreso = UIProgressView(progressViewStyle: .bar)

    init() {
        super.init(effect: UIBlurEffect(style: .systemMaterial))
        clipsToBounds = true
        layer.cornerRadius = 16

        objetivo.font = .preferredFont(forTextStyle: .subheadline)
        objetivo.numberOfLines = 2
        tiempo.font = .monospacedDigitSystemFont(ofSize: 16, weight: .semibold)

        let aparcar = boton("Parking", simbolo: "tray.and.arrow.down",
                            accion: #selector(aparcar))
        let interrupcion = boton("Interrupción", simbolo: "bolt.slash",
                                 accion: #selector(interrumpir))
        let terminar = boton("Terminar", simbolo: "checkmark.circle",
                             accion: #selector(terminar))
        let acciones = UIStackView(arrangedSubviews: [aparcar, interrupcion, terminar])
        acciones.axis = .horizontal
        acciones.spacing = 8
        acciones.distribution = .fillEqually

        let pila = UIStackView(arrangedSubviews: [objetivo, tiempo, progreso, acciones])
        pila.axis = .vertical
        pila.spacing = 8
        pila.translatesAutoresizingMaskIntoConstraints = false
        contentView.addSubview(pila)
        NSLayoutConstraint.activate([
            pila.leadingAnchor.constraint(equalTo: contentView.leadingAnchor, constant: 12),
            pila.trailingAnchor.constraint(equalTo: contentView.trailingAnchor, constant: -12),
            pila.topAnchor.constraint(equalTo: contentView.topAnchor, constant: 10),
            pila.bottomAnchor.constraint(equalTo: contentView.bottomAnchor, constant: -10),
            widthAnchor.constraint(greaterThanOrEqualToConstant: 320)
        ])
    }

    required init?(coder: NSCoder) { fatalError("init(coder:) no implementado") }

    private func boton(_ titulo: String, simbolo: String, accion: Selector) -> UIButton {
        let boton = UIButton(type: .system)
        boton.configuration = .borderless()
        boton.configuration?.title = titulo
        boton.configuration?.image = UIImage(systemName: simbolo)
        boton.configuration?.imagePadding = 4
        boton.titleLabel?.font = .preferredFont(forTextStyle: .caption1)
        boton.addTarget(self, action: accion, for: .touchUpInside)
        return boton
    }

    func actualizar(objetivo texto: String, restantes: TimeInterval,
                    duracion: TimeInterval, apariencia: AparienciaTemporizadorIPad) {
        objetivo.text = texto.isEmpty ? "Sesión de enfoque" : texto
        let segundos = max(0, Int(restantes.rounded(.up)))
        tiempo.text = String(format: "%02d:%02d", segundos / 60, segundos % 60)
        progreso.progress = duracion > 0
            ? Float(1 - min(max(restantes / duracion, 0), 1)) : 0
        switch apariencia {
        case .grande:
            tiempo.isHidden = false; progreso.isHidden = true
            tiempo.font = .monospacedDigitSystemFont(ofSize: 30, weight: .bold)
        case .discreto:
            tiempo.isHidden = false; progreso.isHidden = true
            tiempo.font = .monospacedDigitSystemFont(ofSize: 16, weight: .semibold)
        case .barra:
            tiempo.isHidden = true; progreso.isHidden = false
        case .oculto:
            tiempo.isHidden = true; progreso.isHidden = true
        }
    }

    @objc private func aparcar() { alAparcar?() }
    @objc private func interrumpir() { alInterrupcion?() }
    @objc private func terminar() { alTerminar?() }
}

final class CoordinadorFocoIPad {
    var alActualizar: ((TimeInterval, TimeInterval, AparienciaTemporizadorIPad) -> Void)?
    var alTerminar: (() -> Void)?

    private(set) var configuracion: ConfiguracionFocoIPad?
    private(set) var sesionId: UUID?
    private var inicio: Date?
    private var temporizador: Timer?

    var activo: Bool { configuracion != nil }

    func iniciar(_ configuracion: ConfiguracionFocoIPad, tareaId: UUID? = nil) {
        terminar(registrar: false)
        self.configuracion = configuracion
        inicio = Date()
        let sesion = SesionFoco(objetivo: configuracion.objetivo,
                                duracionMinutos: configuracion.minutos,
                                tareaId: tareaId)
        sesionId = sesion.id
        CentroAcademico.compartido.transaccion { base in
            base.foco.append(sesion)
            base.timeline.append(EventoAcademico(
                tipo: .foco,
                titulo: configuracion.objetivo.isEmpty
                    ? "Sesión de enfoque" : configuracion.objetivo))
        }
        emitir()
        temporizador = Timer.scheduledTimer(withTimeInterval: 1, repeats: true) {
            [weak self] _ in self?.tic()
        }
    }

    func registrarInterrupcion() {
        guard let sesionId else { return }
        CentroAcademico.compartido.transaccion { base in
            guard let indice = base.foco.firstIndex(where: { $0.id == sesionId }) else {
                return
            }
            base.foco[indice].interrupciones += 1
        }
    }

    func aparcar(_ texto: String) {
        let limpio = texto.trimmingCharacters(in: .whitespacesAndNewlines)
        guard !limpio.isEmpty else { return }
        CentroAcademico.compartido.transaccion { base in
            base.parking.append(PensamientoParking(texto: limpio))
        }
    }

    func terminar(registrar: Bool = true) {
        temporizador?.invalidate()
        temporizador = nil
        if registrar, let sesionId {
            CentroAcademico.compartido.transaccion { base in
                guard let indice = base.foco.firstIndex(where: { $0.id == sesionId }) else {
                    return
                }
                base.foco[indice].fin = Date()
                base.foco[indice].activa = false
            }
        }
        configuracion = nil
        inicio = nil
        sesionId = nil
    }

    private func tic() {
        guard let configuracion, let inicio else { return }
        let duracion = TimeInterval(configuracion.minutos * 60)
        if Date().timeIntervalSince(inicio) >= duracion {
            terminar()
            alTerminar?()
        } else {
            emitir()
        }
    }

    private func emitir() {
        guard let configuracion, let inicio else { return }
        let duracion = TimeInterval(configuracion.minutos * 60)
        let restante = max(0, duracion - Date().timeIntervalSince(inicio))
        alActualizar?(restante, duracion, configuracion.temporizador)
    }
}

final class PanelFocoIPad: UITableViewController {
    var alIniciar: ((ConfiguracionFocoIPad) -> Void)?

    private var configuracion = ConfiguracionFocoIPad()
    private let objetivo = UITextField()
    private let minutos = UIStepper()

    init() { super.init(style: .insetGrouped) }
    required init?(coder: NSCoder) { fatalError("init(coder:) no implementado") }

    override func viewDidLoad() {
        super.viewDidLoad()
        title = "Enfoque"
        objetivo.placeholder = "Objetivo concreto de esta sesión"
        objetivo.clearButtonMode = .whileEditing
        minutos.minimumValue = 1
        minutos.maximumValue = 180
        minutos.stepValue = 5
        minutos.value = Double(configuracion.minutos)
        minutos.addTarget(self, action: #selector(cambiarMinutos), for: .valueChanged)
        navigationItem.leftBarButtonItem = UIBarButtonItem(
            barButtonSystemItem: .cancel, target: self, action: #selector(cerrar))
        navigationItem.rightBarButtonItem = UIBarButtonItem(
            title: "Empezar", style: .done, target: self, action: #selector(empezar))
    }

    override func numberOfSections(in tableView: UITableView) -> Int { 4 }

    override func tableView(_ tableView: UITableView,
                            numberOfRowsInSection section: Int) -> Int {
        switch section {
        case 0: return 2
        case 1: return ModoFocoVisualIPad.allCases.count
        case 2: return AparienciaTemporizadorIPad.allCases.count
        default: return 2
        }
    }

    override func tableView(_ tableView: UITableView,
                            titleForHeaderInSection section: Int) -> String? {
        ["Sesión", "Ventana visual", "Temporizador", "Barrera consciente"][section]
    }

    override func tableView(_ tableView: UITableView,
                            cellForRowAt indexPath: IndexPath) -> UITableViewCell {
        let celda = UITableViewCell(style: .value1, reuseIdentifier: nil)
        switch indexPath.section {
        case 0 where indexPath.row == 0:
            objetivo.frame = CGRect(x: 20, y: 0,
                                    width: tableView.bounds.width - 70, height: 44)
            objetivo.autoresizingMask = [.flexibleWidth, .flexibleHeight]
            celda.contentView.addSubview(objetivo)
        case 0:
            celda.textLabel?.text = "Duración"
            celda.detailTextLabel?.text = "\(configuracion.minutos) min"
            celda.accessoryView = minutos
        case 1:
            let modo = ModoFocoVisualIPad.allCases[indexPath.row]
            celda.textLabel?.text = modo.nombre
            celda.accessoryType = modo == configuracion.modo ? .checkmark : .none
        case 2:
            let apariencia = AparienciaTemporizadorIPad.allCases[indexPath.row]
            celda.textLabel?.text = apariencia.nombre
            celda.accessoryType = apariencia == configuracion.temporizador
                ? .checkmark : .none
        default:
            let interruptor = UISwitch()
            if indexPath.row == 0 {
                celda.textLabel?.text = "Ocultar barra"
                interruptor.isOn = configuracion.ocultarBarra
                interruptor.addAction(UIAction { [weak self, weak interruptor] _ in
                    self?.configuracion.ocultarBarra = interruptor?.isOn ?? true
                }, for: .valueChanged)
            } else {
                celda.textLabel?.text = "Confirmar cambio de curso"
                interruptor.isOn = configuracion.bloquearCambioCurso
                interruptor.addAction(UIAction { [weak self, weak interruptor] _ in
                    self?.configuracion.bloquearCambioCurso = interruptor?.isOn ?? true
                }, for: .valueChanged)
            }
            celda.accessoryView = interruptor
        }
        return celda
    }

    override func tableView(_ tableView: UITableView,
                            didSelectRowAt indexPath: IndexPath) {
        if indexPath.section == 1 {
            configuracion.modo = ModoFocoVisualIPad.allCases[indexPath.row]
        } else if indexPath.section == 2 {
            configuracion.temporizador = AparienciaTemporizadorIPad.allCases[indexPath.row]
        }
        tableView.reloadSections(IndexSet(integer: indexPath.section), with: .none)
    }

    @objc private func cambiarMinutos() {
        configuracion.minutos = Int(minutos.value)
        tableView.reloadRows(at: [IndexPath(row: 1, section: 0)], with: .none)
    }

    @objc private func empezar() {
        configuracion.objetivo = objetivo.text?.trimmingCharacters(
            in: .whitespacesAndNewlines) ?? ""
        dismiss(animated: true) { [weak self] in
            guard let self else { return }
            self.alIniciar?(self.configuracion)
        }
    }

    @objc private func cerrar() { dismiss(animated: true) }
}

// MARK: - Panel de espacios y perfiles

final class PanelExperienciaIPad: UITableViewController {
    var alAplicarInterfaz: ((ConfiguracionInterfazIPad) -> Void)?
    var alCambiarEspacio: ((EspacioCursoIPad) -> Void)?
    var alAbrirPaleta: (() -> Void)?
    var alAbrirFoco: (() -> Void)?
    var alAbrirScratch: ((ScratchSpace) -> Void)?
    var alCambiarBarra: (() -> Void)?

    private var estado = CentroExperienciaIPad.compartido.instantanea()
    private var cursos: [CursoAcademico] = []

    init() { super.init(style: .insetGrouped) }
    required init?(coder: NSCoder) { fatalError("init(coder:) no implementado") }

    override func viewDidLoad() {
        super.viewDidLoad()
        title = "iPad: espacios y perfiles"
        navigationItem.leftBarButtonItem = UIBarButtonItem(
            barButtonSystemItem: .close, target: self, action: #selector(cerrar))
        navigationItem.rightBarButtonItem = UIBarButtonItem(
            barButtonSystemItem: .add, target: self, action: #selector(anadirCurso))
        refrescar()
    }

    private func refrescar() {
        cursos = CentroAcademico.compartido.instantanea().cursos
            .filter { !$0.archivado }
            .sorted { $0.nombre.localizedCaseInsensitiveCompare($1.nombre) == .orderedAscending }
        for curso in cursos {
            _ = CentroExperienciaIPad.compartido.asegurarEspacio(para: curso)
        }
        estado = CentroExperienciaIPad.compartido.instantanea()
        tableView.reloadData()
    }

    override func numberOfSections(in tableView: UITableView) -> Int { 7 }

    override func tableView(_ tableView: UITableView,
                            numberOfRowsInSection section: Int) -> Int {
        switch section {
        case 0: return max(1, estado.espacios.count)
        case 1: return PerfilInterfazIPad.allCases.count
        case 2: return 2
        case 3: return max(1, estado.sugerencias.count)
        case 4: return min(estado.recientes.count, 8)
        case 5: return 2
        default: return 2
        }
    }

    override func tableView(_ tableView: UITableView,
                            titleForHeaderInSection section: Int) -> String? {
        ["Espacios por curso", "Perfil de interfaz", "Complejidad",
         "Sugerencias de barra", "Recientes", "Captura", "Acceso rápido"][section]
    }

    override func tableView(_ tableView: UITableView,
                            titleForFooterInSection section: Int) -> String? {
        switch section {
        case 0:
            return "Cada curso conserva papel, plumas, calculadora, perfil y último documento."
        case 3:
            return "La barra nunca se reordena sola. Una propuesta solo se aplica al aceptarla."
        case 5:
            return "El scratchpad y la bandeja permiten capturar antes de decidir una carpeta."
        default: return nil
        }
    }

    override func tableView(_ tableView: UITableView,
                            cellForRowAt indexPath: IndexPath) -> UITableViewCell {
        let celda = UITableViewCell(style: .subtitle, reuseIdentifier: nil)
        switch indexPath.section {
        case 0:
            guard !estado.espacios.isEmpty else {
                celda.textLabel?.text = "Crear primer curso"
                celda.imageView?.image = UIImage(systemName: "plus.circle")
                return celda
            }
            let espacio = estado.espacios[indexPath.row]
            celda.textLabel?.text = espacio.nombre
            celda.detailTextLabel?.text = [espacio.semestre, espacio.perfil.nombre]
                .filter { !$0.isEmpty }.joined(separator: " · ")
            celda.imageView?.image = UIImage(systemName: "rectangle.3.group")
            celda.accessoryType = estado.espacioActivo?.id == espacio.id
                ? .checkmark : .none
        case 1:
            let perfil = PerfilInterfazIPad.allCases[indexPath.row]
            celda.textLabel?.text = perfil.nombre
            celda.detailTextLabel?.text = perfil.detalle
            celda.accessoryType = estado.interfaz.perfil == perfil ? .checkmark : .none
        case 2:
            let nivel = NivelInterfazIPad.allCases[indexPath.row]
            celda.textLabel?.text = nivel.nombre
            celda.detailTextLabel?.text = nivel == .simple
                ? "Acciones esenciales; las funciones siguen disponibles en Comandos."
                : "Barra y paneles técnicos completos."
            celda.accessoryType = estado.interfaz.nivel == nivel ? .checkmark : .none
        case 3:
            guard !estado.sugerencias.isEmpty else {
                celda.textLabel?.text = "Sin sugerencias pendientes"
                celda.detailTextLabel?.text = "La barra conserva el orden elegido."
                celda.selectionStyle = .none
                return celda
            }
            let sugerencia = estado.sugerencias[indexPath.row]
            celda.textLabel?.text = "Agregar \(sugerencia.accion.nombre)"
            celda.detailTextLabel?.text = sugerencia.razon
            celda.imageView?.image = UIImage(systemName: sugerencia.accion.simbolo)
        case 4:
            let reciente = estado.recientes[indexPath.row]
            celda.textLabel?.text = reciente.titulo
            celda.detailTextLabel?.text = Self.fecha.string(from: reciente.fecha)
            celda.imageView?.image = UIImage(systemName:
                (reciente.favoritoHasta ?? .distantPast) > Date() ? "star.fill" : "doc")
        case 5:
            if indexPath.row == 0 {
                celda.textLabel?.text = "Scratchpad global"
                celda.detailTextLabel?.text = "Un espacio infinito sin curso obligatorio."
                celda.imageView?.image = UIImage(systemName: "scribble.variable")
            } else {
                celda.textLabel?.text = "Capturar primero"
                celda.detailTextLabel?.text = "Guardar una idea en la bandeja sin clasificar."
                celda.imageView?.image = UIImage(systemName: "tray.and.arrow.down")
            }
        default:
            if indexPath.row == 0 {
                celda.textLabel?.text = "Paleta de comandos"
                celda.detailTextLabel?.text = "⌘K"
                celda.imageView?.image = UIImage(systemName: "command")
            } else {
                celda.textLabel?.text = "Modo de enfoque"
                celda.detailTextLabel?.text = "Objetivo, temporizador y siete ventanas visuales."
                celda.imageView?.image = UIImage(systemName: "scope")
            }
        }
        return celda
    }

    override func tableView(_ tableView: UITableView,
                            didSelectRowAt indexPath: IndexPath) {
        defer { tableView.deselectRow(at: indexPath, animated: true) }
        switch indexPath.section {
        case 0:
            guard !estado.espacios.isEmpty else { anadirCurso(); return }
            let espacio = estado.espacios[indexPath.row]
            dismiss(animated: true) { [weak self] in
                self?.alCambiarEspacio?(espacio)
            }
        case 1:
            let perfil = PerfilInterfazIPad.allCases[indexPath.row]
            let configuracion = CentroExperienciaIPad.compartido.aplicarPerfil(perfil)
            estado = CentroExperienciaIPad.compartido.instantanea()
            tableView.reloadSections(IndexSet(integer: 1), with: .none)
            alAplicarInterfaz?(configuracion)
        case 2:
            let nivel = NivelInterfazIPad.allCases[indexPath.row]
            let configuracion = CentroExperienciaIPad.compartido.fijarNivel(nivel)
            estado = CentroExperienciaIPad.compartido.instantanea()
            tableView.reloadSections(IndexSet(integer: 2), with: .none)
            alAplicarInterfaz?(configuracion)
        case 3:
            guard !estado.sugerencias.isEmpty else { return }
            ofrecerSugerencia(estado.sugerencias[indexPath.row])
        case 4:
            let reciente = estado.recientes[indexPath.row]
            let hasta = Date().addingTimeInterval(7 * 86_400)
            CentroExperienciaIPad.compartido.favoritoTemporal(
                documentoId: reciente.documentoId, hasta: hasta)
            refrescar()
        case 5 where indexPath.row == 0:
            let scratch = CentroExperienciaIPad.compartido.asegurarScratchGlobal()
            dismiss(animated: true) { [weak self] in
                self?.alAbrirScratch?(scratch)
            }
        case 5:
            capturarPrimero()
        case 6 where indexPath.row == 0:
            dismiss(animated: true) { [weak self] in self?.alAbrirPaleta?() }
        case 6:
            dismiss(animated: true) { [weak self] in self?.alAbrirFoco?() }
        default: break
        }
    }

    private func ofrecerSugerencia(_ sugerencia: SugerenciaBarraIPad) {
        let alerta = UIAlertController(
            title: sugerencia.accion.nombre, message: sugerencia.razon,
            preferredStyle: .actionSheet)
        alerta.addAction(UIAlertAction(title: "Aceptar cambio", style: .default) {
            [weak self] _ in
            CentroExperienciaIPad.compartido.aceptarSugerencia(sugerencia.id)
            self?.refrescar()
            self?.alCambiarBarra?()
        })
        alerta.addAction(UIAlertAction(title: "Descartar", style: .destructive) {
            [weak self] _ in
            CentroExperienciaIPad.compartido.descartarSugerencia(sugerencia.id)
            self?.refrescar()
        })
        alerta.addAction(UIAlertAction(title: "Cancelar", style: .cancel))
        alerta.popoverPresentationController?.sourceView = view
        present(alerta, animated: true)
    }

    private func capturarPrimero() {
        let alerta = UIAlertController(title: "Capturar primero", message: nil,
                                       preferredStyle: .alert)
        alerta.addTextField { $0.placeholder = "Idea, duda, tarea o recordatorio" }
        alerta.addAction(UIAlertAction(title: "Cancelar", style: .cancel))
        alerta.addAction(UIAlertAction(title: "Guardar en bandeja", style: .default) { _ in
            guard let texto = alerta.textFields?.first?.text,
                  !texto.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty else {
                return
            }
            _ = CentroAcademico.compartido.capturar(texto, tipo: .pensamiento,
                cursoSugerido: self.estado.espacioActivo?.cursoId)
        })
        present(alerta, animated: true)
    }

    @objc private func anadirCurso() {
        let alerta = UIAlertController(title: "Nuevo curso", message: nil,
                                       preferredStyle: .alert)
        alerta.addTextField { $0.placeholder = "Nombre del curso" }
        alerta.addTextField { $0.placeholder = "Código (opcional)" }
        alerta.addTextField { $0.placeholder = "Semestre (opcional)" }
        alerta.addAction(UIAlertAction(title: "Cancelar", style: .cancel))
        alerta.addAction(UIAlertAction(title: "Crear", style: .default) { [weak self] _ in
            guard let self,
                  let nombre = alerta.textFields?[0].text?.trimmingCharacters(
                    in: .whitespacesAndNewlines), !nombre.isEmpty else { return }
            var curso = CursoAcademico(nombre: nombre)
            curso.codigo = alerta.textFields?[1].text ?? ""
            curso.semestre = alerta.textFields?[2].text ?? ""
            CentroAcademico.compartido.transaccion { $0.cursos.append(curso) }
            let espacio = CentroExperienciaIPad.compartido.asegurarEspacio(para: curso)
            self.dismiss(animated: true) { [weak self] in
                self?.alCambiarEspacio?(espacio)
            }
        })
        present(alerta, animated: true)
    }

    @objc private func cerrar() { dismiss(animated: true) }

    private static let fecha: DateFormatter = {
        let formato = DateFormatter()
        formato.dateStyle = .medium
        formato.timeStyle = .short
        return formato
    }()
}

// MARK: - Scratchpad global persistente

private final class RepositorioScratchIPad {
    static let compartido = RepositorioScratchIPad()
    private let carpeta: URL

    private init() {
        let documentos = FileManager.default.urls(for: .documentDirectory,
                                                   in: .userDomainMask).first!
        carpeta = documentos.appendingPathComponent("Scratchpads", isDirectory: true)
        try? FileManager.default.createDirectory(at: carpeta,
                                                 withIntermediateDirectories: true)
    }

    private func url(_ id: UUID) -> URL {
        carpeta.appendingPathComponent(id.uuidString).appendingPathExtension("tinta")
    }

    func cargar(_ id: UUID) -> Data? {
        try? Data(contentsOf: url(id), options: .mappedIfSafe)
    }

    func existe(_ id: UUID) -> Bool {
        FileManager.default.fileExists(atPath: url(id).path)
    }

    func guardar(_ datos: Data, id: UUID) throws {
        try datos.write(to: url(id), options: [.atomic, .completeFileProtection])
    }
}

/// Lienzo independiente y persistente para cálculos, ideas y trazos que aún no
/// pertenecen a un curso. No altera ni reemplaza el cuaderno que estaba abierto.
final class ControladorScratchGlobalIPad: UIViewController {
    private let scratch: ScratchSpace
    private let lienzo: LienzoCuadernU
    private lazy var navegador = VistaLienzoNavegable(lienzo: lienzo)
    private let herramientas: UISegmentedControl = {
        let items: [Any] = HerramientaLienzo.allCases.map {
            UIImage(systemName: $0.simbolo) ?? UIImage()
        }
        return UISegmentedControl(items: items)
    }()
    private var guardadoPendiente: DispatchWorkItem?
    private var cargaBloqueada = false

    init(scratch: ScratchSpace) {
        self.scratch = scratch
        self.lienzo = LienzoCuadernU(
            idPersistente: DocumentoPersistenteID(scratch.id))
        super.init(nibName: nil, bundle: nil)
    }

    required init?(coder: NSCoder) { fatalError("init(coder:) no implementado") }

    override func viewDidLoad() {
        super.viewDidLoad()
        view.backgroundColor = .systemBackground
        title = scratch.titulo
        navigationItem.leftBarButtonItem = UIBarButtonItem(
            barButtonSystemItem: .done, target: self, action: #selector(cerrar))
        navigationItem.rightBarButtonItems = [
            UIBarButtonItem(barButtonSystemItem: .save, target: self,
                            action: #selector(guardarAhora)),
            UIBarButtonItem(image: UIImage(systemName: "tray.and.arrow.down"),
                            style: .plain, target: self,
                            action: #selector(capturarEnBandeja))
        ]

        herramientas.selectedSegmentIndex = HerramientaLienzo.pluma.rawValue
        herramientas.addTarget(self, action: #selector(cambiarHerramienta),
                               for: .valueChanged)
        herramientas.translatesAutoresizingMaskIntoConstraints = false
        navegador.translatesAutoresizingMaskIntoConstraints = false
        view.addSubview(herramientas)
        view.addSubview(navegador)
        NSLayoutConstraint.activate([
            herramientas.topAnchor.constraint(equalTo: view.safeAreaLayoutGuide.topAnchor,
                                                constant: 8),
            herramientas.centerXAnchor.constraint(equalTo: view.centerXAnchor),
            navegador.topAnchor.constraint(equalTo: herramientas.bottomAnchor, constant: 8),
            navegador.leadingAnchor.constraint(equalTo: view.leadingAnchor),
            navegador.trailingAnchor.constraint(equalTo: view.trailingAnchor),
            navegador.bottomAnchor.constraint(equalTo: view.bottomAnchor)
        ])

        navegador.aplicarModo(.infinitoXY)
        if let datos = RepositorioScratchIPad.compartido.cargar(scratch.id) {
            if lienzo.sesion.cargar(datos) {
                lienzo.rehornearYDibujar()
            } else {
                cargaBloqueada = true
                presentarErrorDeCarga(
                    "El scratchpad existe, pero su contenido no es válido o pertenece a otra identidad. El archivo original se conservó y no se sobrescribirá.")
            }
        } else if RepositorioScratchIPad.compartido.existe(scratch.id) {
            cargaBloqueada = true
            lienzo.isUserInteractionEnabled = false
            presentarErrorDeCarga(
                "El archivo existe, pero iPadOS no permitió leerlo ahora. No se guardará un lienzo vacío encima.")
        }
        lienzo.alCambiarActividad = { [weak self] activo in
            if !activo { self?.programarGuardado() }
        }
        lienzo.alDesplazarConHerramienta = { [weak self] delta in
            self?.navegador.desplazarConHerramienta(delta)
        }
        NotificationCenter.default.addObserver(
            self, selector: #selector(alEntrarFondo(_:)),
            name: UIApplication.didEnterBackgroundNotification, object: nil)
    }

    override func viewWillDisappear(_ animated: Bool) {
        super.viewWillDisappear(animated)
        guardarAhora()
    }

    deinit { NotificationCenter.default.removeObserver(self) }

    @objc private func cambiarHerramienta() {
        guard let herramienta = HerramientaLienzo(
            rawValue: herramientas.selectedSegmentIndex) else { return }
        lienzo.herramienta = herramienta
    }

    private func programarGuardado() {
        guardadoPendiente?.cancel()
        let trabajo = DispatchWorkItem { [weak self] in self?.guardarAhora() }
        guardadoPendiente = trabajo
        DispatchQueue.main.asyncAfter(deadline: .now() + 1.2, execute: trabajo)
    }

    @objc private func guardarAhora() {
        guard !cargaBloqueada else { return }
        guard let datos = lienzo.sesion.guardar() else { return }
        try? RepositorioScratchIPad.compartido.guardar(datos, id: scratch.id)
        CentroAcademico.compartido.transaccion { base in
            guard let indice = base.scratch.firstIndex(where: { $0.id == scratch.id }) else {
                return
            }
            base.scratch[indice].documentoId = lienzo.sesion.documentoPersistenteId
            base.scratch[indice].fecha = Date()
        }
    }

    private func presentarErrorDeCarga(_ mensaje: String) {
        lienzo.isUserInteractionEnabled = false
        DispatchQueue.main.async { [weak self] in
            let alerta = UIAlertController(
                title: "Scratchpad protegido", message: mensaje,
                preferredStyle: .alert)
            alerta.addAction(UIAlertAction(title: "Cerrar", style: .default) { _ in
                self?.dismiss(animated: true)
            })
            self?.present(alerta, animated: true)
        }
    }

    @objc private func alEntrarFondo(_ notificacion: Notification) {
        guardarAhora()
    }

    @objc private func capturarEnBandeja() {
        let alerta = UIAlertController(title: "Enviar idea a la bandeja", message: nil,
                                       preferredStyle: .alert)
        alerta.addTextField { $0.placeholder = "Describa este fragmento" }
        alerta.addAction(UIAlertAction(title: "Cancelar", style: .cancel))
        alerta.addAction(UIAlertAction(title: "Capturar", style: .default) { _ in
            guard let texto = alerta.textFields?.first?.text,
                  !texto.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty else {
                return
            }
            let capturaId = CentroAcademico.compartido.capturar(
                texto, tipo: .pensamiento)
            CentroExperienciaIPad.compartido.relacionar(
                origen: "scratch:\(self.scratch.id.uuidString)",
                destino: "captura:\(capturaId.uuidString)", tipo: .derivaDe,
                cursoId: nil)
        })
        present(alerta, animated: true)
    }

    @objc private func cerrar() {
        guardarAhora()
        dismiss(animated: true)
    }
}
