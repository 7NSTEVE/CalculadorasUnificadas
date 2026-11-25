#include "study_assistant_tools.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <ctime>
#include <regex>
#include <set>
#include <sstream>

namespace DocStudio {

namespace {

std::string Trim(const std::string& s) {
    std::size_t a = 0;
    while (a < s.size() && std::isspace(static_cast<unsigned char>(s[a]))) ++a;
    std::size_t b = s.size();
    while (b > a && std::isspace(static_cast<unsigned char>(s[b - 1]))) --b;
    return s.substr(a, b - a);
}

std::string ToLowerAscii(std::string s) {
    for (char& ch : s) {
        if (ch >= 'A' && ch <= 'Z') ch = static_cast<char>(ch - 'A' + 'a');
    }
    return s;
}

std::string CollectDocumentContextSnippet(const DocumentModel& doc, std::size_t maxChars = 4200) {
    std::ostringstream oss;
    auto pushLine = [&](const std::string& s) {
        if (s.empty()) return;
        if (oss.str().size() >= maxChars) return;
        oss << s << "\n";
    };
    for (std::size_t p = 0; p < doc.Pages().size(); ++p) {
        const auto& pg = doc.Pages()[p];
        if (!pg.extractedTextUtf8.empty()) pushLine("[Pag." + std::to_string(p + 1) + "] " + pg.extractedTextUtf8.substr(0, 480));
        if (!pg.ocrTextUtf8.empty()) pushLine("[OCR Pag." + std::to_string(p + 1) + "] " + pg.ocrTextUtf8.substr(0, 480));
        for (const auto& o : pg.objects) {
            if (!o.formulaExprUtf8.empty()) {
                pushLine("Formula: " + o.formulaExprUtf8 + (o.evaluatedResultUtf8.empty() ? "" : (" = " + o.evaluatedResultUtf8)));
            } else if (!o.textUtf8.empty()) {
                pushLine(o.textUtf8.substr(0, 420));
            } else if (!o.htrRecognizedTextUtf8.empty()) {
                pushLine(o.htrRecognizedTextUtf8.substr(0, 320));
            }
        }
    }
    for (const auto& o : doc.CanvasObjects()) {
        if (!o.formulaExprUtf8.empty()) {
            pushLine("Formula: " + o.formulaExprUtf8 + (o.evaluatedResultUtf8.empty() ? "" : (" = " + o.evaluatedResultUtf8)));
        } else if (!o.textUtf8.empty()) {
            pushLine(o.textUtf8.substr(0, 420));
        }
    }
    std::string out = oss.str();
    if (out.size() > maxChars) out.resize(maxChars);
    return out;
}

std::string ExtractIsoDateFromLine(const std::string& line) {
    try {
        // YYYY-MM-DD
        static const std::regex kIsoRe(R"((\d{4})[-/](\d{1,2})[-/](\d{1,2}))");
        std::smatch m;
        if (std::regex_search(line, m, kIsoRe)) {
            const int y = std::atoi(m[1].str().c_str());
            const int mo = std::atoi(m[2].str().c_str());
            const int d = std::atoi(m[3].str().c_str());
            if (y >= 2000 && y <= 2100 && mo >= 1 && mo <= 12 && d >= 1 && d <= 31) {
                char buf[16]{};
                std::snprintf(buf, sizeof(buf), "%04d-%02d-%02d", y, mo, d);
                return buf;
            }
        }
        // DD/MM/YYYY or DD-MM-YYYY
        static const std::regex kEsRe(R"((\d{1,2})[/-](\d{1,2})[/-](\d{4}))");
        if (std::regex_search(line, m, kEsRe)) {
            const int d = std::atoi(m[1].str().c_str());
            const int mo = std::atoi(m[2].str().c_str());
            const int y = std::atoi(m[3].str().c_str());
            if (y >= 2000 && y <= 2100 && mo >= 1 && mo <= 12 && d >= 1 && d <= 31) {
                char buf[16]{};
                std::snprintf(buf, sizeof(buf), "%04d-%02d-%02d", y, mo, d);
                return buf;
            }
        }
    } catch (...) {}
    return {};
}

std::string SanitizeIcsText(const std::string& raw) {
    std::string out;
    out.reserve(raw.size());
    for (char ch : raw) {
        if (ch == '\r') continue;
        if (ch == '\n') out += "\\n";
        else if (ch == ',' || ch == ';' || ch == '\\') { out.push_back('\\'); out.push_back(ch); }
        else out.push_back(ch);
    }
    return out;
}

std::string MakeBibKey(const std::string& authors, const std::string& year, const std::string& title) {
    std::string key;
    for (char ch : authors) {
        if (std::isalpha(static_cast<unsigned char>(ch))) {
            key.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));
            if (key.size() >= 8) break;
        } else if (!key.empty()) {
            break;
        }
    }
    if (key.empty()) key = "ref";
    for (char ch : year) {
        if (std::isdigit(static_cast<unsigned char>(ch))) key.push_back(ch);
    }
    std::string word;
    for (char ch : title) {
        if (std::isalpha(static_cast<unsigned char>(ch))) {
            word.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));
            if (word.size() >= 6) break;
        } else if (word.size() >= 3) {
            break;
        } else {
            word.clear();
        }
    }
    if (!word.empty()) key += word;
    return key;
}

} // namespace

// ---------------- PomodoroTimerState ----------------

void PomodoroTimerState::Configure(int focusMin, int breakMin, bool autoFocus) noexcept {
    focusMinutes = std::clamp(focusMin, 1, 180);
    breakMinutes = std::clamp(breakMin, 1, 60);
    autoEnableFocusMode = autoFocus;
    inBreakPhase = false;
    running = false;
    paused = false;
    remainingSeconds = focusMinutes * 60;
    lastTickMs = 0;
}

void PomodoroTimerState::Start(std::uint64_t nowTickMs) noexcept {
    if (!running) {
        running = true;
        paused = false;
        if (remainingSeconds <= 0) {
            remainingSeconds = (inBreakPhase ? breakMinutes : focusMinutes) * 60;
        }
        lastTickMs = nowTickMs;
    } else if (paused) {
        paused = false;
        lastTickMs = nowTickMs;
    }
}

void PomodoroTimerState::TogglePause(std::uint64_t nowTickMs) noexcept {
    if (!running) {
        Start(nowTickMs);
        return;
    }
    paused = !paused;
    lastTickMs = nowTickMs;
}

void PomodoroTimerState::Reset() noexcept {
    running = false;
    paused = false;
    inBreakPhase = false;
    remainingSeconds = focusMinutes * 60;
    lastTickMs = 0;
}

