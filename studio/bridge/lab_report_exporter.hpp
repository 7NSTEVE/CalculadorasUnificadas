#pragma once

#include "../circuit/mna_solver.hpp"
#include "../core/doc_types.hpp"
#include "../core/document_model.hpp"
#include "../core/notebook_vault.hpp"
#include "../render/card_render_common.hpp"

#include <algorithm>
#include <cstdint>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace DocStudio::Bridge {

inline std::string EscapeHtmlText(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 16);
    for (char c : s) {
        switch (c) {
        case '&': out += "&amp;"; break;
        case '<': out += "&lt;"; break;
        case '>': out += "&gt;"; break;
        case '"': out += "&quot;"; break;
        case '\'': out += "&#39;"; break;
        default: out.push_back(c); break;
        }
    }
    return out;
}

inline std::string Base64EncodeBytes(const std::vector<std::uint8_t>& data) {
    static constexpr const char* kAlphabet =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    out.reserve(((data.size() + 2) / 3) * 4);
    std::size_t i = 0;
    while (i + 2 < data.size()) {
        const std::uint32_t triple =
            (static_cast<std::uint32_t>(data[i]) << 16) |
            (static_cast<std::uint32_t>(data[i + 1]) << 8) |
            static_cast<std::uint32_t>(data[i + 2]);
        out.push_back(kAlphabet[(triple >> 18) & 0x3F]);
        out.push_back(kAlphabet[(triple >> 12) & 0x3F]);
        out.push_back(kAlphabet[(triple >> 6) & 0x3F]);
        out.push_back(kAlphabet[triple & 0x3F]);
        i += 3;
    }
    if (i < data.size()) {
        std::uint32_t triple = static_cast<std::uint32_t>(data[i]) << 16;
        if (i + 1 < data.size()) triple |= static_cast<std::uint32_t>(data[i + 1]) << 8;
        out.push_back(kAlphabet[(triple >> 18) & 0x3F]);
        out.push_back(kAlphabet[(triple >> 12) & 0x3F]);
        out.push_back((i + 1 < data.size()) ? kAlphabet[(triple >> 6) & 0x3F] : '=');
        out.push_back('=');
    }
    return out;
}

