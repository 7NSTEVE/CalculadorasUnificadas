#pragma once

#include "circuit_models.hpp"
#include "circuit_schematic.hpp"

#include <cmath>
#include <iomanip>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace DocStudio::Circuit {

struct SymLine {
    double x1 = 0.0;
    double y1 = 0.0;
    double x2 = 0.0;
    double y2 = 0.0;
};

struct SymCircle {
    double cx = 0.0;
    double cy = 0.0;
    double r = 1.0;
    bool filled = false;
};

struct SymArc {
    double cx = 0.0;
    double cy = 0.0;
    double r = 1.0;
    double startDeg = 0.0;
    double sweepDeg = 180.0;
};

struct SymPoly2D {
    std::vector<Vec2> pts;
    bool closed = true;
    bool filled = false;
};

struct SymText {
    double x = 0.0;
    double y = 0.0;
    std::string text;
    double height = 7.0;
};

struct SymbolVectorGeometry {
    std::vector<SymLine> lines;
    std::vector<SymCircle> circles;
    std::vector<SymArc> arcs;
    std::vector<SymPoly2D> polys;
    std::vector<SymText> texts;
};

inline SymbolVectorGeometry GetComponentSymbolGeometry(ComponentKind kind, SymbolStandard std = SymbolStandard::IEC) {
    SymbolVectorGeometry g;
    switch (kind) {
    case ComponentKind::Resistor:
    case ComponentKind::Potentiometer:
    case ComponentKind::Ldr:
    case ComponentKind::NtcThermistor: {
        g.lines.push_back({ -40.0, 0.0, -18.0, 0.0 });
        g.lines.push_back({ 18.0, 0.0, 40.0, 0.0 });
        if (std == SymbolStandard::IEC) {
            g.polys.push_back({ { { -18.0, -7.0 }, { 18.0, -7.0 }, { 18.0, 7.0 }, { -18.0, 7.0 } }, true, false });
        } else {
            g.polys.push_back({ {
                { -18.0, 0.0 }, { -15.0, -7.0 }, { -9.0, 7.0 }, { -3.0, -7.0 },
                { 3.0, 7.0 }, { 9.0, -7.0 }, { 15.0, 7.0 }, { 18.0, 0.0 }
            }, false, false });
        }
        if (kind == ComponentKind::Potentiometer) {
            g.lines.push_back({ 0.0, -20.0, 0.0, -8.0 });
            g.lines.push_back({ -4.0, -12.0, 0.0, -8.0 });
            g.lines.push_back({ 4.0, -12.0, 0.0, -8.0 });
        } else if (kind == ComponentKind::Ldr) {
            g.circles.push_back({ 0.0, 0.0, 20.0, false });
            g.lines.push_back({ -14.0, -24.0, -5.0, -14.0 });
            g.lines.push_back({ -6.0, -26.0, 3.0, -16.0 });
        } else if (kind == ComponentKind::NtcThermistor) {
            g.lines.push_back({ -14.0, 12.0, 14.0, -12.0 });
            g.lines.push_back({ -18.0, 12.0, -14.0, 12.0 });
            g.texts.push_back({ 10.0, -14.0, "-t", 6.0 });
        }
        break;
    }

    case ComponentKind::Capacitor:
    case ComponentKind::PolarizedCapacitor: {
        g.lines.push_back({ -40.0, 0.0, -4.0, 0.0 });
        g.lines.push_back({ 4.0, 0.0, 40.0, 0.0 });
        g.lines.push_back({ -4.0, -14.0, -4.0, 14.0 });
        if (kind == ComponentKind::PolarizedCapacitor && std == SymbolStandard::ANSI) {
            g.arcs.push_back({ 14.0, 0.0, 14.0, 130.0, 100.0 });
            g.texts.push_back({ -14.0, -10.0, "+", 7.0 });
        } else {
            g.lines.push_back({ 4.0, -14.0, 4.0, 14.0 });
            if (kind == ComponentKind::PolarizedCapacitor) {
                g.polys.push_back({ { { 4.0, -14.0 }, { 9.0, -14.0 }, { 9.0, 14.0 }, { 4.0, 14.0 } }, true, false });
                g.texts.push_back({ -14.0, -10.0, "+", 7.0 });
            }
        }
        break;
    }

    case ComponentKind::Inductor: {
        g.lines.push_back({ -40.0, 0.0, -18.0, 0.0 });
        g.lines.push_back({ 18.0, 0.0, 40.0, 0.0 });
        if (std == SymbolStandard::IEC) {
            g.polys.push_back({ { { -18.0, -6.0 }, { 18.0, -6.0 }, { 18.0, 6.0 }, { -18.0, 6.0 } }, true, true });
        } else {
            for (int i = 0; i < 4; ++i) {
                g.arcs.push_back({ -13.5 + static_cast<double>(i) * 9.0, 0.0, 4.5, 0.0, 180.0 });
            }
        }
        break;
    }

    case ComponentKind::Crystal: {
        g.lines.push_back({ -40.0, 0.0, -10.0, 0.0 });
        g.lines.push_back({ 10.0, 0.0, 40.0, 0.0 });
        g.lines.push_back({ -10.0, -12.0, -10.0, 12.0 });
        g.lines.push_back({ 10.0, -12.0, 10.0, 12.0 });
        g.polys.push_back({ { { -6.0, -10.0 }, { 6.0, -10.0 }, { 6.0, 10.0 }, { -6.0, 10.0 } }, true, false });
        break;
    }

    case ComponentKind::Transformer: {
        g.lines.push_back({ -40.0, -20.0, -14.0, -20.0 });
        g.lines.push_back({ -40.0, 20.0, -14.0, 20.0 });
        g.lines.push_back({ 14.0, -20.0, 40.0, -20.0 });
        g.lines.push_back({ 14.0, 20.0, 40.0, 20.0 });
        g.lines.push_back({ -3.0, -20.0, -3.0, 20.0 });
        g.lines.push_back({ 3.0, -20.0, 3.0, 20.0 });
        for (int i = 0; i < 4; ++i) {
            const double cy = -15.0 + static_cast<double>(i) * 10.0;
            g.arcs.push_back({ -14.0, cy, 5.0, -90.0, 180.0 });
            g.arcs.push_back({ 14.0, cy, 5.0, 90.0, 180.0 });
        }
        break;
    }

    case ComponentKind::VoltageSource:
    case ComponentKind::AcSource:
    case ComponentKind::CurrentSource:
    case ComponentKind::PulseSource:
    case ComponentKind::Vcvs:
    case ComponentKind::Vccs: {
        g.lines.push_back({ -40.0, 0.0, -16.0, 0.0 });
        g.lines.push_back({ 16.0, 0.0, 40.0, 0.0 });
        if (kind == ComponentKind::Vcvs || kind == ComponentKind::Vccs) {
            g.polys.push_back({ { { 0.0, -18.0 }, { 18.0, 0.0 }, { 0.0, 18.0 }, { -18.0, 0.0 } }, true, false });
        } else {
            g.circles.push_back({ 0.0, 0.0, 16.0, false });
        }
        if (kind == ComponentKind::VoltageSource || kind == ComponentKind::Vcvs) {
            g.texts.push_back({ -9.0, 3.0, "+", 8.0 });
            g.texts.push_back({ 5.0, 3.0, "-", 8.0 });
        } else if (kind == ComponentKind::AcSource) {
            g.arcs.push_back({ -4.0, 0.0, 4.0, 0.0, 180.0 });
            g.arcs.push_back({ 4.0, 0.0, 4.0, 180.0, 180.0 });
        } else if (kind == ComponentKind::CurrentSource || kind == ComponentKind::Vccs) {
            g.lines.push_back({ -9.0, 0.0, 9.0, 0.0 });
            g.lines.push_back({ 3.0, -5.0, 9.0, 0.0 });
            g.lines.push_back({ 3.0, 5.0, 9.0, 0.0 });
        } else {
            g.polys.push_back({ { { -10.0, 5.0 }, { -5.0, 5.0 }, { -5.0, -5.0 }, { 5.0, -5.0 }, { 5.0, 5.0 }, { 10.0, 5.0 } }, false, false });
        }
        break;
    }

    case ComponentKind::Battery: {
        g.lines.push_back({ -40.0, 0.0, -6.0, 0.0 });
        g.lines.push_back({ 6.0, 0.0, 40.0, 0.0 });
        g.lines.push_back({ -6.0, -14.0, -6.0, 14.0 });
        g.lines.push_back({ -2.0, -7.0, -2.0, 7.0 });
        g.lines.push_back({ 2.0, -14.0, 2.0, 14.0 });
        g.lines.push_back({ 6.0, -7.0, 6.0, 7.0 });
        g.texts.push_back({ -14.0, -10.0, "+", 7.0 });
        break;
    }

    case ComponentKind::Ground: {
        g.lines.push_back({ 0.0, -20.0, 0.0, 0.0 });
        g.lines.push_back({ -14.0, 0.0, 14.0, 0.0 });
        g.lines.push_back({ -9.0, 5.0, 9.0, 5.0 });
        g.lines.push_back({ -4.0, 10.0, 4.0, 10.0 });
        break;
    }

    case ComponentKind::Vcc: {
        g.lines.push_back({ 0.0, 20.0, 0.0, 0.0 });
        g.lines.push_back({ -10.0, 0.0, 10.0, 0.0 });
        g.circles.push_back({ 0.0, -4.0, 3.0, false });
        break;
    }

    case ComponentKind::Diode:
    case ComponentKind::Led:
    case ComponentKind::ZenerDiode:
    case ComponentKind::SchottkyDiode:
    case ComponentKind::Photodiode: {
        g.lines.push_back({ -40.0, 0.0, -10.0, 0.0 });
        g.lines.push_back({ 10.0, 0.0, 40.0, 0.0 });
        g.polys.push_back({ { { -10.0, -10.0 }, { 10.0, 0.0 }, { -10.0, 10.0 } }, true, false });
        g.lines.push_back({ 10.0, -10.0, 10.0, 10.0 });
        if (kind == ComponentKind::ZenerDiode) {
            g.lines.push_back({ 10.0, -10.0, 15.0, -13.0 });
            g.lines.push_back({ 10.0, 10.0, 5.0, 13.0 });
        } else if (kind == ComponentKind::SchottkyDiode) {
            g.lines.push_back({ 10.0, -10.0, 15.0, -10.0 });
            g.lines.push_back({ 15.0, -10.0, 15.0, -7.0 });
            g.lines.push_back({ 10.0, 10.0, 5.0, 10.0 });
            g.lines.push_back({ 5.0, 10.0, 5.0, 7.0 });
        } else if (kind == ComponentKind::Led) {
            g.lines.push_back({ 2.0, -14.0, 10.0, -22.0 });
            g.lines.push_back({ 9.0, -11.0, 17.0, -19.0 });
        } else if (kind == ComponentKind::Photodiode) {
            g.lines.push_back({ 10.0, -22.0, 2.0, -14.0 });
            g.lines.push_back({ 17.0, -19.0, 9.0, -11.0 });
        }
        break;
    }

    case ComponentKind::BjtNpn:
    case ComponentKind::BjtPnp: {
        g.circles.push_back({ 0.0, 0.0, 20.0, false });
        g.lines.push_back({ -40.0, 0.0, -8.0, 0.0 });
        g.lines.push_back({ -8.0, -12.0, -8.0, 12.0 });
        g.lines.push_back({ -8.0, -5.0, 20.0, -20.0 });
        g.lines.push_back({ -8.0, 5.0, 20.0, 20.0 });
        if (kind == ComponentKind::BjtNpn) {
            g.polys.push_back({ { { 8.0, 10.0 }, { 15.0, 17.0 }, { 4.0, 17.0 } }, true, true });
        } else {
            g.polys.push_back({ { { -4.0, -7.0 }, { 5.0, -14.0 }, { 7.0, -9.0 } }, true, true });
        }
        break;
    }

    case ComponentKind::MosfetN:
    case ComponentKind::MosfetP: {
        g.circles.push_back({ 0.0, 0.0, 20.0, false });
        g.lines.push_back({ -40.0, 0.0, -12.0, 0.0 });
        g.lines.push_back({ -12.0, -12.0, -12.0, 12.0 });
        // Broken 3-segment enhancement channel
        g.lines.push_back({ -6.0, -12.0, -6.0, -5.0 });
        g.lines.push_back({ -6.0, -3.0, -6.0, 3.0 });
        g.lines.push_back({ -6.0, 5.0, -6.0, 12.0 });
        g.lines.push_back({ -6.0, -9.0, 20.0, -20.0 });
        g.lines.push_back({ -6.0, 9.0, 20.0, 20.0 });
        g.lines.push_back({ -6.0, 0.0, 8.0, 0.0 });
        g.lines.push_back({ 8.0, 0.0, 8.0, 14.0 });
        if (kind == ComponentKind::MosfetN) {
            // Arrow pointing inward toward gate (NMOS)
            g.polys.push_back({ { { -5.0, 0.0 }, { 2.0, -3.5 }, { 2.0, 3.5 } }, true, true });
        } else {
            // Arrow pointing outward away from gate (PMOS)
            g.polys.push_back({ { { 5.0, 0.0 }, { -2.0, -3.5 }, { -2.0, 3.5 } }, true, true });
        }
        break;
    }
    case ComponentKind::JfetN:
    case ComponentKind::JfetP: {
        g.circles.push_back({ 0.0, 0.0, 20.0, false });
        g.lines.push_back({ -40.0, 0.0, -6.0, 0.0 });
        // Solid continuous JFET channel
        g.lines.push_back({ -6.0, -12.0, -6.0, 12.0 });
        g.lines.push_back({ -6.0, -9.0, 20.0, -20.0 });
        g.lines.push_back({ -6.0, 9.0, 20.0, 20.0 });
        if (kind == ComponentKind::JfetN) {
            // Arrow pointing into channel
            g.polys.push_back({ { { -7.0, 0.0 }, { -14.0, -3.5 }, { -14.0, 3.5 } }, true, true });
        } else {
            // Arrow pointing out of channel
            g.polys.push_back({ { { -15.0, 0.0 }, { -8.0, -3.5 }, { -8.0, 3.5 } }, true, true });
        }
        break;
    }

    case ComponentKind::Scr:
    case ComponentKind::Triac: {
        g.lines.push_back({ -40.0, 0.0, -10.0, 0.0 });
        g.lines.push_back({ 10.0, 0.0, 40.0, 0.0 });
        g.polys.push_back({ { { -10.0, -10.0 }, { 10.0, 0.0 }, { -10.0, 10.0 } }, true, false });
        g.lines.push_back({ 10.0, -10.0, 10.0, 10.0 });
        g.lines.push_back({ 10.0, 5.0, 0.0, 20.0 });
        if (kind == ComponentKind::Triac) {
            g.polys.push_back({ { { 10.0, -12.0 }, { -10.0, -2.0 }, { 10.0, 8.0 } }, true, false });
        }
        break;
    }

    case ComponentKind::OpAmp:
    case ComponentKind::Comparator: {
        g.lines.push_back({ -40.0, -20.0, -20.0, -20.0 });
        g.lines.push_back({ -40.0, 20.0, -20.0, 20.0 });
        g.lines.push_back({ 24.0, 0.0, 40.0, 0.0 });
        if (std == SymbolStandard::IEC) {
            g.polys.push_back({ { { -20.0, -24.0 }, { 24.0, -24.0 }, { 24.0, 24.0 }, { -20.0, 24.0 } }, true, false });
            g.texts.push_back({ -4.0, -6.0, (kind == ComponentKind::OpAmp ? "oo" : ">"), 8.0 });
        } else {
            g.polys.push_back({ { { -20.0, -26.0 }, { 24.0, 0.0 }, { -20.0, 26.0 } }, true, false });
        }
        g.texts.push_back({ -16.0, -12.0, "+", 7.0 });
        g.texts.push_back({ -16.0, 18.0, "-", 7.0 });
        break;
    }

    case ComponentKind::GateAnd:
    case ComponentKind::GateOr:
    case ComponentKind::GateNot:
    case ComponentKind::GateNand:
    case ComponentKind::GateNor:
    case ComponentKind::GateXor: {
        const bool isInv = (kind == ComponentKind::GateNot || kind == ComponentKind::GateNand || kind == ComponentKind::GateNor);
        if (kind == ComponentKind::GateNot) {
            g.lines.push_back({ -40.0, 0.0, -18.0, 0.0 });
        } else {
            g.lines.push_back({ -40.0, -20.0, -18.0, -20.0 });
            g.lines.push_back({ -40.0, 20.0, -18.0, 20.0 });
        }
        const double outX = isInv ? 24.0 : 18.0;
        g.lines.push_back({ outX, 0.0, 40.0, 0.0 });
        if (std == SymbolStandard::IEC) {
            g.polys.push_back({ { { -18.0, -22.0 }, { 18.0, -22.0 }, { 18.0, 22.0 }, { -18.0, 22.0 } }, true, false });
            std::string symTxt = "&";
            if (kind == ComponentKind::GateOr || kind == ComponentKind::GateNor) symTxt = ">=1";
            else if (kind == ComponentKind::GateNot) symTxt = "1";
            else if (kind == ComponentKind::GateXor) symTxt = "=1";
            g.texts.push_back({ -8.0, 2.0, symTxt, 7.0 });
        } else {
            if (kind == ComponentKind::GateNot) {
                g.polys.push_back({ { { -18.0, -14.0 }, { 16.0, 0.0 }, { -18.0, 14.0 } }, true, false });
            } else if (kind == ComponentKind::GateAnd || kind == ComponentKind::GateNand) {
                g.lines.push_back({ -18.0, -22.0, 0.0, -22.0 });
                g.lines.push_back({ -18.0, 22.0, 0.0, 22.0 });
                g.lines.push_back({ -18.0, -22.0, -18.0, 22.0 });
                g.arcs.push_back({ 0.0, 0.0, 22.0, -90.0, 180.0 });
            } else {
                g.polys.push_back({ { { -18.0, -22.0 }, { 4.0, -18.0 }, { 18.0, 0.0 }, { 4.0, 18.0 }, { -18.0, 22.0 }, { -10.0, 0.0 } }, true, false });
                if (kind == ComponentKind::GateXor) {
                    g.arcs.push_back({ -26.0, 0.0, 22.0, -55.0, 110.0 });
                }
            }
        }
        if (isInv) g.circles.push_back({ 20.0, 0.0, 4.0, false });
        break;
    }

    case ComponentKind::Switch:
    case ComponentKind::PushButton: {
        g.lines.push_back({ -40.0, 0.0, -14.0, 0.0 });
        g.lines.push_back({ 14.0, 0.0, 40.0, 0.0 });
        g.circles.push_back({ -14.0, 0.0, 2.5, false });
        g.circles.push_back({ 14.0, 0.0, 2.5, false });
        g.lines.push_back({ -12.0, -1.0, 12.0, -10.0 });
        if (kind == ComponentKind::PushButton) {
            g.lines.push_back({ 0.0, -14.0, 0.0, -6.0 });
            g.lines.push_back({ -8.0, -14.0, 8.0, -14.0 });
        }
        break;
    }

    case ComponentKind::Fuse: {
        g.lines.push_back({ -40.0, 0.0, -16.0, 0.0 });
        g.lines.push_back({ 16.0, 0.0, 40.0, 0.0 });
        g.polys.push_back({ { { -16.0, -7.0 }, { 16.0, -7.0 }, { 16.0, 7.0 }, { -16.0, 7.0 } }, true, false });
        g.lines.push_back({ -16.0, 0.0, 16.0, 0.0 });
        break;
    }

    case ComponentKind::Voltmeter:
    case ComponentKind::Ammeter:
    case ComponentKind::Oscilloscope:
    case ComponentKind::VoltageProbe:
    case ComponentKind::CurrentProbe:
    case ComponentKind::Motor: {
        g.lines.push_back({ -40.0, 0.0, -16.0, 0.0 });
        g.lines.push_back({ 16.0, 0.0, 40.0, 0.0 });
        g.circles.push_back({ 0.0, 0.0, 16.0, false });
        std::string ch = "V";
        if (kind == ComponentKind::Ammeter || kind == ComponentKind::CurrentProbe) ch = "A";
        else if (kind == ComponentKind::Oscilloscope) ch = "OSC";
        else if (kind == ComponentKind::Motor) ch = "M";
        g.texts.push_back({ -5.0, 3.0, ch, 8.0 });
        break;
    }

    case ComponentKind::Antenna: {
        g.lines.push_back({ -40.0, 0.0, 0.0, 0.0 });
        g.lines.push_back({ 0.0, 0.0, 0.0, -22.0 });
        g.lines.push_back({ -10.0, -22.0, 0.0, -10.0 });
        g.lines.push_back({ 10.0, -22.0, 0.0, -10.0 });
        break;
    }

    case ComponentKind::Wire:
    case ComponentKind::Connector: {
        g.lines.push_back({ -40.0, 0.0, 40.0, 0.0 });
        if (kind == ComponentKind::Connector) {
            g.circles.push_back({ 0.0, 0.0, 4.0, true });
        }
        break;
    }

    default: {
        // Ic, VoltageRegulator, Relay, FlipFlopD, FlipFlopJK, Multiplexer, Demultiplexer
        g.lines.push_back({ -40.0, 0.0, -22.0, 0.0 });
        g.lines.push_back({ 22.0, 0.0, 40.0, 0.0 });
        g.polys.push_back({ { { -22.0, -18.0 }, { 22.0, -18.0 }, { 22.0, 18.0 }, { -22.0, 18.0 } }, true, false });
        std::string tag = "IC";
        if (kind == ComponentKind::VoltageRegulator) tag = "REG";
        else if (kind == ComponentKind::FlipFlopD) tag = "DFF";
        else if (kind == ComponentKind::FlipFlopJK) tag = "JK";
        else if (kind == ComponentKind::Multiplexer) tag = "MUX";
        else if (kind == ComponentKind::Demultiplexer) tag = "DMUX";
        else if (kind == ComponentKind::Relay) tag = "RLY";
        g.texts.push_back({ -10.0, 3.0, tag, 7.0 });
        break;
    }
    }
    return g;
}

