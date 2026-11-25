#pragma once

#include "circuit_models.hpp"
#include "circuit_schematic.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <complex>
#include <iomanip>
#include <limits>
#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace DocStudio::Circuit {

// -------- 1. Engineering Toolbox (tinta_nucleo/ingenieria.cpp port) --------

namespace Toolbox {

inline double VoltageDivider(double vIn, double rTop, double rBottom) noexcept {
    return (rTop >= 0.0 && rBottom >= 0.0 && (rTop + rBottom) > 0.0)
        ? (vIn * rBottom / (rTop + rBottom))
        : std::numeric_limits<double>::quiet_NaN();
}

inline double OpAmpNonInvertingGain(double rFeedback, double rGround) noexcept {
    return (rGround > 0.0) ? (1.0 + rFeedback / rGround) : std::numeric_limits<double>::quiet_NaN();
}

inline double OpAmpInvertingGain(double rInput, double rFeedback) noexcept {
    return (rInput > 0.0) ? (-rFeedback / rInput) : std::numeric_limits<double>::quiet_NaN();
}

inline double RcCutoffHz(double rOhm, double cFarad) noexcept {
    return (rOhm > 0.0 && cFarad > 0.0)
        ? (1.0 / (2.0 * kPi * rOhm * cFarad))
        : std::numeric_limits<double>::quiet_NaN();
}

inline double LcResonanceHz(double lHenry, double cFarad) noexcept {
    return (lHenry > 0.0 && cFarad > 0.0)
        ? (1.0 / (2.0 * kPi * std::sqrt(lHenry * cFarad)))
        : std::numeric_limits<double>::quiet_NaN();
}

inline double RcTimeConstantSec(double rOhm, double cFarad) noexcept {
    return rOhm * cFarad;
}

inline double ToDecibels(double ratio, bool isPower = false) noexcept {
    return (ratio > 0.0)
        ? ((isPower ? 10.0 : 20.0) * std::log10(ratio))
        : -std::numeric_limits<double>::infinity();
}

inline double FromDecibels(double db, bool isPower = false) noexcept {
    return std::pow(10.0, db / (isPower ? 10.0 : 20.0));
}

inline std::string FormatEngineeringNotation(double value, const std::string& unit, int sigFigs = 4) {
    static const std::map<int, std::string> prefixes = {
        { -12, "p" }, { -9, "n" }, { -6, "u" }, { -3, "m" },
        { 0, "" }, { 3, "k" }, { 6, "M" }, { 9, "G" }, { 12, "T" }
    };
    if (value == 0.0) return "0 " + unit;
    int exp3 = static_cast<int>(std::floor(std::log10(std::abs(value)) / 3.0)) * 3;
    exp3 = std::clamp(exp3, -12, 12);
    std::ostringstream oss;
    oss << std::setprecision(std::clamp(sigFigs, 1, 12))
        << (value / std::pow(10.0, exp3)) << " " << prefixes.at(exp3) << unit;
    return oss.str();
}

} // namespace Toolbox

// -------- 2. Active-Aware Thevenin / Norton Port --------

struct TheveninNortonResult {
    bool ok = false;
    double vTheveninV = 0.0;
    double rTheveninOhm = 0.0;
    double iNortonA = 0.0;
    std::complex<double> vTheveninAcV{ 0.0, 0.0 };
    std::complex<double> zTheveninAcOhm{ 0.0, 0.0 };
    std::complex<double> iNortonAcA{ 0.0, 0.0 };
    std::string errorUtf8;
};

