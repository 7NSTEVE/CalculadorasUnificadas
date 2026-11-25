import CryptoKit
import Foundation
import PDFKit
import UIKit

struct ConsultaCorpusGoodnotesIPad: Codable, Equatable {
    var termino: String
    var paginasEsperadas: [Int]
}

struct ManifiestoCorpusGoodnotesIPad: Codable, Equatable {
    var version = 1
    var corpusId: String
    var paginas: Int
    var sha256PDF: String
    var bytesPDF: Int64
    var anchoPagina: Double
    var altoPagina: Double
    var consultas: [ConsultaCorpusGoodnotesIPad]
}

struct ResultadoCorpusGoodnotesIPad: Equatable {
    var pdf: URL
    var manifiesto: URL
    var metadata: ManifiestoCorpusGoodnotesIPad
}

enum ErrorCorpusGoodnotesIPad: LocalizedError, Equatable {
    case paginasInvalidas
    case cancelado
    case pdfInvalido
    case shaNoCoincide
    case manifiestoInvalido

    var errorDescription: String? {
        switch self {
        case .paginasInvalidas: return "La cantidad de páginas del corpus no es válida."
        case .cancelado: return "La generación del corpus fue cancelada."
        case .pdfInvalido: return "El PDF del corpus no pudo validarse."
        case .shaNoCoincide: return "El PDF ya no coincide con el SHA-256 del manifiesto."
        case .manifiestoInvalido: return "El manifiesto del corpus es inválido o incompatible."
        }
    }
}

/// Produce un PDF que puede importarse sin modificación en CuadernU y
/// Goodnotes. El archivo se genera una vez y su SHA-256 se conserva; las
/// comparaciones deben reutilizar ese mismo binario, no regenerarlo por app.
enum GeneradorCorpusGoodnotesIPad {
    static let version = 1
    static let paginasCanonicas = 500
    static let tamanoPagina = CGSize(width: 612, height: 792)
    static let corpusCanonicoId = "cuadernu-goodnotes-500p-v1"

    private static let terminos = [
        "KIRCHHOFF", "THEVENIN", "NORTON", "RLC", "FOURIER",
        "LAPLACE", "IMPEDANCIA", "FASOR", "MAXWELL", "TRANSISTOR",
    ]