inline std::string EscapeXmlText(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 4);
    for (char c : s) {
        if (c == '&') out += "&amp;";
        else if (c == '<') out += "&lt;";
        else if (c == '>') out += "&gt;";
        else if (c == '"') out += "&quot;";
        else out += c;
    }
    return out;
}

inline std::string ExportSchematicToSvg(
    const SchematicDocument& sch,
    const CircuitSolveResult* sol = nullptr)
{
    double minX = 0.0, minY = 0.0, maxX = 560.0, maxY = 260.0;
    if (!sch.symbols.empty()) {
        minX = 1e9; minY = 1e9; maxX = -1e9; maxY = -1e9;
        for (const auto& s : sch.symbols) {
            minX = (std::min)(minX, s.center.x - 70.0);
            maxX = (std::max)(maxX, s.center.x + 70.0);
            minY = (std::min)(minY, s.center.y - 50.0);
            maxY = (std::max)(maxY, s.center.y + 50.0);
        }
    }
    const double vbW = (std::max)(360.0, maxX - minX);
    const double vbH = (std::max)(220.0, maxY - minY);

    std::ostringstream svg;
    svg << "<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"" << minX << " " << minY << " " << vbW << " " << vbH
        << "\" width=\"" << static_cast<int>(vbW) << "\" height=\"" << static_cast<int>(vbH) << "\">\n";
    svg << "<defs>\n"
        << "  <marker id=\"arrow-i\" viewBox=\"0 0 10 10\" refX=\"8\" refY=\"5\" markerWidth=\"6\" markerHeight=\"6\" orient=\"auto\">\n"
        << "    <path d=\"M 0 1 L 10 5 L 0 9 z\" fill=\"#0284c7\"/>\n"
        << "  </marker>\n"
        << "</defs>\n";
    svg << "<rect x=\"" << minX << "\" y=\"" << minY << "\" width=\"" << vbW << "\" height=\"" << vbH
        << "\" fill=\"#f8fafc\" stroke=\"#cbd5e1\"/>\n";
    svg << "<text x=\"" << (minX + 12.0) << "\" y=\"" << (minY + 22.0)
        << "\" font-family=\"Segoe UI\" font-size=\"13\" font-weight=\"bold\" fill=\"#0f172a\">"
        << EscapeXmlText(sch.title) << " (" << sch.symbols.size() << " elementos)</text>\n";

    // Wires
    svg << "<g class=\"wires\" fill=\"none\" stroke=\"#334155\" stroke-width=\"2\" stroke-linecap=\"round\" stroke-linejoin=\"round\">\n";
    for (const auto& w : sch.wires) {
        if (w.vertices.size() < 2) continue;
        svg << "  <polyline points=\"";
        for (std::size_t i = 0; i < w.vertices.size(); ++i) {
            if (i > 0) svg << " ";
            svg << w.vertices[i].x << "," << w.vertices[i].y;
        }
        svg << "\"/>\n";
    }
    for (const auto& j : sch.computedJunctions) {
        svg << "  <circle class=\"junction\" cx=\"" << j.pos.x << "\" cy=\"" << j.pos.y
            << "\" r=\"3.5\" fill=\"#0f172a\" stroke=\"none\"/>\n";
    }
    svg << "</g>\n";

    // Symbols
    svg << "<g class=\"symbols\">\n";
    for (const auto& sym : sch.symbols) {
        const SymbolVectorGeometry geom = GetComponentSymbolGeometry(sym.component.kind, sym.standard);
        svg << "  <g transform=\"translate(" << sym.center.x << "," << sym.center.y << ")";
        if (sym.rotationDeg != 0) svg << " rotate(" << sym.rotationDeg << ")";
        if (sym.flipH || sym.flipV) svg << " scale(" << (sym.flipH ? -1 : 1) << "," << (sym.flipV ? -1 : 1) << ")";
        svg << "\">\n";
        for (const auto& l : geom.lines) {
            svg << "    <line x1=\"" << l.x1 << "\" y1=\"" << l.y1 << "\" x2=\"" << l.x2 << "\" y2=\"" << l.y2
                << "\" stroke=\"#1e293b\" stroke-width=\"2\"/>\n";
        }
        for (const auto& c : geom.circles) {
            svg << "    <circle cx=\"" << c.cx << "\" cy=\"" << c.cy << "\" r=\"" << c.r
                << "\" fill=\"" << (c.filled ? "#1e293b" : "none") << "\" stroke=\"#1e293b\" stroke-width=\"1.8\"/>\n";
        }
        for (const auto& a : geom.arcs) {
            const double a0 = a.startDeg * (kPi / 180.0);
            const double a1 = (a.startDeg + a.sweepDeg) * (kPi / 180.0);
            const double x0 = a.cx + a.r * std::cos(a0);
            const double y0 = a.cy - a.r * std::sin(a0);
            const double x1 = a.cx + a.r * std::cos(a1);
            const double y1 = a.cy - a.r * std::sin(a1);
            const int largeArc = (std::abs(a.sweepDeg) > 180.0) ? 1 : 0;
            const int sweepFlag = (a.sweepDeg >= 0.0) ? 0 : 1;
            svg << "    <path d=\"M " << x0 << " " << y0 << " A " << a.r << " " << a.r << " 0 " << largeArc << " "
                << sweepFlag << " " << x1 << " " << y1 << "\" fill=\"none\" stroke=\"#1e293b\" stroke-width=\"1.8\"/>\n";
        }
        for (const auto& p : geom.polys) {
            if (p.pts.size() < 2) continue;
            svg << "    <" << (p.closed ? "polygon" : "polyline") << " points=\"";
            for (std::size_t pi = 0; pi < p.pts.size(); ++pi) {
                if (pi > 0) svg << " ";
                svg << p.pts[pi].x << "," << p.pts[pi].y;
            }
            svg << "\" fill=\"" << (p.filled ? "#1e293b" : "#eff6ff")
                << "\" stroke=\"#1e3a8a\" stroke-width=\"1.8\"/>\n";
        }
        for (const auto& t : geom.texts) {
            svg << "    <text x=\"" << t.x << "\" y=\"" << t.y << "\" font-family=\"Consolas\" font-size=\"" << t.height
                << "\" fill=\"#0f172a\">" << EscapeXmlText(t.text) << "</text>\n";
        }
        svg << "  </g>\n";
        svg << "  <text x=\"" << sym.center.x << "\" y=\"" << (sym.center.y - 24.0)
            << "\" text-anchor=\"middle\" font-family=\"Consolas\" font-size=\"11\" font-weight=\"bold\" fill=\"#1e3a8a\">"
            << EscapeXmlText(sym.component.name) << "</text>\n";
    }
    svg << "</g>\n";

    if (sol && sol->ok) {
        const SchematicOverlayData ov = BuildSchematicOverlay(sch, *sol);
        svg << "<g class=\"overlays\">\n";
        for (const auto& arr : ov.currentArrows) {
            svg << "  <line x1=\"" << arr.pStart.x << "\" y1=\"" << (arr.pStart.y + 16.0)
                << "\" x2=\"" << arr.pEnd.x << "\" y2=\"" << (arr.pEnd.y + 16.0)
                << "\" stroke=\"#0284c7\" stroke-width=\"" << (1.5 + arr.normIntensity * 2.0)
                << "\" marker-end=\"url(#arrow-i)\"/>\n";
            svg << "  <text x=\"" << (0.5 * (arr.pStart.x + arr.pEnd.x)) << "\" y=\"" << (arr.pStart.y + 28.0)
                << "\" text-anchor=\"middle\" font-family=\"Consolas\" font-size=\"10\" fill=\"#0369a1\">"
                << EscapeXmlText(arr.labelUtf8) << "</text>\n";
        }
        for (const auto& badge : ov.polarityBadges) {
            const char* fill = (badge.polaritySign > 0) ? "#2563eb" : (badge.polaritySign < 0 ? "#d97706" : "#475569");
            svg << "  <text x=\"" << badge.anchor.x << "\" y=\"" << (badge.anchor.y - 8.0)
                << "\" font-family=\"Consolas\" font-size=\"10\" font-weight=\"bold\" fill=\"" << fill << "\">"
                << EscapeXmlText(badge.badgeUtf8) << "</text>\n";
        }
        svg << "</g>\n";
    }

    svg << "</svg>\n";
    return svg.str();
}