inline TheveninNortonResult ComputeTheveninNorton(
    const std::vector<CircuitComponent>& comps,
    int nodeA,
    int nodeB,
    double freqHz = 0.0)
{
    TheveninNortonResult res;
    if (comps.empty() || nodeA == nodeB || nodeA < 0 || nodeB < 0) {
        res.errorUtf8 = "Puerto de Thevenin/Norton invalido.";
        return res;
    }

    const CircuitSolveResult openSol = SolveMnaCircuit(comps, freqHz);
    if (!openSol.ok) {
        res.errorUtf8 = openSol.errorUtf8.empty() ? "No se pudo resolver el circuito abierto." : openSol.errorUtf8;
        return res;
    }

    auto getNodeV = [](const CircuitSolveResult& s, int n) -> std::complex<double> {
        if (n == 0) return { 0.0, 0.0 };
        auto it = s.nodeVoltages.find(n);
        return (it != s.nodeVoltages.end()) ? it->second : std::complex<double>{ 0.0, 0.0 };
    };

    const std::complex<double> vOpen = getNodeV(openSol, nodeA) - getNodeV(openSol, nodeB);
    res.vTheveninAcV = vOpen;
    res.vTheveninV = vOpen.real();

    // Solve DC operating point if freqHz > 0 for accurate small-signal linearization of nonlinear devices
    const CircuitSolveResult dcBias = (freqHz > 0.0) ? SolveMnaCircuit(comps, 0.0) : openSol;

    std::vector<CircuitComponent> testComps;
    testComps.reserve(comps.size() + 4);
    int nextAuxNode = 0;
    for (const auto& c : comps) {
        if (!c.active) continue;
        nextAuxNode = (std::max)(nextAuxNode, (std::max)(c.nodePos, (std::max)(c.nodeNeg, c.nodeOut)));
        nextAuxNode = (std::max)(nextAuxNode, (std::max)(c.nodeCtrlPos, c.nodeCtrlNeg));
        for (int en : c.extraNodes) nextAuxNode = (std::max)(nextAuxNode, en);
    }
    for (const auto& c : comps) {
        if (!c.active) continue;
        if (c.kind == ComponentKind::VoltageSource || c.kind == ComponentKind::Battery ||
            c.kind == ComponentKind::AcSource || c.kind == ComponentKind::Vcc ||
            c.kind == ComponentKind::PulseSource) {
            CircuitComponent sc;
            sc.kind = ComponentKind::Resistor;
            sc.name = c.name + "_short";
            sc.nodePos = c.nodePos;
            sc.nodeNeg = (c.kind == ComponentKind::Vcc) ? 0 : c.nodeNeg;
            sc.value = 1e-9;
            testComps.push_back(std::move(sc));
        } else if (c.kind == ComponentKind::CurrentSource) {
            // Open circuit: omit independent current source
        } else if (c.kind == ComponentKind::Diode || c.kind == ComponentKind::Led ||
                   c.kind == ComponentKind::SchottkyDiode || c.kind == ComponentKind::ZenerDiode ||
                   c.kind == ComponentKind::Photodiode) {
            double isSat = 1e-12;
            double nFactor = 1.35;
            double vz = 0.0;
            double iPhoto = 0.0;
            Detail::GetDiodeSpec(c, isSat, nFactor, vz, iPhoto);
            (void)iPhoto;
            const double vt = 0.02585 * nFactor;
            const double vd = std::clamp((getNodeV(dcBias, c.nodePos) - getNodeV(dcBias, c.nodeNeg)).real(), -60.0, 2.8);
            double gd = (std::max)(1e-12, (isSat / vt) * std::exp(std::clamp(vd / vt, -50.0, 65.0)));
            if (vz > 0.5) {
                gd += (1e-3 / vt) * std::exp(std::clamp(-(vd + vz) / vt, -50.0, 65.0));
            }
            CircuitComponent eqR;
            eqR.kind = ComponentKind::Resistor;
            eqR.name = c.name + "_gd";
            eqR.nodePos = c.nodePos;
            eqR.nodeNeg = c.nodeNeg;
            eqR.value = 1.0 / gd;
            testComps.push_back(std::move(eqR));
            if (freqHz > 0.0) {
                CircuitComponent eqC;
                eqC.kind = ComponentKind::Capacitor;
                eqC.name = c.name + "_cj";
                eqC.nodePos = c.nodePos;
                eqC.nodeNeg = c.nodeNeg;
                eqC.value = Detail::GetParam(c, "Cj0", 4e-12);
                testComps.push_back(std::move(eqC));
            }
        } else if (c.kind == ComponentKind::BjtNpn || c.kind == ComponentKind::BjtPnp) {
            const double pol = (c.kind == ComponentKind::BjtNpn) ? 1.0 : -1.0;
            const double vB = getNodeV(dcBias, c.nodeOut).real();
            const double vE = getNodeV(dcBias, c.nodeNeg).real();
            const double vC = getNodeV(dcBias, c.nodePos).real();
            const double vbe = std::clamp(pol * (vB - vE), -2.0, 1.0);
            const double vbc = std::clamp(pol * (vB - vC), -40.0, 1.0);
            constexpr double kVt = 0.02585;
            const double isSat = Detail::GetParam(c, "Is", 1.4e-14);
            const double bf = Detail::GetParam(c, "Bf", 200.0);
            const double vaf = Detail::GetParam(c, "Vaf", 100.0);
            const double qEarly = 1.0 + (std::max)(-0.9, (vbe - vbc) / vaf);
            const double iF = isSat * (std::exp(std::clamp(vbe / kVt, -40.0, 36.0)) - 1.0);
            const double gF = (isSat / kVt) * std::exp(std::clamp(vbe / kVt, -40.0, 36.0));
            const double gm = (std::max)(1e-12, gF * qEarly + iF / vaf);
            const double rpi = bf / (std::max)(1e-12, gF);
            const double ro = vaf / (std::max)(1e-12, iF);

            CircuitComponent rPiComp;
            rPiComp.kind = ComponentKind::Resistor;
            rPiComp.name = c.name + "_rpi";
            rPiComp.nodePos = c.nodeOut;
            rPiComp.nodeNeg = c.nodeNeg;
            rPiComp.value = rpi;
            testComps.push_back(std::move(rPiComp));

            CircuitComponent rOComp;
            rOComp.kind = ComponentKind::Resistor;
            rOComp.name = c.name + "_ro";
            rOComp.nodePos = c.nodePos;
            rOComp.nodeNeg = c.nodeNeg;
            rOComp.value = ro;
            testComps.push_back(std::move(rOComp));

            CircuitComponent gmComp;
            gmComp.kind = ComponentKind::Vccs;
            gmComp.name = c.name + "_gm";
            gmComp.nodePos = c.nodePos;
            gmComp.nodeNeg = c.nodeNeg;
            gmComp.nodeCtrlPos = c.nodeOut;
            gmComp.nodeCtrlNeg = c.nodeNeg;
            gmComp.value = gm;
            gmComp.auxValue = gm;
            testComps.push_back(std::move(gmComp));
        } else if (c.kind == ComponentKind::MosfetN || c.kind == ComponentKind::MosfetP ||
                   c.kind == ComponentKind::JfetN || c.kind == ComponentKind::JfetP) {
            const bool isP = (c.kind == ComponentKind::MosfetP || c.kind == ComponentKind::JfetP);
            const bool isJfet = (c.kind == ComponentKind::JfetN || c.kind == ComponentKind::JfetP);
            const double pol = isP ? -1.0 : 1.0;
            const std::string m = ToUpperAscii(c.modelName);
            double vto = isJfet ? -2.0 : ((m == "2N7000") ? 2.1 : 3.5);
            vto = Detail::GetParam(c, "Vto", vto);
            if (c.kind == ComponentKind::MosfetP && vto < 0.0) vto = -vto;
            const double kp = Detail::GetParam(c, "Kp", isJfet ? 2e-3 : ((m == "2N7000") ? 0.35 : (isP ? 6.0 : 8.5)));
            const double lam = Detail::GetParam(c, "Lambda", 0.02);

            const double vG = getNodeV(dcBias, c.nodeOut).real();
            const double vD = getNodeV(dcBias, c.nodePos).real();
            const double vS = getNodeV(dcBias, c.nodeNeg).real();
            const double vgs = pol * (vG - vS);
            const double vds = pol * (vD - vS);
            const bool rev = (vds < 0.0);
            const double vdsEff = rev ? -vds : vds;
            const double vgsEff = rev ? (vgs - vds) : vgs;
            const double vov = vgsEff - vto;
            const int nD = rev ? c.nodeNeg : c.nodePos;
            const int nS = rev ? c.nodePos : c.nodeNeg;

            double gm = 0.0;
            double gds = 1e-10;
            if (vov > 0.0) {
                if (vdsEff < vov) {
                    gm = kp * vdsEff * (1.0 + lam * vdsEff);
                    gds = (std::max)(1e-10, kp * (vov - vdsEff) * (1.0 + lam * vdsEff) +
                                             kp * (vov * vdsEff - 0.5 * vdsEff * vdsEff) * lam);
                } else {
                    gm = kp * vov * (1.0 + lam * vdsEff);
                    gds = (std::max)(1e-10, 0.5 * kp * vov * vov * lam);
                }
            }
            CircuitComponent rDsComp;
            rDsComp.kind = ComponentKind::Resistor;
            rDsComp.name = c.name + "_rds";
            rDsComp.nodePos = nD;
            rDsComp.nodeNeg = nS;
            rDsComp.value = 1.0 / gds;
            testComps.push_back(std::move(rDsComp));

            CircuitComponent rGsComp;
            rGsComp.kind = ComponentKind::Resistor;
            rGsComp.name = c.name + "_rgs";
            rGsComp.nodePos = c.nodeOut;
            rGsComp.nodeNeg = nS;
            rGsComp.value = isJfet ? 1e8 : 1e11;
            testComps.push_back(std::move(rGsComp));

            if (gm > 0.0) {
                CircuitComponent gmComp;
                gmComp.kind = ComponentKind::Vccs;
                gmComp.name = c.name + "_gm";
                gmComp.nodePos = nD;
                gmComp.nodeNeg = nS;
                gmComp.nodeCtrlPos = c.nodeOut;
                gmComp.nodeCtrlNeg = nS;
                gmComp.value = gm;
                gmComp.auxValue = gm;
                testComps.push_back(std::move(gmComp));
            }
        } else if (c.kind == ComponentKind::OpAmp &&
                   ((!c.modelName.empty() && ToUpperAscii(c.modelName) != "IDEAL" && ToUpperAscii(c.modelName) != "OPAMP_IDEAL") ||
                    c.params.count("Vpp") || c.params.count("Vnn") || c.params.count("Aol") || c.params.count("GBW"))) {
            const std::string m = ToUpperAscii(c.modelName);
            double a0 = 1e5, rIn = 2e6, rOut = 75.0, gbw = 1e6, vPp = 15.0, vNn = -15.0;
            if (m == "LM358") { vPp = 12.0; vNn = 0.0; }
            else if (m == "LM741") { a0 = 2e5; }
            else if (m == "TL072") { a0 = 2e5; rIn = 1e12; gbw = 3e6; }
            a0 = Detail::GetParam(c, "Aol", a0);
            rIn = Detail::GetParam(c, "Rin", rIn);
            rOut = Detail::GetParam(c, "Rout", rOut);
            gbw = Detail::GetParam(c, "GBW", gbw);
            vPp = Detail::GetParam(c, "Vpp", vPp);
            vNn = Detail::GetParam(c, "Vnn", vNn);
            const double vSw = (std::max)(0.5, 0.5 * (vPp - vNn) - 1.0);
            const double vDiff = getNodeV(dcBias, c.nodePos).real() - getNodeV(dcBias, c.nodeNeg).real();
            const double th = std::tanh(std::clamp(a0 * vDiff / vSw, -20.0, 20.0));
            const double gainLocal = (std::max)(1e-6, a0 * (1.0 - th * th));

            CircuitComponent rInComp;
            rInComp.kind = ComponentKind::Resistor;
            rInComp.name = c.name + "_rin";
            rInComp.nodePos = c.nodePos;
            rInComp.nodeNeg = c.nodeNeg;
            rInComp.value = rIn;
            testComps.push_back(std::move(rInComp));

            CircuitComponent rOutComp;
            rOutComp.kind = ComponentKind::Resistor;
            rOutComp.name = c.name + "_rout";
            rOutComp.nodePos = c.nodeOut;
            rOutComp.nodeNeg = 0;
            rOutComp.value = rOut;
            testComps.push_back(std::move(rOutComp));

            const int poleNode = ++nextAuxNode;
            const double fp = gbw / (std::max)(1.0, a0);
            CircuitComponent rPole;
            rPole.kind = ComponentKind::Resistor;
            rPole.name = c.name + "_rp";
            rPole.nodePos = poleNode;
            rPole.nodeNeg = 0;
            rPole.value = 1.0;
            testComps.push_back(std::move(rPole));
            if (freqHz > 0.0) {
                CircuitComponent cPole;
                cPole.kind = ComponentKind::Capacitor;
                cPole.name = c.name + "_cp";
                cPole.nodePos = poleNode;
                cPole.nodeNeg = 0;
                cPole.value = 1.0 / (2.0 * kPi * (std::max)(1e-3, fp));
                testComps.push_back(std::move(cPole));
            }

            CircuitComponent stage1;
            stage1.kind = ComponentKind::Vccs;
            stage1.name = c.name + "_g1";
            stage1.nodePos = 0;
            stage1.nodeNeg = poleNode;
            stage1.nodeCtrlPos = c.nodePos;
            stage1.nodeCtrlNeg = c.nodeNeg;
            stage1.value = gainLocal;
            stage1.auxValue = gainLocal;
            testComps.push_back(std::move(stage1));

            CircuitComponent stage2;
            stage2.kind = ComponentKind::Vccs;
            stage2.name = c.name + "_g2";
            stage2.nodePos = 0;
            stage2.nodeNeg = c.nodeOut;
            stage2.nodeCtrlPos = poleNode;
            stage2.nodeCtrlNeg = 0;
            stage2.value = 1.0 / rOut;
            stage2.auxValue = 1.0 / rOut;
            testComps.push_back(std::move(stage2));
        } else if (c.kind == ComponentKind::VoltageRegulator) {
            const std::string m = ToUpperAscii(c.modelName);
            const bool isLm317 = (m == "LM317");
            const double rOut = 0.02;
            const double gOut = 1.0 / rOut;
            const double drop = Detail::GetParam(c, "Vdrop", 2.0);
            constexpr double kEps = 0.05;
            const double vIn = isLm317 ? getNodeV(dcBias, c.nodePos).real()
                                       : (getNodeV(dcBias, c.nodePos) - getNodeV(dcBias, c.nodeNeg)).real();
            const double vAdj = getNodeV(dcBias, c.nodeNeg).real();
            const double u = vIn - drop;
            const double sU = std::sqrt(u * u + kEps * kEps);
            const double vAvail = 0.5 * (u + sU);
            const double dvAvail_dvIn = 0.5 * (1.0 + u / sU);
            const double vReg = isLm317 ? (vAdj + Detail::GetParam(c, "Vref", 1.25))
                                        : Detail::GetParam(c, "Vreg", (m == "7812" ? 12.0 : (m == "7809" ? 9.0 : 5.0)));
            const double d = vReg - vAvail;
            const double sD = std::sqrt(d * d + kEps * kEps);
            const double dvT_dvIn = 0.5 * (1.0 + d / sD) * dvAvail_dvIn;
            const double dvT_dvAdj = isLm317 ? (0.5 * (1.0 - d / sD)) : 0.0;

            CircuitComponent rIn;
            rIn.kind = ComponentKind::Resistor;
            rIn.name = c.name + "_rin";
            rIn.nodePos = c.nodePos;
            rIn.nodeNeg = isLm317 ? 0 : c.nodeNeg;
            rIn.value = 1e4;
            testComps.push_back(std::move(rIn));

            CircuitComponent rOutComp;
            rOutComp.kind = ComponentKind::Resistor;
            rOutComp.name = c.name + "_rout";
            rOutComp.nodePos = c.nodeOut;
            rOutComp.nodeNeg = isLm317 ? 0 : c.nodeNeg;
            rOutComp.value = rOut;
            testComps.push_back(std::move(rOutComp));

            if (std::abs(dvT_dvIn) > 1e-12) {
                CircuitComponent gIn;
                gIn.kind = ComponentKind::Vccs;
                gIn.name = c.name + "_gin";
                gIn.nodePos = isLm317 ? 0 : c.nodeNeg;
                gIn.nodeNeg = c.nodeOut;
                gIn.nodeCtrlPos = c.nodePos;
                gIn.nodeCtrlNeg = isLm317 ? 0 : c.nodeNeg;
                gIn.value = gOut * dvT_dvIn;
                gIn.auxValue = gOut * dvT_dvIn;
                testComps.push_back(std::move(gIn));
            }
            if (isLm317 && std::abs(dvT_dvAdj) > 1e-12) {
                CircuitComponent gAdj;
                gAdj.kind = ComponentKind::Vccs;
                gAdj.name = c.name + "_gadj";
                gAdj.nodePos = 0;
                gAdj.nodeNeg = c.nodeOut;
                gAdj.nodeCtrlPos = c.nodeNeg;
                gAdj.nodeCtrlNeg = 0;
                gAdj.value = gOut * dvT_dvAdj;
                gAdj.auxValue = gOut * dvT_dvAdj;
                testComps.push_back(std::move(gAdj));
            }
        } else {
            testComps.push_back(c);
        }
    }

    CircuitComponent iTest;
    iTest.kind = ComponentKind::CurrentSource;
    iTest.name = "I_thev_test";
    iTest.nodePos = nodeB; // injects 1A into nodeA
    iTest.nodeNeg = nodeA;
    iTest.value = 1.0;
    testComps.push_back(iTest);

    const CircuitSolveResult testSol = SolveMnaCircuit(testComps, freqHz);
    if (!testSol.ok) {
        res.errorUtf8 = testSol.errorUtf8.empty() ? "Red de prueba singular para R_th." : testSol.errorUtf8;
        return res;
    }

    res.zTheveninAcOhm = getNodeV(testSol, nodeA) - getNodeV(testSol, nodeB);
    res.rTheveninOhm = res.zTheveninAcOhm.real();
    if (std::abs(res.zTheveninAcOhm) > 1e-12) {
        res.iNortonAcA = res.vTheveninAcV / res.zTheveninAcOhm;
        res.iNortonA = res.iNortonAcA.real();
    }
    res.ok = std::isfinite(res.rTheveninOhm) && std::isfinite(res.vTheveninV);
    return res;
}