    @discardableResult
    static func generar(
        en carpeta: URL,
        paginas: Int = paginasCanonicas,
        corpusId: String? = nil,
        archivos: FileManager = .default,
        cancelado: () -> Bool = { false },
        progreso: (Double) -> Void = { _ in }
    ) throws -> ResultadoCorpusGoodnotesIPad {
        guard paginas > 0, paginas <= 2_000 else {
            throw ErrorCorpusGoodnotesIPad.paginasInvalidas
        }
        try archivos.createDirectory(at: carpeta, withIntermediateDirectories: true)
        let id = corpusId ?? (paginas == paginasCanonicas
            ? corpusCanonicoId : "cuadernu-goodnotes-\(paginas)p-v1")
        let pdf = carpeta.appendingPathComponent("\(id).pdf")
        let manifiesto = carpeta.appendingPathComponent("\(id).json")
        let temporal = carpeta.appendingPathComponent(
            ".\(id)-\(UUID().uuidString).pdf")
        try? archivos.removeItem(at: temporal)

        let formato = UIGraphicsPDFRendererFormat()
        formato.documentInfo = [
            kCGPDFContextTitle as String: "CuadernU vs Goodnotes QA Corpus v1",
            kCGPDFContextCreator as String: "CuadernU QA",
            kCGPDFContextAuthor as String: "CuadernU deterministic benchmark",
        ]
        let bounds = CGRect(origin: .zero, size: tamanoPagina)
        let renderer = UIGraphicsPDFRenderer(bounds: bounds, format: formato)
        var fueCancelado = false
        do {
            try renderer.writePDF(to: temporal) { contexto in
                for indice in 0..<paginas {
                    if cancelado() {
                        fueCancelado = true
                        break
                    }
                    contexto.beginPage()
                    dibujarPagina(indice, total: paginas, en: contexto.cgContext)
                    progreso(Double(indice + 1) / Double(paginas))
                }
            }
        } catch {
            try? archivos.removeItem(at: temporal)
            throw error
        }
        if fueCancelado {
            try? archivos.removeItem(at: temporal)
            throw ErrorCorpusGoodnotesIPad.cancelado
        }

        guard let documento = PDFDocument(url: temporal),
              documento.pageCount == paginas else {
            try? archivos.removeItem(at: temporal)
            throw ErrorCorpusGoodnotesIPad.pdfInvalido
        }

        let sha = try sha256(temporal)
        let bytes = try temporal.resourceValues(forKeys: [.fileSizeKey]).fileSize ?? 0
        guard bytes > 0 else {
            try? archivos.removeItem(at: temporal)
            throw ErrorCorpusGoodnotesIPad.pdfInvalido
        }
        let metadata = ManifiestoCorpusGoodnotesIPad(
            corpusId: id,
            paginas: paginas,
            sha256PDF: sha,
            bytesPDF: Int64(bytes),
            anchoPagina: tamanoPagina.width,
            altoPagina: tamanoPagina.height,
            consultas: consultas(paginas: paginas))

        let encoder = JSONEncoder()
        encoder.outputFormatting = [.prettyPrinted, .sortedKeys]
        let datosManifest = try encoder.encode(metadata)
        let manifestTemporal = carpeta.appendingPathComponent(
            ".\(id)-\(UUID().uuidString).json")
        try datosManifest.write(to: manifestTemporal, options: .atomic)

        if archivos.fileExists(atPath: pdf.path) { try archivos.removeItem(at: pdf) }
        if archivos.fileExists(atPath: manifiesto.path) {
            try archivos.removeItem(at: manifiesto)
        }
        try archivos.moveItem(at: temporal, to: pdf)
        do {
            try archivos.moveItem(at: manifestTemporal, to: manifiesto)
        } catch {
            try? archivos.removeItem(at: pdf)
            try? archivos.removeItem(at: manifestTemporal)
            throw error
        }
        return ResultadoCorpusGoodnotesIPad(
            pdf: pdf, manifiesto: manifiesto, metadata: metadata)
    }

    static func validar(
        pdf: URL,
        manifiesto: URL
    ) throws -> ManifiestoCorpusGoodnotesIPad {
        let datos = try Data(contentsOf: manifiesto, options: .mappedIfSafe)
        let metadata = try JSONDecoder().decode(
            ManifiestoCorpusGoodnotesIPad.self, from: datos)
        guard metadata.version == version,
              metadata.paginas > 0,
              metadata.paginas <= 2_000,
              metadata.bytesPDF > 0,
              metadata.sha256PDF.count == 64,
              metadata.anchoPagina > 0,
              metadata.altoPagina > 0,
              !metadata.corpusId.isEmpty else {
            throw ErrorCorpusGoodnotesIPad.manifiestoInvalido
        }
        guard let documento = PDFDocument(url: pdf),
              documento.pageCount == metadata.paginas else {
            throw ErrorCorpusGoodnotesIPad.pdfInvalido
        }
        guard try sha256(pdf) == metadata.sha256PDF else {
            throw ErrorCorpusGoodnotesIPad.shaNoCoincide
        }
        let bytes = try pdf.resourceValues(forKeys: [.fileSizeKey]).fileSize ?? 0
        guard Int64(bytes) == metadata.bytesPDF else {
            throw ErrorCorpusGoodnotesIPad.shaNoCoincide
        }
        return metadata
    }

    private static func sha256(_ url: URL) throws -> String {
        let manejador = try FileHandle(forReadingFrom: url)
        defer { try? manejador.close() }
        var hasher = SHA256()
        while autoreleasepool(invoking: {
            let bloque = try? manejador.read(upToCount: 1_048_576)
            guard let datos = bloque ?? nil, !datos.isEmpty else { return false }
            hasher.update(data: datos)
            return true
        }) {}
        return hasher.finalize().map { String(format: "%02x", $0) }.joined()
    }

    private static func consultas(paginas: Int) -> [ConsultaCorpusGoodnotesIPad] {
        terminos.enumerated().map { indiceTermino, termino in
            let paginasEsperadas = (0..<paginas).compactMap { pagina in
                pagina % terminos.count == indiceTermino ? pagina + 1 : nil
            }
            return ConsultaCorpusGoodnotesIPad(
                termino: termino,
                paginasEsperadas: paginasEsperadas)
        }
    }