bool PomodoroTimerState::Tick(std::uint64_t nowTickMs, std::uint64_t& outAddedFocusSec, bool* outPhaseTransitioned) noexcept {
    outAddedFocusSec = 0;
    if (outPhaseTransitioned) *outPhaseTransitioned = false;
    if (!running || paused) return false;
    if (lastTickMs == 0 || nowTickMs < lastTickMs) {
        lastTickMs = nowTickMs;
        return false;
    }
    const std::uint64_t deltaSec = (nowTickMs - lastTickMs) / 1000ull;
    if (deltaSec == 0) return false;
    lastTickMs += deltaSec * 1000ull;

    const int step = static_cast<int>((std::min<std::uint64_t>)(deltaSec, 3600ull));
    if (!inBreakPhase) {
        const int focusSpent = (std::min)(remainingSeconds, step);
        outAddedFocusSec = static_cast<std::uint64_t>((std::max)(0, focusSpent));
    }
    remainingSeconds -= step;
    if (remainingSeconds <= 0) {
        if (!inBreakPhase) {
            completedCycles++;
            inBreakPhase = true;
            remainingSeconds = breakMinutes * 60;
        } else {
            inBreakPhase = false;
            remainingSeconds = focusMinutes * 60;
        }
        if (outPhaseTransitioned) *outPhaseTransitioned = true;
    }
    return true;
}

std::string PomodoroTimerState::FormatHudBadgeUtf8() const {
    const int rem = (std::max)(0, remainingSeconds);
    const int mm = rem / 60;
    const int ss = rem % 60;
    char buf[64]{};
    std::snprintf(buf, sizeof(buf), "%s %02d:%02d%s",
        inBreakPhase ? "Pausa" : "Foco",
        mm, ss,
        paused ? " (P)" : "");
    return buf;
}

// ---------------- 1. Guided Step-by-Step Tutor ----------------

GuidedTutorResult StudyAssistantTools::RunGuidedTutor(
    const DocumentModel& doc,
    const std::string& problemOrTopicUtf8,
    TutorHintLevel level,
    bool socraticNoSpoiler,
    const AiProviderVault* optionalVault)
{
    GuidedTutorResult res{};
    res.level = level;
    const std::string topic = Trim(problemOrTopicUtf8);
    if (topic.empty()) return res;

    const std::string ctx = CollectDocumentContextSnippet(doc, 2600);
    std::string levelInstruction;
    if (level == TutorHintLevel::ConceptualHint) {
        levelInstruction = socraticNoSpoiler
            ? "NIVEL 1 (Pista Conceptual Socratica): NO entregues la respuesta final ni el despeje completo. Haz 2 preguntas guias y menciona la definicion o principio relevante."
            : "NIVEL 1 (Pista Conceptual): Identifica los conceptos y formulas clave aplicables y orienta el primer paso.";
    } else if (level == TutorHintLevel::StepByStepPlan) {
        levelInstruction = socraticNoSpoiler
            ? "NIVEL 2 (Planteamiento Paso a Paso): Estructura los pasos numerados 1, 2, 3 dejando el calculo final para que quien estudia complete el cierre."
            : "NIVEL 2 (Planteamiento Paso a Paso): Desarrolla el procedimiento paso a paso con justificacion clara de cada transformacion.";
    } else {
        levelInstruction = "NIVEL 3 (Verificacion Completa): Presenta el desarrollo paso a paso, resultado verificado, unidades y un control de comprobacion.";
    }

    if (optionalVault && optionalVault->HasConfiguredCloudOrLocalProvider()) {
        const std::string sys =
            "Eres un tutor de estudio riguroso en espanol. Usa lenguaje claro, estructurado y neutro. "
            + levelInstruction;
        const std::string usr =
            "Contexto del cuaderno:\n" + (ctx.empty() ? "(Cuaderno vacio)" : ctx) +
            "\n\nProblema o tema a estudiar:\n" + topic;
        const AiQueryResult qr = optionalVault->QueryWithFailover(sys, usr);
        if (qr.ok && !qr.replyTextUtf8.empty()) {
            res.ok = true;
            res.usedLiveAi = true;
            res.providerUsedUtf8 = qr.providerNameUsedUtf8 + " (" + qr.modelUsedUtf8 + ")";
            res.latencyMs = qr.latencyMs;
            res.responseMarkdownUtf8 = qr.replyTextUtf8;
            return res;
        }
    }

    // Deterministic offline analytical fallback (grounded in document + symbolic heuristics)
    res.ok = true;
    res.usedLiveAi = false;
    res.providerUsedUtf8 = "Motor Local (Sin Red)";

    const std::string low = ToLowerAscii(topic);
    const bool looksEquation = (topic.find('=') != std::string::npos);
    const bool looksCalculus = (low.find("deriv") != std::string::npos || low.find("integ") != std::string::npos || low.find("lim") != std::string::npos);
    const bool looksCircuit = (low.find("ohm") != std::string::npos || low.find("kirchhoff") != std::string::npos || low.find("imped") != std::string::npos || low.find("circuit") != std::string::npos);

    const auto grounded = GroundedStudyAiAssistant::AnswerQuestionWithCitations(doc, {}, topic, nullptr, nullptr);

    std::ostringstream oss;
    oss << "## Tutor Paso a Paso — ";
    if (level == TutorHintLevel::ConceptualHint) oss << "Nivel 1: Pista Conceptual\n\n";
    else if (level == TutorHintLevel::StepByStepPlan) oss << "Nivel 2: Planteamiento Estructurado\n\n";
    else oss << "Nivel 3: Verificaci\xC3\xB3n y Comprobaci\xC3\xB3n\n\n";

    oss << "Tema analizado: " << topic << "\n\n";

    if (level == TutorHintLevel::ConceptualHint) {
        oss << "1. Identifica los datos conocidos, las inc\xC3\xB3gnitas y las unidades involucradas.\n";
        if (looksEquation) oss << "2. Observa la igualdad principal: \xC2\xBFqu\xC3\xA9 operaci\xC3\xB3n inversa permite aislar la variable objetivo?\n";
        else if (looksCalculus) oss << "2. Revisa si corresponde regla de la cadena, sustituci\xC3\xB3n o integraci\xC3\xB3n por partes antes de evaluar l\xC3\xADmites.\n";
        else if (looksCircuit) oss << "2. Define nodo de referencia (0 V), polaridades de corriente y aplica LKC/LKV seg\xC3\xBAn convenga.\n";
        else oss << "2. Relaciona el concepto con las definiciones previas registradas en tu cuaderno.\n";
    } else if (level == TutorHintLevel::StepByStepPlan) {
        oss << "Paso 1 — Modelo: Escribe las variables y restricciones de dominio/unidades.\n";
        oss << "Paso 2 — Ecuaci\xC3\xB3n de gobierno: Plantea la relaci\xC3\xB3n simb\xC3\xB3lica antes de sustituir n\xC3\xBAmeros.\n";
        oss << "Paso 3 — Transformaci\xC3\xB3n: Simplifica t\xC3\xA9rmino a t\xC3\xA9rmino verificando signos y dimensiones.\n";
        if (socraticNoSpoiler) {
            oss << "Paso 4 — Tu turno: Completa la evaluaci\xC3\xB3n num\xC3\xA9rica y comprueba si el orden de magnitud tiene sentido f\xC3\xADsico.\n";
        } else {
            oss << "Paso 4 — Cierre: Eval\xC3\xBA""a el resultado y contr\xC3\xA1stalo con un caso l\xC3\xADmite conocido.\n";
        }
    } else {
        oss << "1. Planteamiento simb\xC3\xB3lico: Separa variables dependientes e independientes.\n";
        oss << "2. Resoluci\xC3\xB3n anal\xC3\xADtica: Aplica despeje algebraico o m\xC3\xA9todo num\xC3\xA9rico con el bloque CAS (Ctrl+M).\n";
        oss << "3. Comprobaci\xC3\xB3n cruzada: Sustituye la soluci\xC3\xB3n obtenida de vuelta en la expresi\xC3\xB3n original para verificar residuo cero.\n";
        oss << "4. An\xC3\xA1lisis dimensional: Confirma coherencia en unidades SI.\n";
    }

    if (grounded.answered && !grounded.citations.empty()) {
        oss << "\nReferencias encontradas en tu cuaderno:\n";
        for (std::size_t i = 0; i < (std::min<std::size_t>)(grounded.citations.size(), 3u); ++i) {
            const auto& c = grounded.citations[i];
            oss << "- [P\xC3\xA1g. " << (c.pageIndex0 + 1) << "] " << c.quoteSnippetUtf8 << "\n";
        }
    }
    res.responseMarkdownUtf8 = oss.str();
    return res;
}

