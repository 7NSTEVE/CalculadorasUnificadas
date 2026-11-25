import SwiftUI
import UIKit

/// La capa de estudio que rodea a la calculadora.
///
/// No toca la emulación: observa las teclas y lee la pantalla. Está pensada para
/// que perder el hilo al saltar a OneNote y volver deje de costar caro.
struct PanelEstudio: View {
    @ObservedObject var sesion: SesionEstudio
    @ObservedObject var emulador: EmuladorCalculadora
    let nombreCalculadora: String

    @State private var busqueda = ""
    @State private var avisoCopiado: String?
    @State private var pidiendoNota = false
    @State private var notaNueva = ""

    /// Los resultados clavados, siempre visibles por encima de la cinta.
    ///
    /// Van con el resultado grande y la operación pequeña debajo: lo que hace falta
    /// de un vistazo es el número, y la operación solo cuando dudas de dónde salió.
    private var tiraDeFijados: some View {
        VStack(alignment: .leading, spacing: 6) {
            Text("FIJADOS")
                .font(.system(size: 10, weight: .bold))
                .foregroundStyle(.secondary)
            ForEach(sesion.fijados) { entrada in
                HStack(alignment: .firstTextBaseline, spacing: 8) {
                    VStack(alignment: .leading, spacing: 1) {
                        Text(entrada.resultado.isEmpty ? "—" : entrada.resultado)
                            .font(.system(.callout, design: .monospaced).weight(.semibold))
                        Text(entrada.teclas)
                            .font(.system(size: 10, design: .monospaced))
                            .foregroundStyle(.secondary)
                            .lineLimit(1)
                    }
                    Spacer(minLength: 4)
                    Button {
                        sesion.alternarFijado(entrada)
                    } label: {
                        Image(systemName: "pin.slash").font(.caption2)
                    }
                    .buttonStyle(.plain)
                    .foregroundStyle(.secondary)
                }
                .padding(.horizontal, 8)
                .padding(.vertical, 5)
                .background(Color(white: 0.20), in: RoundedRectangle(cornerRadius: 7))
                .contextMenu { menuCopiar(entrada) }
            }
        }
    }

    private var cintaFiltrada: [EntradaCinta] {
        guard !busqueda.isEmpty else { return sesion.cinta }
        let texto = busqueda.lowercased()
        return sesion.cinta.filter {
            $0.teclas.lowercased().contains(texto) || $0.resultado.lowercased().contains(texto)
        }
    }

    var body: some View {
        ScrollView {
            VStack(alignment: .leading, spacing: 0) {
                encabezado
                separador
                estante
                separador
                if !sesion.puntos.isEmpty {
                    puntos
                    separador
                }
                cinta
            }
        }
        .background(Color(white: 0.13))
        .foregroundStyle(.white)
        .overlay(alignment: .bottom) {
            if let aviso = avisoCopiado {
                Text(aviso)
                    .font(.footnote.weight(.medium))
                    .padding(.horizontal, 14).padding(.vertical, 8)
                    .background(.ultraThinMaterial, in: Capsule())
                    .padding(.bottom, 16)
                    .transition(.opacity)
            }
        }
        .animation(.easeInOut(duration: 0.2), value: avisoCopiado)
        .alert("Guardar punto", isPresented: $pidiendoNota) {
            TextField("p. ej. ejercicio 4, antes de despejar", text: $notaNueva)
            Button("Guardar") {
                sesion.anotarPunto(nota: notaNueva, teclas: emulador.historial,
                                   estado: emulador.guardarEstado())
                notaNueva = ""
                avisar("Punto guardado")
            }
            Button("Cancelar", role: .cancel) { notaNueva = "" }
        } message: {
            Text("Se guarda la calculadora entera: pantalla, memorias, modo y el "
                 + "objetivo que tengas escrito.")
        }
    }

    private var separador: some View {
        Divider().overlay(Color.white.opacity(0.15))
    }

    private func titulo(_ texto: String) -> some View {
        Text(texto)
            .font(.system(size: 10, weight: .bold))
            .foregroundStyle(.secondary)
    }

    // MARK: - Objetivo y paso

    private var encabezado: some View {
        VStack(alignment: .leading, spacing: 10) {
            campo(rotulo: "OBJETIVO", marcador: "p. ej. hallar V en R3",
                  texto: $sesion.objetivo, destacado: true)
            campo(rotulo: "AHORA", marcador: "p. ej. equivalente de R2 || R3",
                  texto: $sesion.siguientePaso, destacado: false)

            if !sesion.enCurso.isEmpty {
                HStack(spacing: 6) {
                    Image(systemName: "pencil.line").font(.caption2)
                    Text(sesion.enCurso.joined(separator: " "))
                        .font(.system(.footnote, design: .monospaced))
                        .lineLimit(1)
                }
                .foregroundStyle(.secondary)
            }

            if !sesion.fijados.isEmpty { tiraDeFijados }

            Button {
                notaNueva = sesion.objetivo
                pidiendoNota = true
            } label: {
                Label("Guardar punto", systemImage: "pin.fill")
                    .font(.footnote.weight(.medium))
                    .frame(maxWidth: .infinity)
            }
            .buttonStyle(.bordered)
            .tint(.orange)
            .disabled(emulador.rebobinando)
        }
        .padding(14)
    }

