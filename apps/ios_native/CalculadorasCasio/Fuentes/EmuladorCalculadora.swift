import Combine
import Foundation
import QuartzCore

/// Envoltorio del nucleo en C, con el reloj que lo hace avanzar.
///
/// El emulador no tiene hilo propio a proposito: aqui se le da cuerda desde un
/// CADisplayLink, sincronizado con el refresco de la pantalla. Asi no hay estado
/// compartido entre hilos que proteger.
final class EmuladorCalculadora: ObservableObject {

    /// Matriz de puntos, un byte por pixel (0 o 1), fila a fila.
    @Published private(set) var pixeles: [UInt8] = []
    /// Fila 0 del buffer: los indicadores (S, A, M, STO, DEG, Math...).
    @Published private(set) var indicadores: [UInt8] = []

    /// Lo que pone en pantalla, ya decodificado a texto. Permite que la cinta de
    /// calculo registre numeros de verdad en vez de solo la secuencia de teclas.
    @Published private(set) var lineasPantalla: [String] = []

    private(set) var ancho: Int = 0
    private(set) var alto: Int = 0

    /// Tecla que el usuario tiene el dedo encima, para dibujarla hundida.
    @Published private(set) var teclaPulsada: UInt8?

    private var emulador: OpaquePointer?
    private var enlace: CADisplayLink?
    private var instanteAnterior: CFTimeInterval = 0

    /// Instante en que se pulso la tecla actual, en tiempo emulado acumulado.
    private var msDesdePulsacion: Double = 0
    private var sueltaPendiente = false

    /// Teclas pulsadas desde el ultimo encendido, para poder rebobinar.
    private(set) var historial: [UInt8] = []

    /// Estado completo de la maquina justo ANTES de cada tecla del historial.
    ///
    /// Antes, deshacer reiniciaba y volvia a teclear la secuencia menos la ultima:
    /// exacto por construccion, pero ~20 ms por tecla, o sea segundos en una sesion
    /// larga. Con el volcado del chipset se restaura en decimas de milisegundo
    /// (medido: 0,265 ms frente a 829 ms para volver 50 teclas atras).
    ///
    /// `instantaneas[i]` es el estado previo a `historial[i]`, asi que deshacer una
    /// tecla es cargar `instantaneas[historial.count - 1]`.
    private var instantaneas: [Data] = []

    /// Tope de estados guardados. Cada uno ocupa ~5 KB en la VN PLUS y ~11 KB en la
    /// LA X, asi que 400 son unos 4,4 MB en el peor caso: nada para un iPad, y cubre
    /// de sobra un ejercicio entero. Pasado el tope se tira el mas viejo y ese punto
    /// vuelve a necesitar reproduccion.
    private static let maximoInstantaneas = 400

    /// Cuantas teclas del principio ya no tienen estado guardado. Sirve para saber si
    /// se puede saltar atras al instante o toca reproducir.
    private var teclasSinInstantanea = 0

    /// Puntos de retorno automaticos, con la posicion del historial a la que van.
    ///
    /// Antes esto era inviable: guardar cada 20 segundos habria costado segundos de
    /// congelacion cada vez. Con el volcado del chipset cuesta 0,3 ms, asi que se
    /// puede hacer sin que se note.
    struct Checkpoint: Identifiable {
        let id = UUID()
        let momento: Date
        let teclas: Int
        let motivo: Motivo
        let estado: Data

        enum Motivo: String {
            case periodico = "cada poco"
            case antesDeBorrar = "antes de un AC"
            case cambioDeModo = "al cambiar de modo"
        }
    }

    @Published private(set) var checkpoints: [Checkpoint] = []

    /// Cada cuanto se guarda solo. Veinte segundos es el punto donde ya has escrito
    /// algo que dolería perder pero todavia te acuerdas de lo que hacias.
    static let segundosEntreCheckpoints: Double = 20

    /// Tope de puntos automaticos. Con 30 se cubre media hora larga de ejercicio.
    private static let maximoCheckpoints = 30