// ---------------- 2. Smart Flashcards & Practice StudyTapes ----------------

std::vector<FlashcardItem> StudyAssistantTools::GenerateSmartFlashcards(
    const DocumentModel& doc,
    std::size_t maxCards,
    const AiProviderVault* optionalVault,
    bool* outUsedLiveAi)
{
    if (outUsedLiveAi) *outUsedLiveAi = false;
    std::vector<FlashcardItem> cards;
    const std::size_t limit = std::clamp<std::size_t>(maxCards, 1u, 24u);

    const std::string ctx = CollectDocumentContextSnippet(doc, 3200);
    if (optionalVault && optionalVault->HasConfiguredCloudOrLocalProvider() && !ctx.empty()) {
        const std::string sys =
            "Genera tarjetas de estudio de repaso espaciado en espanol a partir de los apuntes. "
            "Formato ESTRICTO: una tarjeta por linea exactamente como 'P: <pregunta> | R: <respuesta concisa>'. "
            "Sin numeracion ni texto extra.";
        const std::string usr = "Genera maximo " + std::to_string(limit) + " tarjetas de:\n" + ctx;
        const AiQueryResult qr = optionalVault->QueryWithFailover(sys, usr);
        if (qr.ok && !qr.replyTextUtf8.empty()) {
            std::istringstream iss(qr.replyTextUtf8);
            std::string ln;
            std::uint64_t seq = 1;
            while (std::getline(iss, ln) && cards.size() < limit) {
                ln = Trim(ln);
                const auto sep = ln.find('|');
                if (sep == std::string::npos) continue;
                std::string q = Trim(ln.substr(0, sep));
                std::string a = Trim(ln.substr(sep + 1));
                if (q.rfind("P:", 0) == 0 || q.rfind("p:", 0) == 0) q = Trim(q.substr(2));
                if (q.rfind("Q:", 0) == 0 || q.rfind("q:", 0) == 0) q = Trim(q.substr(2));
                if (a.rfind("R:", 0) == 0 || a.rfind("r:", 0) == 0) a = Trim(a.substr(2));
                if (a.rfind("A:", 0) == 0 || a.rfind("a:", 0) == 0) a = Trim(a.substr(2));
                if (!q.empty() && !a.empty()) {
                    FlashcardItem fc{};
                    fc.id = seq++;
                    fc.questionUtf8 = q;
                    fc.answerUtf8 = a;
                    fc.easeFactor = 2.5;
                    cards.push_back(std::move(fc));
                }
            }
            if (!cards.empty()) {
                if (outUsedLiveAi) *outUsedLiveAi = true;
                return cards;
            }
        }
    }

    // Offline harvesting: StudySetManager + Term:Definition + MathBlock fallback
    StudyDeck base = StudySetManager::BuildDeckFromDocument(doc, doc.Title());
    std::set<std::string> seenQ;
    for (auto& c : base.cards) {
        if (cards.size() >= limit) break;
        if (seenQ.insert(c.questionUtf8).second) cards.push_back(std::move(c));
    }

    auto scanObjects = [&](const std::vector<AnnotationObject>& objs, std::size_t pgIdx) {
        for (const auto& o : objs) {
            if (cards.size() >= limit) break;
            if (!o.formulaExprUtf8.empty()) {
                const std::string q = "\xC2\xBF" "Cu\xC3\xA1l es el resultado o expresi\xC3\xB3n de " + o.formulaExprUtf8 + "?";
                const std::string a = o.evaluatedResultUtf8.empty() ? o.formulaExprUtf8 : o.evaluatedResultUtf8;
                if (seenQ.insert(q).second) {
                    FlashcardItem fc{};
                    fc.id = o.id ? o.id : (cards.size() + 1);
                    fc.sourcePageIndex = pgIdx;
                    fc.questionUtf8 = q;
                    fc.answerUtf8 = a;
                    cards.push_back(std::move(fc));
                }
            } else if (!o.textUtf8.empty()) {
                std::istringstream iss(o.textUtf8);
                std::string ln;
                while (std::getline(iss, ln) && cards.size() < limit) {
                    ln = Trim(ln);
                    if (ln.size() < 12) continue;
                    const auto colon = ln.find(':');
                    if (colon != std::string::npos && colon >= 3 && colon <= 56 && colon + 2 < ln.size()) {
                        const std::string term = Trim(ln.substr(0, colon));
                        const std::string def = Trim(ln.substr(colon + 1));
                        if (!term.empty() && def.size() >= 4) {
                            const std::string q = "Define o explica: " + term;
                            if (seenQ.insert(q).second) {
                                FlashcardItem fc{};
                                fc.id = o.id ? o.id : (cards.size() + 1);
                                fc.sourcePageIndex = pgIdx;
                                fc.questionUtf8 = q;
                                fc.answerUtf8 = def;
                                cards.push_back(std::move(fc));
                            }
                        }
                    }
                }
            }
        }
    };

    for (std::size_t p = 0; p < doc.Pages().size() && cards.size() < limit; ++p) {
        scanObjects(doc.Pages()[p].objects, p);
    }
    if (cards.size() < limit) scanObjects(doc.CanvasObjects(), 0);
    return cards;
}

