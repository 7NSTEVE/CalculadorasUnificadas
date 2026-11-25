import CoreImage
import Darwin
import Foundation
import ImageIO
import Network
import Security
import UIKit
import UniformTypeIdentifiers

extension Notification.Name {
    static let estadoControlRemotoWebIPad = Notification.Name(
        "Tinta.estadoControlRemotoWebIPad")
}

/// Puente local para que un iPhone o Android funcione como cámara/paleta sin
/// instalar otra app. El iPad sirve una página autocontenida por la LAN; no hay
/// CDN, analítica, cuenta ni salida a Internet.
///
/// HTTP local se usa deliberadamente porque un certificado autofirmado rompe
/// el flujo de cámara en navegadores móviles. El QR lleva un código aleatorio de
/// 192 bits consumible una vez; luego se usa una cookie HttpOnly de dos horas y
/// una secuencia antirrepetición. También se limitan conexiones, tiempo, tasa,
/// cuerpo (16 MB) y geometría (50 MP). Esto autentica la sesión, pero HTTP no
/// oculta el tráfico a quien pueda capturar paquetes de esa red local.
final class ServicioControlRemotoWebIPad {
    static let compartido = ServicioControlRemotoWebIPad()

    enum Estado: Equatable {
        case detenido
        case iniciando
        case listo
        case fallo(String)
    }

    private let cola = DispatchQueue(label: "cr.tinta.control-web",
                                     qos: .userInitiated)
    private let bloqueo = NSLock()
    private var listener: NWListener?
    private var puertoInterno: NWEndpoint.Port?
    private var autorizacion = EstadoAutorizacionWebIPad(
        codigo: ServicioControlRemotoWebIPad.generarToken())
    private var estadoInterno: Estado = .detenido
    private var conexionesActivas: [ObjectIdentifier: NWConnection] = [:]
    private var cuotaIntentos = CuotaSolicitudesControlRemoto()
    private var cuotaAutorizados = CuotaSolicitudesControlRemoto()
    private var cuotaCapturas = CuotaSolicitudesControlRemoto()

    private let maximoCabeceras = 64 * 1024
    private let maximoImagen = 16 * 1024 * 1024
    private let maximoConexiones = 4
    private let timeoutConexion: TimeInterval = 30
    private lazy var parserHTTP = ParserHTTPControlRemoto(
        maximoCabeceras: maximoCabeceras, maximoCuerpo: maximoImagen)

    private init() {}

    var estado: Estado {
        bloqueo.lock(); defer { bloqueo.unlock() }
        return estadoInterno
    }

    var puerto: NWEndpoint.Port? {
        bloqueo.lock(); defer { bloqueo.unlock() }
        return puertoInterno
    }

    var sesionWebActiva: Bool {
        bloqueo.lock(); defer { bloqueo.unlock() }
        return autorizacion.sesionActiva(ahora: Date())
    }

    var codigoWebDisponible: Bool {
        bloqueo.lock(); defer { bloqueo.unlock() }
        return autorizacion.codigoActual != nil
    }

    func iniciar() {
        bloqueo.lock()
        if listener != nil || estadoInterno == .iniciando {
            bloqueo.unlock()
            notificarEstado()
            return
        }
        autorizacion.renovar(codigo: Self.generarToken())
        cuotaIntentos.reiniciar()
        cuotaAutorizados.reiniciar()
        cuotaCapturas.reiniciar()
        estadoInterno = .iniciando
        bloqueo.unlock()
        notificarEstado()

        do {
            let parametros = NWParameters.tcp
            parametros.allowLocalEndpointReuse = true
            let nuevo = try NWListener(using: parametros, on: .any)
            nuevo.service = NWListener.Service(name: "CuadernU iPad",
                                               type: "_tinta-web._tcp")
            nuevo.newConnectionHandler = { [weak self, weak nuevo] conexion in
                guard let self, let nuevo else {
                    conexion.cancel()
                    return
                }
                self.aceptar(conexion, desde: nuevo)
            }
            nuevo.stateUpdateHandler = { [weak self, weak nuevo] estado in
                guard let self, let nuevo else { return }
                self.procesarEstado(estado, de: nuevo)
            }
            bloqueo.lock()
            guard listener == nil, estadoInterno == .iniciando else {
                bloqueo.unlock()
                nuevo.cancel()
                return
            }
            listener = nuevo
            bloqueo.unlock()
            nuevo.start(queue: cola)
        } catch {
            actualizar(estado: .fallo(error.localizedDescription), puerto: nil)
        }
    }