inline std::string ExportSchematicToDxf(
    const SchematicDocument& sch,
    const CircuitSolveResult* sol = nullptr)
{
    std::ostringstream dxf;
    dxf << std::setprecision(6);

    // 1. HEADER section
    dxf << "0\nSECTION\n2\nHEADER\n"
        << "9\n$ACADVER\n1\nAC1009\n"
        << "9\n$INSUNITS\n70\n4\n"
        << "0\nENDSEC\n";

    // 2. TABLES section (LAYERS)
    dxf << "0\nSECTION\n2\nTABLES\n"
        << "0\nTABLE\n2\nLAYER\n70\n5\n";
    auto emitLayer = [&](const char* name, int color) {
        dxf << "0\nLAYER\n2\n" << name << "\n70\n0\n62\n" << color << "\n6\nCONTINUOUS\n";
    };
    emitLayer("SYMBOLS", 7);
    emitLayer("WIRES", 5);
    emitLayer("LABELS", 3);
    emitLayer("VALUES", 2);
    emitLayer("OVERLAYS", 1);
    dxf << "0\nENDTAB\n0\nENDSEC\n";

    // 3. BLOCKS section (one reusable BLOCK per used (kind, standard))
    dxf << "0\nSECTION\n2\nBLOCKS\n";
    std::set<std::pair<int, int>> emittedBlocks;
    auto blockNameFor = [](ComponentKind k, SymbolStandard st) {
        return "SYM_" + std::to_string(static_cast<int>(k)) + "_" + (st == SymbolStandard::IEC ? "IEC" : "ANSI");
    };
    for (const auto& sym : sch.symbols) {
        const std::pair<int, int> key{ static_cast<int>(sym.component.kind), static_cast<int>(sym.standard) };
        if (!emittedBlocks.insert(key).second) continue;
        const std::string bname = blockNameFor(sym.component.kind, sym.standard);
        const SymbolVectorGeometry geom = GetComponentSymbolGeometry(sym.component.kind, sym.standard);
        dxf << "0\nBLOCK\n8\nSYMBOLS\n2\n" << bname << "\n70\n0\n10\n0.0\n20\n0.0\n30\n0.0\n";
        for (const auto& l : geom.lines) {
            dxf << "0\nLINE\n8\nSYMBOLS\n10\n" << l.x1 << "\n20\n" << -l.y1
                << "\n11\n" << l.x2 << "\n21\n" << -l.y2 << "\n";
        }
        for (const auto& c : geom.circles) {
            dxf << "0\nCIRCLE\n8\nSYMBOLS\n10\n" << c.cx << "\n20\n" << -c.cy << "\n40\n" << c.r << "\n";
        }
        for (const auto& a : geom.arcs) {
            auto normDeg = [](double d) {
                double r = std::fmod(d, 360.0);
                if (r < 0.0) r += 360.0;
                return r;
            };
            const double startA = normDeg(a.startDeg);
            const double endA = normDeg(a.startDeg + a.sweepDeg);
            dxf << "0\nARC\n8\nSYMBOLS\n10\n" << a.cx << "\n20\n" << -a.cy << "\n40\n" << a.r
                << "\n50\n" << startA << "\n51\n" << endA << "\n";
        }
        for (const auto& p : geom.polys) {
            for (std::size_t pi = 1; pi < p.pts.size(); ++pi) {
                dxf << "0\nLINE\n8\nSYMBOLS\n10\n" << p.pts[pi - 1].x << "\n20\n" << -p.pts[pi - 1].y
                    << "\n11\n" << p.pts[pi].x << "\n21\n" << -p.pts[pi].y << "\n";
            }
            if (p.closed && p.pts.size() >= 3) {
                dxf << "0\nLINE\n8\nSYMBOLS\n10\n" << p.pts.back().x << "\n20\n" << -p.pts.back().y
                    << "\n11\n" << p.pts.front().x << "\n21\n" << -p.pts.front().y << "\n";
            }
        }
        dxf << "0\nENDBLK\n";
    }
    dxf << "0\nENDSEC\n";

    // 4. ENTITIES section
    dxf << "0\nSECTION\n2\nENTITIES\n";
    for (const auto& w : sch.wires) {
        for (std::size_t vi = 1; vi < w.vertices.size(); ++vi) {
            dxf << "0\nLINE\n8\nWIRES\n10\n" << w.vertices[vi - 1].x << "\n20\n" << -w.vertices[vi - 1].y
                << "\n11\n" << w.vertices[vi].x << "\n21\n" << -w.vertices[vi].y << "\n";
        }
    }
    for (const auto& j : sch.computedJunctions) {
        dxf << "0\nCIRCLE\n8\nWIRES\n10\n" << j.pos.x << "\n20\n" << -j.pos.y << "\n40\n2.5\n";
    }
    std::map<std::string, const BranchResult*> branchByName;
    if (sol && sol->ok) {
        for (const auto& br : sol->branches) branchByName[br.name] = &br;
    }
    for (std::size_t i = 0; i < sch.symbols.size(); ++i) {
        const auto& sym = sch.symbols[i];
        dxf << "0\nINSERT\n8\nSYMBOLS\n2\n" << blockNameFor(sym.component.kind, sym.standard)
            << "\n10\n" << sym.center.x << "\n20\n" << -sym.center.y
            << "\n41\n" << (sym.flipH ? -1.0 : 1.0) << "\n42\n" << (sym.flipV ? -1.0 : 1.0)
            << "\n50\n" << sym.rotationDeg << "\n";
        dxf << "0\nTEXT\n8\nLABELS\n10\n" << sym.center.x << "\n20\n" << (-sym.center.y + 24.0)
            << "\n40\n5.0\n1\n" << sym.component.name << "\n";
        dxf << "0\nTEXT\n8\nVALUES\n10\n" << sym.center.x << "\n20\n" << (-sym.center.y - 26.0)
            << "\n40\n4.0\n1\n" << sym.component.value << "\n";
        if (sol && sol->ok && sym.component.active) {
            auto itBr = branchByName.find(sym.component.name);
            if (itBr != branchByName.end()) {
                dxf << "0\nTEXT\n8\nOVERLAYS\n10\n" << (sym.center.x + 15.0) << "\n20\n" << (-sym.center.y - 34.0)
                    << "\n40\n3.5\n1\nI=" << itBr->second->currentA.real() << "A\n";
            }
        }
    }
    dxf << "0\nENDSEC\n0\nEOF\n";
    return dxf.str();
}