    private func campo(rotulo: String, marcador: String,
                       texto: Binding<String>, destacado: Bool) -> some View {
        VStack(alignment: .leading, spacing: 3) {
            titulo(rotulo)
            TextField(marcador, text: texto, axis: .vertical)
                .textFieldStyle(.plain)
                .font(destacado ? .callout.weight(.semibold) : .callout)
                .lineLimit(1...3)
        }
    }

    // MARK: - Estante de valores

    /// Los datos del enunciado, con el nombre que tienen allí.
    ///
    /// La Casio guarda números en `A`…`F`, pero no guarda a qué correspondían, y eso
    /// es justo lo que se olvida al volver de otra app. Aquí se escribe al lado.
    private var estante: some View {
        VStack(alignment: .leading, spacing: 8) {
            HStack {
                titulo("VALORES DEL EJERCICIO")
                Spacer()
                Button { sesion.anadirVariable() } label: {
                    Image(systemName: "plus.circle.fill").font(.footnote)
                }
                .buttonStyle(.plain)
                .foregroundStyle(.secondary)
            }

            if sesion.variables.isEmpty {
                Text("Apunta aquí los datos (V = 12 V, R1 = 330 Ω…) para no tener que "
                     + "sostenerlos en la cabeza.")
                    .font(.footnote)
                    .foregroundStyle(.secondary)
            } else {
                ForEach($sesion.variables) { $variable in
                    filaVariable($variable)
                }
            }
        }
        .padding(14)
    }

    private func filaVariable(_ variable: Binding<VariableEstudio>) -> some View {
        HStack(spacing: 6) {
            TextField("R1", text: variable.nombre)
                .frame(width: 54)
                .font(.system(.footnote, design: .monospaced).weight(.semibold))
            Text("=").foregroundStyle(.tertiary).font(.footnote)
            TextField("330", text: variable.valor)
                .font(.system(.footnote, design: .monospaced))
                .keyboardType(.numbersAndPunctuation)
            TextField("Ω", text: variable.unidad)
                .frame(width: 38)
                .font(.footnote)
        }
        .textFieldStyle(.roundedBorder)
        .contentShape(Rectangle())
        .onDrag {
            // Arrastrar el valor suelto: lo normal es querer meterlo en una fórmula
            // de OneNote, no la ficha entera.
            NSItemProvider(object: variable.wrappedValue.valor as NSString)
        }
        .contextMenu {
            Button {
                UIPasteboard.general.string = variable.wrappedValue.etiqueta
                avisar("Copiado \(variable.wrappedValue.etiqueta)")
            } label: {
                Label("Copiar", systemImage: "doc.on.doc")
            }
            Button(role: .destructive) {
                sesion.borrarVariable(variable.wrappedValue)
            } label: {
                Label("Quitar", systemImage: "trash")
            }
        }
    }

    // MARK: - Puntos guardados

    private var puntos: some View {
        VStack(alignment: .leading, spacing: 8) {
            titulo("PUNTOS GUARDADOS")
            ForEach(sesion.puntos) { punto in
                filaPunto(punto)
            }
        }
        .padding(14)
    }

    private func filaPunto(_ punto: PuntoGuardado) -> some View {
        Button {
            emulador.restaurar(secuencia: punto.teclas, estado: punto.estado)
            sesion.adoptar(punto)
        } label: {
            HStack(spacing: 10) {
                Image(systemName: "pin.fill")
                    .font(.caption)
                    .foregroundStyle(.orange)
                VStack(alignment: .leading, spacing: 2) {
                    Text(punto.nota)
                        .font(.footnote.weight(.medium))
                        .lineLimit(1)
                    Text(descripcion(de: punto))
                        .font(.caption2)
                        .foregroundStyle(.secondary)
                        .lineLimit(1)
                }
                Spacer()
                Image(systemName: "arrow.uturn.backward.circle")
                    .foregroundStyle(.tertiary)
            }
            .contentShape(Rectangle())
        }
        .buttonStyle(.plain)
        .disabled(emulador.rebobinando)
        .contextMenu {
            Button(role: .destructive) {
                sesion.borrarPunto(punto)
            } label: {
                Label("Borrar punto", systemImage: "trash")
            }
        }
    }

    /// Volver a un punto cuesta tiempo real, así que se dice antes de pulsarlo.
    private func descripcion(de punto: PuntoGuardado) -> String {
        var partes: [String] = []
        if !punto.resumen.isEmpty { partes.append(punto.resumen) }
        partes.append(punto.momento.formatted(date: .omitted, time: .shortened))
        if punto.segundosEstimados >= 1 {
            partes.append("volver: \(Int(punto.segundosEstimados.rounded())) s")
        }
        return partes.joined(separator: " · ")
    }

