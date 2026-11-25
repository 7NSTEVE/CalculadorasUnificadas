import Foundation

enum ErrorCommitAutoguardado: LocalizedError {
    case destinoCanonicoAusente

    var errorDescription: String? {
        switch self {
        case .destinoCanonicoAusente:
            return "El autoguardado no está vinculado al archivo principal del cuaderno."
        }
    }
}

/// Ejecuta el commit durable en el único orden recuperable:
/// journal íntegro, archivo canónico y, al final, confirmación visual.
enum SecuenciaCommitAutoguardado {
    typealias PersistidorCanonico = (
        Data, @escaping (Result<Void, Error>) -> Void
    ) -> Void
    typealias PersistidorJournal = (
        @escaping (Result<URL, Error>) -> Void
    ) -> Void

    static func ejecutar(
        datosCanonicos: Data,
        persistidorJournal: @escaping PersistidorJournal,
        persistidorCanonico: PersistidorCanonico?,
        terminar: @escaping (Result<URL, Error>) -> Void
    ) {
        persistidorJournal { resultadoJournal in
            switch resultadoJournal {
            case .success(let urlJournal):
                guard let persistidorCanonico else {
                    terminar(.failure(ErrorCommitAutoguardado
                        .destinoCanonicoAusente))
                    return
                }
                persistidorCanonico(datosCanonicos) { resultadoCanonico in
                    switch resultadoCanonico {
                    case .success:
                        terminar(.success(urlJournal))
                    case .failure(let error):
                        terminar(.failure(error))
                    }
                }
            case .failure(let error):
                terminar(.failure(error))
            }
        }
    }
}