    func detener() {
        bloqueo.lock()
        let actual = listener
        let conexiones = Array(conexionesActivas.values)
        listener = nil
        conexionesActivas.removeAll()
        puertoInterno = nil
        estadoInterno = .detenido
        autorizacion.invalidar()
        bloqueo.unlock()
        actual?.cancel()
        conexiones.forEach { $0.cancel() }
        notificarEstado()
    }

    /// Invalida inmediatamente todo QR anterior. El listener no se reinicia y
    /// las conexiones que ya hayan enviado una petición no reciben privilegios
    /// adicionales.
    func renovarCodigo() {
        bloqueo.lock()
        let conexiones = Array(conexionesActivas.values)
        conexionesActivas.removeAll()
        autorizacion.renovar(codigo: Self.generarToken())
        cuotaIntentos.reiniciar()
        cuotaAutorizados.reiniciar()
        cuotaCapturas.reiniciar()
        bloqueo.unlock()
        conexiones.forEach { $0.cancel() }
        notificarEstado()
    }

    func urlsDeAcceso() -> [URL] {
        bloqueo.lock()
        let acceso = autorizacion.codigoActual.flatMap { codigo in
            puertoInterno.map { (codigo, $0) }
        }
        bloqueo.unlock()
        guard let (codigo, puerto) = acceso else { return [] }
        return Self.direccionesIPv4Locales().compactMap {
            URL(string: "http://\($0):\(puerto.rawValue)/?token=\(codigo)")
        }
    }

    // MARK: - Conexiones

    private func aceptar(_ conexion: NWConnection, desde origen: NWListener) {
        let identificador = ObjectIdentifier(conexion)
        bloqueo.lock()
        let admitida = listener === origen
            && estadoInterno == .listo
            && conexionesActivas.count < maximoConexiones
        if admitida { conexionesActivas[identificador] = conexion }
        bloqueo.unlock()

        conexion.stateUpdateHandler = { [weak self, weak conexion] estado in
            guard let self, let conexion else { return }
            if case .failed = estado { self.olvidar(conexion) }
            if case .cancelled = estado { self.olvidar(conexion) }
        }
        conexion.start(queue: cola)
        guard admitida else {
            responder(conexion, codigo: 503, tipo: "text/plain; charset=utf-8",
                       cuerpo: Data("Servidor no disponible o saturado.".utf8),
                       requiereConexionActiva: false)
            return
        }

        var acumulado = Data()
        cola.asyncAfter(deadline: .now() + timeoutConexion) { [weak self, weak conexion] in
            guard let self, let conexion, self.sigueActiva(conexion) else { return }
            self.responder(conexion, codigo: 408, tipo: "text/plain; charset=utf-8",
                           cuerpo: Data("La conexión agotó su tiempo.".utf8))
        }

        func recibir() {
            conexion.receive(minimumIncompleteLength: 1,
                             maximumLength: 64 * 1024) { [weak self] datos, _, completo, error in
                guard let self, self.sigueActiva(conexion) else { return }
                if let datos { acumulado.append(datos) }
                if acumulado.count > self.maximoCabeceras + 4 + self.maximoImagen {
                    self.responder(conexion, codigo: 413, tipo: "text/plain; charset=utf-8",
                                   cuerpo: Data("La solicitud supera 16 MB.".utf8))
                    return
                }

                switch self.parserHTTP.extraer(de: acumulado) {
                case .success(let peticion):
                    self.procesar(peticion, conexion: conexion)
                case .failure(let errorPeticion):
                    if errorPeticion == .incompleta, error == nil, !completo {
                        recibir()
                    } else {
                        self.responder(conexion, codigo: errorPeticion.codigoHTTP,
                                       tipo: "text/plain; charset=utf-8",
                                       cuerpo: Data(errorPeticion.mensaje.utf8))
                    }
                }
            }
        }
        recibir()
    }