    private var segundosDesdeCheckpoint: Double = 0

    /// Teclas que borran trabajo. Se guarda un punto ANTES de dejarlas pasar, que es
    /// el unico momento en que todavia se puede.
    ///
    /// `AC` es 0x42 en las dos maquinas; `DEL` es 0x32. El `AC` es el que se lleva por
    /// delante media hora de cuenta, y siempre por accidente.
    private static let teclasDestructivas: Set<UInt8> = [0x42, 0x32]

    /// Codigos que entran en un menu de modo, donde se pierde lo que habia.
    private static let teclasDeModo: Set<UInt8> = [0x47]

    /// Segundos que se retiene un `AC` antes de aplicarlo, para poder arrepentirse.
    ///
    /// Cero lo desactiva, que es lo que quiere quien ya tiene el dedo hecho. No es un
    /// retardo de la emulacion: la tecla llega intacta, solo que un momento despues.
    @Published var retardoBorradoSegundos: Double = 0

    /// Lo que hay pendiente de aplicar por el retardo anterior.
    @Published private(set) var borradoPendiente: UInt8?
    private var trabajoBorrado: DispatchWorkItem?
    @Published private(set) var rebobinando = false

    /// De 0 a 1 mientras se rebobina. Reproducir cuesta unos 20 ms por tecla, asi
    /// que una sesion larga tarda segundos: sin barra parece que se colgo.
    @Published private(set) var progresoRebobinado: Double = 0

    /// Cierto solo durante un envio a velocidad lenta.
    ///
    /// Se distingue del rebobinado porque la interfaz tiene que hacer lo contrario:
    /// rebobinar tapa la pantalla con un velo, y un envio animado necesita que la
    /// caratula se vea, que es lo unico que hace que se aprenda donde esta cada tecla.
    @Published private(set) var envioAnimado = false

    /// Lo lee el hilo de reproduccion entre tecla y tecla para saber si parar.
    /// A 0,25x una expresion larga son varios segundos, y hay que poder cortarla.
    private let cancelarPeticion = Cancelacion()

    /// Caja compartida entre el hilo principal y el de reproduccion. Es una clase
    /// aparte para que quede claro que es el unico estado que cruza hilos.
    private final class Cancelacion {
        private let candado = NSLock()
        private var pedida = false

        func pedir() { candado.lock(); pedida = true; candado.unlock() }
        func limpiar() { candado.lock(); pedida = false; candado.unlock() }
        var estaPedida: Bool { candado.lock(); defer { candado.unlock() }; return pedida }
    }

    /// Si la vista sigue en pantalla. El reloj se para tambien durante el rebobinado,
    /// y al terminar hay que saber si tocaba reanudarlo o no.
    private var vistaActiva = false

    /// El firmware muestrea el teclado por interrupcion. Un toque humano de 80 ms
    /// se le puede escapar, asi que se garantiza un minimo. Por encima de ~600 ms
    /// saltaria la auto-repeticion, asi que el margen es comodo.
    private static let milisegundosMinimosPulsada: Double = 250

    /// `false` si la ROM no estaba en el paquete. La vista lo consulta para avisar
    /// en vez de quedarse con una pantalla negra sin explicacion.
    private(set) var cargada = false

    init(modelo: ModeloCalculadora) {
        guard let rom = Paquete.datos(modelo.recursoRom, extension: "bin"), !rom.isEmpty else {
            return
        }

        var configuracion = CalcEmuModel(
            hardware_id: modelo.hardwareId,
            real_hardware: modelo.realHardware,
            csr_mask: modelo.csrMask,
            pd_value: modelo.pdValue
        )

        emulador = rom.withUnsafeBytes { bytes -> OpaquePointer? in
            guard let base = bytes.bindMemory(to: UInt8.self).baseAddress else { return nil }
            return calcemu_create(&configuracion, base, rom.count)
        }
        guard let emulador else { return }

        ancho = Int(calcemu_screen_width(emulador))
        alto = Int(calcemu_screen_rows(emulador))
        pixeles = [UInt8](repeating: 0, count: max(0, ancho * alto))
        indicadores = [UInt8](repeating: 0, count: Int(calcemu_screen_row_size(emulador)))
        cargada = true
    }

