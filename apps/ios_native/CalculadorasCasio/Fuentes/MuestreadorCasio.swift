import Combine
import Foundation

/// Evalúa una función preguntándole a la calculadora emulada, punto por punto.
///
/// Es la pieza que permite graficar **sin motor matemático propio**. La alternativa
/// —escribir un evaluador en Swift— haría que la gráfica y el resultado de pulsar `=`
/// pudieran discrepar, y entonces habría dos verdades sobre el mismo número. En un
/// proyecto cuya premisa entera es «los resultados son los de una Casio de verdad»,
/// eso no vale.
///
/// El truco que lo hace viable es la instantánea del chipset: se teclea la expresión
/// UNA vez con un hueco para la `x`, se guarda el estado, y para cada punto se
/// restaura (0,3 ms) y se teclea solo el número. Sin instantáneas habría que
/// reproducir todo desde el encendido en cada punto y serían minutos.
///
/// Aun así cuesta: ~100 ms por punto. Se avisa antes y se puede cancelar.
@MainActor
final class MuestreadorCasio: ObservableObject {

    @Published private(set) var muestreando = false
    @Published private(set) var progreso: Double = 0
    @Published private(set) var muestras: [MuestraCurva] = []
    /// Explicación en curso, para que la espera no sea una barra muda.
    @Published private(set) var estado = ""

    private var cancelado = false

    /// Teclea `expresion` sustituyendo `x` por cada valor y lee el resultado.
    ///
    /// `expresion` viene con `x` como marcador; para cada punto se sustituye por el
    /// número y se traduce entero. Es más lento que teclear solo el número, pero
    /// evita depender de que la máquina esté en un modo concreto, y sobre todo evita
    /// que un modo mal supuesto devuelva números plausibles y equivocados.
    func muestrear(expresion: String, desde: Double, hasta: Double, cantidad: Int,
                   modelo: ModeloCalculadora,
                   emulador: EmuladorCalculadora) async {
        guard !muestreando else { return }
        muestreando = true
        cancelado = false
        progreso = 0
        muestras = []
        estado = "preparando…"

        let valores = Grafica.puntosDeMuestreo(desde: desde, hasta: hasta,
                                               cantidad: cantidad)
        guard !valores.isEmpty else {
            muestreando = false
            estado = "el rango no vale"
            return
        }

        // Estado al que se vuelve antes de cada punto. Si no se puede guardar, no se
        // muestrea: reproducir desde el encendido en cada punto serían minutos, y una
        // barra de progreso que tarda eso es peor que decir que no.
        guard let base = emulador.guardarEstado() else {
            muestreando = false
            estado = "no se pudo guardar el estado de la calculadora"
            return
        }

        var recogidas: [MuestraCurva] = []
        for (indice, x) in valores.enumerated() {
            if cancelado { break }

            estado = "punto \(indice + 1) de \(valores.count)"
            progreso = Double(indice) / Double(valores.count)

            emulador.cargarEstado(base)
            let y = await evaluar(expresion: expresion, en: x,
                                  modelo: modelo, emulador: emulador)
            recogidas.append(MuestraCurva(x: x, y: y))
            muestras = recogidas
        }

        // Se deja la calculadora como estaba antes de graficar: el usuario no pidió
        // que le cambiaran la pantalla, pidió una gráfica.
        emulador.cargarEstado(base)

        progreso = 1
        estado = cancelado ? "cancelado" : "listo"
        muestreando = false
    }

    func cancelar() {
        cancelado = true
    }