inline std::string RenderPlotBlockInlineSvg(const AnnotationObject& obj) {
    const auto plot = RenderCommon::BuildPlotRenderData(obj);
    const double w = 520.0;
    const double h = 280.0;
    const double padL = 36.0, padR = 20.0, padT = 22.0, padB = 28.0;
    const double pw = w - padL - padR;
    const double ph = h - padT - padB;

    auto mapPt = [&](double wx, double wy) -> DocPoint {
        const double nx = std::clamp((wx - plot.xMin) / (std::max)(1e-9, plot.xMax - plot.xMin), 0.0, 1.0);
        const double ny = std::clamp((wy - plot.yMin) / (std::max)(1e-9, plot.yMax - plot.yMin), 0.0, 1.0);
        return { padL + nx * pw, padT + (1.0 - ny) * ph };
    };

    std::ostringstream svg;
    svg.precision(2);
    svg << std::fixed;
    svg << "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"" << static_cast<int>(w)
        << "\" height=\"" << static_cast<int>(h) << "\" viewBox=\"0 0 " << static_cast<int>(w)
        << " " << static_cast<int>(h) << "\">\n";
    svg << "<rect x=\"0\" y=\"0\" width=\"" << w << "\" height=\"" << h
        << "\" rx=\"8\" fill=\"#ffffff\" stroke=\"#cbd5e1\"/>\n";
    svg << "<rect x=\"" << padL << "\" y=\"" << padT << "\" width=\"" << pw << "\" height=\"" << ph
        << "\" fill=\"#f8fafc\" stroke=\"#94a3b8\"/>\n";

    if (plot.is3D && !plot.surface3DSegments.empty()) {
        for (const auto& seg : plot.surface3DSegments) {
            const DocPoint pa = mapPt(seg.a.x, seg.a.y);
            const DocPoint pb = mapPt(seg.b.x, seg.b.y);
            svg << "<line x1=\"" << pa.x << "\" y1=\"" << pa.y
                << "\" x2=\"" << pb.x << "\" y2=\"" << pb.y
                << "\" stroke=\"rgb(" << static_cast<int>(seg.color.r) << ","
                << static_cast<int>(seg.color.g) << "," << static_cast<int>(seg.color.b)
                << ")\" stroke-width=\"1.35\"/>\n";
        }
    } else {
        if (plot.xMin <= 0.0 && plot.xMax >= 0.0) {
            const auto a = mapPt(0.0, plot.yMin), b = mapPt(0.0, plot.yMax);
            svg << "<line x1=\"" << a.x << "\" y1=\"" << a.y << "\" x2=\"" << b.x << "\" y2=\"" << b.y
                << "\" stroke=\"#94a3b8\" stroke-width=\"1\"/>\n";
        }
        if (plot.yMin <= 0.0 && plot.yMax >= 0.0) {
            const auto a = mapPt(plot.xMin, 0.0), b = mapPt(plot.xMax, 0.0);
            svg << "<line x1=\"" << a.x << "\" y1=\"" << a.y << "\" x2=\"" << b.x << "\" y2=\"" << b.y
                << "\" stroke=\"#94a3b8\" stroke-width=\"1\"/>\n";
        }
        if (plot.curvePoints.size() >= 2) {
            svg << "<polyline fill=\"none\" stroke=\"#4f46e5\" stroke-width=\"2\" points=\"";
            for (const auto& p : plot.curvePoints) {
                const auto sp = mapPt(p.x, p.y);
                svg << sp.x << "," << sp.y << " ";
            }
            svg << "\"/>\n";
        }
        for (const auto& sp : plot.scatterPoints) {
            const auto mp = mapPt(sp.x, sp.y);
            svg << "<circle cx=\"" << mp.x << "\" cy=\"" << mp.y << "\" r=\"3.2\" fill=\"#dc2626\"/>\n";
        }
    }

    if (!plot.footerUtf8.empty()) {
        svg << "<text x=\"" << padL << "\" y=\"" << (h - 8.0)
            << "\" font-family=\"Consolas, monospace\" font-size=\"11\" fill=\"#334155\">"
            << EscapeHtmlText(plot.footerUtf8) << "</text>\n";
    }
    svg << "</svg>\n";
    return svg.str();
}

inline std::vector<const AnnotationObject*> CollectOrderedStemObjects(const DocumentModel& doc) {
    std::vector<const AnnotationObject*> list;
    if (doc.Mode() == DocumentMode::InfiniteCanvas) {
        for (const auto& o : doc.CanvasObjects()) list.push_back(&o);
    } else {
        for (const auto& pg : doc.Pages()) {
            for (const auto& o : pg.objects) list.push_back(&o);
        }
        for (const auto& o : doc.CanvasObjects()) list.push_back(&o);
    }
    std::stable_sort(list.begin(), list.end(), [](const AnnotationObject* a, const AnnotationObject* b) {
        if (std::abs(a->bounds.y - b->bounds.y) > 8.0) return a->bounds.y < b->bounds.y;
        return a->bounds.x < b->bounds.x;
    });
    return list;
}

