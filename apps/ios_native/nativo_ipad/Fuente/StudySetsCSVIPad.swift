import Foundation

struct FilaStudySetCSVIPad: Equatable { var pregunta: String; var respuesta: String; var etiquetas: [String] }
enum ErrorStudySetCSVIPad: Error { case vacio, demasiadasFilas, sinTarjetas }

enum ParserStudySetCSVIPad {
    static func analizar(_ texto: String, separador: Character? = nil,
                         limite: Int = 10_000) throws -> [FilaStudySetCSVIPad] {
        guard !texto.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty else { throw ErrorStudySetCSVIPad.vacio }
        let sep = separador ?? detectar(texto)
        let filas = parsear(texto, sep)
        guard filas.count <= limite + 1 else { throw ErrorStudySetCSVIPad.demasiadasFilas }
        var inicio = 0, p = 0, r = 1; var t: Int?
        if let cab = filas.first {
            let n = cab.map(Normalizador.texto)
            if let ip = n.firstIndex(where: { ["pregunta","question","front","anverso"].contains($0) }),
               let ir = n.firstIndex(where: { ["respuesta","answer","back","reverso"].contains($0) }) {
                p = ip; r = ir; inicio = 1
                t = n.firstIndex(where: { ["etiquetas","tags","tag"].contains($0) })
            }
        }
        let salida = filas.dropFirst(inicio).compactMap { fila -> FilaStudySetCSVIPad? in
            guard fila.indices.contains(p), fila.indices.contains(r) else { return nil }
            let a = limpiar(fila[p]), b = limpiar(fila[r]); guard !a.isEmpty, !b.isEmpty else { return nil }
            let tags = t.flatMap { i in fila.indices.contains(i) ? fila[i].split { $0 == ";" || $0 == "|" }.map { limpiar(String($0)) } : nil } ?? []
            return .init(pregunta: a, respuesta: b, etiquetas: tags.filter { !$0.isEmpty })
        }
        guard !salida.isEmpty else { throw ErrorStudySetCSVIPad.sinTarjetas }
        return salida
    }

    private static func detectar(_ texto: String) -> Character {
        let linea = texto.split(whereSeparator: \.isNewline).first.map(String.init) ?? texto
        return linea.filter { $0 == "\t" }.count > linea.filter { $0 == "," }.count ? "\t" : ","
    }
    private static func limpiar(_ s: String) -> String { s.trimmingCharacters(in: .whitespacesAndNewlines) }
    private static func parsear(_ texto: String, _ sep: Character) -> [[String]] {
        var out: [[String]] = [], row: [String] = [], field = "", quoted = false, i = texto.startIndex
        while i < texto.endIndex {
            let c = texto[i], next = texto.index(after: i)
            if c == "\"" {
                if quoted, next < texto.endIndex, texto[next] == "\"" { field.append("\""); i = texto.index(after: next); continue }
                quoted.toggle()
            } else if c == sep && !quoted { row.append(field); field = "" }
            else if c == "\n" && !quoted {
                row.append(field); field = ""; if row.contains(where: { !limpiar($0).isEmpty }) { out.append(row) }; row = []
            } else if c != "\r" || quoted { field.append(c) }
            i = next
        }
        if !field.isEmpty || !row.isEmpty { row.append(field); if row.contains(where: { !limpiar($0).isEmpty }) { out.append(row) } }
        return out
    }
}