inline std::string EscapeSExprString(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 4);
    for (char c : s) {
        if (c == '"' || c == '\\') out += '\\';
        out += c;
    }
    return out;
}

inline std::string ExportCircuitToKiCadSExpr(
    const std::vector<CircuitComponent>& comps,
    const std::string& title = "DocStudioCircuit")
{
    auto kicadLibPart = [](const CircuitComponent& c) -> std::pair<std::string, std::string> {
        switch (c.kind) {
        case ComponentKind::Resistor: return { "Device", "R" };
        case ComponentKind::Potentiometer: return { "Device", "R_Potentiometer" };
        case ComponentKind::Capacitor: return { "Device", "C" };
        case ComponentKind::PolarizedCapacitor: return { "Device", "C_Polarized" };
        case ComponentKind::Inductor: return { "Device", "L" };
        case ComponentKind::Crystal: return { "Device", "Crystal" };
        case ComponentKind::Diode:
        case ComponentKind::SchottkyDiode: return { "Device", "D" };
        case ComponentKind::ZenerDiode: return { "Device", "D_Zener" };
        case ComponentKind::Led: return { "Device", "LED" };
        case ComponentKind::Photodiode: return { "Device", "D_Photo" };
        case ComponentKind::BjtNpn: return { "Transistor_BJT", "Q_NPN_CBE" };
        case ComponentKind::BjtPnp: return { "Transistor_BJT", "Q_PNP_CBE" };
        case ComponentKind::MosfetN: return { "Transistor_FET", "Q_NMOS_GDS" };
        case ComponentKind::MosfetP: return { "Transistor_FET", "Q_PMOS_GDS" };
        case ComponentKind::JfetN: return { "Transistor_FET", "Q_NJFET_DGS" };
        case ComponentKind::JfetP: return { "Transistor_FET", "Q_PJFET_DGS" };
        case ComponentKind::Scr: return { "Thyristor", "SCR" };
        case ComponentKind::Triac: return { "Triac", "TRIAC" };
        case ComponentKind::OpAmp: return { "Amplifier_Operational", c.modelName.empty() ? "LM358" : c.modelName };
        case ComponentKind::Comparator: return { "Comparator", "LM393" };
        case ComponentKind::VoltageRegulator: return { "Regulator_Linear", c.modelName.empty() ? "L7805" : c.modelName };
        case ComponentKind::Transformer: return { "Device", "Transformer_1P_1S" };
        case ComponentKind::Switch:
        case ComponentKind::PushButton: return { "Switch", "SW_SPST" };
        case ComponentKind::Relay: return { "Relay", "Relay_SPST" };
        case ComponentKind::Fuse: return { "Device", "Fuse" };
        case ComponentKind::Ground: return { "power", "GND" };
        case ComponentKind::Vcc: return { "power", "VCC" };
        case ComponentKind::VoltageSource:
        case ComponentKind::AcSource:
        case ComponentKind::PulseSource:
        case ComponentKind::Battery: return { "Simulation_SPICE", "Vsource" };
        case ComponentKind::CurrentSource: return { "Simulation_SPICE", "Isource" };
        default: return { "Device", "Generic" };
        }
    };

    std::ostringstream sx;
    sx << "(export (version \"D\")\n"
       << "  (design\n"
       << "    (source \"" << EscapeSExprString(title) << "\")\n"
       << "    (tool \"Document Studio MNA\"))\n"
       << "  (components\n";

    std::map<int, std::vector<std::pair<std::string, std::string>>> netPins;
    for (const auto& c : comps) {
        if (!c.active) continue;
        const auto [lib, part] = kicadLibPart(c);
        const std::string valStr = !c.modelName.empty() ? c.modelName : std::to_string(c.value);
        const std::string escName = EscapeSExprString(c.name);
        sx << "    (comp (ref \"" << escName << "\")\n"
           << "      (value \"" << EscapeSExprString(valStr) << "\")\n"
           << "      (libsource (lib \"" << EscapeSExprString(lib) << "\") (part \"" << EscapeSExprString(part)
           << "\") (description \"" << EscapeSExprString(ComponentKindLabel(c.kind, false)) << "\")))\n";

        if (c.kind == ComponentKind::Ground || c.kind == ComponentKind::Vcc || c.kind == ComponentKind::Antenna) {
            netPins[c.nodePos].push_back({ escName, "1" });
        } else if (c.kind == ComponentKind::GateNot) {
            netPins[c.nodePos].push_back({ escName, "1" });
            netPins[c.nodeOut].push_back({ escName, "2" });
        } else if (c.kind == ComponentKind::OpAmp || c.kind == ComponentKind::Comparator ||
                   c.kind == ComponentKind::Potentiometer || c.kind == ComponentKind::VoltageRegulator ||
                   c.kind == ComponentKind::Ic ||
                   (c.kind >= ComponentKind::GateAnd && c.kind <= ComponentKind::GateXor)) {
            netPins[c.nodePos].push_back({ escName, "1" });
            netPins[c.nodeNeg].push_back({ escName, "2" });
            netPins[c.nodeOut].push_back({ escName, "3" });
        } else if (c.kind == ComponentKind::BjtNpn || c.kind == ComponentKind::BjtPnp ||
                   c.kind == ComponentKind::MosfetN || c.kind == ComponentKind::MosfetP ||
                   c.kind == ComponentKind::JfetN || c.kind == ComponentKind::JfetP ||
                   c.kind == ComponentKind::Scr || c.kind == ComponentKind::Triac) {
            netPins[c.nodePos].push_back({ escName, "1" });
            netPins[c.nodeOut].push_back({ escName, "2" });
            netPins[c.nodeNeg].push_back({ escName, "3" });
        } else if (c.kind == ComponentKind::Transformer) {
            netPins[c.nodePos].push_back({ escName, "1" });
            netPins[c.nodeNeg].push_back({ escName, "2" });
            netPins[c.nodeOut].push_back({ escName, "3" });
            netPins[c.nodeCtrlPos].push_back({ escName, "4" });
        } else if (c.kind == ComponentKind::Vcvs || c.kind == ComponentKind::Vccs || c.kind == ComponentKind::Relay) {
            netPins[c.nodePos].push_back({ escName, "1" });
            netPins[c.nodeNeg].push_back({ escName, "2" });
            netPins[c.nodeCtrlPos].push_back({ escName, "3" });
            netPins[c.nodeCtrlNeg].push_back({ escName, "4" });
        } else if (c.kind == ComponentKind::FlipFlopD) {
            netPins[c.nodePos].push_back({ escName, "1" });
            netPins[c.nodeCtrlPos].push_back({ escName, "2" });
            netPins[c.nodeOut].push_back({ escName, "3" });
            netPins[c.nodeCtrlNeg].push_back({ escName, "4" });
        } else if (c.kind == ComponentKind::FlipFlopJK) {
            netPins[c.nodePos].push_back({ escName, "1" });
            netPins[c.nodeCtrlNeg].push_back({ escName, "2" });
            netPins[c.nodeCtrlPos].push_back({ escName, "3" });
            netPins[c.nodeOut].push_back({ escName, "4" });
        } else if (c.kind == ComponentKind::Multiplexer || c.kind == ComponentKind::Demultiplexer) {
            netPins[c.nodePos].push_back({ escName, "1" });
            netPins[c.nodeNeg].push_back({ escName, "2" });
            netPins[c.nodeOut].push_back({ escName, "3" });
            netPins[c.nodeCtrlPos].push_back({ escName, "4" });
        } else {
            netPins[c.nodePos].push_back({ escName, "1" });
            netPins[c.nodeNeg].push_back({ escName, "2" });
        }
    }
    sx << "  )\n  (nets\n";
    for (const auto& kv : netPins) {
        const std::string netName = (kv.first == 0) ? "GND" : ("Net-(N" + std::to_string(kv.first) + ")");
        sx << "    (net (code \"" << kv.first << "\") (name \"" << netName << "\")";
        for (const auto& np : kv.second) {
            sx << " (node (ref \"" << np.first << "\") (pin \"" << np.second << "\"))";
        }
        sx << ")\n";
    }
    sx << "  )\n)\n";
    return sx.str();
}