// -------- 3. 4/5/6-Band Resistor & 3/4-Digit/EIA-96/R SMD Decoders --------

struct ResistorCodeResult {
    bool ok = false;
    double ohms = 0.0;
    double tolerancePct = 0.0;
    int tcrPpmPerK = 0;
    std::string formattedUtf8;
    std::string errorUtf8;
};

inline ResistorCodeResult DecodeResistorColorBands(const std::vector<int>& bands) {
    ResistorCodeResult res;
    if (bands.size() < 4 || bands.size() > 6) {
        res.errorUtf8 = "Se requieren 4, 5 o 6 bandas de color.";
        return res;
    }
    const std::size_t digitsCount = (bands.size() >= 5) ? 3 : 2;
    long long digitsVal = 0;
    for (std::size_t i = 0; i < digitsCount; ++i) {
        if (bands[i] < 0 || bands[i] > 9) {
            res.errorUtf8 = "Color de digito invalido (debe ser 0..9).";
            return res;
        }
        digitsVal = digitsVal * 10 + bands[i];
    }

    int multCode = bands[digitsCount];
    if (multCode == 10) multCode = -1;      // Gold compatibility
    else if (multCode == 11) multCode = -2; // Silver compatibility
    if (multCode < -2 || multCode > 9) {
        res.errorUtf8 = "Multiplicador de color invalido.";
        return res;
    }

    static const std::map<int, double> tolMap = {
        { 1, 1.0 }, { 2, 2.0 }, { 5, 0.5 }, { 6, 0.25 },
        { 7, 0.1 }, { 8, 0.05 }, { -1, 5.0 }, { 10, 5.0 },
        { -2, 10.0 }, { 11, 10.0 }, { -3, 20.0 }
    };
    const int tolCode = bands[digitsCount + 1];
    auto itTol = tolMap.find(tolCode);
    if (itTol == tolMap.end()) {
        res.errorUtf8 = "Color de tolerancia invalido.";
        return res;
    }

    if (bands.size() == 6) {
        static const std::map<int, int> tcrMap = {
            { 1, 100 }, { 2, 50 }, { 3, 15 }, { 4, 25 }, { 6, 10 }, { 7, 5 }, { 9, 1 }
        };
        auto itTcr = tcrMap.find(bands[5]);
        if (itTcr == tcrMap.end()) {
            res.errorUtf8 = "Codigo TCR de 6a banda invalido.";
            return res;
        }
        res.tcrPpmPerK = itTcr->second;
    }

    res.ohms = static_cast<double>(digitsVal) * std::pow(10.0, multCode);
    res.tolerancePct = itTol->second;

    std::ostringstream oss;
    oss << Toolbox::FormatEngineeringNotation(res.ohms, "Ohm", 4) << " +/- " << res.tolerancePct << "%";
    if (res.tcrPpmPerK > 0) oss << " (" << res.tcrPpmPerK << " ppm/K)";
    res.formattedUtf8 = oss.str();
    res.ok = true;
    return res;
}

