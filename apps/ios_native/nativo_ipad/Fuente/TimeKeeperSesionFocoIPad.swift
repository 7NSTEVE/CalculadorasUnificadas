import Foundation

extension SesionFoco {
    var finProgramadoTimeKeeper: Date {
        inicio.addingTimeInterval(TimeInterval(max(duracionMinutos, 1) * 60))
    }

    func segundosRestantesTimeKeeper(ahora: Date = Date()) -> TimeInterval {
        guard activa else { return 0 }
        return max(0, finProgramadoTimeKeeper.timeIntervalSince(ahora))
    }

    func segundosTranscurridosTimeKeeper(ahora: Date = Date()) -> TimeInterval {
        let cierre = fin ?? min(ahora, finProgramadoTimeKeeper)
        return max(0, cierre.timeIntervalSince(inicio))
    }

    func vencidaTimeKeeper(ahora: Date = Date()) -> Bool {
        activa && ahora >= finProgramadoTimeKeeper
    }
}

/// Operaciones puras sobre la fuente de verdad existente `BaseAcademica.foco`.
/// No crea un segundo repositorio ni un segundo tipo de sesión.
enum TimeKeeperSesionFocoIPad {
    static func activa(en base: BaseAcademica, ahora: Date = Date()) -> SesionFoco? {
        base.foco.filter { $0.activa && !$0.vencidaTimeKeeper(ahora: ahora) }
            .max { $0.inicio < $1.inicio }
    }

    static func reconciliarExpiradas(_ base: inout BaseAcademica, ahora: Date = Date()) {
        for indice in base.foco.indices where base.foco[indice].vencidaTimeKeeper(ahora: ahora) {
            base.foco[indice].fin = base.foco[indice].finProgramadoTimeKeeper
            base.foco[indice].activa = false
        }
    }

    @discardableResult
    static func iniciar(
        en base: inout BaseAcademica,
        objetivo: String,
        minutos: Int,
        tareaId: UUID? = nil,
        ahora: Date = Date()
    ) -> SesionFoco {
        reconciliarExpiradas(&base, ahora: ahora)
        // Time Keeper transversal mantiene una sola sesión activa. Si existe
        // otra, se cierra explícitamente en vez de dejar dos cronómetros vivos.
        for indice in base.foco.indices where base.foco[indice].activa {
            base.foco[indice].fin = ahora
            base.foco[indice].activa = false
        }
        let sesion = SesionFoco(
            objetivo: objetivo.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty
                ? "Sesión de enfoque" : objetivo,
            inicio: ahora,
            duracionMinutos: min(max(minutos, 1), 12 * 60),
            tareaId: tareaId)
        base.foco.append(sesion)
        return sesion
    }

    @discardableResult
    static func terminar(
        _ id: UUID,
        en base: inout BaseAcademica,
        ahora: Date = Date()
    ) -> SesionFoco? {
        guard let indice = base.foco.firstIndex(where: { $0.id == id }) else { return nil }
        if base.foco[indice].activa {
            base.foco[indice].fin = min(ahora, base.foco[indice].finProgramadoTimeKeeper)
            base.foco[indice].activa = false
        }
        return base.foco[indice]
    }

    @discardableResult
    static func registrarInterrupcion(_ id: UUID, en base: inout BaseAcademica) -> Int? {
        guard let indice = base.foco.firstIndex(where: { $0.id == id }),
              base.foco[indice].activa else { return nil }
        base.foco[indice].interrupciones += 1
        return base.foco[indice].interrupciones
    }
}