std::size_t StudyAssistantTools::InsertPracticeStudyTapes(
    DocumentModel& doc,
    CommandStack& stack,
    ObjectTarget target,
    const std::vector<FlashcardItem>& cards,
    double startX,
    double startY)
{
    if (cards.empty()) return 0;
    std::vector<AnnotationObject> batch;
    batch.reserve(cards.size());
    const std::uint64_t layerId = doc.ActiveLayerId();
    for (std::size_t i = 0; i < cards.size(); ++i) {
        AnnotationObject tape{};
        tape.kind = ObjectKind::StudyTape;
        tape.layerId = layerId;
        const double colX = startX + static_cast<double>(i % 2) * 340.0;
        const double rowY = startY + static_cast<double>(i / 2) * 106.0;
        tape.bounds = { colX, rowY, 316.0, 88.0 };
        tape.captionUtf8 = cards[i].questionUtf8;
        tape.textUtf8 = cards[i].answerUtf8;
        tape.tapeRevealed = false;
        tape.strokeColor = DocColor::RGBA(245, 158, 11, 255);
        tape.fillColor = DocColor::RGBA(254, 243, 199, 245);
        batch.push_back(std::move(tape));
    }
    if (stack.Execute(doc, std::make_unique<BatchAddObjectsCommand>(target, std::move(batch), "Insertar ejercicios de pr\xC3\xA1""ctica"))) {
        return cards.size();
    }
    return 0;
}

// ---------------- 3. Concept Map Generator ----------------

ConceptMapBuildResult StudyAssistantTools::GenerateConceptMapInDocument(
    DocumentModel& doc,
    CommandStack& stack,
    ObjectTarget target,
    const std::string& centralTopicUtf8,
    const AiProviderVault* optionalVault,
    double originX,
    double originY)
{
    ConceptMapBuildResult res{};
    const std::string rootTopic = Trim(centralTopicUtf8).empty()
        ? (doc.Title().empty() ? "Mapa Conceptual" : doc.Title())
        : Trim(centralTopicUtf8);

    std::vector<ConceptMapBranch> branches;
    const std::string ctx = CollectDocumentContextSnippet(doc, 2400);

    // 1. Try AI Multi-Provider structured branch synthesis
    if (optionalVault && optionalVault->HasConfiguredCloudOrLocalProvider()) {
        const std::string sys =
            "Genera un mapa conceptual jerarquico conciso en espanol. "
            "Formato ESTRICTO: 3 a 5 lineas, cada una exactamente 'RAMA: <Concepto> | <Sub1>; <Sub2>'.";
        const std::string usr = "Tema central: " + rootTopic + (ctx.empty() ? "" : ("\nApuntes:\n" + ctx));
        const AiQueryResult qr = optionalVault->QueryWithFailover(sys, usr);
        if (qr.ok && !qr.replyTextUtf8.empty()) {
            std::istringstream iss(qr.replyTextUtf8);
            std::string ln;
            while (std::getline(iss, ln) && branches.size() < 5) {
                ln = Trim(ln);
                const auto p = ln.find("RAMA:");
                if (p != std::string::npos) ln = Trim(ln.substr(p + 5));
                else if (ln.rfind("- ", 0) == 0) ln = Trim(ln.substr(2));
                const auto pipe = ln.find('|');
                ConceptMapBranch b{};
                if (pipe != std::string::npos) {
                    b.titleUtf8 = Trim(ln.substr(0, pipe));
                    std::istringstream subs(ln.substr(pipe + 1));
                    std::string sub;
                    while (std::getline(subs, sub, ';') && b.subItemsUtf8.size() < 3) {
                        sub = Trim(sub);
                        if (!sub.empty()) b.subItemsUtf8.push_back(sub);
                    }
                } else if (ln.size() >= 3) {
                    b.titleUtf8 = ln;
                }
                if (!b.titleUtf8.empty()) branches.push_back(std::move(b));
            }
            if (!branches.empty()) res.usedLiveAi = true;
        }
    }

    // 2. Offline heuristic branch extraction from document headings / concepts / formulas
    if (branches.empty()) {
        std::vector<std::string> formulas;
        std::vector<std::string> definitions;
        std::vector<std::string> notes;
        auto harvestObj = [&](const AnnotationObject& o) {
            if (!o.formulaExprUtf8.empty() && formulas.size() < 3) {
                formulas.push_back(o.formulaExprUtf8);
            } else if (!o.textUtf8.empty()) {
                std::istringstream iss(o.textUtf8);
                std::string ln;
                while (std::getline(iss, ln)) {
                    ln = Trim(ln);
                    if (ln.empty()) continue;
                    if (ln[0] == '#') {
                        while (!ln.empty() && (ln[0] == '#' || ln[0] == ' ')) ln.erase(ln.begin());
                        if (!ln.empty() && notes.size() < 4) notes.push_back(ln.substr(0, 48));
                    } else if (ln.find(':') != std::string::npos && definitions.size() < 3) {
                        definitions.push_back(ln.substr(0, 52));
                    } else if (notes.size() < 4) {
                        notes.push_back(ln.substr(0, 48));
                    }
                }
            }
        };
        for (const auto& pg : doc.Pages()) for (const auto& o : pg.objects) harvestObj(o);
        for (const auto& o : doc.CanvasObjects()) harvestObj(o);

        if (!definitions.empty()) branches.push_back({ "Definiciones Clave", definitions });
        if (!formulas.empty())    branches.push_back({ "F\xC3\xB3rmulas y Modelos", formulas });
        if (!notes.empty())       branches.push_back({ "Ideas Principales", notes });
        if (branches.empty()) {
            branches = {
                { "Conceptos Base",   { "Definici\xC3\xB3n central", "Propiedades" } },
                { "Relaciones",       { "Causas y efectos", "Casos l\xC3\xADmite" } },
                { "Aplicaciones",     { "Ejemplos pr\xC3\xA1""cticos", "Verificaci\xC3\xB3n" } }
            };
        }
    }

    // Build styled TextBox hierarchy + magnetic ShapeKind::Connector arrows atomically
    std::vector<AnnotationObject> batch;
    const std::uint64_t layerId = doc.ActiveLayerId();

    const std::uint64_t rootId = doc.AllocateObjectId();
    AnnotationObject rootNode{};
    rootNode.id = rootId;
    rootNode.kind = ObjectKind::TextBox;
    rootNode.layerId = layerId;
    rootNode.bounds = { originX, originY + 120.0, 210.0, 62.0 };
    rootNode.textUtf8 = rootTopic;
    rootNode.bold = true;
    rootNode.fontSizePt = 14.0;
    rootNode.strokeColor = DocColor::RGBA(255, 255, 255, 255);
    rootNode.fillColor = DocColor::RGBA(0, 122, 255, 245);
    batch.push_back(rootNode);
    res.nodeCount++;

    static const DocColor kBranchFills[5] = {
        DocColor::RGBA(232, 244, 255, 248),
        DocColor::RGBA(233, 249, 240, 248),
        DocColor::RGBA(254, 244, 224, 248),
        DocColor::RGBA(243, 236, 254, 248),
        DocColor::RGBA(253, 236, 240, 248)
    };
    static const DocColor kBranchInks[5] = {
        DocColor::RGBA(15, 76, 156, 255),
        DocColor::RGBA(14, 110, 74, 255),
        DocColor::RGBA(154, 82, 12, 255),
        DocColor::RGBA(100, 44, 168, 255),
        DocColor::RGBA(168, 34, 72, 255)
    };

    const double branchX = originX + 280.0;
    const double leafX = originX + 545.0;
    double curY = originY;

    for (std::size_t bi = 0; bi < branches.size(); ++bi) {
        const auto& br = branches[bi];
        const std::size_t leafRows = (std::max<std::size_t>)(1u, br.subItemsUtf8.size());
        const double blockH = static_cast<double>(leafRows) * 56.0;
        const double branchY = curY + (blockH - 48.0) * 0.5;

        const std::uint64_t bId = doc.AllocateObjectId();
        AnnotationObject bNode{};
        bNode.id = bId;
        bNode.kind = ObjectKind::TextBox;
        bNode.layerId = layerId;
        bNode.bounds = { branchX, branchY, 200.0, 48.0 };
        bNode.textUtf8 = br.titleUtf8;
        bNode.bold = true;
        bNode.fontSizePt = 12.0;
        bNode.fillColor = kBranchFills[bi % 5];
        bNode.strokeColor = kBranchInks[bi % 5];
        batch.push_back(bNode);
        res.nodeCount++;

        AnnotationObject connRoot{};
        connRoot.id = doc.AllocateObjectId();
        connRoot.kind = ObjectKind::Shape;
        connRoot.shapeKind = ShapeKind::Connector;
        connRoot.layerId = layerId;
        connRoot.connectorFromId = rootId;
        connRoot.connectorToId = bId;
        connRoot.shapeStart = { rootNode.bounds.Right(), rootNode.bounds.y + rootNode.bounds.h * 0.5 };
        connRoot.shapeEnd = { bNode.bounds.x, bNode.bounds.y + bNode.bounds.h * 0.5 };
        connRoot.strokeColor = kBranchInks[bi % 5];
        connRoot.strokeWidth = 2.0;
        connRoot.RecomputeBounds();
        batch.push_back(connRoot);
        res.connectorCount++;

        for (std::size_t li = 0; li < br.subItemsUtf8.size(); ++li) {
            const std::uint64_t lId = doc.AllocateObjectId();
            AnnotationObject lNode{};
            lNode.id = lId;
            lNode.kind = ObjectKind::TextBox;
            lNode.layerId = layerId;
            lNode.bounds = { leafX, curY + static_cast<double>(li) * 56.0, 210.0, 44.0 };
            lNode.textUtf8 = br.subItemsUtf8[li];
            lNode.fontSizePt = 11.0;
            lNode.fillColor = DocColor::RGBA(255, 255, 255, 250);
            lNode.strokeColor = DocColor::RGBA(30, 41, 59, 255);
            batch.push_back(lNode);
            res.nodeCount++;

            AnnotationObject connLeaf{};
            connLeaf.id = doc.AllocateObjectId();
            connLeaf.kind = ObjectKind::Shape;
            connLeaf.shapeKind = ShapeKind::Connector;
            connLeaf.layerId = layerId;
            connLeaf.connectorFromId = bId;
            connLeaf.connectorToId = lId;
            connLeaf.shapeStart = { bNode.bounds.Right(), bNode.bounds.y + bNode.bounds.h * 0.5 };
            connLeaf.shapeEnd = { lNode.bounds.x, lNode.bounds.y + lNode.bounds.h * 0.5 };
            connLeaf.strokeColor = DocColor::RGBA(100, 116, 139, 230);
            connLeaf.strokeWidth = 1.6;
            connLeaf.RecomputeBounds();
            batch.push_back(connLeaf);
            res.connectorCount++;
        }
        curY += blockH + 22.0;
    }

    res.ok = stack.Execute(doc, std::make_unique<BatchAddObjectsCommand>(target, std::move(batch), "Generar mapa conceptual"));
    res.summaryUtf8 = "Mapa conceptual: " + std::to_string(res.nodeCount) + " nodos y " +
                      std::to_string(res.connectorCount) + " conectores" +
                      (res.usedLiveAi ? " (IA)" : " (Local)");
    return res;
}