    private static func dibujarPagina(
        _ indice: Int,
        total: Int,
        en cg: CGContext
    ) {
        let bounds = CGRect(origin: .zero, size: tamanoPagina)
        cg.setFillColor(UIColor.white.cgColor)
        cg.fill(bounds)

        cg.setStrokeColor(UIColor(white: 0.88, alpha: 1).cgColor)
        cg.setLineWidth(0.5)
        var y: CGFloat = 54
        while y < bounds.maxY - 42 {
            cg.move(to: CGPoint(x: 36, y: y))
            cg.addLine(to: CGPoint(x: bounds.maxX - 36, y: y))
            y += 24
        }
        cg.strokePath()

        let pagina = indice + 1
        let termino = terminos[indice % terminos.count]
        let cabecera = "QA CORPUS · \(termino) · PAGE \(String(format: "%04d", pagina)) / \(String(format: "%04d", total))"
        (cabecera as NSString).draw(
            at: CGPoint(x: 36, y: 22),
            withAttributes: [
                .font: UIFont.monospacedSystemFont(ofSize: 10, weight: .semibold),
                .foregroundColor: UIColor.black,
            ])

        let cuerpo = "\(termino) benchmark token \(String(format: "%04d", pagina)). Circuit analysis reference: V=IR, KCL sum(I)=0, KVL sum(V)=0. Deterministic searchable text for CuadernU/Goodnotes comparison."
        (cuerpo as NSString).draw(
            in: CGRect(x: 48, y: 78, width: 516, height: 72),
            withAttributes: [
                .font: UIFont.systemFont(ofSize: 12),
                .foregroundColor: UIColor.black,
            ])

        // Diagrama vectorial determinista: una red tipo circuito cuya geometría
        // varía con el índice sin usar RNG ni tiempo de reloj.
        let baseY: CGFloat = 230 + CGFloat((indice % 5) * 12)
        let izquierda: CGFloat = 82
        let derecha: CGFloat = 530
        cg.setStrokeColor(UIColor.black.cgColor)
        cg.setLineWidth(1.4)
        cg.move(to: CGPoint(x: izquierda, y: baseY))
        cg.addLine(to: CGPoint(x: 160, y: baseY))
        var x: CGFloat = 160
        for segmento in 0..<8 {
            let siguiente = x + 26
            let dy: CGFloat = segmento.isMultiple(of: 2) ? -10 : 10
            cg.addLine(to: CGPoint(x: siguiente, y: baseY + dy))
            x = siguiente
        }
        cg.addLine(to: CGPoint(x: derecha, y: baseY))
        cg.strokePath()

        let radio = 18 + CGFloat(indice % 7)
        cg.strokeEllipse(in: CGRect(
            x: bounds.midX - radio,
            y: baseY + 92 - radio,
            width: radio * 2,
            height: radio * 2))
        cg.move(to: CGPoint(x: bounds.midX, y: baseY + 92 - radio))
        cg.addLine(to: CGPoint(x: bounds.midX, y: baseY + 92 + radio))
        cg.strokePath()

        // Curvas vectoriales repetibles para que el PDF no sea sólo texto.
        cg.setLineWidth(1)
        for curva in 0..<6 {
            let offset = CGFloat(curva * 34)
            let ruta = UIBezierPath()
            ruta.move(to: CGPoint(x: 70, y: 420 + offset))
            ruta.addCurve(
                to: CGPoint(x: 540, y: 420 + offset),
                controlPoint1: CGPoint(
                    x: 210, y: 390 + offset + CGFloat(indice % 9)),
                controlPoint2: CGPoint(
                    x: 390, y: 450 + offset - CGFloat(indice % 11)))
            ruta.lineWidth = 1
            ruta.stroke()
        }

        let pie = "CORPUS-ID: \(corpusCanonicoId) · TOKEN: \(termino)-\(String(format: "%04d", pagina))"
        (pie as NSString).draw(
            at: CGPoint(x: 36, y: bounds.maxY - 28),
            withAttributes: [
                .font: UIFont.monospacedSystemFont(ofSize: 8, weight: .regular),
                .foregroundColor: UIColor.darkGray,
            ])
    }
}
