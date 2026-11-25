import Foundation

/// Estado puro del emparejamiento HTTP. El código del QR sirve una sola vez
/// para abrir la página; después las APIs usan una cookie de sesión HttpOnly y
/// una secuencia estricta. HTTP local sigue sin dar confidencialidad frente a
/// un sniffer de la LAN, pero copiar un QR viejo o repetir una petición ya no
/// concede control.
struct EstadoAutorizacionWebIPad {
    enum Apertura: Equatable {
        case rechazada
        case existente(siguienteSecuencia: UInt64)
        case creada(token: String, siguienteSecuencia: UInt64)
    }

    enum ResultadoAPI: Equatable {
        case permitida
        case credencialInvalida
        case secuenciaInvalida
        case expirada
    }

    private struct Sesion {
        var token: String
        var expira: Date
        var siguienteSecuencia: UInt64 = 1
    }

    private(set) var codigo: String
    private(set) var codigoDisponible = true
    private var sesion: Sesion?
    let duracion: TimeInterval

    init(codigo: String, duracion: TimeInterval = 2 * 60 * 60) {
        self.codigo = codigo
        self.duracion = duracion
    }

    var codigoActual: String? { codigoDisponible ? codigo : nil }

    mutating func renovar(codigo nuevo: String) {
        codigo = nuevo
        codigoDisponible = true
        sesion = nil
    }

    mutating func invalidar() {
        codigoDisponible = false
        sesion = nil
    }

    mutating func expirar(siCorresponde fecha: Date) {
        if let sesion, sesion.expira <= fecha { self.sesion = nil }
    }

    mutating func abrir(
        codigo presentado: String?, cookie: String?, nuevoToken: String,
        ahora: Date
    ) -> Apertura {
        expirar(siCorresponde: ahora)
        if let sesion, sesion.expira > ahora,
           Self.igualesConstante(cookie, sesion.token) {
            return .existente(siguienteSecuencia: sesion.siguienteSecuencia)
        }
        guard codigoDisponible,
              Self.igualesConstante(presentado, codigo) else {
            return .rechazada
        }
        codigoDisponible = false
        sesion = Sesion(token: nuevoToken,
                        expira: ahora.addingTimeInterval(duracion))
        return .creada(token: nuevoToken, siguienteSecuencia: 1)
    }

    mutating func autorizarAPI(
        cookie: String?, secuencia: UInt64?, ahora: Date
    ) -> ResultadoAPI {
        let habiaSesion = sesion != nil
        expirar(siCorresponde: ahora)
        guard var actual = sesion else {
            return habiaSesion ? .expirada : .credencialInvalida
        }
        guard Self.igualesConstante(cookie, actual.token) else {
            return .credencialInvalida
        }
        guard let secuencia, secuencia == actual.siguienteSecuencia else {
            return .secuenciaInvalida
        }
        guard actual.siguienteSecuencia < UInt64.max else {
            return .secuenciaInvalida
        }
        actual.siguienteSecuencia += 1
        sesion = actual
        return .permitida
    }

    mutating func sesionActiva(ahora: Date) -> Bool {
        expirar(siCorresponde: ahora)
        return sesion != nil
    }

    static func igualesConstante(_ izquierda: String?, _ derecha: String) -> Bool {
        guard let izquierda else { return false }
        let a = Array(izquierda.utf8)
        let b = Array(derecha.utf8)
        var diferencia = UInt64(a.count ^ b.count)
        let maximo = max(a.count, b.count)
        for indice in 0..<maximo {
            diferencia |= UInt64((indice < a.count ? a[indice] : 0)
                ^ (indice < b.count ? b[indice] : 0))
        }
        return diferencia == 0
    }
}

struct CuotaSolicitudesControlRemoto {
    private var marcas: [TimeInterval] = []

    mutating func admitir(
        ahora: TimeInterval, maximo: Int, ventana: TimeInterval = 60
    ) -> Bool {
        marcas.removeAll { ahora - $0 >= ventana }
        guard marcas.count < maximo else { return false }
        marcas.append(ahora)
        return true
    }