// ---------------- 4. Formula Sheet & Glossary Builder ----------------

FormulaSheetGlossaryResult StudyAssistantTools::BuildFormulaSheetAndGlossary(
    const DocumentModel& doc,
    const KnowledgeVault* optionalVault)
{
    FormulaSheetGlossaryResult res{};
    std::vector<std::string> formulas;
    std::vector<std::pair<std::string, std::string>> glossary;
    std::vector<std::string> codeNotes;
    std::set<std::string> seenFormula;
    std::set<std::string> seenTerm;

    auto scanText = [&](const std::string& text) {
        std::istringstream iss(text);
        std::string ln;
        while (std::getline(iss, ln)) {
            ln = Trim(ln);
            if (ln.size() < 5) continue;
            // Chemical reaction or algebraic equation
            if ((ln.find("->") != std::string::npos || (ln.find('=') != std::string::npos && ln.size() <= 72)) &&
                ln.find("http") == std::string::npos && ln.find("- [") == std::string::npos) {
                if (seenFormula.insert(ln).second) formulas.push_back(ln);
                continue;
            }
            const auto colon = ln.find(':');
            if (colon != std::string::npos && colon >= 3 && colon <= 44 && colon + 3 < ln.size()) {
                std::string term = Trim(ln.substr(0, colon));
                const std::string lowT = ToLowerAscii(term);
                if (lowT == "todo" || lowT == "tarea" || lowT == "entrega" || lowT == "examen" || lowT == "http" || lowT == "https") {
                    continue;
                }
                while (!term.empty() && (term.front() == '-' || term.front() == '*' || term.front() == ' ')) term.erase(term.begin());
                while (!term.empty() && term.back() == '*') term.pop_back();
                const std::string def = Trim(ln.substr(colon + 1));
                if (term.size() >= 2 && def.size() >= 5 && seenTerm.insert(ToLowerAscii(term)).second) {
                    glossary.emplace_back(term, def);
                }
            }
        }
    };

    auto scanObjects = [&](const std::vector<AnnotationObject>& objs) {
        for (const auto& o : objs) {
            if (!o.formulaExprUtf8.empty()) {
                std::string f = o.formulaExprUtf8;
                if (!o.evaluatedResultUtf8.empty()) f += "  =  " + o.evaluatedResultUtf8;
                if (seenFormula.insert(f).second) formulas.push_back(f);
            } else if (o.kind == ObjectKind::CodeBlock && !o.textUtf8.empty()) {
                const std::string hdr = o.captionUtf8.empty() ? ("[" + o.codeLanguageUtf8 + "]") : o.captionUtf8;
                codeNotes.push_back(hdr + " " + o.textUtf8.substr(0, 96));
            } else if (!o.textUtf8.empty()) {
                scanText(o.textUtf8);
            }
        }
    };

    for (const auto& pg : doc.Pages()) scanObjects(pg.objects);
    scanObjects(doc.CanvasObjects());
    if (optionalVault) {
        for (const auto& n : optionalVault->ActiveNotes()) {
            scanText(n.bodyMarkdownUtf8);
        }
    }

    res.formulaCount = formulas.size();
    res.glossaryCount = glossary.size();
    res.codeSnippetCount = codeNotes.size();

    std::ostringstream oss;
    oss << "# Hoja de F\xC3\xB3rmulas y Glosario \xE2\x80\x94 " << (doc.Title().empty() ? "Cuaderno" : doc.Title()) << "\n\n";
    oss << "## F\xC3\xB3rmulas y Ecuaciones (" << formulas.size() << ")\n";
    if (formulas.empty()) {
        oss << "- (No se detectaron bloques de f\xC3\xB3rmulas todav\xC3\xAD""a)\n";
    } else {
        for (const auto& f : formulas) oss << "- " << f << "\n";
    }
    oss << "\n## Glosario de T\xC3\xA9rminos (" << glossary.size() << ")\n";
    if (glossary.empty()) {
        oss << "- (Use el formato 'T\xC3\xA9rmino: definici\xC3\xB3n' en sus apuntes para poblar el glosario)\n";
    } else {
        for (const auto& kv : glossary) oss << "- " << kv.first << ": " << kv.second << "\n";
    }
    if (!codeNotes.empty()) {
        oss << "\n## Fragmentos de C\xC3\xB3""digo Clave (" << codeNotes.size() << ")\n";
        for (const auto& c : codeNotes) oss << "- " << c << "\n";
    }
    res.markdownUtf8 = oss.str();
    return res;
}