    deinit {
        enlace?.invalidate()
        if let emulador { calcemu_destroy(emulador) }
    }

    // MARK: - Reloj

    func arrancar() {
        vistaActiva = true
        reanudarReloj()
    }

    func parar() {
        vistaActiva = false
        detenerReloj()
    }

    private func reanudarReloj() {
        guard enlace == nil, !rebobinando else { return }
        instanteAnterior = CACurrentMediaTime()
        let enlace = CADisplayLink(target: self, selector: #selector(latido))
        enlace.add(to: .main, forMode: .common)
        self.enlace = enlace
    }

    private func detenerReloj() {
        enlace?.invalidate()
        enlace = nil
    }

    @objc private func latido() {
        guard let emulador else { return }

        let ahora = CACurrentMediaTime()
        var transcurrido = (ahora - instanteAnterior) * 1000
        instanteAnterior = ahora
        // Si la app estuvo en segundo plano no tiene sentido intentar recuperar
        // minutos de emulacion de golpe: se acota.
        transcurrido = min(transcurrido, 100)

        calcemu_advance(emulador, UInt32(transcurrido))

        if teclaPulsada != nil {
            msDesdePulsacion += transcurrido
            if sueltaPendiente && msDesdePulsacion >= Self.milisegundosMinimosPulsada {
                soltarDeVerdad()
            }
        }

        if calcemu_take_redraw_flag(emulador) != 0 {
            leerPantalla()
        }

        // Punto de retorno automatico. Se cuelga del reloj que ya existe en vez de
        // montar un Timer aparte: asi no corre con la app en segundo plano, que es
        // justo cuando no hay nada nuevo que guardar.
        //
        // Solo se guarda si hay algo escrito desde el ultimo punto. Guardar el mismo
        // estado veinte veces llenaria la lista de puntos identicos y el usuario
        // tendria que leerlos todos para encontrar el que busca.
        segundosDesdeCheckpoint += transcurrido / 1000
        if segundosDesdeCheckpoint >= Self.segundosEntreCheckpoints {
            segundosDesdeCheckpoint = 0
            if historial.count > (checkpoints.last?.teclas ?? -1) {
                anotarCheckpoint(.periodico)
            }
        }
    }

    /// Lo que hay en el LCD en un instante, ya en memoria de Swift.
    ///
    /// Existe separado de las propiedades publicadas porque leer el emulador y
    /// publicar el resultado no pasan siempre en el mismo hilo: durante un envio
    /// animado se lee desde el hilo de reproduccion y se publica en el principal.
    private struct Instantanea {
        var pixeles: [UInt8]
        var lineas: [String]
        var indicadores: [UInt8]
    }

    /// Lee el emulador. Solo puede llamarlo quien tenga el emulador para si.
    private func capturar() -> Instantanea? {
        guard let emulador, ancho > 0, alto > 0 else { return nil }

        var buffer = [UInt8](repeating: 0, count: ancho * alto)
        let leidos = buffer.withUnsafeMutableBufferPointer { destino -> Int in
            guard let base = destino.baseAddress else { return 0 }
            return calcemu_screen_pixels(emulador, base, destino.count)
        }
        guard leidos > 0 else { return nil }

        let tamanoFila = Int(calcemu_screen_row_size(emulador))
        var crudo = [UInt8](repeating: 0, count: Int(calcemu_screen_buffer_size(emulador)))
        let copiados = crudo.withUnsafeMutableBufferPointer { destino -> Int in
            guard let base = destino.baseAddress else { return 0 }
            return calcemu_screen_buffer(emulador, base, destino.count)
        }

        return Instantanea(
            pixeles: buffer,
            // Solo se decodifica cuando el firmware ha cambiado algo, que es cuando
            // se pide refresco: hacerlo en cada fotograma seria un derroche.
            lineas: LectorPantalla.leer(pixeles: buffer, ancho: ancho, alto: alto),
            indicadores: copiados >= tamanoFila ? Array(crudo[0..<tamanoFila]) : indicadores
        )
    }

    /// Publica una instantanea. Siempre en el hilo principal: son @Published.
    private func aplicar(_ instantanea: Instantanea) {
        pixeles = instantanea.pixeles
        lineasPantalla = instantanea.lineas
        indicadores = instantanea.indicadores
    }

    private func leerPantalla() {
        if let instantanea = capturar() { aplicar(instantanea) }
    }

    // MARK: - Teclado

    func pulsar(_ codigo: UInt8) {
        guard emulador != nil, !rebobinando else { return }

        // Un AC con retardo no se aplica todavia: se queda a la vista un momento para
        // poder cancelarlo. Lo que llega a la Casio es exactamente la misma tecla.
        if retardoBorradoSegundos > 0, Self.teclasDestructivas.contains(codigo),
           borradoPendiente == nil {
            programarBorrado(codigo)
            return
        }
        pulsarDeVerdad(codigo)
    }

    private func pulsarDeVerdad(_ codigo: UInt8) {
        guard let emulador else { return }
        if teclaPulsada != nil { soltarDeVerdad() }

        // Antes de una tecla que borra, se deja un punto de retorno con nombre. Es el
        // caso que mas duele y el unico momento en que todavia se puede evitar.
        if Self.teclasDestructivas.contains(codigo) {
            anotarCheckpoint(.antesDeBorrar)
        } else if Self.teclasDeModo.contains(codigo) {
            anotarCheckpoint(.cambioDeModo)
        }

        // El estado se guarda ANTES de que la tecla llegue al firmware: es lo que hay
        // que recuperar para deshacerla.
        anotarInstantanea()
        calcemu_key_down(emulador, codigo)
        teclaPulsada = codigo
        msDesdePulsacion = 0
        sueltaPendiente = false
        historial.append(codigo)
    }

    // MARK: - Retardo del AC

    private func programarBorrado(_ codigo: UInt8) {
        borradoPendiente = codigo
        let trabajo = DispatchWorkItem { [weak self] in
            guard let self, self.borradoPendiente == codigo else { return }
            self.borradoPendiente = nil
            self.trabajoBorrado = nil
            self.pulsarDeVerdad(codigo)
        }
        trabajoBorrado = trabajo
        DispatchQueue.main.asyncAfter(deadline: .now() + retardoBorradoSegundos,
                                      execute: trabajo)
    }

    /// Se arrepintio: el `AC` no llega a la calculadora.
    func cancelarBorrado() {
        trabajoBorrado?.cancel()
        trabajoBorrado = nil
        borradoPendiente = nil
    }

    /// No quiere esperar: se aplica ya.
    func confirmarBorrado() {
        guard let codigo = borradoPendiente else { return }
        trabajoBorrado?.cancel()
        trabajoBorrado = nil
        borradoPendiente = nil
        pulsarDeVerdad(codigo)
    }

    // MARK: - Checkpoints

    private func anotarCheckpoint(_ motivo: Checkpoint.Motivo) {
        guard let estado = guardarEstado() else { return }
        // Dos puntos seguidos en la misma posicion no aportan nada y llenan la lista.
        if let ultimo = checkpoints.last, ultimo.teclas == historial.count,
           ultimo.motivo == motivo {
            return
        }
        checkpoints.append(Checkpoint(momento: Date(), teclas: historial.count,
                                      motivo: motivo, estado: estado))
        if checkpoints.count > Self.maximoCheckpoints { checkpoints.removeFirst() }
        segundosDesdeCheckpoint = 0
    }

    /// Vuelve a un punto automatico. Inmediato: es un volcado, no una reproduccion.
    func volverA(_ checkpoint: Checkpoint) {
        guard cargarEstado(checkpoint.estado) else { return }
        if teclaPulsada != nil { soltarDeVerdad() }
        // El historial se recorta a donde estaba ese punto, para que deshacer siga
        // contando bien a partir de aqui.
        if checkpoint.teclas <= historial.count {
            historial.removeLast(historial.count - checkpoint.teclas)
        }
        instantaneas.removeAll()
        teclasSinInstantanea = historial.count
        checkpoints.removeAll { $0.teclas > checkpoint.teclas }
    }

    /// Vuelve al ultimo punto anterior a un `AC`. Es el boton de «me lo cargué».
    func volverAntesDelUltimoBorrado() {
        guard let punto = checkpoints.last(where: { $0.motivo == .antesDeBorrar }) else { return }
        volverA(punto)
    }

    // MARK: - Instantaneas

    /// Vuelca el estado completo de la maquina. Nil si el nucleo no lo pudo dar.
    func guardarEstado() -> Data? {
        guard let emulador else { return nil }
        let tamano = calcemu_save_state(emulador, nil, 0)
        guard tamano > 0 else { return nil }
        var datos = Data(count: tamano)
        let escritos = datos.withUnsafeMutableBytes { destino -> Int in
            guard let base = destino.bindMemory(to: UInt8.self).baseAddress else { return 0 }
            return calcemu_save_state(emulador, base, tamano)
        }
        return escritos == tamano ? datos : nil
    }

    /// Deja la maquina en un estado guardado. Falso si el volcado no era de esta
    /// misma ROM y modelo, en cuyo caso la maquina se queda como estaba.
    @discardableResult
    func cargarEstado(_ datos: Data) -> Bool {
        guard let emulador, !datos.isEmpty else { return false }
        let ok = datos.withUnsafeBytes { origen -> Bool in
            guard let base = origen.bindMemory(to: UInt8.self).baseAddress else { return false }
            return calcemu_load_state(emulador, base, datos.count) == 1
        }
        if ok { leerPantalla() }
        return ok
    }

    private func anotarInstantanea() {
        guard let estado = guardarEstado() else { return }
        instantaneas.append(estado)
        if instantaneas.count > Self.maximoInstantaneas {
            instantaneas.removeFirst()
            teclasSinInstantanea += 1
        }
    }

    /// Deshace la ultima tecla.
    ///
    /// Ni la calculadora fisica puede hacer esto: alli la pulsacion es irreversible.
    func deshacer() {
        rebobinar(pasos: 1)
    }

    /// Rebobina varias teclas de golpe.
    ///
    /// Deshacer de una en una no sirve para el caso que mas duele: el `AC` pulsado
    /// sin querer que se llevo por delante media hora de cuenta. Ahi hay que poder
    /// saltar hacia atras varias pulsaciones a la vez.
    ///
    /// Si el punto de destino todavia tiene su estado guardado, es inmediato. Si se
    /// salio del tope y ya no lo tiene, se cae a reproducir desde el encendido, que
    /// es lento pero sigue siendo exacto.
    func rebobinar(pasos: Int) {
        guard pasos > 0, historial.count >= pasos, !rebobinando else { return }
        let destino = historial.count - pasos
        let indice = destino - teclasSinInstantanea

        if indice >= 0, indice < instantaneas.count, cargarEstado(instantaneas[indice]) {
            if teclaPulsada != nil { soltarDeVerdad() }
            historial.removeLast(pasos)
            instantaneas.removeLast(instantaneas.count - indice)
            progresoRebobinado = 1
            return
        }
        reproducir(Array(historial.dropLast(pasos)))
    }

    /// Deja la maquina exactamente como estaba en un punto guardado.
    ///
    /// Si el punto trae su volcado, se restaura al instante; si solo trae la secuencia
    /// de teclas (puntos guardados de antes de tener instantaneas), se reproduce.
    func restaurar(secuencia: [UInt8], estado: Data? = nil) {
        if let estado, cargarEstado(estado) {
            if teclaPulsada != nil { soltarDeVerdad() }
            historial = secuencia
            // Las instantaneas guardadas ya no corresponden a este historial.
            instantaneas.removeAll()
            teclasSinInstantanea = secuencia.count
            progresoRebobinado = 1
            return
        }
        reproducir(secuencia)
    }

    /// Teclea una secuencia seguida, sin reiniciar: es lo que usa el envio de una
    /// expresion pegada desde OneNote.
    ///
    /// Se anade al historial como si se hubieran pulsado a mano, para que deshacer
    /// y los puntos guardados sigan funcionando igual.
    /// A velocidad lenta se enciende cada tecla en la caratula y se refresca el LCD
    /// despues de cada pulsacion, para poder seguirlo con la vista.
    func teclear(_ codigos: [UInt8], velocidad: VelocidadEnvio = .instantaneo) {
        guard let emulador, !rebobinando, !codigos.isEmpty else { return }
        if teclaPulsada != nil { soltarDeVerdad() }
        rebobinando = true
        envioAnimado = velocidad.animada
        progresoRebobinado = 0
        cancelarPeticion.limpiar()
        detenerReloj()

        DispatchQueue.global(qos: .userInitiated).async {
            var tecleadas: [UInt8] = []
            // Se van guardando aqui y se publican al final: `instantaneas` es estado
            // del hilo principal y este bucle corre en otro.
            var estados: [Data] = []
            for (indice, codigo) in codigos.enumerated() {
                if self.cancelarPeticion.estaPedida { break }
                let empezo = CACurrentMediaTime()
                let hecho = Double(indice + 1) / Double(codigos.count)
                // Igual que al pulsar a mano: el estado previo a cada tecla, para que
                // deshacer tras pegar una expresion siga siendo inmediato.
                if let estado = self.guardarEstado() { estados.append(estado) }

                if velocidad.animada {
                    DispatchQueue.main.async {
                        self.teclaPulsada = codigo
                        self.progresoRebobinado = hecho
                    }
                }

                calcemu_key_down(emulador, codigo)
                calcemu_advance(emulador, UInt32(Self.milisegundosMinimosPulsada))
                calcemu_key_up(emulador)
                calcemu_advance(emulador, 400)
                tecleadas.append(codigo)

                if velocidad.animada {
                    let instantanea = self.capturar()
                    DispatchQueue.main.async {
                        self.teclaPulsada = nil
                        if let instantanea { self.aplicar(instantanea) }
                    }
                    // Emular las dos pulsaciones cuesta ~20 ms; el resto del tiempo
                    // se espera de verdad, que es lo que hace que se pueda seguir.
                    let falta = velocidad.segundosPorTecla - (CACurrentMediaTime() - empezo)
                    if falta > 0 { Thread.sleep(forTimeInterval: falta) }
                } else if indice % 8 == 0 {
                    DispatchQueue.main.async { self.progresoRebobinado = hecho }
                }
            }

            let instantanea = self.capturar()
            DispatchQueue.main.async {
                // Se apunta lo que se llego a teclear, no lo que se pidio: si el
                // envio se corto a la mitad, deshacer tiene que saberlo.
                self.historial.append(contentsOf: tecleadas)
                self.instantaneas.append(contentsOf: estados.prefix(tecleadas.count))
                let sobran = self.instantaneas.count - Self.maximoInstantaneas
                if sobran > 0 {
                    self.instantaneas.removeFirst(sobran)
                    self.teclasSinInstantanea += sobran
                }
                self.progresoRebobinado = 1
                self.rebobinando = false
                self.envioAnimado = false
                self.teclaPulsada = nil
                if let instantanea { self.aplicar(instantanea) }
                if self.vistaActiva { self.reanudarReloj() }
            }
        }
    }

    /// Corta un envio animado a medias. Lo tecleado hasta ahi se queda: es
    /// exactamente lo que pasaria si soltaras la calculadora a mitad de escribir.
    func cancelarEnvio() {
        guard envioAnimado else { return }
        cancelarPeticion.pedir()
    }

    /// Cuantas teclas se pueden deshacer todavia.
    var pasosDeshacer: Int { historial.count }

    /// La tecla mas antigua a la que se puede saltar al instante.
    ///
    /// La barra de rebobinado se dibuja desde aqui: mas atras existe, pero cuesta
    /// reproducir, y la barra tiene que prometer solo lo que puede cumplir.
    var primeraPosicionInmediata: Int { teclasSinInstantanea }

    /// Si volver a esa posicion es inmediato o toca reproducir.
    func esInmediatoVolverA(posicion: Int) -> Bool {
        let indice = posicion - teclasSinInstantanea
        return indice >= 0 && indice < instantaneas.count
    }

    /// Deja la maquina como estaba tras las primeras `posicion` teclas.
    ///
    /// Es lo que mueve la barra temporal: arrastrar hacia atras y ver la pantalla de
    /// aquel momento. Con instantaneas cuesta decimas de milisegundo, asi que se
    /// puede llamar en cada movimiento del dedo sin que la barra se atasque.
    func rebobinarA(posicion: Int) {
        guard posicion >= 0, posicion <= historial.count else { return }
        rebobinar(pasos: historial.count - posicion)
    }

    /// Reinicia y vuelve a teclear la secuencia entera. Sale exacto por construccion,
    /// sin tener que serializar el chipset, pero no es gratis: medido sobre esta ROM,
    /// cuesta unos 20 ms por tecla. De ahi que corra en segundo plano y con progreso.
    ///
    /// Mientras dura, este hilo es el unico que toca el emulador: el reloj esta
    /// parado y `pulsar` ignora las teclas. `self` se retiene a proposito, para que
    /// `deinit` no pueda destruir el emulador a mitad de la reproduccion.
    private func reproducir(_ secuencia: [UInt8]) {
        guard let emulador, !rebobinando else { return }
        if teclaPulsada != nil { soltarDeVerdad() }
        rebobinando = true
        progresoRebobinado = 0
        detenerReloj()

        DispatchQueue.global(qos: .userInitiated).async {
            calcemu_power_on(emulador)
            calcemu_advance(emulador, 3000)

            for (indice, codigo) in secuencia.enumerated() {
                calcemu_key_down(emulador, codigo)
                calcemu_advance(emulador, UInt32(Self.milisegundosMinimosPulsada))
                calcemu_key_up(emulador)
                calcemu_advance(emulador, 400)

                // Refrescar la barra en cada tecla seria inundar la cola principal
                // de mensajes para mover un pixel.
                if indice % 8 == 0 {
                    let hecho = Double(indice + 1) / Double(secuencia.count)
                    DispatchQueue.main.async { self.progresoRebobinado = hecho }
                }
            }

            DispatchQueue.main.async {
                self.historial = secuencia
                // Se reconstruyo la maquina desde cero: los volcados que habia ya no
                // corresponden a nada. A partir de aqui se vuelven a acumular.
                self.instantaneas.removeAll()
                self.teclasSinInstantanea = secuencia.count
                self.progresoRebobinado = 1
                self.rebobinando = false
                self.leerPantalla()
                if self.vistaActiva { self.reanudarReloj() }
            }
        }
    }

    /// Levantar el dedo no suelta la tecla de inmediato: se espera a cubrir el
    /// minimo para que el firmware llegue a verla.
    func soltar() {
        guard teclaPulsada != nil else { return }
        if msDesdePulsacion >= Self.milisegundosMinimosPulsada {
            soltarDeVerdad()
        } else {
            sueltaPendiente = true
        }
    }

    private func soltarDeVerdad() {
        guard let emulador else { return }
        calcemu_key_up(emulador)
        teclaPulsada = nil
        sueltaPendiente = false
        msDesdePulsacion = 0
    }
}