    private func sigueActiva(_ conexion: NWConnection) -> Bool {
        bloqueo.lock(); defer { bloqueo.unlock() }
        return conexionesActivas[ObjectIdentifier(conexion)] != nil
    }

    private func olvidar(_ conexion: NWConnection) {
        bloqueo.lock()
        conexionesActivas.removeValue(forKey: ObjectIdentifier(conexion))
        bloqueo.unlock()
    }

    private func reclamarRespuesta(_ conexion: NWConnection) -> Bool {
        bloqueo.lock(); defer { bloqueo.unlock() }
        return conexionesActivas.removeValue(
            forKey: ObjectIdentifier(conexion)) != nil
    }

    private func procesar(_ peticion: PeticionHTTPControlRemoto,
                          conexion: NWConnection) {
        let ahora = Date()
        bloqueo.lock()
        let bajoCuota = cuotaIntentos.admitir(
            ahora: ahora.timeIntervalSinceReferenceDate, maximo: 240)
        bloqueo.unlock()
        guard bajoCuota else {
            responder(conexion, codigo: 429, tipo: "text/plain; charset=utf-8",
                       cuerpo: Data("Demasiadas solicitudes; espere un minuto.".utf8))
            return
        }

        if peticion.metodo == "GET", peticion.ruta == "/" {
            let cookie = Self.cookieSesion(de: peticion.cabeceras)
            let nuevoToken = Self.generarToken()
            bloqueo.lock()
            let apertura = autorizacion.abrir(
                codigo: peticion.consulta["token"], cookie: cookie,
                nuevoToken: nuevoToken, ahora: ahora)
            let duracion = autorizacion.duracion
            bloqueo.unlock()
            switch apertura {
            case .rechazada:
                responder(conexion, codigo: 401, tipo: "text/plain; charset=utf-8",
                           cuerpo: Data("Código vencido, consumido o incorrecto.".utf8))
            case .existente(let siguienteSecuencia):
                responder(conexion, codigo: 200, tipo: "text/html; charset=utf-8",
                           cuerpo: Data(paginaHTML(
                            secuenciaInicial: siguienteSecuencia).utf8))
            case .creada(let token, let siguienteSecuencia):
                let segundos = Int(duracion)
                responder(
                    conexion, codigo: 200, tipo: "text/html; charset=utf-8",
                    cuerpo: Data(paginaHTML(
                        secuenciaInicial: siguienteSecuencia).utf8),
                    cabecerasAdicionales: [
                        "Set-Cookie": "CuadernUSesion=\(token); Path=/; Max-Age=\(segundos); HttpOnly; SameSite=Strict",
                    ])
                notificarEstado()
                cola.asyncAfter(deadline: .now() + duracion) {
                    [weak self] in self?.expirarSesionWeb()
                }
            }
            return
        }

        if let sitio = peticion.cabeceras["sec-fetch-site"], sitio != "same-origin" {
            responder(conexion, codigo: 403, tipo: "text/plain; charset=utf-8",
                       cuerpo: Data("La solicitud no proviene de esta página local.".utf8))
            return
        }
        let cookie = Self.cookieSesion(de: peticion.cabeceras)
        let secuencia = peticion.consulta["seq"].flatMap(UInt64.init)
        bloqueo.lock()
        let autorizada = autorizacion.autorizarAPI(
            cookie: cookie, secuencia: secuencia, ahora: ahora)
        let bajoCuotaAutorizada = autorizada == .permitida
            ? cuotaAutorizados.admitir(
                ahora: ahora.timeIntervalSinceReferenceDate, maximo: 120)
            : true
        bloqueo.unlock()
        guard autorizada == .permitida else {
            let codigo = autorizada == .secuenciaInvalida ? 409 : 401
            responder(conexion, codigo: codigo, tipo: "text/plain; charset=utf-8",
                       cuerpo: Data("Sesión vencida o secuencia ya utilizada.".utf8))
            return
        }
        guard bajoCuotaAutorizada else {
            responder(conexion, codigo: 429, tipo: "text/plain; charset=utf-8",
                       cuerpo: Data("Límite temporal de la sesión alcanzado.".utf8))
            return
        }

        if peticion.metodo == "GET", peticion.ruta == "/api/status" {
            responderJSON(conexion, codigo: 200,
                          objeto: ["estado": "listo", "local": true])
            return
        }
        if peticion.metodo == "POST", peticion.ruta == "/api/command" {
            guard let valor = peticion.consulta["command"],
                  let comando = ComandoRemoto(rawValue: valor) else {
                responderJSON(conexion, codigo: 400,
                              objeto: ["ok": false, "error": "Comando desconocido"])
                return
            }
            DispatchQueue.main.async {
                NotificationCenter.default.post(name: .comandoRemotoRecibido,
                                                object: comando)
            }
            responderJSON(conexion, codigo: 200, objeto: ["ok": true])
            return
        }
        if peticion.metodo == "POST", peticion.ruta == "/api/capture" {
            bloqueo.lock()
            let capturaAdmitida = cuotaCapturas.admitir(
                ahora: ahora.timeIntervalSinceReferenceDate, maximo: 12)
            bloqueo.unlock()
            guard capturaAdmitida else {
                responderJSON(conexion, codigo: 429,
                              objeto: ["ok": false,
                                       "error": "Máximo de 12 capturas por minuto"])
                return
            }
            recibirImagen(peticion, conexion: conexion)
            return
        }
        responder(conexion, codigo: 404, tipo: "text/plain; charset=utf-8",
                   cuerpo: Data("Ruta inexistente.".utf8))
    }