    // MARK: - Cinta

    private var cinta: some View {
        VStack(alignment: .leading, spacing: 0) {
            HStack {
                titulo("CINTA DE CÁLCULO")
                Spacer()
                if !sesion.cinta.isEmpty {
                    Text("\(sesion.cinta.count)")
                        .font(.caption2)
                        .foregroundStyle(.tertiary)
                }
            }
            .padding(.horizontal, 14)
            .padding(.top, 14)

            if sesion.cinta.count > 3 {
                TextField("Buscar 330, sin, 36.36…", text: $busqueda)
                    .textFieldStyle(.roundedBorder)
                    .font(.footnote)
                    .padding(.horizontal, 14)
                    .padding(.top, 6)
            }

            if sesion.cinta.isEmpty {
                Text("Cada `=` queda aquí anotado con su resultado, para no tener que "
                     + "acordarte de dónde salió un número.")
                    .font(.footnote)
                    .foregroundStyle(.secondary)
                    .padding(14)
            } else {
                LazyVStack(alignment: .leading, spacing: 0) {
                    ForEach(cintaFiltrada) { entrada in
                        filaCinta(entrada)
                            .padding(.horizontal, 14)
                            .padding(.vertical, 8)
                        Divider().overlay(Color.white.opacity(0.10))
                    }
                }
                .padding(.top, 4)
            }
        }
    }

    private func filaCinta(_ entrada: EntradaCinta) -> some View {
        HStack(alignment: .top, spacing: 10) {
            // La miniatura es la pantalla real de aquel momento, no una recreación.
            // Reconocer un cálculo por cómo se veía es más rápido que leer la
            // secuencia de teclas.
            if entrada.ancho > 0, entrada.alto > 0 {
                PantallaLCD(pixeles: entrada.pixeles,
                            ancho: entrada.ancho, alto: entrada.alto)
                    .frame(width: 78,
                           height: 78 * CGFloat(entrada.alto) / CGFloat(entrada.ancho))
                    .clipShape(RoundedRectangle(cornerRadius: 2))
            }

            VStack(alignment: .leading, spacing: 3) {
                Text(entrada.teclas)
                    .font(.system(.footnote, design: .monospaced))
                    .foregroundStyle(.secondary)
                HStack(alignment: .firstTextBaseline) {
                    Text(entrada.resultado.isEmpty ? "—" : entrada.resultado)
                        .font(.system(.body, design: .monospaced).weight(.semibold))
                    Spacer()
                    if !entrada.modo.isEmpty {
                        Text(entrada.modo)
                            .font(.system(size: 9))
                            .foregroundStyle(.tertiary)
                    }
                }
            }
        }
        .frame(maxWidth: .infinity, alignment: .leading)
        .contentShape(Rectangle())
        // Arrastrar la fila hasta OneNote es un gesto menos que copiar y pegar, y
        // en pantalla partida no obliga a soltar el iPad.
        .onDrag {
            TarjetaCalculo.proveedor(entrada: entrada, objetivo: sesion.objetivo,
                                     calculadora: nombreCalculadora)
        }
        .contextMenu { menuCopiar(entrada) }
    }

    /// Cuatro formas de llevarse el cálculo, según lo que pida OneNote en cada momento.
    @ViewBuilder private func menuCopiar(_ entrada: EntradaCinta) -> some View {
        Button {
            sesion.alternarFijado(entrada)
        } label: {
            Label(sesion.estaFijado(entrada) ? "Quitar de fijados" : "Fijar a la vista",
                  systemImage: sesion.estaFijado(entrada) ? "pin.slash" : "pin")
        }

        Divider()

        Button {
            UIPasteboard.general.image = TarjetaCalculo.imagen(
                entrada: entrada, objetivo: sesion.objetivo, calculadora: nombreCalculadora)
            avisar("Tarjeta copiada como imagen")
        } label: {
            Label("Copiar tarjeta como imagen", systemImage: "photo")
        }

        Button {
            UIPasteboard.general.image = TarjetaCalculo.imagenDePantalla(entrada: entrada)
            avisar("Pantalla copiada")
        } label: {
            Label("Copiar solo la pantalla", systemImage: "rectangle.dashed")
        }

        Button {
            UIPasteboard.general.string = TarjetaCalculo.texto(
                entrada: entrada, objetivo: sesion.objetivo, calculadora: nombreCalculadora)
            avisar("Texto copiado")
        } label: {
            Label("Copiar como texto", systemImage: "doc.on.doc")
        }

        Button {
            UIPasteboard.general.string = entrada.resultado
            avisar("Resultado copiado")
        } label: {
            Label("Copiar solo el resultado", systemImage: "number")
        }
    }

    private func avisar(_ mensaje: String) {
        avisoCopiado = mensaje
        DispatchQueue.main.asyncAfter(deadline: .now() + 1.6) {
            if avisoCopiado == mensaje { avisoCopiado = nil }
        }
    }
}
