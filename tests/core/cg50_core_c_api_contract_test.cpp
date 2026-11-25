/* ==========================================================================
 * cg50_core_c_api_contract_test.cpp — oráculos de contrato F1 de la ABI C
 * Proyecto: Calculadora Unificada — edición nativa (CG50_ENGINE=native)
 *
 * Alcance de fase 1 (cg50_api_c.h + cabecera del shim): todo se ejercita
 * EXCLUSIVAMENTE vía cg50core_*; ninguna llamada directa al backend.
 * Oráculos heredados del contrato del backend (rama-tests...contract test):
 *   - NOP-ROM hermética (SystemROM 0x00B60000 exacta) -> READY, pc=kRomBase,
 *     sr=kResetSr, cpuState=Running, sin framebuffer fabricado.
 *   - Tick avanza instrucciones reales sin detener la máquina.
 *   - Reset conserva firmware activo y regressa a pc=kRomBase.
 *   - N-9: doble carga -> READY sin crash.
 * Comodines fail-closed de fase 1 pinados (cabecera del shim):
 *   - set_option -> ERR_ARG en instancia sin firmware (pin fase 1, intacto);
 *     con READY, SLEEP_ADVANCE y RTC_VIRTUAL entran al backend (fase 2/3) y
 *     las desconocidas siguen ERR_ARG.
 *   - save -> 0 y restore -> ERR_STATE en instancia sin firmware; con
 *     máquina READY la fase 3 los implementó de verdad (sección 13:
 *     sondeo/capacidad/roundtrip determinista/basura=ERR_STATE).
 *   - read_user_rom -> 0 sin UserROM (pin fase 1, intacto: la instancia
 *     vacía y la faulted sin User nunca tienen UserROM); con UserROM
 *     cargada la fase 4 lo implementó de verdad (sección 6b-bis: sondeo =
 *     CG50CORE_USER_ROM_SIZE, bytes reales, capacidad corta sin escribir).
 *   - load_firmware_raw: NULL/tamaño no exacto -> ERR_ARG; user/c2 no-NULL
 *     con tamaño exacto -> ERR_ARG declarado (fase 2 = InitializeParts);
 *     combinada 0x02000000 -> ERR_ARG (fail-closed por regla de cabecera).
 *   - last_error: protocolo dos llamadas con truncado limpio; nunca vacío.
 * ========================================================================== */

#include "cg50_api_c.h"
#include "cg50_native_core.h"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <vector>

namespace {

/* NOP por pares 0x0009 (SH-4A), idéntico al contrato del backend. */
void FillNops(std::vector<std::uint8_t>& rom)
{
    for (std::size_t i = 0; i + 1 < rom.size(); i += 2) {
        rom[i] = 0x00u;
        rom[i + 1] = 0x09u;
    }
}

void RequireOk(const char* what, bool ok)
{
    if(!ok){
        std::cerr << "FAIL: " << what << '\n';
        std::exit(1);
    }
}

} // namespace