bool StudyAssistantTools::AppendFormulaSheetPage(
    DocumentModel& doc,
    CommandStack& stack,
    const FormulaSheetGlossaryResult& sheet)
{
    if (sheet.markdownUtf8.empty()) return false;
    if (doc.Mode() == DocumentMode::InfiniteCanvas) {
        AnnotationObject card{};
        card.kind = ObjectKind::TextBox;
        card.layerId = doc.ActiveLayerId();
        card.bounds = { 80.0, 80.0, 520.0, 420.0 };
        card.textUtf8 = sheet.markdownUtf8;
        card.captionUtf8 = "Hoja de F\xC3\xB3rmulas y Glosario";
        card.fontSizePt = 12.0;
        card.fillColor = DocColor::RGBA(252, 253, 255, 250);
        card.strokeColor = DocColor::RGBA(24, 30, 42, 255);
        return stack.Execute(doc, std::make_unique<AddObjectCommand>(ObjectTarget{ true, 0 }, std::move(card)));
    }

    PageModel pg{};
    pg.pageId = doc.AllocatePageId();
    pg.widthPt = 612.0;
    pg.heightPt = 792.0;
    pg.background = PaperPattern::Ruled;

    AnnotationObject tb{};
    tb.id = doc.AllocateObjectId();
    tb.zOrder = doc.AllocateZOrder();
    tb.kind = ObjectKind::TextBox;
    tb.layerId = doc.ActiveLayerId();
    tb.bounds = { 48.0, 48.0, 516.0, 696.0 };
    tb.textUtf8 = sheet.markdownUtf8;
    tb.captionUtf8 = "Hoja de F\xC3\xB3rmulas y Glosario";
    tb.fontSizePt = 12.0;
    tb.strokeColor = DocColor::RGBA(24, 30, 42, 255);
    pg.objects.push_back(std::move(tb));

    return stack.Execute(doc, std::make_unique<InsertPageCommand>(doc.Pages().size(), std::move(pg)));
}

// ---------------- 5. Automatic Task & Deadline Extractor + RFC 5545 .ics ----------------

TaskExtractionReport StudyAssistantTools::ExtractTasksAndDeadlines(
    const DocumentModel& doc,
    const KnowledgeVault* optionalVault,
    StructuredDatabase* optionalDbToPopulate)
{
    TaskExtractionReport rep{};
    std::set<std::string> seenTitles;

    auto scanBlock = [&](const std::string& text, std::size_t pgIdx, bool isCanvas) {
        std::istringstream iss(text);
        std::string ln;
        while (std::getline(iss, ln)) {
            const std::string t = Trim(ln);
            if (t.size() < 4) continue;
            const std::string low = ToLowerAscii(t);

            std::string category;
            std::string cleanTitle;
            if (t.rfind("- [ ]", 0) == 0 || t.rfind("* [ ]", 0) == 0) {
                category = "Tarea";
                cleanTitle = Trim(t.substr(5));
            } else if (t.rfind("\xE2\x98\x90", 0) == 0) { // ☐
                category = "Tarea";
                cleanTitle = Trim(t.substr(3));
            } else if (low.rfind("todo:", 0) == 0) {
                category = "Pendiente";
                cleanTitle = Trim(t.substr(5));
            } else if (low.rfind("tarea:", 0) == 0) {
                category = "Tarea";
                cleanTitle = Trim(t.substr(6));
            } else if (low.rfind("entrega:", 0) == 0) {
                category = "Entrega";
                cleanTitle = Trim(t.substr(8));
            } else if (low.rfind("examen:", 0) == 0 || low.rfind("parcial:", 0) == 0) {
                category = "Examen";
                const auto cpos = t.find(':');
                cleanTitle = Trim(t.substr(cpos + 1));
            } else {
                continue;
            }

            if (cleanTitle.empty()) continue;
            const std::string dedupKey = ToLowerAscii(cleanTitle);
            if (!seenTitles.insert(dedupKey).second) continue;

            ExtractedStudyTask task{};
            task.titleUtf8 = cleanTitle;
            task.dueDateIsoUtf8 = ExtractIsoDateFromLine(t);
            task.categoryUtf8 = category;
            task.sourcePageIndex0 = pgIdx;
            task.sourceIsCanvas = isCanvas;
            rep.extracted.push_back(std::move(task));
        }
    };

    for (std::size_t p = 0; p < doc.Pages().size(); ++p) {
        for (const auto& o : doc.Pages()[p].objects) {
            if (!o.textUtf8.empty()) scanBlock(o.textUtf8, p, false);
        }
    }
    for (const auto& o : doc.CanvasObjects()) {
        if (!o.textUtf8.empty()) scanBlock(o.textUtf8, 0, true);
    }
    if (optionalVault) {
        for (const auto& n : optionalVault->ActiveNotes()) {
            scanBlock(n.bodyMarkdownUtf8, 0, false);
        }
    }

    if (optionalDbToPopulate && !rep.extracted.empty()) {
        if (!optionalDbToPopulate->FindColumn("estado")) {
            optionalDbToPopulate->AddColumn({ "estado", "Estado", DbColumnType::Status, { "Pendiente", "En Progreso", "Completado" } });
        }
        if (!optionalDbToPopulate->FindColumn("fecha")) {
            optionalDbToPopulate->AddColumn({ "fecha", "Fecha", DbColumnType::Date, {} });
        }
        if (!optionalDbToPopulate->FindColumn("categoria")) {
            optionalDbToPopulate->AddColumn({ "categoria", "Categor\xC3\xAD""a", DbColumnType::Text, {} });
        }

        std::set<std::string> existingRecs;
        for (const auto& r : optionalDbToPopulate->Records()) {
            existingRecs.insert(ToLowerAscii(Trim(r.titleUtf8)));
        }

        for (const auto& task : rep.extracted) {
            if (existingRecs.count(ToLowerAscii(task.titleUtf8))) continue;
            DbRecord rec{};
            rec.titleUtf8 = task.titleUtf8;
            rec.bodyMarkdownUtf8 = "Origen: " + (task.sourceIsCanvas ? std::string("Lienzo") : ("P\xC3\xA1g. " + std::to_string(task.sourcePageIndex0 + 1)));
            const std::string rid = optionalDbToPopulate->AddRecord(std::move(rec));
            optionalDbToPopulate->SetCellText(rid, "estado", "Pendiente");
            optionalDbToPopulate->SetCellText(rid, "categoria", task.categoryUtf8);
            if (!task.dueDateIsoUtf8.empty()) {
                optionalDbToPopulate->SetCellText(rid, "fecha", task.dueDateIsoUtf8);
            }
            existingRecs.insert(ToLowerAscii(task.titleUtf8));
            rep.insertedIntoDatabaseCount++;
        }
        optionalDbToPopulate->RecomputeFormulasAndRollups();
    }

    rep.icsCalendarUtf8 = ExportTasksToIcsCalendar(rep.extracted, doc.Title().empty() ? "CuadernU Estudio" : doc.Title());
    return rep;
}