inline std::string ExportCircuitToSpiceCir(
    const std::vector<CircuitComponent>& comps,
    const std::string& title = "DocStudioCircuit",
    const std::string& analysisDirective = ".op")
{
    std::ostringstream cir;
    cir << std::setprecision(15);
    auto emitParams = [&](const CircuitComponent& comp) {
        for (const auto& kv : comp.params) {
            if (!kv.first.empty() && kv.first.find(' ') == std::string::npos && std::isfinite(kv.second)) {
                cir << " " << kv.first << "=" << kv.second;
            }
        }
    };
    cir << "* " << title << " (SPICE .cir Export - Document Studio)\n";
    cir << ".model D_DEFAULT D(Is=1e-12 N=1.35)\n"
        << ".model DS_DEFAULT D(Is=1e-6 N=1.08)\n"
        << ".model D1N4148 D(Is=2.52e-9 N=1.752 Rs=0.568)\n"
        << ".model D1N5819 D(Is=2e-5 N=1.08 Rs=0.1)\n"
        << ".model DZ5V1 D(Is=1e-11 Bv=5.1 Ibv=1e-3)\n"
        << ".model Q2N2222 NPN(Is=1.4e-14 Bf=200 Vaf=100)\n"
        << ".model Q2N2907 PNP(Is=1.4e-14 Bf=180 Vaf=90)\n"
        << ".model MIRF540 NMOS(Vto=3.5 Kp=8.5 Lambda=0.02)\n"
        << ".model MIRF9540 PMOS(Vto=-3.5 Kp=6.0 Lambda=0.02)\n";

    for (const auto& c : comps) {
        if (!c.active) continue;
        if (c.kind == ComponentKind::Ground) continue;
        if (c.kind == ComponentKind::OpAmp) {
            cir << "X" << c.name << " N" << c.nodePos << " N" << c.nodeNeg << " N" << c.nodeOut << " "
                << (c.modelName.empty() ? "OPAMP_IDEAL" : c.modelName);
            emitParams(c);
            cir << "\n";
        } else if (c.kind == ComponentKind::Comparator) {
            cir << "COMP " << c.name << " N" << c.nodePos << " N" << c.nodeNeg << " N" << c.nodeOut;
            emitParams(c);
            cir << "\n";
        } else if (c.kind == ComponentKind::VoltageRegulator) {
            cir << "REG " << c.name << " N" << c.nodePos << " N" << c.nodeNeg << " N" << c.nodeOut << " "
                << (c.modelName.empty() ? "7805" : c.modelName);
            emitParams(c);
            cir << "\n";
        } else if (c.kind == ComponentKind::Potentiometer) {
            cir << "POT " << c.name << " N" << c.nodePos << " N" << c.nodeNeg << " N" << c.nodeOut << " "
                << c.value << " " << (c.auxValue > 0.0 ? c.auxValue : 0.5) << "\n";
        } else if (c.kind == ComponentKind::Transformer) {
            cir << "XFMR " << c.name << " N" << c.nodePos << " N" << c.nodeNeg << " N" << c.nodeOut << " N"
                << c.nodeCtrlPos << " " << c.value << " " << (c.auxValue > 0.0 ? c.auxValue : 1.0) << "\n";
        } else if (c.kind == ComponentKind::Relay) {
            cir << "RLY " << c.name << " N" << c.nodePos << " N" << c.nodeNeg << " N" << c.nodeCtrlPos << " N"
                << c.nodeCtrlNeg << " " << c.value << "\n";
        } else if (c.kind == ComponentKind::Diode || c.kind == ComponentKind::Led ||
                   c.kind == ComponentKind::ZenerDiode || c.kind == ComponentKind::SchottkyDiode ||
                   c.kind == ComponentKind::Photodiode) {
            const char* kw = (c.kind == ComponentKind::Led) ? "LED "
                : (c.kind == ComponentKind::ZenerDiode ? "DZ "
                : (c.kind == ComponentKind::SchottkyDiode ? "DS "
                : (c.kind == ComponentKind::Photodiode ? "PD " : "")));
            const char* defMod = (c.kind == ComponentKind::ZenerDiode) ? "DZ5V1"
                : (c.kind == ComponentKind::SchottkyDiode ? "DS_DEFAULT"
                : (c.kind == ComponentKind::Diode ? "D_DEFAULT" : "D1N4148"));
            cir << kw << c.name << " N" << c.nodePos << " N" << c.nodeNeg << " ";
            if (c.kind == ComponentKind::ZenerDiode) {
                const double resolvedVz = Detail::GetParam(c, "Vz", (c.value > 0.5 && c.value < 200.0) ? c.value : 5.1);
                cir << resolvedVz;
                if (!c.modelName.empty()) cir << " " << c.modelName;
            } else if (c.kind == ComponentKind::Photodiode) {
                cir << c.value;
                if (!c.modelName.empty()) cir << " " << c.modelName;
            } else if (c.kind == ComponentKind::Diode && c.modelName.empty() && c.value > 0.0 && c.value < 1.0 &&
                       std::abs(c.value - 1e-12) > 1e-18) {
                cir << c.value;
            } else {
                cir << (c.modelName.empty() ? defMod : c.modelName);
            }
            emitParams(c);
            cir << "\n";
        } else if (c.kind == ComponentKind::BjtNpn || c.kind == ComponentKind::BjtPnp) {
            cir << c.name << " N" << c.nodePos << " N" << c.nodeOut << " N" << c.nodeNeg << " "
                << (c.modelName.empty() ? (c.kind == ComponentKind::BjtNpn ? "Q2N2222" : "Q2N2907") : c.modelName);
            emitParams(c);
            cir << "\n";
        } else if (c.kind == ComponentKind::MosfetN || c.kind == ComponentKind::MosfetP) {
            cir << c.name << " N" << c.nodePos << " N" << c.nodeOut << " N" << c.nodeNeg << " N" << c.nodeNeg << " "
                << (c.modelName.empty() ? (c.kind == ComponentKind::MosfetN ? "MIRF540" : "MIRF9540") : c.modelName);
            emitParams(c);
            cir << "\n";
        } else if (c.kind == ComponentKind::JfetN || c.kind == ComponentKind::JfetP) {
            cir << c.name << " N" << c.nodePos << " N" << c.nodeOut << " N" << c.nodeNeg << " "
                << (c.modelName.empty() ? (c.kind == ComponentKind::JfetN ? "NJF" : "PJF") : c.modelName);
            emitParams(c);
            cir << "\n";
        } else if (c.kind == ComponentKind::Scr || c.kind == ComponentKind::Triac) {
            cir << (c.kind == ComponentKind::Scr ? "SCR " : "TRIAC ") << c.name
                << " N" << c.nodePos << " N" << c.nodeNeg << " N" << c.nodeOut << "\n";
        } else if (c.kind == ComponentKind::Vcvs || c.kind == ComponentKind::Vccs) {
            cir << c.name << " N" << c.nodePos << " N" << c.nodeNeg << " N" << c.nodeCtrlPos << " N" << c.nodeCtrlNeg
                << " " << (c.auxValue != 0.0 ? c.auxValue : c.value) << "\n";
        } else if (c.kind == ComponentKind::Vcc) {
            cir << "VCC " << c.name << " N" << c.nodePos << " " << c.value << "\n";
        } else if (c.kind == ComponentKind::Battery) {
            cir << "BAT " << c.name << " N" << c.nodePos << " N" << c.nodeNeg << " " << c.value << "\n";
        } else if (c.kind == ComponentKind::AcSource) {
            const char* wfTag = (c.waveform == WaveformKind::Square) ? " SQUARE "
                : (c.waveform == WaveformKind::Triangle ? " TRIANGLE "
                : (c.waveform == WaveformKind::Pulse ? " PULSE " : " SINE "));
            cir << "VAC " << c.name << " N" << c.nodePos << " N" << c.nodeNeg;
            if (c.waveform == WaveformKind::Dc) {
                cir << " " << c.value << " dcOffset=" << c.dcOffset << " freq=" << c.freqHz << " phase=" << c.phaseDeg << "\n";
            } else {
                cir << wfTag << c.value << " " << c.freqHz << " dcOffset=" << c.dcOffset << " phase=" << c.phaseDeg << "\n";
            }
        } else if (c.kind == ComponentKind::PulseSource) {
            const char* wfTag = (c.waveform == WaveformKind::Square) ? " SQUARE "
                : (c.waveform == WaveformKind::Triangle ? " TRIANGLE "
                : (c.waveform == WaveformKind::Sine ? " SINE " : " PULSE "));
            cir << "VPULSE " << c.name << " N" << c.nodePos << " N" << c.nodeNeg << wfTag
                << c.value << " " << c.freqHz << " dcOffset=" << c.dcOffset
                << " duty=" << c.dutyCycle << " phase=" << c.phaseDeg << "\n";
        } else if (c.kind == ComponentKind::Ic) {
            cir << "IC " << c.name << " N" << c.nodePos << " N" << c.nodeNeg << " N" << c.nodeOut;
            if (!c.modelName.empty()) cir << " " << c.modelName;
            cir << "\n";
        } else if (c.kind == ComponentKind::PolarizedCapacitor) {
            cir << "CPOL " << c.name << " N" << c.nodePos << " N" << c.nodeNeg << " " << c.value << "\n";
        } else if (c.kind == ComponentKind::Switch || c.kind == ComponentKind::PushButton) {
            cir << (c.kind == ComponentKind::Switch ? "SW " : "PB ") << c.name
                << " N" << c.nodePos << " N" << c.nodeNeg << " " << (c.auxValue > 0.5 ? "1" : "0") << "\n";
        } else if (c.kind == ComponentKind::Fuse) {
            cir << "FUSE " << c.name << " N" << c.nodePos << " N" << c.nodeNeg << " " << c.value << "\n";
        } else if (c.kind == ComponentKind::Motor) {
            cir << "MOT " << c.name << " N" << c.nodePos << " N" << c.nodeNeg << " " << c.value << "\n";
        } else if (c.kind == ComponentKind::Antenna) {
            cir << "ANT " << c.name << " N" << c.nodePos << " N" << c.nodeNeg << " " << c.value << "\n";
        } else if (c.kind == ComponentKind::Ldr) {
            cir << "LDR " << c.name << " N" << c.nodePos << " N" << c.nodeNeg << " " << c.value << "\n";
        } else if (c.kind == ComponentKind::NtcThermistor) {
            cir << "NTC " << c.name << " N" << c.nodePos << " N" << c.nodeNeg << " " << c.value << "\n";
        } else if (c.kind == ComponentKind::Crystal) {
            cir << "XTAL " << c.name << " N" << c.nodePos << " N" << c.nodeNeg << " " << c.value << "\n";
        } else if (c.kind == ComponentKind::Voltmeter || c.kind == ComponentKind::Ammeter ||
                   c.kind == ComponentKind::Oscilloscope || c.kind == ComponentKind::VoltageProbe ||
                   c.kind == ComponentKind::CurrentProbe) {
            const char* kw = (c.kind == ComponentKind::Voltmeter) ? "VM "
                : (c.kind == ComponentKind::Ammeter ? "AM "
                : (c.kind == ComponentKind::Oscilloscope ? "OSC "
                : (c.kind == ComponentKind::VoltageProbe ? "VP " : "IP ")));
            cir << kw << c.name << " N" << c.nodePos << " N" << c.nodeNeg << "\n";
        } else if (c.kind == ComponentKind::GateNot) {
            cir << "NOT " << c.name << " N" << c.nodePos << " N" << c.nodeOut << "\n";
        } else if (c.kind >= ComponentKind::GateAnd && c.kind <= ComponentKind::GateXor) {
            const char* kw = (c.kind == ComponentKind::GateAnd) ? "AND "
                : (c.kind == ComponentKind::GateOr ? "OR "
                : (c.kind == ComponentKind::GateNand ? "NAND "
                : (c.kind == ComponentKind::GateNor ? "NOR " : "XOR ")));
            cir << kw << c.name << " N" << c.nodePos << " N" << c.nodeNeg << " N" << c.nodeOut << "\n";
        } else if (c.kind == ComponentKind::FlipFlopD) {
            cir << "FF_D " << c.name << " N" << c.nodePos << " N" << c.nodeCtrlPos << " N" << c.nodeOut << "\n";
        } else if (c.kind == ComponentKind::FlipFlopJK) {
            cir << "FF_JK " << c.name << " N" << c.nodePos << " N" << c.nodeCtrlNeg << " N" << c.nodeCtrlPos
                << " N" << c.nodeOut << "\n";
        } else if (c.kind == ComponentKind::Multiplexer || c.kind == ComponentKind::Demultiplexer) {
            cir << (c.kind == ComponentKind::Multiplexer ? "MUX " : "DMUX ") << c.name
                << " N" << c.nodePos << " N" << c.nodeNeg << " N" << c.nodeOut << " N" << c.nodeCtrlPos << "\n";
        } else if (c.kind == ComponentKind::Wire || c.kind == ComponentKind::Connector) {
            cir << (c.kind == ComponentKind::Wire ? "WIRE " : "CONN ") << c.name
                << " N" << c.nodePos << " N" << c.nodeNeg << "\n";
        } else {
            cir << c.name << " N" << c.nodePos << " N" << c.nodeNeg << " " << c.value << "\n";
        }
    }
    if (!analysisDirective.empty()) cir << analysisDirective << "\n";
    cir << ".end\n";
    return cir.str();
}

} // namespace DocStudio::Circuit