// Generates a self-contained Calcpad / Lab Calculation Memory HTML5 document (F-127)
inline std::string ExportCalculationReportHtml(const DocumentModel& doc) {
    const std::string title = doc.Title().empty() ? "Memoria de C\xC3\xA1" "lculo e Informe STEM" : doc.Title();
    const auto items = CollectOrderedStemObjects(doc);
    const auto tagged = CollectTaggedItems(doc, "");

    std::ostringstream html;
    html << "<!DOCTYPE html>\n<html lang=\"es\">\n<head>\n<meta charset=\"utf-8\"/>\n"
         << "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\"/>\n"
         << "<title>" << EscapeHtmlText(title) << " - Document Studio STEM</title>\n"
         << "<style>\n"
         << "body{font-family:'Segoe UI',system-ui,sans-serif;max-width:960px;margin:2rem auto;padding:0 1.5rem;color:#0f172a;background:#f8fafc;line-height:1.55;}\n"
         << "header{background:#0f172a;color:#f8fafc;padding:1.4rem 1.8rem;border-radius:12px;margin-bottom:1.6rem;}\n"
         << "header h1{margin:0 0 0.35rem 0;font-size:1.55rem;}\n"
         << ".meta{font-size:0.88rem;color:#cbd5e1;}\n"
         << ".card{background:#ffffff;border:1px solid #cbd5e1;border-left:5px solid #2563eb;border-radius:10px;padding:1rem 1.25rem;margin-bottom:1.1rem;box-shadow:0 1px 3px rgba(15,23,42,0.06);}\n"
         << ".card.calc{border-left-color:#16a34a;background:#f0fdf4;}\n"
         << ".card.circuit{border-left-color:#0d9488;background:#f0fdfa;}\n"
         << ".card.plot{border-left-color:#6366f1;}\n"
         << ".card.tape{border-left-color:#d97706;background:#fffbeb;}\n"
         << ".card h3{margin:0 0 0.45rem 0;font-size:1.02rem;color:#1e3a8a;}\n"
         << "pre,code{font-family:'Cascadia Mono',Consolas,monospace;font-size:0.92rem;white-space:pre-wrap;margin:0.35rem 0;}\n"
         << ".tag{display:inline-block;background:#dbeafe;color:#1d4ed8;border-radius:999px;padding:0.1rem 0.55rem;font-size:0.78rem;margin-right:0.35rem;font-weight:600;}\n"
         << "table{width:100%;border-collapse:collapse;margin-bottom:1.4rem;background:#fff;border-radius:8px;overflow:hidden;}\n"
         << "th,td{border:1px solid #e2e8f0;padding:0.5rem 0.75rem;text-align:left;font-size:0.9rem;}\n"
         << "th{background:#f1f5f9;}\n"
         << "@media print{body{background:#fff;margin:0;} .card{break-inside:avoid;}}\n"
         << "</style>\n</head>\n<body>\n";

    html << "<header>\n<h1>" << EscapeHtmlText(title) << "</h1>\n"
         << "<div class=\"meta\">Calculadora Unificada &amp; Document Studio &mdash; Memoria de C\xC3\xA1" "lculo Verificable (#varsub / MNA / Casio ClassWiz)</div>\n"
         << "</header>\n";

    if (!doc.Bookmarks().empty()) {
        html << "<section class=\"card\"><h3>Marcadores del Documento</h3><ul>\n";
        for (const auto& bm : doc.Bookmarks()) {
            html << "<li><strong>P\xC3\xA1" "gina " << (bm.pageIndex + 1) << ":</strong> "
                 << EscapeHtmlText(bm.titleUtf8) << "</li>\n";
        }
        html << "</ul></section>\n";
    }

    if (!tagged.empty()) {
        html << "<h2>Tablero Resumen de Etiquetas (#tags)</h2>\n<table><thead><tr><th>Etiqueta</th><th>Ubicaci\xC3\xB3n</th><th>Resumen</th></tr></thead><tbody>\n";
        for (const auto& ti : tagged) {
            html << "<tr><td><span class=\"tag\">#" << EscapeHtmlText(ti.tagUtf8) << "</span></td><td>"
                 << (ti.isCanvas ? std::string("Lienzo Infinito") : ("P\xC3\xA1" "gina " + std::to_string(ti.pageIndex + 1)))
                 << "</td><td>" << EscapeHtmlText(ti.summaryUtf8) << "</td></tr>\n";
        }
        html << "</tbody></table>\n";
    }

    html << "<h2>Desarrollo T\xC3\xA9" "cnico y Bloques Verificados</h2>\n";
    for (const auto* o : items) {
        if (!o) continue;
        if (o->kind == ObjectKind::InkStroke || o->kind == ObjectKind::Shape) continue;

        std::string tagPills;
        for (const auto& tg : o->tags) {
            tagPills += "<span class=\"tag\">#" + EscapeHtmlText(tg) + "</span>";
        }

        if (o->kind == ObjectKind::MathBlock) {
            const char* modeLbl = (o->mathSubMode == MathSubMode::NoSub) ? "#nosub"
                                : (o->mathSubMode == MathSubMode::NoVar) ? "#novar" : "#varsub";
            html << "<article class=\"card\"><h3>Bloque Matem\xC3\xA1" "tico (" << modeLbl << ") " << tagPills << "</h3>\n";
            if (o->mathSubMode != MathSubMode::NoVar && !o->formulaExprUtf8.empty()) {
                html << "<div><code>" << EscapeHtmlText(o->formulaExprUtf8) << "</code></div>\n";
            }
            const std::string disp = !o->textUtf8.empty() ? o->textUtf8 : o->evaluatedResultUtf8;
            html << "<pre><strong>" << EscapeHtmlText(disp) << "</strong></pre>\n";
            if (!o->stepCheckStatusUtf8.empty()) {
                html << "<div class=\"meta\">Auditor\xC3\xAD" "a algebraica: " << EscapeHtmlText(o->stepCheckStatusUtf8) << "</div>\n";
            }
            html << "</article>\n";
        } else if (o->kind == ObjectKind::CalcSessionCard) {
            std::string sessionIdBadge;
            for (const auto& tg : o->tags) {
                if (tg.rfind("calc_session:", 0) == 0) {
                    sessionIdBadge = tg.substr(13);
                    break;
                }
            }
            html << "<article class=\"card calc\"><h3>Tarjeta Casio " << EscapeHtmlText(o->calcModelIdUtf8.empty() ? "ClassWiz" : o->calcModelIdUtf8)
                 << " " << tagPills << "</h3>\n";
            if (!sessionIdBadge.empty()) {
                html << "<div class=\"meta\">Sesi\xC3\xB3n en vivo: <code>" << EscapeHtmlText(sessionIdBadge) << "</code></div>\n";
            }
            if (!o->calcKeySeqUtf8.empty()) {
                html << "<div><strong>Secuencia de teclas:</strong> <code>" << EscapeHtmlText(o->calcKeySeqUtf8) << "</code></div>\n";
            }
            if (!o->calcLatexUtf8.empty()) {
                html << "<div><strong>Expresi\xC3\xB3n:</strong> <code>" << EscapeHtmlText(o->calcLatexUtf8) << "</code></div>\n";
            }
            if (!o->evaluatedResultUtf8.empty()) {
                html << "<pre><strong>Resultado = " << EscapeHtmlText(o->evaluatedResultUtf8) << "</strong></pre>\n";
            }
            if (!o->pngBytes.empty()) {
                html << "<div style=\"margin-top:0.5rem\"><img alt=\"LCD Casio\" style=\"max-width:260px;border:1px solid #86efac;border-radius:4px\" src=\"data:image/png;base64,"
                     << Base64EncodeBytes(o->pngBytes) << "\"/></div>\n";
            }
            html << "</article>\n";
        } else if (o->kind == ObjectKind::CircuitBlock) {
            html << "<article class=\"card circuit\"><h3>" << EscapeHtmlText(o->captionUtf8.empty() ? "Circuito MNA AC/DC & Laplace" : o->captionUtf8)
                 << " " << tagPills << "</h3>\n";
            if (!o->circuitNetlistUtf8.empty()) {
                html << "<pre>" << EscapeHtmlText(o->circuitNetlistUtf8) << "</pre>\n";
                std::vector<Circuit::CircuitComponent> comps;
                if (Circuit::ParseNetlistText(o->circuitNetlistUtf8, comps)) {
                    const auto dcSol = Circuit::SolveMnaCircuit(comps, 0.0);
                    const auto lap = Circuit::AnalyzeSymbolicLaplace(comps, 1);
                    if (lap.ok) {
                        html << "<div><strong>Funci\xC3\xB3n de transferencia simb\xC3\xB3lica:</strong> <code>"
                             << EscapeHtmlText(lap.transferFunctionUtf8) << "</code></div>\n";
                    }
                    html << "<div style=\"margin-top:0.6rem\">" << Circuit::ExportCircuitToSvg(comps, dcSol.ok ? &dcSol : nullptr) << "</div>\n";
                }
            }
            if (!o->evaluatedResultUtf8.empty()) {
                html << "<pre>" << EscapeHtmlText(o->evaluatedResultUtf8) << "</pre>\n";
            }
            html << "</article>\n";
        } else if (o->kind == ObjectKind::PlotBlock) {
            html << "<article class=\"card plot\"><h3>" << EscapeHtmlText(o->captionUtf8.empty() ? "Gr\xC3\xA1" "fica STEM 2D / 3D" : o->captionUtf8)
                 << " " << tagPills << "</h3>\n"
                 << RenderPlotBlockInlineSvg(*o)
                 << "</article>\n";
        } else if (o->kind == ObjectKind::StudyTape) {
            html << "<article class=\"card tape\"><h3>Cinta de Estudio " << tagPills << "</h3>\n"
                 << "<pre>" << EscapeHtmlText(o->textUtf8.empty() ? o->captionUtf8 : o->textUtf8) << "</pre></article>\n";
        } else if (o->kind == ObjectKind::TextBox && !o->textUtf8.empty()) {
            html << "<article class=\"card\"><h3>Nota T\xC3\xA9" "cnica " << tagPills << "</h3>\n"
                 << "<pre>" << EscapeHtmlText(o->textUtf8) << "</pre></article>\n";
        }
    }

    html << "</body>\n</html>\n";
    return html.str();
}