inline ResistorCodeResult DecodeResistorColorNames(const std::vector<std::string>& colorNamesUtf8) {
    auto nameToCode = [](std::string s) -> int {
        for (char& ch : s) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
        if (s == "black" || s == "negro") return 0;
        if (s == "brown" || s == "cafe" || s == "marron") return 1;
        if (s == "red" || s == "rojo") return 2;
        if (s == "orange" || s == "naranja") return 3;
        if (s == "yellow" || s == "amarillo") return 4;
        if (s == "green" || s == "verde") return 5;
        if (s == "blue" || s == "azul") return 6;
        if (s == "violet" || s == "purple" || s == "violeta" || s == "morado") return 7;
        if (s == "grey" || s == "gray" || s == "gris") return 8;
        if (s == "white" || s == "blanco") return 9;
        if (s == "gold" || s == "dorado" || s == "oro") return -1;
        if (s == "silver" || s == "plateado" || s == "plata") return -2;
        if (s == "none" || s == "ninguno") return -3;
        return -99;
    };
    std::vector<int> codes;
    codes.reserve(colorNamesUtf8.size());
    for (const auto& nm : colorNamesUtf8) {
        const int c = nameToCode(nm);
        if (c == -99) {
            ResistorCodeResult err;
            err.errorUtf8 = "Color de banda no reconocido: " + nm;
            return err;
        }
        codes.push_back(c);
    }
    return DecodeResistorColorBands(codes);
}