    mutating func reiniciar() { marcas.removeAll(keepingCapacity: true) }
}

enum PoliticaImagenControlRemoto {
    static let maximoLado = 16_384
    static let maximoPixeles = 50_000_000

    static func dimensionesAdmitidas(ancho: Int, alto: Int) -> Bool {
        guard ancho > 0, alto > 0,
              ancho <= maximoLado, alto <= maximoLado else { return false }
        return ancho <= maximoPixeles / alto
    }
}

struct PeticionHTTPControlRemoto: Equatable {
    let metodo: String
    let ruta: String
    let consulta: [String: String]
    let cabeceras: [String: String]
    let cuerpo: Data
}

enum ErrorPeticionControlRemoto: Error, Equatable {
    case incompleta
    case cabeceraGrande
    case invalida
    case largoRequerido
    case demasiadoGrande

    var codigoHTTP: Int {
        switch self {
        case .incompleta, .invalida, .largoRequerido: return 400
        case .cabeceraGrande: return 431
        case .demasiadoGrande: return 413
        }
    }

    var mensaje: String {
        switch self {
        case .incompleta: return "Solicitud HTTP incompleta."
        case .cabeceraGrande: return "Cabeceras demasiado grandes."
        case .invalida: return "Solicitud HTTP inválida."
        case .largoRequerido: return "Se requiere Content-Length."
        case .demasiadoGrande: return "La solicitud supera 16 MB."
        }
    }
}

/// Parser deliberadamente pequeño para una sola petición HTTP/1.x por conexión.
/// No admite pipelining, chunked encoding ni métodos distintos de GET/POST.
/// El servidor siempre responde `Connection: close`.
struct ParserHTTPControlRemoto {
    let maximoCabeceras: Int
    let maximoCuerpo: Int

    func extraer(
        de datos: Data
    ) -> Result<PeticionHTTPControlRemoto, ErrorPeticionControlRemoto> {
        let separador = Data("\r\n\r\n".utf8)
        guard let rango = datos.range(of: separador) else {
            return datos.count > maximoCabeceras
                ? .failure(.cabeceraGrande) : .failure(.incompleta)
        }
        guard rango.lowerBound <= maximoCabeceras else {
            return .failure(.cabeceraGrande)
        }
        guard let texto = String(data: datos[..<rango.lowerBound],
                                 encoding: .utf8) else {
            return .failure(.invalida)
        }
        let lineas = texto.components(separatedBy: "\r\n")
        guard let inicial = lineas.first else { return .failure(.invalida) }
        let piezas = inicial.split(separator: " ", omittingEmptySubsequences: true)
        guard piezas.count == 3,
              piezas[2] == "HTTP/1.0" || piezas[2] == "HTTP/1.1" else {
            return .failure(.invalida)
        }
        let metodo = String(piezas[0]).uppercased()
        guard metodo == "GET" || metodo == "POST" else {
            return .failure(.invalida)
        }
        let objetivo = String(piezas[1])
        guard objetivo.hasPrefix("/"), objetivo.utf8.count <= 4_096,
              !objetivo.contains("#") else { return .failure(.invalida) }

        var cabeceras: [String: String] = [:]
        for linea in lineas.dropFirst() where !linea.isEmpty {
            guard let dosPuntos = linea.firstIndex(of: ":") else {
                return .failure(.invalida)
            }
            let nombre = linea[..<dosPuntos].trimmingCharacters(in: .whitespaces)
                .lowercased()
            let valor = linea[linea.index(after: dosPuntos)...]
                .trimmingCharacters(in: .whitespaces)
            guard Self.esNombreCabeceraValido(nombre),
                  Self.esValorCabeceraValido(valor),
                  cabeceras[nombre] == nil else {
                return .failure(.invalida)
            }
            cabeceras[nombre] = valor
        }
        if cabeceras["transfer-encoding"] != nil { return .failure(.invalida) }

        let necesitaCuerpo = metodo == "POST"
        guard !necesitaCuerpo || cabeceras["content-length"] != nil else {
            return .failure(.largoRequerido)
        }
        guard let largo = Int(cabeceras["content-length"] ?? "0"),
              largo >= 0 else { return .failure(.invalida) }
        guard largo <= maximoCuerpo else { return .failure(.demasiadoGrande) }
        if metodo == "GET", largo != 0 { return .failure(.invalida) }

        let inicioCuerpo = rango.upperBound
        guard datos.count >= inicioCuerpo + largo else { return .failure(.incompleta) }
        // Una conexión sirve una petición. Rechazar bytes sobrantes evita que
        // dos peticiones concatenadas tengan una interpretación ambigua.
        guard datos.count == inicioCuerpo + largo else { return .failure(.invalida) }

        guard let componentes = URLComponents(string: "http://cuadernu\(objetivo)"),
              componentes.host == "cuadernu" else {
            return .failure(.invalida)
        }
        var consulta: [String: String] = [:]
        for item in componentes.queryItems ?? [] {
            guard consulta[item.name] == nil else { return .failure(.invalida) }
            consulta[item.name] = item.value ?? ""
        }
        let cuerpo = datos.subdata(in: inicioCuerpo..<(inicioCuerpo + largo))
        return .success(PeticionHTTPControlRemoto(
            metodo: metodo, ruta: componentes.path, consulta: consulta,
            cabeceras: cabeceras, cuerpo: cuerpo))
    }