int main()
{
    /* --- 0. Constantes de contrato (además de los asserts del header) --- */
    RequireOk("ABI version 0.1.0 (empaquetado May<<16|Men<<8|Parche)",
              CG50CORE_ABI_VERSION == 0x00000100u);
    RequireOk("SystemROM size exacta",
              CG50CORE_SYSTEM_ROM_SIZE == 0x00B60000u);
    RequireOk("UserROM size exacta", CG50CORE_USER_ROM_SIZE == 0x014A0000u);
    RequireOk("C2ROM size exacta", CG50CORE_C2_ROM_SIZE == 0x004A0000u);
    RequireOk("F1 = 0x070A", CG50CORE_KEY_FIRST_F1 == 0x070Au);

    /* --- 1. Ciclo de vida mínimo --------------------------------------- */
    cg50core_destroy(nullptr); /* no-op seguro */

    /* --- 2. Argumentos NULL sin firmware ------------------------------- */
    Cg50Core* empty = cg50core_create();
    RequireOk("create devuelve handle", empty != nullptr);
    RequireOk("estado inicial UNAVAILABLE",
              cg50core_state(empty) == CG50CORE_MACHINE_UNAVAILABLE);
    RequireOk("status(out NULL) es ERR_ARG",
              cg50core_status(empty, nullptr) == CG50CORE_ERR_ARG);
    RequireOk("state(NULL) es ERR_ARG",
              cg50core_state(nullptr) == CG50CORE_ERR_ARG);
    Cg50Status raw{};
    RequireOk("status vacía: cpu=Empty, sin ROM",
              cg50core_status(empty, &raw) == CG50CORE_OK &&
                  raw.cpuState == CG50CORE_CPU_EMPTY &&
                  raw.systemRomLoaded == 0 && raw.userRomLoaded == 0 &&
                  raw.faulted == 0 && raw.instructions == 0);
    RequireOk("key(state vacío) rechaza F1",
              cg50core_key(empty, CG50CORE_KEY_FIRST_F1, 1) == 0);
    RequireOk("key(NULL) rechaza", cg50core_key(nullptr, 0x070Au, 1) == 0);
    RequireOk("tick(NULL) es no-op", [&] { cg50core_tick(nullptr, 1000, 0); return true; }());
    RequireOk("reset_cpu(NULL) es no-op",
              [&] { cg50core_reset_cpu(nullptr); return true; }());
    Cg50FrameInfo finfo{};
    RequireOk("frame_info vacío = ERR_CAPACITY (no sintetiza)",
              cg50core_frame_info(empty, &finfo) == CG50CORE_ERR_CAPACITY);
    RequireOk("frame_copy vacío = 0",
              cg50core_frame_copy(empty, nullptr, 0) == 0);

    /* --- 3. Fase 1: set_option fail-closed, instancia sin cambios ------ */
    Cg50Status beforeOpt{};
    RequireOk("status antes option", cg50core_status(empty, &beforeOpt) == CG50CORE_OK);
    RequireOk("set_option desconocida (0x777) = ERR_ARG",
              cg50core_set_option(empty, 0x777u, 1) == CG50CORE_ERR_ARG);
    RequireOk("set_option SLEEP_ADVANCE (fase 1) = ERR_ARG",
              cg50core_set_option(empty, CG50CORE_OPT_SLEEP_ADVANCE, 1) ==
                  CG50CORE_ERR_ARG);
    RequireOk("set_option RTC_VIRTUAL (fase 1) = ERR_ARG",
              cg50core_set_option(empty, CG50CORE_OPT_RTC_VIRTUAL, 0) ==
                  CG50CORE_ERR_ARG);
    Cg50Status afterOpt{};
    RequireOk("status después option", cg50core_status(empty, &afterOpt) == CG50CORE_OK);
    RequireOk("set_option no alteró nada (regla 10)",
              std::memcmp(&beforeOpt, &afterOpt, sizeof(Cg50Status)) == 0);

    /* --- 4. Fase 1: save/restore/read_user_rom comodines declarados --- */
    RequireOk("save(NULL dst) fase 1 = 0",
              cg50core_save(empty, nullptr, 0) == 0);
    std::vector<std::uint8_t> sink(64, 0xCD);
    RequireOk("save(buffer) fase 1 = 0 (nada escrito)",
              cg50core_save(empty, sink.data(), sink.size()) == 0 &&
                  sink[0] == 0xCD && sink[sink.size() - 1] == 0xCD);
    RequireOk("restore(datos NULL) = ERR_ARG",
              cg50core_restore(empty, nullptr, 16) == CG50CORE_ERR_ARG);
    RequireOk("restore(size 0) = ERR_ARG",
              cg50core_restore(empty, sink.data(), 0) == CG50CORE_ERR_ARG);
    RequireOk("restore(blob admisible fase 1) = ERR_STATE",
              cg50core_restore(empty, sink.data(), sink.size()) ==
                  CG50CORE_ERR_STATE);
    RequireOk("read_user_rom fase 1 = 0",
              cg50core_read_user_rom(empty, nullptr, 0) == 0);

    /* --- 5. last_error: protocolo dos llamadas + truncado limpio ------ */
    const int required = cg50core_last_error(empty, nullptr, 0);
    RequireOk("last_error sondeo > 0 (nunca vacío)", required > 0);
    std::vector<char> text(static_cast<std::size_t>(required) + 1, 0x7F);
    const int copiedFull = cg50core_last_error(empty, text.data(), text.size());
    RequireOk("last_error copia completa", copiedFull == required);
    RequireOk("last_error terminador dentro de capacidad",
              text[static_cast<std::size_t>(copiedFull)] == '\0');
    char smallBuf[5];
    RequireOk("last_error buffer pequeño: 4 bytes + '\\0'",
              cg50core_last_error(empty, smallBuf, sizeof(smallBuf)) == 4 &&
                  smallBuf[4] == '\0');
    for (int i = 0; i < 4; ++i) {
        RequireOk("truncado limpio preserva prefijo",
                  smallBuf[i] == text[static_cast<std::size_t>(i)]);
    }
    RequireOk("last_error(NULL core) = ERR_ARG",
              cg50core_last_error(nullptr, nullptr, 0) == CG50CORE_ERR_ARG);

    /* --- 6. load_firmware_raw: válido SystemROM sola ------------------ */
    std::vector<std::uint8_t> rom(Cg50NativeCore::kRomSize);
    FillNops(rom);
    RequireOk("load system NULL = ERR_ARG",
              cg50core_load_firmware_raw(empty, nullptr, CG50CORE_SYSTEM_ROM_SIZE,
                                         nullptr, 0, nullptr, 0) ==
                  CG50CORE_ERR_ARG);
    RequireOk("load system corta = ERR_ARG",
              cg50core_load_firmware_raw(empty, rom.data(), 16, nullptr, 0,
                                         nullptr, 0) == CG50CORE_ERR_ARG);
    RequireOk("load combinada 0x02000000 NO se acepta (regla cabecera)",
              cg50core_load_firmware_raw(empty, rom.data(),
                                         Cg50NativeCore::kCombinedRomSize,
                                         nullptr, 0, nullptr, 0) ==
                  CG50CORE_ERR_ARG);
    std::vector<std::uint8_t> userZero(CG50CORE_USER_ROM_SIZE, 0);
    std::vector<std::uint8_t> c2Zero(CG50CORE_C2_ROM_SIZE, 0);
    RequireOk("user con tamaño inexacto = ERR_ARG (antes de la fase)",
              cg50core_load_firmware_raw(empty, rom.data(), rom.size(),
                                         userZero.data(), 8, nullptr, 0) ==
                  CG50CORE_ERR_ARG);
    RequireOk("c2 con tamaño inexacto = ERR_ARG (antes de la fase)",
              cg50core_load_firmware_raw(empty, rom.data(), rom.size(),
                                         nullptr, 0, c2Zero.data(), 4) ==
                  CG50CORE_ERR_ARG);
    RequireOk("estado tras todos los rechazos sigue UNAVAILABLE",
              cg50core_state(empty) == CG50CORE_MACHINE_UNAVAILABLE);

    /* --- 6b. fase 2 (2026-11-02): el trío entra por InitializeParts --- */
    RequireOk("load trío user+c2 (NOP) = OK (fase 2 InitializeParts)",
              cg50core_load_firmware_raw(empty, rom.data(), rom.size(),
                                         userZero.data(), userZero.size(),
                                         c2Zero.data(), c2Zero.size()) ==
                  CG50CORE_OK);
    RequireOk("estado READY tras carga del trío",
              cg50core_state(empty) == CG50CORE_MACHINE_READY);
    RequireOk("load user sin c2 = OK (fase 2 InitializeParts)",
              cg50core_load_firmware_raw(empty, rom.data(), rom.size(),
                                         userZero.data(), userZero.size(),
                                         nullptr, 0) == CG50CORE_OK);
    RequireOk("estado READY tras user sin c2",
              cg50core_state(empty) == CG50CORE_MACHINE_READY);

    /* --- 6b-bis fase 4 (2026-11-03): read_user_rom con User cargada ---- */
    /* Persistencia opt-in del host: sondeo = tamaño exacto; la UserROM
     * cargada aquí es userZero (cero binario). Capacidad corta NO escribe. */
    RequireOk("read_user_rom sondeo = tamaño exacto de la UserROM",
              cg50core_read_user_rom(empty, nullptr, 0) ==
                  CG50CORE_USER_ROM_SIZE);
    std::vector<std::uint8_t> userLeido(CG50CORE_USER_ROM_SIZE, 0xAB);
    RequireOk("read_user_rom copia los bytes reales (todos 0 aquí)",
              cg50core_read_user_rom(empty, userLeido.data(),
                                     userLeido.size()) ==
                  CG50CORE_USER_ROM_SIZE &&
                  std::all_of(userLeido.begin(), userLeido.end(),
                              [](std::uint8_t b) { return b == 0x00; }));
    std::vector<std::uint8_t> userChico(1024, 0x77);
    RequireOk("read_user_rom capacidad corta = requerido sin escribir",
              cg50core_read_user_rom(empty, userChico.data(),
                                     userChico.size()) ==
                  CG50CORE_USER_ROM_SIZE &&
                  std::all_of(userChico.begin(), userChico.end(),
                              [](std::uint8_t b) { return b == 0x77; }));

    RequireOk("load SystemROM NOP exacta = OK",
              cg50core_load_firmware_raw(empty, rom.data(), rom.size(), nullptr,
                                         0, nullptr, 0) == CG50CORE_OK);
    RequireOk("estado READY tras carga",
              cg50core_state(empty) == CG50CORE_MACHINE_READY);

    /* --- 6c. fase 2 (2026-11-03): set_option con máquina READY --------- */
    /* Gate por estado (regla 10): en fase 1 (sin firmware) toda opción
     * seguía ERR_ARG (pin L98); con READY, SLEEP_ADVANCE (opt 1) sí se
     * aplica por Cg50Backend::SetOption desde fase 2 y RTC_VIRTUAL (opt 2)
     * desde fase 3 (su bandera vive en el core nativo y se re-aplica en
     * cada re-carga; la prueba de sí en la sección 13). Desconocidas
     * siguen fail-closed. */
    RequireOk("set_option desconocida con READY = ERR_ARG",
              cg50core_set_option(empty, 0x777u, 1) == CG50CORE_ERR_ARG);
    RequireOk("set_option SLEEP_ADVANCE con READY = OK (fase 2)",
              cg50core_set_option(empty, CG50CORE_OPT_SLEEP_ADVANCE, 1) ==
                  CG50CORE_OK);
    RequireOk("set_option no alteró estado ni pila de ejecución (regla 10)",
              cg50core_state(empty) == CG50CORE_MACHINE_READY);
    RequireOk("set_option SLEEP_ADVANCE = 0 con READY = OK (sticky apagado)",
              cg50core_set_option(empty, CG50CORE_OPT_SLEEP_ADVANCE, 0) ==
                  CG50CORE_OK);

    Cg50Status loaded{};
    RequireOk("status tras carga",
              cg50core_status(empty, &loaded) == CG50CORE_OK);
    RequireOk("pc = kRomBase tras carga", loaded.pc == Cg50NativeCore::kRomBase);
    RequireOk("sr = kResetSr tras carga", loaded.sr == Cg50NativeCore::kResetSr);
    RequireOk("cpuState = Running tras carga",
              loaded.cpuState == CG50CORE_CPU_RUNNING);
    RequireOk("systemRomLoaded = 1", loaded.systemRomLoaded == 1);
    RequireOk("userRomLoaded = 0 (no se tocó user)", loaded.userRomLoaded == 0);
    RequireOk("framebufferAvailable = 0 (R61524 sin GRAM)",
              loaded.framebufferAvailable == 0);
    RequireOk("faulted = 0", loaded.faulted == 0);
    RequireOk("layout Cg50Status por offsetof (pc@8)",
              [&] {
                  return *reinterpret_cast<const std::uint32_t*>(
                             reinterpret_cast<const std::uint8_t*>(&loaded) + 8) ==
                         Cg50NativeCore::kRomBase;
              }());

    /* --- 7. tick: presupuesto automático y progreso real --------------- */
    cg50core_tick(empty, 1000, 0); /* budget interior min(1000*60, 2.5M) */
    Cg50Status afterTick{};
    RequireOk("status tras tick", cg50core_status(empty, &afterTick) == CG50CORE_OK);
    RequireOk("READY tras tick NOP", cg50core_state(empty) == CG50CORE_MACHINE_READY);
    RequireOk("instructions avanzaron", afterTick.instructions > 0);
    RequireOk("cycles avanzaron", afterTick.cycles > 0);
    RequireOk("pc avanzó (> kRomBase)", afterTick.pc > Cg50NativeCore::kRomBase);
    RequireOk("sin fault flags con NOP",
              afterTick.memoryFaultSeen == 0 &&
                  afterTick.unsupportedOpcodeSeen == 0 &&
                  afterTick.faulted == 0);
    RequireOk("cpuState sigue Running tras tick",
              afterTick.cpuState == CG50CORE_CPU_RUNNING);

    /* max_instructions != 0 en fase 1: progreso gana (no-honra declarada,
     * registrada en handle; fase 2 = Step(µs,max)). */
    cg50core_tick(empty, 50, 1234);
    Cg50Status afterTickMax{};
    RequireOk("status tras tick con max", cg50core_status(empty, &afterTickMax) == CG50CORE_OK);
    RequireOk("tick con max!=0 sigue avanzando",
              afterTickMax.instructions > afterTick.instructions);

    /* --- 8. key: ciclo down/up del host sobre библиот Ready --------- */
    RequireOk("F1 down aceptada", cg50core_key(empty, 0x070Au, 1) == 1);
    RequireOk("F1 up aceptada", cg50core_key(empty, 0x070Au, 0) == 1);
    /* Nota: no se re-libera una tecla ya liberada (el ciclo lo gobierna el
     * host, regla 9; el contrato del backend solo ejercita el par down/up). */

    /* --- 9. frame_info/frame_copy tras carga NOP (sin GRAM) ----------- */
    RequireOk("frame_info tras NOP carga = ERR_CAPACITY",
              cg50core_frame_info(empty, &finfo) == CG50CORE_ERR_CAPACITY);
    std::vector<std::uint32_t> pix(16, 0x11223344u);
    RequireOk("frame_copy sin GRAM = 0 y buffer intacto",
              cg50core_frame_copy(empty, pix.data(), pix.size()) == 0 &&
                  pix[0] == 0x11223344u);
    RequireOk("frame_copy sondeo sin GRAM = 0",
              cg50core_frame_copy(empty, nullptr, 0) == 0);

    /* --- 10. reset_cpu: debugger reset que conserva firmware ---------- */
    cg50core_reset_cpu(empty);
    RequireOk("READY tras reset", cg50core_state(empty) == CG50CORE_MACHINE_READY);
    Cg50Status afterReset{};
    RequireOk("status tras reset", cg50core_status(empty, &afterReset) == CG50CORE_OK);
    RequireOk("pc de vuelta a kRomBase", afterReset.pc == Cg50NativeCore::kRomBase);
    RequireOk("instructions a cero", afterReset.instructions == 0);
    RequireOk("cpuState Running tras reset",
              afterReset.cpuState == CG50CORE_CPU_RUNNING);

    /* --- 11. N-9: doble carga en la misma instancia ------------------- */
    RequireOk("segunda carga OK (N-9)",
              cg50core_load_firmware_raw(empty, rom.data(), rom.size(), nullptr,
                                         0, nullptr, 0) == CG50CORE_OK);
    RequireOk("READY tras doble carga", cg50core_state(empty) == CG50CORE_MACHINE_READY);
    cg50core_tick(empty, 100, 0);
    RequireOk("READY tras tick de sanity",
              cg50core_state(empty) == CG50CORE_MACHINE_READY);

    /* --- 12. fallo honesto: opcode no soportado -> FAULTED ------------ */
    /* Nota H-003 (SH7305 real): 0xFFFF pertenece al espacio 0xFxxx sin FPU y
     * genera EXPEVT 0x180 (General Illegal Instruction) en vez de detener la
     * maquina; usamos 0x3001 (espacio 0x3nm1 no asignado en SH-4A) para
     * acreditar el contrato CG50CORE_MACHINE_FAULTED sin romper fidelidad SH7305. */
    std::vector<std::uint8_t> bad(rom.size());
    FillNops(bad);
    bad[0] = 0x30u;
    bad[1] = 0x01u;
    RequireOk("carga ROM 0x3001 = OK (estructuralmente válida)",
              cg50core_load_firmware_raw(empty, bad.data(), bad.size(), nullptr,
                                         0, nullptr, 0) == CG50CORE_OK);
    cg50core_tick(empty, 1000, 0);
    RequireOk("estado FAULTED tras opcode no soportado",
              cg50core_state(empty) == CG50CORE_MACHINE_FAULTED);
    Cg50Status faulted{};
    RequireOk("status tras fault",
              cg50core_status(empty, &faulted) == CG50CORE_OK);
    RequireOk("telemetría de fallo honesta",
              faulted.cpuState == CG50CORE_CPU_HALTED &&
                  faulted.unsupportedOpcodeSeen == 1 &&
                  faulted.unsupportedOpcode == 0x3001u &&
                  faulted.faulted == 1);
    RequireOk("diagnóstico superviviente al fallo (nunca vacío)",
              cg50core_last_error(empty, nullptr, 0) > 0);
    RequireOk("save fase 1 sigue = 0 en faulted",
              cg50core_save(empty, nullptr, 0) == 0);
    RequireOk("read_user_rom fase 1 sigue = 0 en faulted",
              cg50core_read_user_rom(empty, nullptr, 0) == 0);

    /* --- 13. fase 3 (2026-11-03): save/restore por bytes (real) ------- */
    /* La sección 12 dejó la máquina FAULTED: se vuelve a READY con la ROM
     * válida y el roundtrip corre sobre Cg50NativeCore::ExportState/
     * ImportState vía la ABI (79 chunks + bus, kStateVersion=2). */
    RequireOk("carga ROM válida (recover para fase 3)",
              cg50core_load_firmware_raw(empty, rom.data(), rom.size(), nullptr,
                                         0, nullptr, 0) == CG50CORE_OK);
    cg50core_tick(empty, 120, 0); /* progreso determinista antes del save */
    const size_t requerido = cg50core_save(empty, nullptr, 0);
    RequireOk("save sondea tamaño > 0 (READY)", requerido > 0);
    std::vector<std::uint8_t> blobA(requerido, 0x5A);
    RequireOk("save escribe exactamente el tamaño sondeado",
              cg50core_save(empty, blobA.data(), blobA.size()) == requerido);
    std::vector<std::uint8_t> blobB(requerido, 0xA5);
    RequireOk("save es determinista (dos pasadas, mismo blob)",
              cg50core_save(empty, blobB.data(), blobB.size()) == requerido &&
                  std::memcmp(blobA.data(), blobB.data(), requerido) == 0);
    std::vector<std::uint8_t> chico(4096, 0xC3);
    RequireOk(
        "capacidad corta: NO escribe nada y devuelve el requerido",
        cg50core_save(empty, chico.data(), chico.size()) == requerido &&
            std::all_of(chico.begin(), chico.end(),
                        [](std::uint8_t b) { return b == 0xC3; }));
    Cg50Status postSave{};
    RequireOk("status tras los saves",
              cg50core_status(empty, &postSave) == CG50CORE_OK);
    RequireOk("READY tras los saves",
              cg50core_state(empty) == CG50CORE_MACHINE_READY);

    Cg50Core* espejo = cg50core_create();
    RequireOk("segunda instancia creada", espejo != nullptr);
    RequireOk("carga en espejo OK",
              cg50core_load_firmware_raw(espejo, rom.data(), rom.size(),
                                         nullptr, 0, nullptr, 0) ==
                  CG50CORE_OK);
    cg50core_tick(espejo, 5000, 0); /* estado propio, para probar overwrite */
    RequireOk("restore blob en espejo = OK",
              cg50core_restore(espejo, blobA.data(), blobA.size()) ==
                  CG50CORE_OK);
    std::vector<std::uint8_t> blobC(requerido, 0x33);
    RequireOk("restore seguido de save = bytes idénticos (FR2 en la ABI)",
              cg50core_save(espejo, blobC.data(), blobC.size()) == requerido &&
                  std::memcmp(blobA.data(), blobC.data(), requerido) == 0);
    Cg50Status restaurado{};
    RequireOk("status del espejo restaurado",
              cg50core_status(espejo, &restaurado) == CG50CORE_OK);
    RequireOk("estado restaurado alineado con el origen",
              restaurado.pc == postSave.pc && restaurado.sr == postSave.sr);
    std::vector<std::uint8_t> cochino(requerido, 0xEE);
    RequireOk("restore blob basura = ERR_STATE",
              cg50core_restore(espejo, cochino.data(), cochino.size()) ==
                  CG50CORE_ERR_STATE);
    RequireOk("restore truncado (20 B) = ERR_STATE",
              cg50core_restore(espejo, blobA.data(), 20) == CG50CORE_ERR_STATE);
    cg50core_destroy(espejo);
    espejo = nullptr;

    /* set_option RTC_VIRTUAL (fase 3): con READY entra al core */
    RequireOk("set_option RTC_VIRTUAL (0→1) con READY = OK",
              cg50core_set_option(empty, CG50CORE_OPT_RTC_VIRTUAL, 1) ==
                  CG50CORE_OK);
    RequireOk("set_option RTC_VIRTUAL (1→0) con READY = OK",
              cg50core_set_option(empty, CG50CORE_OPT_RTC_VIRTUAL, 0) ==
                  CG50CORE_OK);
    RequireOk("READY tras opciones RTC",
              cg50core_state(empty) == CG50CORE_MACHINE_READY);

    /* --- 14. destrucción con instancia viva (regla 2: sin allocations) -- */
    cg50core_destroy(empty);
    empty = nullptr;
    /* Destroy de un handle ya destruido es UB por regla 2: NO se ejercita.
     * En su lugar: un ciclo completo create->destroy una vez más. */
    Cg50Core* fresh = cg50core_create();
    RequireOk("create tras destroy funciona", fresh != nullptr);
    cg50core_destroy(fresh);
    fresh = nullptr;

    std::cout << "cg50_core_c_api_contract OK (fase 4: "
              << "trío por InitializeParts; save/restore por bytes y RTC "
              << "virtual con READY; read_user_rom real con UserROM; "
              << "pines fail-closed sin firmware intactos)\n";
    return 0;
}