inline ResistorCodeResult DecodeSmdResistorCode(const std::string& codeUtf8) {
    ResistorCodeResult res;
    std::string code;
    for (char ch : codeUtf8) {
        if (!std::isspace(static_cast<unsigned char>(ch))) {
            code.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(ch))));
        }
    }
    if (code.empty()) {
        res.errorUtf8 = "Codigo SMD vacio.";
        return res;
    }
    for (char ch : code) {
        if (!std::isalnum(static_cast<unsigned char>(ch))) {
            res.errorUtf8 = "Codigo SMD invalido (caracteres no alfanumericos): " + codeUtf8;
            return res;
        }
    }

    // 1. Zero-ohm jumper ("0", "000", "0000")
    if (code == "0" || code == "00" || code == "000" || code == "0000") {
        res.ohms = 0.0;
        res.tolerancePct = 0.0;
        res.formattedUtf8 = "0 Ohm (Jumper)";
        res.ok = true;
        return res;
    }

    // 2. R/K/M decimal notation ("4R7", "R22", "2K2", "1M5")
    const std::size_t rkmPos = code.find_first_of("RKM");
    if (rkmPos != std::string::npos && code.size() >= 2) {
        bool otherAllDigits = true;
        for (std::size_t j = 0; j < code.size(); ++j) {
            if (j != rkmPos && !std::isdigit(static_cast<unsigned char>(code[j]))) {
                otherAllDigits = false;
                break;
            }
        }
        if (otherAllDigits) {
            const char unitChar = code[rkmPos];
            double mul = 1.0;
            if (unitChar == 'K') mul = 1e3;
            else if (unitChar == 'M') mul = 1e6;
            std::string dec = code;
            dec[rkmPos] = '.';
            if (dec.front() == '.') dec = "0" + dec;
            try {
                std::size_t idx = 0;
                const double val = std::stod(dec, &idx);
                const double ohms = val * mul;
                if (idx == dec.size() && std::isfinite(val) && std::isfinite(ohms)) {
                    res.ohms = ohms;
                    res.tolerancePct = 1.0;
                    res.formattedUtf8 = Toolbox::FormatEngineeringNotation(res.ohms, "Ohm", 4) + " +/- 1%";
                    res.ok = true;
                    return res;
                }
            } catch (...) {}
        }
    }

    auto allDigits = [](const std::string& s) {
        return !s.empty() && std::all_of(s.begin(), s.end(), [](char c) {
            return std::isdigit(static_cast<unsigned char>(c)) != 0;
        });
    };

    // 3. 3-digit standard ("472" -> 47 * 10^2)
    if (code.size() == 3 && allDigits(code)) {
        const int sig = std::stoi(code.substr(0, 2));
        const int exp = code[2] - '0';
        res.ohms = static_cast<double>(sig) * std::pow(10.0, exp);
        res.tolerancePct = 5.0;
        res.formattedUtf8 = Toolbox::FormatEngineeringNotation(res.ohms, "Ohm", 4) + " +/- 5%";
        res.ok = true;
        return res;
    }

    // 4. 4-digit precision ("1002" -> 100 * 10^2)
    if (code.size() == 4 && allDigits(code)) {
        const int sig = std::stoi(code.substr(0, 3));
        const int exp = code[3] - '0';
        res.ohms = static_cast<double>(sig) * std::pow(10.0, exp);
        res.tolerancePct = 1.0;
        res.formattedUtf8 = Toolbox::FormatEngineeringNotation(res.ohms, "Ohm", 4) + " +/- 1%";
        res.ok = true;
        return res;
    }

    // 5. Full 96-entry EIA-96 (2 digits 01..96 + multiplier letter)
    if (code.size() == 3 && std::isdigit(static_cast<unsigned char>(code[0])) &&
        std::isdigit(static_cast<unsigned char>(code[1])) && std::isalpha(static_cast<unsigned char>(code[2]))) {
        static constexpr std::array<int, 96> kEia96Values = {
            100, 102, 105, 107, 110, 113, 115, 118, 121, 124, 127, 130,
            133, 137, 140, 143, 147, 150, 154, 158, 162, 165, 169, 174,
            178, 182, 187, 191, 196, 200, 205, 210, 215, 221, 226, 232,
            237, 243, 249, 255, 261, 267, 274, 280, 287, 294, 301, 309,
            316, 324, 332, 340, 348, 357, 365, 374, 383, 392, 402, 412,
            422, 432, 442, 453, 464, 475, 487, 499, 511, 523, 536, 549,
            562, 576, 590, 604, 619, 634, 649, 665, 681, 698, 715, 732,
            750, 768, 787, 806, 825, 845, 866, 887, 909, 931, 953, 976
        };
        const int idx = std::stoi(code.substr(0, 2));
        double mul = -1.0;
        switch (code[2]) {
        case 'Z': case 'Y': mul = 0.01; break;
        case 'X': case 'S': mul = 0.1; break;
        case 'A': mul = 1.0; break;
        case 'B': case 'H': mul = 10.0; break;
        case 'C': mul = 100.0; break;
        case 'D': mul = 1000.0; break;
        case 'E': mul = 10000.0; break;
        case 'F': mul = 100000.0; break;
        default: break;
        }
        if (idx >= 1 && idx <= 96 && mul > 0.0) {
            res.ohms = static_cast<double>(kEia96Values[static_cast<std::size_t>(idx - 1)]) * mul;
            res.tolerancePct = 1.0;
            res.formattedUtf8 = Toolbox::FormatEngineeringNotation(res.ohms, "Ohm", 4) + " +/- 1% (EIA-96)";
            res.ok = true;
            return res;
        }
    }

    res.errorUtf8 = "Codigo SMD no reconocido: " + codeUtf8;
    return res;
}

