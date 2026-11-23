/* ==========================================================================
 * cg50_battery.cpp �?" batería de cálculo REAL de la fx-CG50 sobre la ABI C
 * Proyecto: TintaEstudio �?" rama cg50-nativo-ios (2026-11-03)
 *
 * MANDATO del dueño (2026-11-03): acreditar que la calculadora WINDOWS hace
 * TODOS los cálculos antes de cualquier traslado a iOS. Aquí no vale
 * «la pantalla cambió»: el OS real de Casio (guest) EJECUTA las expresiones
 * tecleadas por la matriz real del teclado y el resultado se decodifica
 * desde el framebuffer con glifos calibrados POR FUENTE contra el mismo
 * guest. La identidad de cada glifo de calibración es la tecla NUESTRA
 * (dato del anfitrión), no una salida del motor; y las cadenas esperadas
 * son de una referencia independiente documentada (aritmética/p hexadecimal
 * de doble precisión verificada fuera de este proyecto) �?" el motor jamás
 * genera el valor con el que se compara.
 *
 * Ruta por caso (RUN-MAT, defaults del OS 3.60 tras boot: RAD + MathIO):
 *   [AC] <teclas expresión> [EXE] �?" resultado alineado a la derecha.
 *
 * Recibos: BMP de cada corrida en outdir (c_<caso>_final1/_final2),
 * hashes FNV-1a, determinismo de repetición (hash1==hash2) y texto
 * decodificado vs esperado. Resumen por stderr + outdir/battery_result.txt.
 *
 * Referencias (doble precisión, doc del anfitrión):
 *   2�-3=6 · 1+1=2 · 123456789+987654321=1111111110
 *   7÷13=0.5384615384615384�?��?' 10 sig. = 0.5384615385 (redondeo al 5)
 *   (3+4)�-5=35 · 2^3=8 · sin(2) RAD=0.9092974268256817�?��?' 0.9092974268
 *   1÷3=0.3333333333 (=10 sig, sin dígito discriminante)
 *   Σ(x²,x,1,10)=385 · d/dx sin(x)|x=2 = cos(2) = -0.4161468365
 *   �^�x³dx de 0 a 3 = 81/4 (=20.25; el CG50 lo dibuja fracción 81/4,
 *     numerador y denominador en bandas separadas por la barra)
 *   (3+4i)^5 = -237-3116i (1344-1581 = -237; 2108+1008 = 3116)
 *
 * v10 (2026-11-03): sumatorio corregido �?" las casillas del template Σ(
 * se navegan con DERECHA (kDer), no con comas (la corrida v9 dejó el
 * template sin llenar: bandas [??][???]). La integral ya calculaba
 * bien (81/4 en pantalla, verificado contra 81/4=20.25): la v10 añade
 * matcheo estructural de fracciones MathIO (numerador = banda i,
 * denominador = banda i+1; la barra la excluye FilaConTexto).
 *
 * Uso:
 *   cg50_battery.exe <SystemROM> <UserROM> <C2ROM> <outdir|-> [segBoot]
 *
 * Salidas:
 *   0  TODOS los casos verdes (v10 objetivo: 12/12)
 *   7  parciales/divergencias con decodificación usable (no es victoria)
 *   6  calibración/decodificación sin texto usable (infra rota)
 *   3  carga/boot rechazada o máquina detenida
 *   4  arranque sin GRAM en el plazo
 *   2  ROM faltante o tamaño no exacto
 * ========================================================================== */

#include "cg50_api_c.h"

#include <cinttypes>
#include <cstdint>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>
#include <utility>
#include <vector>

namespace fs = std::filesystem;

namespace {

constexpr std::uint32_t kChunkUs = 16667;   /* 60 fps.                        */
constexpr int kAncho = 384;
constexpr int kAlto = 216;
constexpr int kPixeles = kAncho * kAlto;

constexpr std::size_t kTamSystem = 0x00B60000u;
constexpr std::size_t kTamUser = 0x014A0000u;
constexpr std::size_t kTamC2 = 0x004A0000u;

/* Códigos canónicos del teclado (cg50_keys_exact.h, 50 únicos).          */
constexpr std::uint16_t kMenu      = 0x0409;  /* id 9.                     */
constexpr std::uint16_t kExe       = 0x0302;  /* id 49.                    */
constexpr std::uint16_t kAcOn      = 0x0101;  /* id 34 AC/ON.              */
constexpr std::uint16_t kExit      = 0x0408;  /* id 17 EXIT.               */
constexpr std::uint16_t kShift     = 0x0709;  /* id 6 SHIFT.               */
constexpr std::uint16_t kOptn      = 0x0609;  /* id 7 OPTN.                */
constexpr std::uint16_t kF1        = 0x070A;  /* id 0.                     */
constexpr std::uint16_t kF2        = 0x060A;  /* id 1.                     */
constexpr std::uint16_t kF3        = 0x050A;  /* id 2.                     */
constexpr std::uint16_t kF4        = 0x040A;  /* id 3.                     */
constexpr std::uint16_t kXVar      = 0x0707;  /* id 18 X,θ,T.              */
constexpr std::uint16_t kX2        = 0x0608;  /* id 15 x².                 */
constexpr std::uint16_t kComa      = 0x0306;  /* id 28 ','.                */
constexpr std::uint16_t kMenos     = 0x0303;  /* id 44 '�^''.                */
constexpr std::uint16_t kDer       = 0x0208;  /* id 12 DERECHA (navega
                                               * casillas de plantillas).  */
constexpr std::uint16_t kArr       = 0x0209;  /* id 10 ARRIBA.             */
constexpr std::uint16_t kAba       = 0x0308;  /* id 13 ABAJO.              */
constexpr std::uint16_t kF6        = 0x020A;  /* id 5.                     */
constexpr std::uint16_t kPotCaret  = 0x0508;  /* id 16 '^'.                */
constexpr std::uint16_t kParenA    = 0x0506;  /* id 26 '('.                */
constexpr std::uint16_t kParenC    = 0x0406;  /* id 27 ')'.                */
constexpr std::uint16_t kSin       = 0x0407;  /* id 21 'sin('.             */
constexpr std::uint16_t kPor       = 0x0404;  /* id 38 '�-'.                */
constexpr std::uint16_t kEntre     = 0x0304;  /* id 39 '÷'.                */
constexpr std::uint16_t kMas       = 0x0403;  /* id 43 '+'.                */
constexpr std::uint16_t kDigSiete  = 0x0705;  /* id 30.                    */
constexpr std::uint16_t kDigOcho   = 0x0605;  /* id 31.                    */
constexpr std::uint16_t kDigNueve  = 0x0505;  /* id 32.                    */
constexpr std::uint16_t kDigCuatro = 0x0704;  /* id 35.                    */
constexpr std::uint16_t kDigCinco  = 0x0604;  /* id 36.                    */
constexpr std::uint16_t kDigSeis   = 0x0504;  /* id 37.                    */
constexpr std::uint16_t kDigUno    = 0x0703;  /* id 40.                    */
constexpr std::uint16_t kDigDos    = 0x0603;  /* id 41.                    */
constexpr std::uint16_t kDigTres   = 0x0503;  /* id 42.                    */
constexpr std::uint16_t kDigCero   = 0x0702;  /* id 45.                    */
constexpr std::uint16_t kPunto     = 0x0602;  /* id 46.                    */
constexpr std::uint16_t kExpE      = 0x0502;  /* id 47 (inserta '�?').      */
constexpr std::uint16_t kLog       = 0x0607;  /* id 19 'log('.             */
constexpr std::uint16_t kLn        = 0x0507;  /* id 20 'ln('.              */
constexpr std::uint16_t kFrac      = 0x0706;  /* id 24 '␣/␣' (frac MathIO). */
constexpr std::uint16_t kSD        = 0x0606;  /* id 25 S<->D (tecla propia,
                                               * sin SHIFT).               */
constexpr std::uint16_t kCos       = 0x0307;  /* id 40 'cos('.             */
constexpr std::uint16_t kTan       = 0x0207;  /* id 41 'tan('.             */
constexpr std::uint16_t kMenosU    = 0x0402;  /* id 40? '-' unario (neg).  */
constexpr std::uint16_t kF5        = 0x030A;  /* id 4.                     */

bool LeeRomExacta(const char* ruta, std::size_t esperado,
                  std::vector<std::uint8_t>* salida)
{
    if (ruta == nullptr || std::strcmp(ruta, "-") == 0) return true;
    std::FILE* fichero = std::fopen(ruta, "rb");
    if (fichero == nullptr) {
        std::fprintf(stderr, "[BATTERY] no abrio %s\n", ruta);
        return false;
    }
    if (std::fseek(fichero, 0L, SEEK_END) != 0) {
        std::fclose(fichero);
        return false;
    }
    const long bytes = std::ftell(fichero);
    if (bytes < 0 || static_cast<std::size_t>(bytes) != esperado) {
        std::fclose(fichero);
        std::fprintf(stderr, "[BATTERY] %s: %ld bytes != %zu\n",
                     ruta, bytes, esperado);
        return false;
    }
    std::rewind(fichero);
    salida->resize(esperado);
    if (std::fread(salida->data(), 1, esperado, fichero) != esperado) {
        std::fclose(fichero);
        return false;
    }
    std::fclose(fichero);
    return true;
}

std::uint64_t Fnv1a(const std::uint32_t* pixeles, std::size_t n)
{
    std::uint64_t h = 0xcbf29ce484222325ULL;
    for (std::size_t i = 0; i < n; ++i) {
        h ^= static_cast<std::uint64_t>(pixeles[i]);
        h *= 0x00000100000001b3ULL;
    }
    return h;
}

bool EscribeBmpBgr(const char* dir, const char* nombre,
                   const std::uint32_t* pixels)
{
    if (dir == nullptr || nombre == nullptr ||
        std::strcmp(dir, "-") == 0) return true;
    std::error_code ec;
    fs::create_directories(dir, ec);
    const std::string ruta =
        (fs::path(dir) / (std::string(nombre) + ".bmp")).string();
    std::FILE* f = std::fopen(ruta.c_str(), "wb");
    if (f == nullptr) return false;
    constexpr std::uint32_t fila = kAncho * 3;
    constexpr std::uint32_t bytesPixeles = fila * kAlto;
    std::uint8_t enc[54] = {};
    enc[0] = 'B'; enc[1] = 'M';
    const std::uint32_t tamano = 54 + bytesPixeles;
    std::memcpy(enc + 2, &tamano, 4);
    enc[10] = 54; enc[14] = 40;
    const std::int32_t ancho = kAncho, alto = kAlto;
    std::memcpy(enc + 18, &ancho, 4);
    std::memcpy(enc + 22, &alto, 4);
    enc[26] = 1; enc[28] = 24;
    std::memcpy(enc + 34, &bytesPixeles, 4);
    bool ok = std::fwrite(enc, 1, 54, f) == 54;
    for (int fileFila = 0; ok && fileFila < kAlto; ++fileFila) {
        /* La entrada es la vista ORIENTADA al display (g_disp); el BMP
         * 24-bit es bottom-up: la fila 0 del fichero es la inferior.      */
        const int filaFuente = kAlto - 1 - fileFila;
        std::uint8_t buf[kAncho * 3];
        for (int x = 0; x < kAncho; ++x) {
            const std::uint32_t p = pixels[filaFuente * kAncho + x];
            buf[x * 3 + 0] = static_cast<std::uint8_t>(p & 0xFF);
            buf[x * 3 + 1] = static_cast<std::uint8_t>((p >> 8) & 0xFF);
            buf[x * 3 + 2] = static_cast<std::uint8_t>((p >> 16) & 0xFF);
        }
        if (std::fwrite(buf, 1, fila, f) != fila) ok = false;
    }
    std::fclose(f);
    return ok;
}

/* ---------------------------------------------------------------- guest */

std::uint32_t g_pixels[kPixeles];
/* Vista ORIENTADA al display: el frame de la ABI sale rotado 180° respecto
 * de la pantalla física (panel R61524 montado al revés; el smoke v7 lo
 * documentó con diag_rot180). Toda la decodificación trabaja aquí.        */
std::uint32_t g_disp[kPixeles];
Cg50Core* g_core = nullptr;
const char* g_dir = nullptr;

bool CopiaPantalla()
{
    Cg50FrameInfo info{};
    if (cg50core_frame_info(g_core, &info) != CG50CORE_OK) return false;
    if (cg50core_frame_copy(g_core, g_pixels, kPixeles) != kPixeles) {
        return false;
    }
    for (int fila = 0; fila < kAlto; ++fila) {
        const std::uint32_t* fuente = g_pixels + fila * kAncho;
        std::uint32_t* destino = g_disp + (kAlto - 1 - fila) * kAncho;
        for (int col = 0; col < kAncho; ++col) {
            destino[kAncho - 1 - col] = fuente[col];
        }
    }
    return true;
}

std::uint64_t HashPantalla()
{
    return CopiaPantalla() ? Fnv1a(g_pixels, kPixeles) : 0;
}

void Tic(int cuantos)
{
    for (int i = 0; i < cuantos; ++i) cg50core_tick(g_core, kChunkUs, 0);
}

/* Espera de estabilidad: avanza hasta que el hash no cambie durante
 * «estables» chunks seguidos (cap «capChunks»). La corrida v6 demostró la
 * carrera: capturar sin estabilidad picó frames a medio repintar (el '4'
 * calibró como un fragmento de 5x5).                                      */
std::uint64_t EstabilizarPantalla(int estables, int capChunks)
{
    std::uint64_t h = HashPantalla();
    int sinCambio = 0;
    for (int i = 0; i < capChunks && sinCambio < estables; ++i) {
        Tic(1);
        const std::uint64_t ahora = HashPantalla();
        if (ahora == h && ahora != 0) ++sinCambio;
        else { h = ahora; sinCambio = 0; }
    }
    return h;
}

/* Pulsar: down + hold + up + asentar. 0 = tecla rechazada.              */
std::uint64_t Pulso(std::uint16_t codigo, int msHold)
{
    const int chunks = msHold * 1000 / static_cast<int>(kChunkUs);
    if (cg50core_key(g_core, codigo, 1) != 1) {
        std::fprintf(stderr, "[BATTERY] tecla 0x%04X RECHAZADA (estado=%d)\n",
                     codigo, cg50core_state(g_core));
        return 0;
    }
    Tic(chunks);
    cg50core_key(g_core, codigo, 0);
    Tic(chunks);
    return HashPantalla();
}

/* Espera a que la pantalla reaccione a la tecla (hasta maxMsExtra).     */
std::uint64_t PulsoConEspera(std::uint16_t codigo, int msHold, int maxMsExtra)
{
    const std::uint64_t antes = HashPantalla();
    if (Pulso(codigo, msHold) == 0) return 0;
    const int chunks = maxMsExtra * 1000 / static_cast<int>(kChunkUs);
    std::uint64_t ahora = HashPantalla();
    for (int i = 0; i < chunks && ahora == antes; ++i) {
        Tic(1);
        ahora = HashPantalla();
    }
    return ahora;
}

/* ------------------------------------------------- bandas y glifos ---- */

/* Franja del cuerpo: debajo de la barra de estado (0..20) y encima de la
 * franja de pestañas F (JUMP/DELETE/... que vive en ~190..215; la corrida
 * v2 la capturo como «glifos» de 62x22 @x1 �?" no son glifos).              */
constexpr int kCuerpoTop = 21;
constexpr int kCuerpoFondo = 187;
/* Geometria medida sobre la captura v2 (c_2x3_final1.bmp): el resultado
 * «6» ocupa x362..374; en x378/382-383 hay un borde vertical NEGRO de
 * ~165 filas (une todas las bandas) y en x380-381 la barra azul. Se
 * analiza hasta x375.                                                    */
constexpr int kColTexto = 376;

bool PixelOscuro(int fila, int col)
{
    const std::uint32_t p = g_disp[fila * kAncho + col];
    const int r = static_cast<int>((p >> 16) & 0xFF);
    const int g = static_cast<int>((p >> 8) & 0xFF);
    const int b = static_cast<int>(p & 0xFF);
    /* Azul dominante = cromado de UI (scroll, batería): no es texto. La
     * corrida v4 lo demostró: los resultados se alinean hasta x�?^378 y un
     * límite de columnas los convertía en astillas de 5x1.               */
    if (b > 2 * r + 40 && b > 2 * g + 40) return false;
    const int luma = (r * 299 + g * 587 + b * 114) / 1000;
    return luma < 128;
}

/* ¿La fila tiene texto (oscuridad con huecos de luz)? Las filas sólidas
 * (reglas/bordes) se rechazan: no son glifos.                             */
bool FilaConTexto(int fila)
{
    bool hay = false, hayLuz = false;
    /* v24b (forense modexp02): +2 columnas (376-377; la barra de scroll
     * empieza en 378). El '4' right-aligned termina su barra horizontal
     * SOLIDA en x375: sin margen, esta funcion la rechaza como fila
     * solida y PARTE el glifo en dos bandas ([?] top + [?] palo - los
     * bits del 4 eran identicos a la calibracion; el defecto era del
     * extractor, no del glifo). Solo cambia filas cuyo recorrido oscuro
     * toca el borde: ninguno de los 97 pasantes termina en '4'.       */
    for (int col = 0; col < kColTexto + 2 && col < kAncho; ++col) {
        if (PixelOscuro(fila, col)) hay = true;
        else if (hay) hayLuz = true;
    }
    return hay && hayLuz;
}

struct BandaTexto { int f0 = -1, f1 = -1; };

/* Grupos contiguos de filas con texto dentro de [top..fondo], de arriba
 * hacia abajo.                                                            */
std::vector<BandaTexto> ExtraerBandas()
{
    std::vector<BandaTexto> bandas;
    bool enBanda = false;
    for (int fila = kCuerpoTop; fila <= kCuerpoFondo; ++fila) {
        const bool texto = FilaConTexto(fila);
        if (texto && !enBanda) {
            bandas.push_back(BandaTexto{fila, fila});
            enBanda = true;
        } else if (texto && enBanda) {
            bandas.back().f1 = fila;
        } else if (!texto) {
            enBanda = false;
        }
    }
    return bandas;
}

/* Localiza la banda de la línea de entrada viva: la �sLTIMA banda de texto
 * del cuerpo (la historia crece hacia abajo; el símbolo recién tecleado
 * queda en la banda más baja, con el cursor a su derecha).                */
bool BandaEntradaViva(BandaTexto* salida)
{
    std::vector<BandaTexto> bandas = ExtraerBandas();
    if (bandas.empty()) return false;
    *salida = bandas.back();
    return true;
}

/* Sanity de navegación: el RUN-MAT real pinta la franja de estado arriba
 * (batería + Math/Rad/Norm1...). La pantalla «MAIN MEMORY CLEARED» no.
 * Sin barra de estado no se entró a la aplicación: fail-closed.           */
bool BarraEstadoPresente()
{
    int oscuros = 0;
    for (int fila = 0; fila <= 20; ++fila) {
        for (int col = 0; col < kAncho; ++col) {
            if (PixelOscuro(fila, col)) ++oscuros;
        }
    }
    std::fprintf(stderr, "[BATTERY] barra estado: %d pixeles oscuros "
                 "(umbral 60)\n", oscuros);
    return oscuros >= 60;
}

struct Caja {
    int x0 = -1, x1 = -1, y0 = -1, y1 = -1;
    std::vector<std::uint8_t> bits;
    int ancho() const { return x1 - x0 + 1; }
    int alto() const { return y1 - y0 + 1; }
};

void ClustersEnBanda(int fila0, int fila1, std::vector<Caja>* salida)
{
    salida->clear();
    bool enBlanco = true;
    int abierto = -1;
    for (int col = 0; col <= kColTexto; ++col) {
        bool hay = false;
        if (col < kColTexto) {
            for (int fila = fila0; fila <= fila1 && !hay; ++fila) {
                if (PixelOscuro(fila, col)) hay = true;
            }
        }
        if (hay && enBlanco) { abierto = col; enBlanco = false; }
        else if (!hay && !enBlanco) {
            Caja caja;
            caja.x0 = abierto; caja.x1 = col - 1;
            caja.y0 = fila1 + 1; caja.y1 = fila0 - 1;
            for (int y = fila0; y <= fila1; ++y) {
                for (int x = abierto; x <= col - 1; ++x) {
                    if (PixelOscuro(y, x)) {
                        if (y < caja.y0) caja.y0 = y;
                        if (y > caja.y1) caja.y1 = y;
                    }
                }
            }
            caja.bits.assign(
                static_cast<std::size_t>(caja.alto() * caja.ancho()), 0);
            for (int y = caja.y0; y <= caja.y1; ++y) {
                for (int x = caja.x0; x <= caja.x1; ++x) {
                    if (PixelOscuro(y, x)) {
                        caja.bits[static_cast<std::size_t>(
                                      (y - caja.y0) * caja.ancho() +
                                      (x - caja.x0))] = 1;
                    }
                }
            }
            salida->push_back(caja);
            enBlanco = true;
        }
    }
}

/* La línea del cursor tras EXE: un cuadrilátero hueco a la izquierda.
 * Se excluye del decode (parpadea y no es texto).                         */
bool BandaEsCursor(const BandaTexto& b, const std::vector<Caja>& cajas)
{
    (void)b;
    if (cajas.size() != 1) return false;
    const Caja& c = cajas[0];
    return c.x0 <= 60 && c.ancho() <= 18 && c.alto() <= 22;
}

int DivergenciaGlifo(const Caja& a, const Caja& b)
{
    if (a.ancho() != b.ancho() || a.alto() != b.alto()) return -1;
    int dif = 0;
    const std::size_t n = a.bits.size();
    for (std::size_t i = 0; i < n; ++i) {
        dif += a.bits[i] != b.bits[i] ? 1 : 0;
    }
    return dif;
}

std::vector<std::pair<char, Caja>> g_calibradas;

/* Contrato calibración v4: la calibración se hace CONTRA RESULTADOS del
 * guest, no contra la línea de entrada. La corrida v3 lo demostró: la
 * entrada decodifica perfecta («2?3») pero los resultados salían '?'
 * porque la fuente del resultado es MÁS GRANDE que la de la entrada
 * (glifo 13x19 en la entrada vs ~24 px de alto en el resultado).
 * AC �?' teclear la expresión ancla �?' EXE �?' la banda del resultado (última
 * banda no-cursor) es del MISMO tipo de letra que los resultados a
 * verificar. La identidad del símbolo la dan NUESTRAS teclas ancla; el
 * guest solo pinta la forma.                                             */

/* Ejecuta teclas + EXE y devuelve la banda del resultado (última banda
 * de texto no-cursor del cuerpo). Vacia si no apareció.                   */
bool BandaResultadoTrasExe(const std::vector<std::uint16_t>& teclas,
                           BandaTexto* salida)
{
    if (Pulso(kAcOn, 120) == 0) return false;
    Tic(60);
    for (const std::uint16_t k : teclas) {
        const std::uint64_t antesT = HashPantalla();
        const std::uint64_t ahoraT = PulsoConEspera(k, 150, 400);
        if (ahoraT == antesT) return false;   /* tecla no registrada.     */
        Tic(2);
        Cg50Status st{};
        cg50core_status(g_core, &st);
        std::fprintf(stderr, "[BATTERY]   tecla 0x%04X ok inst=%" PRIu64
                     "\n", k, st.instructions);
    }
    if (PulsoConEspera(kExe, 150, 2500) == 0) return false;
    EstabilizarPantalla(20, 60);
    Cg50Status st{};
    cg50core_status(g_core, &st);
    std::fprintf(stderr, "[BATTERY]   exe ok inst=%" PRIu64 " cpu=%d\n",
                 st.instructions, st.cpuState);
    std::vector<BandaTexto> bandas = ExtraerBandas();
    for (auto it = bandas.rbegin(); it != bandas.rend(); ++it) {
        std::vector<Caja> cajas;
        ClustersEnBanda(it->f0, it->f1, &cajas);
        if (!BandaEsCursor(*it, cajas)) {
            *salida = *it;
            return !cajas.empty();
        }
    }
    return false;
}

/* Calibra un símbolo desde un resultado ancla. indiceCluster: posición
 * del cluster en la banda del resultado, o -1 para el MÁS PEQUE�'O
 * (el punto decimal en «0.5»).                                           */
bool CalibrarResultado(char s, const std::vector<std::uint16_t>& teclas,
                       int indiceCluster)
{
    BandaTexto banda;
    if (!BandaResultadoTrasExe(teclas, &banda)) {
        std::fprintf(stderr, "[BATTERY] calibR '%c': sin banda de "
                     "resultado\n", s);
        return false;
    }
    std::vector<Caja> cajas;
    ClustersEnBanda(banda.f0, banda.f1, &cajas);
    if (cajas.empty()) {
        std::fprintf(stderr, "[BATTERY] calibR '%c': sin clusters\n", s);
        return false;
    }
    const Caja* elegida = nullptr;
    if (indiceCluster >= 0 &&
        indiceCluster < static_cast<int>(cajas.size())) {
        elegida = &cajas[static_cast<std::size_t>(indiceCluster)];
    } else if (indiceCluster == -2) {
        elegida = &cajas.back();          /* el �sLTIMO (p.ej. 'i' en «1i»). */
    } else {
        std::size_t area = ~static_cast<std::size_t>(0);
        for (const Caja& c : cajas) {
            const std::size_t a =
                static_cast<std::size_t>(c.ancho()) * c.alto();
            if (a < area) { area = a; elegida = &c; }
        }
    }
    if (elegida == nullptr) return false;
    g_calibradas.emplace_back(s, *elegida);
    std::fprintf(stderr, "[BATTERY] calibR '%c': %dx%d @x%d y%d (%zu "
                 "clusters)\n", s, elegida->ancho(), elegida->alto(),
                 elegida->x0, elegida->y0, cajas.size());
    return true;
}

/* Contrato calibración de ENTRADA (para glifos que solo se ven al
 * teclear; los casos actuales no la usan).                                */
bool CalibrarSimbolo(char s, std::uint16_t codigo)
{
    if (Pulso(kAcOn, 120) == 0) return false;
    Tic(60);
    const std::uint64_t antes = HashPantalla();
    if (Pulso(codigo, 200) == 0) return false;
    /* esperar a que el glifo se pinte (cap 600 ms): sin reaccion es
     * calibrar al vacio, fail-closed.                                    */
    std::uint64_t ahora = HashPantalla();
    for (int i = 0; i < 36 && ahora == antes; ++i) {
        Tic(1);
        ahora = HashPantalla();
    }
    if (ahora == antes) {
        std::fprintf(stderr, "[BATTERY] calib '%c': tecla sin reaccion\n", s);
        return false;
    }
    Tic(30);
    BandaTexto banda;
    if (!BandaEntradaViva(&banda)) {
        std::fprintf(stderr, "[BATTERY] calib '%c': sin banda de entrada\n",
                     s);
        return false;
    }
    std::vector<Caja> cajas;
    ClustersEnBanda(banda.f0, banda.f1, &cajas);
    if (cajas.empty()) {
        std::fprintf(stderr, "[BATTERY] calib '%c': sin pixels\n", s);
        return false;
    }
    Caja mejor = cajas.front();
    for (const Caja& c : cajas) {
        if (c.x0 < mejor.x0) mejor = c;
    }
    g_calibradas.emplace_back(s, mejor);
    std::fprintf(stderr, "[BATTERY] calib '%c': %dx%d @x%d y%d\n",
                 s, mejor.ancho(), mejor.alto(), mejor.x0, mejor.y0);
    return true;
}

/* Decodifica una caja contra la calibración (tolerancia 0: match exacto
 * de forma).                                                              */
char DecodificarCaja(const Caja& c)
{
    for (const auto& par : g_calibradas) {
        if (DivergenciaGlifo(c, par.second) == 0) return par.first;
    }
    return '?';
}

/* Decodifica TODAS las bandas de texto del cuerpo (sin la línea del
 * cursor): el resultado del guest está en alguna, alineado a la derecha. */
std::vector<std::string> DecodificarBandas()
{
    std::vector<std::string> textos;
    for (const BandaTexto& b : ExtraerBandas()) {
        std::vector<Caja> cajas;
        ClustersEnBanda(b.f0, b.f1, &cajas);
        if (BandaEsCursor(b, cajas)) continue;
        /* v25 (full-run v24d): banda-BARRA de fraccion apilada. El
         * fix v24b (+2 columnas en FilaConTexto) tambien acepta las
         * filas de la barra de fracciones right-aligned (tocan x375
         * igual que el '4'), y la barra pasa a ser banda propia [?]
         * que rompe el matcheo estructural [num][den] de
         * integral/fraccion (regresion capturada en vivo por la
         * corrida completa: [??????][81][?][4]). La barra es un
         * cluster unico, delgado (alto<=4) y ancho (>=8): se salta
         * como el cursor y el par [num][den] vuelve a ser el
         * ultimo. Un guion/menos NUNCA forma banda propia (viene
         * pegado a los digitos por kerning).                       */
        if (cajas.size() == 1 &&
            cajas[0].alto() <= 4 && cajas[0].ancho() >= 8) {
            continue;
        }
        std::string t;
        for (const Caja& c : cajas) {
            const char ch = DecodificarCaja(c);
            if (ch != '\0') t += ch;
        }
        textos.push_back(t);
    }
    return textos;
}

/* ----------------------------------------------------------- casos ---- */

/* v31: una calibracion por eco: banda (indice 0-based top->down de
 * ExtraerBandas) y cluster desde la DERECHA (los valores van
 * right-aligned). El caracter lo dicta NUESTRA tecla (el eco del
 * editor EQUATION), jamas un resultado calculado por el OS: el motor
 * no se calibra con sus propios outputs.                          */
struct CalibCelda {
    int banda;
    int desde;
    char c;
};

struct Caso {
    const char* etiqueta;
    const char* esperada;               /* referencia independiente.     */
    std::vector<std::uint16_t> teclas;  /* sin el EXE final (se agrega).  */
    bool volcar;                        /* dump BMP por tecla (rep 1):
                                         * instrumentación de plantillas
                                         * OPTN (v8).                    */
    std::vector<std::uint16_t> post;    /* v16: teclas TRAS el EXE (con
                                         * reaccion exigida; un kExe
                                         * dentro repite estabilidad).
                                         * Para S<->D, ANS y cadenas.    */
    bool parBandas;                     /* v27b OPT-IN: esperada
                                         * "a,b[,c...]" -> ultimas k
                                         * bandas (en orden) == las
                                         * partes, sin '?'. Para las
                                         * pantallas de soluciones
                                         * EQUATION. Opt-in porque las
                                         * esperadas de salida-lista
                                         * ({-2,2}) llevan comas y
                                         * NUNCA deben entrar a esta
                                         * rama (falso-paso si la
                                         * banda unica falla).        */