    private func expirarSesionWeb() {
        bloqueo.lock()
        autorizacion.expirar(siCorresponde: Date())
        bloqueo.unlock()
        notificarEstado()
    }

    private static func cookieSesion(de cabeceras: [String: String]) -> String? {
        guard let cookies = cabeceras["cookie"] else { return nil }
        for fragmento in cookies.split(separator: ";") {
            let partes = fragmento.trimmingCharacters(in: .whitespaces)
                .split(separator: "=", maxSplits: 1,
                       omittingEmptySubsequences: false)
            if partes.count == 2, partes[0] == "CuadernUSesion" {
                return String(partes[1])
            }
        }
        return nil
    }

    private func recibirImagen(_ peticion: PeticionHTTPControlRemoto,
                               conexion: NWConnection) {
        guard let valor = peticion.consulta["type"],
              let tipo = TipoCapturaRemota(rawValue: valor) else {
            responderJSON(conexion, codigo: 400,
                          objeto: ["ok": false, "error": "Tipo de captura inválido"])
            return
        }
        let mime = peticion.cabeceras["content-type"]?.lowercased() ?? ""
        guard mime.hasPrefix("image/"), !peticion.cuerpo.isEmpty,
              let fuente = CGImageSourceCreateWithData(
                peticion.cuerpo as CFData, nil),
              let propiedades = CGImageSourceCopyPropertiesAtIndex(
                fuente, 0, nil) as? [CFString: Any],
              let ancho = (propiedades[kCGImagePropertyPixelWidth] as? NSNumber)?.intValue,
              let alto = (propiedades[kCGImagePropertyPixelHeight] as? NSNumber)?.intValue,
              PoliticaImagenControlRemoto.dimensionesAdmitidas(
                ancho: ancho, alto: alto) else {
            responderJSON(conexion, codigo: 415,
                           objeto: ["ok": false,
                                    "error": "Imagen inválida o superior a 50 MP"])
            return
        }

        let extensionArchivo = CGImageSourceGetType(fuente)
            .flatMap { UTType($0 as String)?.preferredFilenameExtension }
            ?? "jpg"
        let nombre = "\(tipo.rawValue)-web-\(UUID().uuidString).\(extensionArchivo)"
        let temporal = FileManager.default.temporaryDirectory
            .appendingPathComponent(nombre)
        do {
            try peticion.cuerpo.write(to: temporal, options: .atomic)
            defer { try? FileManager.default.removeItem(at: temporal) }
            let adjunto = try RepositorioAdjuntos.compartido.importar(url: temporal)
            let metadatos = MetadatosCapturaRemota(
                tipo: tipo, nombre: nombre,
                corregirPizarra: tipo == .pizarra || tipo == .diapositiva)
            DispatchQueue.main.async {
                NotificationCenter.default.post(
                    name: .capturaRemotaRecibida,
                    object: CapturaRemotaRecibida(metadatos: metadatos,
                                                  adjunto: adjunto))
            }
            responderJSON(conexion, codigo: 201,
                          objeto: ["ok": true, "nombre": nombre])
        } catch {
            responderJSON(conexion, codigo: 500,
                          objeto: ["ok": false,
                                   "error": "No se pudo guardar la captura"])
        }
    }