// -------- 4. Trainable 14-Feature Handwritten Circuit Stroke Recognizer --------

struct StrokePolyline {
    std::vector<Vec2> points;
};

struct CircuitRecognitionResult {
    ComponentKind kind = ComponentKind::Wire;
    float confidence = 0.0f;
    std::vector<Vec2> terminals;
    bool fromUserTemplate = false;
};

namespace Detail {

enum class Shape2DKind { None, Line, Rectangle, Ellipse, Triangle, Arrow };

inline float DistF(Vec2 a, Vec2 b) noexcept {
    return static_cast<float>(std::hypot(a.x - b.x, a.y - b.y));
}

inline float DistToLineF(Vec2 p, Vec2 a, Vec2 b) noexcept {
    const float dx = static_cast<float>(b.x - a.x);
    const float dy = static_cast<float>(b.y - a.y);
    const float len = std::sqrt(dx * dx + dy * dy);
    if (len < 1e-6f) return DistF(p, a);
    return std::fabs((static_cast<float>(p.x - a.x)) * dy - (static_cast<float>(p.y - a.y)) * dx) / len;
}

inline void RdpSimplifyRec(
    const std::vector<Vec2>& pts,
    std::size_t start,
    std::size_t end,
    float tol,
    std::vector<bool>& keep)
{
    if (end <= start + 1) return;
    float worst = 0.0f;
    std::size_t idx = start;
    for (std::size_t i = start + 1; i < end; ++i) {
        const float d = DistToLineF(pts[i], pts[start], pts[end]);
        if (d > worst) { worst = d; idx = i; }
    }
    if (worst <= tol) return;
    keep[idx] = true;
    RdpSimplifyRec(pts, start, idx, tol, keep);
    RdpSimplifyRec(pts, idx, end, tol, keep);
}

inline std::vector<Vec2> SimplifyStrokeRdp(const std::vector<Vec2>& pts, float tol) {
    if (pts.size() < 3 || tol <= 0.0f) return pts;
    std::vector<bool> keep(pts.size(), false);
    keep.front() = true;
    keep.back() = true;
    RdpSimplifyRec(pts, 0, pts.size() - 1, tol, keep);
    std::vector<Vec2> out;
    for (std::size_t i = 0; i < pts.size(); ++i) {
        if (keep[i]) out.push_back(pts[i]);
    }
    return out;
}

inline float MeanAngularTurn(const std::vector<Vec2>& pts) noexcept {
    if (pts.size() < 3) return 0.0f;
    float sum = 0.0f;
    int cnt = 0;
    for (std::size_t i = 1; i + 1 < pts.size(); ++i) {
        const float a1 = std::atan2(static_cast<float>(pts[i].y - pts[i - 1].y),
                                    static_cast<float>(pts[i].x - pts[i - 1].x));
        const float a2 = std::atan2(static_cast<float>(pts[i + 1].y - pts[i].y),
                                    static_cast<float>(pts[i + 1].x - pts[i].x));
        float d = std::fabs(a2 - a1);
        if (d > 3.14159265f) d = 2.0f * 3.14159265f - d;
        sum += d;
        ++cnt;
    }
    return (cnt > 0) ? (sum / static_cast<float>(cnt)) : 0.0f;
}

inline float StrokePathLength(const std::vector<Vec2>& pts) noexcept {
    float total = 0.0f;
    for (std::size_t i = 1; i < pts.size(); ++i) total += DistF(pts[i - 1], pts[i]);
    return total;
}

inline Shape2DKind ClassifySingleStrokeShape(const std::vector<Vec2>& pts) {
    if (pts.size() < 2) return Shape2DKind::None;
    const float len = StrokePathLength(pts);
    if (len < 1e-5f) return Shape2DKind::None;
    if (pts.size() < 4) {
        return (DistF(pts.front(), pts.back()) > 1e-5f) ? Shape2DKind::Line : Shape2DKind::None;
    }

    double x0 = pts[0].x, x1 = x0, y0 = pts[0].y, y1 = y0;
    for (const auto& p : pts) {
        x0 = (std::min)(x0, p.x); x1 = (std::max)(x1, p.x);
        y0 = (std::min)(y0, p.y); y1 = (std::max)(y1, p.y);
    }
    const float w = static_cast<float>(x1 - x0);
    const float h = static_cast<float>(y1 - y0);
    const float diag = std::sqrt(w * w + h * h);
    if (diag < 1e-5f) return Shape2DKind::None;

    const float closure = DistF(pts.front(), pts.back()) / len;
    const bool closed = (closure < 0.22f);

    if (!closed) {
        float maxDev = 0.0f;
        for (const auto& p : pts) maxDev = (std::max)(maxDev, DistToLineF(p, pts.front(), pts.back()));
        if ((maxDev / len) < 0.055f) return Shape2DKind::Line;
        return Shape2DKind::None;
    }

    const Vec2 center{ 0.5 * (x0 + x1), 0.5 * (y0 + y1) };
    const float sx = (std::max)(w * 0.5f, 1e-5f);
    const float sy = (std::max)(h * 0.5f, 1e-5f);
    float sumR = 0.0f;
    for (const auto& p : pts) {
        const float nx = static_cast<float>(p.x - center.x) / sx;
        const float ny = static_cast<float>(p.y - center.y) / sy;
        sumR += std::sqrt(nx * nx + ny * ny);
    }
    const float meanR = sumR / static_cast<float>(pts.size());
    float varR = 0.0f;
    for (const auto& p : pts) {
        const float nx = static_cast<float>(p.x - center.x) / sx;
        const float ny = static_cast<float>(p.y - center.y) / sy;
        const float d = std::sqrt(nx * nx + ny * ny) - meanR;
        varR += d * d;
    }
    const float disp = std::sqrt(varR / static_cast<float>(pts.size()));

    auto simp = SimplifyStrokeRdp(pts, diag * 0.06f);
    if (simp.size() > 1 && DistF(simp.front(), simp.back()) < diag * 0.15f) simp.pop_back();
    const std::size_t sides = simp.size();

    const bool looksEllipse = (disp < 0.18f);
    const bool looksPoly = (sides == 3 || sides == 4);
    if (looksPoly && (!looksEllipse || sides <= 4)) {
        return (sides == 3) ? Shape2DKind::Triangle : Shape2DKind::Rectangle;
    }
    if (looksEllipse) return Shape2DKind::Ellipse;
    return Shape2DKind::None;
}

} // namespace Detail