    std::vector<CalibCelda> calib;      /* v31: glifos a calibrar por
                                         * eco de teclas ANTES de
                                         * decodificar (eqn-cal).     */};

const Caso kCasos[] = {
    {"2x3",                 "6",            {kDigDos, kPor, kDigTres},
     false},
    {"1+1",                 "2",            {kDigUno, kMas, kDigUno},
     false},
    {"123456789+987654321", "1111111110",   {
        kDigUno, kDigDos, kDigTres, kDigCuatro, kDigCinco, kDigSeis,
        kDigSiete, kDigOcho, kDigNueve, kMas,
        kDigNueve, kDigOcho, kDigSiete, kDigSeis, kDigCinco, kDigCuatro,
        kDigTres, kDigDos, kDigUno}, false},
    {"7div13",              "0.5384615385", {kDigSiete, kEntre, kDigUno, kDigTres},
     false},
    {"3y4xcinco",           "35",           {kParenA, kDigTres, kMas,
                                             kDigCuatro, kParenC, kPor,
                                             kDigCinco}, false},
    {" dos^3",              "8",            {kDigDos, kPotCaret, kDigTres},
     false},
    /* MathIO auto-cierra las plantillas (la corrida v6 lo demostró: la
     * ')' manual produjo Syntax ERROR y el error envenenó los casos
     * siguientes porque AC no sale de un error: hace falta EXIT).        */
    {"sin2rad",             "0.9092974268", {kSin, kDigDos}, false},
    {"1div3",               "0.3333333333", {kDigUno, kEntre, kDigTres},
     false},
    /* OPTN F4 (calc): F1 Solve, F2 d/dx, F3 d²/dx², F4 �^�dx, F5 SolveN;
     * F6 �-� �?' página 2: FMin, FMax, Σ(, logab (leído del volcado v8b).
     * Las plantillas de casillas se navegan con �?'/�?'/�?", no con comas.     */
    /* Σ(X²,X,1,10)=385. Σ( es plantilla de casillas: se navega con �?'
     * (kDer); las comas NO avanzan el cursor en MathIO (corrida v9:
     * template sin llenar, bandas [??][???]). X² con la tecla x² (sin
     * caret para no abrir casilla de exponente dentro del template).  */
    {"sumatorio",           "385",          {kOptn, kF4, kF6, kF3, kXVar,
                                             kX2, kDer, kXVar, kDer,
                                             kDigUno, kDer, kDigUno,
                                             kDigCero}, true},
    /* d/dx(sin x)|x=2 = cos(2). f(x) en la casilla, �?' salta a «|x=».     */
    {"derivada",            "-0.4161468365",{kOptn, kF4, kF2, kSin, kXVar,
                                             kDer, kDigDos}, false},
    /* �^� x³ dx de 0 a 3 = 81/4. v10: el resultado se dibuja como
     * FRACCI�"N 81/4 (S-D de MathIO; la corrida v9 lo demostró con
     * bandas [??????][81][4]): esperada en notación fracción, la
     * resuelve el matcheo estructural añadido. Teclas intactas de la
     * v9 (calculó bien); sin dump por tecla (ya instrumentado).         */
    {"integral",            "81/4",         {kOptn, kF4, kF4, kXVar,
                                             kPotCaret, kDigTres, kArr,
                                             kDigTres, kAba, kDigCero},
     false},
    /* (3+4i)^5 = -237-3116i (aritmética de referencia del anfitrión).
     * '_' = el '�^'' de operador entre parte real e imaginaria (calibrado
     * aparte: su forma difiere del '�^'' inicial; corrida v8b).            */
    {"complejo",            "-237_3116i",   {kParenA, kDigTres, kMas,
                                             kDigCuatro, kOptn, kF3, kF1,
                                             kParenC, kPotCaret, kDigCinco},
     false},
    /* v11 (2026-11-03): LOG/LN/TRIG-INV/RADICAL/X²/FRAC. Los resultados
     * usan solo glifos ya calibrados (dígitos 0-9 y '.'): NO hacen falta
     * anclas nuevos. ln10/log2 reemplazan ln(e)/log(1e4): resultados de
     * 1 cifra colisionaban con history; estas referencias son largas y
     * seguras (10 sig., verificadas de forma independiente):
     *   ln(10)=2.302585092994045684�?��?'2.302585093 (11ª cifra 9)
     *   log(2)=0.3010299956639812�?��?'0.3010299957 (11ª cifra 6 �?' redondeo).
     *   asin(0.5) RAD=�?/6=0.523598775598�?��?'0.5235987756 (10ª '5' +8 �?' 6). */
    {"raiz9",               "3",            {kShift, kX2, kDigNueve}, false},
    {"siete_x2",            "49",           {kDigSiete, kX2}, false},
    /* v41-monorepo (Modo Normal sin Exam Mode #461): las 10 expresiones
     * exactas simbolicas (pi/6, 27/2, 1/2, sqrt(2)/2, 2pi/3, pi/4, pi/2)
     * rinden forma cerrada en MathIO real; post-EXE {kSD} alterna a su
     * representacion decimal de 10 cifras (misma politica que pi_dos e
     * integral_sd).                                                     */
    {"asin05",              "0.5235987756", {kShift, kSin, kDigCero,
                                             kPunto, kDigCinco}, false, {kSD}},
    /* 1/2+1/3: el guest BCD debe responder 5/6 (fraccion MathIO, S-D,
     * pareja [5][6] del matcheo estructural); un motor IEEE responde
     * 0.8333333333 �?" el caso DISCRIMINA el motor decimal.
     * v14 (recibo c_fraccion_final1.bmp): sin navegacion el kFrac
     * anida (el guest calculo 1/(2+1/3)=3/7 �?" CORRECTO para lo
     * tecleado). El cursor tras el numerador cae en el denominador;
     * kDer sale de la fraccion a la linea principal. */
    {"fraccion",            "5/6",          {kDigUno, kFrac, kDigDos, kDer,
                                             kMas, kDigUno, kFrac, kDigTres,
                                             kDer},
     false},
    /* v14c: pi_dos DIFERIDO a ola 2 - MathIO muestra 2*pi SIMBOLICO
     * (�2pi�) y el glifo pi no esta calibrado; haria falta el toggle
     * S-D tras EXE (soporte pos-EXE en el corredor). Queda anotado en
     * MATRIZ_INVENTARIO.md como BLOQUEADA, no como fallo del guest. */
    {"ln10",                "2.302585093",  {kLn, kDigUno, kDigCero},
     false},
    {"log2",                "0.3010299957", {kLog, kDigDos}, false},
    /* --- importados de Antigravity (solo lectura; suite sin
     *  ejecucion previa: cada caso pasa por NUESTRA vara real).
     *  etiqueta <id> = primera letra del dominio + numero
     *  (ver IMPORTACION_ANTIGRAVITY.md para origen y reglas). */
    /* MATH-ARITH-01 * -12.5 + 4.75 * oracle: Casio fx-CG Manager PLUS v3.60 & Closed-form: -7.75 */
    {"ar01",   "-7.75",   { kMenosU, kDigUno, kDigDos, kPunto, kDigCinco, kMas, kDigCuatro, kPunto, kDigSiete, kDigCinco },
     false},
    /* MATH-ARITH-02 * 0.004 * -25 * oracle: Casio fx-CG Manager PLUS v3.60 & Closed-form: -0.1 */
    {"ar02",   "-0.1",   { kDigCero, kPunto, kDigCero, kDigCero, kDigCuatro, kPor, kMenosU, kDigDos, kDigCinco },
     false},
    /* MATH-ARITH-03 * -35.7 / 7 * oracle: Casio fx-CG Manager PLUS v3.60 & Closed-form: -5.1 */
    {"ar03",   "-5.1",   { kMenosU, kDigTres, kDigCinco, kPunto, kDigSiete, kEntre, kDigSiete },
     false},
    /* MATH-ARITH-04 * (-8.2 + 3.4) * -(2.5 - 7.5) * oracle: Casio fx-CG Manager PLUS v3.60 & Closed-form: -24 */
    {"ar04",   "-24",   { kParenA, kMenosU, kDigOcho, kPunto, kDigDos, kMas, kDigTres, kPunto, kDigCuatro, kParenC, kPor, kMenosU, kParenA, kDigDos, kPunto, kDigCinco, kMenos, kDigSiete, kPunto, kDigCinco, kParenC },
     false},
    /* MATH-ARITH-05 * 1.25E8 * -3.2E-5 * oracle: Casio fx-CG Manager PLUS v3.60 & Closed-form: -4000 */
    {"ar05",   "-4000",   { kDigUno, kPunto, kDigDos, kDigCinco, kExpE, kDigOcho, kPor, kMenosU, kDigTres, kPunto, kDigDos, kExpE, kMenosU, kDigCinco },
     false},
    /* MATH-POW-01 * raiz(182.25) * oracle: Casio fx-CG Manager PLUS v3.60 & Closed-form: 13.5 */
    {"pw01",   "13.5",   { kShift, kX2, kDigUno, kDigOcho, kDigDos, kPunto, kDigDos, kDigCinco },
     false, {kSD}},
    /* MATH-POW-02 * raiz cubica(-1728) * oracle: Casio fx-CG Manager PLUS v3.60 & Closed-form: -12 */
    {"pw02",   "-12",   { kShift, kParenA, kMenosU, kDigUno, kDigSiete, kDigDos, kDigOcho },
     false},
    /* MATH-POW-03 * 5 x-raiz 243 * oracle: Casio fx-CG Manager PLUS v3.60 & Closed-form: 3 */
    {"pw03",   "3",   { kDigCinco, kShift, kPotCaret, kDigDos, kDigCuatro, kDigTres },
     false},
    /* MATH-POW-04 * 2.5 ^ 4 * oracle: Casio fx-CG Manager PLUS v3.60 & Closed-form: 39.0625 */
    {"pw04",   "39.0625",   { kDigDos, kPunto, kDigCinco, kPotCaret, kDigCuatro },
     false},
    /* MATH-POW-05 * ln(20.085537) * oracle: ln(e^3 + 7.68e-8) = 3.0000000038 -> 3.000000004
     * v14 (recibo c_pw05_final1.bmp): el guest mostro 3.000000004 �?"
     * el oraculo '3' del suite era falso a 10 cifras (el argumento
     * del suite es 20.085537, no e^3 exacto). Referencia verificada
     * a mano: e^3 = 20.085536923187668; ln(20.085537) = 3 + 3.82e-9. */
    {"pw05",   "3.000000004",   { kLn, kDigDos, kDigCero, kPunto, kDigCero, kDigOcho, kDigCinco, kDigCinco, kDigTres, kDigSiete },
     false},
    /* MATH-POW-06 * log(500) * oracle: Casio fx-CG Manager PLUS v3.60 & Closed-form: 2.6989700043 */
    {"pw06",   "2.698970004",   { kLog, kDigCinco, kDigCero, kDigCero },
     false},
    /* MATH-POW-07 * logab(2, 1024) * oracle: Casio fx-CG Manager PLUS v3.60 & Closed-form: 10
     * v14 (recibo c_pw07_final1.bmp + F-tab): la pagina 2 del menu
     * CALC es FMin | FMax | Sigma( | logab -> logab = OPTN F4 F6 F4
     * (el F1 era FMin, como predijo la marca REVISAR). */
    {"pw07",   "10",   { kOptn, kF4, kF6, kF4, kDigDos, kDer, kDigUno, kDigCero, kDigDos, kDigCuatro },
     false},
    /* MATH-TRIG-01 * sin(30 grados) * oracle: Casio fx-CG Manager PLUS v3.60 & Closed-form: 0.5
     * v14 (recibo v13 c_tg01_final1.bmp): OPTN F6 F3 = menu PROB
     * (x! nPr nCr RAND, F-tab en el BMP) -> insertaba '!' no '°'.
     * ANGLE queda en F4 de la pagina 2 (hipotesis: probe volcar lo
     * verifica). Sin ')' manual: la plantilla auto-cierra (recibo
     * cp03/mt01: ')' extra = Syntax ERROR). */
    {"tg01",   "0.5",   { kSin, kDigTres, kDigCero, kOptn, kF6, kF5, kF1 },
     true, {kSD}},
    /* MATH-TRIG-02 * cos(60 grados) * oracle: Casio fx-CG Manager PLUS v3.60 & Closed-form: 0.5 */
    {"tg02",   "0.5",   { kCos, kDigSeis, kDigCero, kOptn, kF6, kF5, kF1 },
     false, {kSD}},
    /* MATH-TRIG-03 * tan(45 grados) * oracle: Casio fx-CG Manager PLUS v3.60 & Closed-form: 1 */
    {"tg03",   "1",   { kTan, kDigCuatro, kDigCinco, kOptn, kF6, kF5, kF1 },
     false},
    /* MATH-TRIG-04 * cos(45 grados) * oracle: Closed-form 0.7071067812 (raiz2/2)
     * v14: reescrito a 45° (la forma pi/4 r del suite anotaba mal: el
     * sufijo r liga al numero inmediato, "pi/4 r" = pi/(4r)); la
     * referencia externa es la misma. La cobertura de pi (SHIFT+EXP)
     * queda diferida a ola 2 (ver pi_dos en el bloque de vivos). */
    {"tg04",   "0.7071067812",   { kCos, kDigCuatro, kDigCinco, kOptn, kF6, kF5, kF1 },
     false, {kSD}},
    /* MATH-TRIG-05 * arcsin(0.5) * oracle: Closed-form pi/6 = 0.5235987756
     * v14c: el modo global es RAD (barra de estado, recibos v13/v14b) y
     * la SALIDA de una inversa es un ANGULO en el modo vigente. El
     * oraculo '30' del suite asumia DEG. Adaptacion honesta a RAD:
     * pi/6 (misma forma cerrada que asin05, PASO en v13). El '�' de la
     * entrada se QUITA: 0.5 es adimensional y 0.5� corromperia el
     * input (sin^-1(0.0087...)). */
    {"tg05",   "0.5235987756",   { kShift, kSin, kDigCero, kPunto, kDigCinco },
     false, {kSD}},
    /* MATH-TRIG-06 * arccos(-0.5) * oracle: Closed-form 2*pi/3 = 2.094395102 */
    {"tg06",   "2.094395102",   { kShift, kCos, kMenosU, kDigCero, kPunto, kDigCinco },
     false, {kSD}},
    /* MATH-TRIG-07 * arctan(1) * oracle: Closed-form pi/4 = 0.7853981634
     * v14c: valor CONFIRMADO por recibo - v13 mostro 0.7853981634 en
     * pantalla para tan^-1(1!) (el '!' factorial de la ruta mala es
     * inocuo sobre el 1). */
    {"tg07",   "0.7853981634",   { kShift, kTan, kDigUno },
     false, {kSD}},
    /* MATH-CPLX-01 * (3+4i)*(1-2i) * oracle: Closed-form 11-2i (remapeado a 11_2i, recibo v8b) */
    {"cp01",   "11_2i",   { kParenA, kDigTres, kMas, kDigCuatro, kOptn, kF3, kF1, kParenC, kPor, kParenA, kDigUno, kMenos, kDigDos, kOptn, kF3, kF1, kParenC },
     false},
    /* MATH-CPLX-03 * Abs(3+4i) * oracle: Closed-form 5
     * v14 (recibo c_cp03_final1.bmp): sin ')' manual (plantilla
     * auto-cierra; el ')' extra dio Syntax ERROR). */
    {"cp03",   "5",   { kOptn, kF3, kF2, kDigTres, kMas, kDigCuatro, kOptn, kF3, kF1 },
     false},
    /* MATH-CPLX-04 * Arg(5i) * oracle: Closed-form pi/2 = 1.570796327
     * v14 (recibo c_cp04_final1.bmp): sin ')' manual. El modo global es
     * RAD (barra de estado en los BMP + recibo v9 sin2rad): el guest
     * devuelve 1.570796327, no 90. El oraculo '90' del suite asumia
     * DEG sin anotar; la referencia externa verificada a mano es
     * pi/2 = 1.57079632679... -> 1.570796327 (10 sig).
     * v15 (recibo probe-v14c): Arg 0+5i dio Ma ERROR - Arg liga solo el
     * siguiente primario: Arg(0) indefinido. El guest es correcto para
     * lo recibido. Fix: input 5i directo (i por SHIFT-0, verificado). */
    {"cp04",   "1.570796327",   { kOptn, kF3, kF3, kDigCinco, kShift, kDigCero },
     false, {kSD}},
    /* MATH-CPLX-06 * ReP(8.5-3.2i) * oracle: Closed-form 8.5
     * v14 (recibo c_cp06_final1.bmp + F-tab): ReP esta en la pagina 2
     * del menu CPLX: OPTN F3 F6 F1. Sin ')' manual tras plantilla.
     * v15 (recibo probe-v14c + zoom del eco): ReP es un PREFIJO pelado
     * (no plantilla de caja): liga solo el siguiente primario
     * (�ReP 8.5-3.2i� -> ReP(8.5)-3.2i = 8.5-3.2i, exacto lo mostrado).
     * La sintaxis del manual es ReP(<expr>) con PARENTESIS PROPIOS:
     * se agrupa el argumento con kParenA/kParenC. i por SHIFT-0. */
    {"cp06",   "8.5",   { kOptn, kF3, kF6, kF1, kParenA, kDigOcho, kPunto, kDigCinco, kMenos, kDigTres, kPunto, kDigDos, kShift, kDigCero, kParenC },
     false},
    /* MATH-CPLX-07 * ImP(8.5-3.2i) * oracle: Closed-form -3.2
     * v14 (recibo c_cp07_final1.bmp + F-tab pagina 2): ImP = OPTN F3
     * F6 F2. El 'i' por SHIFT-0 VERIFICADO por recibo (el echo mostro el i).
     * v15: mismo fix de agrupacion que cp06 (ImP liga solo el primario). */
    {"cp07",   "-3.2",   { kOptn, kF3, kF6, kF2, kParenA, kDigOcho, kPunto, kDigCinco, kMenos, kDigTres, kPunto, kDigDos, kShift, kDigCero, kParenC },
     false},
    /* MATH-CALC-01 * d/dx(3x^3-5x^2+2x-7, 2) * oracle: Analytical 18
     * v14c (recibo v14b): sin kDer tras el exponente, TODO el rabo caia
     * DENTRO del exponente de x^3 (el guest calculo correcto lo recibido:
     * -6.43062e-5 verificado a mano). v14c-delta (recibo probe-v14c):
     * con kDer kDer el 2 quedo DESPUES de la caja (|x=?2, Syntax ERROR):
     * con el rabo en la linea principal UN kDer lleva de fin-de-caja a
     * |x= (ca05 necesita dos porque termina en el exponente). Fix v15:
     * kDer tras el 3 del exponente + UN kDer antes del valor. */
    {"ca01",   "18",   { kOptn, kF4, kF2, kDigTres, kXVar, kPotCaret, kDigTres, kDer, kMenos, kDigCinco, kXVar, kX2, kMas, kDigDos, kXVar, kMenos, kDigSiete, kDer, kDigDos },
     true},
    /* MATH-CALC-02 * d/dx(sin x, 0) * oracle: Analytical 1 (coma->DER) */
    {"ca02",   "1",   { kOptn, kF4, kF2, kSin, kXVar, kDer, kDigCero },
     false},
    /* MATH-CALC-03 * int(3x^2+2x+1, 1, 4) * oracle: Analytical 81
     * ajustado: plantilla int apilada: main box -> ARR (limite sup=4)
     * -> ABA (limite inf=1) (recibo v9 integral). */
    {"ca03",   "81",   { kOptn, kF4, kF4, kDigTres, kXVar, kX2, kMas, kDigDos, kXVar, kMas, kDigUno, kArr, kDigCuatro, kAba, kDigUno },
     false},
    /* MATH-CALC-04 * int(e^x, 0, 1) * oracle: Analytical 1.718281828459
     * ajustado: apilada con e^( de SHIFT-LN. */
    {"ca04",   "1.718281828",   { kOptn, kF4, kF4, kShift, kLn, kXVar, kArr, kDigUno, kAba, kDigCero },
     false},
    /* MATH-CALC-05 * d2/dx2(x^4, 3) * oracle: Analytical 108
     * v14b: PASO determinista 2/2 (recibo probe-v14b: match1=match2=
     * '108') - el kDer kDer (salir del exponente, salir de la caja
     * de funcion hasta |x=) quedo VERIFICADO por ejecucion real. */
    {"ca05",   "108",   { kOptn, kF4, kF3, kXVar, kPotCaret, kDigCuatro, kDer, kDer, kDigTres },
     true},
    /* MATH-MAT-01 * Det [[4,7][2,6]] * oracle: Analytical 10
     * v14 (recibo c_mt01_final1.bmp): los corchetes SHIFT+MAS/MENOS
     * SI insertan [ ] (el echo los mostro) y OPTN F2 F3 = Det (F-tab:
     * Mat Mat->Lstb Det Trn Augment). El unico defecto era el ')'
     * manual tras la plantilla -> Syntax ERROR. Sin comas de
     * navegacion: las comas internas son literales de la matriz. */
    {"mt01",   "10",   { kOptn, kF2, kF3, kShift, kMas, kShift, kMas, kDigCuatro, kComa, kDigSiete, kShift, kMenos, kShift, kMas, kDigDos, kComa, kDigSeis, kShift, kMenos, kShift, kMenos },
     false},
    /* MATH-MAT-05 * Det [[1,2,3][0,4,5][1,0,6]] * oracle: Analytical 22 */
    {"mt05",   "22",   { kOptn, kF2, kF3, kShift, kMas, kShift, kMas, kDigUno, kComa, kDigDos, kComa, kDigTres, kShift, kMenos, kShift, kMas, kDigCero, kComa, kDigCuatro, kComa, kDigCinco, kShift, kMenos, kShift, kMas, kDigUno, kComa, kDigCero, kComa, kDigSeis, kShift, kMenos, kShift, kMenos },
     false},
    /* MATH-STAT-01 * P(0) normal * oracle: Closed-form 0.5
     * REVISAR: ruta OPTN F6 F3 F6 F1 sin verificar (nunca se ejecuto). */
    {"st01",   "0.5",   { kOptn, kF6, kF3, kF6, kF1, kDigCero, kParenC },
     false},
    /* MATH-STAT-02 * P(1) * oracle: Standard Normal Table 0.8413447461 */
    {"st02",   "0.8413447461",   { kOptn, kF6, kF3, kF6, kF1, kDigUno, kParenC },
     false},
    /* MATH-STAT-03 * R(2) * oracle: Standard Normal Table 0.02275013195 */
    {"st03",   "0.02275013195",   { kOptn, kF6, kF3, kF6, kF3, kDigDos, kParenC },
     false},
    /* fin importados de Antigravity */

    /* --- v16: soporte post-EXE (S<->D, ANS) ---------------------------- */
    /* La tecla S<->D es PROPIA (id 25, 0x0606; sin SHIFT). El toggle
     * S-D sobre el resultado 81/4 debe rendir 20.25 (la referencia
     * independiente real del caso integral): hace visible el valor
     * decimal sin depender del matcheo estructural de pares.            */
    {"integral_sd",         "20.25",        {kOptn, kF4, kF4, kXVar,
                                             kPotCaret, kDigTres, kArr,
                                             kDigTres, kAba, kDigCero},
     false, {kSD}},
    /* 2*pi = 6.28318307? No: 2*pi = 6.283185307179586... -> 6.283185307
     * (11.a cifra 7 -> redondea la 10.a 0? verif. a mano: 6.28318530718
     * -> 10 sig = 6.283185307). MathIO muestra �2pi� simbolico; el
     * S-D post-EXE debe rendir el decimal. pi = SHIFT + EXP (0x0502).  */
    {"pi_dos",              "6.283185307",  {kDigDos, kShift, kExpE},
     true, {kSD}},
    /* ANS: 1+1=2; luego Ans+1=3. Ans = SHIFT + (0x0402): la corrida
     * v13 demostro con ar01-ar05 que 0x0402 es el (�^') unario (la tabla
     * canonica lo desetiqueta �ANS�); Ans vive en su SHIFT.            */
    {"an01",                "3",            {kDigUno, kMas, kDigUno},
     true, {kShift, kMenosU, kMas, kDigUno, kExe}},

    /* --- v17 OLA 2: OPTN pagina 2 (rutas verificadas por sonda v14a:
     * F1=HYPERBL, F3=PROB(!|nPr|nCr|RAND), F4=NUMERIC(Abs|Int|Frac|Rnd|Intg)).
     * Referencias independientes verificadas a mano:
     *   sinh(1)=1.1752011936438014 -> 1.175201194
     *   cosh(1)=1.5430806348152437 -> 1.543080635
     *   tanh(1)=0.7615941559557649 -> 0.761594156
     *   5!=120 ; 5 nPr 2 = 20 ; 5 nCr 2 = 10
     *   Int(-2.5)=-2 (trunca hacia 0) vs Intg(-2.5)=-3 (piso): el par
     *   DISCRIMINA las dos compuertas. Frac(3.75)=0.75.                  */
    /* v17b (recibo probe-v17b): HYPERBL NO es F1 de la pagina 2 - F1 es
     * el hueco (tecla sin reaccion; el v14a ya lo habia detectado). La
     * tab strip �HYPERBL|PROB|NUMERIC|ANGLE|?� queda mapeada como
     * F2=HYPERBL (esta fix), F3=PROB, F4=NUMERIC, F5=ANGLE (los tres
     * PASO en v17b). HYPERBL menu: F1=sinh, F2=cosh, F3=tanh (manual
     * cap.2: �{sinh}/{cosh}/{tanh}�). Volcar=true para verificar.     */
    {"hp01",   "1.175201194",   { kOptn, kF6, kF2, kF1, kDigUno },
     true},
    {"hp02",   "1.543080635",   { kOptn, kF6, kF2, kF2, kDigUno },
     false},
    {"hp03",   "0.761594156",   { kOptn, kF6, kF2, kF3, kDigUno },
     false},
    {"pr01",   "120",           { kDigCinco, kOptn, kF6, kF3, kF1 },
     false},
    {"pr02",   "20",            { kDigCinco, kOptn, kF6, kF3, kF2, kDigDos },
     false},
    {"pr03",   "10",            { kDigCinco, kOptn, kF6, kF3, kF3, kDigDos },
     false},
    {"nu01",   "7.5",           { kOptn, kF6, kF4, kF1, kMenosU, kDigSiete, kPunto, kDigCinco },
     false},
    {"nu02",   "-2",            { kOptn, kF6, kF4, kF2, kMenosU, kDigDos, kPunto, kDigCinco },
     false},
    {"nu03",   "0.75",          { kOptn, kF6, kF4, kF3, kDigTres, kPunto, kDigSiete, kDigCinco },
     false},
    {"nu04",   "-3",            { kOptn, kF6, kF4, kF5, kMenosU, kDigDos, kPunto, kDigCinco },
     false},

    /* --- v19: LOGIC/CONVERT/CALC/HYPERBL-inversas (propuesta del agente
     * L�GICA con verificaci�n a mano por vector; ver
     * D:\Temp\cg50work\propuesta-vectores-logica.md) --------------------- */
    /* LOGIC de RUN-MAT es L�GICO (0/1), no bitwise (manual PAG 73); el
     * bitwise vive en BASE-N (diferido D1). P�gina 3 de OPTN: F2=LOGIC
     * (hip�tesis; lo01 lleva volcar). Men� LOGIC: F1=And,F2=Or,F3=Not,
     * F4=Xor.                                                           */
    {"lo01",   "1",             { kDigDos, kOptn, kF6, kF6, kF4, kF1, kDigTres },
     true},
    {"lo02",   "1",             { kDigCero, kOptn, kF6, kF6, kF4, kF2, kDigCinco },
     false},
    {"lo03",   "0",             { kDigCinco, kOptn, kF6, kF6, kF4, kF4, kDigSiete },
     false},
    {"lo04",   "1",             { kOptn, kF6, kF6, kF4, kF3, kDigCero },
     false},
    /* CONVERT: sondas - v14a midi� hueco en F1 de la p�g. 2 (el
     * complemento m�trico parece ausente): si el eco sale corrupto, el
     * recibo documenta la ausencia (no cuenta como fallo del guest).    */
    /* v25: co01/co02 RETIRADOS de la tabla ejecutable - NO_APLICABLE:
     * el guest no trae el add-in de conversion metrica (manual ES pag
     * 38 verbatim �unicamente si esta instalado el complemento�), las
     * sondas v14a lo midieron y TODAS las corridas los dejan en eco
     * corrupto. No es defecto de compatibilidad matematica del motor:
     * son casos no testables contra este firmware. Quedan como
     * referencia; el denominador honesto de la bateria pasa a 108.
     * Matriz: co01/co02 = NO_APLICABLE (IMPORTACION 5).
    {"co01",   "77",            { kDigDos, kDigCinco,
                                  kOptn, kF6, kF1, kF6, kF1, kF1, kExit,
                                  kOptn, kF6, kF1, kF1, kExit,
                                  kOptn, kF6, kF1, kF6, kF1, kF3 },
     true},
    {"co02",   "0.01",          { kDigUno, kDigCero, kDigCero,
                                  kOptn, kF6, kF1, kF3, kF2, kExit,
                                  kOptn, kF6, kF1, kF1, kExit,
                                  kOptn, kF6, kF1, kF3, kF3 },
     true},
    */
    /* CALC: SolveN/Solve/FMin/FMax - plantillas de casillas (kDer), sin
     * ')' final. SolveN y Solve documentan su forma (glifos { } y x= sin
     * calibrar: divergencia honesta esperada que documenta el formato). */
    {"ca06",   "{-2,2}",        { kOptn, kF4, kF5, kXVar, kX2, kMenos,
                                  kDigCuatro },
     true, {kExit}},
    /* v24 (7898f67): kComa (0x0306) como separador de argumento -
     * kDer (0x0208) es la FLECHA de navegacion: frames del full-run
     * c_ca07_p12/c_ca08_p19 muestran el cursor desplazado y digitos
     * insertados en posicion equivocada (3Solve(... / 0FMin(...).    */
    /* v24b (recibo sonda v24): Solve( en RUN-MAT imprime SOLO el valor
     * (�3�); el formato �x=3� es de la app EQUATION. Banda final [3]
     * en las 2 reps con h1?h2 por scroll - el match solo mira la
     * ULTIMA banda (v13), el det depende del match (1675).           */
    {"ca07",   "3",             { kOptn, kF4, kF1, kXVar, kX2, kMenos,
                                  kDigCinco, kXVar, kMas, kDigSeis,
                                  kComa, kDigTres },
     true},
    /* FMin: v�rtice -b/2a = 12 (interior a (0,24); f''>0). FMax: -5/(2�-1)
     * = 2.5 (interior a (0,4); f''<0). B�squeda num�rica del guest: si
     * imprime 11.99999998-style el recibo calibra la tolerancia.        */
    /* v24b (recibo sonda v24): FMin devuelve el PAR {argmin, valor} -
     * banda [{12,1}] decodificada limpia en 2 reps (el manual P83 dice
     * �valor minimo�; el guest imprime el par: el recibo calibra la
     * semantica real del guest, que manda sobre el manual).           */
    {"ca08",   "{12,1}",        { kOptn, kF4, kF6, kF1,
                                  kXVar, kX2, kMenos, kDigDos, kDigCuatro,
                                  kXVar, kMas, kDigUno, kDigCuatro, kDigCinco,
                                  kComa, kDigCero, kComa, kDigDos,
                                  kDigCuatro },
     true},
    /* v24b (recibo sonda v24): FMax = par {5/2,45/4} con fracciones
     * apiladas MathIO (banda [??,??]: la barra NO parte la banda y
     * numerador+denominador se clusterizan en una caja vertical).
     * Post-EXE kSD (0x0606, tecla propia, sin SHIFT) las convierte a
     * decimal - ruta S-D ya acreditada por integral_sd (v16).         */
    {"ca09",   "{2.5,11.25}",   { kOptn, kF4, kF6, kF2,
                                  kDigCinco, kMas, kDigCinco, kXVar, kMenos,
                                  kXVar, kX2,
                                  kComa, kDigCero, kComa, kDigCuatro },
     true, {kSD}},
    /* ? nuevo (patr�n sumatorio v9 probado): ?(X�-3X+5,X,2,6)=55, doble
     * verificaci�n a mano (3+5+9+15+23=55 y 90-60+25=55).               */
    {"ca10",   "55",            { kOptn, kF4, kF6, kF3,
                                  kXVar, kX2, kMenos, kDigTres, kXVar, kMas,
                                  kDigCinco, kDer, kXVar, kDer, kDigDos,
                                  kDer, kDigSeis },
     false},
    /* HYPERBL inversas (F4=sinh?� y F6=tanh?� inferidos del orden PAG 44;
     * F5=cosh?� verbatim PAG 45). Referencias: ln(1+?2), �ln3, ln(2+?3). */
    {"hp04",   "0.881373587",   { kOptn, kF6, kF2, kF4, kDigUno },
     true},
    {"hp05",   "0.5493061443",  { kOptn, kF6, kF2, kF6, kDigCero, kPunto,
                                  kDigCinco },
     false},
    {"hp06",   "1.316957897",   { kOptn, kF6, kF2, kF5, kDigDos },
     false},

    /* --- v20: DISTRIBUCIONES (OPTN F5 STAT -> F3 DIST; manual PAG 235
     * verbatim para la ruta, PAG 339-343 para sintaxis/defaults: sigma=1,
     * mu=0, tail=L al omitir). Sin ')' final (auto-cierre EXE, leccion
     * v6/mt01). Separador kComa = hipotesis H2 funcion-plana
     * (precedentes st02/nu01-04/hp01-06); variante C kDer si el eco
     * muestra casillas. Referencias verificadas a mano en
     * D:\Temp\cg50work\propuesta-vectores-distribuciones.md
     * Poisson es (x, lambda) - di08 discrimina el orden. --------------- */
    {"di01",   "0.3989422804", { kOptn, kF5, kF3, kF1, kF1, kDigCero },
     true},
    {"di02",   "0.3413447461", { kOptn, kF5, kF3, kF1, kF2, kDigCero, kComa,
                                 kDigUno },
     false},
    {"di03",   "0.3183098862", { kOptn, kF5, kF3, kF2, kF1, kDigCero, kComa,
                                 kDigUno },
     true},
    {"di04",   "0.5",          { kOptn, kF5, kF3, kF2, kF2, kMenosU, kDigUno,
                                 kComa, kDigUno, kComa, kDigUno },
     false},
    {"di05",   "0.375",        { kOptn, kF5, kF3, kF5, kF1, kDigDos, kComa,
                                 kDigCuatro, kComa, kDigCero, kPunto,
                                 kDigCinco },
     true},
    {"di06",   "0.6875",       { kOptn, kF5, kF3, kF5, kF2, kDigDos, kComa,
                                 kDigCuatro, kComa, kDigCero, kPunto,
                                 kDigCinco },
     false},
    {"di07",   "2",            { kOptn, kF5, kF3, kF5, kF3, kDigCero, kPunto,
                                 kDigCinco, kComa, kDigCuatro, kComa,
                                 kDigCero, kPunto, kDigCinco },
     false},
    {"di08",   "0.2240418077", { kOptn, kF5, kF3, kF6, kF1, kF1, kDigDos,
                                 kComa, kDigTres },
     true},
    {"di09",   "0.4231900811", { kOptn, kF5, kF3, kF6, kF1, kF2, kDigDos,
                                 kComa, kDigTres },
     false},
    {"di10",   "0.140625",     { kOptn, kF5, kF3, kF6, kF2, kF1, kDigTres,
                                 kComa, kDigCero, kPunto, kDigDos, kDigCinco },
     true},
    {"di11",   "0.4375",       { kOptn, kF5, kF3, kF6, kF2, kF2, kDigDos,
                                 kComa, kDigCero, kPunto, kDigDos, kDigCinco },
     false},
    {"di12",   "0.6",          { kOptn, kF5, kF3, kF6, kF3, kF1, kDigUno,
                                 kComa, kDigDos, kComa, kDigTres, kComa,
                                 kDigCinco },
     true},

    /* --- v21: FINANCIERO (agente FINANCIERO; manual PAG 297 ejemplo
     * RUN-MAT verbatim: OPTN ?? F2[FINANCE]). Los tokens financieros
     * insertan �(� y el ')' se TECLEA (funciones planas, no plantillas).
     * Arbol: F1=SIMPLE(SI,SFV) F2=COMPND(n,I%,PV,PMT,FV) F3=CASH
     * F4=AMORTZN F5=CONVERT(EFF,APR) F6?(COST/Sell/Margin, DAYS/PRD,
     * BOND). Referencias a mano en propuesta-vectores-financiero.md. -- */
    /* Smpl_SI(730,5,300) = -(730/365)*300*0.05 = -30 (ejemplo manual).  */
    {"fi01",   "-30",           { kOptn, kF6, kF6, kF2, kF1, kF1, kDigSiete,
                                  kDigTres, kDigCero, kComa, kDigCinco, kComa,
                                  kDigTres, kDigCero, kDigCero, kParenC },
     false},
    /* Smpl_SFV(730,5,300) = -(300+30) = -330 (mismo ejemplo).           */
    {"fi02",   "-330",          { kOptn, kF6, kF6, kF2, kF1, kF2, kDigSiete,
                                  kDigTres, kDigCero, kComa, kDigCinco, kComa,
                                  kDigTres, kDigCero, kDigCero, kParenC },
     false},
    /* Cnvt_EFF(4,12) = (1.03^4-1)*100 = 12.550881 (cuadrado perfecto).  */
    {"fi03",   "12.550881",     { kOptn, kF6, kF6, kF2, kF5, kF1, kDigCuatro,
                                  kComa, kDigUno, kDigDos, kParenC },
     false},
    /* Cnvt_APR(2,21) = (sqrt(1.21)-1)*2*100 = 20 exacto.                */
    {"fi04",   "20",            { kOptn, kF6, kF6, kF2, kF5, kF2, kDigDos,
                                  kComa, kDigDos, kDigUno, kParenC },
     false},
    /* Sell(100,20) = 100/(1-0.20) = 125 (margen sobre venta, no markup).*/
    {"fi05",   "125",           { kOptn, kF6, kF6, kF2, kF6, kF1, kF2,
                                  kDigUno, kDigCero, kDigCero, kComa,
                                  kDigDos, kDigCero, kParenC },
     false},
    /* Days_Prd(1,1,2001,1,1,2002) = 365 (ano comun).                    */
    {"fi06",   "365",           { kOptn, kF6, kF6, kF2, kF6, kF2, kF1,
                                  kDigUno, kComa, kDigUno, kComa, kDigDos,
                                  kDigCero, kDigCero, kDigUno, kComa, kDigUno,
                                  kComa, kDigUno, kComa, kDigDos, kDigCero,
                                  kDigCero, kDigDos, kParenC },
     false},
    /* Days_Prd(1,1,2000,1,1,2001) = 366 (bisiesto /400).                */
    {"fi07",   "366",           { kOptn, kF6, kF6, kF2, kF6, kF2, kF1,
                                  kDigUno, kComa, kDigUno, kComa, kDigDos,
                                  kDigCero, kDigCero, kDigCero, kComa,
                                  kDigUno, kComa, kDigUno, kComa, kDigDos,
                                  kDigCero, kDigCero, kDigUno, kParenC },
     false},
    /* Cmpd_PV(2,10,0,1210,1,1) = -1210/1.1^2 = -1000 exacto.            */
    {"fi08",   "-1000",         { kOptn, kF6, kF6, kF2, kF2, kF3, kDigDos,
                                  kComa, kDigUno, kDigCero, kComa, kDigCero,
                                  kComa, kDigUno, kDigDos, kDigUno, kDigCero,
                                  kComa, kDigUno, kComa, kDigUno, kParenC },
     false},

    /* --- v22: NUMERIC pagina 2 (tras F6 del menu F4: RndFix=GCD=LCM=MOD
     * en orden; ejemplo verbatim del manual �6(g) 1(RndFix)�). Escalares
     * con referencia a mano: GCD(84,36)=12, LCM(4,6)=12, MOD(13,5)=3,
     * RndFix(1.23456789,3)=1.235. inv01: x^-1 (SHIFT+x^2 hipotesis,
     * volcar verifica): 4^-1=0.25. Funciones planas sin ')' final.     */
    {"gcd01",  "12",            { kOptn, kF6, kF4, kF6, kF2, kDigOcho,
                                  kDigCuatro, kComa, kDigTres, kDigSeis },
     true},
    {"lcm01",  "12",            { kOptn, kF6, kF4, kF6, kF3, kDigCuatro,
                                  kComa, kDigSeis },
     false},
    {"mod01",  "3",             { kOptn, kF6, kF4, kF6, kF4, kDigUno,
                                  kDigTres, kComa, kDigCinco },
     false},
    {"rf01",   "1.235",         { kOptn, kF6, kF4, kF6, kF1, kDigUno,
                                  kPunto, kDigDos, kDigTres, kDigCuatro,
                                  kDigCinco, kDigSeis, kDigSiete, kDigOcho,
                                  kDigNueve, kComa, kDigTres },
     false},
    /* --- v24 (OLA 4 �2): MOD_Exp - F5 de NUMERIC pag 2, confirmado
     * por frame c_gcd01_p4 y manual pag 71 verbatim
     * �MOD_Exp(5,3,3)=2�. n^p mod m. Oraculos verificados a mano:
     * 125=41*3+2; 1024=204*5+4; 7^0=1; 1000=142*7+6.              */
    {"modexp01", "2",           { kOptn, kF6, kF4, kF6, kF5, kDigCinco,
                                  kComa, kDigTres, kComa, kDigTres },
     false},   /* 5^3 mod 3 = 2 */
    {"modexp02", "4",           { kOptn, kF6, kF4, kF6, kF5, kDigDos,
                                  kComa, kDigUno, kDigCero, kComa,
                                  kDigCinco },
     false},   /* 2^10 mod 5 = 4 */
    {"modexp03", "1",           { kOptn, kF6, kF4, kF6, kF5, kDigSiete,
                                  kComa, kDigCero, kComa, kDigUno,
                                  kDigTres },
     false},   /* 7^0 mod 13 = 1 */
    {"modexp04", "6",           { kOptn, kF6, kF4, kF6, kF5, kDigUno,
                                  kDigCero, kComa, kDigTres, kComa,
                                  kDigSiete },
     false},   /* 10^3 mod 7 = 6 */
    /* --- v24b (OLA 4 �3): CALC pag 3 = F1=Int�, F2=Rmdr, F3=Simp -
     * verbatim manual P74 �K4(CALC) 6(g) 6(g) 1(Int�)�. volcar=TRUE
     * confirma el slot por frame en la primera sonda.               */
    /* v24d (forense v24c): la notaci�n del guest es INFIJA - el
     * accidente de rmdr_a (kDer mand� el cursor a posici�n 0 y el 5
     * qued� delante) evalu� �5 Rmdr 13�=5 CORRECTO (5 mod 13=5, frame
     * c_rmdr_a_final1: resultado right-aligned �5�). El manual ES P74
     * escribe �13Rmdr5� infijo. La forma prefija �Rmdr  13,5� da
     * Syntax ERROR SIEMPRE (v24b/v24c: 7 variantes probadas). El token
     * se inserta EN MEDIO: 13, token, 5 - sin par�ntesis ni coma.    */
    {"rmdr01",   "3",           { kDigUno, kDigTres, kOptn, kF4, kF6,
                                  kF6, kF2, kDigCinco },
     true},    /* 13 Rmdr 5 = 3 */
    {"intdiv01", "2",           { kDigUno, kDigTres, kOptn, kF4, kF6,
                                  kF6, kF1, kDigCinco },
     true},    /* 13 Int� 5 = 2 */
    /* --- v25 (OLA 4 �4.1 + oraculos Antigravity test_writer_math):
     * dominio CPLX completo. El token i = OPTN F3 F1 (confirmado por
     * el caso �complejo�); F2=Abs, F3=Arg, F4=Conjg, F5=ReP, F6=ImP
     * seg�n manual verbatim - los frames del volcar de cplx02/cplx05
     * confirman los slots en la primera sonda. El auto-cierre del EXE
     * aplica a los tokens CON '(' (patr�n gcd01). El '+' de operador
     * lo calibra el ancla �1+2i� a�adida al arranque (v25). El '_' ya
     * est� calibrado (ancla �2-3i�, corrida v8b).                 */
    {"cplx01", "11_2i",         { kParenA, kDigTres, kMas, kDigCuatro,
                                  kOptn, kF3, kF1, kParenC, kPor,
                                  kParenA, kDigUno, kMenos, kDigDos,
                                  kOptn, kF3, kF1, kParenC },
     false},   /* (3+4i)*(1-2i) = 11-2i */
    {"cplx02", "4+3i",          { kParenA, kDigCinco, kMas, kDigUno,
                                  kDigCero, kOptn, kF3, kF1, kParenC,
                                  kEntre, kParenA, kDigDos, kMas,
                                  kOptn, kF3, kF1, kParenC },
     true},    /* (5+10i)/(2+i) = 4+3i - NECESITA '+' */
    /* v25.2 (forense de la sonda v25): F2-Abs NO inserta la palabra
     * �Abs� sino las BARRAS |..| AUTO-CERRADAS (zoom c_cplx03:
     * input |3+4i)| con barra inicial gruesa; mi kParenC cayo
     * DENTRO de las barras -> Syntax ERROR). Fix: sin kParenC
     * final, el vector deja |3+4i| y el EXE rinde 5.           */
    {"cplx03", "5",             { kOptn, kF3, kF2, kDigTres, kMas,
                                  kDigCuatro, kOptn, kF3, kF1 },
     false},   /* |3+4i| (barras de Abs) = 5 */
    /* v25.2: F3-Arg inserta el nombre DESNUDO sin paren (zoom
     * c_cplx04: �Arg 0+5i)� con espacio, sin curva) y liga SOLO el
     * termino siguiente: Arg 0 = angulo de 0 = Ma ERROR. La
     * sonda diag cplxdB demostro que Arg(0+5i) con paren
     * explicito rinde 1.570796327 = PI/2: la unidad de angulo del
     * guest es RAD (barra de estado), no grados como asumia el
     * oraculo de Antigravity (90). Fix: parens explicitos y
     * expectativa en Rad.                                      */
    {"cplx04", "1.570796327",   { kOptn, kF3, kF3, kParenA, kDigCero,
                                  kMas, kDigCinco, kOptn, kF3, kF1,
                                  kParenC },
     false, {kSD}},   /* Arg(0+5i) = PI/2 en Rad (cplxdB) */
    /* v25.2: el vector original no tecleaba i ni ')': Conjg
     * desnudo ligaria Conjg(7)-12 = -5 (asi lo midio la sonda:
     * resultado -5). Fix: parens explicitos + i.               */
    {"cplx05", "7+12i",         { kOptn, kF3, kF4, kParenA, kDigSiete,
                                  kMenos, kDigUno, kDigDos, kOptn,
                                  kF3, kF1, kParenC },
     true},    /* Conjg(7-12i) = 7+12i - NECESITA '+' */
    /* v25.2 (diagnostico cplxd6/cplxd61/cplxd62): ReP/ImP viven en
     * la PAGINA 2 del submenu COMPLEX: tras kOptn,kF3 la barra es
     * i|Abs|Arg|Conjug|> y kF6 (la FLECHA) la desplaza a
     * ReP|ImP|r<A|a+bi|> (frame c_cplxd6_p3). F1=ReP y F2=ImP
     * insertan el nombre DESNUDO (c_cplxd61_p4: input �ReP�), asi
     * que el vector agrega el paren explicito. El 'i' final
     * re-abre OPTN->F3 (pagina 1 fresca) y pide F1=i.           */
    {"cplx06", "8.5",           { kOptn, kF3, kF6, kF1, kParenA,
                                  kDigOcho, kPunto, kDigCinco, kMenos,
                                  kDigTres, kPunto, kDigDos, kOptn,
                                  kF3, kF1, kParenC },
     false},   /* ReP(8.5-3.2i) = 8.5 (pag 2 del submenu) */
    {"cplx07", "-3.2",          { kOptn, kF3, kF6, kF2, kParenA,
                                  kDigOcho, kPunto, kDigCinco, kMenos,
                                  kDigTres, kPunto, kDigDos, kOptn,
                                  kF3, kF1, kParenC },
     false},   /* ImP(8.5-3.2i) = -3.2 (pag 2; el menos unario
                * de resultados decodifica '-', como nu02/nu04) */
    {"inv01",  "0.25",          { kDigCuatro, kPotCaret, kParenA,
                                  kMenos, kDigUno, kParenC },
     false},   /* 4^(-1) = 0.25 - el guest lo despliega DECIMAL
                      * nativo en MathIO+d/c: '^' rinde el resultado
                       * en decimal, sin S<->D que dar. Forense v25:
                      * (a) SHIFT+x2 NO inserta '^' sino la RAIZ sqrt(
                      * (c_inv01 v1: 4sqrt(-1) -> Non-Real ERROR); la
                      * potencia es kPotCaret (0x0508) directa (como
                      * dos^3/pw03/ca01). (b) el post kSD (0x0606) NO
                      * togglea este estado: el press abrio el menu
                      * OPTN en rep 1 y la barra JUMP|DELETE en rep 2
                      * (frames c_inv01_post0/final1/final2, det=NO
                      * por estados distintos) - el S<->D ya queda
                      * acreditado por integral_sd (81/4->20.25) y
                      * pi_dos (2pi->6.283185307).
                       * (c) mini-sonda v25f: el par 4^(-1)->0.25 es el
                       * ULTIMO del historial, que ACUMULA una entrada
                       * por rep (el layout se corre una fila: h1!=h2)
                       * pero la ULTIMA banda es 0.25 limpia en ambas;
                       * det es por TEXTO (match1==match2 no vacios),
                       * el h post-EXE es solo diagnostico.                */
    /* DIAG v26 FINANCE retirado (fin-dA/fin-dB): frames leidos y
     * preservados (recibos + Evidencia_CG50\outdir-findiag).
     * LECCIONES: (a) OPTN F6 F6 F2 abre FINANCE (SIMPLE|COMPND|CASH|
     * AMORTZN|CONVERT|?); (b) los picks de submen� se hacen con teclas
     * F, NO con d�gitos (el '2' de fin-dB fue a la l�nea de entrada). */
    /* DIAG v27 (RETIRAR tras leer frames; patron cplxdA/B): slots
     * tras cada pick. fin-dC: COMPND (kF2). fin-dD: AMORTZN (kF4).
     * fin-dE: p�gina 2 del submen� FINANCE (kF6 ? = TAX/COST|DAYS|
     * BOND seg�n fi05). fin-dF: slots tras pick TAX (kF6,kF1) -
     * Cost/Margin comparten submen� con Sell (fi05: kF6,kF1,kF2). */
    /* v33: fin-dC..fin-dH RETIRADOS (lote de discovery FINANCE cerrado).
     * Eran los DIAG de v30 y sus lecciones ya estan ACREDITADAS y
     * documentadas: (1) la ruta OPTN F6 F6 F2 abre FINANCE con sub-tab
     * SIMPLE|COMPND|CASH|AMORTZN|CONVERT (F2 = COMPND, F4 = AMORTZN,
     * F6 = pagina 2); (2) el pick NO inserta texto, solo conmuta el
     * F-tab a slots de variable (n|I%|PV|PMT|FV / Cost|Sell|Margin);
     * (3) F1 en COMPND inserta Cmpd_n( (frame c_fin-dG_final1 de la
     * sonda v29) -> acreditado y EN USO por el caso real fin-rA, que
     * valida la ruta completa con oraculo a mano 34.45443226;
     * (4) fin-dH demostro kMenu+letra -> app Financial(1/2) con campos
     * TVM INVENDIBLES (etiquetas sin calibrar en toda fila; leccion
     * (20) del tablero). Evidencia permanente: los F-dumps c_fin-dC..
     * c_fin-dH de la sonda v29 + tablero COORDINACION-SESIONES
     * bloques (3),(4),(14)-(21). Convertirlos a reales DUPLICARIA
     * fin-rA o exigiria plantillas AMORT/CASH nunca verificadas
     * (riesgo nuevo sin necesidad). La tabla queda SIN DIAG:
     * 141 - 6 = 135 casos, requisito de la corrida FINAL (0 DIAG,
     * exit 0 exige todo verde).
     */
    /* fin-rA (v30): caso REAL de finanzas por RUN-MAT: OPTN F6 F6 F2
     * (FINANCE -> COMPND) F1 inserta Cmpd_n( (frame c_fin-dG_final1
     * de la sonda v29), luego los 4 argumentos I%,PV,PMT,FV con
     * digitos calibrados y comas; MathIO auto-cierra la plantilla
     * (leccion sumatorio: ')' manual produjo Syntax ERROR).
     * Cmpd_n(10,-1000,0,1331) = numero de periodos n de
     * PV*(1+i)^n + FV = 0 exacto (-1000*1.1^3 + 1331 = 0) -> n=3.
     * El I% es el numero 10 (el '%' NO se escribe). volcar=true
     * instrumenta el ensamblado real (si la plantilla fuese de
     * casillas, las comas no avanzarian y las bandas lo diran).  */
    /* v32 fin-rA: ORACULO A MANO (independiente): el OS aplica P/Y=12
     * mensual por defecto -> n = ln(1.331)/ln(1+0.1/12) =
     * 0.2859305394/0.0082988028 = 34.45443226 (los 10 digitos que el
     * OS ya mostro en la sonda v31). El comentario v31 de encima
     * queda como historial del oraculo refutado n=3. kExit de
     * prefijo normaliza F-tab/OPTN (rep 1 v31: kOptn sin reaccion).
     * Las teclas restantes son EXACTAMENTE las de v31 (el typo
     * kDigCero de 1331 ya corregido).                                   */
    {"fin-rA", "34.45443226",     { kExit, kOptn, kF6, kF6, kF2, kF2,
                                    kF1,
                                    kDigUno, kDigCero, kComa,
                                    kMenos, kDigUno, kDigCero,
                                    kDigCero, kDigCero, kComa,
                                    kDigCero, kComa,
                                    kDigUno, kDigTres, kDigTres,
                                    kDigUno },
     true},
    /* --- OLA 5: vectores reales de rutas cerradas (censo �8). --- */
    /* Q(0) = densidad normal en 0 = 1/V(2pi) = 0.3989422804.
     * Hermano de P(/R( acreditados (st01-03: misma fila pagina 2 de
     * PROB F1=P( F2=Q( F3=R(); st03 = R(2) con kF3). El slot F2 es
     * el �nico que faltaba confirmar; volcar=true lo verifica.     */
    /* v29: Q(0) corregido a 0 (PROBABILIDAD CENTRAL 2*Phi(0)-1). La
     * expectativa previa 0.3989422804 asumia Q=densidad phi; la
     * sonda v27eqn EJECUTO Q(0) y el firmware rindio 0 (frame
     * c_q01_final1: Q( 0 ) en historial, resultado 0 right-aligned).
     * Esquema del firmware por ejecucion real: P(t)=Phi(t) (st01
     * P(0)=0.5, st02 P(1)=0.8413447461), R(t)=1-Phi(t) (st03
     * R(2)=0.02275013195) y Q(t)=2*Phi(t)-1 (q01). q02 disCRIMINA
     * el esquema en t no degenerado: cola superior daria
     * 0.02275013195, bicola 0.0455002639, central 0.9544997361.  */
    /* v30 CORRECCION DEL ORACULO (por frame del OS): el frame
     * c_q02_final1 de la sonda v29 muestra Q(2)->0.4772498681 =
     * Phi(2)-0.5 - la Casio define Q(t) = integral de 0 a t de
     * phi(x)dx (normal acumulada CENTRAL = Phi(t)-0.5, probabilidad
     * entre 0 y t, como el manual), NO 2*Phi(t)-1. El comentario
     * previo decia "verificado por ejecucion", pero Q(0)=0 es
     * consistente con AMBAS definiciones (Phi(0)-0.5 = 2*Phi(0)-1
     * = 0) y no discriminaba; el frame del OS en t no degenerado
     * si. Esquema COMPLETO: P(t)=Phi(t) (st01/st02), R(t)=1-Phi(t)
     * (st03), Q(t)=Phi(t)-0.5 (q01 Q(0)=0, q02 Q(2)=0.4772498681). */
    {"q01",    "0",               { kOptn, kF6, kF3, kF6, kF2, kDigCero,
                                    kParenC },
     true},
    {"q02",    "0.4772498681",    { kOptn, kF6, kF3, kF6, kF2, kDigDos,
                                    kParenC },
     true},
    /* Conjg(2+4i) = 2-4i (manual P85-86, verbatim censo �8). Patron
     * cplx05: el 'i' re-abre OPTN->F3 y pide F1=i; el '_' codifica el
     * menos de operador entre real e imaginaria (calibrado en
     * "complejo" -237_3116i).                                       */
    {"conj01", "2_4i",            { kOptn, kF3, kF4, kParenA, kDigDos,
                                    kMas, kDigCuatro, kOptn, kF3, kF1,
                                    kParenC },
     true},
    /* --- DIAG v27 EQUATION (app icono 10; despliegue NUEVO ->
     * sonda FR�A). Ruta del censo �9.2: kMenu,kAba,kDer*4,kExe.
     * PRUEBA PARALELA de rutas: siml-a..eqn03 usan kMenu,kXVar (la
     * tecla x lleva etiqueta roja A = icono 10; independiente del
     * cursor); eqn04/eqn05 usan la ruta cursor del censo. Los frames
     * deciden cual es robusta (el cursor del MENU recuerda el
     * �ltimo icono entre casos: la ruta kXVar no depende de �l).   */
    /* v31 eqn-cal: ANCLA DE CALIBRACION POR ECO DE TECLAS de la fuente
     * de celdas EQUATION. Maquina verificada POR EL OS (frames v30):
     * kXVar re-abre la app en su ULTIMA pantalla (warm v22 = Select
     * Type, c_siml01_p2); SelectType F1=SIMUL (c_siml01_p3); el prompt
     * Number Of Unknowns? tiene F1..F5 = 2..6; el editor 2x2 FRESCO se
     * crea con CEROS y cursor invertido en a1 (c_eqn-dC_p4 = 3x3);
     * EXE avanza celda. En el editor FRESCO escribimos a1=1, b1=-2,
     * c1=-3 (ECO DE NUESTRAS TECLAS, jamas un output del OS) y
     * calibramos '1','-','2','3' desde la banda de la fila 1 (clusters
     * right-aligned; tras c1 el cursor queda en a2, fila 2). La
     * identidad fuente celdas==solucion la documentan el eco y los
     * frames. DIAG: su decode no se aserta; el log "calib eco" y los
     * frames documentan el eco real. Los glifos estructurales ('X',
     * corchetes) JAMAS se calibran.                                */
    /* v32 eqn-cal (convertida a REAL): el eco v31 quedo documentado
     * (log "calib eco" + frames); la identidad fuente celdas ==
     * resultado la acreditan ahora los casos siml (sus fracciones se
     * decodifican con las calibraciones de arranque). Sistema nuevo:
     * X-2Y=-3, X+Y=1 -> Y=4/3, X=-1/3 (algebra a mano). Patron
     * verificado v31: kMenu,kXVar abre EQUATION en Select Type;
     * kF1=SIMUL; kF1 en el prompt = 2 desconocidas (la F-key del
     * prompt compromete directo, frames eqn-cal v30/v31). Celdas:
     * digitos+EXE avanza; kMenos antes del digito funciona en el
     * editor. Tras la ultima celda kF1=SOLVE; el EXE del harness
     * avanza el cursor X->Y; el OS pinta Y=4/3 como fraccion
     * apilada DENTRO de la zona de texto (bandas [4][3], matcher de
     * fraccion). Calib de re-eco sobre la pantalla final (bandas
     * crudas de ExtraerBandas: 0=bloque grande, 1=num, 2=barra,
     * 3=den): banda 1 num='4', banda 3 den='3'.                        */
    /* v34 eqn-cal: kExit de prefijo (patron siml01-03 probado 2 reps:
     * muerto en Select Type rep1; en rep2 devuelve desde la pantalla
     * de solucion recordada por la app -> flujo determinista).       */
    {"eqn-cal", "4/3",            { kMenu, kXVar, kExit, kF1, kF1,
                                    kDigUno, kExe,
                                    kMenos, kDigDos, kExe,
                                    kMenos, kDigTres, kExe,
                                    kDigUno, kExe,
                                    kDigUno, kExe,
                                    kDigUno, kExe, kF1 },
     true, {}, false,
     { {1, 0, '4'}, {3, 0, '3'} }},
    /* v32 siml01 (REAL): SIML 2x2 X-Y=1, 2X+3Y=5 -> X=8/5, Y=3/5
     * (algebra a mano). Entrada verificada por frames v31: kExit es
     * muerto en Select Type (escalon de la escalera, inocuo); kF1=
     * SIMUL; kF1 en el prompt = 2 desconocidas -> editor 2x2 FRESCO
     * a ceros (en v31 el primer digito moria en el prompt y el EXE
     * lo consumia: digitos corridos un slot, landed (1,5,1)/(-1,-1,0)
     * -> X=-0.25, Y=0.25, matcher leyo "4"). El sistema v32 evita
     * la corrida: 6 celdas 1,-1,1 / 2,3,5. kF1=SOLVE; el EXE del
     * harness avanza X->Y -> display grande 3/5 apilado (bandas
     * [3][5], matcher de fraccion).                                    */
    {"siml01", "3/5",             { kMenu, kXVar, kExit, kF1, kF1,
                                    kDigUno, kExe,
                                    kMenos, kDigUno, kExe,
                                    kDigUno, kExe,
                                    kDigDos, kExe,
                                    kDigTres, kExe,
                                    kDigCinco, kExe, kF1 },
     true, {}, false},
    /* v32 siml02 (REAL): SIML 2x2 X-2Y=1, 2X+Y=4 -> X=9/5, Y=2/5
     * (algebra a mano). Mismo flujo verificado que siml01; el EXE
     * del harness deja el cursor en Y -> display grande 2/5 apilado
     * (bandas [2][5]).                                                 */
    {"siml02", "2/5",             { kMenu, kXVar, kExit, kF1, kF1,
                                    kDigUno, kExe,
                                    kMenos, kDigDos, kExe,
                                    kDigUno, kExe,
                                    kDigDos, kExe,
                                    kDigUno, kExe,
                                    kDigCuatro, kExe, kF1 },
     true, {}, false},
    /* v32 siml03 (REAL): SIML 3x3 x+y+z=6, x+2y=5, 3y+3z=11 ->
     * x=7/3, y=4/3, z=7/3 (algebra a mano). En v31 kF2 caia en
     * Select Type y abrio POLY (Ma ERROR); en el prompt F1..F5 =
     * 2..6 desconocidas (kF2 = 3). 12 celdas por digitos+EXE;
     * kF1=SOLVE; el EXE del harness avanza el cursor X->Y (NO la
     * ultima fila: evita el post-EXE sin verificar de v31) ->
     * display grande 4/3 apilado (bandas [4][3]).                      */
    {"siml03", "4/3",             { kMenu, kXVar, kExit, kF1, kF2,
                                    kDigUno, kExe, kDigUno, kExe,
                                    kDigUno, kExe, kDigSeis, kExe,
                                    kDigUno, kExe, kDigDos, kExe,
                                    kDigCero, kExe, kDigCinco, kExe,
                                    kDigCero, kExe, kDigTres, kExe,
                                    kDigTres, kExe,
                                    kDigUno, kDigUno, kExe, kF1 },
     true, {}, false},
    /* v32 eqn-p01 (REAL): POLY grado 2, 2x^2-7x+3=0 -> x1=3, x2=1/2
     * (algebra a mano: (7+-5)/4). En v31 (x^2-2x-3) el display mostro
     * x2=-1 ENTERO right-aligned MAS alla de x376 -> recortado en
     * astillas (forense v31). v32 elige raiz NO-ENTERA: el OS la
     * pinta como fraccion apilada dentro de la zona de texto.
     * kF2=POLY en Select Type; kF1 en el prompt Degree? = grado 2
     * (commit directo, frames v31); celdas a,b,c; kF1=SOLVE ->
     * cursor x1; el EXE del harness avanza a x2 -> bandas [1][2].     */
    {"eqn-p01", "1/2",            { kMenu, kXVar, kExit, kExit,
                                    kF2, kF1,
                                    kDigDos, kExe,
                                    kMenos, kDigSiete, kExe,
                                    kDigTres, kExe, kF1 },
     true, {}, false},
    /* v32 det-rA (convertida de mat-dA): el resultado-matriz de Trn es
     * una rejilla multi-banda invendible para el matcher (forense v31:
     * 4 clusters fusionados verticalmente -> "????"; la pantalla
     * final de mat-dA/mostraba el eco [[1,3][2,4]] como rejilla).
     * Oraculo ESCALAR: Det([[1,2][3,4]]) = 1*4-2*3 = -2 (algebra a
     * mano). Det = F3 de la pagina 1 de MAT (F-tab v13:
     * Mat|Mat->Lst|Det|Trn|Augment). El literal de matriz esta
     * acreditado ('[' = SHIFT+Mas, ']' = SHIFT+Menos, echo de mat-dA
     * v31); MathIO auto-cierra la funcion OPTN (Trn, acreditado). El
     * EXE del harness ejecuta; -2 right-aligned -> banda "-2" (el
     * '-' calibrado es 10x2, el '2' 13x19).                            */
    {"det-rA", "-2",              { kOptn, kF2, kF3,
                                    kShift, kMas, kShift, kMas,
                                    kDigUno, kComa, kDigDos,
                                    kShift, kMenos, kShift, kMas,
                                    kDigTres, kComa, kDigCuatro,
                                    kShift, kMenos, kShift, kMenos },
     true},
    /* v34 det-rB (forense sonda v32): Det de la SUMA produjo el
     * dialogo "Invalid Type" (Det() del OS NO acepta una expresion de
     * matrices como argumento: OS-truth para iOS; Det exige variable o
     * literal directo). Ademas el estado del submenu MAT de OPTN se
     * FILTRA del caso anterior (det-rA rep2 paso con kOptn muerto: la
     * pagina/submenu ya estaba abierta). FIX: kExit de prefijo (cierra
     * el submenu filtrado; patron recovery) + segundo literal SIN
     * suma: Det([[7,5][2,3]]) = 7*3-5*2 = 11 (algebra a mano).
     * Mismas teclas probadas de det-rA. volcar=true documenta el
     * cierre del submenu filtrado.                                    */
    {"det-rB", "11",              { kExit, kOptn, kF2, kF3,
                                    kShift, kMas, kShift, kMas,
                                    kDigSiete, kComa, kDigCinco,
                                    kShift, kMenos, kShift, kMas,
                                    kDigDos, kComa, kDigTres,
                                    kShift, kMenos, kShift, kMenos },
     true},    /* v32 hyp-rB (convertida de dim-dA): Dim( con literal es Syntax
     * ERROR (frames v31: Dim exige variable de matriz y la asignacion
     * no esta inventariada). Conversion por la ruta REAL ya vista en
     * los frames de rnd-dA v31: OPTN pag 2 F2 = HYP
     * (sinh|cosh|tanh|sinh^-1|cosh^-1|tanh^-1). F2 = cosh. cosh(1) =
     * (e+e^-1)/2 = 1.5430806348152437 -> 10 digitos del OS:
     * 1.543080635 (redondeo d11=8 sube). MathIO auto-cierra.           */
    {"hyp-rB", "1.543080635",     { kOptn, kF6, kF2, kF2, kDigUno },
     true},
    /* v34 stat-dA (forense sonda v32): el flujo CALC->1-VAR de v32 era
     * CORRECTO (p11-p14: datos {1,2,3,4,5}, F-tab pag1, CALC sub-tab
     * 1-VAR|2-VAR|REG|SET, pantalla 1-Variable PERFECTA en p14) pero
     * (1) el EXE del harness DEVUELVE al editor y el F-tab CALC queda
     * activo (final1 = editor + sub-tab) -> el decode leyo el editor;
     * FIX: post={kF1} re-abre los resultados antes del decode
     * (p13->p14 probado: desde el sub-tab, kF1=1-VAR -> resultados).
     * (2) el F-tab del editor PERSISTE por app entre casos (stat-dB
     * rep2: el kF6 cayo en pag1 y DEL-ALL abrio INTR). FIX: kExit
     * normaliza al entrar (sub-tab -> pag1; en pag1 inofensivo:
     * pantalla raiz, patron Select Type) + DEL-ALL SIEMPRE (la List 1
     * persiste: leccion v28 n=10 duplicado) + kF6 de regreso a pag1
     * antes de CALC. Calib por POSICION en el orden izq->der del
     * REPLICA de bandas (banda[5]: 0='n', 1='=', 2='5', 3=flecha sin
     * calibrar -> '?' si el OS la clusteriza; el orden lo confirma el
     * log de la sonda v34). volcar=true.                               */
    /* v35: F-tab pag2[TOOL..INSERT]; kF6 #1 -> pag3[CSV]; 2do kF6 -> pag1; kF2=CALC; kF1=1-VAR. (forense v34 p16->p17) */
    /* v36: +kF6 #2 landa pag1 (confirmado forense v35: p18=v34p3 hash); hedges: kExit antes del CALC; 2do kF1 si el final es el sub-tab. (forense v35) */
    /* v37: +kF1 extra (refuerzo: si el final es el sub-tab, selecciona 1-VAR). */
    /* v41: band[5] de 1-Variable tiene 3 clusters [0]='n',[1]='=',[2]='5'/'4'
     * (19px, misma fuente que calib_digitos). AplicarCalibraciones indexa
     * desde la DERECHA (size-1-desde): desde=2 -> 'n', desde=1 -> '='.
     * Los digitos '5' y '4' se decodifican exclusivamente contra calib_digitos
     * (cero circularidad).                                                 */
    {"stat-dA", "n=5",            { kMenu, kDigDos, kExit,
                                    kF6, kF4, kF1,
                                    kDigUno, kExe, kDigDos, kExe,
                                    kDigTres, kExe, kDigCuatro, kExe,
                                    kDigCinco, kExe,
                                    kF6, kF6, kExit, kF2, kF1, kF1, kF1 },
     true, { kF1 }, true,
     { {5, 3, 'n'}, {5, 2, '='}, {5, 0, '\0'} }},
    /* v32 hyp-rA: sinh(1) = 1.175201194 */
    {"hyp-rA", "1.175201194",     { kOptn, kF6, kF2, kF1, kDigUno },
     true},
    /* v41 stat-dB: mismo calibrador de etiquetas ('n','=') para lotes --range */
    {"stat-dB", "n=4",            { kMenu, kDigDos, kExit,
                                    kF6, kF4, kF1,
                                    kDigDos, kExe, kDigCuatro, kExe,
                                    kDigSeis, kExe, kDigOcho, kExe,
                                    kF6, kF6, kExit, kF2, kF1, kF1, kF1 },
     true, { kF1 }, true,
     { {5, 3, 'n'}, {5, 2, '='}, {5, 0, '\0'} }},
    /* v41 bas-dA (forense v40): SETUP->Mode->Bin + F1(d~o)->F1(d) + 22 -> bin */
    {"bas-dA", "0000000000010110", { kMenu, kDigUno,
                                     kShift, kMenu, kAba, kF4, kExit,
                                     kF1, kF1, kDigDos, kDigDos },
     true},
    /* v41 bas-dB (forense v40): SETUP->Mode->Oct + F1(d~o)->F1(d) + 255 -> oct */
    {"bas-dB", "00000000377",      { kMenu, kDigUno,
                                     kShift, kMenu, kAba, kF5, kExit,
                                     kF1, kF1, kDigDos, kDigCinco, kDigCinco },
     true},
    /* v41 setup-dD: garantiza Mode=Comp, fija Fix 3 en Display, 1/3 -> 0.333,
     * restaura Norm 1 y verifica 1/3 -> fraccion [1][3].                    */
    {"setup-dD", "1/3",           { kMenu, kDigUno,
                                    kShift, kMenu, kAba, kF1, kExit,
                                    kShift, kMenu,
                                    kAba, kAba, kAba, kAba, kAba, kAba,
                                    kAba, kAba, kAba, kAba, kAba, kAba,
                                    kF1, kDigTres, kExe, kExit,
                                    kDigUno, kFrac, kDigTres, kExe,
                                    kShift, kMenu,
                                    kAba, kAba, kAba, kAba, kAba, kAba,
                                    kAba, kAba, kAba, kAba, kAba, kAba,
                                    kF3, kDigUno, kExit,
                                    kDigUno, kFrac, kDigTres },
     true},
};
constexpr std::size_t kNumCasos = sizeof(kCasos) / sizeof(kCasos[0]);

/* v31: aplica las calibraciones por eco de un caso ANTES de decodificar
 * y registra el eco de la banda para su revision contra el frame
 * correspondiente (el aserto humano lo hace el dueno sobre el frame y
 * el log de la sonda).                                            */
void AplicarCalibraciones(const Caso& caso)
{
    if (caso.calib.empty()) {
        return;
    }
    const std::vector<BandaTexto> bandas = ExtraerBandas();
    for (const CalibCelda& cb : caso.calib) {
        if (cb.banda < 0 || cb.banda >= static_cast<int>(bandas.size())) {
            std::fprintf(stderr, "[BATTERY] calib %s: banda %d fuera de "
                         "rango (%d bandas)\n", caso.etiqueta, cb.banda,
                         static_cast<int>(bandas.size()));
            continue;
        }
        std::vector<Caja> cajas;
        ClustersEnBanda(bandas[cb.banda].f0, bandas[cb.banda].f1, &cajas);
        if (cb.desde < 0 || cb.desde >= static_cast<int>(cajas.size())) {
            std::fprintf(stderr, "[BATTERY] calib %s: cluster %d fuera de "
                         "rango (banda %d, %d clusters)\n", caso.etiqueta,
                         cb.desde, cb.banda,
                         static_cast<int>(cajas.size()));
            continue;
        }
        const Caja& caja = cajas[cajas.size() - 1 - cb.desde];
        bool yaCalib = false;
        for (const auto& par : g_calibradas) {
            if (par.first == cb.c && DivergenciaGlifo(par.second, caja) == 0) { yaCalib = true; break; }
        }
        if (!yaCalib) g_calibradas.emplace_back(cb.c, caja);
    }
    /* Eco de revision: la banda calibrada, decodificada con la tabla
     * vigente (incluye lo recien calibrado).                          */
    const int bandaEco = caso.calib.front().banda;
    if (bandaEco >= 0 && bandaEco < static_cast<int>(bandas.size())) {
        std::vector<Caja> cajas;
        ClustersEnBanda(bandas[bandaEco].f0, bandas[bandaEco].f1, &cajas);
        std::string t;
        for (const Caja& c : cajas) {
            const char ch = DecodificarCaja(c);
            if (ch != '\0') t += ch;
        }
        std::fprintf(stderr, "[BATTERY] calib eco %s banda=%d (%d "
                     "clusters): '%s'\n", caso.etiqueta, bandaEco,
                     static_cast<int>(cajas.size()), t.c_str());
    }
}

} /* namespace */