    private func responderJSON(_ conexion: NWConnection, codigo: Int,
                               objeto: [String: Any]) {
        let cuerpo = (try? JSONSerialization.data(withJSONObject: objeto,
                                                   options: [.sortedKeys])) ?? Data("{}".utf8)
        responder(conexion, codigo: codigo,
                   tipo: "application/json; charset=utf-8", cuerpo: cuerpo)
    }

    private func responder(_ conexion: NWConnection, codigo: Int,
                            tipo: String, cuerpo: Data,
                            cabecerasAdicionales: [String: String] = [:],
                            requiereConexionActiva: Bool = true) {
        if requiereConexionActiva, !reclamarRespuesta(conexion) { return }
        let salida = ConstructorRespuestaHTTPControlRemoto.datos(
            codigo: codigo, tipo: tipo, cuerpo: cuerpo,
            cabecerasAdicionales: cabecerasAdicionales)
        conexion.send(content: salida, completion: .contentProcessed { _ in
            conexion.cancel()
        })
    }

    // MARK: - Estado y direcciones

    private func actualizar(estado: Estado, puerto: NWEndpoint.Port?) {
        bloqueo.lock()
        estadoInterno = estado
        puertoInterno = puerto
        bloqueo.unlock()
        notificarEstado()
    }

    /// Ignora callbacks tardíos de listeners ya detenidos o reemplazados. Sin
    /// esta comprobación, un `.cancelled` viejo podía marcar como detenido un
    /// listener nuevo que ya estaba listo.
    private func procesarEstado(_ estadoRed: NWListener.State,
                                de origen: NWListener) {
        var conexiones: [NWConnection] = []
        var cancelarOrigen = false
        bloqueo.lock()
        guard listener === origen else {
            bloqueo.unlock()
            return
        }
        switch estadoRed {
        case .ready:
            estadoInterno = .listo
            puertoInterno = origen.port
        case .failed(let error):
            estadoInterno = .fallo(error.localizedDescription)
            puertoInterno = nil
            listener = nil
            autorizacion.invalidar()
            conexiones = Array(conexionesActivas.values)
            conexionesActivas.removeAll()
            cancelarOrigen = true
        case .cancelled:
            estadoInterno = .detenido
            puertoInterno = nil
            listener = nil
            autorizacion.invalidar()
            conexiones = Array(conexionesActivas.values)
            conexionesActivas.removeAll()
        default:
            bloqueo.unlock()
            return
        }
        bloqueo.unlock()
        if cancelarOrigen { origen.cancel() }
        conexiones.forEach { $0.cancel() }
        notificarEstado()
    }

    private func notificarEstado() {
        DispatchQueue.main.async {
            NotificationCenter.default.post(name: .estadoControlRemotoWebIPad,
                                            object: self)
        }
    }

    private static func generarToken() -> String {
        var bytes = [UInt8](repeating: 0, count: 24)
        let resultado = bytes.withUnsafeMutableBytes { memoria -> OSStatus in
            guard let base = memoria.baseAddress else { return errSecParam }
            return SecRandomCopyBytes(kSecRandomDefault, memoria.count, base)
        }
        if resultado == errSecSuccess {
            return Data(bytes).base64EncodedString()
                .replacingOccurrences(of: "+", with: "-")
                .replacingOccurrences(of: "/", with: "_")
                .replacingOccurrences(of: "=", with: "")
        }
        return UUID().uuidString.replacingOccurrences(of: "-", with: "")
            + UUID().uuidString.replacingOccurrences(of: "-", with: "")
    }