inline std::array<float, 14> ExtractCircuitFeatures14(const std::vector<StrokePolyline>& strokes) {
    std::array<float, 14> f{};
    if (strokes.empty()) return f;

    double x0 = 0.0, y0 = 0.0, x1 = 0.0, y1 = 0.0;
    bool first = true;
    float totalLen = 0.0f;
    float closures = 0.0f;
    float vertsNorm = 0.0f;
    float turnsNorm = 0.0f;
    int lines = 0, ellipses = 0, rects = 0, triangles = 0;
    std::vector<Vec2> endpoints;

    // First pass: compute global bounding box diagonal for strictly scale-invariant tolerances
    for (const auto& st : strokes) {
        for (const auto& p : st.points) {
            if (first) { x0 = x1 = p.x; y0 = y1 = p.y; first = false; }
            x0 = (std::min)(x0, p.x); x1 = (std::max)(x1, p.x);
            y0 = (std::min)(y0, p.y); y1 = (std::max)(y1, p.y);
        }
    }
    const float w = static_cast<float>((std::max)(x1 - x0, 0.0));
    const float h = static_cast<float>((std::max)(y1 - y0, 0.0));
    const float diag = (std::max)(std::hypot(w, h), 1e-6f);

    for (const auto& st : strokes) {
        if (st.points.empty()) continue;
        const float len = Detail::StrokePathLength(st.points);
        totalLen += len;
        const Vec2 pStart = st.points.front();
        const Vec2 pEnd = st.points.back();
        endpoints.push_back(pStart);
        endpoints.push_back(pEnd);
        if (len > 1e-6f) {
            closures += std::clamp(1.0f - Detail::DistF(pStart, pEnd) / len, 0.0f, 1.0f);
        }
        const auto simp = Detail::SimplifyStrokeRdp(st.points, (std::max)(diag * 0.015f, 1e-6f));
        vertsNorm += (std::min)(static_cast<float>(simp.size()), 24.0f) / 24.0f;
        turnsNorm += Detail::MeanAngularTurn(simp) / 3.14159265f;

        switch (Detail::ClassifySingleStrokeShape(st.points)) {
        case Detail::Shape2DKind::Line: ++lines; break;
        case Detail::Shape2DKind::Ellipse: ++ellipses; break;
        case Detail::Shape2DKind::Rectangle: ++rects; break;
        case Detail::Shape2DKind::Triangle: ++triangles; break;
        default: break;
        }
    }

    const float n = (std::max)(static_cast<float>(strokes.size()), 1.0f);

    int closePairs = 0;
    int totalPairs = 0;
    for (std::size_t i = 0; i < endpoints.size(); ++i) {
        for (std::size_t j = i + 1; j < endpoints.size(); ++j) {
            ++totalPairs;
            if (Detail::DistF(endpoints[i], endpoints[j]) < diag * 0.10f) ++closePairs;
        }
    }

    f[0] = (std::min)(static_cast<float>(strokes.size()), 16.0f) / 16.0f;
    f[1] = static_cast<float>(lines) / n;
    f[2] = static_cast<float>(ellipses) / n;
    f[3] = static_cast<float>(rects) / n;
    f[4] = static_cast<float>(triangles) / n;
    f[5] = w / (std::max)(w + h, 1e-4f);
    f[6] = closures / n;
    f[7] = std::tanh(totalLen / (diag * n * 4.0f));
    f[8] = vertsNorm / n;
    f[9] = turnsNorm / n;
    f[10] = (totalPairs > 0) ? (static_cast<float>(closePairs) / static_cast<float>(totalPairs)) : 0.0f;
    f[11] = (std::min)(totalLen / (diag * 12.0f), 1.0f);
    f[12] = (std::min)(w / (std::max)(h, diag * 0.02f), 10.0f) / 10.0f;
    f[13] = (std::min)(h / (std::max)(w, diag * 0.02f), 10.0f) / 10.0f;
    return f;
}

