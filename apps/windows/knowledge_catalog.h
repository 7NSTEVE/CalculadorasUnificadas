#pragma once
// ============================================================================
// knowledge_catalog.h
// Componente 1: Catalogo Determinista de Conocimiento CASIO (100% offline).
// Contiene:
//   - 150 teclas exactas (50 ClassWiz + 50 ES/VN PLUS + 50 fx-CG50) con 3 capas
//     (Normal, SHIFT, ALPHA), formulas LaTeX 2D, ejemplos, advertencias y PDF.
//   - 38 indicadores LCD nativos (20 ClassWiz + 18 ES PLUS) con hit-test por
//     sprite en indRect (pantalla principal y Pop-up Display).
//   - Procedimientos verificados por modelo ("Como hacer en Casio"),
//     busqueda determinista con expansion de sinonimos, comparador multi-modelo,
//     centro "Por que muestra esto?" + errores oficiales, trucos/cuidados,
//     perfiles recomendados, lista de comprobacion previa y favoritos locales.
//   - Serializacion determinista a JSON UTF-8 (catalog/casio_knowledge_catalog.json).
// ============================================================================

#include <windows.h>
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cwctype>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "lcd_indicators.h"
#include "operation_capture.h"

namespace CasioKnowledge {

struct KeyLayerInfo {
    bool available = false;
    std::wstring label;
    std::wstring category;
    std::wstring summary;
    std::wstring latexFormula;
    std::wstring latexExample;
    std::wstring tipOrWarning;
};

struct CatalogKeyRecord {
    int calcIdx = 0; // 0=ClassWiz fx-570/991LA X, 1=fx-570ES/VN PLUS, 2=fx-CG50
    int keyId = 0;   // 0..49
    std::wstring keyName;
    std::wstring pdfFile;
    int pdfPage = 1;
    KeyLayerInfo normal;
    KeyLayerInfo shift;
    KeyLayerInfo alpha;
    std::vector<int> demoSeq;
};

struct LcdIndicatorDoc {
    int calcIdx = 0;      // 0=ClassWiz, 1=ES PLUS
    int spriteIndex = 0;  // index in kClassWizSprites / kEsPlusSprites
    std::wstring symbol;
    std::wstring title;
    std::wstring meaning;
    std::wstring whenAppears;
    std::wstring relatedKeys;
    std::wstring pdfFile;
    int pdfPage = 5;
};

struct VerifiedModelSeq {
    bool supported = false;
    std::vector<int> keys;
    std::wstring stepsSummary;
    std::wstring expectedLcd;
    std::wstring pdfFile;
    int pdfPage = 1;
    std::wstring note;
};

struct ProcedureEntry {
    std::wstring id;
    std::wstring category;
    std::wstring title;
    std::wstring goalSummary;
    std::wstring latexFormula;
    std::wstring commonErrors;
    std::wstring tags;
    VerifiedModelSeq perModel[3];
};

struct DiagnosticScenario {
    std::wstring id;
    std::wstring symptomTitle;
    std::wstring category;
    std::wstring whyItHappens;
    std::wstring latexExplanation;
    std::wstring howToFix;
    std::wstring pdfRefCW;
    std::wstring pdfRefES;
    int quickFixCalcIdx = -1;
    std::vector<int> quickFixSeq;
};

struct CasioErrorDoc {
    std::wstring errorName;
    std::wstring appliesTo;
    std::wstring cause;
    std::wstring resolution;
    std::wstring pdfFile;
    int pdfPage = 1;
};

struct TipWarningCard {
    std::wstring id;
    std::wstring kind; // "Truco", "Advertencia", "Cuidado"
    std::wstring title;
    std::wstring summary;
    std::wstring latexDetail;
    std::wstring pdfRef;
    int calcIdx = 0;
    std::vector<int> demoSeq;
};

struct SetupProfilePreset {
    std::wstring id;
    std::wstring title;
    std::wstring description;
    std::wstring settingsSummary;
    std::wstring warning;
    std::vector<int> cwSeq;
    std::vector<int> esSeq;
};

struct ChecklistItem {
    std::wstring id;
    std::wstring title;
    std::wstring detail;
    std::wstring indicatorHint;
    bool checked = false;
};

struct ModelComparisonRow {
    std::wstring feature;
    std::wstring category;
    std::wstring cwBehavior;
    std::wstring esBehavior;
    std::wstring cgBehavior;
    std::wstring keyDifferenceNote;
    std::vector<int> cwDemo;
    std::vector<int> esDemo;
};

struct FavoriteItem {
    std::wstring kind; // "key", "proc", "diag", "tip"
    std::wstring id;
    std::wstring title;
    std::wstring subtitle;
    int calcIdx = 0;
    std::vector<int> demoSeq;
    std::wstring pdfFile;
    int pdfPage = 1;
};

struct KnowledgeCatalogData {
    std::vector<CatalogKeyRecord> keys;
    std::vector<LcdIndicatorDoc> indicators;
    std::vector<ProcedureEntry> procedures;
    std::vector<DiagnosticScenario> diagnostics;
    std::vector<CasioErrorDoc> errors;
    std::vector<TipWarningCard> tips;
    std::vector<SetupProfilePreset> setupProfiles;
    std::vector<ChecklistItem> checklist;
    std::vector<ModelComparisonRow> comparisons;
};

// ----------------------------------------------------------------------------
// Utilidades deterministas de texto y expansion de sinonimos
// ----------------------------------------------------------------------------
inline std::wstring NormalizeKnowledgeText(const std::wstring& s) {
    std::wstring out;
    out.reserve(s.size());
    for (wchar_t ch : s) {
        wchar_t c = static_cast<wchar_t>(towlower(ch));
        switch (c) {
        case L'\u00E1': case L'\u00E0': case L'\u00E4': c = L'a'; break;
        case L'\u00E9': case L'\u00E8': case L'\u00EB': c = L'e'; break;
        case L'\u00ED': case L'\u00EC': case L'\u00EF': c = L'i'; break;
        case L'\u00F3': case L'\u00F2': case L'\u00F6': c = L'o'; break;
        case L'\u00FA': case L'\u00F9': case L'\u00FC': c = L'u'; break;
        case L'\u00F1': c = L'n'; break;
        default: break;
        }
        out.push_back(c);
    }
    return out;
}

inline std::vector<std::wstring> ExpandQueryWithSynonyms(const std::wstring& rawQuery) {
    std::wstring norm = NormalizeKnowledgeText(rawQuery);
    std::wstringstream ss(norm);
    std::wstring tok;
    std::vector<std::wstring> tokens;
    while (ss >> tok) {
        if (!tok.empty()) tokens.push_back(tok);
    }
    struct SynPair { const wchar_t* trigger; const wchar_t* extra; };
    static const SynPair kSyns[] = {
        { L"decimales",  L"fix sd math decimal redondeo" },
        { L"decimal",    L"fix sd math decimal fraccion" },
        { L"redondear",  L"fix sci rnd decimal" },
        { L"fraccion",   L"sd ab mixta math" },
        { L"fracciones", L"sd ab mixta math" },
        { L"radianes",   L"rad unidad angular deg" },
        { L"grados",     L"deg unidad angular rad" },
        { L"angulos",    L"deg rad gra unidad angular" },
        { L"ecuacion",   L"solve cuadratica sistema algebra" },
        { L"ecuaciones", L"solve cuadratica sistema algebra" },
        { L"derivada",   L"d/dx calculo diferencial" },
        { L"integral",   L"calculo area definida" },
        { L"reiniciar",  L"reset inicializar borrar" },
        { L"resetear",   L"reset inicializar borrar" },
        { L"borrar",     L"ac del clr reset" },
        { L"complejos",  L"cmplx polar rectangular i angulo" },
        { L"matriz",     L"mata matrices determinante" },
        { L"matrices",   L"mata matriz determinante" },
        { L"cientifica", L"sci exp x10 norm" },
        { L"memoria",    L"sto rcl ans m variable" },
        { L"grafica",    L"cg50 graph g-solv v-window" },
        { L"graficar",   L"cg50 graph tabla" },
    };
    std::vector<std::wstring> expanded = tokens;
    for (const auto& t : tokens) {
        for (const auto& sp : kSyns) {
            if (t == sp.trigger) {
                std::wstringstream es(sp.extra);
                std::wstring e;
                while (es >> e) expanded.push_back(e);
            }
        }
    }
    return expanded;
}

inline int ScoreKnowledgeMatch(const std::wstring& corpus, const std::wstring& rawQuery) {
    if (rawQuery.empty()) return 1;
    std::wstring c = NormalizeKnowledgeText(corpus);
    std::wstring q = NormalizeKnowledgeText(rawQuery);
    if (c.find(q) != std::wstring::npos) return 100;

    std::wstringstream ss(q);
    std::wstring tok;
    std::vector<std::wstring> strictTokens;
    while (ss >> tok) strictTokens.push_back(tok);

    int hits = 0;
    for (const auto& t : strictTokens) {
        if (c.find(t) != std::wstring::npos) hits += 20;
    }
    if (hits == static_cast<int>(strictTokens.size()) * 20 && hits > 0) return hits;

    auto syns = ExpandQueryWithSynonyms(rawQuery);
    int synHits = 0;
    for (const auto& s : syns) {
        if (c.find(s) != std::wstring::npos) synHits += 8;
    }
    return hits + synHits;
}

inline std::wstring ModelDisplayLabel(int calcIdx) {
    if (calcIdx == 0) return L"ClassWiz fx-570/991LA X";
    if (calcIdx == 1) return L"fx-570ES / VN PLUS";
    if (calcIdx == 2) return L"fx-CG50 PRIZM";
    return L"Desconocida";
}

// ----------------------------------------------------------------------------
// Builders internos para teclas e indicadores
// ----------------------------------------------------------------------------
namespace detail {

inline void AddKeyRec(
    std::vector<CatalogKeyRecord>& out,
    int calcIdx, int keyId, const wchar_t* keyName,
    const wchar_t* pdfFile, int pdfPage,
    KeyLayerInfo nrm, KeyLayerInfo shf, KeyLayerInfo alp,
    std::vector<int> demo = {}
) {
    CatalogKeyRecord r;
    r.calcIdx = calcIdx;
    r.keyId = keyId;
    r.keyName = keyName;
    r.pdfFile = pdfFile;
    r.pdfPage = pdfPage;
    r.normal = std::move(nrm);
    r.shift = std::move(shf);
    r.alpha = std::move(alp);
    r.demoSeq = std::move(demo);
    out.push_back(std::move(r));
}

inline KeyLayerInfo Lyr(const wchar_t* lbl, const wchar_t* cat, const wchar_t* sum,
                        const wchar_t* latF, const wchar_t* latE, const wchar_t* tip) {
    KeyLayerInfo l;
    l.available = (lbl && lbl[0] != L'\0');
    if (l.available) {
        l.label = lbl;
        l.category = cat;
        l.summary = sum;
        l.latexFormula = latF;
        l.latexExample = latE;
        l.tipOrWarning = tip;
    }
    return l;
}

inline KeyLayerInfo NoneLyr() {
    return KeyLayerInfo{};
}

inline void PopulateClassWizKeys(std::vector<CatalogKeyRecord>& out) {
    const wchar_t* pdf = L"fx-570_991LA_X_ES.pdf";
    auto K = [&](int id, const wchar_t* name, int pg, KeyLayerInfo n, KeyLayerInfo s, KeyLayerInfo a, std::vector<int> demo = {}) {
        AddKeyRec(out, 0, id, name, pdf, pg, std::move(n), std::move(s), std::move(a), std::move(demo));
    };

    K(0, L"SHIFT", 5,
      Lyr(L"SHIFT", L"Modificador", L"Activa las funciones secundarias en color dorado ubicadas arriba de cada tecla.", L"$\\mathrm{SHIFT} + \\mathrm{tecla}$", L"$\\mathrm{SHIFT} + \\sin \\to \\sin^{-1}$", L"Aparece el indicador S arriba; se desactiva tras la siguiente tecla."),
      NoneLyr(), NoneLyr(), {0});
    K(1, L"ALPHA", 5,
      Lyr(L"ALPHA", L"Modificador", L"Activa letras de variables (A-F, x, y, M) y comandos en rojo.", L"$\\mathrm{ALPHA} + \\mathrm{tecla}$", L"$\\mathrm{ALPHA} + (-) \\to A$", L"Aparece el indicador A arriba; util para escribir ecuaciones y fórmulas."),
      NoneLyr(), NoneLyr(), {1});
    K(2, L"ARRIBA", 14,
      Lyr(L"Flecha Arriba", L"Navegaci\u00F3n", L"Sube al numerador, historial previo o elemento superior del men\u00FA.", L"$\\uparrow$", L"Revisar c\u00E1lculos anteriores", L"Cuando aparece flecha arriba en el LCD hay historial disponible."),
      NoneLyr(), NoneLyr());
    K(3, L"IZQUIERDA", 11,
      Lyr(L"Flecha Izquierda", L"Navegaci\u00F3n", L"Mueve el cursor a la izquierda o edita desde el final de la expresi\u00F3n tras presionar =.", L"$\\leftarrow$", L"Editar expresi\u00F3n anterior", L"En un mensaje ERROR, mueve el cursor al lugar exacto del fallo."),
      NoneLyr(), NoneLyr());
    K(4, L"DERECHA", 11,
      Lyr(L"Flecha Derecha", L"Navegaci\u00F3n", L"Sale del bloque actual (fracci\u00F3n, ra\u00EDz, exponente, l\u00EDmite de integral) o avanza a la derecha.", L"$\\rightarrow$", L"Salir de $\\sqrt{x}$ antes de sumar", L"Error com\u00FAn: seguir escribiendo dentro de la ra\u00EDz por no presionar flecha derecha."),
      NoneLyr(), NoneLyr());
    K(5, L"ABAJO", 14,
      Lyr(L"Flecha Abajo", L"Navegaci\u00F3n", L"Baja al denominador, l\u00EDmite inferior o historial reciente.", L"$\\downarrow$", L"Pasar de numerador a denominador", L"Cuando aparece flecha abajo en LCD hay m\u00E1s opciones debajo."),
      NoneLyr(), NoneLyr());
    K(6, L"MENU", 6,
      Lyr(L"MENU", L"Configuraci\u00F3n", L"Abre la cuadr\u00EDcula de 12 aplicaciones (Calcular, Complejos, Matrices, Ecuaci\u00F3n, Tabla, etc.).", L"$\\mathrm{MENU} \\to 1..C$", L"$\\mathrm{MENU} \\to 1$ vuelve a Calcular", L"Verifica el indicador de modo antes de calcular."),
      Lyr(L"SET UP", L"Configuraci\u00F3n", L"Abre ajustes de Entrada/Salida (Math/L\u00EDnea), Unidad angular (Deg/Rad/Gra) y Formato (Fix/Sci/Norm).", L"$\\mathrm{SHIFT} + \\mathrm{MENU}$", L"$\\mathrm{SHIFT} \\to \\mathrm{MENU} \\to 2 \\to 1$ (Grados)", L"Si trigonometr\u00EDa da valores extra\u00F1os, revisa Unidad angular aqu\u00ED."),
      NoneLyr(), {6});
    K(7, L"ON", 4,
      Lyr(L"ON", L"Sistema", L"Enciende la calculadora o reinicia la pantalla de entrada actual.", L"$\\mathrm{ON}$", L"Recupera foco si se apag\u00F3", L"No borra variables guardadas con STO; si borra historial reciente."),
      NoneLyr(), NoneLyr());
    K(8, L"OPTN", 19,
      Lyr(L"OPTN", L"Men\u00FA contextual", L"Muestra funciones especiales del modo actual (Conjugado en Complejos, Matrices, Hiperb\u00F3licas, Unidad angular).", L"$\\mathrm{OPTN}$", L"En Mat: $MatA, Det, Trn$", L"Cambia din\u00E1micamente seg\u00FAn el modo elegido en MENU."),
      Lyr(L"QR", L"Utilidad", L"Genera un c\u00F3digo QR del resultado o tabla actual en la calculadora f\u00EDsica/emulador.", L"$\\mathrm{SHIFT} + \\mathrm{OPTN}$", L"QR de tabla $f(x)$", L"Se muestra en pantalla en modelos compatibles."),
      NoneLyr(), {8});
    K(9, L"CALC", 21,
      Lyr(L"CALC", L"\u00C1lgebra", L"Eval\u00FAa la expresi\u00F3n actual pidiendo valores para variables (X, A, B...).", L"$f(x) \\to \\mathrm{CALC} \\to x=?$", L"Evaluar $x^{2}+3$ para $x=4 \\to 19$", L"\u00DAtil para comprobar m\u00FAltiples valores sin reescribir la f\u00F3rmula."),
      Lyr(L"SOLVE", L"\u00C1lgebra", L"Resuelve num\u00E9ricamente una ecuaci\u00F3n usando m\u00E9todo de Newton a partir de un valor inicial.", L"$f(x) = g(x) \\to x \\approx ?$", L"$2x + 3 = 11 \\to x = 4$", L"SOLVE usa el signo = rojo (ALPHA + CALC), no el bot\u00F3n igual inferior."),
      Lyr(L"=", L"\u00C1lgebra", L"Inserta el signo de igualdad simb\u00F3lico para ecuaciones de SOLVE u Hoja de c\u00E1lculo.", L"$\\mathrm{ALPHA} + \\mathrm{CALC} \\to =$", L"$x^{2} - 9 = 0$", L"Diferente al bot\u00F3n = evaluador inferior."),
      {41, 11, 43, 42, 9});
    K(10, L"INTEGRAL", 17,
      Lyr(L"Integral Definida", L"C\u00E1lculo", L"Calcula la integral definida num\u00E9rica de $f(x)$ entre $a$ y $b$.", L"$\\int_{a}^{b} f(x)\\,dx$", L"$\\int_{0}^{3} (x^{2}+2x)\\,dx = 18$", L"La variable de integraci\u00F3n debe ser x. Funciones discontinuas pueden tardar."),
      Lyr(L"Derivada d/dx", L"C\u00E1lculo", L"Calcula la derivada num\u00E9rica en un punto $x=a$.", L"$\\left.\\frac{d}{dx}f(x)\\right|_{x=a}$", L"$\\left.\\frac{d}{dx}(x^{3})\\right|_{x=2} = 12$", L"Requiere modo Calcular y especificar un punto num\u00E9rico en el cuadro derecho."),
      Lyr(L":", L"Multisentencia", L"Separa varias expresiones en la misma l\u00EDnea para ejecutarlas en secuencia.", L"$A=3 : A^{2}+1$", L"$2+3 : \\mathrm{Ans}\\times 4$", L"Aparece indicador de pausa entre sentencias."),
      {10, 11, 14, 4, 45, 4, 42, 49});
    K(11, L"VARIABLE", 15,
      Lyr(L"Tecla x", L"\u00C1lgebra", L"Ingresa directamente la variable $x$ con una sola pulsaci\u00F3n sin usar ALPHA.", L"$x$", L"$x^{2} + 2x$", L"Ideal para integrales, derivadas, Tabla f(x) y SOLVE."),
      Lyr(L"Sumatoria \\sum", L"C\u00E1lculo", L"Calcula una sumatoria discreta desde $k=a$ hasta $b$.", L"$\\sum_{x=a}^{b} f(x)$", L"$\\sum_{x=1}^{5} x = 15$", L"El \u00EDndice de sumatoria siempre usa x y l\u00EDmites enteros."),
      NoneLyr(), {11, 14, 43, 40});
    K(12, L"FRACCION", 12,
      Lyr(L"Fracci\u00F3n", L"B\u00E1sicas", L"Inserta plantilla natural numerador / denominador.", L"$\\frac{a}{b}$", L"$\\frac{3}{4} + \\frac{1}{2} = \\frac{5}{4}$", L"Si un valor supera 10 d\u00EDgitos en totales, ClassWiz convierte a decimal."),
      Lyr(L"Fracci\u00F3n Mixta", L"B\u00E1sicas", L"Inserta plantilla de n\u00FAmero mixto entero + fracci\u00F3n propia.", L"$a\\frac{b}{c}$", L"$1\\frac{1}{2} = \\frac{3}{2}$", L"Usa S<=>D con SHIFT para alternar entre mixta e impropia."),
      NoneLyr(), {12, 42, 5, 35, 49});
    K(13, L"RAIZ", 16,
      Lyr(L"Ra\u00EDz Cuadrada", L"\u00C1lgebra", L"Calcula la ra\u00EDz cuadrada principal positiva (o imaginaria en Complejos).", L"$\\sqrt{x}$", L"$\\sqrt{25} = 5$", L"En Calcular, ra\u00EDz par de n\u00FAmero negativo genera Math ERROR."),
      Lyr(L"Ra\u00EDz C\u00FAbica", L"\u00C1lgebra", L"Calcula la ra\u00EDz c\u00FAbica real de n\u00FAmeros positivos o negativos.", L"$\\sqrt[3]{x}$", L"$\\sqrt[3]{-27} = -3$", L"No da error para radicandos negativos reales."),
      NoneLyr(), {13, 41, 36, 49});
    K(14, L"CUADRADO", 16,
      Lyr(L"Cuadrado", L"\u00C1lgebra", L"Eleva el valor o par\u00E9ntesis anterior a la potencia 2.", L"$x^{2}$", L"$(-3)^{2} = 9$", L"Cuidado: $-3^{2} = -9$ por prioridad de operadores; usa $(-3)^{2}$."),
      Lyr(L"Cubo", L"\u00C1lgebra", L"Eleva la expresi\u00F3n al exponente 3.", L"$x^{3}$", L"$2^{3} = 8$", L"Evita abrir exponente manual cuando solo necesitas grado 3."),
      Lyr(L"DEC (Base-N)", L"Sistemas Num\u00E9ricos", L"Cambia o etiqueta ingreso decimal en modo Base-N.", L"$x_{10}$", L"Conversi\u00F3n Base-N a decimal", L"Activo dentro de MENU -> 3 (Base-N)."),
      {41, 14, 49});
    K(15, L"POTENCIA", 16,
      Lyr(L"Potencia x^y", L"\u00C1lgebra", L"Eleva la base a cualquier exponente entero, fraccionario o negativo.", L"$x^{n}$", L"$2^{5} = 32$", L"Presiona flecha derecha para bajar del exponente."),
      Lyr(L"Ra\u00EDz n-\u00E9sima", L"\u00C1lgebra", L"Calcula la ra\u00EDz de \u00EDndice $n$ arbitrario.", L"$\\sqrt[n]{x}$", L"$\\sqrt[4]{16} = 2$", L"Ingresa primero el \u00EDndice o navega entre los dos cuadros."),
      Lyr(L"HEX (Base-N)", L"Sistemas Num\u00E9ricos", L"Cambia a hexadecimal en modo Base-N.", L"$x_{16}$", L"$\\mathrm{FF}_{16} = 255_{10}$", L"Las teclas A..F ingresan d\u00EDgitos hexadecimales autom\u00E1ticamente en Base-N."),
      {41, 15, 36, 49});
    K(16, L"LOG_B", 16,
      Lyr(L"Logaritmo Base a", L"\u00C1lgebra", L"Calcula $\\log_{a}(b)$ eligiendo base y argumento.", L"$\\log_{a}(b)$", L"$\\log_{2}(8) = 3$", L"Para logaritmo decimal r\u00E1pido tambi\u00E9n existe SHIFT + (-)."),
      Lyr(L"Base 10^x", L"\u00C1lgebra", L"Funci\u00F3n antilogaritmo decimal $10^{x}$.", L"$10^{x}$", L"$10^{3} = 1000$", L"Diferente de la tecla x10^x inferior que escribe notaci\u00F3n cient\u00EDfica."),
      Lyr(L"BIN (Base-N)", L"Sistemas Num\u00E9ricos", L"Cambia a binario en modo Base-N.", L"$x_{2}$", L"$1010_{2} = 10_{10}$", L"Solo acepta d\u00EDgitos 0 y 1 en ingreso binario."),
      {16, 41, 4, 31, 49});
    K(17, L"LN", 16,
      Lyr(L"Logaritmo Natural", L"\u00C1lgebra", L"Calcula logaritmo natural en base $e \\approx 2.7182818$.", L"$\\ln(x)$", L"$\\ln(e) = 1$", L"Requiere $x > 0$ en n\u00FAmeros reales."),
      Lyr(L"Exponencial e^x", L"\u00C1lgebra", L"Eleva el n\u00FAmero de Euler $e$ a un exponente.", L"$e^{x}$", L"$e^{0} = 1$", L"Usa flecha derecha para salir del exponente antes de sumar."),
      Lyr(L"OCT (Base-N)", L"Sistemas Num\u00E9ricos", L"Cambia a octal en modo Base-N.", L"$x_{8}$", L"$77_{8} = 63_{10}$", L"Disponible en MENU -> 3 (Base-N)."),
      {17, 40, 49});
    K(18, L"NEGATIVO", 10,
      Lyr(L"Signo Negativo (-)", L"B\u00E1sicas", L"Cambia el signo de un n\u00FAmero o expresi\u00F3n (operador unario, distinto a la resta).", L"$-x$", L"$(-5) + 8 = 3$", L"No confundir (-) con el bot\u00F3n resta [-] al iniciar exponentes negativos."),
      Lyr(L"log Base 10", L"\u00C1lgebra", L"Ingresa $\\log($ directo en base 10 sin pedir caja de base.", L"$\\log_{10}(x)$", L"$\\log(100) = 2$", L"Atajo r\u00E1pido respecto a log_a(b)."),
      Lyr(L"Variable A", L"Variables A-F,X,Y,M", L"Ingresa la letra A (o d\u00EDgito hex A en Base-N / opci\u00F3n Ecuaci\u00F3n en MENU).", L"$A$", L"$\\mathrm{STO} \\to A$", L"En MENU, permite entrar directo a Ecuaci\u00F3n/Funci\u00F3n."),
      {18, 36, 43, 31, 49});
    K(19, L"GRADOS", 16,
      Lyr(L"Grados Min Seg", L"Trigonometr\u00EDa", L"Ingresa o convierte \u00E1ngulos y horas en formato sexagesimal ($\\circ\\,'\\,''$).", L"$d^{\\circ} m' s''$", L"$30^{\\circ} 15' 0'' = 30.25^{\\circ}$", L"Presionar de nuevo tras un resultado alterna decimal y sexagesimal."),
      Lyr(L"FACT (Descomposici\u00F3n)", L"Aritm\u00E9tica", L"Factoriza un entero resultado en producto de n\u00FAmeros primos.", L"$n = p_{1}^{a} \\cdot p_{2}^{b}$", L"$360 = 2^{3}\\times 3^{2}\\times 5$", L"Se aplica sobre el \u00FAltimo entero positivo mostrado en pantalla."),
      Lyr(L"Variable B", L"Variables A-F,X,Y,M", L"Inserta variable B o d\u00EDgito hexadecimal B.", L"$B$", L"$A + B$", L"En MENU selecciona Desigualdad (B)."),
      {42, 45, 19, 35, 36, 19, 49});
    K(20, L"INVERSO", 16,
      Lyr(L"Inverso Multiplicativo", L"\u00C1lgebra", L"Calcula el rec\u00EDproco multiplicativo $x^{-1} = \\frac{1}{x}$ para $x \\neq 0$; tambi\u00E9n invierte matrices cuadradas.", L"$x^{-1} = \\frac{1}{x}$", L"$4^{-1} = \\frac{1}{4} = 0.25$", L"Inverso multiplicativo NO es cambio de signo (-). No dividir entre 0."),
      Lyr(L"Factorial x!", L"Combinatoria", L"Calcula $n! = n(n-1)\\cdots 1$ para enteros $0 \\le n \\le 69$.", L"$n!$", L"$5! = 120$", L"Por definici\u00F3n $0! = 1$. Valores $\\ge 70$ provocan Math ERROR."),
      Lyr(L"Variable C", L"Variables A-F,X,Y,M", L"Inserta variable C o d\u00EDgito hexadecimal C.", L"$C$", L"$\\mathrm{STO} \\to C$", L"En MENU selecciona Raz\u00F3n / Proporci\u00F3n (C)."),
      {35, 20, 49});
    K(21, L"SIN", 16,
      Lyr(L"Seno $\\sin$", L"Trigonometr\u00EDa", L"Calcula el seno del \u00E1ngulo seg\u00FAn la unidad activa (D, R o G).", L"$\\sin(\\theta)$", L"$\\sin(30^{\\circ}) = \\frac{1}{2}$", L"Revisa si arriba dice D (Grados) o R (Radianes) antes de interpretar el resultado."),
      Lyr(L"Arcoseno $\\sin^{-1}$", L"Trigonometr\u00EDa", L"Calcula el \u00E1ngulo cuyo seno es $x \\in [-1, 1]$.", L"$\\sin^{-1}(x)$", L"$\\sin^{-1}(0.5) = 30^{\\circ}$", L"Si $|x| > 1$ produce Math ERROR."),
      Lyr(L"Variable D", L"Variables A-F,X,Y,M", L"Inserta variable $D$ o d\u00EDgito hex $D$.", L"$D$", L"$\\mathrm{STO} \\to D$", L"Se consulta luego con SHIFT + STO (RECALL)."),
      {21, 42, 45, 27, 49});
    K(22, L"COS", 16,
      Lyr(L"Coseno $\\cos$", L"Trigonometr\u00EDa", L"Calcula el coseno en Grados, Radianes o Gradianes.", L"$\\cos(\\theta)$", L"$\\cos(60^{\\circ}) = 0.5$", L"Comprueba el indicador angular D/R/G."),
      Lyr(L"Arcocoseno $\\cos^{-1}$", L"Trigonometr\u00EDa", L"Calcula $\\arccos(x) \\in [0, 180^{\\circ}]$ o $[0, \\pi]$.", L"$\\cos^{-1}(x)$", L"$\\cos^{-1}(0) = 90^{\\circ}$", L"Solo definido en reales para $-1 \\le x \\le 1$."),
      Lyr(L"Variable E", L"Variables A-F,X,Y,M", L"Inserta variable $E$ o d\u00EDgito hex $E$.", L"$E$", L"$\\mathrm{STO} \\to E$", L"Conserva su valor al apagar salvo RESET."),
      {22, 37, 45, 27, 49});
    K(23, L"TAN", 16,
      Lyr(L"Tangente $\\tan$", L"Trigonometr\u00EDa", L"Calcula $\\tan(\\theta) = \\frac{\\sin\\theta}{\\cos\\theta}$.", L"$\\tan(\\theta)$", L"$\\tan(45^{\\circ}) = 1$", L"$\\tan(90^{\\circ})$ no est\u00E1 definida y produce Math ERROR."),
      Lyr(L"Arcotangente $\\tan^{-1}$", L"Trigonometr\u00EDa", L"Calcula $\\arctan(x) \\in (-90^{\\circ}, 90^{\\circ})$.", L"$\\tan^{-1}(x)$", L"$\\tan^{-1}(1) = 45^{\\circ}$", L"\u00DAtil para hallar \u00E1ngulos en tri\u00E1ngulos y vectores."),
      Lyr(L"Variable F", L"Variables A-F,X,Y,M", L"Inserta variable $F$ o d\u00EDgito hex $F$.", L"$F$", L"$\\mathrm{STO} \\to F$", L"Disponible en Base-N para hex F."),
      {23, 35, 36, 27, 49});
    K(24, L"STO", 15,
      Lyr(L"STO (Guardar)", L"Memorias", L"Asigna el resultado mostrado o ingresado a una variable (A-F, M, x, y). No requiere SHIFT en ClassWiz.", L"$\\mathrm{valor} \\to \\mathrm{Variable}$", L"$5 \\to \\mathrm{STO} \\to (-) \\text{ (A=5)}$", L"En ClassWiz se pulsa STO directo (sin SHIFT) y luego la tecla de variable."),
      Lyr(L"RECALL", L"Memorias", L"Muestra en una sola tabla todas las variables almacenadas (A..F, M, x, y).", L"$\\mathrm{SHIFT} + \\mathrm{STO}$", L"Ver valores actuales de A..F, M", L"Presiona AC para cerrar la vista RECALL sin recuperar."),
      NoneLyr());
    K(25, L"ENG", 13,
      Lyr(L"Notaci\u00F3n Ingenier\u00EDa ->", L"Formato", L"Desplaza el punto decimal a la derecha ajustando exponente a m\u00FAltiplos de 3 ($10^{3}, 10^{6}, 10^{-3}$).", L"$m \\times 10^{3k}$", L"$1234 \\to \\mathrm{ENG} \\to 1.234 \\times 10^{3}$", L"Ideal para unidades SI (mili, micro, kilo, mega)."),
      Lyr(L"ENG <- / Angulo polar", L"Complejos/Formato", L"En Calcular desplaza exponente de ingenier\u00EDa hacia la izquierda; en Complejos inserta el s\u00EDmbolo de \u00E1ngulo polar.", L"$r\\angle\\theta$", L"$2\\angle 45^{\\circ}$", L"Solo ingresa \u00E1ngulo polar cuando est\u00E1s en modo Complejos (MENU 2)."),
      Lyr(L"Unidad imaginaria i", L"Complejos", L"Inserta la unidad imaginaria $i = \\sqrt{-1}$ en modo Complejos.", L"$a + bi$", L"$(3+4i)(1-2i) = 11-2i$", L"Solo v\u00E1lida dentro de MENU -> 2 (Complejos)."),
      {40, 41, 42, 35, 49, 25});
    K(26, L"PAREN_ABRE", 18,
      Lyr(L"Par\u00E9ntesis (", L"Edici\u00F3n", L"Agrupa subexpresiones para alterar prioridad de operaciones.", L"$(a+b)\\times c$", L"$(2+3)\\times 4 = 20$", L"La calculadora cierra autom\u00E1ticamente par\u00E9ntesis finales abiertos, pero es m\u00E1s seguro cerrarlos manualmente."),
      Lyr(L"Valor Absoluto Abs", L"\u00C1lgebra", L"Inserta barras de valor absoluto $|x|$ (o m\u00F3dulo $|z|$ en complejos).", L"$|x|$", L"$|-7| = 7$", L"Para salir de las barras usa flecha derecha."),
      NoneLyr(), {0, 26, 18, 30, 49});
    K(27, L"PAREN_CIERRA", 15,
      Lyr(L"Par\u00E9ntesis )", L"Edici\u00F3n", L"Cierra el par\u00E9ntesis abierto m\u00E1s reciente.", L"$(...)$", L"$\\sin(30) = 0.5$", L"Siempre cierra funciones trigonom\u00E9tricas antes de sumar otro t\u00E9rmino."),
      Lyr(L"Punto y coma ;", L"Sintaxis", L"Separa argumentos en comandos de entrada lineal (ej: $\\log(base; val)$, Pol, Rec).", L"$\\mathrm{func}(a; b)$", L"$\\mathrm{Pol}(3; 4)$", L"En entrada lineal usa ; y no coma decimal."),
      Lyr(L"Variable X", L"Variables A-F,X,Y,M", L"Inserta X (en Calcular tambi\u00E9n existe la tecla directa x arriba a la derecha).", L"$X$", L"$X^{2} + 1$", L"Funci\u00F3n id\u00E9ntica a la variable x en memorias."),
      {26, 41, 43, 42, 27, 38, 35, 49});
    K(28, L"SD", 12,
      Lyr(L"S<=>D", L"Formato", L"Alterna el \u00FAltimo resultado entre expresi\u00F3n simb\u00F3lica exacta (fracci\u00F3n, radical, $\\pi$) y aproximaci\u00F3n decimal.", L"$\\frac{3}{4} \\Leftrightarrow 0.75$", L"$\\sqrt{2} \\Leftrightarrow 1.414213562$", L"Si el formato de salida es Mat/Decimal, S<=>D puede no revertir a fracci\u00F3n en expresiones complejas."),
      Lyr(L"a b/c <=> d/c", L"Formato", L"Convierte el resultado entre fracci\u00F3n mixta $a\\frac{b}{c}$ y fracci\u00F3n impropia $\\frac{d}{c}$.", L"$1\\frac{1}{4} \\Leftrightarrow \\frac{5}{4}$", L"$\\frac{7}{3} \\Leftrightarrow 2\\frac{1}{3}$", L"No requiere reevaluar el c\u00E1lculo."),
      Lyr(L"Variable Y", L"Variables A-F,X,Y,M", L"Inserta la variable $Y$ (usada tambi\u00E9n junto a Tabla y memorias).", L"$Y$", L"$2X + Y$", L"Se guarda con STO -> S<=>D."),
      {12, 42, 5, 35, 49, 28});
    K(29, L"MAS_M", 15,
      Lyr(L"M+ (Sumar a M)", L"Memorias", L"Eval\u00FAa la pantalla actual y la suma acumulada a la memoria independiente $M$.", L"$M \\leftarrow M + \\mathrm{res}$", L"$10\\,\\mathrm{M+} \\; ; \\; 5\\,\\mathrm{M+} \\Rightarrow M=15$", L"Cuando M != 0 aparece el indicador M en la fila superior LCD."),
      Lyr(L"M- (Restar de M)", L"Memorias", L"Resta el resultado actual de la memoria independiente $M$.", L"$M \\leftarrow M - \\mathrm{res}$", L"$M - 3$", L"Para limpiar M a cero: 0 -> STO -> M+."),
      Lyr(L"Variable M", L"Variables A-F,X,Y,M", L"Inserta el valor guardado en la memoria $M$ dentro de una expresi\u00F3n.", L"$M$", L"$2 \\times M$", L"Usa SHIFT + STO (RECALL) para ver M en cualquier momento."),
      {40, 45, 29});
    K(30, L"7", 40,
      Lyr(L"D\u00EDgito 7", L"Num\u00E9rico", L"Ingresa el d\u00EDgito 7 o elige la opci\u00F3n 7 en un men\u00FA.", L"$7$", L"$7 \\times 8 = 56$", L"En MENU -> 7 seleccionas Distribuci\u00F3n."),
      Lyr(L"CONST (47 constantes)", L"F\u00EDsica/Qu\u00EDmica", L"Abre el cat\u00E1logo de 47 constantes cient\u00EDficas CODATA ($c_{0}, h, g, N_{A}, R...$).", L"$c_{0} = 299792458\\,\\mathrm{m/s}$", L"SHIFT + 7 -> 1 (Universal) -> 3 (c0)", L"Las unidades vienen en Sistema Internacional."),
      NoneLyr(), {30, 38, 31, 49});
    K(31, L"8", 26,
      Lyr(L"D\u00EDgito 8", L"Num\u00E9rico", L"Ingresa el d\u00EDgito 8 o elige la opci\u00F3n 8 en un men\u00FA.", L"$8$", L"$8^{2} = 64$", L"En MENU -> 8 seleccionas Hoja de c\u00E1lculo."),
      Lyr(L"CONV (40 conversiones)", L"Unidades", L"Convierte un valor multiplic\u00E1ndolo por una relaci\u00F3n oficial (ej: pulgada a cm, atm a Pa, km/h a m/s).", L"$1\\,\\mathrm{in} = 2.54\\,\\mathrm{cm}$", L"10 SHIFT + 8 -> in>cm", L"Primero escribe el n\u00FAmero y luego inserta el comando de conversi\u00F3n."),
      NoneLyr());
    K(32, L"9", 9,
      Lyr(L"D\u00EDgito 9", L"Num\u00E9rico", L"Ingresa el d\u00EDgito 9 o selecciona Tabla (MENU 9).", L"$9$", L"$\\sqrt{9} = 3$", L"MENU -> 9 abre r\u00E1pidamente generaci\u00F3n de tablas f(x)."),
      Lyr(L"RESET", L"Sistema", L"Abre el men\u00FA para restablecer Configuraci\u00F3n, Memorias o Inicializar todo.", L"$\\mathrm{SHIFT} + 9 \\to 1/2/3$", L"Conf. datos devuelve ajustes por defecto", L"Inicializar todo borra memorias A-F, matrices, estad\u00EDstica e historial."),
      NoneLyr());
    K(33, L"DEL", 11,
      Lyr(L"DEL (Borrar car\u00E1cter)", L"Edici\u00F3n", L"Elimina el car\u00E1cter o plantilla a la izquierda del cursor.", L"$\\mathrm{DEL}$", L"Corregir d\u00EDgito sin perder la f\u00F3rmula", L"Si te equivocaste en un n\u00FAmero, usa DEL en lugar de AC."),
      Lyr(L"INS (Modo Insertar)", L"Edici\u00F3n", L"En entrada lineal alterna entre inserci\u00F3n y sobrescritura.", L"$\\mathrm{SHIFT} + \\mathrm{DEL}$", L"Modo cursor bloque/raya", L"En entrada Math siempre est\u00E1 activo el modo insertar."),
      Lyr(L"UNDO (Deshacer tecla)", L"Edici\u00F3n", L"Revierte una edici\u00F3n o borrado accidental en la l\u00EDnea actual.", L"$\\mathrm{ALPHA} + \\mathrm{DEL}$", L"Recupera t\u00E9rmino borrado por error", L"\u00DAtil si borraste una subexpresi\u00F3n completa con DEL o AC."),
      {30, 31, 33});
    K(34, L"AC", 4,
      Lyr(L"AC (All Clear)", L"Edici\u00F3n", L"Limpia la l\u00EDnea de entrada actual y cancela mensajes de error. Mantiene historial y variables.", L"$\\mathrm{AC}$", L"Volver a entrada limpia", L"Para volver al c\u00E1lculo previo tras AC, pulsa flecha Arriba."),
      Lyr(L"OFF (Apagar)", L"Sistema", L"Apaga la calculadora ahorrando energ\u00EDa (el emulador conserva el modo activo o reanuda con ON).", L"$\\mathrm{SHIFT} + \\mathrm{AC}$", L"Apagado seguro", L"Las variables guardadas con STO sobreviven al apagado."),
      NoneLyr(), {34});
    K(35, L"4", 28,
      Lyr(L"D\u00EDgito 4", L"Num\u00E9rico", L"Ingresa 4 o selecciona Matriz (MENU 4).", L"$4$", L"$4 \\times 4 = 16$", L"En MENU -> 4 entras al editor de MatA..MatD."),
      NoneLyr(), NoneLyr());
    K(36, L"5", 31,
      Lyr(L"D\u00EDgito 5", L"Num\u00E9rico", L"Ingresa 5 o selecciona Vector (MENU 5).", L"$5$", L"$5^{2} = 25$", L"En MENU -> 5 entras al editor VctA..VctD."),
      NoneLyr(), NoneLyr());
    K(37, L"6", 22,
      Lyr(L"D\u00EDgito 6", L"Num\u00E9rico", L"Ingresa 6 o selecciona Estad\u00EDstica (MENU 6).", L"$6$", L"$6! = 720$", L"En MENU -> 6 seleccionas 1-Variable o regresiones."),
      NoneLyr(), NoneLyr());
    K(38, L"POR", 10,
      Lyr(L"Multiplicaci\u00F3n x", L"B\u00E1sicas", L"Multiplica n\u00FAmeros, fracciones, complejos o matrices.", L"$a \\times b$", L"$6 \\times 7 = 42$", L"En multiplicaci\u00F3n impl\u00EDcita recuerda su prioridad frente a divisores."),
      Lyr(L"Permutaciones nPr", L"Combinatoria", L"Calcula variaciones sin repetici\u00F3n $P(n,r) = \\frac{n!}{(n-r)!}$.", L"$nPr = \\frac{n!}{(n-r)!}$", L"$5\\,\\mathrm{P}\\,2 = 20$", L"Ingresa primero $n$, luego SHIFT + x, luego $r$."),
      NoneLyr(), {37, 38, 30, 49});
    K(39, L"ENTRE", 10,
      Lyr(L"Divisi\u00F3n /", L"B\u00E1sicas", L"Realiza divisi\u00F3n est\u00E1ndar (en formato Math suele mostrar resultado fraccionario exacto).", L"$a \\div b$", L"$15 \\div 4 = \\frac{15}{4}$", L"Dividir entre 0 produce Math ERROR."),
      Lyr(L"Combinaciones nCr", L"Combinatoria", L"Calcula combinaciones $C(n,r) = \\frac{n!}{r!(n-r)!}$.", L"$C(n,r) = \\frac{n!}{r!(n-r)!}$", L"$5\\,\\mathrm{C}\\,2 = 10$", L"No admite valores negativos ni $r > n$."),
      NoneLyr(), {35, 35, 39, 35, 49});
    K(40, L"1", 6,
      Lyr(L"D\u00EDgito 1", L"Num\u00E9rico", L"Ingresa 1 o selecciona Calcular (MENU 1).", L"$1$", L"MENU -> 1 vuelve al modo est\u00E1ndar", L"Atajo universal para volver a c\u00E1lculos generales."),
      NoneLyr(), NoneLyr());
    K(41, L"2", 20,
      Lyr(L"D\u00EDgito 2", L"Num\u00E9rico", L"Ingresa 2 o selecciona Complejos (MENU 2).", L"$2$", L"MENU -> 2 activa i", L"En modo Complejos aparece indicador i."),
      NoneLyr(), NoneLyr());
    K(42, L"3", 26,
      Lyr(L"D\u00EDgito 3", L"Num\u00E9rico", L"Ingresa 3 o selecciona Base-N (MENU 3).", L"$3$", L"MENU -> 3 activa bin/oct/hex", L"En Base-N solo funcionan operaciones enteras."),
      NoneLyr(), NoneLyr());
    K(43, L"MAS", 10,
      Lyr(L"Suma +", L"B\u00E1sicas", L"Suma dos t\u00E9rminos num\u00E9ricos, vectoriales o matriciales.", L"$a + b$", L"$12 + 8 = 20$", L"Si el primer operando falta al pulsar +, usar\u00E1 Ans autom\u00E1ticamente."),
      Lyr(L"Pol(x, y)", L"Coordenadas", L"Convierte coordenadas rectangulares $(x,y)$ a polares $(r,\\theta)$.", L"$r=\\sqrt{x^{2}+y^{2}},\\; \\theta=\\mathrm{atan2}(y,x)$", L"$\\mathrm{Pol}(3; 4) \\to r=5$", L"Guarda autom\u00E1ticamente $r$ en $x$ y $\\theta$ en $y$."),
      NoneLyr(), {40, 41, 43, 30, 49});
    K(44, L"MENOS", 10,
      Lyr(L"Resta -", L"B\u00E1sicas", L"Resta el segundo operando del primero (operador binario).", L"$a - b$", L"$15 - 6 = 9$", L"Para signos negativos iniciales o dentro de potencias usa (-)."),
      Lyr(L"Rec(r, theta)", L"Coordenadas", L"Convierte coordenadas polares $(r,\\theta)$ a rectangulares $(x,y)$.", L"$x=r\\cos\\theta,\\; y=r\\sin\\theta$", L"$\\mathrm{Rec}(5; 53.13^{\\circ}) \\to x\\approx 3, y\\approx 4$", L"El \u00E1ngulo $\\theta$ depende del indicador D/R/G activo."),
      NoneLyr(), {41, 45, 44, 31, 49});
    K(45, L"0", 18,
      Lyr(L"D\u00EDgito 0", L"Num\u00E9rico", L"Ingresa 0.", L"$0$", L"$10 + 0 = 10$", L"Cuidado al dividir entre 0."),
      Lyr(L"Rnd (Redondeo interno)", L"\u00C1lgebra/Precisi\u00F3n", L"Redondea internamente el argumento a los d\u00EDgitos mostrados seg\u00FAn FIX/SCI.", L"$\\mathrm{Rnd}(x)$", L"$\\mathrm{Rnd}(\\mathrm{Ans})$", L"FIX solo maquilla pantalla; usa Rnd cuando necesites truncar la precisi\u00F3n real en c\u00E1lculos siguientes."),
      NoneLyr());
    K(46, L"COMA", 18,
      Lyr(L"Punto decimal .", L"Num\u00E9rico", L"Inserta el separador decimal en n\u00FAmeros reales.", L"$3.1416$", L"$0.5 + 0.25 = 0.75$", L"No se usa para separar argumentos en Math (usa SHIFT + ) para ';')."),
      Lyr(L"Ran# (Aleatorio 0..1)", L"Probabilidad", L"Genera un n\u00FAmero pseudoaleatorio uniforme en $[0.000, 0.999]$.", L"$\\mathrm{Ran\\#} \\in [0, 1)$", L"$1000\\,\\mathrm{Ran\\#}$", L"Cambia en cada ejecuci\u00F3n con =."),
      Lyr(L"RanInt#(a, b)", L"Probabilidad", L"Genera un entero aleatorio entre $a$ y $b$ inclusive.", L"$\\mathrm{RanInt\\#}(a; b)$", L"$\\mathrm{RanInt\\#}(1; 6)$", L"Separa l\u00EDmites con punto y coma (;)."),
      {45, 46, 36, 49});
    K(47, L"EXP", 13,
      Lyr(L"Notaci\u00F3n x10^x", L"Cient\u00EDfica", L"Ingresa mantisa por potencia de 10 ($a \\times 10^{b}$) sin crear multiplicaci\u00F3n separada.", L"$a \\times 10^{b}$", L"$6.022 \\times 10^{23}$", L"Atado como un solo n\u00FAmero: m\u00E1s seguro que escribir x 10 ^ x en denominadores."),
      Lyr(L"Constante Pi", L"Constantes", L"Inserta la constante matem\u00E1tica $\\pi \\approx 3.14159265359$.", L"$\\pi$", L"$\\sin(\\pi) = 0 \\text{ (en Rad)}$", L"La salida exacta suele conservar $\\pi$ simb\u00F3lico hasta pulsar S<=>D."),
      Lyr(L"Constante e", L"Constantes", L"Inserta la base de logaritmos naturales $e \\approx 2.71828182846$.", L"$e$", L"$\\ln(e) = 1$", L"No confundir con variable E."),
      {42, 47, 36, 49});
    K(48, L"ANS", 15,
      Lyr(L"Ans (\u00DAltimo resultado)", L"Memorias", L"Inserta el resultado num\u00E9rico o simb\u00F3lico exacto de la \u00FAltima evaluaci\u00F3n.", L"$\\mathrm{Ans}$", L"$\\mathrm{Ans} + 5$", L"Ideal para m\u00E9todos iterativos y cadenas paso a paso."),
      Lyr(L"Porcentaje %", L"Aritm\u00E9tica", L"Divide el valor precedente por 100 ($x\\% = \\frac{x}{100}$).", L"$x\\% = \\frac{x}{100}$", L"$150 \\times 20\\% = 30$", L"Muy pr\u00E1ctico para porcentajes directos."),
      NoneLyr());
    K(49, L"IGUAL", 9,
      Lyr(L"Evaluar =", L"Ejecutador", L"Eval\u00FAa la expresi\u00F3n actual y actualiza Ans.", L"$=$", L"$2+3 = 5$", L"Captura de pantalla autom\u00E1tica si est\u00E1 activa; copia seg\u00FAn configuraci\u00F3n."),
      Lyr(L"Aproximar Decimal \u2248", L"Formato", L"Fuerza la salida directa a n\u00FAmero decimal salt\u00E1ndose la fracci\u00F3n o radical exacto.", L"$\\approx$", L"$\\frac{1}{3} \\to \\mathrm{SHIFT+=} \\to 0.3333...$", L"Ahorra tener que presionar S<=>D despu\u00E9s."),
      NoneLyr(), {41, 43, 42, 49});
}

inline void PopulateEsPlusKeys(std::vector<CatalogKeyRecord>& out) {
    const wchar_t* pdf = L"fx-570_991ES_PLUS_ES.pdf";
    auto K = [&](int id, const wchar_t* name, int pg, KeyLayerInfo n, KeyLayerInfo s, KeyLayerInfo a, std::vector<int> demo = {}) {
        AddKeyRec(out, 1, id, name, pdf, pg, std::move(n), std::move(s), std::move(a), std::move(demo));
    };

    K(0, L"SHIFT", 5,
      Lyr(L"SHIFT", L"Modificador", L"Activa funciones doradas impresas sobre las teclas.", L"$\\mathrm{SHIFT} + \\mathrm{tecla}$", L"$\\mathrm{SHIFT} + \\mathrm{MODE} \\to \\mathrm{SETUP}$", L"Muestra indicador S en esquina superior izquierda."),
      NoneLyr(), NoneLyr(), {0});
    K(1, L"ON", 4,
      Lyr(L"ON", L"Sistema", L"Enciende el equipo o limpia la pantalla activa retornando al prompt del modo actual.", L"$\\mathrm{ON}$", L"Reanudar operaci\u00F3n", L"Mantiene variables almacenadas en A..F, X, Y, M."),
      NoneLyr(), NoneLyr());
    K(2, L"ARRIBA", 15,
      Lyr(L"Flecha Arriba", L"Navegaci\u00F3n", L"Desplaza hacia el historial de c\u00E1lculos anteriores o hacia arriba en plantillas Natural-V.P.A.M.", L"$\\uparrow$", L"Revisar expresi\u00F3n previa", L"Se activa cuando aparece flecha arriba en LCD."),
      NoneLyr(), NoneLyr());
    K(3, L"ALPHA", 5,
      Lyr(L"ALPHA", L"Modificador", L"Activa letras rojas (A..F, X, Y, M), signo '=' para SOLVE y ':' multisentencia.", L"$\\mathrm{ALPHA} + \\mathrm{tecla}$", L"$\\mathrm{ALPHA} + ) \\to X$", L"Muestra indicador A en la barra superior LCD."),
      NoneLyr(), NoneLyr(), {3});
    K(4, L"MODE", 6,
      Lyr(L"MODE", L"Configuraci\u00F3n", L"Abre el men\u00FA num\u00E9rico 1-8 (1:COMP, 2:CMPLX, 3:STAT, 4:BASE-N, 5:EQN, 6:MATRIX, 7:TABLE, 8:VECTOR).", L"$\\mathrm{MODE} \\to 1..8$", L"$\\mathrm{MODE} \\to 5$ (Ecuaciones)", L"En ES PLUS la navegaci\u00F3n de modos es por n\u00FAmero 1..8, no iconos."),
      Lyr(L"SETUP", L"Configuraci\u00F3n", L"Abre 1:MthIO, 2:LineIO, 3:Deg, 4:Rad, 5:Gra, 6:Fix, 7:Sci, 8:Norm (y m\u00E1s opciones con Abajo).", L"$\\mathrm{SHIFT} + \\mathrm{MODE}$", L"$\\mathrm{SHIFT} + \\mathrm{MODE} \\to 3$ (Deg)", L"Siempre revisa D/R/G e indicador Math aqu\u00ED."),
      NoneLyr(), {4});
    K(5, L"IZQUIERDA", 43,
      Lyr(L"Flecha Izquierda", L"Navegaci\u00F3n", L"Mueve cursor a la izquierda; en pantallas de error coloca el cursor exactamente sobre el fallo.", L"$\\leftarrow$", L"Corregir tras Syntax ERROR", L"Atajo indispensable al depurar f\u00F3rmulas largas."),
      NoneLyr(), NoneLyr());
    K(6, L"DERECHA", 11,
      Lyr(L"Flecha Derecha", L"Navegaci\u00F3n", L"Sale del bloque actual (fracci\u00F3n, ra\u00EDz, integral) o mueve el cursor a la derecha.", L"$\\rightarrow$", L"Avanzar entre l\u00EDmites de integral", L"Evita encerrar t\u00E9rminos por error dentro de una ra\u00EDz o exponente."),
      NoneLyr(), NoneLyr());
    K(7, L"ABAJO", 15,
      Lyr(L"Flecha Abajo", L"Navegaci\u00F3n", L"Mueve al denominador/l\u00EDmite inferior o avanza en historial reciente.", L"$\\downarrow$", L"Cambiar de numerador a denominador", L"En SETUP revela la segunda p\u00E1gina de opciones."),
      NoneLyr(), NoneLyr());
    K(8, L"CALC", 22,
      Lyr(L"CALC", L"\u00C1lgebra", L"Eval\u00FAa expresiones sustituyendo valores en A, B, C, X, Y.", L"$f(X) \\to \\mathrm{CALC} \\to X=?$", L"Evaluar $X^{2}+2$ para $X=3 \\to 11$", L"Conserva la expresi\u00F3n para probar otros valores."),
      Lyr(L"SOLVE", L"\u00C1lgebra", L"Encuentra una ra\u00EDz num\u00E9rica de una ecuaci\u00F3n con m\u00E9todo de Newton.", L"$f(X) = g(X) \\to X \\approx ?$", L"$2X + 3 = 11 \\to X = 4$", L"Introduce '=' usando ALPHA + CALC."),
      Lyr(L"=", L"\u00C1lgebra", L"Ingresa '=' simb\u00F3lico para resolver con SOLVE.", L"$\\mathrm{ALPHA} + \\mathrm{CALC} \\to =$", L"$X^{2} - 4 = 0$", L"No eval\u00FAa; s\u00F3lo construye la ecuaci\u00F3n."),
      {8});
    K(9, L"INTEGRAL", 16,
      Lyr(L"Integral Definida", L"C\u00E1lculo", L"Calcula integrales num\u00E9ricas definidas $\\int_a^b f(X)\\,dX$.", L"$\\int_{a}^{b} f(X)\\,dX$", L"$\\int_{0}^{3} (X^{2}+2X)\\,dX = 18$", L"La variable debe llamarse X (ALPHA + ')')."),
      Lyr(L"Derivada d/dx", L"C\u00E1lculo", L"Calcula la derivada num\u00E9rica en un punto $X=a$.", L"$\\left.\\frac{d}{dX}f(X)\\right|_{X=a}$", L"$\\left.\\frac{d}{dX}(\\sin X)\\right|_{X=0} = 1 \\text{ (en Rad)}$", L"Presiona flecha derecha para rellenar el valor $X=a$."),
      Lyr(L":", L"Multisentencia", L"Separa c\u00E1lculos en cadena mostrando indicador Disp entre pasos.", L"$A=2 : A\\times 5$", L"$2+3 : \\mathrm{Ans}\\times 4$", L"Pulsa = para avanzar de una sentencia a la siguiente."),
      {9, 3, 27, 14, 6, 45, 6, 41, 49});
    K(10, L"INVERSO", 16,
      Lyr(L"Inverso Multiplicativo x^-1", L"\u00C1lgebra", L"Calcula $\\frac{1}{x}$ para $x \\neq 0$ (y matriz inversa $MatA^{-1}$ en MATRIX).", L"$x^{-1} = \\frac{1}{x}$", L"$4^{-1} = 0.25$", L"Inverso multiplicativo != negativo (-). Error com\u00FAn al invertir matrices."),
      Lyr(L"Factorial x!", L"Combinatoria", L"Calcula $n!$ para enteros no negativos hasta $69!$.", L"$n!$", L"$5! = 120$", L"En ENTEROS $\\ge 70$ da Math ERROR."),
      NoneLyr(), {35, 10, 49});
    K(11, L"LOG_B", 16,
      Lyr(L"Logaritmo Base a", L"\u00C1lgebra", L"Calcula $\\log_{a}(b)$ en Natural-V.P.A.M.", L"$\\log_{a}(b)$", L"$\\log_{2}(16) = 4$", L"En formato LineIO se ingresa como log(a, b)."),
      Lyr(L"Sumatoria Sigma", L"C\u00E1lculo", L"Calcula $\\sum_{X=a}^{b} f(X)$ paso entero unitario.", L"$\\sum_{X=a}^{b} f(X)$", L"$\\sum_{X=1}^{4} X = 10$", L"Usa ALPHA + ) para la variable X."),
      Lyr(L"Productorio Pi", L"C\u00E1lculo", L"Multiplicatoria o s\u00EDmbolo en modelos VN PLUS (reemplazado por variable/funci\u00F3n seg\u00FAn firmware).", L"$\\prod$", L"Serie finita", L"Visible en serigraf\u00EDa superior de log_b."),
      {11, 41, 6, 31, 49});
    K(12, L"FRACCION", 10,
      Lyr(L"Fracci\u00F3n a/b", L"B\u00E1sicas", L"Inserta plantilla de fracci\u00F3n vertical natural.", L"$\\frac{a}{b}$", L"$\\frac{3}{4} = 0.75$", L"Alterna fracci\u00F3n/decimal con S<=>D."),
      Lyr(L"Fracci\u00F3n Mixta", L"B\u00E1sicas", L"Inserta plantilla $a\\frac{b}{c}$.", L"$a\\frac{b}{c}$", L"$1\\frac{1}{2} = \\frac{3}{2}$", L"Cambia entre mixta e impropia con SHIFT + S<=>D."),
      Lyr(L"Divisi\u00F3n Entera R", L"Aritm\u00E9tica", L"Calcula cociente y residuo entero (disponible en VN PLUS).", L"$a \\div R\\, b$", L"$17 \\div R\\, 5 \\to Q=3, R=2$", L"Funci\u00F3n espec\u00EDfica roja del modelo VN PLUS."),
      {12, 42, 7, 35, 49});
    K(13, L"RAIZ", 15,
      Lyr(L"Ra\u00EDz Cuadrada", L"\u00C1lgebra", L"Calcula $\\sqrt{x}$ exacta en radical cuando es posible (ej: $\\sqrt{8} = 2\\sqrt{2}$).", L"$\\sqrt{x}$", L"$\\sqrt{8} = 2\\sqrt{2}$", L"Presiona S<=>D para ver su aproximaci\u00F3n decimal."),
      Lyr(L"Ra\u00EDz C\u00FAbica", L"\u00C1lgebra", L"Calcula $\\sqrt[3]{x}$.", L"$\\sqrt[3]{x}$", L"$\\sqrt[3]{27} = 3$", L"Acepta radicandos negativos reales."),
      NoneLyr(), {13, 31, 49});
    K(14, L"CUADRADO", 15,
      Lyr(L"Cuadrado x^2", L"\u00C1lgebra", L"Eleva al exponente 2.", L"$x^{2}$", L"$5^{2} = 25$", L"Para n\u00FAmeros negativos usa par\u00E9ntesis: $(-5)^{2}$."),
      Lyr(L"Cubo x^3", L"\u00C1lgebra", L"Eleva al exponente 3.", L"$x^{3}$", L"$3^{3} = 27$", L"Evita escribir x^box + 3."),
      Lyr(L"DEC (Base-N)", L"Sistemas Num\u00E9ricos", L"Selecciona decimal en MODE 4 (BASE-N).", L"$x_{10}$", L"Base decimal", L"Activo dentro de MODE -> 4."),
      {36, 14, 49});
    K(15, L"POTENCIA", 15,
      Lyr(L"Potencia x^n", L"\u00C1lgebra", L"Eleva a cualquier potencia.", L"$x^{n}$", L"$2^{6} = 64$", L"Sal del cuadro de exponente con flecha derecha."),
      Lyr(L"Ra\u00EDz n-\u00E9sima", L"\u00C1lgebra", L"Ra\u00EDz de orden $n$.", L"$\\sqrt[n]{x}$", L"$\\sqrt[4]{81} = 3$", L"Navega con flechas para rellenar \u00EDndice y radicando."),
      Lyr(L"HEX (Base-N)", L"Sistemas Num\u00E9ricos", L"Selecciona hexadecimal en MODE 4.", L"$x_{16}$", L"Base 16", L"Usa teclas A-F directamente en BASE-N."),
      {41, 15, 37, 49});
    K(16, L"LOG", 16,
      Lyr(L"Logaritmo Base 10", L"\u00C1lgebra", L"Calcula $\\log_{10}(x)$ directamente (en ES PLUS esta tecla es base 10 por defecto).", L"$\\log(x)$", L"$\\log(100) = 2$", L"En ClassWiz esta tecla pas\u00F3 a log_a(b) y log fue a SHIFT+(-)."),
      Lyr(L"Potencia 10^x", L"\u00C1lgebra", L"Calcula $10^{x}$.", L"$10^{x}$", L"$10^{2} = 100$", L"Inversa de $\\log(x)$."),
      Lyr(L"BIN (Base-N)", L"Sistemas Num\u00E9ricos", L"Selecciona binario en MODE 4.", L"$x_{2}$", L"$101_{2} = 5$", L"Solo admite 0 y 1."),
      {16, 40, 45, 45, 49});
    K(17, L"LN", 16,
      Lyr(L"Logaritmo Natural", L"\u00C1lgebra", L"Calcula $\\ln(x)$ en base $e$.", L"$\\ln(x)$", L"$\\ln(1) = 0$", L"Solo para $x > 0$."),
      Lyr(L"Exponencial e^x", L"\u00C1lgebra", L"Calcula $e^{x}$.", L"$e^{x}$", L"$e^{1} \\approx 2.71828$", L"Inversa de $\\ln(x)$."),
      Lyr(L"OCT (Base-N)", L"Sistemas Num\u00E9ricos", L"Selecciona octal en MODE 4.", L"$x_{8}$", L"$10_{8} = 8$", L"D\u00EDgitos v\u00E1lidos 0..7."),
      {17, 40, 49});
    K(18, L"NEGATIVO", 9,
      Lyr(L"Signo Negativo (-)", L"B\u00E1sicas", L"Signo unario negativo distinto al bot\u00F3n de resta '-'.", L"$-x$", L"$(-4)\\times 2 = -8$", L"Usar en exponentes negativos: $10^{-3}$."),
      Lyr(L"Angulo polar", L"Complejos", L"Inserta s\u00EDmbolo polar $\\angle$ en MODE 2 (CMPLX).", L"$r\\angle\\theta$", L"$2\\angle 30^{\\circ}$", L"En ES PLUS el s\u00EDmbolo $\\angle$ est\u00E1 sobre (-), distinto de ClassWiz."),
      Lyr(L"Variable A", L"Variables", L"Ingresa variable A (ALPHA + (-)).", L"$A$", L"$2A+1$", L"Conserva valor asignado con SHIFT+RCL(STO)."),
      {18, 35, 43, 30, 49});
    K(19, L"GRADOS", 15,
      Lyr(L"Grados Min Seg", L"Trigonometr\u00EDa", L"Ingresa o convierte sexagesimales $\\circ\\,'\\,''$.", L"$d^{\\circ}m's''$", L"$45^{\\circ}30' = 45.5^{\\circ}$", L"Pulsa tras el = para convertir formato."),
      Lyr(L"FACT (Primos)", L"Aritm\u00E9tica", L"Descompone un n\u00FAmero entero en factores primos (en modelos VN PLUS).", L"$n = \\prod p_i^{k_i}$", L"$120 = 2^{3}\\times 3\\times 5$", L"Requiere tener un entero en pantalla."),
      Lyr(L"Variable B", L"Variables", L"Inserta variable B.", L"$B$", L"B en ecuaciones", L"Uso con ALPHA + grados."),
      {42, 45, 19, 42, 45, 19, 49});
    K(20, L"HYP", 15,
      Lyr(L"Hiperb\u00F3licas hyp", L"Trigonometr\u00EDa", L"Abre men\u00FA $\\sinh, \\cosh, \\tanh$ y sus inversas $\\sinh^{-1}, \\cosh^{-1}, \\tanh^{-1}$.", L"$\\sinh(x) = \\frac{e^{x}-e^{-x}}{2}$", L"$\\mathrm{hyp}\\to 1 (\\sinh)\\; 0 = 0$", L"En ClassWiz las hiperb\u00F3licas pasaron dentro de OPTN."),
      Lyr(L"Abs (Valor Absoluto)", L"\u00C1lgebra", L"Calcula valor absoluto $|x|$ o m\u00F3dulo complejo.", L"$|x|$", L"$|-9| = 9$", L"En ES PLUS est\u00E1 sobre hyp; en ClassWiz sobre '('."),
      Lyr(L"Variable C", L"Variables", L"Inserta variable C.", L"$C$", L"Variable C", L"Tambi\u00E9n d\u00EDgito hex C en BASE-N."),
      {0, 20, 18, 32, 49});
    K(21, L"SIN", 15,
      Lyr(L"Seno $\\sin$", L"Trigonometr\u00EDa", L"Calcula seno trigonom\u00E9trico seg\u00FAn D, R o G.", L"$\\sin(\\theta)$", L"$\\sin(30^{\\circ}) = 0.5$", L"Verifica siempre la letra D/R/G arriba."),
      Lyr(L"Arcoseno $\\sin^{-1}$", L"Trigonometr\u00EDa", L"Funci\u00F3n inversa del seno.", L"$\\sin^{-1}(x)$", L"$\\sin^{-1}(0.5) = 30^{\\circ}$", L"Dominio real $[-1, 1]$."),
      Lyr(L"Variable D", L"Variables", L"Inserta variable $D$.", L"$D$", L"Variable $D$", L"D\u00EDgito hex D en BASE-N."),
      {21, 42, 45, 27, 49});
    K(22, L"COS", 15,
      Lyr(L"Coseno $\\cos$", L"Trigonometr\u00EDa", L"Calcula coseno trigonom\u00E9trico.", L"$\\cos(\\theta)$", L"$\\cos(60^{\\circ}) = 0.5$", L"El coseno de $90^{\\circ}$ es 0."),
      Lyr(L"Arcocoseno $\\cos^{-1}$", L"Trigonometr\u00EDa", L"Funci\u00F3n inversa del coseno.", L"$\\cos^{-1}(x)$", L"$\\cos^{-1}(0.5) = 60^{\\circ}$", L"Dominio real $[-1, 1]$."),
      Lyr(L"Variable E", L"Variables", L"Inserta variable $E$.", L"$E$", L"Variable $E$", L"D\u00EDgito hex E en BASE-N."),
      {22, 37, 45, 27, 49});
    K(23, L"TAN", 15,
      Lyr(L"Tangente $\\tan$", L"Trigonometr\u00EDa", L"Calcula tangente trigonom\u00E9trica.", L"$\\tan(\\theta)$", L"$\\tan(45^{\\circ}) = 1$", L"Falla en as\u00EDntotas como $90^{\\circ}$."),
      Lyr(L"Arcotangente $\\tan^{-1}$", L"Trigonometr\u00EDa", L"Funci\u00F3n inversa de tangente.", L"$\\tan^{-1}(x)$", L"$\\tan^{-1}(1) = 45^{\\circ}$", L"\u00DAtil para pendiente o fase."),
      Lyr(L"Variable F", L"Variables", L"Inserta variable $F$.", L"$F$", L"Variable $F$", L"D\u00EDgito hex F en BASE-N."),
      {23, 35, 36, 27, 49});
    K(24, L"RCL", 14,
      Lyr(L"RCL (Recuperar variable)", L"Memorias", L"Recupera e inserta el valor de una variable A..F, X, Y, M. Aparece indicador RCL.", L"$\\mathrm{RCL} \\to A$", L"RCL -> A muestra contenido de A", L"Diferencia clave con ClassWiz: en ES PLUS la capa normal es RCL y STO est\u00E1 en SHIFT."),
      Lyr(L"STO (Guardar variable)", L"Memorias", L"Guarda el valor actual en A..F, X, Y, M. Aparece indicador STO.", L"$\\mathrm{SHIFT} + \\mathrm{RCL} \\to \\mathrm{var}$", L"5 -> SHIFT + RCL -> (-) (A=5)", L"Observa si arriba aparece STO antes de elegir la letra."),
      NoneLyr());
    K(25, L"ENG", 12,
      Lyr(L"Ingenier\u00EDa ENG ->", L"Formato", L"Ajusta notaci\u00F3n exponencial a potencias de 10 m\u00FAltiples de 3.", L"$m \\times 10^{3k}$", L"$2500 \\to 2.5 \\times 10^{3}$", L"SHIFT + ENG mueve el exponente al rev\u00E9s."),
      Lyr(L"ENG <-", L"Formato", L"Desplaza el exponente de ingenier\u00EDa a la izquierda ($10^{3} \\to 10^{6}$).", L"$\\leftarrow \\mathrm{ENG}$", L"SHIFT + ENG", L"Muy \u00FAtil en electr\u00F3nica y f\u00EDsica."),
      Lyr(L"i (Imaginario)", L"Complejos", L"Inserta unidad imaginaria $i$ en modo CMPLX (MODE 2).", L"$a + bi$", L"$2 + 3i$", L"Muestra indicador CMPLX arriba cuando est\u00E1 habilitado."),
      {41, 36, 45, 45, 49, 25});
    K(26, L"PAREN_ABRE", 10,
      Lyr(L"Par\u00E9ntesis (", L"Edici\u00F3n", L"Agrupa expresiones con prioridad.", L"$(...)$", L"$(1+2)\\times 3 = 9$", L"Cierra manualmente con ')' cuando hay denominadores."),
      Lyr(L"Porcentaje %", L"Aritm\u00E9tica", L"Convierte un n\u00FAmero en fracci\u00F3n centesimal $\\frac{x}{100}$.", L"$x\\% = \\frac{x}{100}$", L"$200 \\times 15\\% = 30$", L"En ES PLUS '%' est\u00E1 en SHIFT + '(' (en ClassWiz est\u00E1 sobre Ans)."),
      NoneLyr());
    K(27, L"PAREN_CIERRA", 14,
      Lyr(L"Par\u00E9ntesis )", L"Edici\u00F3n", L"Cierra grupo de par\u00E9ntesis abierto.", L"$(a)$", L"$\\sin(30)$", L"Indispensable tras funciones trigonom\u00E9tricas."),
      Lyr(L"Coma , (Separador)", L"Sintaxis", L"Separa argumentos en Pol(x, y), Rec(r, theta), RanInt#(a, b).", L"$\\mathrm{Pol}(x, y)$", L"$\\mathrm{Pol}(3, 4)$", L"ES PLUS usa coma ',' donde ClassWiz usa ';'."),
      Lyr(L"Variable X", L"Variables", L"Inserta la variable X (clave para integrales, derivadas, Tabla y SOLVE en ES PLUS).", L"$X$", L"$\\int_{0}^{2} X^{2}\\,dX$", L"En ES PLUS X requiere ALPHA + ')', pues no hay tecla x dedicada."),
      {26, 41, 43, 42, 27, 38, 41, 49});
    K(28, L"SD", 10,
      Lyr(L"S<=>D", L"Formato", L"Alterna salida simb\u00F3lica (fracci\u00F3n, radical, $\\pi$) a decimal y viceversa.", L"$\\frac{1}{4} \\Leftrightarrow 0.25$", L"$\\sqrt{2} \\Leftrightarrow 1.414213562$", L"En LineIO o salida decimal no todas las expresiones vuelven a radical."),
      Lyr(L"a b/c <=> d/c", L"Formato", L"Alterna fracci\u00F3n mixta e impropia.", L"$1\\frac{1}{3} \\Leftrightarrow \\frac{4}{3}$", L"SHIFT + S<=>D", L"R\u00E1pido tras calcular fracciones impropias."),
      Lyr(L"Variable Y", L"Variables", L"Inserta la variable Y.", L"$Y$", L"$X+Y$", L"Almacenamiento independiente."),
      {12, 42, 7, 35, 49, 28});
    K(29, L"MAS_M", 14,
      Lyr(L"M+ (Sumar a M)", L"Memorias", L"Suma la expresi\u00F3n actual a la memoria M.", L"$M \\leftarrow M + x$", L"$12\\,\\mathrm{M+}$", L"Enciende el indicador M en pantalla."),
      Lyr(L"M- (Restar de M)", L"Memorias", L"Resta el valor actual de M.", L"$M \\leftarrow M - x$", L"SHIFT + M+", L"No altera otras variables A..F."),
      Lyr(L"Variable M", L"Variables", L"Inserta el valor acumulado de M.", L"$M$", L"$M \\times 2$", L"Para ver sin borrar: RCL -> M+."),
      {40, 45, 29});
    K(30, L"7", 38,
      Lyr(L"D\u00EDgito 7", L"Num\u00E9rico", L"Ingresa 7 (o TABLE con MODE 7).", L"$7$", L"$7+3=10$", L"MODE -> 7 genera tablas $f(X)$."),
      Lyr(L"CONST (40 constantes)", L"F\u00EDsica/Qu\u00EDmica", L"Solicita n\u00FAmero de constante 01..40 de la tapa/manual (ej: 28 = $c_{0}$).", L"$c_{0}, h, G, g...$", L"SHIFT + 7 -> 28", L"A diferencia de ClassWiz (men\u00FA), ES PLUS pide el c\u00F3digo de 2 d\u00EDgitos."),
      NoneLyr(), {30, 38, 30, 49});
    K(31, L"8", 30,
      Lyr(L"D\u00EDgito 8", L"Num\u00E9rico", L"Ingresa 8 (o VECTOR con MODE 8).", L"$8$", L"$8\\div 2 = 4$", L"MODE -> 8 abre vectores VctA..VctC."),
      Lyr(L"CONV (40 conversiones)", L"Unidades", L"Pide c\u00F3digo de conversi\u00F3n 01..40 (ej: 01 = in>cm).", L"$\\mathrm{valor}\\,\\mathrm{CONV}\\,\\mathrm{cod}$", L"10 SHIFT + 8 -> 01", L"Consulta c\u00F3digos en manual p.30-40."),
      NoneLyr());
    K(32, L"9", 2,
      Lyr(L"D\u00EDgito 9", L"Num\u00E9rico", L"Ingresa 9.", L"$9$", L"$9^{2}=81$", L"Ra\u00EDz exacta 3."),
      Lyr(L"CLR (Limpiar/Reset)", L"Sistema", L"Abre men\u00FA 1:Setup, 2:Memory, 3:All.", L"$\\mathrm{SHIFT}+9 \\to 1/2/3$", L"3 (=) para inicializar todo", L"Borrar All restablece Setup y memorias por completo."),
      NoneLyr());
    K(33, L"DEL", 11,
      Lyr(L"DEL (Borrar)", L"Edici\u00F3n", L"Borra un paso atr\u00E1s en la expresi\u00F3n.", L"$\\mathrm{DEL}$", L"Editar entrada actual", L"En errores regresando con flechas permite corregir sin borrar todo."),
      Lyr(L"INS (Insertar)", L"Edici\u00F3n", L"Alterna inserci\u00F3n/sobrescritura en modo LineIO.", L"$\\mathrm{SHIFT}+\\mathrm{DEL}$", L"Modo LineIO", L"En MthIO siempre inserta."),
      NoneLyr(), {30, 31, 33});
    K(34, L"AC", 4,
      Lyr(L"AC (All Clear)", L"Edici\u00F3n", L"Limpia pantalla o error manteniendo historial y memorias.", L"$\\mathrm{AC}$", L"Limpiar expresi\u00F3n", L"Flecha Arriba recupera el c\u00E1lculo previo."),
      Lyr(L"OFF", L"Sistema", L"Apaga la calculadora.", L"$\\mathrm{SHIFT}+\\mathrm{AC}$", L"Encender de nuevo con ON", L"Conserva variables en RAM."),
      NoneLyr(), {34});
    K(35, L"4", 32,
      Lyr(L"D\u00EDgito 4", L"Num\u00E9rico", L"Ingresa 4 o selecciona BASE-N (MODE 4).", L"$4$", L"4", L"Tambi\u00E9n acceso secundario a men\u00FA MATRIX."),
      Lyr(L"Men\u00FA MATRIX", L"Matrices", L"Abre operaciones matriciales (Dim, Data, MatA..MatC, Det, Trn) cuando est\u00E1s en MODE 6.", L"$\\mathrm{SHIFT} + 4$", L"SHIFT + 4 -> 3 (MatA)", L"En ClassWiz esto lo reemplaz\u00F3 la tecla OPTN."),
      NoneLyr());
    K(36, L"5", 36,
      Lyr(L"D\u00EDgito 5", L"Num\u00E9rico", L"Ingresa 5 o selecciona EQN (MODE 5).", L"$5$", L"MODE -> 5 (Ecuaciones)", L"En MODE 5 eliges tipo de ecuaci\u00F3n 1..4."),
      Lyr(L"Men\u00FA VECTOR", L"Vectores", L"Abre men\u00FA de vectores (Dim, Data, VctA..C, Dot) dentro de MODE 8.", L"$\\mathrm{SHIFT} + 5$", L"Producto punto Dot", L"Solo activo en modo VECTOR."),
      NoneLyr());
    K(37, L"6", 32,
      Lyr(L"D\u00EDgito 6", L"Num\u00E9rico", L"Ingresa 6 o selecciona MATRIX (MODE 6).", L"$6$", L"MODE -> 6 (Matrices)", L"Atajo directo a matrices."),
      NoneLyr(), NoneLyr());
    K(38, L"POR", 35,
      Lyr(L"Multiplicaci\u00F3n x", L"B\u00E1sicas", L"Multiplica n\u00FAmeros, matrices o vectores.", L"$a \\times b$", L"$4 \\times 5 = 20$", L"Operador binario."),
      Lyr(L"Permutaci\u00F3n nPr", L"Combinatoria", L"Calcula $nPr$.", L"$nPr = \\frac{n!}{(n-r)!}$", L"$6\\mathrm{P}2 = 30$", L"Requiere enteros $0 \\le r \\le n$."),
      Lyr(L"GCD (M.C.D.)", L"Aritm\u00E9tica", L"M\u00E1ximo com\u00FAn divisor de dos enteros (en VN PLUS).", L"$\\mathrm{GCD}(a, b)$", L"$\\mathrm{GCD}(24, 36) = 12$", L"Disponible en firmware VN PLUS."),
      {36, 38, 30, 49});
    K(39, L"ENTRE", 35,
      Lyr(L"Divisi\u00F3n /", L"B\u00E1sicas", L"Divide entre $b \\neq 0$.", L"$a \\div b$", L"$9 \\div 2 = \\frac{9}{2}$", L"Cero en divisor dispara Math ERROR."),
      Lyr(L"Combinaci\u00F3n nCr", L"Combinatoria", L"Calcula $C(n,r)$ sin repeticiones.", L"$nCr = \\frac{n!}{r!(n-r)!}$", L"$6\\mathrm{C}2 = 15$", L"Utilizado en probabilidad y binomio."),
      Lyr(L"LCM (M.C.M.)", L"Aritm\u00E9tica", L"M\u00EDnimo com\u00FAn m\u00FAltiplo de dos enteros (en VN PLUS).", L"$\\mathrm{LCM}(a, b)$", L"$\\mathrm{LCM}(12, 18) = 36$", L"Disponible en firmware VN PLUS."),
      {37, 39, 41, 49});
    K(40, L"1", 24,
      Lyr(L"D\u00EDgito 1", L"Num\u00E9rico", L"Ingresa 1 o vuelve a COMP (MODE 1).", L"$1$", L"MODE -> 1 (Calcular)", L"Vuelve a c\u00E1lculo est\u00E1ndar."),
      Lyr(L"Men\u00FA STAT / DIST", L"Estad\u00EDstica", L"Abre men\u00FA estad\u00EDstico (Type, Data, Sum, Var, Reg, Distr) en MODE 3.", L"$\\mathrm{SHIFT} + 1$", L"Obtener media $\\bar{x}$ o desviaci\u00F3n", L"Solo funcional dentro del modo STAT."),
      NoneLyr());
    K(41, L"2", 20,
      Lyr(L"D\u00EDgito 2", L"Num\u00E9rico", L"Ingresa 2 o activa CMPLX (MODE 2).", L"$2$", L"MODE -> 2 (Complejos)", L"Enciende indicador CMPLX arriba."),
      Lyr(L"Men\u00FA CMPLX", L"Complejos", L"Abre 1:arg, 2:Conjg, 3:r<theta, 4:a+bi dentro de MODE 2.", L"$\\mathrm{SHIFT} + 2$", L"$\\mathrm{SHIFT}+2\\to 3$ (a polar)", L"En ClassWiz esto pas\u00F3 a OPTN."),
      NoneLyr());
    K(42, L"3", 28,
      Lyr(L"D\u00EDgito 3", L"Num\u00E9rico", L"Ingresa 3 o activa BASE-N (MODE 3/4 seg\u00FAn modelo).", L"$3$", L"3", L"En ES PLUS: 3:STAT, 4:BASE-N."),
      Lyr(L"Men\u00FA BASE", L"Sistemas Num\u00E9ricos", L"Abre operadores l\u00F3gicos (and, or, xor, Not) en MODE 4.", L"$\\mathrm{SHIFT} + 3$", L"Operaciones bit a bit", L"Solo en modo BASE-N."),
      NoneLyr());
    K(43, L"MAS", 8,
      Lyr(L"Suma +", L"B\u00E1sicas", L"Adici\u00F3n est\u00E1ndar.", L"$a + b$", L"$8+4=12$", L"Usa Ans si inicias una nueva operaci\u00F3n con +."),
      Lyr(L"Pol(x, y)", L"Coordenadas", L"Convierte rectangulares a polares $(r,\\theta)$.", L"$r=\\sqrt{x^{2}+y^{2}}$", L"$\\mathrm{Pol}(3,4)$", L"Separa con coma ',' (SHIFT + ')')."),
      Lyr(L"Int(x) (Parte entera)", L"Aritm\u00E9tica", L"Mayor entero menor o igual a $x$.", L"$\\lfloor x \\rfloor$", L"$\\mathrm{Int}(-2.4) = -3$", L"Redondea hacia menos infinito."),
      {41, 43, 35, 49});
    K(44, L"MENOS", 8,
      Lyr(L"Resta -", L"B\u00E1sicas", L"Sustracci\u00F3n binaria.", L"$a - b$", L"$10-3=7$", L"Distinta al signo negativo (-)."),
      Lyr(L"Rec(r, theta)", L"Coordenadas", L"Convierte polares a rectangulares $(X,Y)$.", L"$x=r\\cos\\theta$", L"$\\mathrm{Rec}(5, 30)$", L"Verifica D o R arriba."),
      Lyr(L"Intg(x) (Truncar)", L"Aritm\u00E9tica", L"Trunca parte fraccionaria de $x$.", L"$\\mathrm{trunc}(x)$", L"$\\mathrm{Intg}(-2.4) = -2$", L"Difiere de Int() en negativos."),
      {32, 44, 35, 49});
    K(45, L"0", 6,
      Lyr(L"D\u00EDgito 0", L"Num\u00E9rico", L"Ingresa 0.", L"$0$", L"$0$", L"Divisor cero invalida la operaci\u00F3n."),
      Lyr(L"Rnd (Redondeo)", L"Precisi\u00F3n", L"Redondea el valor interno al formato mostrado en pantalla.", L"$\\mathrm{Rnd}(x)$", L"$\\mathrm{Rnd}(1/3)$", L"Imprescindible cuando usas FIX y quieres redondeo real interno."),
      NoneLyr());
    K(46, L"PUNTO", 6,
      Lyr(L"Punto decimal .", L"Num\u00E9rico", L"Inserta separador decimal.", L"$0.5$", L"$2.5 \\times 2 = 5$", L"El punto y la coma de argumentos son teclas distintas."),
      Lyr(L"Ran#", L"Probabilidad", L"Pseudoaleatorio $[0, 1)$.", L"$\\mathrm{Ran\\#} \\in [0, 1)$", L"SHIFT + .", L"Cada '=' genera uno nuevo."),
      Lyr(L"RanInt#(a, b)", L"Probabilidad", L"Entero aleatorio en $[a, b]$.", L"$\\mathrm{RanInt\\#}(a, b)$", L"ALPHA + .", L"Separa a y b con coma ','."),
      {45, 46, 36, 49});
    K(47, L"EXP", 12,
      Lyr(L"Notaci\u00F3n x10^x", L"Cient\u00EDfica", L"Inserta potencia de diez integrada al n\u00FAmero.", L"$a \\times 10^{b}$", L"$3 \\times 10^{8}$", L"Se eval\u00FAa como unidad num\u00E9rica indivisible en divisiones."),
      Lyr(L"Constante Pi", L"Constantes", L"Inserta $\\pi$.", L"$\\pi$", L"$2\\pi$", L"Se conserva simb\u00F3lico en MthIO."),
      Lyr(L"Constante e", L"Constantes", L"Inserta n\u00FAmero de Euler $e$.", L"$e$", L"$e^{1}$", L"Muy \u00FAtil en exponenciales y complejos."),
      {42, 47, 36, 49});
    K(48, L"ANS", 4,
      Lyr(L"Ans", L"Memorias", L"\u00DAltimo resultado calculado.", L"$\\mathrm{Ans}$", L"$\\mathrm{Ans}\\times 2$", L"Se actualiza cada vez que pulsas = sin error."),
      Lyr(L"DRG> (Conversor angular)", L"Trigonometr\u00EDa", L"Permite especificar unidad o gradiente de un \u00E1ngulo puntual ($\\circ, r, g$).", L"$\\theta^{\\circ}, \\theta^{r}, \\theta^{g}$", L"$\\sin(\\pi/2\\,r) = 1$ estando en Deg", L"No cambia tu SETUP global; solo etiqueta ese \u00E1ngulo."),
      Lyr(L"PreAns", L"Memorias", L"Resultado pen\u00FAltimo (en VN PLUS).", L"$\\mathrm{PreAns}$", L"Sucesi\u00F3n de Fibonacci con Ans y PreAns", L"Excelente atajo para relaciones de recurrencia."),
      {});
    K(49, L"IGUAL", 4,
      Lyr(L"Igual =", L"Ejecutador", L"Eval\u00FAa y guarda resultado en Ans.", L"$=$", L"$5+5=10$", L"SHIFT + '=' fuerza aproximaci\u00F3n decimal directa."),
      Lyr(L"Aproximar ≈", L"Formato", L"Eval\u00FAa directamente en formato decimal.", L"$\\approx$", L"$\\sqrt{2} \\to 1.4142...$", L"Equivalente a '=' + S<=>D en un paso."),
      NoneLyr(), {41, 43, 42, 49});
}

inline void PopulateCg50Keys(std::vector<CatalogKeyRecord>& out) {
    const wchar_t* pdf = L"fx-CG50_Soft_v370_ES.pdf";
    auto K = [&](int id, const wchar_t* name, int pg, KeyLayerInfo n, KeyLayerInfo s, KeyLayerInfo a, std::vector<int> demo = {}) {
        AddKeyRec(out, 2, id, name, pdf, pg, std::move(n), std::move(s), std::move(a), std::move(demo));
    };

    K(0, L"F1", 197,
      Lyr(L"F1 (Softkey 1)", L"Men\u00FA de pantalla", L"Ejecuta la opci\u00F3n inferior 1 en pantalla LCD a color (var\u00EDa seg\u00FAn aplicaci\u00F3n).", L"$F1$", L"Graph: Selecci\u00F3n / estilo", L"F1..F6 corresponden de izquierda a derecha a los iconos inferiores del LCD."),
      Lyr(L"Trace (Rastrear)", L"Graficaci\u00F3n", L"Activa el cursor sobre la curva para leer coordenadas $(x, y)$ paso a paso.", L"$\\mathrm{SHIFT} + F1$", L"Leer $y$ en curva $Y_{1}$", L"Luego puedes teclear un n\u00FAmero $x$ directo para saltar ah\u00ED."),
      NoneLyr());
    K(1, L"F2", 160,
      Lyr(L"F2 (Softkey 2)", L"Men\u00FA de pantalla", L"Ejecuta segunda etiqueta inferior LCD.", L"$F2$", L"Run-Matrix: DEL / editar", L"Funci\u00F3n dependiente del men\u00FA activo."),
      Lyr(L"Zoom", L"Graficaci\u00F3n", L"Abre controles de acercamiento/aleje/cuadro/Auto sobre gr\u00E1ficas.", L"$\\mathrm{SHIFT} + F2$", L"Zoom -> Auto para encuadrar funci\u00F3n", L"Usa Zoom -> Box para aislar ra\u00EDces cercanas."),
      NoneLyr());
    K(2, L"F3", 147,
      Lyr(L"F3 (Softkey 3)", L"Men\u00FA de pantalla", L"Tercer bot\u00F3n contextual inferior.", L"$F3$", L"Ver men\u00FA Mat/Vct", L"Revisa barra inferior del LCD."),
      Lyr(L"V-Window", L"Graficaci\u00F3n", L"Configura l\u00EDmites de ejes $(X_{\\min}, X_{\\max}, Y_{\\min}, Y_{\\max})$ y escalas.", L"$\\mathrm{SHIFT} + F3$", L"Preset Trig o Std", L"Si tu gr\u00E1fica aparece en blanco, revisa V-Window."),
      NoneLyr());
    K(3, L"F4", 190,
      Lyr(L"F4 (Softkey 4)", L"Men\u00FA de pantalla", L"Cuarto bot\u00F3n contextual (en Run-Matrix abre MATH).", L"$F4$", L"Run-Matrix -> MATH ($\\int, d/dx, \\log_{a}b$)", L"Aquí es donde se encuentran plantillas de derivada e integral en CG50."),
      Lyr(L"Sketch", L"Graficaci\u00F3n", L"Dibuja rectas tangentes, normales, inversas o texto sobre gr\u00E1ficas.", L"$\\mathrm{SHIFT} + F4$", L"Trazar Tangente en punto", L"Requiere tener gr\u00E1fica trazada."),
      NoneLyr());
    K(4, L"F5", 197,
      Lyr(L"F5 (Softkey 5)", L"Men\u00FA de pantalla", L"Quinto bot\u00F3n contextual.", L"$F5$", L"Opciones avanzadas", L"Contexto dependiente de modo."),
      Lyr(L"G-Solv (An\u00E1lisis gr\u00E1fico)", L"Graficaci\u00F3n", L"Encuentra ra\u00EDces (ROOT), m\u00E1ximos, m\u00EDnimos, intersecciones e integral gr\u00E1fica.", L"$\\mathrm{SHIFT} + F5$", L"G-Solv -> ROOT halla ceros de $f(x)$", L"Herramienta fundamental de c\u00E1lculo gr\u00E1fico en fx-CG50."),
      NoneLyr());
    K(5, L"F6", 145,
      Lyr(L"F6 (Softkey 6)", L"Men\u00FA de pantalla", L"Sexto bot\u00F3n contextual (en Graph es DRAW; en varios men\u00FAs cambia p\u00E1gina '>').", L"$F6$", L"Graph: F6 (DRAW) grafica", L"El icono '>' en F6 indica m\u00E1s opciones."),
      Lyr(L"G<->T (Gr\u00E1fica/Tabla)", L"Graficaci\u00F3n", L"Alterna r\u00E1pidamente entre la pantalla gr\u00E1fica y el editor/tabla de funciones.", L"$\\mathrm{SHIFT} + F6$", L"Alternar pantalla visual y datos", L"\u00DAtil sin perder la ventana actual."),
      NoneLyr());
    K(6, L"SHIFT", 4,
      Lyr(L"SHIFT", L"Modificador", L"Activa funciones doradas sobre teclas (V-Window, G-Solv, SET UP, OFF, Pi).", L"$\\mathrm{SHIFT} + \\mathrm{tecla}$", L"$\\mathrm{SHIFT} + \\mathrm{MENU} \\to \\mathrm{SET\\,UP}$", L"Se muestra icono 'S' en barra de estado superior."),
      NoneLyr(), NoneLyr(), {6});
    K(7, L"OPTN", 58,
      Lyr(L"OPTN", L"Men\u00FA contextual", L"Abre cat\u00E1logo inferior con LIST, MAT/VCT, CPLX, CALC, STAT, PROB, ANGL.", L"$\\mathrm{OPTN}$", L"OPTN -> F4 (CALC) -> d/dx o Intg", L"Muchos comandos avanzados de Run-Matrix residen aqu\u00ED."),
      NoneLyr(), NoneLyr());
    K(8, L"VARS", 59,
      Lyr(L"VARS", L"Variables/Sistema", L"Permite insertar variables de sistema (V-Window, Graph Y1..Y20, Table, Picture).", L"$\\mathrm{VARS}$", L"Insertar $Y_{1}$ en c\u00E1lculo", L"Util para evaluar $Y_{1}(2)$ desde Run-Matrix."),
      Lyr(L"PRGM", L"Programaci\u00F3n", L"Comandos de control Casio BASIC en edici\u00F3n de programas.", L"$\\mathrm{SHIFT}+\\mathrm{VARS}$", L"If/Then/For", L"Solo en aplicaci\u00F3n Program."),
      NoneLyr());
    K(9, L"MENU", 48,
      Lyr(L"MENU", L"Sistema", L"Regresa al Men\u00FA Principal con iconos a color (1:Run-Matrix, 3:eActivity, 5:Graph, A:Equa...).", L"$\\mathrm{MENU}$", L"MENU -> 1 (Run-Matrix)", L"Puedes entrar a iconos presionando su n\u00FAmero o letra directamente."),
      Lyr(L"SET UP", L"Configuraci\u00F3n", L"Abre configuraci\u00F3n global del modo activo (Input/Output Math, Angle Deg/Rad, Display Fix/Sci/Norm).", L"$\\mathrm{SHIFT} + \\mathrm{MENU}$", L"SET UP -> Angle -> F1(Deg)/F2(Rad)", L"Al terminar usa EXIT."),
      NoneLyr(), {9});
    K(10, L"ARRIBA", 14, Lyr(L"Flecha Arriba", L"Navegaci\u00F3n", L"Sube en historial, men\u00FAs o gr\u00E1ficas.", L"$\\uparrow$", L"Subir en Run-Matrix", L"En Graph desplaza la vista arriba."), NoneLyr(), NoneLyr());
    K(11, L"IZQUIERDA", 14, Lyr(L"Flecha Izquierda", L"Navegaci\u00F3n", L"Izquierda en editor o gr\u00E1fica.", L"$\\leftarrow$", L"Mover cursor izquierdo", L"Usa Replay tras errores."), NoneLyr(), NoneLyr());
    K(12, L"DERECHA", 14, Lyr(L"Flecha Derecha", L"Navegaci\u00F3n", L"Sale del bloque fracci\u00F3n/ra\u00EDz/exponente o derecha en gr\u00E1fica.", L"$\\rightarrow$", L"Avanzar en editor", L"Sale del nivel actual."), NoneLyr(), NoneLyr());
    K(13, L"ABAJO", 14, Lyr(L"Flecha Abajo", L"Navegaci\u00F3n", L"Baja al denominador o siguiente rengl\u00F3n.", L"$\\downarrow$", L"Bajar opci\u00F3n", L"Navega listas y matrices."), NoneLyr(), NoneLyr());
    K(14, L"ALPHA", 5,
      Lyr(L"ALPHA", L"Modificador", L"Activa letras A-Z en rojo para nombrar variables y texto.", L"$\\mathrm{ALPHA} + \\mathrm{tecla}$", L"ALPHA + X,theta,T -> A", L"Para bloquear may\u00FAsculas continua usa SHIFT + ALPHA."),
      Lyr(L"A-LOCK (Bloqueo alfa)", L"Modificador", L"Mantiene modo alfab\u00E9tico fijo hasta volver a pulsar ALPHA.", L"$\\mathrm{SHIFT} + \\mathrm{ALPHA}$", L"Escribir nombres largos", L"Pulsa ALPHA una vez para desbloquear."),
      NoneLyr());
    K(15, L"X^2", 62,
      Lyr(L"Cuadrado x^2", L"\u00C1lgebra", L"Eleva al cuadrado.", L"$x^{2}$", L"$5^{2} = 25$", L"Para negativos usa $(-5)^{2}$."),
      Lyr(L"Ra\u00EDz Cuadrada", L"\u00C1lgebra", L"Calcula $\\sqrt{x}$ (en CG50 est\u00E1 sobre $x^{2}$ en SHIFT).", L"$\\sqrt{x}$", L"$\\mathrm{SHIFT}+x^{2}\\; 25 \\to 5$", L"Nota la diferencia con ClassWiz/ES PLUS: aqu\u00ED la ra\u00EDz requiere SHIFT."),
      Lyr(L"Letra r", L"Alfab\u00E9tico", L"Letra min\u00FAscula r (para funciones polares r=f(theta)).", L"$r$", L"Polar $r = 2\\cos(\\theta)$", L"Disponible con ALPHA."),
      {36, 15, 49});
    K(16, L"POT", 62,
      Lyr(L"Potencia ^", L"\u00C1lgebra", L"Eleva a cualquier exponente.", L"$x^{n}$", L"$2^{5} = 32$", L"Usa flecha derecha para salir del exponente en entrada Math."),
      Lyr(L"Ra\u00EDz n-\u00E9sima", L"\u00C1lgebra", L"Ra\u00EDz de orden arbitrario.", L"$\\sqrt[n]{x}$", L"$\\sqrt[4]{16} = 2$", L"Est\u00E1 en SHIFT + '^'."),
      Lyr(L"S\u00EDmbolo theta", L"Alfab\u00E9tico", L"Inserta $\\theta$.", L"$\\theta$", L"Param\u00E9trica/Polar", L"Usado en ecuaciones polares."),
      {41, 16, 36, 49});
    K(17, L"EXIT", 15,
      Lyr(L"EXIT (Salir submen\u00FA)", L"Navegaci\u00F3n", L"Retrocede un nivel desde cualquier men\u00FA, SET UP o pantalla gr\u00E1fica.", L"$\\mathrm{EXIT}$", L"Salir de SET UP a Run-Matrix", L"Equivalente al 'Atr\u00E1s' universal en fx-CG50."),
      Lyr(L"QUIT", L"Navegaci\u00F3n", L"Sale del editor modal actual.", L"$\\mathrm{SHIFT} + \\mathrm{EXIT}$", L"Cerrar pantalla", L"Alternativa a EXIT en depuraci\u00F3n."),
      NoneLyr(), {17});
    K(18, L"X_THETA", 133,
      Lyr(L"Variable X,theta,T", L"\u00C1lgebra/Gr\u00E1ficas", L"Inserta autom\u00E1ticamente $X$ en rectangulares, $\\theta$ en polares o $T$ en param\u00E9tricas.", L"$X,\\theta,T$", L"$Y_{1} = \\sin(X)$", L"Imprescindible al escribir ecuaciones de gr\u00E1fica y tablas."),
      Lyr(L"Angulo polar", L"Complejos", L"S\u00EDmbolo polar $\\angle$.", L"$r\\angle\\theta$", L"$2\\angle 45$", L"Para complejos polares en Run-Matrix."),
      Lyr(L"Variable A", L"Alfab\u00E9tico", L"Letra A.", L"$A$", L"$5 \\to A$", L"Asignable con flecha '->'."),
      {18});
    K(19, L"LOG", 62,
      Lyr(L"Logaritmo base 10", L"\u00C1lgebra", L"Calcula $\\log_{10}(x)$.", L"$\\log(x)$", L"$\\log(1000) = 3$", L"Para otra base usa MATH -> logab(."),
      Lyr(L"Base 10^x", L"\u00C1lgebra", L"Antilogaritmo $10^{x}$.", L"$10^{x}$", L"$10^{2} = 100$", L"Inversa de log."),
      Lyr(L"Variable B", L"Alfab\u00E9tico", L"Letra B.", L"$B$", L"Variable B", L"Asignable con ->."),
      {19, 40, 45, 45, 49});
    K(20, L"LN", 62,
      Lyr(L"Logaritmo Natural", L"\u00C1lgebra", L"Calcula $\\ln(x)$.", L"$\\ln(x)$", L"$\\ln(e) = 1$", L"Base $e$."),
      Lyr(L"Exponencial e^x", L"\u00C1lgebra", L"Calcula $e^{x}$.", L"$e^{x}$", L"$e^{1}$", L"Inversa de ln."),
      Lyr(L"Variable C", L"Alfab\u00E9tico", L"Letra C.", L"$C$", L"Variable C", L"ALPHA + ln."),
      {20, 40, 49});
    K(21, L"SIN", 61,
      Lyr(L"Seno $\\sin$", L"Trigonometr\u00EDa", L"Calcula $\\sin(\\theta)$ seg\u00FAn SET UP (Deg o Rad).", L"$\\sin(x)$", L"$\\sin(30^{\\circ}) = 0.5$", L"La barra superior muestra Deg o Rad."),
      Lyr(L"Arcoseno $\\sin^{-1}$", L"Trigonometr\u00EDa", L"Calcula $\\sin^{-1}(x)$.", L"$\\sin^{-1}(x)$", L"$\\sin^{-1}(0.5) = 30^{\\circ}$", L"Dominio $[-1, 1]$."),
      Lyr(L"Variable D", L"Alfab\u00E9tico", L"Letra $D$.", L"$D$", L"Variable $D$", L"ALPHA + sin."),
      {21, 42, 45, 49});
    K(22, L"COS", 61,
      Lyr(L"Coseno $\\cos$", L"Trigonometr\u00EDa", L"Calcula $\\cos(\\theta)$.", L"$\\cos(x)$", L"$\\cos(60^{\\circ}) = 0.5$", L"Verifica unidad angular."),
      Lyr(L"Arcocoseno $\\cos^{-1}$", L"Trigonometr\u00EDa", L"Calcula $\\cos^{-1}(x)$.", L"$\\cos^{-1}(x)$", L"$\\cos^{-1}(0.5) = 60^{\\circ}$", L"Dominio $[-1, 1]$."),
      Lyr(L"Variable E", L"Alfab\u00E9tico", L"Letra $E$.", L"$E$", L"Variable $E$", L"ALPHA + cos."),
      {22, 37, 45, 49});
    K(23, L"TAN", 61,
      Lyr(L"Tangente $\\tan$", L"Trigonometr\u00EDa", L"Calcula $\\tan(\\theta)$.", L"$\\tan(x)$", L"$\\tan(45^{\\circ}) = 1$", L"Falla en as\u00EDntotas."),
      Lyr(L"Arcotangente $\\tan^{-1}$", L"Trigonometr\u00EDa", L"Calcula $\\tan^{-1}(x)$.", L"$\\tan^{-1}(x)$", L"$\\tan^{-1}(1) = 45^{\\circ}$", L"Rango principal $(-90^{\\circ}, 90^{\\circ})$."),
      Lyr(L"Variable F", L"Alfab\u00E9tico", L"Letra $F$.", L"$F$", L"Variable $F$", L"ALPHA + tan."),
      {23, 35, 36, 49});
    K(24, L"FRAC", 56,
      Lyr(L"Fracci\u00F3n", L"B\u00E1sicas", L"Inserta fracci\u00F3n natural $\\frac{a}{b}$.", L"$\\frac{a}{b}$", L"$\\frac{3}{4}$", L"Alterna fracci\u00F3n/decimal con F<->D."),
      Lyr(L"Fracci\u00F3n Mixta", L"B\u00E1sicas", L"Inserta entero + fracci\u00F3n.", L"$a\\frac{b}{c}$", L"$1\\frac{1}{2}$", L"SHIFT + a/b."),
      Lyr(L"Variable G", L"Alfab\u00E9tico", L"Letra G.", L"$G$", L"Variable G", L"ALPHA + a/b."),
      {24, 42, 13, 35, 49});
    K(25, L"SD", 56,
      Lyr(L"F<->D / S<=>D", L"Formato", L"Alterna el resultado entre fracci\u00F3n/radical exacto y decimal.", L"$\\frac{3}{4} \\Leftrightarrow 0.75$", L"$\\sqrt{2} \\Leftrightarrow 1.41421...$", L"En CG50 f\u00EDsica aparece como F<->D / S<=>D."),
      Lyr(L"a b/c <-> d/c", L"Formato", L"Alterna mixta e impropia.", L"$1\\frac{1}{2} \\Leftrightarrow \\frac{3}{2}$", L"SHIFT + F<->D", L"Conversi\u00F3n inmediata."),
      Lyr(L"Variable H", L"Alfab\u00E9tico", L"Letra H.", L"$H$", L"Variable H", L"ALPHA + F<->D."),
      {24, 42, 13, 35, 49, 25});
    K(26, L"PAREN_A", 52,
      Lyr(L"Par\u00E9ntesis (", L"Edici\u00F3n", L"Abre agrupaci\u00F3n.", L"$(...)$", L"$(2+3)\\times 4 = 20$", L"Ayuda a evitar ambig\u00FCedades en f\u00F3rmulas."),
      Lyr(L"Ra\u00EDz C\u00FAbica", L"\u00C1lgebra", L"Calcula $\\sqrt[3]{x}$ (en CG50 est\u00E1 en SHIFT + '(').", L"$\\sqrt[3]{x}$", L"$\\sqrt[3]{-8} = -2$", L"Posici\u00F3n propia de la serie fx-CG."),
      Lyr(L"Variable I", L"Alfab\u00E9tico", L"Letra I.", L"$I$", L"Variable I", L"ALPHA + '('."),
      {});
    K(27, L"PAREN_C", 52,
      Lyr(L"Par\u00E9ntesis )", L"Edici\u00F3n", L"Cierra agrupaci\u00F3n.", L"$(...)$", L"$\\sin(X)$", L"Siempre cierra funciones."),
      Lyr(L"Inverso x^-1", L"\u00C1lgebra", L"Calcula rec\u00EDproco $x^{-1}$ (en CG50 est\u00E1 en SHIFT + ')').", L"$x^{-1} = \\frac{1}{x}$", L"$4^{-1} = 0.25$", L"No dividir por 0."),
      Lyr(L"Variable J", L"Alfab\u00E9tico", L"Letra J.", L"$J$", L"Variable J", L"ALPHA + ')'."),
      {});
    K(28, L"COMA", 52,
      Lyr(L"Coma ,", L"Sintaxis", L"Separa argumentos en listas, matrices y comandos.", L"$f(a, b)$", L"$\\mathrm{Pol}(3, 4)$", L"Tambi\u00E9n separa pasos en ciertos comandos."),
      Lyr(L"Conversi\u00F3n fraccional/mixta", L"Formato", L"Acceso secundario de formato r\u00E1pido.", L"$a\\frac{b}{c}$", L"SHIFT + ','", L"Formato de entrada."),
      Lyr(L"Variable K", L"Alfab\u00E9tico", L"Letra K.", L"$K$", L"Variable K", L"ALPHA + ','."),
      {});
    K(29, L"FLECHA", 59,
      Lyr(L"Asignar -> (STO)", L"Memorias", L"Asigna el valor o matriz izquierda a la variable derecha.", L"$\\mathrm{valor} \\to \\mathrm{Variable}$", L"$5 \\to \\mathrm{ALPHA}\\; A \\; \\mathrm{EXE}$", L"En CG50 '->' es tecla directa; reemplaza al STO de ClassWiz/ES."),
      Lyr(L"Cambio/Unidad", L"Edici\u00F3n", L"Alterna vista o conversi\u00F3n en editores espec\u00EDficos.", L"$\\mathrm{SHIFT} + \\to$", L"Uso contextual", L"Seg\u00FAn aplicaci\u00F3n."),
      Lyr(L"Variable L", L"Alfab\u00E9tico", L"Letra L.", L"$L$", L"Variable L", L"ALPHA + '->'."),
      {});
    K(30, L"7", 52, Lyr(L"D\u00EDgito 7", L"Num\u00E9rico", L"Ingresa 7.", L"$7$", L"7", L""), Lyr(L"CAPTURE", L"Utilidad", L"Captura imagen de pantalla LCD en equipo f\u00EDsico.", L"$\\mathrm{SHIFT}+7$", L"Captura interna CG50", L""), Lyr(L"M", L"Alfab\u00E9tico", L"Letra M.", L"$M$", L"Variable M", L""));
    K(31, L"8", 52, Lyr(L"D\u00EDgito 8", L"Num\u00E9rico", L"Ingresa 8.", L"$8$", L"8", L""), Lyr(L"CLIP", L"Edici\u00F3n", L"Copia la selecci\u00F3n al portapapeles interno de la calculadora.", L"$\\mathrm{SHIFT}+8$", L"Seleccionar y copiar f\u00F3rmula", L"Se pega con PASTE (SHIFT+9)."), Lyr(L"N", L"Alfab\u00E9tico", L"Letra N.", L"$N$", L"Variable N", L""));
    K(32, L"9", 52, Lyr(L"D\u00EDgito 9", L"Num\u00E9rico", L"Ingresa 9.", L"$9$", L"9", L""), Lyr(L"PASTE", L"Edici\u00F3n", L"Pega texto copiado con CLIP.", L"$\\mathrm{SHIFT}+9$", L"Reutilizar expresiones largas", L""), Lyr(L"O", L"Alfab\u00E9tico", L"Letra O.", L"$O$", L"Variable O", L""));
    K(33, L"DEL", 52, Lyr(L"DEL", L"Edici\u00F3n", L"Borra car\u00E1cter a la izquierda.", L"$\\mathrm{DEL}$", L"Editar", L""), Lyr(L"INS", L"Edici\u00F3n", L"Alterna inserci\u00F3n.", L"$\\mathrm{SHIFT}+\\mathrm{DEL}$", L"Insertar", L""), Lyr(L"UNDO", L"Edici\u00F3n", L"Deshacer acci\u00F3n en editor.", L"$\\mathrm{ALPHA}+\\mathrm{DEL}$", L"Recuperar edici\u00F3n", L""));
    K(34, L"AC", 48, Lyr(L"AC / ON", L"Edici\u00F3n/Sistema", L"Limpia entrada o cancela error.", L"$\\mathrm{AC}$", L"Limpiar", L"Con SHIFT apaga equipo."), Lyr(L"OFF", L"Sistema", L"Apaga la calculadora.", L"$\\mathrm{SHIFT}+\\mathrm{AC}$", L"Apagado", L""), NoneLyr());
    K(35, L"4", 58, Lyr(L"D\u00EDgito 4", L"Num\u00E9rico", L"Ingresa 4.", L"$4$", L"4", L""), Lyr(L"CATALOG", L"Referencia", L"Lista alfab\u00E9tica de todos los comandos de fx-CG50 con descripci\u00F3n y ejemplos.", L"$\\mathrm{SHIFT}+4$", L"Buscar cualquier funci\u00F3n", L"Atajo indispensable si no recuerdas el submen\u00FA de un comando."), Lyr(L"P", L"Alfab\u00E9tico", L"Letra P.", L"$P$", L"Variable P", L""));
    K(36, L"5", 52, Lyr(L"D\u00EDgito 5", L"Num\u00E9rico", L"Ingresa 5.", L"$5$", L"5", L""), Lyr(L"FORMAT", L"Apariencia", L"Cambia colores de texto/l\u00EDnea en programas, gr\u00E1ficas y tablas.", L"$\\mathrm{SHIFT}+5$", L"Color de trazo", L""), Lyr(L"Q", L"Alfab\u00E9tico", L"Letra Q.", L"$Q$", L"Variable Q", L""));
    K(37, L"6", 52, Lyr(L"D\u00EDgito 6", L"Num\u00E9rico", L"Ingresa 6.", L"$6$", L"6", L""), NoneLyr(), Lyr(L"R", L"Alfab\u00E9tico", L"Letra R.", L"$R$", L"Variable R", L""));
    K(38, L"POR", 52, Lyr(L"Multiplicaci\u00F3n x", L"B\u00E1sicas", L"Multiplica valores.", L"$a \\times b$", L"$4 \\times 5 = 20$", L""), Lyr(L"Llave {", L"Listas", L"Abre llave de lista num\u00E9rica $\\{1, 2, 3\\}$.", L"$\\{1,2,3\\}$", L"$\\{2,4,6\\} \\to \\mathrm{List\\,1}$", L"Fundamental para listas en Run-Matrix."), Lyr(L"S", L"Alfab\u00E9tico", L"Letra S.", L"$S$", L"Variable S", L""));
    K(39, L"ENTRE", 52, Lyr(L"Divisi\u00F3n /", L"B\u00E1sicas", L"Divide entre divisor no nulo.", L"$a \\div b$", L"$12 \\div 3 = 4$", L""), Lyr(L"Llave }", L"Listas", L"Cierra lista num\u00E9rica.", L"$\\{...\\}$", L"$\\{1,2,3\\}$", L""), Lyr(L"T", L"Alfab\u00E9tico", L"Letra T.", L"$T$", L"Variable T", L""));
    K(40, L"1", 52, Lyr(L"D\u00EDgito 1", L"Num\u00E9rico", L"Ingresa 1.", L"$1$", L"1", L""), Lyr(L"List", L"Listas", L"Inserta palabra reservada 'List'.", L"$\\mathrm{List}\\,1$", L"$\\mathrm{List\\,1}[3]$", L""), Lyr(L"U", L"Alfab\u00E9tico", L"Letra U.", L"$U$", L"Variable U", L""));
    K(41, L"2", 91, Lyr(L"D\u00EDgito 2", L"Num\u00E9rico", L"Ingresa 2.", L"$2$", L"2", L""), Lyr(L"Mat", L"Matrices", L"Inserta palabra reservada 'Mat' para referenciar Mat A..Mat Z.", L"$\\mathrm{Mat\\,A}$", L"$\\mathrm{Mat\\,A} \\times \\mathrm{Mat\\,B}$", L""), Lyr(L"V", L"Alfab\u00E9tico", L"Letra V.", L"$V$", L"Variable V", L""));
    K(42, L"3", 52, Lyr(L"D\u00EDgito 3", L"Num\u00E9rico", L"Ingresa 3.", L"$3$", L"3", L""), NoneLyr(), Lyr(L"W", L"Alfab\u00E9tico", L"Letra W.", L"$W$", L"Variable W", L""));
    K(43, L"MAS", 52, Lyr(L"Suma +", L"B\u00E1sicas", L"Suma.", L"$a+b$", L"$2+3=5$", L""), Lyr(L"Corchete [", L"Matrices", L"Abre corchete para definir matrices en l\u00EDnea $[[1,2],[3,4]]$.", L"$[[a,b],[c,d]]$", L"$[[1,2],[3,4]] \\to \\mathrm{Mat\\,A}$", L"En CG50 puedes crear matrices directo sin editor separado."), Lyr(L"X", L"Alfab\u00E9tico", L"Letra X.", L"$X$", L"Variable X", L""));
    K(44, L"MENOS", 52, Lyr(L"Resta -", L"B\u00E1sicas", L"Resta.", L"$a-b$", L"$9-4=5$", L""), Lyr(L"Corchete ]", L"Matrices", L"Cierra corchete de matriz.", L"$[[...]]$", L"$[[1,2],[3,4]]$", L""), Lyr(L"Y", L"Alfab\u00E9tico", L"Letra Y.", L"$Y$", L"Variable Y", L""));
    K(45, L"0", 85, Lyr(L"D\u00EDgito 0", L"Num\u00E9rico", L"Ingresa 0.", L"$0$", L"0", L""), Lyr(L"i (Imaginario)", L"Complejos", L"Unidad imaginaria $i$ en CG50 (siempre disponible sin cambiar de modo).", L"$a + bi$", L"$(2+3i)^{2} = -5+12i$", L"Ventaja CG50: admite complejos directo en Run-Matrix."), Lyr(L"Z", L"Alfab\u00E9tico", L"Letra Z.", L"$Z$", L"Variable Z", L""));
    K(46, L"PUNTO", 52, Lyr(L"Punto decimal .", L"Num\u00E9rico", L"Punto decimal.", L"$3.5$", L"3.5", L""), Lyr(L"=", L"Sintaxis", L"Inserta signo '=' relacional.", L"$A=B$", L"Condiciones/Ecuaciones", L""), Lyr(L"SPACE", L"Alfab\u00E9tico", L"Espacio en cadenas de texto.", L"$\"\\mathrm{a\\,b}\"$", L"Texto en programas", L""));
    K(47, L"EXP", 52, Lyr(L"x10^x", L"Cient\u00EDfica", L"Notaci\u00F3n exponencial.", L"$a \\times 10^{b}$", L"$6.02\\times 10^{23}$", L""), Lyr(L"Constante Pi", L"Constantes", L"Constante $\\pi$.", L"$\\pi$", L"$\\sin(\\pi/2)$", L""), Lyr(L"Comillas \"", L"Alfab\u00E9tico", L"Comillas para texto.", L"$\"\\mathrm{Hola}\"$", L"Locate/Text", L""));
    K(48, L"NEGATIVO", 52, Lyr(L"Negativo (-)", L"B\u00E1sicas", L"Signo negativo unario.", L"$-x$", L"$(-5)+2 = -3$", L"En CG50 este bot\u00F3n est\u00E1 junto a EXE; su SHIFT es Ans."), Lyr(L"Ans", L"Memorias", L"\u00DAltimo resultado calculado.", L"$\\mathrm{Ans}$", L"$\\mathrm{SHIFT} + (-)$", L"Nota importante: en CG50 Ans est\u00E1 en SHIFT+(-)."), NoneLyr());
    K(49, L"EXE", 51, Lyr(L"EXE (Ejecutar)", L"Ejecutador", L"Eval\u00FAa la operaci\u00F3n o confirma selecci\u00F3n en men\u00FAs.", L"$\\mathrm{EXE}$", L"$2+3\\,\\mathrm{EXE} \\to 5$", L"Equivalente al '=' de las calculadoras cient\u00EDficas."), Lyr(L"Nueva l\u00EDnea", L"Programaci\u00F3n", L"Salto de l\u00EDnea.", L"$\\hookleftarrow$", L"En programas", L""), NoneLyr(), {41, 43, 42, 49});
}

inline void PopulateIndicators(std::vector<LcdIndicatorDoc>& out) {
    // 20 ClassWiz indicators (order matches kCwGlyphPtrs in lcd_indicators.h)
    const wchar_t* pdfCW = L"fx-570_991LA_X_ES.pdf";
    auto CW = [&](int idx, const wchar_t* sym, const wchar_t* title, const wchar_t* meaning, const wchar_t* when, const wchar_t* keys, int pg) {
        out.push_back({ 0, idx, sym, title, meaning, when, keys, pdfCW, pg });
    };
    CW(0,  L"S",     L"Indicador SHIFT",             L"Teclado desplazado a segunda funci\u00F3n (dorada).",           L"Al presionar [SHIFT]. Se apaga despu\u00E9s de la siguiente tecla.", L"[SHIFT]", 5);
    CW(1,  L"A",     L"Indicador ALPHA",             L"Modo alfab\u00E9tico/variables rojo activo.",                   L"Al presionar [ALPHA].",                                          L"[ALPHA]", 5);
    CW(2,  L"M",     L"Memoria Independiente",       L"Existe un valor distinto de 0 guardado en la variable M.",     L"Tras usar [M+] o guardar en M con [STO].",                       L"[M+], [SHIFT]+[STO] (RECALL)", 5);
    CW(3,  L"STO",   L"En espera de Guardar (STO)",  L"El pr\u00F3ximo bot\u00F3n de variable guardar\u00E1 el n\u00FAmero en pantalla.", L"Despu\u00E9s de presionar [STO].",                 L"[STO] + A..F/x/y/M", 5);
    CW(4,  L"Math",  L"Formato Natural",             L"Entrada/salida en formato de libro de texto (fracciones y ra\u00EDces).", L"Cuando SETUP Entrada/Salida = E Mat/S Mat o E Mat/S Dec.", L"[SHIFT]+[MENU] -> 1", 7);
    CW(5,  L"D",     L"Grados Sexagesimales (Deg)",  L"La unidad angular activa es grados (360\u00B0 por vuelta).", L"Por defecto al inicializar o en SETUP -> Unidad angular -> 1.", L"[SHIFT]+[MENU] -> 2 -> 1", 16);
    CW(6,  L"R",     L"Radianes (Rad)",              L"La unidad angular activa es radianes (2\u03C0 rad por vuelta).", L"Al elegir SETUP -> Unidad angular -> 2.",                        L"[SHIFT]+[MENU] -> 2 -> 2", 16);
    CW(7,  L"G",     L"Gradianes (Gra)",             L"La unidad angular activa es gradianes (400g por vuelta).", L"Al elegir SETUP -> Unidad angular -> 3.",                       L"[SHIFT]+[MENU] -> 2 -> 3", 16);
    CW(8,  L"FIX",   L"Decimales Fijos (Fix)",       L"Los resultados visibles se redondean a N decimales fijos (0..9). No altera precisi\u00F3n interna salvo usar Rnd.", L"En SETUP -> Formato n\u00FAmero -> 1 (Fijar dec.).", L"[SHIFT]+[MENU] -> 3 -> 1", 8);
    CW(9,  L"SCI",   L"Notaci\u00F3n Cient\u00EDfica (Sci)", L"Los resultados visibles muestran N cifras significativas (0..9).", L"En SETUP -> Formato n\u00FAmero -> 2 (Not. cient\u00EDfica).", L"[SHIFT]+[MENU] -> 3 -> 2", 8);
    CW(10, L"E",     L"Indicador E / Notaci\u00F3n", L"Indica notaci\u00F3n exponencial o modo/estado especial E.",    L"Seg\u00FAn configuraci\u00F3n de formato o pantalla.",           L"[ENG], SETUP", 5);
    CW(11, L"i",     L"Modo Complejos (i)",          L"La calculadora est\u00E1 en modo N\u00FAmeros Complejos (a + bi).", L"Al entrar en MENU -> 2.",                                      L"[MENU] -> 2", 20);
    CW(12, L"Ang",   L"Coordenadas Polares (Ang)",   L"Modo polar activo o esperando magnitud/fase r\u2220\u03B8.", L"En Complejos o al configurar salida polar.",                    L"[OPTN], [SHIFT]+[ENG]", 20);
    CW(13, L"Menu+", L"M\u00E1s elementos abajo",    L"Indica desplazamiento disponible en listas o tablas.",         L"Cuando un men\u00FA supera una pantalla.",                       L"[ABAJO] / [ARRIBA]", 5);
    CW(14, L"<",     L"Expresi\u00F3n a la Izquierda", L"Hay contenido oculto hacia la izquierda de la pantalla.",   L"Al escribir o inspeccionar f\u00F3rmulas largas.",               L"[IZQUIERDA]", 11);
    CW(15, L"v",     L"Historial / Tabla abajo",     L"Existen m\u00E1s filas o resultados debajo.",                  L"Despu\u00E9s de encadenar varios c\u00E1lculos o listas.",     L"[ABAJO]", 14);
    CW(16, L"^",     L"Historial / Tabla arriba",    L"Existen c\u00E1lculos anteriores accesibles arriba.",         L"Tras ejecutar al menos dos expresiones seguidas.",               L"[ARRIBA]", 14);
    CW(17, L">",     L"Expresi\u00F3n a la Derecha", L"La expresi\u00F3n contin\u00FAa fuera del borde derecho LCD.", L"En f\u00F3rmulas largas o resultados extensos.",                L"[DERECHA]", 11);
    CW(18, L"Pause", L"Pausa Multisentencia",        L"Ejecuci\u00F3n detenida entre sentencias separadas por ':'.", L"Despu\u00E9s de presionar '=' en f\u00F3rmula con ':'.",       L"[=], [ALPHA]+[INTEGRAL]", 13);
    CW(19, L"Solar", L"Indicador Solar",             L"S\u00EDmbolo de alimentaci\u00F3n solar activa en el LCD.",    L"Visible cuando el panel recibe luz suficiente.",                 L"Panel solar superior", 5);

    // 18 ES / VN PLUS indicators (order matches kEsGlyphPtrs in lcd_indicators.h)
    const wchar_t* pdfES = L"fx-570_991ES_PLUS_ES.pdf";
    auto ES = [&](int idx, const wchar_t* sym, const wchar_t* title, const wchar_t* meaning, const wchar_t* when, const wchar_t* keys, int pg) {
        out.push_back({ 1, idx, sym, title, meaning, when, keys, pdfES, pg });
    };
    ES(0,  L"S",     L"Indicador SHIFT",             L"Funciones doradas habilitadas para la pr\u00F3xima tecla.",     L"Al presionar [SHIFT].",                                          L"[SHIFT]", 5);
    ES(1,  L"A",     L"Indicador ALPHA",             L"Variables y funciones rojas habilitadas.",                       L"Al presionar [ALPHA].",                                          L"[ALPHA]", 5);
    ES(2,  L"M",     L"Memoria Independiente",       L"Memoria M contiene un n\u00FAmero distinto de 0.",             L"Al acumular con [M+] o [SHIFT]+[RCL] -> M.",                   L"[M+], [RCL]", 5);
    ES(3,  L"STO",   L"Guardar (STO)",               L"Esperando tecla de variable A..F, X, Y, M para almacenar.",    L"Tras presionar [SHIFT] + [RCL] (STO).",                          L"[SHIFT]+[RCL]", 5);
    ES(4,  L"RCL",   L"Recuperar (RCL)",             L"Esperando tecla de variable para consultar su valor.",         L"Tras presionar [RCL].",                                          L"[RCL]", 5);
    ES(5,  L"STAT",  L"Modo Estad\u00EDstica",       L"Calculadora en modo estad\u00EDstico y regresiones.",          L"Al pulsar [MODE] -> 3 (STAT).",                                  L"[MODE] -> 3", 6);
    ES(6,  L"CMPLX", L"Modo Complejos",              L"Admite unidad imaginaria i y coordenadas polares r<theta.",     L"Al pulsar [MODE] -> 2 (CMPLX).",                                 L"[MODE] -> 2", 6);
    ES(7,  L"MAT",   L"Modo Matrices",               L"Habilita MatA, MatB, MatC y operaciones matriciales.",         L"Al pulsar [MODE] -> 6 (MATRIX).",                                L"[MODE] -> 6", 6);
    ES(8,  L"VCT",   L"Modo Vectores",               L"Habilita VctA, VctB, VctC y producto punto/cruz.",             L"Al pulsar [MODE] -> 8 (VECTOR).",                                L"[MODE] -> 8", 6);
    ES(9,  L"D",     L"Grados Sexagesimales (Deg)",  L"Unidad angular en grados (360\u00B0).",                         L"SETUP -> 3 (Deg).",                                              L"[SHIFT]+[MODE] -> 3", 6);
    ES(10, L"R",     L"Radianes (Rad)",              L"Unidad angular en radianes (2\u03C0 rad).",                     L"SETUP -> 4 (Rad).",                                              L"[SHIFT]+[MODE] -> 4", 6);
    ES(11, L"G",     L"Gradianes (Gra)",             L"Unidad angular en gradianes (400g).",                           L"SETUP -> 5 (Gra).",                                              L"[SHIFT]+[MODE] -> 5", 6);
    ES(12, L"FIX",   L"Decimales Fijos",             L"Presentaci\u00F3n redondeada a N decimales visibles.",         L"SETUP -> 6 (Fix) + d\u00EDgito 0..9.",                           L"[SHIFT]+[MODE] -> 6", 6);
    ES(13, L"SCI",   L"Notaci\u00F3n Cient\u00EDfica", L"Presentaci\u00F3n con N cifras significativas.",             L"SETUP -> 7 (Sci) + d\u00EDgito 0..9.",                           L"[SHIFT]+[MODE] -> 7", 6);
    ES(14, L"Math",  L"Visualizaci\u00F3n Natural",  L"Modo MthIO (Natural-V.P.A.M.): fracciones y radicales en 2D.", L"SETUP -> 1 (MthIO).",                                            L"[SHIFT]+[MODE] -> 1", 6);
    ES(15, L"v",     L"Historial abajo",             L"Hay resultados o l\u00EDneas adicionales debajo.",             L"Cuando existen m\u00FAltiples c\u00E1lculos en memoria.",        L"[ABAJO]", 15);
    ES(16, L"^",     L"Historial arriba",            L"Hay c\u00E1lculos previos accesibles arriba.",                 L"Tras ejecutar varias operaciones seguidas.",                      L"[ARRIBA]", 15);
    ES(17, L"Disp",  L"Pantalla en pausa (':')",     L"Indica pausa intermedia dentro de un c\u00E1lculo multisentencia.", L"Al usar [ALPHA] + [INTEGRAL] (':').",                       L"[=], [ALPHA]+[INTEGRAL]", 15);
}

inline void PopulateProcedures(std::vector<ProcedureEntry>& out) {
    // Secuencias verificadas individualmente por modelo sobre ROM / hardware oficial.
    auto mkSeq = [](bool sup, std::vector<int> k, const wchar_t* sum, const wchar_t* lcd,
                    const wchar_t* pdf, int pg, const wchar_t* note) {
        VerifiedModelSeq m;
        m.supported = sup;
        m.keys = std::move(k);
        m.stepsSummary = sum;
        m.expectedLcd = lcd;
        m.pdfFile = pdf;
        m.pdfPage = pg;
        m.note = note;
        return m;
    };

    // 1. Integral definida
    {
        ProcedureEntry p;
        p.id = L"proc_def_integral";
        p.category = L"C\u00E1lculo";
        p.title = L"Integral definida en un intervalo [a, b]";
        p.goalSummary = L"Calcula num\u00E9ricamente el \u00E1rea neta bajo la curva ingresando integrando y l\u00EDmites directamente.";
        p.latexFormula = L"$\\int_{0}^{3} (x^{2} + 2x)\\,dx = \\left[\\frac{x^{3}}{3} + x^{2}\\right]_{0}^{3} = 18$";
        p.commonErrors = L"Olvidar salir del integrando con [DERECHA], o escribir una letra distinta de x/X.";
        p.tags = L"integral area definida gauss calculo dx";
        p.perModel[0] = mkSeq(true, { 6, 40, 10, 11, 14, 43, 41, 11, 4, 45, 4, 42, 49 },
            L"[MENU]->[1] (Calcular), [INTEGRAL], [x], [x^2], [+], [2], [x], [DERECHA], [0], [DERECHA], [3], [=]",
            L"18", L"fx-570_991LA_X_ES.pdf", 17, L"Usa la tecla directa [x]; en ClassWiz el c\u00E1lculo es casi instant\u00E1neo.");
        p.perModel[1] = mkSeq(true, { 4, 40, 9, 3, 27, 14, 43, 41, 3, 27, 6, 45, 6, 42, 49 },
            L"[MODE]->[1] (COMP), [INTEGRAL], [ALPHA]+[)], [x^2], [+], [2], [ALPHA]+[)], [DERECHA], [0], [DERECHA], [3], [=]",
            L"18", L"fx-570_991ES_PLUS_ES.pdf", 16, L"En ES PLUS la variable X se ingresa con [ALPHA]+[)].");
        p.perModel[2] = mkSeq(false, {},
            L"[MENU]->[1] (Run-Matrix) -> [F4] (MATH) -> [F6] -> [F1] (Integral dx) -> ingresar f(X), 0, 3 -> [EXE]",
            L"18", L"fx-CG50_Soft_v370_ES.pdf", 25, L"Tambi\u00E9n disponible gr\u00E1ficamente en Graph -> [SHIFT]+[F5] (G-Solv) -> Integral.");
        out.push_back(std::move(p));
    }

    // 2. Derivada numerica
    {
        ProcedureEntry p;
        p.id = L"proc_num_deriv";
        p.category = L"C\u00E1lculo";
        p.title = L"Derivada num\u00E9rica en un punto x = a";
        p.goalSummary = L"Eval\u00FAa la tasa instant\u00E1nea de cambio $f'(a)$ mediante diferenciaci\u00F3n central.";
        p.latexFormula = L"$\\left.\\frac{d}{dx}(x^{3})\\right|_{x=2} = 3(2)^{2} = 12$";
        p.commonErrors = L"Usar grados cuando se deriva una funci\u00F3n trigonom\u00E9trica (en c\u00E1lculo usar Rad).";
        p.tags = L"derivada d/dx pendiente tangente calculo";
        p.perModel[0] = mkSeq(true, { 6, 40, 0, 10, 11, 15, 42, 4, 4, 41, 49 },
            L"[MENU]->[1], [SHIFT]+[INTEGRAL] (d/dx), [x], [x^n], [3], [DERECHA], [DERECHA], [2], [=]",
            L"12", L"fx-570_991LA_X_ES.pdf", 17, L"Requiere salir del exponente 3 y pasar al recuadro x = [ ] con flecha derecha.");
        p.perModel[1] = mkSeq(true, { 4, 40, 0, 9, 3, 27, 15, 42, 6, 6, 41, 49 },
            L"[MODE]->[1], [SHIFT]+[INTEGRAL] (d/dx), [ALPHA]+[)] (X), [x^n], [3], [DERECHA], [DERECHA], [2], [=]",
            L"12", L"fx-570_991ES_PLUS_ES.pdf", 16, L"Usa [ALPHA]+[)] para ingresar X.");
        p.perModel[2] = mkSeq(false, {},
            L"[MENU]->[1] (Run-Matrix) -> [F4] (MATH) -> [F4] (d/dx) -> X^3, X=2 -> [EXE]",
            L"12", L"fx-CG50_Soft_v370_ES.pdf", 25, L"En CG50 aparece en F4 (MATH) o OPTN -> F4 (CALC).");
        out.push_back(std::move(p));
    }

    // 3. Fraccion <-> Decimal S<=>D
    {
        ProcedureEntry p;
        p.id = L"proc_frac_dec";
        p.category = L"B\u00E1sicas";
        p.title = L"Convertir fracci\u00F3n o radical a decimal y viceversa (S<=>D)";
        p.goalSummary = L"Alterna al instante entre resultado exacto (fracci\u00F3n/ra\u00EDz/$\\pi$) y n\u00FAmero decimal.";
        p.latexFormula = L"$\\frac{3}{4} \\Longleftrightarrow 0.75$";
        p.commonErrors = L"Tener activado 'Mat/Decimal' en SETUP (siempre devuelve decimales y no vuelve a fracci\u00F3n).";
        p.tags = L"fraccion decimal s<=>d convertir exacta aproximada";
        p.perModel[0] = mkSeq(true, { 6, 40, 12, 42, 5, 35, 49, 28 },
            L"[MENU]->[1], [a/b], [3], [ABAJO], [4], [=], [S<=>D]",
            L"3/4 <-> 0.75", L"fx-570_991LA_X_ES.pdf", 12, L"[SHIFT]+[=] entrega 0.75 directo sin pasar por fracci\u00F3n.");
        p.perModel[1] = mkSeq(true, { 4, 40, 12, 42, 7, 35, 49, 28 },
            L"[MODE]->[1], [a/b], [3], [ABAJO], [4], [=], [S<=>D]",
            L"3/4 <-> 0.75", L"fx-570_991ES_PLUS_ES.pdf", 10, L"[SHIFT]+[S<=>D] convierte entre fracci\u00F3n mixta e impropia.");
        p.perModel[2] = mkSeq(false, {},
            L"Run-Matrix -> [a/b], [3], [ABAJO], [4], [EXE] -> [F<->D] (S<=>D)",
            L"3/4 <-> 0.75", L"fx-CG50_Soft_v370_ES.pdf", 56, L"La tecla se etiqueta F<->D / S<=>D en fx-CG50.");
        out.push_back(std::move(p));
    }

    // 4. Trigonometria Deg vs Rad
    {
        ProcedureEntry p;
        p.id = L"proc_trig_deg_rad";
        p.category = L"Trigonometr\u00EDa";
        p.title = L"Cambiar unidad angular entre Grados (Deg) y Radianes (Rad)";
        p.goalSummary = L"Configura el indicador D o R antes de evaluar $\\sin, \\cos, \\tan$ o n\u00FAmeros complejos polares.";
        p.latexFormula = L"$\\sin(30^{\\circ}) = \\frac{1}{2} \\quad | \\quad \\sin\\left(\\frac{\\pi}{6}\\right) = \\frac{1}{2}$";
        p.commonErrors = L"Evaluar sen(30) con la letra 'R' arriba produce -0.98803 en vez de 0.5.";
        p.tags = L"angulos grados radianes deg rad seno coseno tangente trigonometria";
        p.perModel[0] = mkSeq(true, { 0, 6, 41, 40, 21, 42, 45, 27, 49 },
            L"[SHIFT]+[MENU] (SETUP), [2] (Unidad angular), [1] (Grados D), [sin], [3], [0], [)], [=]",
            L"1/2 (con D en LCD)", L"fx-570_991LA_X_ES.pdf", 16, L"En SETUP -> 2 -> 2 activas Rad (R).");
        p.perModel[1] = mkSeq(true, { 0, 4, 42, 21, 42, 45, 27, 49 },
            L"[SHIFT]+[MODE] (SETUP), [3] (Deg), [sin], [3], [0], [)], [=]",
            L"1/2 (con D en LCD)", L"fx-570_991ES_PLUS_ES.pdf", 6, L"En ES PLUS el 4 selecciona Rad y el 5 Gra directo sin submen\u00FA.");
        p.perModel[2] = mkSeq(false, {},
            L"[SHIFT]+[MENU] (SET UP) -> bajar hasta Angle -> [F1] (Deg) o [F2] (Rad) -> [EXIT]",
            L"Indicador Deg / Rad", L"fx-CG50_Soft_v370_ES.pdf", 53, L"En fx-CG50 aparece texto 'Deg' o 'Rad' en la barra superior.");
        out.push_back(std::move(p));
    }

    // 5. Ecuacion cuadratica
    {
        ProcedureEntry p;
        p.id = L"proc_quad_eqn";
        p.category = L"\u00C1lgebra";
        p.title = L"Resolver ecuaci\u00F3n cuadr\u00E1tica $ax^{2} + bx + c = 0$";
        p.goalSummary = L"Obtiene ambas ra\u00EDces reales o complejas (y v\u00E9rtice en ClassWiz) ingresando coeficientes.";
        p.latexFormula = L"$x^{2} - 5x + 6 = 0 \\implies x_{1} = 3,\\; x_{2} = 2$";
        p.commonErrors = L"Olvidar usar signo negativo (-) para coeficientes negativos (ej: -5) o dejar coeficientes viejos.";
        p.tags = L"cuadratica polinomial raices ecuacion x1 x2 parabola";
        p.perModel[0] = mkSeq(true, { 6, 18, 41, 41, 40, 49, 18, 36, 49, 37, 49, 49 },
            L"[MENU], [(-)] (A:Ecuaci\u00F3n), [2] (Polin\u00F3mica), [2] (Grado 2), [1][=][(-5)][=][6][=][=]",
            L"x1 = 3, x2 = 2", L"fx-570_991LA_X_ES.pdf", 27, L"Pulsa [ABA] o [=] repetidas veces para ver x1, x2 y m\u00EDnimo/m\u00E1ximo de par\u00E1bola.");
        p.perModel[1] = mkSeq(true, { 4, 36, 42, 40, 49, 18, 36, 49, 37, 49, 49 },
            L"[MODE], [5] (EQN), [3] (aX^2+bX+c=0), [1][=][(-5)][=][6][=][=]",
            L"X1 = 3, X2 = 2", L"fx-570_991ES_PLUS_ES.pdf", 23, L"En ES PLUS grado 2 es opci\u00F3n 3 de MODE->5.");
        p.perModel[2] = mkSeq(false, {},
            L"[MENU]->[A] (Equation) -> [F2] (Polynomial) -> [F1] (Degree 2) -> ingresar a,b,c -> [F1] (SOLVE)",
            L"x1 = 3, x2 = 2", L"fx-CG50_Soft_v370_ES.pdf", 138, L"En CG50 la aplicaci\u00F3n Equa permite grados 2 a 6.");
        out.push_back(std::move(p));
    }

    // 6. Sistema de ecuaciones 2x2
    {
        ProcedureEntry p;
        p.id = L"proc_sys_2x2";
        p.category = L"\u00C1lgebra";
        p.title = L"Sistema de ecuaciones lineales 2x2 (simult\u00E1neas)";
        p.goalSummary = L"Resuelve sistemas de forma $a_{1}x + b_{1}y = c_{1}$, $a_{2}x + b_{2}y = c_{2}$ con 2, 3 o 4 inc\u00F3gnitas.";
        p.latexFormula = L"$2x + y = 5,\\; x - y = 1 \\implies x = 2,\\; y = 1$";
        p.commonErrors = L"Introducir los t\u00E9rminos constantes al lado izquierdo con signo incorrecto (deben ir al lado derecho '=c').";
        p.tags = L"sistema ecuaciones simultaneas lineales 2x2 x y";
        p.perModel[0] = mkSeq(true, { 6, 18, 40, 41, 41, 49, 40, 49, 36, 49, 40, 49, 18, 40, 49, 40, 49, 49 },
            L"[MENU], [(-)] (A:Ecuaci\u00F3n), [1] (Simul), [2] (inc\u00F3gnitas), [2][=][1][=][5][=][1][=][(-1)][=][1][=][=]",
            L"x = 2, y = 1", L"fx-570_991LA_X_ES.pdf", 27, L"Muestra mensaje expl\u00EDcito si hay infinitas soluciones o no hay soluci\u00F3n.");
        p.perModel[1] = mkSeq(true, { 4, 36, 40, 41, 49, 40, 49, 36, 49, 40, 49, 18, 40, 49, 40, 49, 49 },
            L"[MODE], [5] (EQN), [1] (anX+bnY=cn), [2][=][1][=][5][=][1][=][(-1)][=][1][=][=]",
            L"X = 2, Y = 1", L"fx-570_991ES_PLUS_ES.pdf", 22, L"Opc\u00F3n 1 de EQN corresponde a 2x2 y opci\u00F3n 2 a 3x3.");
        p.perModel[2] = mkSeq(false, {},
            L"[MENU]->[A] (Equation) -> [F1] (Simultaneous) -> [F1] (2 unknowns) -> matriz 2x3 -> [F1] (SOLVE)",
            L"x = 2, y = 1", L"fx-CG50_Soft_v370_ES.pdf", 136, L"Soporta hasta 6 inc\u00F3gnitas en fx-CG50.");
        out.push_back(std::move(p));
    }

    // 7. Resolver ecuacion arbitraria SOLVE
    {
        ProcedureEntry p;
        p.id = L"proc_solve_linear";
        p.category = L"\u00C1lgebra";
        p.title = L"Despejar inc\u00F3gnita x con SOLVE (Newton)";
        p.goalSummary = L"Encuentra una ra\u00EDz num\u00E9rica de ecuaciones lineales o no lineales cerca de un valor inicial.";
        p.latexFormula = L"$2x + 3 = 11 \\implies x = 4$";
        p.commonErrors = L"Usar el bot\u00F3n igual de abajo (= evaluador) en vez de [ALPHA]+[CALC] (= ecuaci\u00F3n).";
        p.tags = L"solve despejar ecuacion incognita newton valor inicial";
        p.perModel[0] = mkSeq(true, { 6, 40, 41, 11, 43, 42, 1, 9, 40, 40, 0, 9, 49, 49 },
            L"[MENU]->[1], [2], [x], [+], [3], [ALPHA]+[CALC](=), [1], [1], [SHIFT]+[CALC](SOLVE), [=] (semilla), [=]",
            L"x = 4, L-R = 0", L"fx-570_991LA_X_ES.pdf", 21, L"L-R = 0 confirma convergencia exacta; la solución se guarda tambi\u00E9n en la variable x.");
        p.perModel[1] = mkSeq(true, { 4, 40, 41, 3, 27, 43, 42, 3, 8, 40, 40, 0, 8, 49, 49 },
            L"[MODE]->[1], [2], [ALPHA]+[)], [+], [3], [ALPHA]+[CALC](=), [1], [1], [SHIFT]+[CALC](SOLVE), [=], [=]",
            L"X = 4, L-R = 0", L"fx-570_991ES_PLUS_ES.pdf", 22, L"En ES PLUS X requiere [ALPHA]+[)].");
        p.perModel[2] = mkSeq(false, {},
            L"[MENU]->[A] (Equation) -> [F3] (Solver) -> ingresar 2X+3=11 -> [EXE] -> valor inicial -> [F6] (SOLVE)",
            L"X = 4, Lft-Rgt=0", L"fx-CG50_Soft_v370_ES.pdf", 140, L"Tambi\u00E9n existe SolveN( en OPTN->CALC.");
        out.push_back(std::move(p));
    }

    // 8. Numeros complejos y Polar <-> Rectangular
    {
        ProcedureEntry p;
        p.id = L"proc_complex_rect_polar";
        p.category = L"Complejos";
        p.title = L"N\u00FAmeros complejos: suma, m\u00F3dulo y forma polar (r<theta)";
        p.goalSummary = L"Opera con $a+bi$ y convierte entre forma cartesiana y fasorial $r\\angle\\theta$.";
        p.latexFormula = L"$3 + 4i = 5\\angle 53.13^{\\circ}$";
        p.commonErrors = L"Intentar escribir 'i' en modo Calcular (MENU 1) da Syntax ERROR; hay que entrar al modo Complejos.";
        p.tags = L"complejos fasor polar rectangular i cmplx modulo argumento";
        p.perModel[0] = mkSeq(true, { 6, 41, 42, 43, 35, 1, 25, 49 },
            L"[MENU]->[2] (Complejos, aparece 'i'), [3], [+], [4], [ALPHA]+[ENG] (i), [=]; para polar usar [OPTN]->[ABAJO]->[1]",
            L"3 + 4i", L"fx-570_991LA_X_ES.pdf", 20, L"En ClassWiz [OPTN] -> [ABAJO] -> [1] convierte directo a $r\\angle\\theta$.");
        p.perModel[1] = mkSeq(true, { 4, 41, 42, 43, 35, 3, 25, 49 },
            L"[MODE]->[2] (CMPLX), [3], [+], [4], [ALPHA]+[ENG] (i), [=]; para polar usar [SHIFT]+[2] -> [3]",
            L"3 + 4i", L"fx-570_991ES_PLUS_ES.pdf", 20, L"ES PLUS usa [SHIFT]+[2] (CMPLX) -> 3 (>r<theta) y 4 (>a+bi).");
        p.perModel[2] = mkSeq(false, {},
            L"En Run-Matrix: [3][+][4][SHIFT]+[0](i)[EXE]; en [OPTN]->[F3] (CPLX) convertir a polar",
            L"3 + 4i", L"fx-CG50_Soft_v370_ES.pdf", 85, L"En fx-CG50 no necesitas cambiar de modo; ajusta Complex Mode en SET UP (a+bi o r<theta).");
        out.push_back(std::move(p));
    }

    // 9. Matrices 2x2
    {
        ProcedureEntry p;
        p.id = L"proc_matrix_mult";
        p.category = L"Matrices";
        p.title = L"Definir y multiplicar matrices 2x2 (MatA x MatB)";
        p.goalSummary = L"Crea dos matrices en memoria, calcula su producto y permite determinante o inversa.";
        p.latexFormula = L"$A = [[2, 1], [1, 1]],\\; \\det(A) = 1$";
        p.commonErrors = L"Dimension ERROR cuando columnas de A no coinciden con filas de B.";
        p.tags = L"matriz matrices mata matb producto determinante inversa dimension";
        p.perModel[0] = mkSeq(true, { 6, 35, 40, 41, 41, 41, 49, 40, 49, 40, 49, 40, 49 },
            L"[MENU]->[4] (Matriz), [1] (MatA), [2] filas, [2] cols, ingresar [2][=][1][=][1][=][1][=]; luego [OPTN] para operar",
            L"MatA [[2,1],[1,1]]", L"fx-570_991LA_X_ES.pdf", 28, L"ClassWiz admite matrices hasta 4x4; usa [OPTN] para Det, Trn y MatA..MatD.");
        p.perModel[1] = mkSeq(true, { 4, 37, 40, 36, 41, 49, 40, 49, 40, 49, 40, 49 },
            L"[MODE]->[6] (MATRIX), [1] (MatA), [5] (2x2), ingresar [2][=][1][=][1][=][1][=]; luego [AC] y [SHIFT]+[4] para operar",
            L"MatA 2x2", L"fx-570_991ES_PLUS_ES.pdf", 32, L"ES PLUS soporta hasta 3x3 y usa [SHIFT]+[4] (MATRIX) como men\u00FA.");
        p.perModel[2] = mkSeq(false, {},
            L"[MENU]->[1] (Run-Matrix) -> [F3] (>MAT) definir Mat A 2x2 o escribir [[2,1],[1,1]] -> [->Mat A]",
            L"Mat A 2x2", L"fx-CG50_Soft_v370_ES.pdf", 91, L"CG50 maneja matrices grandes y permite edici\u00F3n visual por celda o corchetes.");
        out.push_back(std::move(p));
    }

    // 10. Tabla f(x)
    {
        ProcedureEntry p;
        p.id = L"proc_table_fx";
        p.category = L"\u00C1lgebra";
        p.title = L"Generar tabla de valores num\u00E9ricos f(x)";
        p.goalSummary = L"Eval\u00FAa una funci\u00F3n en un rango [Inicio, Final] con paso fijo.";
        p.latexFormula = L"$f(x) = x^{2} + 1,\\; x \\in [0, 4],\\; \\Delta x = 1$";
        p.commonErrors = L"Definir un rango con demasiados pasos (m\u00E1x. ~30-45 filas) causa Insufficient MEM / Range ERROR.";
        p.tags = L"tabla valores fx rango paso graficar";
        p.perModel[0] = mkSeq(true, { 6, 32, 11, 14, 43, 40, 49, 49, 45, 49, 35, 49, 40, 49, 49 },
            L"[MENU]->[9] (Tabla), f(x)=[x^2+1][=], g(x)=[=], Inicio=[0][=], Final=[4][=], Paso=[1][=][=]",
            L"Tabla x=0..4 -> 1,2,5,10,17", L"fx-570_991LA_X_ES.pdf", 30, L"Puedes cambiar cualquier celda de la columna x en tiempo real.");
        p.perModel[1] = mkSeq(true, { 4, 30, 3, 27, 14, 43, 40, 49, 45, 49, 35, 49, 40, 49 },
            L"[MODE]->[7] (TABLE), f(X)=[X^2+1][=], Start=[0][=], End=[4][=], Step=[1][=]",
            L"Tabla X=0..4", L"fx-570_991ES_PLUS_ES.pdf", 7, L"ES PLUS permite 1 o 2 funciones (f(X), g(X) en modelos m\u00E1s nuevos).");
        p.perModel[2] = mkSeq(false, {},
            L"[MENU]->[7] (Table) -> Y1=X^2+1 -> [F5] (SET) rango -> [F6] (TABL)",
            L"Tabla color", L"fx-CG50_Soft_v370_ES.pdf", 7, L"Tambi\u00E9n genera gr\u00E1fica simult\u00E1nea con [F5]/[F6].");
        out.push_back(std::move(p));
    }

    // 11. Guardar y reutilizar variables (STO y Ans)
    {
        ProcedureEntry p;
        p.id = L"proc_store_vars";
        p.category = L"B\u00E1sicas";
        p.title = L"Guardar constantes propias en variables (A-F) y usar Ans";
        p.goalSummary = L"Almacena valores intermedios sin errores de redondeo manual y recup\u00E9ralos en c\u00E1lculos siguientes.";
        p.latexFormula = L"$9.81 \\to A,\\quad 5 \\times A = 49.05$";
        p.commonErrors = L"Confundir [STO] directa (ClassWiz) con [SHIFT]+[RCL] (ES PLUS) o con flecha [->] (CG50).";
        p.tags = L"guardar memoria sto rcl variables ans constante";
        p.perModel[0] = mkSeq(true, { 6, 40, 32, 46, 31, 40, 24, 18, 36, 38, 1, 18, 49 },
            L"[MENU]->[1], [9.81], [STO], [(-)] (A), luego [5], [x], [ALPHA]+[(-)] (A), [=]",
            L"9.81 -> A ; 49.05", L"fx-570_991LA_X_ES.pdf", 15, L"[SHIFT]+[STO] muestra todas las variables juntas (RECALL).");
        p.perModel[1] = mkSeq(true, { 4, 40, 32, 46, 31, 40, 0, 24, 18, 36, 38, 3, 18, 49 },
            L"[MODE]->[1], [9.81], [SHIFT]+[RCL] (STO), [(-)] (A), luego [5], [x], [ALPHA]+[(-)] (A), [=]",
            L"9.81 -> A ; 49.05", L"fx-570_991ES_PLUS_ES.pdf", 4, L"[RCL] sin SHIFT consulta la variable.");
        p.perModel[2] = mkSeq(false, {},
            L"En Run-Matrix: 9.81 [->] [ALPHA]+[A] [EXE]; luego 5 x ALPHA+A [EXE]",
            L"49.05", L"fx-CG50_Soft_v370_ES.pdf", 3, L"En CG50 hay 26 variables alfab\u00E9ticas A..Z + r + theta.");
        out.push_back(std::move(p));
    }

    // 12. FIX / SCI / NORM
    {
        ProcedureEntry p;
        p.id = L"proc_fix_sci_norm";
        p.category = L"Configuraci\u00F3n";
        p.title = L"Configurar decimales fijos (FIX), notaci\u00F3n cient\u00EDfica (SCI) o Normal";
        p.goalSummary = L"Controla c\u00F3mo se muestran n\u00FAmeros grandes, peque\u00F1os y decimales sin perder precisi\u00F3n interna.";
        p.latexFormula = L"$\\frac{100}{6} = 16.67\\;(\\mathrm{FIX\\,2}) \\quad|\\quad 1.67\\times 10^{1}\\;(\\mathrm{SCI\\,3})$";
        p.commonErrors = L"Creer que FIX redondea el n\u00FAmero guardado en Ans para futuras multiplicaciones (solo cambia visualizaci\u00F3n salvo usar Rnd()).";
        p.tags = L"decimales redondeo fijar fix sci normal cientifica cifras significativas formato";
        p.perModel[0] = mkSeq(true, { 6, 40, 0, 6, 42, 40, 41, 40, 45, 45, 39, 37, 49 },
            L"[SHIFT]+[MENU] (SETUP), [3] (Formato n\u00FAmero), [1] (Fix), [2] (decimales); prueba 100 / 6 =",
            L"16.67 (indicador FIX)", L"fx-570_991LA_X_ES.pdf", 8, L"Para volver a Normal: [SHIFT]+[MENU] -> 3 -> 3 -> 1.");
        p.perModel[1] = mkSeq(true, { 4, 40, 0, 4, 37, 41, 40, 45, 45, 39, 37, 49 },
            L"[SHIFT]+[MODE] (SETUP), [6] (Fix), [2] (decimales); prueba 100 / 6 =",
            L"16.67 (indicador FIX)", L"fx-570_991ES_PLUS_ES.pdf", 6, L"Para volver a Normal: [SHIFT]+[MODE] -> 8 -> 1.");
        p.perModel[2] = mkSeq(false, {},
            L"[SHIFT]+[MENU] (SET UP) -> Display -> [F1] (Fix) / [F2] (Sci) / [F3] (Norm) -> [EXIT]",
            L"FIX2 / SCI3", L"fx-CG50_Soft_v370_ES.pdf", 51, L"Configuraci\u00F3n global de Display en SET UP.");
        out.push_back(std::move(p));
    }

    // 13. Reset / Inicializar ajustes
    {
        ProcedureEntry p;
        p.id = L"proc_reset_init";
        p.category = L"Configuraci\u00F3n";
        p.title = L"Restablecer ajustes de f\u00E1brica (RESET Conf. datos / Setup)";
        p.goalSummary = L"Restaura formato Mat/Mat, grados Deg y Normal 1 para eliminar configuraciones accidentales.";
        p.latexFormula = L"$\\mathrm{SETUP} \\longrightarrow \\{\\mathrm{Mat/Mat},\\; \\mathrm{Deg},\\; \\mathrm{Norm\\,1}\\}$";
        p.commonErrors = L"Elegir 'Inicializar todo' cuando solo quer\u00EDas restaurar ajustes y perder variables/matrices.";
        p.tags = L"reset inicializar reiniciar limpiar configuracion fabrica defaults";
        p.perModel[0] = mkSeq(true, { 0, 32, 40, 49, 34 },
            L"[SHIFT]+[9] (RESET), [1] (Conf. datos), [=] (S\u00ED), [AC]",
            L"Math + D activos", L"fx-570_991LA_X_ES.pdf", 9, L"Opci\u00F3n 1 conserva variables guardadas; opci\u00F3n 3 borra todo (memorias incluidas).");
        p.perModel[1] = mkSeq(true, { 0, 32, 40, 49, 34 },
            L"[SHIFT]+[9] (CLR), [1] (Setup), [=] (Yes), [AC]",
            L"Math + D activos", L"fx-570_991ES_PLUS_ES.pdf", 2, L"En ES PLUS: 1=Setup, 2=Memory, 3=All.");
        p.perModel[2] = mkSeq(false, {},
            L"[MENU]->[G] (System) -> [F5] (Reset) -> [F1] (Setup Data) o [F2] (Main Memories)",
            L"Setup limpio", L"fx-CG50_Soft_v370_ES.pdf", 48, L"En CG50 Reset est\u00E1 dentro de la aplicaci\u00F3n System.");
        out.push_back(std::move(p));
    }

    // 14. Graficacion en fx-CG50
    {
        ProcedureEntry p;
        p.id = L"proc_cg50_graph";
        p.category = L"Graficaci\u00F3n";
        p.title = L"Graficar funci\u00F3n y encontrar ra\u00EDces con G-Solv (fx-CG50)";
        p.goalSummary = L"Traza $Y_{1}(x)$ en la pantalla a color de fx-CG50 y calcula ceros o intersecciones.";
        p.latexFormula = L"$Y_{1} = x^{2} - 4 \\implies \\mathrm{ROOT}:\\; x = -2,\\; x = 2$";
        p.commonErrors = L"Tener una curva antigua marcada o V-Window alejada del rango de la funci\u00F3n.";
        p.tags = L"grafica graficar curva root raices g-solv v-window cg50";
        p.perModel[0] = mkSeq(false, {},
            L"ClassWiz no cuenta con pantalla gr\u00E1fica; usa MENU -> 9 (Tabla) o QR para visualizar en m\u00F3vil.",
            L"Solo tabla num\u00E9rica", L"fx-570_991LA_X_ES.pdf", 30, L"Alternativa: Tabla de valores en ClassWiz o usar fx-CG50.");
        p.perModel[1] = mkSeq(false, {},
            L"fx-570ES PLUS no incluye graficaci\u00F3n; usa MODE -> 7 (TABLE) para obtener pares (x, y).",
            L"Solo tabla num\u00E9rica", L"fx-570_991ES_PLUS_ES.pdf", 7, L"Usa fx-CG50 para gr\u00E1ficas reales en pantalla.");
        p.perModel[2] = mkSeq(true, { 9, 36, 49, 18, 15, 44, 35, 49, 5, 6, 4, 0 },
            L"[MENU]->[5] (Graph), escribir [X][x^2][-][4][EXE], [F6] (DRAW), luego [SHIFT]+[F5] (G-Solv)->[F1] (ROOT)",
            L"Gr\u00E1fica de X^2-4 y ceros en -2, +2", L"fx-CG50_Soft_v370_ES.pdf", 197, L"Usa Flecha Derecha dentro de G-Solv para saltar entre m\u00FAltiples ra\u00EDces.");
        out.push_back(std::move(p));
    }
}

inline void PopulateDiagnosticsAndErrors(std::vector<DiagnosticScenario>& diags, std::vector<CasioErrorDoc>& errs) {
    diags = {
        {
            L"diag_trig_unexpected",
            L"\u00BFPor qu\u00E9 sin(30) me da -0.9880316... en lugar de 0.5?",
            L"Unidad angular",
            L"Tu calculadora tiene seleccionada la unidad Radianes (indicador 'R' o 'Rad' en la fila superior LCD), por lo que interpreta 30 como 30 radianes y no 30 grados.",
            L"$\\sin(30\\,\\mathrm{rad}) \\approx -0.98803 \\quad\\neq\\quad \\sin(30\\degree) = 0.5$",
            L"Abre Configuraci\u00F3n (SHIFT + MENU en ClassWiz/CG50 o SHIFT + MODE en ES PLUS) -> Unidad angular -> Grados (D).",
            L"fx-570_991LA_X_ES.pdf p. 16", L"fx-570_991ES_PLUS_ES.pdf p. 6",
            0, { 0, 6, 41, 40 }
        },
        {
            L"diag_frac_vs_dec",
            L"\u00BFPor qu\u00E9 la calculadora muestra 0.75 en vez de 3/4 (o al rev\u00E9s)?",
            L"Formato Entrada/Salida",
            L"El formato de salida est\u00E1 configurado en Decimal (Mat/Dec o Line/Dec) o la expresi\u00F3n supera el l\u00EDmite de 10 d\u00EDgitos para fracci\u00F3n.",
            L"$\\mathrm{Mat/Mat}:\\; \\frac{3}{4} \\to \\mathrm{S\\Leftrightarrow D} \\to 0.75 \\quad|\\quad \\mathrm{Mat/Dec}:\\; 0.75$",
            L"Pulsa [S<=>D] tras obtener el resultado, o entra a SETUP -> Entrada/Salida -> E Mat/S Mat (MthIO).",
            L"fx-570_991LA_X_ES.pdf p. 7, 12", L"fx-570_991ES_PLUS_ES.pdf p. 6, 10",
            0, { 0, 6, 40, 40 }
        },
        {
            L"diag_fix_rounding",
            L"\u00BFPor qu\u00E9 el indicador FIX redondea en pantalla pero el c\u00E1lculo siguiente no coincide?",
            L"Precisi\u00F3n vs Visualizaci\u00F3n",
            L"FIX y SCI solo formatean la salida visualizada. Internamente, CASIO sigue conservando toda la mantisa de 15 d\u00EDgitos reales.",
            L"$\\frac{4}{3} \\to \\mathrm{FIX\\,2} \\to 1.33$ (pantalla), aunque $\\mathrm{Ans} = 1.33333333333333$",
            L"Si necesitas que el valor redondeado sea el real para el siguiente paso, aplica [SHIFT]+[0] (Rnd(Ans)), o regresa a Normal 1.",
            L"fx-570_991LA_X_ES.pdf p. 8, 18", L"fx-570_991ES_PLUS_ES.pdf p. 6",
            0, { 0, 6, 42, 42, 40 }
        },
        {
            L"diag_sci_notation",
            L"\u00BFPor qu\u00E9 sale 2.5 x 10^-4 en lugar de 0.00025?",
            L"Formato num\u00E9rico Normal 1 vs Normal 2",
            L"En modo 'Normal 1' (predeterminado), todo valor menor que 0.01 (10^-2) salta autom\u00E1ticamente a notaci\u00F3n exponencial; o bien est\u00E1 activado SCI.",
            L"$\\mathrm{Norm\\,1}: |x| < 10^{-2} \\to a\\times 10^{n} \\quad|\\quad \\mathrm{Norm\\,2}: |x| < 10^{-9}$",
            L"Cambia a Normal 2 en SETUP -> Formato n\u00FAmero -> Normal -> 2 para ver decimales planos hasta 9 ceros.",
            L"fx-570_991LA_X_ES.pdf p. 8", L"fx-570_991ES_PLUS_ES.pdf p. 6",
            0, { 0, 6, 42, 42, 41 }
        },
        {
            L"diag_complex_mode",
            L"\u00BFPor qu\u00E9 aparece 'i' o 'CMPLX' arriba o no funciona sqrt(-1)?",
            L"Modos especiales",
            L"En modo Calcular est\u00E1ndar los n\u00FAmeros imaginarios no est\u00E1n permitidos (sqrt(-1) produce Math ERROR). Al entrar al modo Complejos aparece 'i'/'CMPLX'.",
            L"$\\sqrt{-4} = 2i \\;\\text{(solo v\u00E1lido en MENU 2 / MODE 2)}$",
            L"Usa MENU -> 2 (ClassWiz) o MODE -> 2 (ES PLUS) para operar con complejos; o MENU/MODE -> 1 para volver a reales.",
            L"fx-570_991LA_X_ES.pdf p. 20", L"fx-570_991ES_PLUS_ES.pdf p. 20",
            0, { 6, 41 }
        },
        {
            L"diag_minus_sign",
            L"\u00BFPor qu\u00E9 obtengo Syntax ERROR al escribir exponentes o matrices negativas?",
            L"Sintaxis de entrada",
            L"Se us\u00F3 el bot\u00F3n de resta binaria '-' en lugar del signo negativo unario '(-)', o qued\u00F3 un bloque de plantilla vac\u00EDo.",
            L"$(-5)^{2} \\text{ (correcto)} \\quad\\neq\\quad -\\text{binario al inicio de argumento}$",
            L"Pulsa [IZQUIERDA] o [DERECHA] al ver Syntax ERROR: el cursor salta al punto exacto para cambiar el operador.",
            L"fx-570_991LA_X_ES.pdf p. 10, 41", L"fx-570_991ES_PLUS_ES.pdf p. 43",
            -1, {}
        },
        {
            L"diag_neg_sq",
            L"\u00BFPor qu\u00E9 -3^2 me da -9 en vez de +9?",
            L"Prioridad de operadores",
            L"En la jerarqu\u00EDa oficial CASIO, la potencia ($x^{2}$) tiene mayor prioridad que el signo negativo unario (-).",
            L"$-3^{2} = -(3^{2}) = -9 \\quad\\neq\\quad (-3)^{2} = +9$",
            L"Envuelve siempre los n\u00FAmeros negativos entre par\u00E9ntesis antes de elevarlos al cuadrado o a potencia par.",
            L"fx-570_991LA_X_ES.pdf p. 10", L"fx-570_991ES_PLUS_ES.pdf p. 9",
            -1, {}
        },
        {
            L"diag_truncated_scroll",
            L"\u00BFPor qu\u00E9 aparecen flechitas <, >, Arriba o Abajo en la barra superior LCD?",
            L"Indicadores LCD",
            L"La expresi\u00F3n excede el ancho visible o existen resultados previos en el historial de sesi\u00F3n.",
            L"$\\leftarrow \\;\\text{y}\\; \\rightarrow \\text{ desplazan expresi\u00F3n};\\; \\uparrow \\;\\text{y}\\; \\downarrow \\text{ historial}$",
            L"Usa las 4 flechas del pad direccional central para leer toda la f\u00F3rmula o recuperar c\u00E1lculos previos.",
            L"fx-570_991LA_X_ES.pdf p. 11, 14", L"fx-570_991ES_PLUS_ES.pdf p. 15",
            -1, {}
        },
        {
            L"diag_memory_leftover",
            L"\u00BFPor qu\u00E9 aparece la letra 'M' arriba si no estoy guardando nada?",
            L"Indicadores LCD",
            L"La memoria independiente M acumulada tiene un n\u00FAmero distinto de 0 guardado de una operaci\u00F3n anterior con [M+].",
            L"$M \\neq 0 \\implies \\text{indicador } \\mathbf{M} \\text{ encendido}$",
            L"Consulta su valor con RECALL ([SHIFT]+[STO] en CW o [RCL]+[M+] en ES) o ponla a cero con 0 -> STO -> M+.",
            L"fx-570_991LA_X_ES.pdf p. 15", L"fx-570_991ES_PLUS_ES.pdf p. 4",
            -1, {}
        },
        {
            L"diag_slow_integral",
            L"\u00BFPor qu\u00E9 tarda mucho una integral definida o muestra 'Time Out'?",
            L"C\u00E1lculo num\u00E9rico",
            L"La integraci\u00F3n num\u00E9rica Gauss-Kronrod requiere m\u00E1s subdivisiones en funciones oscilatorias o cerca de as\u00EDntotas/discontinuidades.",
            L"$\\int_{a}^{b} f(x)\\,dx \\approx \\sum_{i} w_{i} f(x_{i})$",
            L"Divide el intervalo de integraci\u00F3n alrededor de picos, usa tolerancia tol mayor si aplica o verifica el dominio.",
            L"fx-570_991LA_X_ES.pdf p. 17, 42", L"fx-570_991ES_PLUS_ES.pdf p. 16",
            -1, {}
        }
    };

    errs = {
        { L"Math ERROR (ERROR C\u00E1lculo)",   L"Todas",          L"Divisi\u00F3n entre 0, ra\u00EDz par negativa en modo real, log(<=0), argumento > |1| en sin^-1/cos^-1 o resultado >= 10^100.", L"Pulsa [<] o [>] para editar la expresi\u00F3n o [AC] para limpiar. Verifica unidad angular y dominio real/complejo.", L"fx-570_991LA_X_ES.pdf", 41 },
        { L"Syntax ERROR (ERROR Sintaxis)",  L"Todas",          L"Formato de comando incorrecto, cuadro vac\u00EDo en plantilla, dos operadores binarios seguidos o uso de '-' en lugar de '(-)'.", L"Pulsa [<] o [>] al ver el error: el cursor se posicionar\u00E1 exactamente donde ocurri\u00F3 el fallo de sintaxis.", L"fx-570_991ES_PLUS_ES.pdf", 43 },
        { L"Stack ERROR (ERROR Pila)",       L"Todas",          L"Capacidad de pila num\u00E9rica (10 niveles en cient\u00EDficas) o pila de comandos excedida por demasiados par\u00E9ntesis/funciones anidadas.", L"Simplifica la expresi\u00F3n en pasos usando variables (STO) o Ans para no anidar tanto.", L"fx-570_991LA_X_ES.pdf", 41 },
        { L"Dimension ERROR",                L"Matrices/Vectores/Listas", L"Operaci\u00F3n entre matrices o vectores de dimensiones incompatibles (ej: sumar 2x2 + 2x3) o usar matriz sin definir.", L"Revisa dimensiones m x n antes de sumar/multiplicar; en multiplicaci\u00F3n A*B exige cols(A) == filas(B).", L"fx-570_991ES_PLUS_ES.pdf", 44 },
        { L"Time Out (Tiempo agotado)",      L"ClassWiz / ES",  L"El algoritmo num\u00E9rico de integral o derivada no alcanz\u00F3 convergencia dentro del n\u00FAmero m\u00E1ximo de subdivisiones.", L"Aumenta el valor de tolerancia 'tol' o parte la integral evitando puntos donde la derivada salta.", L"fx-570_991LA_X_ES.pdf", 42 },
        { L"Non-Real ERROR / Argument",      L"fx-CG50",        L"Resultado complejo cuando SET UP Complex Mode est\u00E1 en Real, o relaci\u00F3n de argumentos inv\u00E1lida como nCr(3, 10).", L"Cambia SET UP -> Complex Mode a 'a+bi' o verifica que n >= r >= 0.", L"fx-CG50_Soft_v370_ES.pdf", 605 }
    };
}

inline void PopulateTipsAndProfiles(
    std::vector<TipWarningCard>& tips,
    std::vector<SetupProfilePreset>& profiles,
    std::vector<ChecklistItem>& checklist,
    std::vector<ModelComparisonRow>& comp
) {
    tips = {
        { L"tip_01", L"Truco",       L"Atajo SHIFT + (=) para decimal directo",      L"Pulsa [SHIFT] antes de [=] para obtener la aproximaci\u00F3n decimal directa sin pasar por la fracci\u00F3n.", L"$\\frac{7}{3} \\to \\mathrm{SHIFT+=} \\to 2.333333333$", L"fx-570_991LA_X_ES.pdf p. 12", 0, {12, 30, 5, 42, 0, 49} },
        { L"tip_02", L"Truco",       L"Encadenar iteraciones r\u00E1pidas con Ans",  L"Si escribes 'x_0 =' y luego una f\u00F3rmula con 'Ans', cada pulso de [=] aplica el siguiente paso iterativo (Newton o punto fijo).", L"$x_{n+1} = \\frac{1}{2}\\left(x_{n} + \\frac{2}{x_{n}}\\right)$", L"fx-570_991LA_X_ES.pdf p. 13", 0, {} },
        { L"tip_03", L"Truco",       L"Recuperar c\u00E1lculos tras pulsar AC",     L"Presionar [AC] limpia la pantalla pero no borra el historial reciente: pulsa [ARRIBA] para recuperar la expresi\u00F3n anterior.", L"$\\uparrow \\text{ recorre el historial}$", L"fx-570_991LA_X_ES.pdf p. 14", 0, {} },
        { L"tip_04", L"Truco",       L"Inspeccionar todas las variables juntas",     L"En ClassWiz [SHIFT]+[STO] (RECALL) lista A, B, C, D, E, F, M, x, y al mismo tiempo.", L"$\\mathrm{RECALL} \\to \\{A..F, M, x, y\\}$", L"fx-570_991LA_X_ES.pdf p. 15", 0, {0, 24} },
        { L"tip_05", L"Truco",       L"Especificar un \u00E1ngulo aislado en radianes o grados", L"En lugar de cambiar todo el SETUP, agrega unidad puntual con OPTN -> Unidad angular (o SHIFT+Ans en ES PLUS).", L"$\\sin\\left(\\frac{\\pi}{2}^{r}\\right) = 1$", L"fx-570_991LA_X_ES.pdf p. 16", 0, {} },
        { L"tip_06", L"Truco",       L"Descomposici\u00F3n prima instant\u00E1nea",   L"Tras calcular un n\u00FAmero entero, pulsa [SHIFT]+[grados] (FACT) para ver su descomposici\u00F3n factorial.", L"$360 = 2^{3} \\times 3^{2} \\times 5$", L"fx-570_991LA_X_ES.pdf p. 10", 0, {42, 37, 45, 49, 0, 19} },
        { L"warn_01", L"Advertencia", L"Negativo al cuadrado sin par\u00E9ntesis", L"$-5^{2}$ se interpreta como $-(5^{2})=-25$. Usa siempre $(-5)^{2}=25$.", L"$-5^{2} = -25 \\quad\\neq\\quad (-5)^{2} = 25$", L"fx-570_991LA_X_ES.pdf p. 10", 0, {18, 36, 14, 49} },
        { L"warn_02", L"Advertencia", L"Multiplicaci\u00F3n impl\u00EDcita en divisores", L"Expresiones como $6 \\div 2(1+2)$ o $1 \\div 2\\pi$ pueden diferir de la notaci\u00F3n lineal cl\u00E1sica; usa la plantilla fracci\u00F3n $\\frac{a}{b}$ siempre.", L"$\\frac{1}{2\\pi} \\quad\\text{es inequ\u00EDvoco}$", L"fx-570_991LA_X_ES.pdf p. 10", 0, {} },
        { L"warn_03", L"Advertencia", L"Inverso multiplicativo x^-1 vs signo (-)",  L"La tecla [$x^{-1}$] calcula $\\frac{1}{x}$, no cambia el signo. Nunca uses [(-)] para invertir ni [$x^{-1}$] para negar.", L"$4^{-1} = \\frac{1}{4}, \\quad (-4) = -4$", L"fx-570_991LA_X_ES.pdf p. 16", 0, {35, 20, 49} },
        { L"warn_04", L"Advertencia", L"SOLVE usa ALPHA+= y no el bot\u00F3n inferior", L"Para despejar ecuaciones debes separar lados con [=] rojo ([ALPHA]+[CALC]); el [=] gris de abajo intenta evaluar al instante.", L"$2x + 3 \\mathbf{=} 11$", L"fx-570_991LA_X_ES.pdf p. 21", 0, {} },
        { L"warn_05", L"Advertencia", L"Dejar Radianes o FIX activos en otra materia", L"Cambiar a Rad para C\u00E1lculo o a FIX 2 en F\u00EDsica persiste entre sesiones hasta restablecerlo en SETUP.", L"$\\text{Indicador } \\mathbf{D} / \\mathbf{R} \\;\\text{y}\\; \\mathbf{FIX}$", L"fx-570_991LA_X_ES.pdf p. 8, 16", 0, {} },
        { L"care_01", L"Cuidado",     L"FIX no trunca el valor interno real",       L"Con FIX 2, $1/3 + 1/3 + 1/3$ mostrar\u00E1 1.00 porque internamente conserva todos los d\u00EDgitos; aplica Rnd() solo si quieres truncado estricto.", L"$\\mathrm{Rnd}(x) \\text{ s\u00ED recorta la memoria}$", L"fx-570_991LA_X_ES.pdf p. 18", 0, {} },
        { L"care_02", L"Cuidado",     L"Diferencia entre Reset Setup vs Borrar Todo", L"SHIFT+9 -> 1 (Conf. datos) restablece ajustes manteniendo memorias; la opci\u00F3n 3 borra variables, matrices y estad\u00EDstica.", L"$\\mathrm{SHIFT}+9 \\to 1\\;\\text{(seguro)} \\;vs\\; 3\\;\\text{(borrado total)}$", L"fx-570_991LA_X_ES.pdf p. 9", 0, {} },
        { L"care_03", L"Cuidado",     L"Cambiar de modo borra historial y entrada",  L"Al salir del modo Calcular hacia Complejos, Matrices o Estad\u00EDstica se borran los resultados anteriores y el historial temporal.", L"$\\mathrm{MENU} \\to \\text{nuevo modo limpia historial}$", L"fx-570_991LA_X_ES.pdf p. 14", 0, {} },
        { L"care_04", L"Cuidado",     L"Hoja de c\u00E1lculo y Tabla no guardan al apagar", L"En ClassWiz las celdas de Hoja de c\u00E1lculo se borran al cambiar de aplicaci\u00F3n o apagar.", L"$\\text{Guardar resultados importantes en captura PNG}$", L"fx-570_991LA_X_ES.pdf p. 36", 0, {} },
        { L"care_05", L"Cuidado",     L"Saltarse el cierre con [DERECHA] en plantillas", L"En $\\sqrt{a}+b$ o $2^{x}+1$, si no sales con [DERECHA], el '+b' queda dentro de la ra\u00EDz o exponente.", L"$\\sqrt{25}+1=6 \\;\\neq\\; \\sqrt{25+1}=5.099$", L"fx-570_991LA_X_ES.pdf p. 11", 0, {} }
    };

    profiles = {
        {
            L"prof_std_deg",
            L"Est\u00E1ndar Ciencias / Ingenier\u00EDa (Grados Deg)",
            L"Configuraci\u00F3n \u00F3ptima para \u00E1lgebra, f\u00EDsica b\u00E1sica y geometr\u00EDa anal\u00EDtica.",
            L"Entrada/Salida: Natural (Math)  |  \u00C1ngulo: Grados (D)  |  Formato: Normal 1",
            L"Verifica que el modo sea Calcular (MENU 1 / MODE 1).",
            { 6, 40, 0, 6, 40, 40, 0, 6, 41, 40, 0, 6, 42, 42, 40 },
            { 4, 40, 0, 4, 40, 0, 4, 42, 0, 4, 31, 40 }
        },
        {
            L"prof_calc_rad",
            L"C\u00E1lculo Diferencial e Integral (Radianes Rad)",
            L"Imprescindible para derivadas, integrales y series trigonom\u00E9tricas.",
            L"Entrada/Salida: Natural (Math)  |  \u00C1ngulo: Radianes (R)  |  Formato: Normal 1",
            L"Recuerda volver a Deg si luego resuelves tri\u00E1ngulos en grados sexagesimales.",
            { 6, 40, 0, 6, 41, 41, 0, 6, 40, 40, 0, 6, 42, 42, 40 },
            { 4, 40, 0, 4, 35, 0, 4, 40, 0, 4, 31, 40 }
        },
        {
            L"prof_fix_2",
            L"Finanzas / Laboratorio (2 Decimales Fijos FIX 2)",
            L"Muestra siempre exactamente dos decimales en pantalla facilitando lecturas monetarias o de balanza.",
            L"Entrada/Salida: Mat/Decimal  |  Formato: FIX 2",
            L"Los c\u00E1lculos internos mantienen 15 d\u00EDgitos reales.",
            { 6, 40, 0, 6, 42, 40, 41 },
            { 4, 40, 0, 4, 37, 41 }
        },
        {
            L"prof_sci_3",
            L"Laboratorio Qu\u00EDmica / F\u00EDsica (Notaci\u00F3n Cient\u00EDfica SCI 3)",
            L"Expresa todas las salidas con 3 cifras significativas y potencia de 10.",
            L"Formato: SCI 3  |  Normal desactivado hasta volver a SETUP",
            L"Recuerda regresar a Normal 1 cuando termines la pr\u00E1ctica.",
            { 6, 40, 0, 6, 42, 41, 42 },
            { 4, 40, 0, 4, 30, 42 }
        },
        {
            L"prof_clean_eval",
            L"Estado Limpio para Evaluaci\u00F3n / Comprobaci\u00F3n",
            L"Restablece configuraci\u00F3n predeterminada (Math + D + Norm 1) sin dejar FIX/SCI residuales.",
            L"RESET Conf. datos -> Mat/Mat, Grados D, Norm 1, pantalla limpia",
            L"Limpia tambi\u00E9n M y variables si no quieres valores previos.",
            { 0, 32, 40, 49, 34 },
            { 0, 32, 40, 49, 34 }
        }
    };

    checklist = {
        { L"chk_angle",   L"Verificar indicador angular (D o R)",            L"Aseg\u00FArate de ver 'D' para grados ($30^{\\circ}$) o 'R' para radianes ($\\pi/2$).", L"Fila LCD superior: D / R / G", false },
        { L"chk_math",    L"Comprobar formato Entrada/Salida Natural (Math)", L"Si quieres fracciones y ra\u00EDces reales debe estar visible el indicador 'Math'.",         L"Indicador 'Math' activo", false },
        { L"chk_fixsci",  L"Revisar que no haya FIX o SCI activo por descuido", L"Un FIX antiguo redondear\u00E1 tus resultados visibles aunque no lo necesites.",          L"Sin FIX / SCI salvo pedido", false },
        { L"chk_mode",    L"Confirmar modo general (MENU 1 / MODE 1)",       L"Si ves 'i', 'CMPLX', 'MAT' o 'STAT' y vas a calcular normal, vuelve al modo 1.",        L"Sin indicadores i/CMPLX/MAT", false },
        { L"chk_mem",     L"Revisar o limpiar memoria M y variables",        L"Si aparece 'M' arriba, guarda 0 o revisa RECALL antes de empezar.",                       L"Sin M o revisada con RECALL", false },
        { L"chk_parens",  L"Usar [(-)] y par\u00E9ntesis en potencias negativas", L"Prueba Mental: $(-2)^{2} = 4$, pero $-2^{2} = -4$.",                                  L"Tecla (-) vs tecla resta -", false },
        { L"chk_witness", L"Ejecutar operaci\u00F3n testigo r\u00E1pida",     L"Teclea sin(30)= en Grados; si da 0.5 y 1/2 queda como fracci\u00F3n, est\u00E1 listo.",  L"sin(30)=1/2 y 0.5 con S<=>D", false },
        { L"chk_right",   L"Salir de bloques con flecha [DERECHA]",          L"Siempre presiona [DERECHA] antes de sumar un t\u00E9rmino fuera de $\\sqrt{\\;}$ o $x^{n}$.", L"Cursor fuera de ra\u00EDz", false }
    };

    comp = {
        { L"Acceso a Modos",         L"Navegaci\u00F3n",   L"[MENU] abre cuadr\u00EDcula visual de 12 iconos (1..9, A..C).",      L"[MODE] muestra lista textual numerada 1..8.",            L"[MENU] muestra iconos a color con Python, Graph, eActivity.", L"ClassWiz y CG50 usan letras A..C / A..H en teclas; ES PLUS solo 1..8.", {6}, {4} },
        { L"Entrada de Variable x",  L"\u00C1lgebra",      L"Tiene tecla dedicada [x] arriba a la derecha (1 clic).",             L"Requiere dos teclas: [ALPHA] seguido de [ ) ].",          L"Tiene tecla dedicada [X,theta,T] adaptativa al tipo gr\u00E1fico.", L"Mucho m\u00E1s r\u00E1pido ingresar polinomios en ClassWiz y CG50.", {11}, {3, 27} },
        { L"Guardar Variable (STO)", L"Memorias",         L"Tecla [STO] directa (sin SHIFT) + variable.",                         L"Requiere [SHIFT] + [RCL] (STO) + variable.",              L"Tecla directa flecha [->] entre valor y variable ALPHA.",    L"En ES PLUS RCL est\u00E1 en capa normal y STO oculto en SHIFT.", {24}, {0, 24} },
        { L"Logaritmos base 10 vs a", L"\u00C1lgebra",     L"Tecla principal es log_a(b); log_10 est\u00E1 en SHIFT+(-).",       L"Tecla dedicada [log] (base 10) + tecla separada [log_box].", L"Tecla directa [log] (base 10); base a dentro de F4 MATH.",   L"ES PLUS tiene ambas teclas f\u00EDsicas visibles al mismo tiempo.", {16}, {16} },
        { L"Men\u00FA contextual (MAT/CMPLX)", L"Operaciones", L"Usa una sola tecla [OPTN] sensible al modo actual.",                L"Usa [SHIFT]+[1..5] distintos seg\u00FAn sea STAT, CMPLX, MAT o VCT.", L"[OPTN] + botones F1..F6 contextuales debajo de la pantalla.", L"ClassWiz unific\u00F3 todos los submen\u00FAs en [OPTN].", {8}, {0, 35} },
        { L"Constantes Cient\u00EDficas", L"Ciencia",     L"47 constantes con men\u00FA categorizado en [SHIFT]+[7].",            L"40 constantes ingresando c\u00F3digo num\u00E9rico 01..40.", L"Biblioteca integrada / app Physium a color.",              L"En ClassWiz no necesitas memorizar los c\u00F3digos num\u00E9ricos.", {0, 30}, {0, 30} },
        { L"Matrices m\u00E1ximas",  L"Matrices",         L"Hasta 4 filas x 4 columnas (MatA..MatD).",                           L"Hasta 3 filas x 3 columnas (MatA..MatC).",               L"Matrices grandes (hasta decenas de filas/cols) en tabla color.", L"Si necesitas sistemas/matrices 4x4, usa ClassWiz o CG50.", {}, {} },
        { L"Porcentaje % y Abs",     L"Aritm\u00E9tica",  L"[%] en [SHIFT]+[Ans]; Abs en [SHIFT]+[(].",                          L"[%] en [SHIFT]+[(]; Abs en [SHIFT]+[hyp].",                L"Abs dentro de OPTN/CATALOG; porcentajes manuales /100.",      L"M\u00E1xima causa de confusi\u00F3n al cambiar entre gris y blanca.", {}, {} },
        { L"Graficaci\u00F3n y G-Solv", L"Visualizaci\u00F3n", L"Sin gr\u00E1ficas nativas; Tabla + c\u00F3digo QR.",               L"Sin gr\u00E1ficas ni QR; Tabla de pares (X, f(X)).",     L"Gr\u00E1ficas a color 2D/3D, Zoom, Trace y G-Solv directo.", L"fx-CG50 resuelve intersecciones y ra\u00EDces sobre la curva.", {}, {} },
        { L"Resoluci\u00F3n LCD",    L"Hardware",         L"192 x 63 p\u00EDxeles monocromos de alta definici\u00F3n (4-6 l\u00EDneas).", L"96 x 31 p\u00EDxeles monocromos Natural-V.P.A.M.", L"384 x 216 p\u00EDxeles a color de 16 bits RGB565.",         L"ClassWiz duplica los puntos lineales de ES PLUS.", {}, {} }
    };
}

} // namespace detail

inline const KnowledgeCatalogData& GetCatalog() {
    static KnowledgeCatalogData s_cat = []() {
        KnowledgeCatalogData cat;
        detail::PopulateClassWizKeys(cat.keys);
        detail::PopulateEsPlusKeys(cat.keys);
        detail::PopulateCg50Keys(cat.keys);
        detail::PopulateIndicators(cat.indicators);
        detail::PopulateProcedures(cat.procedures);
        detail::PopulateDiagnosticsAndErrors(cat.diagnostics, cat.errors);
        detail::PopulateTipsAndProfiles(cat.tips, cat.setupProfiles, cat.checklist, cat.comparisons);
        return cat;
    }();
    return s_cat;
}

inline const CatalogKeyRecord* FindKeyRecord(int calcIdx, int keyId) {
    const auto& keys = GetCatalog().keys;
    for (const auto& k : keys) {
        if (k.calcIdx == calcIdx && k.keyId == keyId) return &k;
    }
    return nullptr;
}

inline const LcdIndicatorDoc* FindIndicatorDoc(int calcIdx, int spriteIndex) {
    const auto& inds = GetCatalog().indicators;
    for (const auto& ind : inds) {
        if (ind.calcIdx == calcIdx && ind.spriteIndex == spriteIndex) return &ind;
    }
    return nullptr;
}

// Dado un punto cliente sobre `indRect`, devuelve el sprite LCD bajo el puntero y si esta encendido
inline bool HitTestLcdIndicator(
    int calcIdx, int ptX, int ptY, const RECT& indRect,
    const unsigned char* rawBuf, size_t rawSize,
    int& outSpriteIndex, bool& outIsLit
) {
    outSpriteIndex = -1;
    outIsLit = false;
    if (calcIdx < 0 || calcIdx > 1) return false;
    if (ptX < indRect.left || ptX >= indRect.right || ptY < indRect.top || ptY >= indRect.bottom) {
        return false;
    }
    const int dstW = indRect.right - indRect.left;
    if (dstW <= 0) return false;
    const int stripW = CasioLcdIndicators::GetNativeStripWidth(calcIdx);
    const int count = CasioLcdIndicators::GetIndicatorCount(calcIdx);
    for (int i = 0; i < count; ++i) {
        const auto* sp = CasioLcdIndicators::GetIndicatorSpec(calcIdx, i);
        if (!sp) continue;
        int dx0 = indRect.left + MulDiv(sp->srcX, dstW, stripW);
        int dx1 = indRect.left + MulDiv(sp->srcX + sp->srcW, dstW, stripW);
        if (ptX >= dx0 && ptX <= (std::max)(dx0 + 2, dx1)) {
            outSpriteIndex = i;
            if (rawBuf && static_cast<size_t>(sp->rawByteOffset) < rawSize) {
                outIsLit = (rawBuf[sp->rawByteOffset] & sp->bitMask) != 0;
            }
            return true;
        }
    }
    return false;
}

// ----------------------------------------------------------------------------
// Favoritos Personales Persistentes (config_unificada.ini / favorites.ini)
// ----------------------------------------------------------------------------
inline std::wstring GetFavoritesIniPath() {
    std::wstring cfg = ResolveWritableConfigIniPath();
    size_t sl = cfg.find_last_of(L"\\/");
    std::wstring dir = (sl != std::wstring::npos) ? cfg.substr(0, sl) : GetUnifiedExecutableDirectory();
    return dir + L"\\casio_favoritos.ini";
}

inline std::vector<FavoriteItem> LoadFavoritesFromDisk() {
    std::vector<FavoriteItem> items;
    std::wstring ini = GetFavoritesIniPath();
    if (!std::filesystem::exists(std::filesystem::path(ini))) {
        // Dos favoritos utiles iniciales (deterministas, modificables/eliminables)
        FavoriteItem f1;
        f1.kind = L"proc"; f1.id = L"proc_def_integral";
        f1.title = L"Integral definida en [a, b]";
        f1.subtitle = L"C\u00E1lculo - ClassWiz / ES PLUS";
        f1.calcIdx = 0; f1.demoSeq = { 6, 40, 10, 11, 14, 43, 41, 11, 4, 45, 4, 42, 49 };
        f1.pdfFile = L"fx-570_991LA_X_ES.pdf"; f1.pdfPage = 17;
        items.push_back(f1);

        FavoriteItem f2;
        f2.kind = L"key"; f2.id = L"key_0_20";
        f2.title = L"Tecla x^-1 / x! / C (ClassWiz)";
        f2.subtitle = L"\u00C1lgebra - Inverso multiplicativo y factorial";
        f2.calcIdx = 0; f2.demoSeq = { 35, 20, 49 };
        f2.pdfFile = L"fx-570_991LA_X_ES.pdf"; f2.pdfPage = 16;
        items.push_back(f2);
        return items;
    }
    int count = static_cast<int>(GetPrivateProfileIntW(L"Favorites", L"Count", 0, ini.c_str()));
    for (int i = 0; i < count && i < 200; ++i) {
        wchar_t sec[32], buf[512];
        swprintf(sec, 32, L"Fav_%d", i);
        FavoriteItem f;
        GetPrivateProfileStringW(sec, L"Kind", L"", buf, 512, ini.c_str()); f.kind = buf;
        GetPrivateProfileStringW(sec, L"Id", L"", buf, 512, ini.c_str()); f.id = buf;
        GetPrivateProfileStringW(sec, L"Title", L"", buf, 512, ini.c_str()); f.title = buf;
        GetPrivateProfileStringW(sec, L"Sub", L"", buf, 512, ini.c_str()); f.subtitle = buf;
        f.calcIdx = static_cast<int>(GetPrivateProfileIntW(sec, L"Calc", 0, ini.c_str()));
        GetPrivateProfileStringW(sec, L"Pdf", L"", buf, 512, ini.c_str()); f.pdfFile = buf;
        f.pdfPage = static_cast<int>(GetPrivateProfileIntW(sec, L"Page", 1, ini.c_str()));
        GetPrivateProfileStringW(sec, L"Seq", L"", buf, 512, ini.c_str());
        std::wstringstream ss(buf);
        std::wstring tok;
        while (std::getline(ss, tok, L',')) {
            if (!tok.empty()) f.demoSeq.push_back(_wtoi(tok.c_str()));
        }
        if (!f.id.empty()) items.push_back(std::move(f));
    }
    return items;
}

inline bool SaveFavoritesToDisk(const std::vector<FavoriteItem>& items) {
    std::wstring ini = GetFavoritesIniPath();
    wchar_t cbuf[32];
    swprintf(cbuf, 32, L"%d", static_cast<int>(items.size()));
    if (!WritePrivateProfileStringW(L"Favorites", L"Count", cbuf, ini.c_str())) {
        return false;
    }
    for (size_t i = 0; i < items.size(); ++i) {
        const auto& f = items[i];
        wchar_t sec[32];
        swprintf(sec, 32, L"Fav_%zu", i);
        WritePrivateProfileStringW(sec, L"Kind", f.kind.c_str(), ini.c_str());
        WritePrivateProfileStringW(sec, L"Id", f.id.c_str(), ini.c_str());
        WritePrivateProfileStringW(sec, L"Title", f.title.c_str(), ini.c_str());
        WritePrivateProfileStringW(sec, L"Sub", f.subtitle.c_str(), ini.c_str());
        wchar_t val[32];
        swprintf(val, 32, L"%d", f.calcIdx);
        WritePrivateProfileStringW(sec, L"Calc", val, ini.c_str());
        WritePrivateProfileStringW(sec, L"Pdf", f.pdfFile.c_str(), ini.c_str());
        swprintf(val, 32, L"%d", f.pdfPage);
        WritePrivateProfileStringW(sec, L"Page", val, ini.c_str());
        std::wstring seq;
        for (size_t j = 0; j < f.demoSeq.size(); ++j) {
            if (j) seq += L",";
            seq += std::to_wstring(f.demoSeq[j]);
        }
        WritePrivateProfileStringW(sec, L"Seq", seq.c_str(), ini.c_str());
    }
    return true;
}

// ----------------------------------------------------------------------------
// Serializador JSON UTF-8 determinista (catalog/casio_knowledge_catalog.json)
// ----------------------------------------------------------------------------
namespace detail {
inline std::string WideToUtf8(const std::wstring& w) {
    if (w.empty()) return {};
    int len = WideCharToMultiByte(CP_UTF8, 0, w.data(), static_cast<int>(w.size()), nullptr, 0, nullptr, nullptr);
    if (len <= 0) return {};
    std::string s(len, '\0');
    WideCharToMultiByte(CP_UTF8, 0, w.data(), static_cast<int>(w.size()), s.data(), len, nullptr, nullptr);
    return s;
}

inline std::string JsonStr(const std::wstring& w) {
    std::string u = WideToUtf8(w);
    std::string o = "\"";
    for (unsigned char c : u) {
        if (c == '\"') o += "\\\"";
        else if (c == '\\') o += "\\\\";
        else if (c == '\b') o += "\\b";
        else if (c == '\f') o += "\\f";
        else if (c == '\n') o += "\\n";
        else if (c == '\r') o += "\\r";
        else if (c == '\t') o += "\\t";
        else if (c < 0x20) {
            char buf[8];
            snprintf(buf, sizeof(buf), "\\u%04x", c);
            o += buf;
        } else {
            o.push_back(static_cast<char>(c));
        }
    }
    o.push_back('\"');
    return o;
}

inline void WriteLayerJson(std::ostream& os, const char* name, const KeyLayerInfo& l, bool comma) {
    os << "      \"" << name << "\": {"
       << "\"available\": " << (l.available ? "true" : "false") << ", "
       << "\"label\": " << JsonStr(l.label) << ", "
       << "\"category\": " << JsonStr(l.category) << ", "
       << "\"summary\": " << JsonStr(l.summary) << ", "
       << "\"latexFormula\": " << JsonStr(l.latexFormula) << ", "
       << "\"latexExample\": " << JsonStr(l.latexExample) << ", "
       << "\"tipOrWarning\": " << JsonStr(l.tipOrWarning)
       << "}" << (comma ? ",\n" : "\n");
}

inline std::string IntArrayJson(const std::vector<int>& v) {
    std::string s = "[";
    for (size_t i = 0; i < v.size(); ++i) {
        if (i) s += ", ";
        s += std::to_string(v[i]);
    }
    s += "]";
    return s;
}
} // namespace detail

inline bool ExportCatalogToJson(const std::wstring& fullPath) {
    const auto& cat = GetCatalog();
    // Ensure parent dir exists
    size_t sl = fullPath.find_last_of(L"\\/");
    if (sl != std::wstring::npos) {
        CreateDirectoryW(fullPath.substr(0, sl).c_str(), nullptr);
    }
    std::ofstream os(std::filesystem::path(fullPath), std::ios::binary | std::ios::trunc);
    if (!os) return false;

    os << "{\n";
    os << "  \"schemaVersion\": \"1.0.0\",\n";
    os << "  \"deterministicOffline\": true,\n";
    os << "  \"counts\": {\n"
       << "    \"keys\": " << cat.keys.size() << ",\n"
       << "    \"indicators\": " << cat.indicators.size() << ",\n"
       << "    \"procedures\": " << cat.procedures.size() << ",\n"
       << "    \"diagnostics\": " << cat.diagnostics.size() << ",\n"
       << "    \"errors\": " << cat.errors.size() << ",\n"
       << "    \"tipsAndWarnings\": " << cat.tips.size() << ",\n"
       << "    \"setupProfiles\": " << cat.setupProfiles.size() << ",\n"
       << "    \"checklist\": " << cat.checklist.size() << ",\n"
       << "    \"comparisons\": " << cat.comparisons.size() << "\n"
       << "  },\n";

    os << "  \"keys\": [\n";
    for (size_t i = 0; i < cat.keys.size(); ++i) {
        const auto& k = cat.keys[i];
        os << "    {\n";
        os << "      \"calcIdx\": " << k.calcIdx << ", \"keyId\": " << k.keyId
           << ", \"keyName\": " << detail::JsonStr(k.keyName)
           << ", \"pdfFile\": " << detail::JsonStr(k.pdfFile)
           << ", \"pdfPage\": " << k.pdfPage
           << ", \"demoSeq\": " << detail::IntArrayJson(k.demoSeq) << ",\n";
        detail::WriteLayerJson(os, "normal", k.normal, true);
        detail::WriteLayerJson(os, "shift", k.shift, true);
        detail::WriteLayerJson(os, "alpha", k.alpha, false);
        os << "    }" << (i + 1 < cat.keys.size() ? ",\n" : "\n");
    }
    os << "  ],\n";

    os << "  \"indicators\": [\n";
    for (size_t i = 0; i < cat.indicators.size(); ++i) {
        const auto& ind = cat.indicators[i];
        os << "    {\"calcIdx\": " << ind.calcIdx << ", \"spriteIndex\": " << ind.spriteIndex
           << ", \"symbol\": " << detail::JsonStr(ind.symbol)
           << ", \"title\": " << detail::JsonStr(ind.title)
           << ", \"meaning\": " << detail::JsonStr(ind.meaning)
           << ", \"whenAppears\": " << detail::JsonStr(ind.whenAppears)
           << ", \"relatedKeys\": " << detail::JsonStr(ind.relatedKeys)
           << ", \"pdfFile\": " << detail::JsonStr(ind.pdfFile)
           << ", \"pdfPage\": " << ind.pdfPage << "}"
           << (i + 1 < cat.indicators.size() ? ",\n" : "\n");
    }
    os << "  ],\n";

    os << "  \"procedures\": [\n";
    for (size_t i = 0; i < cat.procedures.size(); ++i) {
        const auto& p = cat.procedures[i];
        os << "    {\"id\": " << detail::JsonStr(p.id)
           << ", \"category\": " << detail::JsonStr(p.category)
           << ", \"title\": " << detail::JsonStr(p.title)
           << ", \"goalSummary\": " << detail::JsonStr(p.goalSummary)
           << ", \"latexFormula\": " << detail::JsonStr(p.latexFormula)
           << ", \"commonErrors\": " << detail::JsonStr(p.commonErrors)
           << ", \"tags\": " << detail::JsonStr(p.tags) << ", \"models\": [";
        for (int m = 0; m < 3; ++m) {
            const auto& vm = p.perModel[m];
            os << "{\"calcIdx\": " << m << ", \"supported\": " << (vm.supported ? "true" : "false")
               << ", \"keys\": " << detail::IntArrayJson(vm.keys)
               << ", \"stepsSummary\": " << detail::JsonStr(vm.stepsSummary)
               << ", \"expectedLcd\": " << detail::JsonStr(vm.expectedLcd)
               << ", \"pdfFile\": " << detail::JsonStr(vm.pdfFile)
               << ", \"pdfPage\": " << vm.pdfPage << "}"
               << (m < 2 ? ", " : "");
        }
        os << "]}" << (i + 1 < cat.procedures.size() ? ",\n" : "\n");
    }
    os << "  ],\n";

    os << "  \"diagnostics\": [\n";
    for (size_t i = 0; i < cat.diagnostics.size(); ++i) {
        const auto& d = cat.diagnostics[i];
        os << "    {\"id\": " << detail::JsonStr(d.id)
           << ", \"symptom\": " << detail::JsonStr(d.symptomTitle)
           << ", \"category\": " << detail::JsonStr(d.category)
           << ", \"why\": " << detail::JsonStr(d.whyItHappens)
           << ", \"latex\": " << detail::JsonStr(d.latexExplanation)
           << ", \"fix\": " << detail::JsonStr(d.howToFix)
           << ", \"quickFixCalcIdx\": " << d.quickFixCalcIdx
           << ", \"quickFixSeq\": " << detail::IntArrayJson(d.quickFixSeq) << "}"
           << (i + 1 < cat.diagnostics.size() ? ",\n" : "\n");
    }
    os << "  ],\n";

    os << "  \"errors\": [\n";
    for (size_t i = 0; i < cat.errors.size(); ++i) {
        const auto& e = cat.errors[i];
        os << "    {\"name\": " << detail::JsonStr(e.errorName)
           << ", \"appliesTo\": " << detail::JsonStr(e.appliesTo)
           << ", \"cause\": " << detail::JsonStr(e.cause)
           << ", \"resolution\": " << detail::JsonStr(e.resolution)
           << ", \"pdfFile\": " << detail::JsonStr(e.pdfFile)
           << ", \"pdfPage\": " << e.pdfPage << "}"
           << (i + 1 < cat.errors.size() ? ",\n" : "\n");
    }
    os << "  ]\n";
    os << "}\n";
    return os.good();
}

} // namespace CasioKnowledge