int main(int argc, char** argv)
{
    if (argc < 5) {
        std::fprintf(stderr,
                     "uso: %s <SystemROM> <UserROM> <C2ROM> <outdir|-> "
                     "[segBoot] [soloCaso]\n", argv[0]);
        return 2;
    }
    g_dir = argv[4];
    /* modo iteración:optativo. Con soloCaso, se corren SOLO boot+calibra-
     * ción+ese caso (recibos por tecla si volcar=1). La salida 0 exige de
     * todos modos la batería COMPLETA: una corrida filtrada nunca
     * certifica «batería verde».                     */
    const char* soloCaso = argc > 6 ? argv[6] : nullptr;
    std::size_t rangeIni = 0, rangeFin = kNumCasos;
    bool modoRango = false;
    std::vector<std::string> soloLista;
    if (soloCaso != nullptr && std::strcmp(soloCaso, "-") != 0 &&
        std::strcmp(soloCaso, "all") != 0) {
        const std::string s(soloCaso);
        if (s.rfind("range:", 0) == 0) {
            const std::size_t sep = s.find(':', 6);
            if (sep != std::string::npos) {
                rangeIni = static_cast<std::size_t>(std::strtoull(s.substr(6, sep - 6).c_str(), nullptr, 10));
                rangeFin = static_cast<std::size_t>(std::strtoull(s.substr(sep + 1).c_str(), nullptr, 10));
                if (rangeFin > kNumCasos) rangeFin = kNumCasos;
                modoRango = true;
            }
        } else {
            std::size_t pos = 0;
            while (pos <= s.size()) {
                const std::size_t coma = s.find(',', pos);
                const std::string item = (coma == std::string::npos)
                    ? s.substr(pos) : s.substr(pos, coma - pos);
                if (!item.empty()) soloLista.push_back(item);
                if (coma == std::string::npos) break;
                pos = coma + 1;
            }
        }
    }
    int nJugados = 0;

    std::vector<std::uint8_t> sys, user, c2;
    if (!LeeRomExacta(argv[1], kTamSystem, &sys) ||
        !LeeRomExacta(argv[2], kTamUser, &user) ||
        !LeeRomExacta(argv[3], kTamC2, &c2)) {
        return 2;
    }

    g_core = cg50core_create();
    if (g_core == nullptr) {
        std::fprintf(stderr, "[BATTERY] cg50core_create fallo\n");
        return 3;
    }
    const std::uint8_t* ps = sys.empty() ? nullptr : sys.data();
    const std::uint8_t* pu = user.empty() ? nullptr : user.data();
    const std::uint8_t* pc = c2.empty() ? nullptr : c2.data();
    if (cg50core_load_firmware_raw(g_core, ps, sys.size(), pu, user.size(),
                                   pc, c2.size()) != CG50CORE_OK) {
        Cg50Status st{};
        char diag[256];
        cg50core_status(g_core, &st);
        cg50core_last_error(g_core, diag, sizeof(diag));
        std::fprintf(stderr, "[BATTERY] carga rechazada: pc=0x%08X diag=%s\n",
                     st.pc, diag);
        cg50core_destroy(g_core);
        return 3;
    }
    if (cg50core_set_option(g_core, CG50CORE_OPT_SLEEP_ADVANCE, 1) !=
        CG50CORE_OK) {
        std::fprintf(stderr, "[BATTERY] aviso: sin SLEEP_ADVANCE\n");
    }

    /* --- arranque FRIO o CALIENTE (warm-boot, mision �9) ----------------
     * Frio: ventana COMPLETA + estabilidad + MENU->EXE (ruta probada).
     * Caliente: cg50core_restore del snapshot �warm_state.bin� del outdir,
     * tomado exactamente al final de un arranque frio (tras MENU->EXE).
     * El snapshot es estado del MISMO motor con las ROMs canonicas; tras
     * restore se EXIGE estado Ready + barra de estado; cualquier fallo
     * cae a frio recargando firmware. Equivalencia frio-vs-caliente: la
     * misma suite corre por ambas vias y los result files se cotejan.   */
    const bool modoWarm = argc > 7 && std::strcmp(argv[7], "warm") == 0;
    const std::string rutaWarm = (fs::path(g_dir) / "warm_state.bin").string();
    bool caliente = false;
    bool habiaSnapshot = false;
    if (modoWarm) {
        std::FILE* fw = std::fopen(rutaWarm.c_str(), "rb");
        if (fw != nullptr) {
            habiaSnapshot = true;
            std::fseek(fw, 0L, SEEK_END);
            const long nBytes = std::ftell(fw);
            std::rewind(fw);
            bool leido = nBytes > 0;
            std::vector<std::uint8_t> blob;
            if (leido) {
                blob.resize(static_cast<std::size_t>(nBytes));
                leido = std::fread(blob.data(), 1, blob.size(), fw) ==
                        blob.size();
            }
            std::fclose(fw);
            int rc = CG50CORE_ERR_ARG;
            if (leido) {
                rc = cg50core_restore(g_core, blob.data(), blob.size());
            }
            const int estadoMaquina = cg50core_state(g_core);
            CopiaPantalla();
            if (rc == CG50CORE_OK && estadoMaquina == 1 &&
                BarraEstadoPresente()) {
                caliente = true;
                std::fprintf(stderr, "[BATTERY] arranque CALIENTE: restore "
                             "%ld bytes OK, barra de estado verificada\n",
                             nBytes);
            } else {
                std::fprintf(stderr, "[BATTERY] restore fallo (rc=%d "
                             "estado=%d): cae a arranque frio\n", rc,
                             estadoMaquina);
                if (cg50core_load_firmware_raw(g_core, ps, sys.size(), pu,
                                               user.size(), pc, c2.size()) !=
                    CG50CORE_OK) {
                    std::fprintf(stderr, "[BATTERY] recarga tras restore "
                                 "fallida\n");
                    cg50core_destroy(g_core);
                    return 3;
                }
            }
        } else {
            std::fprintf(stderr, "[BATTERY] warm: sin snapshot previo "
                         "(se creara al final del frio)\n");
        }
    }

    if (!caliente) {
    /* --- boot real: la ventana COMPLETA antes de teclear. El smoke v7
     * corrio los 14 s integros; la corrida 1 de esta bateria corto en la
     * primera GRAM (chunk 5 = 0.08 s sim) y el OS ignoro TODAS las teclas
     * (hash DCE621980851884D constante en los 8 casos). Ahora: ventana
     * completa + estabilidad de pantalla antes del primer pulso.         -- */
    double segBoot = argc > 5 ? std::atof(argv[5]) : 14.0;
    if (segBoot < 1.0) segBoot = 1.0;
    const int chunksBoot = static_cast<int>(segBoot * 1e6 / kChunkUs);
    bool hayGram = false;
    int primeraGramChunk = -1;
    for (int chunk = 0; chunk < chunksBoot; ++chunk) {
        cg50core_tick(g_core, kChunkUs, 0);
        Cg50FrameInfo info{};
        if (cg50core_frame_info(g_core, &info) == CG50CORE_OK &&
            CopiaPantalla() && !hayGram) {
            hayGram = true;
            primeraGramChunk = chunk;
            std::fprintf(stderr, "[BATTERY] primera pantalla GRAM: chunk %d\n",
                         chunk);
        }
    }
    if (!hayGram) {
        Cg50Status st{};
        cg50core_status(g_core, &st);
        std::fprintf(stderr, "[BATTERY] sin GRAM tras %.1fs: inst=%" PRIu64
                     " cpu=%d\n", segBoot, st.instructions, st.cpuState);
        cg50core_destroy(g_core);
        return 4;
    }
    /* Estabilidad: pantalla sin cambios 30 chunks seguidos (cap 10 s).   */
    {
        std::uint64_t estable = HashPantalla();
        int sinCambio = 0;
        for (int chunk = 0; chunk < 600 && sinCambio < 30; ++chunk) {
            cg50core_tick(g_core, kChunkUs, 0);
            const std::uint64_t ahora = HashPantalla();
            if (ahora == estable && ahora != 0) ++sinCambio;
            else { estable = ahora; sinCambio = 0; }
        }
        std::fprintf(stderr, "[BATTERY] boot estable: hash=%016" PRIX64
                     "\n", estable);
    }

    /* Entrar a RUN-MAT (ruta probada del smoke v4): MENU -> EXE.
     * Cada pulso EXIGE reaccion: si la pantalla no cambia, el guest no
     * registro la tecla y seguir aqui es inutil (fail-closed).           */
    {
        const std::uint64_t antesMenu = HashPantalla();
        if (Pulso(kMenu, 2000) == 0) { cg50core_destroy(g_core); return 3; }
        const std::uint64_t trasMenu = PulsoConEspera(kExe, 2000, 800);
        /* NOTA: menu y exe van separados; si el menu no movio la pantalla */
        if (trasMenu == antesMenu || trasMenu == 0) {
            std::fprintf(stderr, "[BATTERY] MENU/EXE sin reaccion de "
                         "pantalla: navegacion rota\n");
            cg50core_destroy(g_core);
            return 3;
        }
        std::fprintf(stderr, "[BATTERY] MENU->EXE reacciono: %016" PRIX64
                     " -> %016" PRIX64 "\n", antesMenu, trasMenu);
    }
    if (!BarraEstadoPresente()) {
        std::fprintf(stderr, "[BATTERY] sin barra de estado tras MENU->EXE: "
                     "no se entro a RUN-MAT\n");
        cg50core_destroy(g_core);
        return 3;
    }
    if (modoWarm) {
        /* Guardar el snapshot al final del arranque frio exitoso: el
         * estado exacto en el que las sondas empiezan a teclear.         */
        const size_t tamWarm = cg50core_save(g_core, nullptr, 0);
        if (tamWarm > 0) {
            std::vector<std::uint8_t> blob(tamWarm);
            const size_t escrito =
                cg50core_save(g_core, blob.data(), tamWarm);
            if (escrito == tamWarm) {
                std::FILE* fw = std::fopen(rutaWarm.c_str(), "wb");
                if (fw != nullptr) {
                    std::fwrite(blob.data(), 1, escrito, fw);
                    std::fclose(fw);
                    std::fprintf(stderr, "[BATTERY] snapshot warm guardado: "
                                 "%zu bytes\n", escrito);
                }
            }
        }
    }
    } /* fin !caliente */

    /* --- calibración: teclas OURS �?' anclas; el guest pinta resultados --- */
    g_calibradas.clear();
    /* Dígitos desde UN SOLO ancla «1234567890»: el resultado renderiza 10
     * clusters en orden; cluster i = dígito i+1. La v6/v7 demostró que la
     * calibración del '4' (5.º ancla individual, justo en el borde de
     * scroll) capturó un fragmento determinista de 5x5; el ancla única
     * corre temprano, sin scroll, y evita esa zona.                      */
    if (!CalibrarResultado('#',
                           {kDigUno, kDigDos, kDigTres, kDigCuatro, kDigCinco,
                            kDigSeis, kDigSiete, kDigOcho, kDigNueve,
                            kDigCero}, 0)) {
        std::fprintf(stderr, "[BATTERY] calibracion ancla digitos fallo\n");
        cg50core_destroy(g_core);
        return 6;
    }
    {
        BandaTexto banda;
        if (!BandaResultadoTrasExe({kDigUno, kDigDos, kDigTres, kDigCuatro,
                                    kDigCinco, kDigSeis, kDigSiete,
                                    kDigOcho, kDigNueve, kDigCero},
                                   &banda)) {
            std::fprintf(stderr, "[BATTERY] ancla digitos: sin banda\n");
            cg50core_destroy(g_core);
            return 6;
        }
        std::vector<Caja> cajas;
        ClustersEnBanda(banda.f0, banda.f1, &cajas);
        std::fprintf(stderr, "[BATTERY] ancla digitos: %zu clusters\n",
                     cajas.size());
        static const char kOrden[10] = {'1','2','3','4','5','6','7','8',
                                        '9','0'};
        if (cajas.size() != 10) {
            std::fprintf(stderr, "[BATTERY] se esperaban 10 clusters\n");
            cg50core_destroy(g_core);
            return 6;
        }
        for (int i = 0; i < 10; ++i) {
            g_calibradas.emplace_back(kOrden[i], cajas[i]);
            std::fprintf(stderr, "[BATTERY] calibR '%c': %dx%d @x%d y%d\n",
                         kOrden[i], cajas[i].ancho(), cajas[i].alto(),
                         cajas[i].x0, cajas[i].y0);
        }
        /* La entrada '#' del ancla (bitmap del '1') hace de sombra: el
         * decodificador devuelve el PRIMER match exacto y todo '1' salía
         * '#' (v8b: «#########0»). Se elimina: los dígitos quedan del
         * bloque explícito.                                                 */
        g_calibradas.erase(g_calibradas.begin());
        (void)EscribeBmpBgr(g_dir, "calib_digitos", g_disp);
    }
    /* «0.5» �?' 3 clusters; el punto es el más pequeño.                     */
    if (!CalibrarResultado('.', {kDigCero, kPunto, kDigCinco}, -1) ||
        !EscribeBmpBgr(g_dir, "calib_punto", g_disp)) {
        std::fprintf(stderr, "[BATTERY] calibracion '.' fallo\n");
        cg50core_destroy(g_core);
        return 6;
    }
    /* Glifos de resultados firmados/complejos:
     * '�^'' desde el resultado «-0.5» (ancla 0�^'0.5, cluster 0).
     * 'i' desde «1i» via OPTN F3 F1 (CPLX �?' i), cluster �sLTIMO: el
     * resultado puede ser «i» o «1i» y en ambos 'i' es el último.        */
    if (!CalibrarResultado('-',
                           {kDigCero, kMenos, kDigCero, kPunto, kDigCinco},
                           0) ||
        !EscribeBmpBgr(g_dir, "calib_menos", g_disp)) {
        std::fprintf(stderr, "[BATTERY] calibracion '-' fallo\n");
        cg50core_destroy(g_core);
        return 6;
    }
    /* '_' = '�^'' de OPERADOR en resultados complejos (separador real/
     * imaginario): la corrida v8b demostró que su forma difiere del
     * '�^'' inicial («-237?3##6?»: el medio no matcheó). Ancla «2-3i» �?'
     * resultado «2-3i»: cluster 1 = '�^'' operador, cluster 3 = 'i'.      */
    if (!CalibrarResultado('_',
                           {kDigDos, kMenos, kDigTres, kOptn, kF3, kF1},
                           1) ||
        !EscribeBmpBgr(g_dir, "calib_menosop", g_disp)) {
        std::fprintf(stderr, "[BATTERY] calibracion '_' fallo\n");
        cg50core_destroy(g_core);
        return 6;
    }
    /* 'i' del resultado complejo: mismo ancla «2-3i», cluster �sLTIMO.    */
    if (!CalibrarResultado('i',
                           {kDigDos, kMenos, kDigTres, kOptn, kF3, kF1},
                           -2) ||
        !EscribeBmpBgr(g_dir, "calib_i", g_disp)) {
        std::fprintf(stderr, "[BATTERY] calibracion 'i' fallo\n");
        cg50core_destroy(g_core);
        return 6;
    }
    /* '+' de OPERADOR en resultados complejos (v25, CPLX-02/05): ancla
     * �1+2i� -> resultado �1+2i� (4 clusters: '1','+','2','i'; cluster
     * 1 = '+'). El modo complejo esta activo por defecto del ROM en
     * frio (demostrado: las anclas '_'/'i' de �2-3i� calibran en la
     * corrida completa). El '+' de operador es el mismo glifo que
     * necesitaran los resultados tipo �3+4i�.                        */
    if (!CalibrarResultado('+',
                           {kDigUno, kMas, kDigDos, kOptn, kF3, kF1},
                           1) ||
        !EscribeBmpBgr(g_dir, "calib_mas", g_disp)) {
        std::fprintf(stderr, "[BATTERY] calibracion '+' fallo\n");
        cg50core_destroy(g_core);
        return 6;
    }
    /* Llaves y coma de LISTA (v23b): ancla SIN warning - la lista
     * literal {1,2} tecleada (SHIFT+�- abre, SHIFT+÷ cierra; hallazgo
     * del agente FINANCIERO) + EXE rende el resultado «{1,2}» (5
     * clusters: '{','1',',','2','}'). Identidad: teclas OURS.           */
    {
        const std::vector<std::uint16_t> anclaLista = {
            kShift, kPor, kDigUno, kComa, kDigDos, kShift, kEntre};
        BandaTexto bandaLista;
        if (!BandaResultadoTrasExe(anclaLista, &bandaLista)) {
            std::fprintf(stderr, "[BATTERY] ancla {1,2} fallo\n");
            cg50core_destroy(g_core);
            return 6;
        }
        std::vector<Caja> cajas;
        ClustersEnBanda(bandaLista.f0, bandaLista.f1, &cajas);
        if (cajas.size() != 5) {
            std::fprintf(stderr, "[BATTERY] ancla {1,2}: %zu clusters "
                         "(esperados 5)\n", cajas.size());
            cg50core_destroy(g_core);
            return 6;
        }
        g_calibradas.emplace_back('{', cajas[0]);
        g_calibradas.emplace_back(',', cajas[2]);
        g_calibradas.emplace_back('}', cajas[4]);
        std::fprintf(stderr,
                     "[BATTERY] calibR '{': %dx%d, ',': %dx%d, '}': %dx%d\n",
                     cajas[0].ancho(), cajas[0].alto(),
                     cajas[2].ancho(), cajas[2].alto(),
                     cajas[4].ancho(), cajas[4].alto());
        (void)EscribeBmpBgr(g_dir, "calib_llaves", g_disp);
    }
    /* sanidad: repetir el ancla del '5' �?' mismo bitmap (determinismo).
     * La entrada temporal '@' se retira para no sombrear nada.            */
    CalibrarResultado('@', {kDigCinco}, 0);
    {
        const Caja* a = nullptr;
        const Caja* b = nullptr;
        for (const auto& par : g_calibradas) {
            if (par.first == '@') b = &par.second;
            if (par.first == '5' && a == nullptr) a = &par.second;
        }
        if (a != nullptr && b != nullptr) {
            const int dif = DivergenciaGlifo(*a, *b);
            std::fprintf(stderr, "[BATTERY] calibR '5' repetida: %s "
                         "(dif=%d)\n", dif == 0 ? "IGUAL" : "DISTINTA", dif);
        }
    }
    for (auto it = g_calibradas.begin(); it != g_calibradas.end();) {
        it = (it->first == '@') ? g_calibradas.erase(it) : it + 1;
    }

    /* --- casos --------------------------------------------------------- */
    int pasados = 0, divergencias = 0, decodificables = 0;
    std::FILE* resumen = nullptr;
    if (std::strcmp(g_dir, "-") != 0) {
        const std::string rutaResumen =
            (fs::path(g_dir) / "battery_result.txt").string();
        resumen = std::fopen(rutaResumen.c_str(), "wb");
    }
    for (std::size_t idxCaso = 0; idxCaso < kNumCasos; ++idxCaso) {
        const Caso& caso = kCasos[idxCaso];
        if (modoRango) {
            if (idxCaso < rangeIni || idxCaso >= rangeFin) continue;
        } else if (!soloLista.empty()) {
            bool soloCoincide = false;
            for (const std::string& s : soloLista) {
                if (s == caso.etiqueta) { soloCoincide = true; break; }
            }
            if (!soloCoincide) continue;
        }
        ++nJugados;
        std::fprintf(stderr, "[BATTERY] caso %s\n", caso.etiqueta);
        std::uint64_t hash1 = 0, hash2 = 0;
        std::string texto1, texto2;       /* texto de la banda del resultado */
        std::string bandas1, bandas2;     /* TODAS las bandas, evidencia.    */
        for (int rep = 0; rep < 2; ++rep) {
            /* Recuperación: tras un Syntax ERROR el guest queda modal y
             * AC no limpia (corrida v6: el error envenenó los casos
             * siguientes). EXIT descarta el diálogo/entrada y AC deja la
             * línea limpia. En pantalla normal son inocuos.                */
            Pulso(kExit, 150); Tic(20);
            Pulso(kAcOn, 120);
            Tic(60);
            /* Re-entrada RUN-MAT (diseño del suite importado, sin
             * ejecutar por su autor): MENU �?' EXE reabre la app, cierra
             * diálogos residuales y, si limpia el historial, lo veremos
             * en los recibos bandas1/bandas2. La limpieza del historial
             * NO es requisito del matcheo v13: al aceptar solo la
             * �sLTIMA banda, las residuales no pueden envenenar (esa es
             * la defensa primaria); esto es higiene adicional.          */


            /* v31: kExe -> kDigUno (los NUMEROS del MAIN MENU abren la app
             * directamente; frame c_stat-dB_p2 de la sonda v29:
             * kMenu,kDigDos abrio STAT). kExe re-entra la ULTIMA app y
             * arrastra su estado (lecciones v29/v30: fue la contaminacion
             * de los 8 diags v28 y el desvio de eqn-dC/siml02).
             * kDigUno = icono 1 = RUN-MAT limpio.                        */            if (Pulso(kMenu, 2000) == 0) {
                std::fprintf(stderr, "[BATTERY]   MENU sin reaccion (caso %s"
                             " rep %d)\n", caso.etiqueta, rep + 1);
            }
            if (PulsoConEspera(kDigUno, 2000, 800) == 0) {
                std::fprintf(stderr, "[BATTERY]   tecla 1 re-entrada sin reaccion"
                             " (caso %s rep %d)\n", caso.etiqueta, rep + 1);
            }
            Tic(120);
            if (!BarraEstadoPresente()) {
                std::fprintf(stderr, "[BATTERY]   re-entrada no volvio a "
                             "RUN-MAT; reintento (caso %s rep %d)\n",
                             caso.etiqueta, rep + 1);
                Pulso(kMenu, 2000);
                PulsoConEspera(kDigUno, 2000, 800);
                Tic(120);
                if (!BarraEstadoPresente()) {
                    std::fprintf(stderr, "[BATTERY]   re-entrada FALLIDA "
                                 "definitiva (caso %s rep %d): el caso "
                                 "probablemente no ejecute su teclado en "
                                 "RUN-MAT\n", caso.etiqueta, rep + 1);
                }
            }
            int paso = 0;
            for (const std::uint16_t k : caso.teclas) {
                const std::uint64_t antesT = HashPantalla();
                const std::uint64_t ahoraT =
                    PulsoConEspera(k, 150, 400);
                if (ahoraT == antesT) {
                    std::fprintf(stderr, "[BATTERY]   tecla 0x%04X sin "
                                 "reaccion (caso %s rep %d)\n",
                                 k, caso.etiqueta, rep + 1);
                }
                Tic(2);
                ++paso;
                if (caso.volcar && rep == 0) {
                    EstabilizarPantalla(12, 40);
                    const std::string tagV =
                        "c_" + std::string(caso.etiqueta) + "_p" +
                        std::to_string(paso);
                    (void)EscribeBmpBgr(g_dir, tagV.c_str(), g_disp);
                }
            }
            const std::uint64_t final =
                PulsoConEspera(kExe, 150, 2500);
            EstabilizarPantalla(20, 60);
            /* v16 post-EXE: S<->D, ANS y cadenas. Cada tecla exige
             * reaccion; un kExe dentro repite la estabilidad completa.  */
            {
                int pasoPost = 0;
                for (const std::uint16_t k : caso.post) {
                    const std::uint64_t antesP = HashPantalla();
                    const std::uint64_t ahoraP =
                        PulsoConEspera(k, 150, 600);
                    if (ahoraP == antesP) {
                        std::fprintf(stderr, "[BATTERY]   post 0x%04X sin "
                                     "reaccion (caso %s rep %d)\n",
                                     k, caso.etiqueta, rep + 1);
                    }
                    Tic(2);
                    if (k == kExe) {
                        EstabilizarPantalla(20, 60);
                    }
                    if (caso.volcar && rep == 0) {
                        EstabilizarPantalla(12, 40);
                        (void)EscribeBmpBgr(g_dir,
                            ("c_" + std::string(caso.etiqueta) + "_post" +
                             std::to_string(pasoPost)).c_str(),
                            g_disp);
                    }
                    ++pasoPost;
                }
            }
            /* v31: calibraciones por eco de teclas (eqn-cal) antes de
             * decodificar.                                                  */
            AplicarCalibraciones(caso);
            const std::vector<std::string> bandas = DecodificarBandas();
            /* usable: una banda SIN '?' que contenga la referencia. La
             * banda del resultado del guest contiene solo glifos del
             * resultado; '?' marcaria un glifo sin calibrar.             */
            std::string match;
            std::string todas;
            std::size_t nBandas = bandas.size();
            for (std::size_t bi = 0; bi < nBandas; ++bi) {
                const std::string& t = bandas[bi];
                todas += "[" + t + "]";
            }
            /* Igualdad EXACTA y SOLO la ULTIMA banda (v13): el recibo
             * determinista del mini-corridor v10 («385») confirmó que el
             * resultado fresco de RUN-MAT es SIEMPRE la última banda de
             * la lista top�?'down, con residuos del historial ARRIBA
             * ([5][??][?????][???][385]). Con esperadas cortas ('1','3',
             * '10','30') en una batería de 52 casos, una banda residual
             * podía envenenar; limitada a la última, deja de poder.   */
            if (nBandas > 0 && !bandas[nBandas - 1].empty() &&
                bandas[nBandas - 1].find('?') == std::string::npos &&
                bandas[nBandas - 1] == caso.esperada) {
                match = bandas[nBandas - 1];
            }
            /* Fracción S-D de MathIO (v10): el �^� se dibuja 81/4 con la
             * barra ENTRE bandas adyacentes (v9: [??????][81][4]); no
             * existe banda única que contenga «81/4». Matcheo
             * estructural: numerador = banda i, denominador = banda
             * i+1, ambas sin '?', en orden top�?'down de ExtraerBandas.
             * v13: SOLO el �sLTIMO par (tal como en el matcheo plano, el
             * par fresco siempre es el último; así un par residual del
             * historial tampoco puede envenenar).                    */
            if (match.empty()) {
                const std::string esperadaT(caso.esperada);
                const std::size_t barra = esperadaT.find('/');
                if (barra != std::string::npos && barra > 0 &&
                    barra + 1 < esperadaT.size()) {
                    const std::string numerador =
                        esperadaT.substr(0, barra);
                    const std::string denominador =
                        esperadaT.substr(barra + 1);
                    if (nBandas >= 2 &&
                        bandas[nBandas - 2] == numerador &&
                        bandas[nBandas - 1] == denominador &&
                        bandas[nBandas - 2].find('?') == std::string::npos &&
                        bandas[nBandas - 1].find('?') == std::string::npos) {
                        match = esperadaT;
                    }
                }
            }
            /* v27d EQUATION (OLA5 �9.2): pantallas de soluciones con
             * PAR/N de bandas (X1=4 / X2=3; X=3 / Y=2; rejilla a,b,c).
             * OPT-IN por caso (caso.parBandas): las esperadas de
             * salida-lista ({-2,2}) llevan comas y NUNCA deben entrar
             * a esta rama (falso-paso si la banda �nica falla). Con
             * opt-in: esperada "p1,p2[,p3...]" -> las �ltimas k bandas
             * (en orden top->down) == las partes, cada una sin '?'
             * (el par fresco; un par residual del historial no
             * envenena). El formato real de las pantallas lo
             * confirman los frames de la sonda diag v27eqn.          */
            if (match.empty() && caso.parBandas) {
                const std::string esperadaP(caso.esperada);
                std::vector<std::string> partes;
                std::size_t ini = 0;
                for (;;) {
                    const std::size_t c = esperadaP.find(',', ini);
                    if (c == std::string::npos) {
                        partes.push_back(esperadaP.substr(ini));
                        break;
                    }
                    partes.push_back(esperadaP.substr(ini, c - ini));
                    ini = c + 1;
                }
                const std::size_t k = partes.size();
                /* v29: las pantallas de soluciones EQUATION intercalan
                 * bandas de ETIQUETA con glifos sin calibrar ('?') -
                 * "X [" / "Y [" (frame c_siml-a_final1: X [ 3 / Y [ 2,
                 * valores right-aligned en celdas) - ANTES de las de
                 * valor. Tomar las ultimas k bandas LIBRES de '?' (en
                 * orden top->down); si hay menos de k, falla. El par
                 * fresco sigue siendo el ultimo: un residual del
                 * historial no puede envenenar (queda arriba).        */
                std::vector<std::size_t> libres;
                if (k >= 1) {
                    for (std::size_t i = 0; i < nBandas; ++i) {
                        if (!bandas[i].empty() &&
                            bandas[i].find('?') == std::string::npos) {
                            libres.push_back(i);
                        }
                    }
                }
                if (k >= 1 && libres.size() >= k) {
                    bool ok = true;
                    for (std::size_t p = 0; p < k && ok; ++p) {
                        const std::string& b =
                            bandas[libres[libres.size() - k + p]];
                        if (b != partes[p]) {
                            ok = false;
                        }
                    }
                    if (ok) {
                        match = esperadaP;
                    }
                }
            }
            const std::string tag =
                "c_" + std::string(caso.etiqueta) + "_final" +
                std::to_string(rep + 1);
            (void)EscribeBmpBgr(g_dir, tag.c_str(), g_disp);
            if (rep == 0) {
                hash1 = final; texto1 = match; bandas1 = todas;
            } else {
                hash2 = final; texto2 = match; bandas2 = todas;
            }
        }
        const bool determinista = !texto1.empty() && texto1 == texto2;
        const bool usable = !texto1.empty();
        std::fprintf(stderr,
                     "[BATTERY]   h=%016" PRIX64 "/%016" PRIX64 " det=%s "
                     "match1='%s' match2='%s' esperada='%s' bandas1=%s "
                     "-> %s\n",
                     hash1, hash2, determinista ? "SI" : "NO",
                     texto1.c_str(), texto2.c_str(), caso.esperada,
                     bandas1.c_str(), usable ? "PASO" : "DIVERGE");
        if (resumen != nullptr) {
            std::fprintf(resumen,
                         "%s determinismo=%s match1='%s' match2='%s' "
                         "esperada='%s'\n",
                         caso.etiqueta, determinista ? "SI" : "NO",
                         texto1.c_str(), texto2.c_str(), caso.esperada);
        }
        if (determinista) ++pasados;
        else if (usable) ++decodificables;
        else ++divergencias;
    }
    if (resumen != nullptr) {
        std::fprintf(resumen, "pasados=%d de %zu (jugados=%d); "
                     "divergencias=%d; decodificables=%d\n",
                     pasados, kNumCasos, nJugados, divergencias,
                     decodificables);
        std::fclose(resumen);
    }
    cg50core_destroy(g_core);
    /* La batería solo es verde si TODOS los casos definidos pasaron en
     * las 2 reps (una corrida con filtro no certifica nunca).           */
    if (nJugados == kNumCasos && kNumCasos > 0 &&
        pasados == kNumCasos) {
        std::fprintf(stderr, "[BATTERY] OK: BATERIA COMPLETA %d/%zu en "
                     "verde\n", pasados, kNumCasos);
        return 0;
    }
    if (pasados > 0 || nJugados > 0) {
        std::fprintf(stderr, "[BATTERY] parcial: %d/%zu pasados "
                     "(jugados=%d), %d divergencias, %d decodificables "
                     "(no es victoria: reporta)\n",
                     pasados, kNumCasos, nJugados, divergencias,
                     decodificables);
        return 7;
    }
    if (soloCaso != nullptr) {
        std::fprintf(stderr, "[BATTERY] filtro '%s' no coincide con "
                     "ningun caso\n", soloCaso);
        return 2;
    }
    return 6;
}