inline CircuitRecognitionResult RecognizeCircuitStrokes(const std::vector<StrokePolyline>& strokes) {
    CircuitRecognitionResult res;
    if (strokes.empty()) return res;

    int ellipses = 0, lines = 0, triangles = 0, rects = 0;
    for (const auto& st : strokes) {
        switch (Detail::ClassifySingleStrokeShape(st.points)) {
        case Detail::Shape2DKind::Ellipse: ++ellipses; break;
        case Detail::Shape2DKind::Line: ++lines; break;
        case Detail::Shape2DKind::Triangle: ++triangles; break;
        case Detail::Shape2DKind::Rectangle: ++rects; break;
        default: break;
        }
    }

    if (ellipses == 1 && strokes.size() <= 2) {
        res.kind = ComponentKind::VoltageSource;
        res.confidence = 0.78f;
    } else if (triangles == 1 && lines >= 1) {
        res.kind = (strokes.size() >= 3) ? ComponentKind::Diode : ComponentKind::OpAmp;
        res.confidence = 0.74f;
    } else if (triangles == 1 && strokes.size() == 1) {
        res.kind = ComponentKind::OpAmp;
        res.confidence = 0.72f;
    } else if (rects == 1 && strokes.size() <= 3) {
        res.kind = ComponentKind::Resistor;
        res.confidence = 0.72f;
    } else if (lines == 2 && ellipses == 0 && strokes.size() == 2) {
        res.kind = ComponentKind::Capacitor;
        res.confidence = 0.68f;
    } else if (lines >= 3 && ellipses == 0 && strokes.size() >= 3) {
        res.kind = ComponentKind::Ground;
        res.confidence = 0.65f;
    } else if (strokes.size() == 1 && !strokes[0].points.empty()) {
        const auto shape = Detail::ClassifySingleStrokeShape(strokes[0].points);
        if (shape == Detail::Shape2DKind::Line) {
            res.kind = ComponentKind::Wire;
            res.confidence = 0.90f;
        } else {
            const auto& pts = strokes[0].points;
            double x0 = pts[0].x, x1 = x0, y0 = pts[0].y, y1 = y0;
            for (const auto& p : pts) {
                x0 = (std::min)(x0, p.x); x1 = (std::max)(x1, p.x);
                y0 = (std::min)(y0, p.y); y1 = (std::max)(y1, p.y);
            }
            const float diag = (std::max)(static_cast<float>(std::hypot(x1 - x0, y1 - y0)), 1e-4f);
            const auto simp = Detail::SimplifyStrokeRdp(pts, diag * 0.045f);
            const float turn = Detail::MeanAngularTurn(simp);
            if (simp.size() >= 6 && turn > 0.72f) {
                res.kind = ComponentKind::Resistor;
                res.confidence = std::clamp(0.58f + turn * 0.16f, 0.0f, 0.90f);
            } else if (simp.size() >= 5 && turn > 0.18f && turn <= 0.72f) {
                res.kind = ComponentKind::Inductor;
                res.confidence = std::clamp(0.62f + (0.72f - turn) * 0.18f, 0.0f, 0.86f);
            }
        }
    }

    if (!strokes.front().points.empty()) {
        res.terminals.push_back(strokes.front().points.front());
        res.terminals.push_back(strokes.front().points.back());
    }
    return res;
}

class TrainableCircuitRecognizer {
public:
    struct LabeledSample {
        ComponentKind kind = ComponentKind::Resistor;
        std::array<float, 14> features{};
    };

    void AddExample(ComponentKind kind, const std::vector<StrokePolyline>& strokes) {
        samples_.push_back({ kind, ExtractCircuitFeatures14(strokes) });
    }

    void AddFeatureSample(ComponentKind kind, const std::array<float, 14>& features) {
        samples_.push_back({ kind, features });
    }

    void Clear() { samples_.clear(); }
    std::size_t SampleCount() const noexcept { return samples_.size(); }

    CircuitRecognitionResult Classify(const std::vector<StrokePolyline>& strokes, float minConfidence = 0.55f) const {
        if (!samples_.empty() && !strokes.empty()) {
            const auto feat = ExtractCircuitFeatures14(strokes);
            double bestDist = std::numeric_limits<double>::infinity();
            ComponentKind bestKind = ComponentKind::Unknown;
            for (const auto& s : samples_) {
                double d2 = 0.0;
                for (std::size_t k = 0; k < 14; ++k) {
                    const double diff = static_cast<double>(feat[k] - s.features[k]);
                    d2 += diff * diff;
                }
                const double dist = std::sqrt(d2);
                if (dist < bestDist) {
                    bestDist = dist;
                    bestKind = s.kind;
                }
            }
            const float conf = std::clamp(1.0f - static_cast<float>(bestDist / std::sqrt(14.0)), 0.0f, 1.0f);
            if (conf >= minConfidence) {
                CircuitRecognitionResult r;
                r.kind = bestKind;
                r.confidence = conf;
                r.fromUserTemplate = true;
                if (!strokes.front().points.empty()) {
                    r.terminals.push_back(strokes.front().points.front());
                    r.terminals.push_back(strokes.front().points.back());
                }
                return r;
            }
        }
        return RecognizeCircuitStrokes(strokes);
    }

    std::string SerializeJson() const {
        std::ostringstream oss;
        oss << std::setprecision(6) << "{\"format\":\"docstudio_circuit_samples_v1\",\"samples\":[";
        for (std::size_t i = 0; i < samples_.size(); ++i) {
            if (i > 0) oss << ",";
            oss << "{\"kind\":" << static_cast<int>(samples_[i].kind) << ",\"f\":[";
            for (std::size_t k = 0; k < 14; ++k) {
                if (k > 0) oss << ",";
                oss << samples_[i].features[k];
            }
            oss << "]}";
        }
        oss << "]}";
        return oss.str();
    }

    bool DeserializeJson(const std::string& jsonUtf8) {
        Detail::MiniJsonParser parser(jsonUtf8);
        Detail::JsonValue root;
        if (!parser.Parse(root) || root.type != Detail::JsonValue::Type::Object) return false;
        const auto* fmt = root.Get("format");
        if (!fmt || fmt->AsString() != "docstudio_circuit_samples_v1") return false;
        const auto* arr = root.Get("samples");
        if (!arr || arr->type != Detail::JsonValue::Type::Array) return false;

        std::vector<LabeledSample> loaded;
        for (const auto& item : arr->arrVal) {
            const auto* kv = item.Get("kind");
            const auto* fv = item.Get("f");
            if (!kv || !fv || fv->type != Detail::JsonValue::Type::Array || fv->arrVal.size() != 14) continue;
            LabeledSample s;
            s.kind = static_cast<ComponentKind>(kv->AsInt(0));
            for (std::size_t k = 0; k < 14; ++k) {
                s.features[k] = static_cast<float>(fv->arrVal[k].AsDouble(0.0));
            }
            loaded.push_back(s);
        }
        samples_ = std::move(loaded);
        return true;
    }

private:
    std::vector<LabeledSample> samples_;
};

} // namespace DocStudio::Circuit