    private static func direccionesIPv4Locales() -> [String] {
        var inicio: UnsafeMutablePointer<ifaddrs>?
        guard getifaddrs(&inicio) == 0, let inicio else { return [] }
        defer { freeifaddrs(inicio) }

        var resultados: [(interfaz: String, direccion: String)] = []
        var cursor: UnsafeMutablePointer<ifaddrs>? = inicio
        while let actual = cursor {
            defer { cursor = actual.pointee.ifa_next }
            guard let direccion = actual.pointee.ifa_addr,
                  Int32(direccion.pointee.sa_family) == AF_INET else { continue }
            let banderas = Int32(actual.pointee.ifa_flags)
            guard banderas & IFF_UP != 0, banderas & IFF_LOOPBACK == 0 else { continue }

            var host = [CChar](repeating: 0, count: Int(NI_MAXHOST))
            let resultado = getnameinfo(
                direccion, socklen_t(direccion.pointee.sa_len),
                &host, socklen_t(host.count), nil, 0, NI_NUMERICHOST)
            guard resultado == 0 else { continue }
            resultados.append((String(cString: actual.pointee.ifa_name),
                               String(cString: host)))
        }
        let prioridad: [String: Int] = ["en0": 0, "en1": 1, "bridge100": 2]
        return resultados.sorted {
            (prioridad[$0.interfaz] ?? 50, $0.interfaz)
                < (prioridad[$1.interfaz] ?? 50, $1.interfaz)
        }.map(\.direccion).uniques()
    }

    private func paginaHTML(secuenciaInicial: UInt64) -> String {
        let secuenciaAnterior = secuenciaInicial > 0 ? secuenciaInicial - 1 : 0
        return """
        <!doctype html>
        <html lang="es"><head>
        <meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1,viewport-fit=cover">
        <title>Control remoto de CuadernU</title>
        <style>
        :root{color-scheme:light dark;font-family:system-ui,BlinkMacSystemFont,"Segoe UI",sans-serif}
        body{margin:0;padding:max(22px,env(safe-area-inset-top)) 18px 30px;background:#10131a;color:#f7f8fb}
        main{max-width:620px;margin:auto}.card{background:#1c2230;border:1px solid #343d50;border-radius:18px;padding:18px;margin:14px 0}
        h1{font-size:1.55rem;margin:.2rem 0}.sub{color:#b8c1d1;margin:.4rem 0 1rem}
        button,label{display:flex;box-sizing:border-box;width:100%;min-height:52px;align-items:center;justify-content:center;border:0;border-radius:13px;margin:10px 0;padding:12px;font-size:1.05rem;font-weight:650;background:#3178f6;color:white}
        button.secondary,label.secondary{background:#30394b}button:disabled{opacity:.55}select{width:100%;font-size:1rem;padding:12px;border-radius:10px;background:#0f1420;color:white}
        input{position:absolute;left:-9999px}.grid{display:grid;grid-template-columns:1fr 1fr;gap:10px}.grid button{margin:0}
        #estado{min-height:1.4em;color:#8ed89d;font-weight:600}.error{color:#ff9b9b!important}
        </style></head><body><main>
        <h1>CuadernU · control de clase</h1><p class="sub">La foto viaja directamente a este iPad. La sesión dura hasta dos horas; evite redes Wi-Fi públicas.</p>
        <section class="card"><select id="tipo" aria-label="Tipo de foto"><option value="pizarra">Pizarra (corregir perspectiva)</option><option value="diapositiva">Diapositiva / proyector</option><option value="documento">Documento</option><option value="fotoLibre">Foto libre</option></select>
        <label for="foto">Abrir cámara y enviar</label><input id="foto" type="file" accept="image/*" capture="environment"><div id="estado" role="status">Listo.</div></section>
        <section class="card"><div class="grid"><button data-cmd="mePerdi">Me perdí</button><button data-cmd="importante">Importante</button><button data-cmd="examen">Examen</button><button data-cmd="deshacer" class="secondary">Deshacer</button><button data-cmd="anteriorSlide" class="secondary">Slide anterior</button><button data-cmd="siguienteSlide" class="secondary">Siguiente slide</button></div></section>
        </main><script>
        history.replaceState(null,'','/');const estado=document.getElementById('estado'),foto=document.getElementById('foto');let secuencia=\(secuenciaAnterior),cola=Promise.resolve();
        function mensaje(t,error=false){estado.textContent=t;estado.className=error?'error':''}
        function enviar(url,opciones){const trabajo=cola.catch(()=>{}).then(async()=>{const n=++secuencia,r=await fetch(url+(url.includes('?')?'&':'?')+'seq='+n,opciones);let j={};try{j=await r.json()}catch{}if(!r.ok)throw new Error(j.error||('HTTP '+r.status));return j});cola=trabajo;return trabajo}
        foto.addEventListener('change',async()=>{const f=foto.files[0];if(!f)return;mensaje('Enviando '+Math.round(f.size/1024)+' KB…');foto.disabled=true;try{await enviar('/api/capture?type='+encodeURIComponent(document.getElementById('tipo').value),{method:'POST',headers:{'Content-Type':f.type||'image/jpeg'},body:f});mensaje('Foto recibida por el iPad. Puede tomar otra.')}catch(e){mensaje(e.message,true)}finally{foto.disabled=false;foto.value=''}});
        document.querySelectorAll('[data-cmd]').forEach(b=>b.addEventListener('click',async()=>{b.disabled=true;try{await enviar('/api/command?command='+encodeURIComponent(b.dataset.cmd),{method:'POST',body:''});mensaje('Comando enviado: '+b.textContent)}catch(e){mensaje(e.message,true)}finally{b.disabled=false}}));
        </script></body></html>
        """
    }
}