    /// Un punto: sustituye, traduce, teclea y lee el LCD.
    ///
    /// Devuelve nil si la calculadora no dio un número: un `Math ERROR` por dividir
    /// entre cero es información legítima sobre la función, no un fallo que ocultar.
    private func evaluar(expresion: String, en x: Double,
                         modelo: ModeloCalculadora,
                         emulador: EmuladorCalculadora) async -> Double? {
        // %.12g conserva las cifras que la Casio puede aceptar sin arrastrar el ruido
        // binario de un Double completo.
        let numero = String(format: "%.12g", x)
        // Entre paréntesis: sin ellos, una x negativa cambiaría la precedencia.
        let concreta = expresion.replacingOccurrences(of: "x", with: "(\(numero))")

        guard let traduccion = try? TraductorExpresion.traducir(
                  concreta, para: modelo.modeloTraductor),
              let codigos = modelo.disposicion.codigos(para: traduccion.teclas)
        else { return nil }

        emulador.teclear(codigos + igual(modelo), velocidad: .instantaneo)

        // `teclear` corre en segundo plano; hay que dejarle terminar antes de leer.
        while emulador.rebobinando {
            try? await Task.sleep(nanoseconds: 10_000_000)
        }

        guard let ultima = emulador.lineasPantalla.last(where: { !$0.isEmpty }) else {
            return nil
        }
        if ultima.contains("ERROR") { return nil }
        return LectorContenido.numero(ultima)
    }

    private func igual(_ modelo: ModeloCalculadora) -> [UInt8] {
        modelo.disposicion.teclas.first { $0.nombre == "IGUAL" }.map { [$0.codigo] } ?? []
    }
}

/// Árbol de intentos: probar un camino, ver que estaba mal y volver sin perderlo.
///
/// Puntos 27, 28 y 29. Estaban bloqueados porque volver a un estado costaba segundos;
/// con la instantánea del chipset cada rama es un volcado de unos pocos KB y saltar
/// entre ellas es inmediato.
///
/// Para qué sirve de verdad: en un ejercicio largo pruebas un camino, te das cuenta a
/// mitad de que el planteamiento estaba mal, y hoy la única salida es empezar de
/// cero. Aquí se marca la rama como fallida, se vuelve al punto donde se bifurcó, y
/// **la rama mala sigue estando** para compararla y entender por qué no salía.
final class ArbolIntentos: ObservableObject {

    struct Rama: Identifiable {
        let id = UUID()
        var nombre: String
        /// De qué rama salió. Nil en la raíz.
        var padre: UUID?
        /// Estado de la máquina al empezar esta rama.
        var estadoInicial: Data
        /// Estado al abandonarla, para poder compararla sin volver a ejecutarla.
        var estadoFinal: Data?
        var teclasAlEmpezar: Int
        var resultado: String
        var descartada: Bool
        var momento: Date
    }

    @Published private(set) var ramas: [Rama] = []
    @Published private(set) var actual: UUID?

    /// Empieza una rama nueva desde el estado de ahora.
    func bifurcar(nombre: String, estado: Data, teclas: Int) {
        // La rama que se abandona guarda su estado final, que es lo que permite
        // compararla después sin tener que volver a ejecutarla.
        if let indice = ramas.firstIndex(where: { $0.id == actual }) {
            ramas[indice].estadoFinal = estado
        }
        let rama = Rama(nombre: nombre, padre: actual, estadoInicial: estado,
                        estadoFinal: nil, teclasAlEmpezar: teclas,
                        resultado: "", descartada: false, momento: Date())
        ramas.append(rama)
        actual = rama.id
    }

    /// «Esto estaba mal»: se marca y se devuelve el estado al que volver.
    func descartar(_ id: UUID, estadoActual: Data?) -> Data? {
        guard let indice = ramas.firstIndex(where: { $0.id == id }) else { return nil }
        ramas[indice].descartada = true
        if let estadoActual { ramas[indice].estadoFinal = estadoActual }

        // Se vuelve al principio de la rama descartada, que es donde se bifurcó.
        actual = ramas[indice].padre
        return ramas[indice].estadoInicial
    }

    func anotarResultado(_ texto: String) {
        guard let indice = ramas.firstIndex(where: { $0.id == actual }) else { return }
        ramas[indice].resultado = texto
    }

    /// Las ramas hijas de una, para dibujar el árbol.
    func hijas(de id: UUID?) -> [Rama] {
        ramas.filter { $0.padre == id }
    }

    /// Dos ramas juntas, para el punto 29.
    func comparar(_ primera: UUID, _ segunda: UUID) -> (Rama, Rama)? {
        guard let a = ramas.first(where: { $0.id == primera }),
              let b = ramas.first(where: { $0.id == segunda }) else { return nil }
        return (a, b)
    }

    func limpiar() {
        ramas = []
        actual = nil
    }
}