    private static func esNombreCabeceraValido(_ nombre: String) -> Bool {
        guard !nombre.isEmpty else { return false }
        let extras = Set("!#$%&'*+-.^_`|~".utf8)
        return nombre.utf8.allSatisfy { byte in
            (byte >= 48 && byte <= 57)
                || (byte >= 65 && byte <= 90)
                || (byte >= 97 && byte <= 122)
                || extras.contains(byte)
        }
    }

    private static func esValorCabeceraValido(_ valor: String) -> Bool {
        valor.utf8.allSatisfy { $0 == 9 || $0 >= 32 && $0 != 127 }
    }
}

enum ConstructorRespuestaHTTPControlRemoto {
    static func datos(
        codigo: Int, tipo: String, cuerpo: Data,
        cabecerasAdicionales: [String: String] = [:]
    ) -> Data {
        var lineas = [
            "HTTP/1.1 \(codigo) \(razon(codigo))",
            "Content-Type: \(tipo)",
            "Content-Length: \(cuerpo.count)",
            "Cache-Control: no-store",
            "X-Content-Type-Options: nosniff",
            "X-Frame-Options: DENY",
            "Cross-Origin-Resource-Policy: same-origin",
            "Referrer-Policy: no-referrer",
            "Content-Security-Policy: default-src 'self'; img-src 'self' data: blob:; style-src 'unsafe-inline'; script-src 'unsafe-inline'",
            "Permissions-Policy: camera=(self)",
        ]
        for nombre in cabecerasAdicionales.keys.sorted() {
            guard let valor = cabecerasAdicionales[nombre],
                  cabeceraSegura(nombre), cabeceraSegura(valor) else { continue }
            lineas.append("\(nombre): \(valor)")
        }
        lineas.append("Connection: close")
        var salida = Data((lineas.joined(separator: "\r\n") + "\r\n\r\n").utf8)
        salida.append(cuerpo)
        return salida
    }

    private static func razon(_ codigo: Int) -> String {
        switch codigo {
        case 200: return "OK"
        case 201: return "Created"
        case 400: return "Bad Request"
        case 401: return "Unauthorized"
        case 403: return "Forbidden"
        case 404: return "Not Found"
        case 408: return "Request Timeout"
        case 409: return "Conflict"
        case 413: return "Payload Too Large"
        case 415: return "Unsupported Media Type"
        case 429: return "Too Many Requests"
        case 431: return "Request Header Fields Too Large"
        case 503: return "Service Unavailable"
        default: return "Internal Server Error"
        }
    }

    private static func cabeceraSegura(_ valor: String) -> Bool {
        !valor.contains("\r") && !valor.contains("\n")
    }
}