inline std::string ExportCalculationReportMarkdown(const DocumentModel& doc) {
    const std::string title = doc.Title().empty() ? "Memoria de Calculo STEM" : doc.Title();
    const auto items = CollectOrderedStemObjects(doc);
    std::ostringstream md;
    md << "# " << title << "\n\n";
    for (const auto* o : items) {
        if (!o) continue;
        if (o->kind == ObjectKind::MathBlock) {
            md << "### Ecuaci\xC3\xB3n (`" << o->formulaExprUtf8 << "`)\n\n"
               << "- **Sustituci\xC3\xB3n / Resultado:** `" << (!o->textUtf8.empty() ? o->textUtf8 : o->evaluatedResultUtf8) << "`\n\n";
        } else if (o->kind == ObjectKind::CalcSessionCard) {
            md << "### Tarjeta Casio " << o->calcModelIdUtf8 << "\n\n"
               << "- **Teclas:** `" << o->calcKeySeqUtf8 << "`\n"
               << "- **Resultado:** `" << o->evaluatedResultUtf8 << "`\n\n";
        } else if (o->kind == ObjectKind::CircuitBlock) {
            md << "### Circuito MNA\n\n```spice\n" << o->circuitNetlistUtf8 << "\n```\n\n"
               << "- **Soluci\xC3\xB3n:** " << o->evaluatedResultUtf8 << "\n\n";
        } else if (o->kind == ObjectKind::PlotBlock) {
            md << "### Gr\xC3\xA1" "fica (`" << o->formulaExprUtf8 << "`)\n\n"
               << "- " << o->evaluatedResultUtf8 << "\n\n";
        } else if (o->kind == ObjectKind::TextBox && !o->textUtf8.empty()) {
            md << o->textUtf8 << "\n\n";
        }
    }
    return md.str();
}

inline bool SaveUtf8ReportToFile(const std::string& pathUtf8, const std::string& contentUtf8) {
    std::ofstream ofs(pathUtf8, std::ios::binary | std::ios::trunc);
    if (!ofs) return false;
    ofs.write(contentUtf8.data(), static_cast<std::streamsize>(contentUtf8.size()));
    return ofs.good();
}

} // namespace DocStudio::Bridge