std::string StudyAssistantTools::ExportTasksToIcsCalendar(
    const std::vector<ExtractedStudyTask>& tasks,
    const std::string& calendarTitleUtf8)
{
    std::time_t nowT = std::time(nullptr);
    std::tm utcTm{};
#ifdef _WIN32
    gmtime_s(&utcTm, &nowT);
#else
    gmtime_r(&nowT, &utcTm);
#endif
    char stampBuf[32]{};
    char todayBuf[16]{};
    std::snprintf(stampBuf, sizeof(stampBuf), "%04d%02d%02dT%02d%02d%02dZ",
        utcTm.tm_year + 1900, utcTm.tm_mon + 1, utcTm.tm_mday,
        utcTm.tm_hour, utcTm.tm_min, utcTm.tm_sec);
    std::snprintf(todayBuf, sizeof(todayBuf), "%04d%02d%02d",
        utcTm.tm_year + 1900, utcTm.tm_mon + 1, utcTm.tm_mday);

    std::ostringstream oss;
    oss << "BEGIN:VCALENDAR\r\n"
        << "VERSION:2.0\r\n"
        << "PRODID:-//CuadernU//StudyTasks//ES\r\n"
        << "CALSCALE:GREGORIAN\r\n"
        << "X-WR-CALNAME:" << SanitizeIcsText(calendarTitleUtf8) << "\r\n";

    std::size_t idx = 1;
    for (const auto& t : tasks) {
        std::string dtVal = todayBuf;
        if (t.dueDateIsoUtf8.size() == 10) {
            dtVal = t.dueDateIsoUtf8.substr(0, 4) + t.dueDateIsoUtf8.substr(5, 2) + t.dueDateIsoUtf8.substr(8, 2);
        }
        oss << "BEGIN:VEVENT\r\n"
            << "UID:cuadernu-task-" << idx++ << "-" << dtVal << "@cuadernu.local\r\n"
            << "DTSTAMP:" << stampBuf << "\r\n"
            << "DTSTART;VALUE=DATE:" << dtVal << "\r\n"
            << "SUMMARY:" << SanitizeIcsText("[" + t.categoryUtf8 + "] " + t.titleUtf8) << "\r\n"
            << "DESCRIPTION:" << SanitizeIcsText("Extraido de CuadernU (Pag. " + std::to_string(t.sourcePageIndex0 + 1) + ")") << "\r\n"
            << "END:VEVENT\r\n";
    }
    oss << "END:VCALENDAR\r\n";
    return oss.str();
}

// ---------------- 6. 1-Click Post-Audio Lecture Pipeline ----------------

PostLectureAutomationReport StudyAssistantTools::RunPostLectureAutomation(
    DocumentModel& doc,
    CommandStack& stack,
    ObjectTarget target,
    AudioTimelineSession& audioSession,
    StructuredDatabase& db,
    const AiProviderVault* optionalVault,
    double cardX,
    double cardY)
{
    PostLectureAutomationReport rep{};
    auto segs = audioSession.DiarizeMultiSpeakerTranscript();
    if (segs.empty() && !audioSession.Samples().empty()) {
        const auto offlineSegs = audioSession.TranscribeOffline();
        for (const auto& os : offlineSegs) {
            SpeakerTranscriptSegment sp{};
            sp.speakerIdUtf8 = "Hablante 1";
            sp.startMs = os.startMs;
            sp.endMs = os.endMs;
            sp.textUtf8 = os.textUtf8;
            segs.push_back(std::move(sp));
        }
    }
    rep.transcriptSegments = segs.size();

    rep.summaryMarkdownUtf8 = GroundedStudyAiAssistant::GenerateExecutiveSummary(doc, segs, nullptr, optionalVault);

    const auto docTasksRep = ExtractTasksAndDeadlines(doc, nullptr, &db);
    rep.tasksInserted = docTasksRep.insertedIntoDatabaseCount;
    if (!segs.empty()) {
        DocumentModel trProxy;
        trProxy.ResetToBlank(DocumentMode::InfiniteCanvas);
        std::ostringstream trText;
        for (const auto& s : segs) trText << s.textUtf8 << "\n";
        AnnotationObject trObj{};
        trObj.kind = ObjectKind::TextBox;
        trObj.textUtf8 = trText.str();
        trProxy.AddCanvasObject(std::move(trObj));
        const auto trTasksRep = ExtractTasksAndDeadlines(trProxy, nullptr, &db);
        rep.tasksInserted += trTasksRep.insertedIntoDatabaseCount;
    }

    if (!rep.summaryMarkdownUtf8.empty()) {
        AnnotationObject card{};
        card.kind = ObjectKind::TextBox;
        card.layerId = doc.ActiveLayerId();
        card.bounds = { cardX, cardY, 440.0, 220.0 };
        card.captionUtf8 = "S\xC3\xADntesis Post-Audio";
        card.textUtf8 = rep.summaryMarkdownUtf8;
        card.fontSizePt = 11.5;
        card.fillColor = DocColor::RGBA(244, 248, 255, 250);
        card.strokeColor = DocColor::RGBA(24, 30, 42, 255);
        auto cmd = std::make_unique<AddObjectCommand>(target, std::move(card));
        auto* rawPtr = cmd.get();
        if (stack.Execute(doc, std::move(cmd))) {
            rep.insertedCardObjId = rawPtr->ObjectId();
            rep.ok = true;
        }
    }

    rep.statusNoteUtf8 = "Flujo post-audio: " + std::to_string(rep.transcriptSegments) + " seg., " +
                         std::to_string(rep.tasksInserted) + " tarea(s) a BD";
    return rep;
}