private extension Array where Element: Hashable {
    func uniques() -> [Element] {
        var vistos = Set<Element>()
        return filter { vistos.insert($0).inserted }
    }
}

/// Panel de emparejamiento: muestra la URL y su QR, permite copiarla, renovar
/// el token o apagar el listener al terminar la clase.
final class PanelControlRemotoWebIPad: UIViewController {
    private let servicio = ServicioControlRemotoWebIPad.compartido
    private let estado = UILabel()
    private let qr = UIImageView()
    private let direccion = UILabel()
    private let iniciar = UIButton(type: .system)

    override func viewDidLoad() {
        super.viewDidLoad()
        title = "Teléfono como control"
        view.backgroundColor = .systemGroupedBackground
        navigationItem.leftBarButtonItem = UIBarButtonItem(
            barButtonSystemItem: .close, target: self, action: #selector(cerrar))

        estado.numberOfLines = 0
        estado.textAlignment = .center
        estado.font = .preferredFont(forTextStyle: .headline)
        qr.contentMode = .scaleAspectFit
        qr.backgroundColor = .white
        qr.layer.cornerRadius = 12
        qr.clipsToBounds = true
        qr.heightAnchor.constraint(equalTo: qr.widthAnchor).isActive = true
        direccion.numberOfLines = 0
        direccion.textAlignment = .center
        direccion.font = .monospacedSystemFont(ofSize: 12, weight: .regular)
        direccion.textColor = .secondaryLabel

        let copiar = boton("Copiar enlace", simbolo: "doc.on.doc",
                           accion: #selector(copiarEnlace))
        let renovar = boton("Renovar código", simbolo: "arrow.clockwise",
                            accion: #selector(renovarCodigo))
        iniciar.configuration = .bordered()
        iniciar.addTarget(self, action: #selector(alternarServicio),
                          for: .touchUpInside)

        let ayuda = UILabel()
        ayuda.numberOfLines = 0
        ayuda.font = .preferredFont(forTextStyle: .footnote)
        ayuda.textColor = .secondaryLabel
        ayuda.text = "Abra el QR desde un teléfono en la misma red. El código se consume al primer uso y la sesión vence en dos horas. No usa Internet ni TLS: evite redes públicas y renueve el código al cambiar de clase."

        let pila = UIStackView(arrangedSubviews: [estado, qr, direccion,
                                                   copiar, renovar, iniciar, ayuda])
        pila.axis = .vertical
        pila.spacing = 14
        pila.translatesAutoresizingMaskIntoConstraints = false
        view.addSubview(pila)
        NSLayoutConstraint.activate([
            pila.topAnchor.constraint(equalTo: view.safeAreaLayoutGuide.topAnchor,
                                      constant: 20),
            pila.centerXAnchor.constraint(equalTo: view.centerXAnchor),
            pila.widthAnchor.constraint(lessThanOrEqualToConstant: 420),
            pila.leadingAnchor.constraint(greaterThanOrEqualTo: view.leadingAnchor,
                                          constant: 24),
            pila.trailingAnchor.constraint(lessThanOrEqualTo: view.trailingAnchor,
                                           constant: -24),
        ])
        NotificationCenter.default.addObserver(
            self, selector: #selector(actualizar),
            name: .estadoControlRemotoWebIPad, object: servicio)
        // Abrir este panel es el consentimiento explícito para anunciar solo el
        // servidor web autenticado. Multipeer no se inicia desde este flujo:
        // aceptar pares automáticamente era una superficie distinta e insegura.
        servicio.iniciar()
        actualizar()
    }

    private func boton(_ titulo: String, simbolo: String,
                       accion: Selector) -> UIButton {
        let boton = UIButton(type: .system)
        boton.configuration = .borderedProminent()
        boton.configuration?.title = titulo
        boton.configuration?.image = UIImage(systemName: simbolo)
        boton.configuration?.imagePadding = 8
        boton.addTarget(self, action: accion, for: .touchUpInside)
        return boton
    }

    @objc private func actualizar() {
        let urls = servicio.urlsDeAcceso()
        switch servicio.estado {
        case .detenido:
            estado.text = "Control web apagado"
            iniciar.configuration?.title = "Iniciar control web"
        case .iniciando:
            estado.text = "Preparando la red local…"
            iniciar.configuration?.title = "Detener"
        case .listo:
            if servicio.sesionWebActiva {
                estado.text = "Teléfono conectado · el QR ya no se puede reutilizar"
            } else if !servicio.codigoWebDisponible {
                estado.text = "Sesión vencida · pulse Renovar código"
            } else {
                estado.text = urls.isEmpty
                    ? "Servidor listo; conecte el iPad a Wi-Fi para obtener una dirección."
                    : "Escanee el QR con el teléfono"
            }
            iniciar.configuration?.title = "Detener al terminar"
        case .fallo(let mensaje):
            estado.text = "No se pudo iniciar: \(mensaje)"
            iniciar.configuration?.title = "Reintentar"
        }
        direccion.text = servicio.sesionWebActiva
            ? "Sesión activa. Pulse Renovar código para expulsarla y crear otro QR."
            : urls.map(\.absoluteString).joined(separator: "\n")
        qr.image = urls.first.flatMap(Self.codigoQR)
        qr.isHidden = qr.image == nil
    }

    @objc private func copiarEnlace() {
        guard let url = servicio.urlsDeAcceso().first else { return }
        UIPasteboard.general.setItems(
            [[UTType.url.identifier: url as NSURL]],
            options: [
                .localOnly: true,
                .expirationDate: Date().addingTimeInterval(10 * 60),
            ])
        UIAccessibility.post(notification: .announcement,
                             argument: "Enlace local copiado por diez minutos")
    }

    @objc private func renovarCodigo() {
        servicio.renovarCodigo()
        actualizar()
    }

    @objc private func alternarServicio() {
        switch servicio.estado {
        case .detenido, .fallo:
            servicio.iniciar()
        case .iniciando, .listo:
            servicio.detener()
        }
    }

    @objc private func cerrar() { dismiss(animated: true) }

    private static func codigoQR(_ url: URL) -> UIImage? {
        guard let filtro = CIFilter(name: "CIQRCodeGenerator") else { return nil }
        filtro.setValue(Data(url.absoluteString.utf8), forKey: "inputMessage")
        filtro.setValue("M", forKey: "inputCorrectionLevel")
        guard let salida = filtro.outputImage?.transformed(
            by: CGAffineTransform(scaleX: 10, y: 10)) else { return nil }
        let contexto = CIContext(options: [.useSoftwareRenderer: false])
        guard let cg = contexto.createCGImage(salida, from: salida.extent) else { return nil }
        return UIImage(cgImage: cg)
    }
}