// ---------------- 7. Academic Citation Formatter (APA 7, IEEE, BibTeX) ----------------

std::string StudyAssistantTools::FormatAcademicCitation(
    const AcademicSourceInfo& src,
    CitationStyle style,
    int ieeeIndex)
{
    const std::string authors = Trim(src.authorsUtf8).empty() ? "Autor desconocido" : Trim(src.authorsUtf8);
    const std::string title = Trim(src.titleUtf8).empty() ? "Documento sin t\xC3\xADtulo" : Trim(src.titleUtf8);
    const std::string year = Trim(src.yearUtf8).empty() ? "s. f." : Trim(src.yearUtf8);
    const std::string venue = Trim(src.venueOrPublisherUtf8);
    const std::string url = Trim(src.urlUtf8);
    const std::string doi = Trim(src.doiUtf8);
    const std::string pages = Trim(src.pagesUtf8);

    std::ostringstream oss;
    if (style == CitationStyle::Apa7) {
        oss << authors;
        if (authors.back() != '.') oss << ".";
        oss << " (" << year << "). " << title << ".";
        if (!venue.empty()) oss << " " << venue << ".";
        if (!pages.empty()) oss << " pp. " << pages << ".";
        if (!doi.empty()) oss << " https://doi.org/" << doi;
        else if (!url.empty()) oss << " " << url;
        return oss.str();
    }

    if (style == CitationStyle::Ieee) {
        oss << "[" << (std::max)(1, ieeeIndex) << "] " << authors << ", \"" << title << ",\"";
        if (!venue.empty()) oss << " " << venue << ",";
        if (!pages.empty()) oss << " pp. " << pages << ",";
        oss << " " << year << ".";
        if (!doi.empty()) oss << " doi: " << doi << ".";
        else if (!url.empty()) oss << " [En l\xC3\xADnea]. Disponible: " << url;
        return oss.str();
    }

    // BibTeX
    const std::string kindLow = ToLowerAscii(src.sourceKindUtf8);
    const std::string entryType = (kindLow == "book") ? "book" : ((kindLow == "article") ? "article" : "misc");
    const std::string bibKey = MakeBibKey(authors, year, title);
    oss << "@" << entryType << "{" << bibKey << ",\n"
        << "  author = {" << authors << "},\n"
        << "  title = {" << title << "},\n"
        << "  year = {" << year << "}";
    if (!venue.empty()) oss << ",\n  " << (entryType == "article" ? "journal" : "publisher") << " = {" << venue << "}";
    if (!pages.empty()) oss << ",\n  pages = {" << pages << "}";
    if (!doi.empty()) oss << ",\n  doi = {" << doi << "}";
    if (!url.empty()) oss << ",\n  url = {" << url << "}";
    oss << "\n}";
    return oss.str();
}

std::vector<AcademicSourceInfo> StudyAssistantTools::HarvestCitationCandidates(
    const DocumentModel& doc,
    const FlexcilStudyOverlay* optionalFlexcil)
{
    std::vector<AcademicSourceInfo> list;
    std::set<std::string> seenTitles;

    if (optionalFlexcil) {
        const auto indexed = optionalFlexcil->SearchIndexedPdfMetadata("");
        for (const auto& m : indexed) {
            if (m.titleUtf8.empty() || !seenTitles.insert(m.titleUtf8).second) continue;
            AcademicSourceInfo s{};
            s.titleUtf8 = m.titleUtf8;
            s.authorsUtf8 = m.authorUtf8.empty() ? "Autor" : m.authorUtf8;
            s.yearUtf8 = "2026";
            s.venueOrPublisherUtf8 = "PDF Indexado";
            s.pagesUtf8 = m.pageCount > 0 ? ("1-" + std::to_string(m.pageCount)) : "";
            s.sourceKindUtf8 = "book";
            list.push_back(std::move(s));
        }
    }

    // Also harvest Web Clipper captures inside document TextBoxes ("- Fuente: https://...")
    auto harvestFromObjs = [&](const std::vector<AnnotationObject>& objs) {
        for (const auto& o : objs) {
            if (o.kind != ObjectKind::TextBox || o.textUtf8.empty()) continue;
            if (o.textUtf8.find("Fuente:") == std::string::npos && o.textUtf8.find("http") == std::string::npos) continue;
            AcademicSourceInfo s{};
            s.titleUtf8 = !o.captionUtf8.empty() ? o.captionUtf8 : doc.Title();
            s.yearUtf8 = "2026";
            s.sourceKindUtf8 = "web";
            std::istringstream iss(o.textUtf8);
            std::string ln;
            while (std::getline(iss, ln)) {
                ln = Trim(ln);
                if (ln.rfind("# ", 0) == 0 && s.titleUtf8.empty()) s.titleUtf8 = Trim(ln.substr(2));
                else if (ln.find("Fuente:") != std::string::npos) s.urlUtf8 = Trim(ln.substr(ln.find("Fuente:") + 7));
                else if (ln.find("Autor:") != std::string::npos) s.authorsUtf8 = Trim(ln.substr(ln.find("Autor:") + 6));
            }
            if (!s.titleUtf8.empty() && seenTitles.insert(s.titleUtf8).second) {
                list.push_back(std::move(s));
            }
        }
    };
    for (const auto& pg : doc.Pages()) harvestFromObjs(pg.objects);
    harvestFromObjs(doc.CanvasObjects());

    if (list.empty()) {
        AcademicSourceInfo cur{};
        cur.titleUtf8 = doc.Title().empty() ? "Cuaderno de Estudio" : doc.Title();
        cur.authorsUtf8 = "Autor Local";
        cur.yearUtf8 = "2026";
        cur.venueOrPublisherUtf8 = "CuadernU";
        cur.pagesUtf8 = doc.Pages().empty() ? "" : ("1-" + std::to_string(doc.Pages().size()));
        cur.sourceKindUtf8 = "misc";
        list.push_back(std::move(cur));
    }
    return list;
}

} // namespace DocStudio
