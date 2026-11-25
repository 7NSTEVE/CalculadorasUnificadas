#include "mna_solver.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstring>
#include <limits>
#include <sstream>
#include <unordered_map>
#include <unordered_set>

namespace DocStudio::Circuit {

namespace {

bool ParseScaledDouble(std::string tok, double& out) {
    if (tok.empty()) return false;
    for (const char* suf : { "Ohm", "ohm", "Hz", "hz", "V", "A", "F", "H", "W", "C", "lux" }) {
        const std::size_t slen = std::strlen(suf);
        if (tok.size() > slen && tok.compare(tok.size() - slen, slen, suf) == 0) {
            tok.resize(tok.size() - slen);
            break;
        }
    }
    if (tok.empty()) return false;
    double mul = 1.0;
    const char last = tok.back();
    if (!std::isdigit(static_cast<unsigned char>(last))) {
        switch (last) {
        case 'f': mul = 1e-15; tok.pop_back(); break;
        case 'p': mul = 1e-12; tok.pop_back(); break;
        case 'n': mul = 1e-9; tok.pop_back(); break;
        case 'u': mul = 1e-6; tok.pop_back(); break;
        case 'm': mul = 1e-3; tok.pop_back(); break;
        case 'k': case 'K': mul = 1e3; tok.pop_back(); break;
        case 'M': mul = 1e6; tok.pop_back(); break;
        case 'G': mul = 1e9; tok.pop_back(); break;
        default: return false;
        }
    }
    try {
        std::size_t idx = 0;
        const double v = std::stod(tok, &idx);
        if (idx != tok.size()) return false;
        out = v * mul;
        return std::isfinite(out);
    } catch (...) {
        return false;
    }
}

std::string FmtComplex(const std::complex<double>& c) {
    std::ostringstream oss;
    oss.precision(4);
    if (std::abs(c.imag()) <= 1e-9) {
        oss << c.real();
    } else {
        oss << std::abs(c) << "\xE2\x88\xA0" << (std::arg(c) * 180.0 / kPi) << "\xC2\xB0";
    }
    return oss.str();
}

bool SolveComplexLinearSystem(
    std::vector<std::vector<std::complex<double>>>& A,
    std::vector<std::complex<double>>& b)
{
    const std::size_t n = b.size();
    for (std::size_t col = 0; col < n; ++col) {
        std::size_t piv = col;
        double best = std::abs(A[col][col]);
        for (std::size_t r = col + 1; r < n; ++r) {
            const double val = std::abs(A[r][col]);
            if (val > best) {
                best = val;
                piv = r;
            }
        }
        if (best < 1e-15 || !std::isfinite(best)) return false;
        if (piv != col) {
            std::swap(A[col], A[piv]);
            std::swap(b[col], b[piv]);
        }
        const std::complex<double> inv = std::complex<double>(1.0, 0.0) / A[col][col];
        for (std::size_t c = col; c < n; ++c) A[col][c] *= inv;
        b[col] *= inv;
        for (std::size_t r = 0; r < n; ++r) {
            if (r == col) continue;
            const std::complex<double> f = A[r][col];
            if (std::abs(f) == 0.0) continue;
            for (std::size_t c = col; c < n; ++c) A[r][c] -= f * A[col][c];
            b[r] -= f * b[col];
        }
    }
    return true;
}

bool IsNonlinearKind(ComponentKind k) noexcept {
    switch (k) {
    case ComponentKind::Diode:
    case ComponentKind::Led:
    case ComponentKind::ZenerDiode:
    case ComponentKind::SchottkyDiode:
    case ComponentKind::Photodiode:
    case ComponentKind::BjtNpn:
    case ComponentKind::BjtPnp:
    case ComponentKind::MosfetN:
    case ComponentKind::MosfetP:
    case ComponentKind::JfetN:
    case ComponentKind::JfetP:
    case ComponentKind::Scr:
    case ComponentKind::Triac:
    case ComponentKind::Comparator:
    case ComponentKind::VoltageRegulator:
    case ComponentKind::Ic:
    case ComponentKind::GateAnd:
    case ComponentKind::GateOr:
    case ComponentKind::GateNot:
    case ComponentKind::GateNand:
    case ComponentKind::GateNor:
    case ComponentKind::GateXor:
    case ComponentKind::FlipFlopD:
    case ComponentKind::FlipFlopJK:
    case ComponentKind::Multiplexer:
    case ComponentKind::Demultiplexer:
        return true;
    default:
        return false;
    }
}

bool IsRealOpAmp(const CircuitComponent& c) noexcept {
    if (c.kind != ComponentKind::OpAmp) return false;
    const std::string um = ToUpperAscii(c.modelName);
    if (!um.empty() && um != "IDEAL" && um != "OPAMP_IDEAL") return true;
    if (c.params.count("Vpp") || c.params.count("Vnn") || c.params.count("Aol") || c.params.count("GBW")) return true;
    return false;
}

bool HasAnyNonlinear(const std::vector<CircuitComponent>& comps) noexcept {
    for (const auto& c : comps) {
        if (!c.active) continue;
        if (IsNonlinearKind(c.kind) || IsRealOpAmp(c)) return true;
    }
    return false;
}

void GetOpAmpSpec(const CircuitComponent& c, double& a0, double& rIn, double& rOut, double& gbwHz, double& vPp, double& vNn) {
    const std::string m = ToUpperAscii(c.modelName);
    a0 = 1e5;
    rIn = 2e6;
    rOut = 75.0;
    gbwHz = 1e6;
    vPp = 15.0;
    vNn = -15.0;
    if (m == "LM358") {
        a0 = 1e5; rIn = 2e6; rOut = 75.0; gbwHz = 1e6; vPp = 12.0; vNn = 0.0;
    } else if (m == "LM741") {
        a0 = 2e5; rIn = 2e6; rOut = 75.0; gbwHz = 1e6; vPp = 15.0; vNn = -15.0;
    } else if (m == "TL072") {
        a0 = 2e5; rIn = 1e12; rOut = 75.0; gbwHz = 3e6; vPp = 15.0; vNn = -15.0;
    }
    a0 = Detail::GetParam(c, "Aol", a0);
    rIn = Detail::GetParam(c, "Rin", rIn);
    rOut = Detail::GetParam(c, "Rout", rOut);
    gbwHz = Detail::GetParam(c, "GBW", gbwHz);
    vPp = Detail::GetParam(c, "Vpp", vPp);
    vNn = Detail::GetParam(c, "Vnn", vNn);
}

struct MnaTopology {
    int maxNode = 0;
    std::size_t numAux = 0;
    std::vector<int> auxIndex;
    std::vector<int> crystalAuxNode; // internal BVD node (>0)
};

MnaTopology BuildTopology(const std::vector<CircuitComponent>& comps) {
    MnaTopology topo;
    topo.auxIndex.assign(comps.size(), -1);
    topo.crystalAuxNode.assign(comps.size(), 0);
    for (std::size_t i = 0; i < comps.size(); ++i) {
        const auto& c = comps[i];
        if (!c.active) continue;
        topo.maxNode = (std::max)(topo.maxNode, c.nodePos);
        topo.maxNode = (std::max)(topo.maxNode, c.nodeNeg);
        if (c.kind == ComponentKind::OpAmp || c.kind == ComponentKind::Comparator ||
            c.kind == ComponentKind::BjtNpn || c.kind == ComponentKind::BjtPnp ||
            c.kind == ComponentKind::MosfetN || c.kind == ComponentKind::MosfetP ||
            c.kind == ComponentKind::JfetN || c.kind == ComponentKind::JfetP ||
            c.kind == ComponentKind::Scr || c.kind == ComponentKind::Triac ||
            c.kind == ComponentKind::Potentiometer || c.kind == ComponentKind::VoltageRegulator ||
            c.kind == ComponentKind::Ic || c.kind == ComponentKind::Transformer ||
            c.kind == ComponentKind::GateAnd || c.kind == ComponentKind::GateOr ||
            c.kind == ComponentKind::GateNot || c.kind == ComponentKind::GateNand ||
            c.kind == ComponentKind::GateNor || c.kind == ComponentKind::GateXor ||
            c.kind == ComponentKind::FlipFlopD || c.kind == ComponentKind::FlipFlopJK ||
            c.kind == ComponentKind::Multiplexer || c.kind == ComponentKind::Demultiplexer) {
            topo.maxNode = (std::max)(topo.maxNode, c.nodeOut);
        }
        if (c.kind == ComponentKind::Transformer || c.kind == ComponentKind::Vcvs ||
            c.kind == ComponentKind::Vccs || c.kind == ComponentKind::Relay ||
            c.kind == ComponentKind::FlipFlopD || c.kind == ComponentKind::FlipFlopJK ||
            c.kind == ComponentKind::Multiplexer || c.kind == ComponentKind::Demultiplexer) {
            topo.maxNode = (std::max)(topo.maxNode, c.nodeCtrlPos);
            topo.maxNode = (std::max)(topo.maxNode, c.nodeCtrlNeg);
        }
        for (int en : c.extraNodes) topo.maxNode = (std::max)(topo.maxNode, en);

        if (c.kind == ComponentKind::VoltageSource || c.kind == ComponentKind::Battery ||
            c.kind == ComponentKind::AcSource || c.kind == ComponentKind::Vcc ||
            c.kind == ComponentKind::PulseSource || c.kind == ComponentKind::Inductor ||
            (c.kind == ComponentKind::OpAmp && !IsRealOpAmp(c)) ||
            c.kind == ComponentKind::Vcvs || c.kind == ComponentKind::Transformer ||
            c.kind == ComponentKind::Ammeter || c.kind == ComponentKind::CurrentProbe) {
            topo.auxIndex[i] = static_cast<int>(topo.numAux++);
        }
    }
    return topo;
}

struct DeviceNonlinearState {
    std::vector<double> diodeVd;
    std::vector<double> bjtVbe;
    std::vector<double> bjtVbc;
    std::vector<double> mosVgs;
    std::vector<double> mosVds;
    std::vector<double> opAmpDiff;

    void Init(std::size_t n) {
        diodeVd.assign(n, 0.55);
        bjtVbe.assign(n, 0.65);
        bjtVbc.assign(n, -2.0);
        mosVgs.assign(n, 3.0);
        mosVds.assign(n, 2.0);
        opAmpDiff.assign(n, 0.0);
    }
};

struct TransientHistory {
    bool isTransient = false;
    bool useTrapezoidal = false;
    double stepDt = 1e-4;
    double t = 0.0;
    std::vector<bool> capUseTrap;
    std::vector<bool> indUseTrap;
    std::vector<double> capVoltPrev;
    std::vector<double> capCurrPrev;
    std::vector<double> indCurrPrev;
    std::vector<double> indVoltPrev;
    std::vector<double> motorOmegaPrev;
    std::vector<double> motorIaPrev;
    std::vector<bool> relayClosed;
    std::vector<bool> fuseBlown;
    std::vector<bool> scrLatched;
    std::vector<bool> ffStateQ;
    std::vector<double> ffPrevClk;
};

bool AssembleAndSolveStep(
    const std::vector<CircuitComponent>& comps,
    const MnaTopology& topo,
    double freqHz,
    double gMin,
    double sourceScale,
    DeviceNonlinearState& nl,
    const DeviceNonlinearState* acLinearizeBias,
    const TransientHistory* tran,
    const std::map<std::string, SignalGeneratorConfig>* generators,
    std::vector<std::complex<double>>& outSolVec)
{
    const int maxNode = topo.maxNode;
    const std::size_t dim = static_cast<std::size_t>(maxNode) + topo.numAux;
    if (maxNode <= 0 || dim == 0) return false;

    const double omega = 2.0 * kPi * (std::max)(0.0, freqHz);
    const bool isAc = (omega > 0.0 && (!tran || !tran->isTransient));
    const bool isTran = (tran && tran->isTransient);
    const DeviceNonlinearState& bias = (isAc && acLinearizeBias) ? *acLinearizeBias : nl;

    std::vector<std::vector<std::complex<double>>> M(dim, std::vector<std::complex<double>>(dim, { 0.0, 0.0 }));
    std::vector<std::complex<double>> rhs(dim, { 0.0, 0.0 });

    for (int k = 0; k < maxNode; ++k) {
        M[static_cast<std::size_t>(k)][static_cast<std::size_t>(k)] += std::complex<double>(gMin, 0.0);
    }

    auto addY = [&](int n1, int n2, std::complex<double> y) {
        if (n1 > 0) M[static_cast<std::size_t>(n1 - 1)][static_cast<std::size_t>(n1 - 1)] += y;
        if (n2 > 0) M[static_cast<std::size_t>(n2 - 1)][static_cast<std::size_t>(n2 - 1)] += y;
        if (n1 > 0 && n2 > 0) {
            M[static_cast<std::size_t>(n1 - 1)][static_cast<std::size_t>(n2 - 1)] -= y;
            M[static_cast<std::size_t>(n2 - 1)][static_cast<std::size_t>(n1 - 1)] -= y;
        }
    };

    auto evalSourceVal = [&](const CircuitComponent& c) -> std::complex<double> {
        if (isTran) {
            if (generators) {
                auto it = generators->find(c.name);
                if (it != generators->end()) {
                    const auto& g = it->second;
                    return { sourceScale * Detail::EvalWaveform(g.waveform, g.amplitude, g.dcOffset, g.freqHz,
                        g.phaseDeg, g.dutyCycle, g.riseSec, g.fallSec, g.modFreqHz, g.modIndex, tran->t), 0.0 };
                }
            }
            const double val = Detail::EvalWaveform(c.waveform, c.value, c.dcOffset, c.freqHz,
                c.phaseDeg, c.dutyCycle, c.riseSec, c.fallSec, c.modFreqHz, c.modIndex, tran->t);
            return { sourceScale * val, 0.0 };
        }
        if (isAc) {
            if (c.kind == ComponentKind::Battery || c.kind == ComponentKind::Vcc) {
                return { 0.0, 0.0 }; // DC rails are AC small-signal ground
            }
            return std::polar(sourceScale * c.value, c.phaseDeg * (kPi / 180.0));
        }
        if (c.kind == ComponentKind::AcSource) {
            return { sourceScale * c.dcOffset, 0.0 };
        }
        return { sourceScale * (c.dcOffset + c.value), 0.0 };
    };

    for (std::size_t i = 0; i < comps.size(); ++i) {
        const auto& c = comps[i];
        if (!c.active) continue;

        switch (c.kind) {
        case ComponentKind::Resistor: {
            const double r = (std::max)(1e-9, std::abs(c.value));
            addY(c.nodePos, c.nodeNeg, { 1.0 / r, 0.0 });
            break;
        }
        case ComponentKind::Potentiometer: {
            const double alpha = std::clamp(c.auxValue > 0.0 ? c.auxValue : 0.5, 0.001, 0.999);
            const double rTot = (std::max)(1e-6, std::abs(c.value));
            addY(c.nodePos, c.nodeOut, { 1.0 / ((1.0 - alpha) * rTot), 0.0 });
            addY(c.nodeOut, c.nodeNeg, { 1.0 / (alpha * rTot), 0.0 });
            break;
        }
        case ComponentKind::Ldr: {
            const double lux = (std::max)(0.1, c.value);
            const double r10 = Detail::GetParam(c, "R10", 10000.0);
            const double gamma = Detail::GetParam(c, "Gamma", 0.7);
            const double r = std::clamp(r10 * std::pow(lux / 10.0, -gamma), 50.0, 1e7);
            addY(c.nodePos, c.nodeNeg, { 1.0 / r, 0.0 });
            break;
        }
        case ComponentKind::NtcThermistor: {
            const double tempK = (c.value > -200.0 ? c.value : 25.0) + 273.15;
            const double r25 = Detail::GetParam(c, "R25", 10000.0);
            const double beta = Detail::GetParam(c, "B", 3950.0);
            const double r = std::clamp(r25 * std::exp(beta * (1.0 / tempK - 1.0 / 298.15)), 1.0, 1e8);
            addY(c.nodePos, c.nodeNeg, { 1.0 / r, 0.0 });
            break;
        }
        case ComponentKind::Capacitor:
        case ComponentKind::PolarizedCapacitor: {
            const double cap = (std::max)(1e-15, std::abs(c.value));
            if (isTran) {
                const bool trap = (i < tran->capUseTrap.size()) ? tran->capUseTrap[i] : tran->useTrapezoidal;
                const double gC = trap ? (2.0 * cap / tran->stepDt) : (cap / tran->stepDt);
                const double iHist = trap
                    ? (gC * tran->capVoltPrev[i] + tran->capCurrPrev[i])
                    : (gC * tran->capVoltPrev[i]);
                addY(c.nodePos, c.nodeNeg, { gC + 1e-12, 0.0 });
                if (c.nodePos > 0) rhs[static_cast<std::size_t>(c.nodePos - 1)] += iHist;
                if (c.nodeNeg > 0) rhs[static_cast<std::size_t>(c.nodeNeg - 1)] -= iHist;
            } else if (isAc) {
                addY(c.nodePos, c.nodeNeg, { 1e-12, omega * cap });
            } else {
                addY(c.nodePos, c.nodeNeg, { 1e-12, 0.0 });
            }
            break;
        }
        case ComponentKind::Inductor: {
            const std::size_t k = static_cast<std::size_t>(maxNode + topo.auxIndex[i]);
            const double ind = (std::max)(1e-12, std::abs(c.value));
            if (c.nodePos > 0) { M[static_cast<std::size_t>(c.nodePos - 1)][k] += 1.0; M[k][static_cast<std::size_t>(c.nodePos - 1)] += 1.0; }
            if (c.nodeNeg > 0) { M[static_cast<std::size_t>(c.nodeNeg - 1)][k] -= 1.0; M[k][static_cast<std::size_t>(c.nodeNeg - 1)] -= 1.0; }
            if (isTran) {
                const bool trap = (i < tran->indUseTrap.size()) ? tran->indUseTrap[i] : tran->useTrapezoidal;
                const double rEq = trap ? (2.0 * ind / tran->stepDt) : (ind / tran->stepDt);
                M[k][k] -= (rEq + 1e-9);
                rhs[k] = trap
                    ? (-rEq * tran->indCurrPrev[i] - tran->indVoltPrev[i])
                    : (-rEq * tran->indCurrPrev[i]);
            } else {
                M[k][k] -= std::complex<double>(1e-9, isAc ? (omega * ind) : 0.0);
            }
            break;
        }
        case ComponentKind::Crystal: {
            const double fs = (c.value > 1.0) ? c.value : 4e6;
            const double c0 = Detail::GetParam(c, "C0", 5e-12);
            const double cm = Detail::GetParam(c, "Cm", 20e-15);
            const double rm = Detail::GetParam(c, "Rm", 25.0);
            const double lm = 1.0 / (std::pow(2.0 * kPi * fs, 2) * cm);
            if (isAc) {
                const std::complex<double> zm(rm, omega * lm - 1.0 / ((std::max)(1e-18, omega * cm)));
                const std::complex<double> yTotal = std::complex<double>(1e-12, omega * c0) + (std::complex<double>(1.0, 0.0) / zm);
                addY(c.nodePos, c.nodeNeg, yTotal);
            } else if (isTran) {
                addY(c.nodePos, c.nodeNeg, { c0 / tran->stepDt + 1e-12, 0.0 });
            } else {
                addY(c.nodePos, c.nodeNeg, { 1e-11, 0.0 });
            }
            break;
        }
        case ComponentKind::Transformer: {
            const std::size_t k = static_cast<std::size_t>(maxNode + topo.auxIndex[i]);
            const double nRatio = (std::abs(c.value) > 1e-9) ? c.value : 1.0;
            const double lm = (c.auxValue > 1e-9) ? c.auxValue : 1.0;
            if (c.nodeOut > 0) { M[static_cast<std::size_t>(c.nodeOut - 1)][k] += 1.0; M[k][static_cast<std::size_t>(c.nodeOut - 1)] += 1.0; }
            if (c.nodeCtrlPos > 0) { M[static_cast<std::size_t>(c.nodeCtrlPos - 1)][k] -= 1.0; M[k][static_cast<std::size_t>(c.nodeCtrlPos - 1)] -= 1.0; }
            if (c.nodePos > 0) { M[static_cast<std::size_t>(c.nodePos - 1)][k] -= nRatio; M[k][static_cast<std::size_t>(c.nodePos - 1)] -= nRatio; }
            if (c.nodeNeg > 0) { M[static_cast<std::size_t>(c.nodeNeg - 1)][k] += nRatio; M[k][static_cast<std::size_t>(c.nodeNeg - 1)] += nRatio; }
            if (isAc) {
                addY(c.nodePos, c.nodeNeg, std::complex<double>(1.0, 0.0) / std::complex<double>(1e-3, omega * lm));
            } else {
                addY(c.nodePos, c.nodeNeg, { 1e-9, 0.0 });
            }
            break;
        }
        case ComponentKind::VoltageSource:
        case ComponentKind::Battery:
        case ComponentKind::AcSource:
        case ComponentKind::Vcc:
        case ComponentKind::PulseSource: {
            const std::size_t k = static_cast<std::size_t>(maxNode + topo.auxIndex[i]);
            const int nNeg = (c.kind == ComponentKind::Vcc) ? 0 : c.nodeNeg;
            if (c.nodePos > 0) { M[static_cast<std::size_t>(c.nodePos - 1)][k] += 1.0; M[k][static_cast<std::size_t>(c.nodePos - 1)] += 1.0; }
            if (nNeg > 0) { M[static_cast<std::size_t>(nNeg - 1)][k] -= 1.0; M[k][static_cast<std::size_t>(nNeg - 1)] -= 1.0; }
            rhs[k] = evalSourceVal(c);
            break;
        }
        case ComponentKind::CurrentSource: {
            const std::complex<double> cur = evalSourceVal(c);
            if (c.nodePos > 0) rhs[static_cast<std::size_t>(c.nodePos - 1)] -= cur;
            if (c.nodeNeg > 0) rhs[static_cast<std::size_t>(c.nodeNeg - 1)] += cur;
            break;
        }
        case ComponentKind::Vcvs: {
            const std::size_t k = static_cast<std::size_t>(maxNode + topo.auxIndex[i]);
            const double av = (c.auxValue != 0.0) ? c.auxValue : c.value;
            if (c.nodePos > 0) { M[static_cast<std::size_t>(c.nodePos - 1)][k] += 1.0; M[k][static_cast<std::size_t>(c.nodePos - 1)] += 1.0; }
            if (c.nodeNeg > 0) { M[static_cast<std::size_t>(c.nodeNeg - 1)][k] -= 1.0; M[k][static_cast<std::size_t>(c.nodeNeg - 1)] -= 1.0; }
            if (c.nodeCtrlPos > 0) M[k][static_cast<std::size_t>(c.nodeCtrlPos - 1)] -= av;
            if (c.nodeCtrlNeg > 0) M[k][static_cast<std::size_t>(c.nodeCtrlNeg - 1)] += av;
            break;
        }
        case ComponentKind::Vccs: {
            const double gm = (c.auxValue != 0.0) ? c.auxValue : c.value;
            if (c.nodePos > 0 && c.nodeCtrlPos > 0) M[static_cast<std::size_t>(c.nodePos - 1)][static_cast<std::size_t>(c.nodeCtrlPos - 1)] += gm;
            if (c.nodePos > 0 && c.nodeCtrlNeg > 0) M[static_cast<std::size_t>(c.nodePos - 1)][static_cast<std::size_t>(c.nodeCtrlNeg - 1)] -= gm;
            if (c.nodeNeg > 0 && c.nodeCtrlPos > 0) M[static_cast<std::size_t>(c.nodeNeg - 1)][static_cast<std::size_t>(c.nodeCtrlPos - 1)] -= gm;
            if (c.nodeNeg > 0 && c.nodeCtrlNeg > 0) M[static_cast<std::size_t>(c.nodeNeg - 1)][static_cast<std::size_t>(c.nodeCtrlNeg - 1)] += gm;
            break;
        }
        case ComponentKind::Ground: {
            addY(c.nodePos, 0, { 1e9, 0.0 });
            break;
        }
        case ComponentKind::Wire:
        case ComponentKind::Connector: {
            addY(c.nodePos, c.nodeNeg, { 1e9, 0.0 });
            break;
        }
        case ComponentKind::Switch:
        case ComponentKind::PushButton: {
            const bool closed = (c.auxValue > 0.5);
            addY(c.nodePos, c.nodeNeg, { closed ? 100.0 : 1e-9, 0.0 });
            break;
        }
        case ComponentKind::Relay: {
            const double rCoil = Detail::GetParam(c, "Rcoil", 100.0);
            addY(c.nodeCtrlPos, c.nodeCtrlNeg, { 1.0 / rCoil, 0.0 });
            const bool closed = isTran ? tran->relayClosed[i] : (c.auxValue > 0.5);
            addY(c.nodePos, c.nodeNeg, { closed ? 100.0 : 1e-9, 0.0 });
            break;
        }
        case ComponentKind::Fuse: {
            const bool blown = isTran ? tran->fuseBlown[i] : (c.auxValue < -0.5);
            addY(c.nodePos, c.nodeNeg, { blown ? 1e-9 : 20.0, 0.0 });
            break;
        }
        case ComponentKind::Motor: {
            const double ra = Detail::GetParam(c, "Ra", (c.value > 0.1 ? c.value : 4.0));
            const double la = Detail::GetParam(c, "La", 5e-3);
            const double ke = Detail::GetParam(c, "Ke", 0.1);
            if (isAc) {
                addY(c.nodePos, c.nodeNeg, std::complex<double>(1.0, 0.0) / std::complex<double>(ra, omega * la));
            } else if (isTran && i < tran->motorOmegaPrev.size() && i < tran->motorIaPrev.size()) {
                const double vEmf = ke * tran->motorOmegaPrev[i];
                const double rEq = ra + la / tran->stepDt;
                const double gMot = 1.0 / (std::max)(1e-6, rEq);
                const double iEq = gMot * (vEmf - (la / tran->stepDt) * tran->motorIaPrev[i]);
                addY(c.nodePos, c.nodeNeg, { gMot, 0.0 });
                if (c.nodePos > 0) rhs[static_cast<std::size_t>(c.nodePos - 1)] += iEq;
                if (c.nodeNeg > 0) rhs[static_cast<std::size_t>(c.nodeNeg - 1)] -= iEq;
            } else {
                addY(c.nodePos, c.nodeNeg, { 1.0 / ra, 0.0 });
            }
            break;
        }
        case ComponentKind::Antenna: {
            const double rAnt = (c.value > 1.0) ? c.value : 50.0;
            addY(c.nodePos, c.nodeNeg, { 1.0 / rAnt, 0.0 });
            break;
        }
        case ComponentKind::VoltageProbe:
        case ComponentKind::Voltmeter:
        case ComponentKind::Oscilloscope: {
            addY(c.nodePos, c.nodeNeg, { 1e-10, 0.0 });
            break;
        }
        case ComponentKind::Ammeter:
        case ComponentKind::CurrentProbe: {
            const std::size_t k = static_cast<std::size_t>(maxNode + topo.auxIndex[i]);
            if (c.nodePos > 0) { M[static_cast<std::size_t>(c.nodePos - 1)][k] += 1.0; M[k][static_cast<std::size_t>(c.nodePos - 1)] += 1.0; }
            if (c.nodeNeg > 0) { M[static_cast<std::size_t>(c.nodeNeg - 1)][k] -= 1.0; M[k][static_cast<std::size_t>(c.nodeNeg - 1)] -= 1.0; }
            break;
        }
        case ComponentKind::OpAmp: {
            if (!IsRealOpAmp(c)) {
                const std::size_t k = static_cast<std::size_t>(maxNode + topo.auxIndex[i]);
                if (c.nodeOut > 0) M[static_cast<std::size_t>(c.nodeOut - 1)][k] += 1.0;
                if (c.nodePos > 0) M[k][static_cast<std::size_t>(c.nodePos - 1)] += 1.0;
                if (c.nodeNeg > 0) M[k][static_cast<std::size_t>(c.nodeNeg - 1)] -= 1.0;
            } else {
                double a0 = 0.0, rIn = 0.0, rOut = 0.0, gbw = 0.0, vPp = 0.0, vNn = 0.0;
                GetOpAmpSpec(c, a0, rIn, rOut, gbw, vPp, vNn);
                addY(c.nodePos, c.nodeNeg, { 1.0 / rIn, 0.0 });
                const double gOut = 1.0 / rOut;
                addY(c.nodeOut, 0, { gOut, 0.0 });
                const double vMid = 0.5 * (vPp + vNn);
                const double vSw = (std::max)(0.5, 0.5 * (vPp - vNn) - 1.0);
                const double vDiff = bias.opAmpDiff[i];
                const double arg = std::clamp(a0 * vDiff / vSw, -20.0, 20.0);
                const double th = std::tanh(arg);
                const double sech2 = 1.0 - th * th;
                const double gainLocal = (std::max)(1e-6, a0 * sech2);
                if (isAc) {
                    const double fp = gbw / (std::max)(1.0, a0);
                    const std::complex<double> acGain = std::complex<double>(gainLocal, 0.0) / std::complex<double>(1.0, freqHz / fp);
                    const std::complex<double> gmOut = gOut * acGain;
                    if (c.nodeOut > 0 && c.nodePos > 0) M[static_cast<std::size_t>(c.nodeOut - 1)][static_cast<std::size_t>(c.nodePos - 1)] -= gmOut;
                    if (c.nodeOut > 0 && c.nodeNeg > 0) M[static_cast<std::size_t>(c.nodeOut - 1)][static_cast<std::size_t>(c.nodeNeg - 1)] += gmOut;
                } else {
                    const double vOutTarget = vMid + vSw * th;
                    const double vOutLin0 = vOutTarget - gainLocal * vDiff;
                    if (c.nodeOut > 0 && c.nodePos > 0) M[static_cast<std::size_t>(c.nodeOut - 1)][static_cast<std::size_t>(c.nodePos - 1)] -= gOut * gainLocal;
                    if (c.nodeOut > 0 && c.nodeNeg > 0) M[static_cast<std::size_t>(c.nodeOut - 1)][static_cast<std::size_t>(c.nodeNeg - 1)] += gOut * gainLocal;
                    if (c.nodeOut > 0) rhs[static_cast<std::size_t>(c.nodeOut - 1)] += gOut * vOutLin0;
                }
            }
            break;
        }
        case ComponentKind::Comparator: {
            addY(c.nodePos, c.nodeNeg, { 1e-7, 0.0 });
            const double vHi = Detail::GetParam(c, "Voh", 5.0);
            const double vLo = Detail::GetParam(c, "Vol", 0.0);
            const double rOut = Detail::GetParam(c, "Rout", 100.0);
            const double gOut = 1.0 / rOut;
            addY(c.nodeOut, 0, { gOut, 0.0 });
            const double vDiff = bias.opAmpDiff[i];
            const double slope = 0.005;
            const double x = std::clamp(vDiff / slope, -30.0, 30.0);
            const double sig = 1.0 / (1.0 + std::exp(-x));
            const double gain = (vHi - vLo) * sig * (1.0 - sig) / slope;
            if (!isAc) {
                const double vOut = vLo + (vHi - vLo) * sig;
                const double vLin = vOut - gain * vDiff;
                if (c.nodeOut > 0 && c.nodePos > 0) M[static_cast<std::size_t>(c.nodeOut - 1)][static_cast<std::size_t>(c.nodePos - 1)] -= gOut * gain;
                if (c.nodeOut > 0 && c.nodeNeg > 0) M[static_cast<std::size_t>(c.nodeOut - 1)][static_cast<std::size_t>(c.nodeNeg - 1)] += gOut * gain;
                if (c.nodeOut > 0) rhs[static_cast<std::size_t>(c.nodeOut - 1)] += gOut * vLin;
            }
            break;
        }
        case ComponentKind::VoltageRegulator:
        case ComponentKind::Ic: {
            const std::string m = ToUpperAscii(c.modelName);
            const bool isLm317 = (m == "LM317");
            const double rOut = 0.02;
            const double gOut = 1.0 / rOut;
            const double drop = Detail::GetParam(c, "Vdrop", 2.0);
            constexpr double kEps = 0.05;
            if (isLm317) {
                const double vRef = Detail::GetParam(c, "Vref", 1.25);
                const double vIn = bias.diodeVd[i];
                const double vAdj = bias.bjtVbe[i];
                const double u = vIn - drop;
                const double sU = std::sqrt(u * u + kEps * kEps);
                const double vAvail = 0.5 * (u + sU);
                const double dvAvail_dvIn = 0.5 * (1.0 + u / sU);

                const double vReg = vAdj + vRef;
                const double d = vReg - vAvail;
                const double sD = std::sqrt(d * d + kEps * kEps);
                const double vTarget = 0.5 * (vReg + vAvail - sD);
                const double dvT_dvReg = 0.5 * (1.0 - d / sD);
                const double dvT_dvAvail = 0.5 * (1.0 + d / sD);
                const double dvT_dvIn = dvT_dvAvail * dvAvail_dvIn;
                const double dvT_dvAdj = dvT_dvReg;

                if (c.nodeOut > 0) {
                    M[static_cast<std::size_t>(c.nodeOut - 1)][static_cast<std::size_t>(c.nodeOut - 1)] += gOut;
                    if (c.nodePos > 0) M[static_cast<std::size_t>(c.nodeOut - 1)][static_cast<std::size_t>(c.nodePos - 1)] -= gOut * dvT_dvIn;
                    if (c.nodeNeg > 0) M[static_cast<std::size_t>(c.nodeOut - 1)][static_cast<std::size_t>(c.nodeNeg - 1)] -= gOut * dvT_dvAdj;
                }
                if (!isAc) {
                    const double vLin = vTarget - dvT_dvIn * vIn - dvT_dvAdj * vAdj;
                    if (c.nodeOut > 0) rhs[static_cast<std::size_t>(c.nodeOut - 1)] += gOut * vLin;
                    if (c.nodeNeg > 0) rhs[static_cast<std::size_t>(c.nodeNeg - 1)] += 50e-6; // I_adj flows out of Adj pin
                }
                addY(c.nodePos, 0, { 1e-4, 0.0 });
            } else {
                const double vReg = Detail::GetParam(c, "Vreg", (m == "7812" ? 12.0 : (m == "7809" ? 9.0 : 5.0)));
                const double vIn = bias.diodeVd[i]; // tracks V(nodePos) - V(nodeNeg)
                const double u = vIn - drop;
                const double sU = std::sqrt(u * u + kEps * kEps);
                const double vAvail = 0.5 * (u + sU);
                const double dvAvail_dvIn = 0.5 * (1.0 + u / sU);
                const double d = vReg - vAvail;
                const double sD = std::sqrt(d * d + kEps * kEps);
                const double vTarget = 0.5 * (vReg + vAvail - sD);
                const double dvT_dvIn = 0.5 * (1.0 + d / sD) * dvAvail_dvIn;

                if (c.nodeOut > 0) M[static_cast<std::size_t>(c.nodeOut - 1)][static_cast<std::size_t>(c.nodeOut - 1)] += gOut;
                if (c.nodeOut > 0 && c.nodeNeg > 0) M[static_cast<std::size_t>(c.nodeOut - 1)][static_cast<std::size_t>(c.nodeNeg - 1)] -= gOut;
                if (c.nodeOut > 0 && c.nodePos > 0) M[static_cast<std::size_t>(c.nodeOut - 1)][static_cast<std::size_t>(c.nodePos - 1)] -= gOut * dvT_dvIn;
                if (c.nodeOut > 0 && c.nodeNeg > 0) M[static_cast<std::size_t>(c.nodeOut - 1)][static_cast<std::size_t>(c.nodeNeg - 1)] += gOut * dvT_dvIn;
                addY(c.nodePos, c.nodeNeg, { 1e-4, 0.0 });
                if (!isAc) {
                    const double vLin = vTarget - dvT_dvIn * vIn;
                    if (c.nodeOut > 0) rhs[static_cast<std::size_t>(c.nodeOut - 1)] += gOut * vLin;
                    if (c.nodePos > 0) rhs[static_cast<std::size_t>(c.nodePos - 1)] -= 5e-3 * sourceScale;
                    if (c.nodeNeg > 0) rhs[static_cast<std::size_t>(c.nodeNeg - 1)] += 5e-3 * sourceScale;
                }
            }
            break;
        }
        case ComponentKind::Diode:
        case ComponentKind::Led:
        case ComponentKind::SchottkyDiode:
        case ComponentKind::ZenerDiode:
        case ComponentKind::Photodiode: {
            double isSat = 0.0, nFac = 1.35, vz = 0.0, iPh = 0.0;
            Detail::GetDiodeSpec(c, isSat, nFac, vz, iPh);
            const double vt = 0.02585 * nFac;
            const double vd = std::clamp(bias.diodeVd[i], -60.0, 2.8);
            const double ev = std::exp(std::clamp(vd / vt, -50.0, 65.0));
            double id = isSat * (ev - 1.0) - iPh;
            double gd = (std::max)(1e-12, (isSat / vt) * ev);
            if (vz > 0.5) {
                const double ibv = 1e-3;
                const double revArg = std::clamp(-(vd + vz) / vt, -50.0, 65.0);
                const double erev = std::exp(revArg);
                id -= ibv * erev;
                gd += (ibv / vt) * erev;
            }
            if (isAc) {
                const double cj = Detail::GetParam(c, "Cj0", 4e-12);
                addY(c.nodePos, c.nodeNeg, { gd, omega * cj });
            } else {
                const double ieq = id - gd * vd;
                addY(c.nodePos, c.nodeNeg, { gd, 0.0 });
                if (c.nodePos > 0) rhs[static_cast<std::size_t>(c.nodePos - 1)] -= ieq;
                if (c.nodeNeg > 0) rhs[static_cast<std::size_t>(c.nodeNeg - 1)] += ieq;
            }
            break;
        }
        case ComponentKind::BjtNpn:
        case ComponentKind::BjtPnp: {
            const double pol = (c.kind == ComponentKind::BjtNpn) ? 1.0 : -1.0;
            const double isSat = Detail::GetParam(c, "Is", 1.4e-14);
            const double bf = Detail::GetParam(c, "Bf", 200.0);
            const double br = Detail::GetParam(c, "Br", 3.0);
            const double vaf = Detail::GetParam(c, "Vaf", 100.0);
            const double vt = 0.02585;

            const double vbe = std::clamp(bias.bjtVbe[i], -20.0, 1.1);
            const double vbc = std::clamp(bias.bjtVbc[i], -40.0, 1.1);
            const double ebe = std::exp(std::clamp(vbe / vt, -40.0, 36.0));
            const double ebc = std::exp(std::clamp(vbc / vt, -40.0, 36.0));
            const double iF = isSat * (ebe - 1.0);
            const double iR = isSat * (ebc - 1.0);
            const double qEarly = 1.0 + (std::max)(-0.9, (vbe - vbc) / vaf);

            const double gF = (isSat / vt) * ebe;
            const double gR = (isSat / vt) * ebc;

            const double iC = pol * ((iF - iR) * qEarly - iR / br);
            const double iB = pol * (iF / bf + iR / br);
            const double iE = -(iC + iB);

            const double dIc_dVbe = (std::max)(0.0, gF * qEarly + (iF - iR) / vaf);
            const double dIc_dVbc = -gR * (qEarly + 1.0 / br) - (iF - iR) / vaf;
            const double dIb_dVbe = (std::max)(1e-12, gF / bf);
            const double dIb_dVbc = (std::max)(1e-12, gR / br);

            // Terminal voltage derivatives: Vbe = pol*(Vb - Ve), Vbc = pol*(Vb - Vc)
            // Note pol^2 = 1 so dI_term / dV_node cancels pol!
            const double jCC = -dIc_dVbc + 1e-12;
            const double jCB = dIc_dVbe + dIc_dVbc;
            const double jCE = -dIc_dVbe;

            const double jBC = -dIb_dVbc;
            const double jBB = dIb_dVbe + dIb_dVbc + 1e-12;
            const double jBE = -dIb_dVbe;

            const double jEC = -(jCC + jBC);
            const double jEB = -(jCB + jBB);
            const double jEE = -(jCE + jBE) + 1e-12;

            const int nC = c.nodePos;
            const int nB = c.nodeOut;
            const int nE = c.nodeNeg;
            auto addM = [&](int r, int col, double val) {
                if (r > 0 && col > 0) M[static_cast<std::size_t>(r - 1)][static_cast<std::size_t>(col - 1)] += val;
            };
            addM(nC, nC, jCC); addM(nC, nB, jCB); addM(nC, nE, jCE);
            addM(nB, nC, jBC); addM(nB, nB, jBB); addM(nB, nE, jBE);
            addM(nE, nC, jEC); addM(nE, nB, jEB); addM(nE, nE, jEE);

            if (!isAc) {
                // Vb - Ve = pol*vbe, Vb - Vc = pol*vbc -> J * V_old in differences:
                const double dvbe_old = pol * vbe;
                const double dvbc_old = pol * vbc;
                const double linC = dIc_dVbe * dvbe_old + dIc_dVbc * dvbc_old;
                const double linB = dIb_dVbe * dvbe_old + dIb_dVbc * dvbc_old;
                const double linE = -(linC + linB);
                if (nC > 0) rhs[static_cast<std::size_t>(nC - 1)] -= (iC - linC);
                if (nB > 0) rhs[static_cast<std::size_t>(nB - 1)] -= (iB - linB);
                if (nE > 0) rhs[static_cast<std::size_t>(nE - 1)] -= (iE - linE);
            }
            break;
        }
        case ComponentKind::MosfetN:
        case ComponentKind::MosfetP:
        case ComponentKind::JfetN:
        case ComponentKind::JfetP: {
            const bool isP = (c.kind == ComponentKind::MosfetP || c.kind == ComponentKind::JfetP);
            const bool isJfet = (c.kind == ComponentKind::JfetN || c.kind == ComponentKind::JfetP);
            const double pol = isP ? -1.0 : 1.0;
            const std::string m = ToUpperAscii(c.modelName);
            double vto = isJfet ? -2.0 : ((m == "2N7000") ? 2.1 : 3.5);
            double kp = isJfet ? 2e-3 : ((m == "2N7000") ? 0.35 : (isP ? 6.0 : 8.5));
            double lam = 0.02;
            vto = Detail::GetParam(c, "Vto", vto);
            if (c.kind == ComponentKind::MosfetP && vto < 0.0) vto = -vto;
            kp = Detail::GetParam(c, "Kp", kp);
            lam = Detail::GetParam(c, "Lambda", lam);

            const double vgs = bias.mosVgs[i];
            const double vds = bias.mosVds[i];
            const bool rev = (vds < 0.0);
            const double vdsEff = rev ? -vds : vds;
            const double vgsEff = rev ? (vgs - vds) : vgs;
            const double vov = vgsEff - vto;

            double iDrain = 0.0, gm = 0.0, gds = 1e-10;
            if (vov > 0.0) {
                if (vdsEff < vov) {
                    iDrain = kp * (vov * vdsEff - 0.5 * vdsEff * vdsEff) * (1.0 + lam * vdsEff);
                    gm = kp * vdsEff * (1.0 + lam * vdsEff);
                    gds = (std::max)(1e-10, kp * (vov - vdsEff) * (1.0 + lam * vdsEff) +
                                             kp * (vov * vdsEff - 0.5 * vdsEff * vdsEff) * lam);
                } else {
                    iDrain = 0.5 * kp * vov * vov * (1.0 + lam * vdsEff);
                    gm = kp * vov * (1.0 + lam * vdsEff);
                    gds = (std::max)(1e-10, 0.5 * kp * vov * vov * lam);
                }
            } else {
                iDrain = 1e-10 * vdsEff;
                gds = 1e-10;
            }
            // When rev == true, nD and nS are swapped below, so current into nD has sign +pol * iDrain
            const double iSigned = pol * iDrain;

            const int nD = rev ? c.nodeNeg : c.nodePos;
            const int nS = rev ? c.nodePos : c.nodeNeg;
            const int nG = c.nodeOut;
            addY(nD, nS, { gds, 0.0 });
            if (nD > 0 && nG > 0) M[static_cast<std::size_t>(nD - 1)][static_cast<std::size_t>(nG - 1)] += gm;
            if (nD > 0 && nS > 0) M[static_cast<std::size_t>(nD - 1)][static_cast<std::size_t>(nS - 1)] -= gm;
            if (nS > 0 && nG > 0) M[static_cast<std::size_t>(nS - 1)][static_cast<std::size_t>(nG - 1)] -= gm;
            if (nS > 0) M[static_cast<std::size_t>(nS - 1)][static_cast<std::size_t>(nS - 1)] += gm;

            if (!isAc) {
                const double iEq = iSigned - pol * (gm * vgsEff + gds * vdsEff);
                if (nD > 0) rhs[static_cast<std::size_t>(nD - 1)] -= iEq;
                if (nS > 0) rhs[static_cast<std::size_t>(nS - 1)] += iEq;
            }
            // Gate leakage / JFET diode
            addY(nG, nS, { isJfet ? 1e-8 : 1e-11, 0.0 });
            break;
        }
        case ComponentKind::Scr:
        case ComponentKind::Triac: {
            const bool latched = isTran ? tran->scrLatched[i] : (c.auxValue > 0.5);
            addY(c.nodePos, c.nodeNeg, { latched ? 2.0 : 1e-8, 0.0 });
            addY(c.nodeOut, c.nodeNeg, { 1e-3, 0.0 }); // gate-cathode
            break;
        }
        case ComponentKind::GateAnd:
        case ComponentKind::GateOr:
        case ComponentKind::GateNot:
        case ComponentKind::GateNand:
        case ComponentKind::GateNor:
        case ComponentKind::GateXor: {
            const double vdd = Detail::GetParam(c, "Vdd", 5.0);
            const double vth = 0.5 * vdd;
            const double vslope = 0.05 * vdd;
            addY(c.nodePos, 0, { 1e-7, 0.0 });
            if (c.kind != ComponentKind::GateNot) addY(c.nodeNeg, 0, { 1e-7, 0.0 });
            const double rOut = 50.0;
            const double gOut = 1.0 / rOut;
            addY(c.nodeOut, 0, { gOut, 0.0 });

            const double va = bias.diodeVd[i];   // tracks V(nodePos)
            const double vb = bias.bjtVbe[i];    // tracks V(nodeNeg)
            auto sigFn = [&](double v) {
                return 1.0 / (1.0 + std::exp(-std::clamp((v - vth) / vslope, -30.0, 30.0)));
            };
            const double sa = sigFn(va);
            const double sb = sigFn(vb);
            const double dsa = sa * (1.0 - sa) / vslope;
            const double dsb = sb * (1.0 - sb) / vslope;

            double yBool = 0.0, dy_da = 0.0, dy_db = 0.0;
            if (c.kind == ComponentKind::GateNot) {
                yBool = 1.0 - sa;
                dy_da = -dsa;
            } else if (c.kind == ComponentKind::GateAnd) {
                yBool = sa * sb;
                dy_da = dsa * sb; dy_db = sa * dsb;
            } else if (c.kind == ComponentKind::GateOr) {
                yBool = 1.0 - (1.0 - sa) * (1.0 - sb);
                dy_da = dsa * (1.0 - sb); dy_db = (1.0 - sa) * dsb;
            } else if (c.kind == ComponentKind::GateNand) {
                yBool = 1.0 - sa * sb;
                dy_da = -dsa * sb; dy_db = -sa * dsb;
            } else if (c.kind == ComponentKind::GateNor) {
                yBool = (1.0 - sa) * (1.0 - sb);
                dy_da = -dsa * (1.0 - sb); dy_db = -(1.0 - sa) * dsb;
            } else if (c.kind == ComponentKind::GateXor) {
                yBool = sa * (1.0 - sb) + (1.0 - sa) * sb;
                dy_da = dsa * (1.0 - 2.0 * sb); dy_db = (1.0 - 2.0 * sa) * dsb;
            }
            const double vDrv = vdd * yBool;
            const double gA = gOut * vdd * dy_da;
            const double gB = gOut * vdd * dy_db;
            if (c.nodeOut > 0 && c.nodePos > 0) M[static_cast<std::size_t>(c.nodeOut - 1)][static_cast<std::size_t>(c.nodePos - 1)] -= gA;
            if (c.nodeOut > 0 && c.nodeNeg > 0 && c.kind != ComponentKind::GateNot) {
                M[static_cast<std::size_t>(c.nodeOut - 1)][static_cast<std::size_t>(c.nodeNeg - 1)] -= gB;
            }
            if (!isAc && c.nodeOut > 0) {
                const double iEq = gOut * vDrv - gA * va - gB * vb;
                rhs[static_cast<std::size_t>(c.nodeOut - 1)] += iEq;
            }
            break;
        }
        case ComponentKind::FlipFlopD:
        case ComponentKind::FlipFlopJK: {
            const double vdd = Detail::GetParam(c, "Vdd", 5.0);
            addY(c.nodePos, 0, { 1e-7, 0.0 });
            addY(c.nodeCtrlPos, 0, { 1e-7, 0.0 });
            const bool qState = isTran ? tran->ffStateQ[i] : (c.auxValue > 0.5);
            const double gOut = 1.0 / 50.0;
            addY(c.nodeOut, 0, { gOut, 0.0 });
            if (!isAc && c.nodeOut > 0) {
                rhs[static_cast<std::size_t>(c.nodeOut - 1)] += gOut * (qState ? vdd : 0.0);
            }
            break;
        }
        case ComponentKind::Multiplexer:
        case ComponentKind::Demultiplexer: {
            addY(c.nodeCtrlPos, 0, { 1e-7, 0.0 });
            const bool sel1 = (bias.diodeVd[i] >= 2.5);
            addY(c.nodePos, c.nodeOut, { sel1 ? 1e-9 : (1.0 / 60.0), 0.0 });
            addY(c.nodeNeg, c.nodeOut, { sel1 ? (1.0 / 60.0) : 1e-9, 0.0 });
            break;
        }
        default:
            addY(c.nodePos, c.nodeNeg, { 1e-9, 0.0 });
            break;
        }
    }

    if (!SolveComplexLinearSystem(M, rhs)) return false;
    outSolVec = std::move(rhs);
    return true;
}

double UpdateNonlinearStateWithDamping(
    const std::vector<CircuitComponent>& comps,
    int maxNode,
    const std::vector<std::complex<double>>& solVec,
    DeviceNonlinearState& nl)
{
    auto getV = [&](int n) -> double {
        return (n > 0 && n <= maxNode) ? solVec[static_cast<std::size_t>(n - 1)].real() : 0.0;
    };
    double maxDelta = 0.0;
    for (std::size_t i = 0; i < comps.size(); ++i) {
        const auto& c = comps[i];
        if (!c.active) continue;
        if (c.kind == ComponentKind::Diode || c.kind == ComponentKind::Led ||
            c.kind == ComponentKind::SchottkyDiode || c.kind == ComponentKind::ZenerDiode ||
            c.kind == ComponentKind::Photodiode) {
            double isSat = 0.0, nFac = 1.35, vz = 0.0, iPh = 0.0;
            Detail::GetDiodeSpec(c, isSat, nFac, vz, iPh);
            const double vt = 0.02585 * nFac;
            const double vCrit = vt * std::log(vt / (std::sqrt(2.0) * (std::max)(1e-16, isSat)));
            const double vRaw = getV(c.nodePos) - getV(c.nodeNeg);
            const double vNew = Detail::PnjLimZener(vRaw, nl.diodeVd[i], vt, vCrit, vz);
            maxDelta = (std::max)(maxDelta, std::abs(vNew - nl.diodeVd[i]));
            nl.diodeVd[i] = vNew;
        } else if (c.kind == ComponentKind::BjtNpn || c.kind == ComponentKind::BjtPnp) {
            const double pol = (c.kind == ComponentKind::BjtNpn) ? 1.0 : -1.0;
            const double isSat = Detail::GetParam(c, "Is", 1.4e-14);
            const double vt = 0.02585;
            const double vCrit = vt * std::log(vt / (std::sqrt(2.0) * isSat));
            const double vbeRaw = pol * (getV(c.nodeOut) - getV(c.nodeNeg));
            const double vbcRaw = pol * (getV(c.nodeOut) - getV(c.nodePos));
            const double vbeNew = Detail::PnjLim(vbeRaw, nl.bjtVbe[i], vt, vCrit);
            const double vbcNew = Detail::PnjLim(vbcRaw, nl.bjtVbc[i], vt, vCrit);
            maxDelta = (std::max)(maxDelta, std::abs(vbeNew - nl.bjtVbe[i]));
            maxDelta = (std::max)(maxDelta, std::abs(vbcNew - nl.bjtVbc[i]));
            nl.bjtVbe[i] = vbeNew;
            nl.bjtVbc[i] = vbcNew;
        } else if (c.kind == ComponentKind::MosfetN || c.kind == ComponentKind::MosfetP ||
                   c.kind == ComponentKind::JfetN || c.kind == ComponentKind::JfetP) {
            const bool isP = (c.kind == ComponentKind::MosfetP || c.kind == ComponentKind::JfetP);
            const double pol = isP ? -1.0 : 1.0;
            const std::string m = ToUpperAscii(c.modelName);
            double vto = (c.kind == ComponentKind::JfetN || c.kind == ComponentKind::JfetP)
                ? -2.0 : ((m == "2N7000") ? 2.1 : 3.5);
            vto = Detail::GetParam(c, "Vto", vto);
            if (c.kind == ComponentKind::MosfetP && vto < 0.0) vto = -vto;
            const double vgsRaw = pol * (getV(c.nodeOut) - getV(c.nodeNeg));
            const double vdsRaw = pol * (getV(c.nodePos) - getV(c.nodeNeg));
            const double vgsNew = Detail::FetLim(vgsRaw, nl.mosVgs[i], vto);
            const double vdsNew = nl.mosVds[i] + std::clamp(vdsRaw - nl.mosVds[i], -3.0, 3.0);
            maxDelta = (std::max)(maxDelta, std::abs(vgsNew - nl.mosVgs[i]));
            maxDelta = (std::max)(maxDelta, std::abs(vdsNew - nl.mosVds[i]));
            nl.mosVgs[i] = vgsNew;
            nl.mosVds[i] = vdsNew;
        } else if (IsRealOpAmp(c) || c.kind == ComponentKind::Comparator) {
            const double vDiff = getV(c.nodePos) - getV(c.nodeNeg);
            const double step = vDiff - nl.opAmpDiff[i];
            const double damp = (std::abs(step) > 0.02) ? 0.7 : 1.0;
            const double vNew = nl.opAmpDiff[i] + damp * step;
            maxDelta = (std::max)(maxDelta, std::abs(vNew - nl.opAmpDiff[i]));
            nl.opAmpDiff[i] = vNew;
        } else if (c.kind == ComponentKind::VoltageRegulator || c.kind == ComponentKind::Ic) {
            const bool isLm317 = (ToUpperAscii(c.modelName) == "LM317");
            if (isLm317) {
                const double vInRaw = getV(c.nodePos);
                const double vAdjRaw = getV(c.nodeNeg);
                const double vInNew = nl.diodeVd[i] + 0.65 * (vInRaw - nl.diodeVd[i]);
                const double vAdjNew = nl.bjtVbe[i] + 0.65 * (vAdjRaw - nl.bjtVbe[i]);
                maxDelta = (std::max)(maxDelta, std::abs(vInNew - nl.diodeVd[i]));
                maxDelta = (std::max)(maxDelta, std::abs(vAdjNew - nl.bjtVbe[i]));
                nl.diodeVd[i] = vInNew;
                nl.bjtVbe[i] = vAdjNew;
            } else {
                const double vInRaw = getV(c.nodePos) - getV(c.nodeNeg);
                const double vInNew = nl.diodeVd[i] + 0.65 * (vInRaw - nl.diodeVd[i]);
                maxDelta = (std::max)(maxDelta, std::abs(vInNew - nl.diodeVd[i]));
                nl.diodeVd[i] = vInNew;
            }
        } else if (c.kind >= ComponentKind::GateAnd && c.kind <= ComponentKind::GateXor) {
            const double va = getV(c.nodePos);
            const double vb = getV(c.nodeNeg);
            maxDelta = (std::max)(maxDelta, std::abs(va - nl.diodeVd[i]));
            maxDelta = (std::max)(maxDelta, std::abs(vb - nl.bjtVbe[i]));
            nl.diodeVd[i] = va;
            nl.bjtVbe[i] = vb;
        } else if (c.kind == ComponentKind::Multiplexer || c.kind == ComponentKind::Demultiplexer) {
            const double vsel = getV(c.nodeCtrlPos);
            maxDelta = (std::max)(maxDelta, std::abs(vsel - nl.diodeVd[i]));
            nl.diodeVd[i] = vsel;
        }
    }
    return maxDelta;
}

bool SolveNonlinearDcOrStep(
    const std::vector<CircuitComponent>& comps,
    const MnaTopology& topo,
    DeviceNonlinearState& nl,
    const TransientHistory* tran,
    const std::map<std::string, SignalGeneratorConfig>* generators,
    std::vector<std::complex<double>>& outSolVec)
{
    const bool hasNl = HasAnyNonlinear(comps);
    if (!hasNl) {
        return AssembleAndSolveStep(comps, topo, 0.0, 1e-12, 1.0, nl, nullptr, tran, generators, outSolVec);
    }

    // Pass 1: Direct NR with voltage limiting
    for (int iter = 0; iter < 80; ++iter) {
        if (!AssembleAndSolveStep(comps, topo, 0.0, 1e-12, 1.0, nl, nullptr, tran, generators, outSolVec)) {
            break;
        }
        const double maxDelta = UpdateNonlinearStateWithDamping(comps, topo.maxNode, outSolVec, nl);
        if (maxDelta < 1e-10) {
            // One final linear solve using updated nl so outSolVec and nl match to machine precision
            AssembleAndSolveStep(comps, topo, 0.0, 1e-12, 1.0, nl, nullptr, tran, generators, outSolVec);
            return true;
        }
    }

    // Pass 2: Gmin-stepping fallback (1e-4 -> 1e-12)
    DeviceNonlinearState nlGmin = nl;
    bool gminOk = false;
    for (double gm : { 1e-4, 1e-6, 1e-8, 1e-10, 1e-12 }) {
        gminOk = false;
        for (int iter = 0; iter < 60; ++iter) {
            if (!AssembleAndSolveStep(comps, topo, 0.0, gm, 1.0, nlGmin, nullptr, tran, generators, outSolVec)) break;
            if (UpdateNonlinearStateWithDamping(comps, topo.maxNode, outSolVec, nlGmin) < 1e-9) {
                gminOk = true;
                break;
            }
        }
        if (!gminOk) break;
    }
    if (gminOk) {
        nl = nlGmin;
        AssembleAndSolveStep(comps, topo, 0.0, 1e-12, 1.0, nl, nullptr, tran, generators, outSolVec);
        return true;
    }

    // Pass 3: Source-stepping fallback (0.1 -> 1.0)
    DeviceNonlinearState nlSrc;
    nlSrc.Init(comps.size());
    bool srcOk = false;
    for (double sc : { 0.1, 0.25, 0.5, 0.75, 1.0 }) {
        srcOk = false;
        for (int iter = 0; iter < 60; ++iter) {
            if (!AssembleAndSolveStep(comps, topo, 0.0, 1e-10, sc, nlSrc, nullptr, tran, generators, outSolVec)) break;
            if (UpdateNonlinearStateWithDamping(comps, topo.maxNode, outSolVec, nlSrc) < 1e-9) {
                srcOk = true;
                break;
            }
        }
        if (!srcOk) break;
    }
    if (srcOk) {
        nl = nlSrc;
        AssembleAndSolveStep(comps, topo, 0.0, 1e-10, 1.0, nl, nullptr, tran, generators, outSolVec);
        return true;
    }
    return false;
}

} // namespace

// -------- Public ParseNetlistText (backwards-compatible + 53+ components + schematic JSON) --------

bool ParseNetlistText(
    const std::string& netlistUtf8,
    std::vector<CircuitComponent>& outComponents,
    std::string* outErr)
{
    outComponents.clear();
    if (netlistUtf8.find("\"docstudio_schematic_v1\"") != std::string::npos) {
        SchematicDocument sch;
        if (SchematicFromJson(netlistUtf8, sch)) {
            outComponents = CompileSchematicToMna(sch);
            return !outComponents.empty();
        }
    }

    struct SpiceModelCard {
        std::string typeUpper;
        std::map<std::string, double> params;
        double bv = 0.0;
    };
    std::map<std::string, SpiceModelCard> modelCards;
    std::vector<bool> wasExplicitKind;
    auto pushComp = [&](CircuitComponent comp, bool isExpl) {
        outComponents.push_back(std::move(comp));
        wasExplicitKind.push_back(isExpl);
    };

    std::string norm = netlistUtf8;
    std::replace(norm.begin(), norm.end(), ';', '\n');
    std::istringstream iss(norm);
    std::string line;
    while (std::getline(iss, line)) {
        const std::size_t first = line.find_first_not_of(" \t\r\n");
        if (first == std::string::npos) continue;
        line = line.substr(first);
        if (line[0] == '*' || line[0] == '#' || (line.size() >= 2 && line[0] == '/' && line[1] == '/')) continue;
        if (line[0] == '.') {
            const std::string low = ToUpperAscii(line);
            if (low.rfind(".MODEL", 0) == 0 &&
                (low.size() == 6 || std::isspace(static_cast<unsigned char>(low[6])))) {
                std::string mLine = line;
                for (char& ch : mLine) {
                    if (ch == '(' || ch == ')' || ch == ',') ch = ' ';
                }
                std::istringstream ms(mLine);
                std::vector<std::string> mtoks;
                std::string mw;
                while (ms >> mw) mtoks.push_back(mw);
                if (mtoks.size() >= 3) {
                    const std::string key = ToUpperAscii(mtoks[1]);
                    SpiceModelCard card;
                    card.typeUpper = ToUpperAscii(mtoks[2]);
                    for (std::size_t mi = 3; mi < mtoks.size(); ++mi) {
                        const auto eq = mtoks[mi].find('=');
                        if (eq != std::string::npos && eq > 0 && eq + 1 < mtoks[mi].size()) {
                            const std::string k = mtoks[mi].substr(0, eq);
                            const std::string v = mtoks[mi].substr(eq + 1);
                            double num = 0.0;
                            if (ParseScaledDouble(v, num)) {
                                card.params[k] = num;
                                if (ToUpperAscii(k) == "BV") card.bv = num;
                            }
                        }
                    }
                    modelCards[key] = std::move(card);
                }
                continue;
            }
            if (low.rfind(".END", 0) == 0 || low.rfind(".OP", 0) == 0 ||
                low.rfind(".AC", 0) == 0 || low.rfind(".TRAN", 0) == 0) {
                continue;
            }
        }

        std::istringstream ls(line);
        std::vector<std::string> toks;
        std::string w;
        while (ls >> w) toks.push_back(w);
        if (toks.empty()) continue;

        const std::string kw = ToUpperAscii(toks[0]);
        auto parseIntNode = [](const std::string& s, int& out) -> bool {
            if (ToUpperAscii(s) == "GND") { out = 0; return true; }
            std::string t = s;
            if ((t[0] == 'N' || t[0] == 'n') && t.size() > 1 && std::isdigit(static_cast<unsigned char>(t[1]))) {
                t = t.substr(1);
            }
            try {
                std::size_t idx = 0;
                out = std::stoi(t, &idx);
                return idx == t.size();
            } catch (...) {
                return false;
            }
        };

        auto parseKeyValParams = [&](std::size_t startIdx, CircuitComponent& c) {
            for (std::size_t ti = startIdx; ti < toks.size(); ++ti) {
                const auto eq = toks[ti].find('=');
                if (eq != std::string::npos && eq > 0 && eq + 1 < toks[ti].size()) {
                    const std::string k = toks[ti].substr(0, eq);
                    const std::string v = toks[ti].substr(eq + 1);
                    const std::string uk = ToUpperAscii(k);
                    double num = 0.0;
                    if (ParseScaledDouble(v, num)) {
                        c.params[k] = num;
                        if (uk == "VZ") c.auxValue = num;
                        else if (uk == "DCOFFSET" || uk == "DC") c.dcOffset = num;
                        else if (uk == "FREQ" || uk == "FREQHZ") c.freqHz = num;
                        else if (uk == "DUTY" || uk == "DUTYCYCLE") c.dutyCycle = num;
                        else if (uk == "PHASE" || uk == "PHASEDEG") c.phaseDeg = num;
                        else if (uk == "RISE" || uk == "RISESEC") c.riseSec = num;
                        else if (uk == "FALL" || uk == "FALLSEC") c.fallSec = num;
                    } else {
                        c.modelName = v;
                    }
                } else if (ti == startIdx && c.modelName.empty() &&
                           std::isalnum(static_cast<unsigned char>(toks[ti][0]))) {
                    double tmp = 0.0;
                    if (!ParseScaledDouble(toks[ti], tmp)) {
                        c.modelName = toks[ti];
                    }
                }
            }
        };

        CircuitComponent c{};
        std::size_t idxOff = 0;
        bool explicitKeyword = false;
        auto isNodeToken = [&](const std::string& tok) -> bool {
            int dummy = 0;
            return parseIntNode(tok, dummy) && dummy >= 0;
        };
        const bool hasNameArg = (toks.size() >= 2 && !isNodeToken(toks[1]));

        if (kw == "OPAMP" && hasNameArg && toks.size() >= 5) {
            c.name = toks[1]; c.kind = ComponentKind::OpAmp; idxOff = 1; explicitKeyword = true;
        } else if (kw == "COMP" && hasNameArg && toks.size() >= 5) {
            c.name = toks[1]; c.kind = ComponentKind::Comparator; idxOff = 1; explicitKeyword = true;
        } else if ((kw == "GND" || kw == "GROUND") &&
                   (toks.size() == 2 || (toks.size() >= 3 && hasNameArg && toks.size() < 6))) {
            c.name = (toks.size() >= 3) ? toks[1] : "GND";
            c.kind = ComponentKind::Ground;
            if (!parseIntNode(toks.back(), c.nodePos)) {
                if (outErr) *outErr = "Nodo GND invalido: " + line;
                return false;
            }
            pushComp(std::move(c), true);
            continue;
        } else if (kw == "VCC" && hasNameArg && toks.size() >= 3) {
            c.name = toks[1];
            c.kind = ComponentKind::Vcc;
            if (!parseIntNode(toks[2], c.nodePos)) {
                if (outErr) *outErr = "Nodo VCC invalido: " + line;
                return false;
            }
            c.value = 5.0;
            if (toks.size() >= 4) ParseScaledDouble(toks[3], c.value);
            pushComp(std::move(c), true);
            continue;
        } else if (kw == "LED" && hasNameArg && toks.size() >= 4) {
            c.name = toks[1]; c.kind = ComponentKind::Led; idxOff = 1; explicitKeyword = true;
        } else if ((kw == "DZ" || kw == "ZENER") && hasNameArg && toks.size() >= 4) {
            c.name = toks[1]; c.kind = ComponentKind::ZenerDiode; idxOff = 1; explicitKeyword = true;
        } else if ((kw == "DS" || kw == "SCHOTTKY") && hasNameArg && toks.size() >= 4) {
            c.name = toks[1]; c.kind = ComponentKind::SchottkyDiode; idxOff = 1; explicitKeyword = true;
        } else if ((kw == "PD" || kw == "PHOTO") && hasNameArg && toks.size() >= 4) {
            c.name = toks[1]; c.kind = ComponentKind::Photodiode; idxOff = 1; explicitKeyword = true;
        } else if (kw == "CPOL" && hasNameArg && toks.size() >= 5) {
            c.name = toks[1]; c.kind = ComponentKind::PolarizedCapacitor; idxOff = 1; explicitKeyword = true;
        } else if (kw == "POT" && hasNameArg && toks.size() >= 6) {
            c.name = toks[1]; c.kind = ComponentKind::Potentiometer; idxOff = 1; explicitKeyword = true;
        } else if (kw == "LDR" && hasNameArg && toks.size() >= 5) {
            c.name = toks[1]; c.kind = ComponentKind::Ldr; idxOff = 1; explicitKeyword = true;
        } else if (kw == "NTC" && hasNameArg && toks.size() >= 5) {
            c.name = toks[1]; c.kind = ComponentKind::NtcThermistor; idxOff = 1; explicitKeyword = true;
        } else if (kw == "REG" && hasNameArg && toks.size() >= 5) {
            c.name = toks[1]; c.kind = ComponentKind::VoltageRegulator; idxOff = 1; explicitKeyword = true;
        } else if (kw == "IC" && hasNameArg && toks.size() >= 5) {
            c.name = toks[1]; c.kind = ComponentKind::Ic; idxOff = 1; explicitKeyword = true;
        } else if (kw == "BAT" && hasNameArg && toks.size() >= 5) {
            c.name = toks[1]; c.kind = ComponentKind::Battery; idxOff = 1; explicitKeyword = true;
        } else if (kw == "VAC" && hasNameArg && toks.size() >= 5) {
            c.name = toks[1]; c.kind = ComponentKind::AcSource; idxOff = 1; explicitKeyword = true;
        } else if (kw == "VPULSE" && hasNameArg && toks.size() >= 5) {
            c.name = toks[1]; c.kind = ComponentKind::PulseSource; idxOff = 1; explicitKeyword = true;
        } else if ((kw == "XFMR" || kw == "TR") && hasNameArg && toks.size() >= 7) {
            c.name = toks[1]; c.kind = ComponentKind::Transformer; idxOff = 1; explicitKeyword = true;
        } else if (kw == "XTAL" && hasNameArg && toks.size() >= 5) {
            c.name = toks[1]; c.kind = ComponentKind::Crystal; idxOff = 1; explicitKeyword = true;
        } else if (kw == "SW" && hasNameArg && toks.size() >= 4) {
            c.name = toks[1]; c.kind = ComponentKind::Switch; idxOff = 1; explicitKeyword = true;
        } else if (kw == "PB" && hasNameArg && toks.size() >= 4) {
            c.name = toks[1]; c.kind = ComponentKind::PushButton; idxOff = 1; explicitKeyword = true;
        } else if (kw == "FUSE" && hasNameArg && toks.size() >= 4) {
            c.name = toks[1]; c.kind = ComponentKind::Fuse; idxOff = 1; explicitKeyword = true;
        } else if ((kw == "WIRE" || kw == "W") && hasNameArg && toks.size() >= 4) {
            c.name = toks[1]; c.kind = ComponentKind::Wire; idxOff = 1; explicitKeyword = true;
        } else if ((kw == "CONN" || kw == "CONNECTOR") && hasNameArg && toks.size() >= 4) {
            c.name = toks[1]; c.kind = ComponentKind::Connector; idxOff = 1; explicitKeyword = true;
        } else if (kw == "AND" && hasNameArg && toks.size() >= 5) {
            c.name = toks[1]; c.kind = ComponentKind::GateAnd; idxOff = 1; explicitKeyword = true;
        } else if (kw == "OR" && hasNameArg && toks.size() >= 5) {
            c.name = toks[1]; c.kind = ComponentKind::GateOr; idxOff = 1; explicitKeyword = true;
        } else if (kw == "NOT" && hasNameArg && toks.size() >= 4) {
            c.name = toks[1]; c.kind = ComponentKind::GateNot; idxOff = 1; explicitKeyword = true;
        } else if (kw == "NAND" && hasNameArg && toks.size() >= 5) {
            c.name = toks[1]; c.kind = ComponentKind::GateNand; idxOff = 1; explicitKeyword = true;
        } else if (kw == "NOR" && hasNameArg && toks.size() >= 5) {
            c.name = toks[1]; c.kind = ComponentKind::GateNor; idxOff = 1; explicitKeyword = true;
        } else if (kw == "XOR" && hasNameArg && toks.size() >= 5) {
            c.name = toks[1]; c.kind = ComponentKind::GateXor; idxOff = 1; explicitKeyword = true;
        } else if ((kw == "FF_D" || kw == "FFD") && hasNameArg && toks.size() >= 5) {
            c.name = toks[1]; c.kind = ComponentKind::FlipFlopD; idxOff = 1; explicitKeyword = true;
        } else if ((kw == "FF_JK" || kw == "FFJK") && hasNameArg && toks.size() >= 6) {
            c.name = toks[1]; c.kind = ComponentKind::FlipFlopJK; idxOff = 1; explicitKeyword = true;
        } else if (kw == "MUX" && hasNameArg && toks.size() >= 6) {
            c.name = toks[1]; c.kind = ComponentKind::Multiplexer; idxOff = 1; explicitKeyword = true;
        } else if (kw == "DMUX" && hasNameArg && toks.size() >= 6) {
            c.name = toks[1]; c.kind = ComponentKind::Demultiplexer; idxOff = 1; explicitKeyword = true;
        } else if ((kw == "VM" || kw == "VP") && hasNameArg && toks.size() >= 4) {
            c.name = toks[1]; c.kind = (kw == "VP") ? ComponentKind::VoltageProbe : ComponentKind::Voltmeter;
            idxOff = 1; explicitKeyword = true;
        } else if ((kw == "AM" || kw == "IP") && hasNameArg && toks.size() >= 4) {
            c.name = toks[1]; c.kind = (kw == "IP") ? ComponentKind::CurrentProbe : ComponentKind::Ammeter;
            idxOff = 1; explicitKeyword = true;
        } else if (kw == "OSC" && hasNameArg && toks.size() >= 4) {
            c.name = toks[1]; c.kind = ComponentKind::Oscilloscope; idxOff = 1; explicitKeyword = true;
        } else if (kw == "SCR" && hasNameArg && toks.size() >= 5) {
            c.name = toks[1]; c.kind = ComponentKind::Scr; idxOff = 1; explicitKeyword = true;
        } else if (kw == "TRIAC" && hasNameArg && toks.size() >= 5) {
            c.name = toks[1]; c.kind = ComponentKind::Triac; idxOff = 1; explicitKeyword = true;
        } else if (kw == "RLY" && hasNameArg && toks.size() >= 6) {
            c.name = toks[1]; c.kind = ComponentKind::Relay; idxOff = 1; explicitKeyword = true;
        } else if (kw == "MOT" && hasNameArg && toks.size() >= 4) {
            c.name = toks[1]; c.kind = ComponentKind::Motor; idxOff = 1; explicitKeyword = true;
        } else if (kw == "ANT" && hasNameArg && toks.size() >= 4) {
            c.name = toks[1]; c.kind = ComponentKind::Antenna; idxOff = 1; explicitKeyword = true;
        } else {
            c.name = toks[0];
        }

        const char lead = static_cast<char>(std::toupper(static_cast<unsigned char>(c.name[0])));
        if (!explicitKeyword && lead == 'W') {
            c.kind = ComponentKind::Wire;
        }
        const bool isDiodeLike = (c.kind == ComponentKind::Diode || c.kind == ComponentKind::Led ||
                                  c.kind == ComponentKind::ZenerDiode || c.kind == ComponentKind::SchottkyDiode ||
                                  c.kind == ComponentKind::Photodiode || (!explicitKeyword && lead == 'D'));
        const bool isNotGate = (c.kind == ComponentKind::GateNot);
        if (toks.size() < 4 + idxOff && !isDiodeLike && !isNotGate &&
            c.kind != ComponentKind::Switch && c.kind != ComponentKind::PushButton &&
            c.kind != ComponentKind::Voltmeter && c.kind != ComponentKind::Ammeter &&
            c.kind != ComponentKind::VoltageProbe && c.kind != ComponentKind::CurrentProbe &&
            c.kind != ComponentKind::Oscilloscope && c.kind != ComponentKind::Wire &&
            c.kind != ComponentKind::Connector && c.kind != ComponentKind::Fuse &&
            c.kind != ComponentKind::Motor && c.kind != ComponentKind::Antenna) {
            if (outErr) *outErr = "Linea de netlist incompleta: " + line;
            return false;
        }
        if ((isDiodeLike || c.kind == ComponentKind::Wire || c.kind == ComponentKind::Connector) &&
            toks.size() < 3 + idxOff) {
            if (outErr) *outErr = "Componente requiere al menos 2 nodos: " + line;
            return false;
        }

        if (c.kind == ComponentKind::GateNot) {
            if (!parseIntNode(toks[1 + idxOff], c.nodePos) || !parseIntNode(toks[2 + idxOff], c.nodeOut)) {
                if (outErr) *outErr = "Nodos invalidos en NOT: " + line;
                return false;
            }
            parseKeyValParams(3 + idxOff, c);
            pushComp(std::move(c), true);
            continue;
        }

        if (!parseIntNode(toks[1 + idxOff], c.nodePos) || !parseIntNode(toks[2 + idxOff], c.nodeNeg)) {
            if (outErr) *outErr = "Nodos deben ser enteros (0=GND): " + line;
            return false;
        }

        if (c.kind == ComponentKind::OpAmp || c.kind == ComponentKind::Comparator ||
            (!explicitKeyword && ((lead == 'O' && toks.size() >= 4) || (lead == 'X' && toks.size() >= 5)))) {
            if (c.kind == ComponentKind::Resistor) c.kind = ComponentKind::OpAmp;
            if (!parseIntNode(toks[3 + idxOff], c.nodeOut)) {
                if (outErr) *outErr = "Nodo de salida invalido en OpAmp/Comparator: " + line;
                return false;
            }
            c.value = 1e6;
            parseKeyValParams(4 + idxOff, c);
            if (!explicitKeyword && lead == 'X' && ToUpperAscii(c.modelName) == "OPAMP_IDEAL") {
                c.modelName.clear();
            }
            pushComp(std::move(c), explicitKeyword);
            continue;
        }

        if (c.kind == ComponentKind::VoltageRegulator || c.kind == ComponentKind::Ic) {
            if (!parseIntNode(toks[3 + idxOff], c.nodeOut)) {
                if (outErr) *outErr = "Nodo de salida invalido en Regulador/IC: " + line;
                return false;
            }
            c.value = 5.0;
            std::size_t paramIdx = 4 + idxOff;
            if (paramIdx < toks.size() && toks[paramIdx].find('=') == std::string::npos) {
                c.modelName = toks[paramIdx];
                ++paramIdx;
            }
            parseKeyValParams(paramIdx, c);
            pushComp(std::move(c), explicitKeyword);
            continue;
        }

        if (c.kind >= ComponentKind::GateAnd && c.kind <= ComponentKind::GateXor) {
            if (!parseIntNode(toks[3 + idxOff], c.nodeOut)) {
                if (outErr) *outErr = "Nodo de salida invalido en compuerta: " + line;
                return false;
            }
            parseKeyValParams(4 + idxOff, c);
            pushComp(std::move(c), explicitKeyword);
            continue;
        }

        if (c.kind == ComponentKind::FlipFlopD) {
            // FF_D <name> <D> <CLK> <Q> [Qn]
            if (!parseIntNode(toks[2 + idxOff], c.nodeCtrlPos) ||
                !parseIntNode(toks[3 + idxOff], c.nodeOut)) {
                if (outErr) *outErr = "Nodos invalidos en FlipFlopD: " + line;
                return false;
            }
            c.nodeNeg = 0;
            std::size_t nextIdx = 4 + idxOff;
            int qn = 0;
            if (nextIdx < toks.size() && parseIntNode(toks[nextIdx], qn)) {
                c.nodeCtrlNeg = qn;
                ++nextIdx;
            }
            parseKeyValParams(nextIdx, c);
            pushComp(std::move(c), explicitKeyword);
            continue;
        }

        if (c.kind == ComponentKind::FlipFlopJK) {
            // FF_JK <name> <J> <K> <CLK> <Q>
            if (!parseIntNode(toks[2 + idxOff], c.nodeCtrlNeg) ||
                !parseIntNode(toks[3 + idxOff], c.nodeCtrlPos) ||
                !parseIntNode(toks[4 + idxOff], c.nodeOut)) {
                if (outErr) *outErr = "Nodos invalidos en FlipFlopJK: " + line;
                return false;
            }
            c.nodeNeg = 0;
            parseKeyValParams(5 + idxOff, c);
            pushComp(std::move(c), explicitKeyword);
            continue;
        }

        if (c.kind == ComponentKind::Multiplexer || c.kind == ComponentKind::Demultiplexer) {
            // MUX/DMUX <name> <nodePos> <nodeNeg> <nodeOut> <SEL>
            if (!parseIntNode(toks[3 + idxOff], c.nodeOut) ||
                !parseIntNode(toks[4 + idxOff], c.nodeCtrlPos)) {
                if (outErr) *outErr = "Nodos invalidos en MUX/DMUX: " + line;
                return false;
            }
            parseKeyValParams(5 + idxOff, c);
            pushComp(std::move(c), explicitKeyword);
            continue;
        }

        if (c.kind == ComponentKind::Relay) {
            // RLY <name> <n+> <n-> <coil+> <coil-> [Vnom]
            if (!parseIntNode(toks[3 + idxOff], c.nodeCtrlPos) ||
                !parseIntNode(toks[4 + idxOff], c.nodeCtrlNeg)) {
                if (outErr) *outErr = "Nodos de bobina invalidos en Rele: " + line;
                return false;
            }
            c.value = 5.0;
            std::size_t nextIdx = 5 + idxOff;
            if (nextIdx < toks.size() && toks[nextIdx].find('=') == std::string::npos) {
                double vNom = 5.0;
                if (ParseScaledDouble(toks[nextIdx], vNom)) {
                    c.value = vNom;
                    ++nextIdx;
                }
            }
            parseKeyValParams(nextIdx, c);
            pushComp(std::move(c), explicitKeyword);
            continue;
        }

        if (!explicitKeyword && lead == 'Q') {
            // Q <name> <C> <B> <E> [model]
            if (toks.size() < 4 || !parseIntNode(toks[3], c.nodeOut)) {
                if (outErr) *outErr = "BJT requiere nodos C B E: " + line;
                return false;
            }
            // Note: standard SPICE Q1 C B E -> nodePos=C, nodeOut=B, nodeNeg=E
            const int nodeE = c.nodeOut;
            const int nodeB = c.nodeNeg;
            c.nodeOut = nodeB;
            c.nodeNeg = nodeE;
            c.kind = ComponentKind::BjtNpn;
            parseKeyValParams(4, c);
            pushComp(std::move(c), false);
            continue;
        }

        if (!explicitKeyword && (lead == 'M' || lead == 'J')) {
            // M <name> <D> <G> <S> [B] [model]  or J <name> <D> <G> <S> [model]
            if (toks.size() < 4) {
                if (outErr) *outErr = "FET requiere nodos D G S: " + line;
                return false;
            }
            const int nodeG = c.nodeNeg;
            int nodeS = 0;
            if (!parseIntNode(toks[3], nodeS)) {
                if (outErr) *outErr = "Nodo S invalido en FET: " + line;
                return false;
            }
            c.nodeOut = nodeG;
            c.nodeNeg = nodeS;
            std::size_t nextIdx = 4;
            int maybeBulk = 0;
            if (nextIdx < toks.size() && parseIntNode(toks[nextIdx], maybeBulk)) {
                c.nodeCtrlPos = maybeBulk;
                ++nextIdx;
            }
            c.kind = (lead == 'M') ? ComponentKind::MosfetN : ComponentKind::JfetN;
            parseKeyValParams(nextIdx, c);
            pushComp(std::move(c), false);
            continue;
        }

        if (c.kind == ComponentKind::Scr || c.kind == ComponentKind::Triac) {
            if (!parseIntNode(toks[3 + idxOff], c.nodeOut)) {
                if (outErr) *outErr = "Nodo Gate invalido en SCR/TRIAC: " + line;
                return false;
            }
            parseKeyValParams(4 + idxOff, c);
            pushComp(std::move(c), explicitKeyword);
            continue;
        }

        if (c.kind == ComponentKind::Potentiometer) {
            if (!parseIntNode(toks[3 + idxOff], c.nodeOut) || !ParseScaledDouble(toks[4 + idxOff], c.value)) {
                if (outErr) *outErr = "Potenciometro requiere POT name n1 n2 wiper R [alpha]: " + line;
                return false;
            }
            c.auxValue = 0.5;
            if (5 + idxOff < toks.size()) ParseScaledDouble(toks[5 + idxOff], c.auxValue);
            pushComp(std::move(c), explicitKeyword);
            continue;
        }

        if (c.kind == ComponentKind::Transformer) {
            if (!parseIntNode(toks[3 + idxOff], c.nodeOut) ||
                !parseIntNode(toks[4 + idxOff], c.nodeCtrlPos) ||
                !ParseScaledDouble(toks[5 + idxOff], c.value)) {
                if (outErr) *outErr = "Transformador requiere XFMR name p+ p- s+ s- ratio: " + line;
                return false;
            }
            c.auxValue = 1.0;
            if (6 + idxOff < toks.size()) ParseScaledDouble(toks[6 + idxOff], c.auxValue);
            pushComp(std::move(c), explicitKeyword);
            continue;
        }

        if (!explicitKeyword && (lead == 'E' || lead == 'G')) {
            c.kind = (lead == 'E') ? ComponentKind::Vcvs : ComponentKind::Vccs;
            if (toks.size() < 6 || !parseIntNode(toks[3], c.nodeCtrlPos) ||
                !parseIntNode(toks[4], c.nodeCtrlNeg) || !ParseScaledDouble(toks[5], c.value)) {
                if (outErr) *outErr = "Fuente dependiente requiere E/G name n+ n- nc+ nc- val: " + line;
                return false;
            }
            c.auxValue = c.value;
            pushComp(std::move(c), false);
            continue;
        }

        if (c.kind == ComponentKind::Diode || c.kind == ComponentKind::Led ||
            c.kind == ComponentKind::ZenerDiode || c.kind == ComponentKind::SchottkyDiode ||
            c.kind == ComponentKind::Photodiode || (!explicitKeyword && lead == 'D')) {
            if (!explicitKeyword) c.kind = ComponentKind::Diode;
            c.value = (c.kind == ComponentKind::ZenerDiode) ? 5.1 : 1e-12;
            if (3 + idxOff < toks.size()) {
                double customVal = 0.0;
                if (toks[3 + idxOff].find('=') != std::string::npos) {
                    parseKeyValParams(3 + idxOff, c);
                } else {
                    const bool acceptZero = (c.kind == ComponentKind::Photodiode);
                    if (ParseScaledDouble(toks[3 + idxOff], customVal) &&
                        (acceptZero ? (customVal >= 0.0) : (customVal > 0.0))) {
                        c.value = customVal;
                    } else {
                        c.modelName = toks[3 + idxOff];
                    }
                    parseKeyValParams(4 + idxOff, c);
                }
            }
            pushComp(std::move(c), explicitKeyword);
            continue;
        }

        if (c.kind == ComponentKind::Switch || c.kind == ComponentKind::PushButton) {
            c.auxValue = 1.0;
            if (3 + idxOff < toks.size()) {
                const std::string st = ToUpperAscii(toks[3 + idxOff]);
                if (st == "0" || st == "OFF" || st == "OPEN") c.auxValue = 0.0;
                else c.auxValue = 1.0;
            }
            pushComp(std::move(c), explicitKeyword);
            continue;
        }

        if (c.kind == ComponentKind::Voltmeter || c.kind == ComponentKind::Ammeter ||
            c.kind == ComponentKind::VoltageProbe || c.kind == ComponentKind::CurrentProbe ||
            c.kind == ComponentKind::Oscilloscope) {
            c.value = 1e10;
            pushComp(std::move(c), explicitKeyword);
            continue;
        }
        if (c.kind == ComponentKind::Wire || c.kind == ComponentKind::Connector) {
            c.value = 1e-9;
            pushComp(std::move(c), explicitKeyword);
            continue;
        }

        if (!explicitKeyword) {
            if (lead == 'R') c.kind = ComponentKind::Resistor;
            else if (lead == 'C') c.kind = ComponentKind::Capacitor;
            else if (lead == 'L') c.kind = ComponentKind::Inductor;
            else if (lead == 'V') c.kind = ComponentKind::VoltageSource;
            else if (lead == 'I') c.kind = ComponentKind::CurrentSource;
            else if (lead == 'W') {
                c.kind = ComponentKind::Wire;
                c.value = 1e-9;
                pushComp(std::move(c), false);
                continue;
            } else {
                if (outErr) *outErr = "Componente no reconocido: " + c.name;
                return false;
            }
        }

        std::size_t valIdx = 3 + idxOff;
        if ((c.kind == ComponentKind::VoltageSource || c.kind == ComponentKind::CurrentSource ||
             c.kind == ComponentKind::AcSource || c.kind == ComponentKind::PulseSource) &&
            valIdx < toks.size()) {
            const std::string tag = ToUpperAscii(toks[valIdx]);
            if (tag == "DC" || tag == "AC") {
                ++valIdx;
            } else if (tag == "SINE" || tag == "SQUARE" || tag == "TRIANGLE" || tag == "PULSE") {
                if (tag == "SINE") c.waveform = WaveformKind::Sine;
                else if (tag == "SQUARE") c.waveform = WaveformKind::Square;
                else if (tag == "TRIANGLE") c.waveform = WaveformKind::Triangle;
                else if (tag == "PULSE") c.waveform = WaveformKind::Pulse;
                ++valIdx;
            }
        }
        if (valIdx >= toks.size() && (c.kind == ComponentKind::Fuse || c.kind == ComponentKind::Motor || c.kind == ComponentKind::Antenna)) {
            c.value = (c.kind == ComponentKind::Antenna) ? 50.0 : ((c.kind == ComponentKind::Motor) ? 4.0 : 1.0);
            pushComp(std::move(c), explicitKeyword);
            continue;
        }
        if (valIdx >= toks.size() || !ParseScaledDouble(toks[valIdx], c.value)) {
            if (outErr) *outErr = "Valor invalido en: " + line;
            return false;
        }
        if (valIdx + 1 < toks.size()) {
            double p2 = 0.0;
            if (ParseScaledDouble(toks[valIdx + 1], p2)) {
                if (c.waveform != WaveformKind::Dc) c.freqHz = p2;
                else c.phaseDeg = p2;
            }
        }
        parseKeyValParams(valIdx + 1, c);
        pushComp(std::move(c), explicitKeyword);
    }

    auto isDefaultPreambleModel = [](const std::string& um) -> bool {
        return um == "D_DEFAULT" || um == "DS_DEFAULT" ||
               um == "D1N4148" || um == "D1N5819" || um == "DZ5V1" ||
               um == "Q2N2222" || um == "Q2N2907" || um == "MIRF540" || um == "MIRF9540";
    };
    auto hasParamCaseInsensitive = [](const CircuitComponent& comp, const std::string& key) -> bool {
        const std::string uk = ToUpperAscii(key);
        for (const auto& kv : comp.params) {
            if (ToUpperAscii(kv.first) == uk) return true;
        }
        return false;
    };

    for (std::size_t idx = 0; idx < outComponents.size(); ++idx) {
        auto& comp = outComponents[idx];
        const std::string um = ToUpperAscii(comp.modelName);
        const auto itMod = um.empty() ? modelCards.end() : modelCards.find(um);

        if (comp.kind == ComponentKind::BjtNpn || comp.kind == ComponentKind::BjtPnp) {
            if (itMod != modelCards.end()) {
                if (itMod->second.typeUpper == "PNP") comp.kind = ComponentKind::BjtPnp;
                else if (itMod->second.typeUpper == "NPN") comp.kind = ComponentKind::BjtNpn;
            } else if (um.find("PNP") != std::string::npos || um.find("2N2907") != std::string::npos ||
                       um.find("2N3906") != std::string::npos || um.find("BC557") != std::string::npos ||
                       um.find("BC327") != std::string::npos || um.find("2N5401") != std::string::npos ||
                       um.find("TIP32") != std::string::npos || um.find("TIP42") != std::string::npos) {
                comp.kind = ComponentKind::BjtPnp;
            }
        } else if (comp.kind == ComponentKind::MosfetN || comp.kind == ComponentKind::MosfetP) {
            if (itMod != modelCards.end()) {
                if (itMod->second.typeUpper == "PMOS") comp.kind = ComponentKind::MosfetP;
                else if (itMod->second.typeUpper == "NMOS") comp.kind = ComponentKind::MosfetN;
            } else if (um.find("PMOS") != std::string::npos || um.find("IRF9540") != std::string::npos ||
                       um.find("IRF9") != std::string::npos || um.find("BS250") != std::string::npos ||
                       um.find("2N6849") != std::string::npos) {
                comp.kind = ComponentKind::MosfetP;
            }
        } else if (comp.kind == ComponentKind::JfetN || comp.kind == ComponentKind::JfetP) {
            if (itMod != modelCards.end()) {
                if (itMod->second.typeUpper == "PJF") comp.kind = ComponentKind::JfetP;
                else if (itMod->second.typeUpper == "NJF") comp.kind = ComponentKind::JfetN;
            } else if (um.find("PJF") != std::string::npos || um.find("PMOS") != std::string::npos ||
                       um.find("2N5460") != std::string::npos || um.find("2N5461") != std::string::npos ||
                       um.find("J174") != std::string::npos || um.find("J17") != std::string::npos) {
                comp.kind = ComponentKind::JfetP;
            }
        } else if (!wasExplicitKind[idx] && comp.kind == ComponentKind::Diode) {
            if (um == "DS_DEFAULT" || um.find("SCHOTTKY") != std::string::npos ||
                um.find("BAT54") != std::string::npos) {
                comp.kind = ComponentKind::SchottkyDiode;
            } else if (um.find("ZENER") != std::string::npos || um.find("1N47") != std::string::npos ||
                       um.find("BZX") != std::string::npos || (itMod != modelCards.end() && itMod->second.bv > 0.5)) {
                comp.kind = ComponentKind::ZenerDiode;
                if (itMod != modelCards.end() && itMod->second.bv > 0.5) comp.value = itMod->second.bv;
            } else if (um.find("LED") != std::string::npos) {
                comp.kind = ComponentKind::Led;
            } else if (um.find("PHOTO") != std::string::npos) {
                comp.kind = ComponentKind::Photodiode;
            }
        }

        if (itMod != modelCards.end() && !isDefaultPreambleModel(um)) {
            for (const auto& kv : itMod->second.params) {
                if (!hasParamCaseInsensitive(comp, kv.first)) {
                    comp.params[kv.first] = kv.second;
                }
            }
        }
    }
    return !outComponents.empty();
}

// -------- Public SolveMnaCircuit (DC NR + AC Linearized at DC Operating Point) --------

bool HasFloatingCircuitIsland(const std::vector<CircuitComponent>& components, std::string& outErr) {
    std::unordered_set<int> usedNodes;
    std::unordered_map<int, std::vector<int>> adj;
    auto addEdge = [&](int a, int b) {
        if (a < 0 || b < 0) return;
        usedNodes.insert(a);
        usedNodes.insert(b);
        if (a != b) {
            adj[a].push_back(b);
            adj[b].push_back(a);
        }
    };
    for (const auto& c : components) {
        if (!c.active) continue;
        if (c.kind == ComponentKind::Ground || c.kind == ComponentKind::Vcc) {
            addEdge(c.nodePos, 0);
            if (c.nodeNeg > 0) addEdge(c.nodeNeg, 0);
        } else {
            addEdge(c.nodePos, c.nodeNeg);
        }
        if (c.kind == ComponentKind::OpAmp || c.kind == ComponentKind::Comparator ||
            c.kind == ComponentKind::VoltageRegulator || c.kind == ComponentKind::Ic ||
            c.kind == ComponentKind::GateAnd || c.kind == ComponentKind::GateOr ||
            c.kind == ComponentKind::GateNot || c.kind == ComponentKind::GateNand ||
            c.kind == ComponentKind::GateNor || c.kind == ComponentKind::GateXor ||
            c.kind == ComponentKind::FlipFlopD || c.kind == ComponentKind::FlipFlopJK ||
            c.kind == ComponentKind::Multiplexer || c.kind == ComponentKind::Demultiplexer) {
            addEdge(c.nodePos, c.nodeOut);
            addEdge(c.nodeNeg, c.nodeOut);
            addEdge(c.nodeOut, 0);
        } else if (c.kind == ComponentKind::BjtNpn || c.kind == ComponentKind::BjtPnp ||
                   c.kind == ComponentKind::MosfetN || c.kind == ComponentKind::MosfetP ||
                   c.kind == ComponentKind::JfetN || c.kind == ComponentKind::JfetP ||
                   c.kind == ComponentKind::Scr || c.kind == ComponentKind::Triac ||
                   c.kind == ComponentKind::Potentiometer) {
            addEdge(c.nodePos, c.nodeOut);
            addEdge(c.nodeNeg, c.nodeOut);
        } else if (c.kind == ComponentKind::Transformer) {
            addEdge(c.nodeOut, c.nodeCtrlPos);
            addEdge(c.nodePos, c.nodeOut);
        } else if (c.kind == ComponentKind::Vcvs || c.kind == ComponentKind::Vccs ||
                   c.kind == ComponentKind::Relay) {
            addEdge(c.nodeCtrlPos, c.nodeCtrlNeg);
        }
        for (int en : c.extraNodes) {
            addEdge(c.nodePos, en);
        }
    }
    if (usedNodes.empty()) {
        outErr = "Circuito vacio.";
        return true;
    }
    if (usedNodes.count(0) == 0) {
        outErr = "Nodo flotante detectado: el circuito no tiene referencia a tierra (nodo 0).";
        return true;
    }
    std::unordered_set<int> visited;
    std::vector<int> q;
    q.push_back(0);
    visited.insert(0);
    for (std::size_t qi = 0; qi < q.size(); ++qi) {
        const int u = q[qi];
        auto it = adj.find(u);
        if (it == adj.end()) continue;
        for (int v : it->second) {
            if (visited.insert(v).second) q.push_back(v);
        }
    }
    for (int n : usedNodes) {
        if (visited.count(n) == 0) {
            outErr = "Nodo flotante sin ruta a tierra (nodo " + std::to_string(n) + ").";
            return true;
        }
    }
    return false;
}

CircuitSolveResult SolveMnaCircuit(const std::vector<CircuitComponent>& components, double freqHz) {
    CircuitSolveResult res{};
    res.freqHz = freqHz;
    if (components.empty()) {
        res.errorUtf8 = "Circuito vacio.";
        return res;
    }

    const MnaTopology topo = BuildTopology(components);
    if (topo.maxNode <= 0) {
        res.errorUtf8 = "No hay nodos no-referencia (>0).";
        return res;
    }
    {
        std::string topoErr;
        if (HasFloatingCircuitIsland(components, topoErr)) {
            res.errorUtf8 = std::move(topoErr);
            return res;
        }
    }

    const double omega = 2.0 * kPi * (std::max)(0.0, freqHz);
    DeviceNonlinearState nl;
    nl.Init(components.size());
    DeviceNonlinearState dcBias;
    dcBias.Init(components.size());

    std::vector<std::complex<double>> solVec;
    if (omega == 0.0) {
        if (!SolveNonlinearDcOrStep(components, topo, nl, nullptr, nullptr, solVec)) {
            res.errorUtf8 = "Matriz MNA singular o sin convergencia NR (verifique tierra nodo 0 o lazo de fuentes de tension).";
            return res;
        }
    } else {
        const bool hasNl = HasAnyNonlinear(components);
        if (hasNl) {
            std::vector<std::complex<double>> dcSol;
            if (!SolveNonlinearDcOrStep(components, topo, dcBias, nullptr, nullptr, dcSol)) {
                res.errorUtf8 = "No convergio el punto de operacion DC previo al analisis AC.";
                return res;
            }
        }
        if (!AssembleAndSolveStep(components, topo, freqHz, 1e-12, 1.0, nl, hasNl ? &dcBias : nullptr, nullptr, nullptr, solVec)) {
            res.errorUtf8 = "Matriz MNA AC singular (verifique tierra nodo 0 o lazo de fuentes de tension).";
            return res;
        }
    }

    const DeviceNonlinearState& activeNl = (omega == 0.0) ? nl : dcBias;
    const int maxNode = topo.maxNode;
    auto nodeV = [&](int node) -> std::complex<double> {
        if (node <= 0 || node > maxNode) return { 0.0, 0.0 };
        return solVec[static_cast<std::size_t>(node - 1)];
    };

    for (int n = 1; n <= maxNode; ++n) {
        res.nodeVoltages[n] = nodeV(n);
    }

    const bool isAc = (omega > 0.0);
    const double pwrFactor = isAc ? 0.5 : 1.0;
    double sumPower = 0.0;
    for (std::size_t i = 0; i < components.size(); ++i) {
        const auto& c = components[i];
        if (!c.active) continue;
        BranchResult br;
        br.name = c.name;
        const std::complex<double> vp = nodeV(c.nodePos);
        const std::complex<double> vn = nodeV(c.nodeNeg);
        br.voltageDropV = vp - vn;
        bool customPowerComputed = false;

        if (c.kind == ComponentKind::Resistor) {
            br.currentA = br.voltageDropV / (std::max)(1e-9, std::abs(c.value));
        } else if (c.kind == ComponentKind::Potentiometer) {
            const double alpha = std::clamp(c.auxValue > 0.0 ? c.auxValue : 0.5, 0.001, 0.999);
            const double rTot = (std::max)(1e-6, std::abs(c.value));
            const std::complex<double> vOut = nodeV(c.nodeOut);
            const std::complex<double> iTop = (vp - vOut) / ((1.0 - alpha) * rTot);
            const std::complex<double> iBot = (vOut - vn) / (alpha * rTot);
            br.currentA = 0.5 * (iTop + iBot);
            br.activePowerW = pwrFactor * (((vp - vOut) * std::conj(iTop)).real() + ((vOut - vn) * std::conj(iBot)).real());
            customPowerComputed = true;
        } else if (c.kind == ComponentKind::Ldr) {
            const double lux = (std::max)(0.1, c.value);
            const double r10 = Detail::GetParam(c, "R10", 10000.0);
            const double gamma = Detail::GetParam(c, "Gamma", 0.7);
            const double r = std::clamp(r10 * std::pow(lux / 10.0, -gamma), 50.0, 1e7);
            br.currentA = br.voltageDropV / r;
        } else if (c.kind == ComponentKind::NtcThermistor) {
            const double tempK = (c.value > -200.0 ? c.value : 25.0) + 273.15;
            const double r25 = Detail::GetParam(c, "R25", 10000.0);
            const double beta = Detail::GetParam(c, "B", 3950.0);
            const double r = std::clamp(r25 * std::exp(beta * (1.0 / tempK - 1.0 / 298.15)), 1.0, 1e8);
            br.currentA = br.voltageDropV / r;
        } else if (c.kind == ComponentKind::Capacitor || c.kind == ComponentKind::PolarizedCapacitor) {
            br.currentA = br.voltageDropV * std::complex<double>(1e-12, omega * (std::max)(1e-15, std::abs(c.value)));
        } else if (c.kind == ComponentKind::Crystal) {
            const double fs = (c.value > 1.0) ? c.value : 4e6;
            const double c0 = Detail::GetParam(c, "C0", 5e-12);
            const double cm = Detail::GetParam(c, "Cm", 20e-15);
            const double rm = Detail::GetParam(c, "Rm", 25.0);
            const double lm = 1.0 / (std::pow(2.0 * kPi * fs, 2) * cm);
            if (isAc) {
                const std::complex<double> zm(rm, omega * lm - 1.0 / ((std::max)(1e-18, omega * cm)));
                const std::complex<double> yTotal = std::complex<double>(1e-12, omega * c0) + (std::complex<double>(1.0, 0.0) / zm);
                br.currentA = br.voltageDropV * yTotal;
            } else {
                br.currentA = br.voltageDropV * 1e-11;
            }
        } else if (c.kind == ComponentKind::Transformer) {
            const std::complex<double> iSec = solVec[static_cast<std::size_t>(maxNode + topo.auxIndex[i])];
            const double nRatio = (std::abs(c.value) > 1e-9) ? c.value : 1.0;
            const double lm = (c.auxValue > 1e-9) ? c.auxValue : 1.0;
            const std::complex<double> yMag = isAc
                ? (std::complex<double>(1.0, 0.0) / std::complex<double>(1e-3, omega * lm))
                : std::complex<double>(1e-9, 0.0);
            const std::complex<double> iPri = -nRatio * iSec + yMag * br.voltageDropV;
            const std::complex<double> vSec = nodeV(c.nodeOut) - nodeV(c.nodeCtrlPos);
            br.currentA = iPri;
            br.activePowerW = pwrFactor * ((br.voltageDropV * std::conj(iPri)).real() + (vSec * std::conj(iSec)).real());
            customPowerComputed = true;
        } else if (c.kind == ComponentKind::Inductor || c.kind == ComponentKind::VoltageSource ||
                   c.kind == ComponentKind::Battery || c.kind == ComponentKind::AcSource ||
                   c.kind == ComponentKind::Vcc || c.kind == ComponentKind::PulseSource ||
                   c.kind == ComponentKind::Vcvs || c.kind == ComponentKind::Ammeter ||
                   c.kind == ComponentKind::CurrentProbe) {
            if (c.kind == ComponentKind::Vcc) br.voltageDropV = vp;
            br.currentA = solVec[static_cast<std::size_t>(maxNode + topo.auxIndex[i])];
        } else if (c.kind == ComponentKind::CurrentSource) {
            br.currentA = isAc
                ? std::polar(c.value, c.phaseDeg * (kPi / 180.0))
                : std::complex<double>(c.dcOffset + c.value, 0.0);
        } else if (c.kind == ComponentKind::Vccs) {
            const double gm = (c.auxValue != 0.0) ? c.auxValue : c.value;
            br.currentA = gm * (nodeV(c.nodeCtrlPos) - nodeV(c.nodeCtrlNeg));
        } else if (c.kind == ComponentKind::OpAmp) {
            br.voltageDropV = nodeV(c.nodeOut);
            if (!IsRealOpAmp(c)) {
                br.currentA = -solVec[static_cast<std::size_t>(maxNode + topo.auxIndex[i])];
                br.activePowerW = pwrFactor * (br.voltageDropV * std::conj(-br.currentA)).real();
            } else {
                double a0 = 0.0, rIn = 0.0, rOut = 0.0, gbw = 0.0, vPp = 0.0, vNn = 0.0;
                GetOpAmpSpec(c, a0, rIn, rOut, gbw, vPp, vNn);
                const double gOut = 1.0 / rOut;
                const double vMid = 0.5 * (vPp + vNn);
                const double vSw = (std::max)(0.5, 0.5 * (vPp - vNn) - 1.0);
                const double vDiffBias = activeNl.opAmpDiff[i];
                const double arg = std::clamp(a0 * vDiffBias / vSw, -20.0, 20.0);
                const double th = std::tanh(arg);
                const double gainLocal = (std::max)(1e-6, a0 * (1.0 - th * th));
                const std::complex<double> iIn = (vp - vn) / rIn;
                std::complex<double> iOutSrc{ 0.0, 0.0 };
                if (isAc) {
                    const double fp = gbw / (std::max)(1.0, a0);
                    const std::complex<double> acGain = std::complex<double>(gainLocal, 0.0) / std::complex<double>(1.0, freqHz / fp);
                    iOutSrc = gOut * (acGain * (vp - vn) - br.voltageDropV);
                } else {
                    const double vOutTarget = vMid + vSw * th;
                    iOutSrc = { gOut * (vOutTarget - br.voltageDropV.real()), 0.0 };
                }
                br.currentA = iOutSrc; // current flowing out of OpAmp output into nodeOut
                br.activePowerW = pwrFactor * (((vp - vn) * std::conj(iIn)).real() +
                                               (br.voltageDropV * std::conj(-iOutSrc)).real());
            }
            customPowerComputed = true;
        } else if (c.kind == ComponentKind::Comparator) {
            const double vHi = Detail::GetParam(c, "Voh", 5.0);
            const double vLo = Detail::GetParam(c, "Vol", 0.0);
            const double rOut = Detail::GetParam(c, "Rout", 100.0);
            const double gOut = 1.0 / rOut;
            const std::complex<double> vOut = nodeV(c.nodeOut);
            const std::complex<double> iIn = (vp - vn) * 1e-7;
            std::complex<double> iOutIntoDev{ 0.0, 0.0 };
            if (isAc) {
                iOutIntoDev = gOut * vOut;
            } else {
                const double sig = 1.0 / (1.0 + std::exp(-std::clamp(activeNl.opAmpDiff[i] / 0.005, -30.0, 30.0)));
                const double vTarget = vLo + (vHi - vLo) * sig;
                iOutIntoDev = { gOut * (vOut.real() - vTarget), 0.0 };
            }
            br.voltageDropV = vOut;
            br.currentA = -iOutIntoDev;
            br.activePowerW = pwrFactor * (((vp - vn) * std::conj(iIn)).real() + (vOut * std::conj(iOutIntoDev)).real());
            customPowerComputed = true;
        } else if (c.kind == ComponentKind::VoltageRegulator || c.kind == ComponentKind::Ic) {
            const std::string m = ToUpperAscii(c.modelName);
            const bool isLm317 = (m == "LM317");
            const double rOut = 0.02;
            const double gOut = 1.0 / rOut;
            const double drop = Detail::GetParam(c, "Vdrop", 2.0);
            constexpr double kEps = 0.05;
            const std::complex<double> vOut = nodeV(c.nodeOut);
            if (isLm317) {
                const double vRef = Detail::GetParam(c, "Vref", 1.25);
                const double vIn = activeNl.diodeVd[i];
                const double vAdj = activeNl.bjtVbe[i];
                const double u = vIn - drop;
                const double sU = std::sqrt(u * u + kEps * kEps);
                const double vAvail = 0.5 * (u + sU);
                const double vReg = vAdj + vRef;
                const double d = vReg - vAvail;
                const double sD = std::sqrt(d * d + kEps * kEps);
                const double vTarget = 0.5 * (vReg + vAvail - sD);
                const std::complex<double> iPos = vp * 1e-4;
                const std::complex<double> iAdj = isAc ? std::complex<double>(0.0, 0.0) : std::complex<double>(-50e-6, 0.0);
                const std::complex<double> iOutIntoDev = isAc
                    ? gOut * (vOut - (0.5 * (1.0 + d / sD) * 0.5 * (1.0 + u / sU)) * vp - (0.5 * (1.0 - d / sD)) * vn)
                    : std::complex<double>(gOut * (vOut.real() - vTarget), 0.0);
                br.voltageDropV = vp - vOut;
                br.currentA = -iOutIntoDev;
                br.activePowerW = pwrFactor * ((vp * std::conj(iPos)).real() + (vn * std::conj(iAdj)).real() +
                                               (vOut * std::conj(iOutIntoDev)).real());
            } else {
                const double vReg = Detail::GetParam(c, "Vreg", (m == "7812" ? 12.0 : (m == "7809" ? 9.0 : 5.0)));
                const double vIn = activeNl.diodeVd[i];
                const double u = vIn - drop;
                const double sU = std::sqrt(u * u + kEps * kEps);
                const double vAvail = 0.5 * (u + sU);
                const double d = vReg - vAvail;
                const double sD = std::sqrt(d * d + kEps * kEps);
                const double vTarget = 0.5 * (vReg + vAvail - sD);
                const double dvT_dvIn = 0.5 * (1.0 + d / sD) * 0.5 * (1.0 + u / sU);
                const std::complex<double> iQuies = (vp - vn) * 1e-4 + (isAc ? std::complex<double>(0.0, 0.0) : std::complex<double>(5e-3, 0.0));
                const std::complex<double> iOutIntoDev = isAc
                    ? gOut * ((vOut - vn) - dvT_dvIn * (vp - vn))
                    : std::complex<double>(gOut * ((vOut.real() - vn.real()) - vTarget), 0.0);
                br.voltageDropV = vp - vOut;
                br.currentA = -iOutIntoDev;
                br.activePowerW = pwrFactor * (((vp - vn) * std::conj(iQuies)).real() +
                                               ((vOut - vn) * std::conj(iOutIntoDev)).real());
            }
            customPowerComputed = true;
        } else if (c.kind == ComponentKind::Diode || c.kind == ComponentKind::Led ||
                   c.kind == ComponentKind::SchottkyDiode || c.kind == ComponentKind::ZenerDiode ||
                   c.kind == ComponentKind::Photodiode) {
            double isSat = 0.0, nFac = 1.35, vz = 0.0, iPh = 0.0;
            Detail::GetDiodeSpec(c, isSat, nFac, vz, iPh);
            const double vt = 0.02585 * nFac;
            const double vd = std::clamp(activeNl.diodeVd[i], -60.0, 2.8);
            const double ev = std::exp(std::clamp(vd / vt, -50.0, 65.0));
            if (isAc) {
                double gd = (std::max)(1e-12, (isSat / vt) * ev);
                if (vz > 0.5) gd += (1e-3 / vt) * std::exp(std::clamp(-(vd + vz) / vt, -50.0, 65.0));
                const double cj = Detail::GetParam(c, "Cj0", 4e-12);
                br.currentA = br.voltageDropV * std::complex<double>(gd, omega * cj);
            } else {
                double id = isSat * (ev - 1.0) - iPh;
                if (vz > 0.5) id -= 1e-3 * std::exp(std::clamp(-(vd + vz) / vt, -50.0, 65.0));
                br.currentA = { id, 0.0 };
            }
        } else if (c.kind == ComponentKind::BjtNpn || c.kind == ComponentKind::BjtPnp) {
            const double pol = (c.kind == ComponentKind::BjtNpn) ? 1.0 : -1.0;
            const double isSat = Detail::GetParam(c, "Is", 1.4e-14);
            const double bf = Detail::GetParam(c, "Bf", 200.0);
            const double brR = Detail::GetParam(c, "Br", 3.0);
            const double vaf = Detail::GetParam(c, "Vaf", 100.0);
            const double vt = 0.02585;
            const double vbe = std::clamp(activeNl.bjtVbe[i], -20.0, 1.1);
            const double vbc = std::clamp(activeNl.bjtVbc[i], -40.0, 1.1);
            const double ebe = std::exp(std::clamp(vbe / vt, -40.0, 36.0));
            const double ebc = std::exp(std::clamp(vbc / vt, -40.0, 36.0));
            const double iF = isSat * (ebe - 1.0);
            const double iR = isSat * (ebc - 1.0);
            const double qEarly = 1.0 + (std::max)(-0.9, (vbe - vbc) / vaf);
            const std::complex<double> vC = vp, vB = nodeV(c.nodeOut), vE = vn;
            if (isAc) {
                const double gF = (isSat / vt) * ebe;
                const double gR = (isSat / vt) * ebc;
                const double dIc_dVbe = (std::max)(0.0, gF * qEarly + (iF - iR) / vaf);
                const double dIc_dVbc = -gR * (qEarly + 1.0 / brR) - (iF - iR) / vaf;
                const double dIb_dVbe = (std::max)(1e-12, gF / bf);
                const double dIb_dVbc = (std::max)(1e-12, gR / brR);
                const std::complex<double> iC = dIc_dVbe * (vB - vE) + dIc_dVbc * (vB - vC) + 1e-12 * vC;
                const std::complex<double> iB = dIb_dVbe * (vB - vE) + dIb_dVbc * (vB - vC) + 1e-12 * vB;
                const std::complex<double> iE = -(iC + iB) + 1e-12 * vE;
                br.currentA = iC;
                br.activePowerW = pwrFactor * ((vC * std::conj(iC)).real() + (vB * std::conj(iB)).real() + (vE * std::conj(iE)).real());
            } else {
                const double iC = pol * ((iF - iR) * qEarly - iR / brR);
                const double iB = pol * (iF / bf + iR / brR);
                const double iE = -(iC + iB);
                br.currentA = { iC, 0.0 };
                br.activePowerW = vC.real() * iC + vB.real() * iB + vE.real() * iE;
            }
            customPowerComputed = true;
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
            const double vgs = activeNl.mosVgs[i];
            const double vds = activeNl.mosVds[i];
            const bool rev = (vds < 0.0);
            const double vdsEff = rev ? -vds : vds;
            const double vgsEff = rev ? (vgs - vds) : vgs;
            const double vov = vgsEff - vto;
            double iDrain = 0.0, gm = 0.0, gds = 1e-10;
            if (vov > 0.0) {
                if (vdsEff < vov) {
                    iDrain = kp * (vov * vdsEff - 0.5 * vdsEff * vdsEff) * (1.0 + lam * vdsEff);
                    gm = kp * vdsEff * (1.0 + lam * vdsEff);
                    gds = (std::max)(1e-10, kp * (vov - vdsEff) * (1.0 + lam * vdsEff) +
                                             kp * (vov * vdsEff - 0.5 * vdsEff * vdsEff) * lam);
                } else {
                    iDrain = 0.5 * kp * vov * vov * (1.0 + lam * vdsEff);
                    gm = kp * vov * (1.0 + lam * vdsEff);
                    gds = (std::max)(1e-10, 0.5 * kp * vov * vov * lam);
                }
            } else {
                iDrain = 1e-10 * vdsEff;
                gds = 1e-10;
            }
            const int nD = rev ? c.nodeNeg : c.nodePos;
            const int nS = rev ? c.nodePos : c.nodeNeg;
            const std::complex<double> vD = nodeV(nD), vS = nodeV(nS), vG = nodeV(c.nodeOut);
            const double gGate = isJfet ? 1e-8 : 1e-11;
            const std::complex<double> iG = (vG - vS) * gGate;
            if (isAc) {
                const std::complex<double> iD_nD = gm * (vG - vS) + gds * (vD - vS);
                br.currentA = rev ? -iD_nD : iD_nD;
                br.activePowerW = pwrFactor * (((vD - vS) * std::conj(iD_nD)).real() + ((vG - vS) * std::conj(iG)).real());
            } else {
                const double iSigned = pol * iDrain; // into nD
                br.currentA = { rev ? -iSigned : iSigned, 0.0 };
                br.activePowerW = ((vD.real() - vS.real()) * iSigned) + ((vG.real() - vS.real()) * iG.real());
            }
            customPowerComputed = true;
        } else if (c.kind == ComponentKind::Scr || c.kind == ComponentKind::Triac) {
            const bool latched = (c.auxValue > 0.5);
            const std::complex<double> iMain = br.voltageDropV * (latched ? 2.0 : 1e-8);
            const std::complex<double> vGk = nodeV(c.nodeOut) - vn;
            const std::complex<double> iGate = vGk * 1e-3;
            br.currentA = iMain;
            br.activePowerW = pwrFactor * ((br.voltageDropV * std::conj(iMain)).real() + (vGk * std::conj(iGate)).real());
            customPowerComputed = true;
        } else if (c.kind >= ComponentKind::GateAnd && c.kind <= ComponentKind::GateXor) {
            const double vdd = Detail::GetParam(c, "Vdd", 5.0);
            const double gOut = 1.0 / 50.0;
            const std::complex<double> vOut = nodeV(c.nodeOut);
            std::complex<double> iOutIntoDev{ 0.0, 0.0 };
            if (!isAc) {
                const double vth = 0.5 * vdd;
                const double vslope = 0.05 * vdd;
                auto sigFn = [&](double v) { return 1.0 / (1.0 + std::exp(-std::clamp((v - vth) / vslope, -30.0, 30.0))); };
                const double sa = sigFn(activeNl.diodeVd[i]);
                const double sb = sigFn(activeNl.bjtVbe[i]);
                double yBool = 0.0;
                if (c.kind == ComponentKind::GateNot) yBool = 1.0 - sa;
                else if (c.kind == ComponentKind::GateAnd) yBool = sa * sb;
                else if (c.kind == ComponentKind::GateOr) yBool = 1.0 - (1.0 - sa) * (1.0 - sb);
                else if (c.kind == ComponentKind::GateNand) yBool = 1.0 - sa * sb;
                else if (c.kind == ComponentKind::GateNor) yBool = (1.0 - sa) * (1.0 - sb);
                else if (c.kind == ComponentKind::GateXor) yBool = sa * (1.0 - sb) + (1.0 - sa) * sb;
                iOutIntoDev = { gOut * (vOut.real() - vdd * yBool), 0.0 };
            }
            br.voltageDropV = vOut;
            br.currentA = -iOutIntoDev;
            br.activePowerW = pwrFactor * (vOut * std::conj(iOutIntoDev)).real();
            customPowerComputed = true;
        } else if (c.kind == ComponentKind::FlipFlopD || c.kind == ComponentKind::FlipFlopJK) {
            const double vdd = Detail::GetParam(c, "Vdd", 5.0);
            const double gOut = 1.0 / 50.0;
            const std::complex<double> vOut = nodeV(c.nodeOut);
            const std::complex<double> iOutIntoDev = isAc ? (gOut * vOut)
                : std::complex<double>(gOut * (vOut.real() - ((c.auxValue > 0.5) ? vdd : 0.0)), 0.0);
            br.voltageDropV = vOut;
            br.currentA = -iOutIntoDev;
            br.activePowerW = pwrFactor * (vOut * std::conj(iOutIntoDev)).real();
            customPowerComputed = true;
        } else if (c.kind == ComponentKind::Multiplexer || c.kind == ComponentKind::Demultiplexer) {
            const bool sel1 = (activeNl.diodeVd[i] >= 2.5);
            const std::complex<double> vOut = nodeV(c.nodeOut);
            const std::complex<double> i0 = (vp - vOut) * (sel1 ? 1e-9 : (1.0 / 60.0));
            const std::complex<double> i1 = (vn - vOut) * (sel1 ? (1.0 / 60.0) : 1e-9);
            br.currentA = sel1 ? i1 : i0;
            br.activePowerW = pwrFactor * (((vp - vOut) * std::conj(i0)).real() + ((vn - vOut) * std::conj(i1)).real());
            customPowerComputed = true;
        } else if (c.kind == ComponentKind::Relay) {
            const double rCoil = Detail::GetParam(c, "Rcoil", 100.0);
            const std::complex<double> vCoil = nodeV(c.nodeCtrlPos) - nodeV(c.nodeCtrlNeg);
            const std::complex<double> iCoil = vCoil / rCoil;
            const std::complex<double> iCont = br.voltageDropV * ((c.auxValue > 0.5) ? 100.0 : 1e-9);
            br.currentA = iCont;
            br.activePowerW = pwrFactor * ((br.voltageDropV * std::conj(iCont)).real() + (vCoil * std::conj(iCoil)).real());
            customPowerComputed = true;
        } else if (c.kind == ComponentKind::Fuse) {
            br.currentA = br.voltageDropV * ((c.auxValue < -0.5) ? 1e-9 : 20.0);
        } else if (c.kind == ComponentKind::Motor) {
            const double ra = Detail::GetParam(c, "Ra", (c.value > 0.1 ? c.value : 4.0));
            const double la = Detail::GetParam(c, "La", 5e-3);
            br.currentA = isAc ? (br.voltageDropV / std::complex<double>(ra, omega * la)) : (br.voltageDropV / ra);
        } else if (c.kind == ComponentKind::Antenna) {
            const double rAnt = (c.value > 1.0) ? c.value : 50.0;
            br.currentA = br.voltageDropV / rAnt;
        } else if (c.kind == ComponentKind::Switch || c.kind == ComponentKind::PushButton) {
            br.currentA = br.voltageDropV * ((c.auxValue > 0.5) ? 100.0 : 1e-9);
        } else if (c.kind == ComponentKind::Wire || c.kind == ComponentKind::Connector) {
            br.currentA = br.voltageDropV * 1e9;
        } else if (c.kind == ComponentKind::Ground) {
            br.voltageDropV = vp;
            br.currentA = vp * 1e9;
        }

        if (!customPowerComputed) {
            const std::complex<double> pC = br.voltageDropV * std::conj(br.currentA);
            br.activePowerW = pwrFactor * pC.real();
        }
        sumPower += br.activePowerW;
        res.branches.push_back(std::move(br));
    }

    res.totalPowerSumW = sumPower;

    for (int n = 1; n <= maxNode; ++n) {
        std::ostringstream eq;
        eq << "KCL Nodo " << n << " (V" << n << "=" << FmtComplex(nodeV(n)) << " V): ";
        bool firstTerm = true;
        for (const auto& c : components) {
            if (!c.active) continue;
            if (c.kind == ComponentKind::OpAmp) {
                if (c.nodeOut == n) {
                    if (!firstTerm) eq << " + ";
                    firstTerm = false;
                    eq << "I_" << c.name << "_out";
                }
                continue;
            }
            if (c.nodePos != n && c.nodeNeg != n) continue;
            if (!firstTerm) eq << " + ";
            firstTerm = false;
            eq << (c.nodePos == n ? "+" : "-") << "I(" << c.name << ")";
        }
        if (firstTerm) eq << "0";
        eq << " = 0";
        res.kclKvlEquations.push_back(eq.str());
    }
    for (const auto& c : components) {
        if (c.active && c.kind == ComponentKind::OpAmp) {
            res.kclKvlEquations.push_back("OpAmp " + c.name + ": V(" + std::to_string(c.nodePos) +
                                          ") - V(" + std::to_string(c.nodeNeg) + ") = 0");
        }
    }

    std::ostringstream sumOss;
    for (const auto& kv : res.nodeVoltages) {
        sumOss << "V(" << kv.first << ")=" << FmtComplex(kv.second) << " V  ";
    }
    for (const auto& b : res.branches) {
        sumOss << "| I(" << b.name << ")=" << FmtComplex(b.currentA) << " A ";
    }
    res.summaryUtf8 = sumOss.str();
    res.ok = true;
    return res;
}

CircuitSolveResult SolveMnaNetlist(const std::string& netlistUtf8, double freqHz) {
    std::vector<CircuitComponent> comps;
    std::string err;
    if (!ParseNetlistText(netlistUtf8, comps, &err)) {
        CircuitSolveResult r{};
        r.errorUtf8 = err;
        return r;
    }
    return SolveMnaCircuit(comps, freqHz);
}

std::vector<BodePoint> ComputeBodeSweep(
    const std::vector<CircuitComponent>& components,
    int outputNode,
    double fStartHz,
    double fEndHz,
    std::size_t numPoints)
{
    std::vector<BodePoint> sweep;
    if (numPoints < 2 || fStartHz <= 0.0 || fEndHz <= fStartHz) return sweep;
    const double logA = std::log10(fStartHz);
    const double logB = std::log10(fEndHz);
    sweep.reserve(numPoints);
    for (std::size_t i = 0; i < numPoints; ++i) {
        const double f = std::pow(10.0, logA + (static_cast<double>(i) / static_cast<double>(numPoints - 1)) * (logB - logA));
        const CircuitSolveResult sr = SolveMnaCircuit(components, f);
        if (!sr.ok) continue;
        auto it = sr.nodeVoltages.find(outputNode);
        if (it == sr.nodeVoltages.end()) continue;
        const double mag = (std::max)(1e-15, std::abs(it->second));
        const double db = 20.0 * std::log10(mag);
        const double ph = std::arg(it->second) * (180.0 / kPi);
        sweep.push_back({ f, db, ph });
    }
    return sweep;
}

DcSweepResult SimulateDcSweepMna(
    const std::vector<CircuitComponent>& components,
    const std::string& sourceNameOrIndex,
    double startVal,
    double endVal,
    std::size_t numPoints,
    int probedNode)
{
    DcSweepResult res;
    res.probedNode = probedNode;
    if (components.empty() || numPoints < 2) {
        res.errorUtf8 = "Parametros invalidos para Barrido DC.";
        return res;
    }

    std::size_t targetIdx = components.size();
    for (std::size_t i = 0; i < components.size(); ++i) {
        if (components[i].name == sourceNameOrIndex) { targetIdx = i; break; }
    }
    if (targetIdx >= components.size()) {
        try {
            const std::size_t parsedIdx = static_cast<std::size_t>(std::stoul(sourceNameOrIndex));
            if (parsedIdx < components.size()) targetIdx = parsedIdx;
        } catch (...) {}
    }
    if (targetIdx >= components.size()) {
        res.errorUtf8 = "Fuente no encontrada para Barrido DC: " + sourceNameOrIndex;
        return res;
    }
    res.sourceName = components[targetIdx].name;

    std::vector<CircuitComponent> swept = components;
    const MnaTopology topo = BuildTopology(swept);
    if (topo.maxNode <= 0 || probedNode <= 0 || probedNode > topo.maxNode) {
        res.errorUtf8 = "Nodo de prueba fuera de rango.";
        return res;
    }

    DeviceNonlinearState nl;
    nl.Init(swept.size());
    std::vector<std::complex<double>> solVec;

    res.points.reserve(numPoints);
    for (std::size_t k = 0; k < numPoints; ++k) {
        const double val = startVal + (endVal - startVal) * (static_cast<double>(k) / static_cast<double>(numPoints - 1));
        swept[targetIdx].value = val;
        if (!SolveNonlinearDcOrStep(swept, topo, nl, nullptr, nullptr, solVec)) {
            res.errorUtf8 = "Fallo de convergencia DC en valor " + std::to_string(val);
            return res;
        }
        const double vNode = solVec[static_cast<std::size_t>(probedNode - 1)].real();
        double iSrc = 0.0;
        if (topo.auxIndex[targetIdx] >= 0) {
            iSrc = solVec[static_cast<std::size_t>(topo.maxNode + topo.auxIndex[targetIdx])].real();
        } else if (swept[targetIdx].kind == ComponentKind::CurrentSource) {
            iSrc = val;
        }
        res.points.push_back({ val, vNode, iSrc });
    }
    res.ok = true;
    return res;
}

// -------- Quine-McCluskey Karnaugh Map Simplifier --------

KMapSolution SimplifyKarnaughMap(
    int numVars,
    const std::vector<int>& minterms,
    const std::vector<int>& dontCares,
    const std::vector<std::string>& varNames)
{
    KMapSolution sol{};
    if (numVars < 2 || numVars > 4) {
        sol.errorUtf8 = "Solo se soporta 2, 3 o 4 variables para Mapa K.";
        return sol;
    }
    sol.numVars = numVars;
    sol.varNames = varNames;
    while (static_cast<int>(sol.varNames.size()) < numVars) {
        sol.varNames.push_back(std::string(1, static_cast<char>('A' + sol.varNames.size())));
    }

    const int nCells = 1 << numVars;
    sol.gridValues.assign(static_cast<std::size_t>(nCells), 0);

    std::unordered_set<int> onesSet, dcSet, allOn;
    for (int m : minterms) {
        if (m >= 0 && m < nCells) {
            onesSet.insert(m);
            allOn.insert(m);
            sol.gridValues[static_cast<std::size_t>(m)] = 1;
        }
    }
    for (int d : dontCares) {
        if (d >= 0 && d < nCells && onesSet.count(d) == 0) {
            dcSet.insert(d);
            allOn.insert(d);
            sol.gridValues[static_cast<std::size_t>(d)] = -1;
        }
    }

    if (onesSet.empty()) {
        sol.ok = true;
        sol.minimizedSopUtf8 = "F = 0";
        return sol;
    }
    if (static_cast<int>(allOn.size()) == nCells) {
        sol.ok = true;
        sol.minimizedSopUtf8 = "F = 1";
        return sol;
    }

    struct Implicant {
        unsigned int bits = 0;
        unsigned int mask = 0;
        bool combined = false;
        bool operator==(const Implicant& o) const noexcept { return bits == o.bits && mask == o.mask; }
    };

    std::vector<Implicant> cur;
    for (int m : allOn) cur.push_back({ static_cast<unsigned int>(m), 0u, false });

    std::vector<Implicant> primes;
    while (!cur.empty()) {
        std::vector<Implicant> next;
        for (std::size_t i = 0; i < cur.size(); ++i) {
            for (std::size_t j = i + 1; j < cur.size(); ++j) {
                if (cur[i].mask != cur[j].mask) continue;
                const unsigned int diff = cur[i].bits ^ cur[j].bits;
                if (diff != 0u && (diff & (diff - 1u)) == 0u) {
                    cur[i].combined = true;
                    cur[j].combined = true;
                    Implicant comb{ cur[i].bits & ~diff, cur[i].mask | diff, false };
                    if (std::find(next.begin(), next.end(), comb) == next.end()) next.push_back(comb);
                }
            }
        }
        for (const auto& imp : cur) {
            if (!imp.combined && std::find(primes.begin(), primes.end(), imp) == primes.end()) primes.push_back(imp);
        }
        cur.swap(next);
    }

    auto covers = [&](const Implicant& imp, int m) -> bool {
        return ((static_cast<unsigned int>(m) ^ imp.bits) & ~imp.mask) == 0u;
    };

    auto formatImp = [&](const Implicant& imp) -> std::string {
        std::string term;
        for (int b = numVars - 1; b >= 0; --b) {
            const unsigned int bitMask = 1u << b;
            if ((imp.mask & bitMask) != 0u) continue;
            const std::size_t vIdx = static_cast<std::size_t>((numVars - 1) - b);
            term += sol.varNames[vIdx];
            if ((imp.bits & bitMask) == 0u) term += "'";
        }
        return term.empty() ? "1" : term;
    };

    std::unordered_set<int> uncovered = onesSet;
    std::vector<Implicant> chosen;
    std::vector<bool> used(primes.size(), false);

    for (int m : onesSet) {
        int coverCount = 0;
        std::size_t lastIdx = 0;
        for (std::size_t i = 0; i < primes.size(); ++i) {
            if (covers(primes[i], m)) { ++coverCount; lastIdx = i; }
        }
        if (coverCount == 1 && !used[lastIdx]) {
            used[lastIdx] = true;
            chosen.push_back(primes[lastIdx]);
            for (int om : onesSet) if (covers(primes[lastIdx], om)) uncovered.erase(om);
        }
    }

    while (!uncovered.empty()) {
        std::size_t bestIdx = primes.size();
        int bestCov = -1;
        for (std::size_t i = 0; i < primes.size(); ++i) {
            if (used[i]) continue;
            int cov = 0;
            for (int om : uncovered) if (covers(primes[i], om)) ++cov;
            if (cov > bestCov) { bestCov = cov; bestIdx = i; }
        }
        if (bestIdx >= primes.size() || bestCov <= 0) break;
        used[bestIdx] = true;
        chosen.push_back(primes[bestIdx]);
        for (auto it = uncovered.begin(); it != uncovered.end();) {
            if (covers(primes[bestIdx], *it)) it = uncovered.erase(it);
            else ++it;
        }
    }

    for (const auto& p : primes) sol.primeImplicants.push_back(formatImp(p));

    std::string sop = "F = ";
    for (std::size_t i = 0; i < chosen.size(); ++i) {
        if (i > 0) sop += " + ";
        sop += formatImp(chosen[i]);
    }
    sol.minimizedSopUtf8 = std::move(sop);
    sol.ok = true;
    return sol;
}

// -------- Backwards-Compatible CAD/EDA Exporters (delegating to circuit_exporters.hpp) --------

std::string ExportCircuitToSvg(const std::vector<CircuitComponent>& comps, const CircuitSolveResult* sol) {
    SchematicDocument sch = AutoLayoutFromNetlist(comps, SymbolStandard::IEC);
    return ExportSchematicToSvg(sch, sol);
}

std::string ExportCircuitToDxf(const std::vector<CircuitComponent>& comps, const CircuitSolveResult* sol) {
    SchematicDocument sch = AutoLayoutFromNetlist(comps, SymbolStandard::IEC);
    return ExportSchematicToDxf(sch, sol);
}

std::string ExportCircuitToKiCadNetlist(const std::vector<CircuitComponent>& comps, const std::string& title) {
    std::ostringstream net;
    net << "* " << title << " (KiCad/SPICE Netlist Export - Document Studio)\n";
    for (const auto& c : comps) {
        net << c.name << " N" << c.nodePos << " N" << c.nodeNeg;
        if (c.kind == ComponentKind::OpAmp) net << " N" << c.nodeOut << " OPAMP\n";
        else net << " " << c.value << "\n";
    }
    net << ".end\n";
    return net.str();
}

// -------- Transient MNA Simulator --------

TransientWaveformResult SimulateTransientMna(
    const std::vector<CircuitComponent>& components,
    double durationSec,
    std::size_t numSteps,
    double sourceFreqHz,
    int probedNode)
{
    TransientWaveformResult tr{};
    if (components.empty() || durationSec <= 0.0 || numSteps < 2) {
        tr.errorUtf8 = "Parametros invalidos para simulacion transitoria.";
        return tr;
    }

    std::vector<CircuitComponent> comps = components;
    if (sourceFreqHz > 0.0) {
        for (auto& c : comps) {
            if ((c.kind == ComponentKind::VoltageSource || c.kind == ComponentKind::CurrentSource ||
                 c.kind == ComponentKind::AcSource) && c.freqHz <= 0.0) {
                c.freqHz = sourceFreqHz;
                c.waveform = WaveformKind::Sine;
            }
        }
    }

    const MnaTopology topo = BuildTopology(comps);
    if (topo.maxNode <= 0) {
        tr.errorUtf8 = "No hay nodos no-referencia (>0).";
        return tr;
    }

    const int probe = std::clamp(probedNode, 1, topo.maxNode);
    tr.probedNode = probe;
    tr.durationSec = durationSec;
    const double dt = durationSec / static_cast<double>(numSteps);
    tr.dtSec = dt;

    for (int n = 1; n <= topo.maxNode; ++n) tr.nodeWaveforms[n].reserve(numSteps + 1);
    tr.timeSec.reserve(numSteps + 1);

    DeviceNonlinearState nl;
    nl.Init(comps.size());

    TransientHistory tran;
    tran.isTransient = true;
    tran.useTrapezoidal = false; // Backward-Euler preserves step-0 compatibility cleanly
    tran.capVoltPrev.assign(comps.size(), 0.0);
    tran.capCurrPrev.assign(comps.size(), 0.0);
    tran.indCurrPrev.assign(comps.size(), 0.0);
    tran.indVoltPrev.assign(comps.size(), 0.0);
    tran.motorOmegaPrev.assign(comps.size(), 0.0);
    tran.motorIaPrev.assign(comps.size(), 0.0);
    tran.relayClosed.assign(comps.size(), false);
    tran.fuseBlown.assign(comps.size(), false);
    tran.scrLatched.assign(comps.size(), false);
    tran.ffStateQ.assign(comps.size(), false);
    tran.ffPrevClk.assign(comps.size(), 0.0);
    std::vector<double> fuseThermalInt(comps.size(), 0.0);

    std::vector<std::complex<double>> solVec;
    for (std::size_t step = 0; step <= numSteps; ++step) {
        const double t = static_cast<double>(step) * dt;
        tran.t = t;
        tran.stepDt = (step == 0) ? (dt * 1e-9) : dt;

        if (!SolveNonlinearDcOrStep(comps, topo, nl, &tran, nullptr, solVec)) {
            tr.errorUtf8 = "Fallo de convergencia MNA en t=" + std::to_string(t) + " s.";
            return tr;
        }

        auto getV = [&](int n) -> double {
            return (n > 0 && n <= topo.maxNode) ? solVec[static_cast<std::size_t>(n - 1)].real() : 0.0;
        };

        for (std::size_t i = 0; i < comps.size(); ++i) {
            const auto& c = comps[i];
            if (!c.active) continue;
            if (c.kind == ComponentKind::Capacitor || c.kind == ComponentKind::PolarizedCapacitor) {
                const double vc = getV(c.nodePos) - getV(c.nodeNeg);
                tran.capCurrPrev[i] = (c.value / tran.stepDt) * (vc - tran.capVoltPrev[i]);
                tran.capVoltPrev[i] = vc;
            } else if (c.kind == ComponentKind::Inductor && topo.auxIndex[i] >= 0) {
                tran.indVoltPrev[i] = getV(c.nodePos) - getV(c.nodeNeg);
                tran.indCurrPrev[i] = solVec[static_cast<std::size_t>(topo.maxNode + topo.auxIndex[i])].real();
            } else if (c.kind == ComponentKind::Relay) {
                const double vCoil = std::abs(getV(c.nodeCtrlPos) - getV(c.nodeCtrlNeg));
                const double vNom = (c.value > 0.1) ? c.value : 5.0;
                if (!tran.relayClosed[i] && vCoil >= 0.7 * vNom) tran.relayClosed[i] = true;
                else if (tran.relayClosed[i] && vCoil <= 0.3 * vNom) tran.relayClosed[i] = false;
            } else if (c.kind == ComponentKind::Fuse) {
                const double vDrop = getV(c.nodePos) - getV(c.nodeNeg);
                const double iBr = vDrop * (tran.fuseBlown[i] ? 1e-9 : 20.0);
                const double iRat = (c.value > 1e-4) ? c.value : 1.0;
                fuseThermalInt[i] += (iBr * iBr) / (iRat * iRat) * tran.stepDt;
                if (fuseThermalInt[i] > 0.05) tran.fuseBlown[i] = true;
            } else if (c.kind == ComponentKind::Scr || c.kind == ComponentKind::Triac) {
                const double vAk = getV(c.nodePos) - getV(c.nodeNeg);
                const double vGk = getV(c.nodeOut) - getV(c.nodeNeg);
                const double vGt = Detail::GetParam(c, "Vgt", 0.7);
                const double iHold = Detail::GetParam(c, "Ih", 5e-3);
                const bool isTriac = (c.kind == ComponentKind::Triac);
                const double iAnode = vAk * (tran.scrLatched[i] ? 2.0 : 1e-8);
                if (!tran.scrLatched[i]) {
                    if (isTriac ? (std::abs(vGk) >= vGt && std::abs(vAk) > 0.5) : (vGk >= vGt && vAk > 0.5)) {
                        tran.scrLatched[i] = true;
                    }
                } else {
                    if (!(isTriac ? (std::abs(iAnode) >= iHold) : (iAnode >= iHold))) {
                        tran.scrLatched[i] = false;
                    }
                }
            } else if (c.kind == ComponentKind::Motor) {
                const double ra = Detail::GetParam(c, "Ra", (c.value > 0.1 ? c.value : 4.0));
                const double la = Detail::GetParam(c, "La", 5e-3);
                const double ke = Detail::GetParam(c, "Ke", 0.1);
                const double kt = Detail::GetParam(c, "Kt", ke);
                const double jRot = Detail::GetParam(c, "J", 1e-4);
                const double bVisc = Detail::GetParam(c, "B", 1e-4);
                const double vDrop = getV(c.nodePos) - getV(c.nodeNeg);
                const double vEmf = ke * tran.motorOmegaPrev[i];
                const double rEq = ra + la / tran.stepDt;
                const double iA = (vDrop - vEmf + (la / tran.stepDt) * tran.motorIaPrev[i]) / (std::max)(1e-6, rEq);
                tran.motorIaPrev[i] = iA;
                tran.motorOmegaPrev[i] = (jRot * tran.motorOmegaPrev[i] + tran.stepDt * kt * iA) /
                                         (std::max)(1e-12, jRot + bVisc * tran.stepDt);
            } else if (c.kind == ComponentKind::FlipFlopD) {
                const double vClk = getV(c.nodeCtrlPos);
                if (tran.ffPrevClk[i] < 2.5 && vClk >= 2.5) {
                    tran.ffStateQ[i] = (getV(c.nodePos) >= 2.5);
                }
                tran.ffPrevClk[i] = vClk;
            } else if (c.kind == ComponentKind::FlipFlopJK) {
                const double vClk = getV(c.nodeCtrlPos);
                if (tran.ffPrevClk[i] < 2.5 && vClk >= 2.5) {
                    const bool j = getV(c.nodePos) >= 2.5;
                    const bool kIn = getV(c.nodeCtrlNeg) >= 2.5;
                    if (j && !kIn) tran.ffStateQ[i] = true;
                    else if (!j && kIn) tran.ffStateQ[i] = false;
                    else if (j && kIn) tran.ffStateQ[i] = !tran.ffStateQ[i];
                }
                tran.ffPrevClk[i] = vClk;
            }
        }

        tr.timeSec.push_back(t);
        for (int n = 1; n <= topo.maxNode; ++n) tr.nodeWaveforms[n].push_back(getV(n));
    }

    const auto& wf = tr.nodeWaveforms[probe];
    if (!wf.empty()) {
        tr.vMax = wf.front();
        tr.vMin = wf.front();
        double sum = 0.0, sumSq = 0.0;
        for (double v : wf) {
            tr.vMax = (std::max)(tr.vMax, v);
            tr.vMin = (std::min)(tr.vMin, v);
            sum += v;
            sumSq += v * v;
        }
        const double invN = 1.0 / static_cast<double>(wf.size());
        tr.vDcAvg = sum * invN;
        tr.vRms = std::sqrt(sumSq * invN);
    }

    const double zeroSnap = 1e-12 * (std::max)(1.0, (std::max)(std::abs(tr.vMax), std::abs(tr.vMin)));
    auto fmtV = [zeroSnap](double v) noexcept { return (std::abs(v) < zeroSnap) ? 0.0 : v; };
    std::ostringstream oss;
    oss.precision(4);
    oss << "Osciloscopio Nodo " << probe << " [0.." << durationSec * 1000.0 << " ms]: "
        << "Vmax=" << fmtV(tr.vMax) << " V, Vmin=" << fmtV(tr.vMin) << " V, "
        << "Vrms=" << fmtV(tr.vRms) << " V, Vdc=" << fmtV(tr.vDcAvg) << " V";
    tr.summaryUtf8 = oss.str();
    tr.ok = true;
    return tr;
}

TransientWaveformResult SimulateTransientNetlist(
    const std::string& netlistUtf8,
    double durationSec,
    std::size_t numSteps,
    double sourceFreqHz,
    int probedNode)
{
    std::vector<CircuitComponent> comps;
    std::string err;
    if (!ParseNetlistText(netlistUtf8, comps, &err)) {
        TransientWaveformResult r{};
        r.errorUtf8 = err;
        return r;
    }
    return SimulateTransientMna(comps, durationSec, numSteps, sourceFreqHz, probedNode);
}

// -------- ~60 Hz Streaming Interactive Transient Session (F-249) --------

bool CircuitTransientSession::Initialize(
    const std::vector<CircuitComponent>& comps,
    double dtSec,
    std::size_t ringCapacity,
    bool useTrapezoidal,
    std::string* outErr)
{
    if (comps.empty() || dtSec <= 0.0 || ringCapacity < 16) {
        if (outErr) *outErr = "Parametros invalidos para sesion transitoria.";
        return false;
    }
    const MnaTopology topo = BuildTopology(comps);
    if (topo.maxNode <= 0) {
        if (outErr) *outErr = "No hay nodos no-referencia (>0).";
        return false;
    }
    initialComponents_ = comps;
    components_ = comps;
    dtSec_ = dtSec;
    ringCapacity_ = ringCapacity;
    useTrapezoidal_ = useTrapezoidal;
    maxNode_ = topo.maxNode;
    generators_.clear();
    Reset();
    initialized_ = true;
    return true;
}

void CircuitTransientSession::Reset() {
    components_ = initialComponents_;
    simTimeSec_ = 0.0;
    forceBackwardEulerNext_ = true;
    const std::size_t n = components_.size();
    capVoltPrev_.assign(n, 0.0);
    capCurrPrev_.assign(n, 0.0);
    indCurrPrev_.assign(n, 0.0);
    indVoltPrev_.assign(n, 0.0);
    diodeVdPrev_.assign(n, 0.55);
    bjtVbePrev_.assign(n, 0.65);
    bjtVbcPrev_.assign(n, -2.0);
    mosVgsPrev_.assign(n, 3.0);
    mosVdsPrev_.assign(n, 2.0);
    motorOmegaPrev_.assign(n, 0.0);
    motorIaPrev_.assign(n, 0.0);
    fuseThermalInt_.assign(n, 0.0);
    fuseBlown_.assign(n, false);
    relayClosed_.assign(n, false);
    scrLatched_.assign(n, false);
    ffStateQ_.assign(n, false);
    ffPrevClk_.assign(n, 0.0);
    lastNodeVoltages_.assign(static_cast<std::size_t>(maxNode_ + 1), 0.0);
    timeRing_.clear();
    channelRing_.clear();
    lastVoltmeters_.clear();
    lastAmmeters_.clear();
}

void CircuitTransientSession::SetSignalGenerator(const SignalGeneratorConfig& cfg) {
    generators_[cfg.sourceName] = cfg;
    forceBackwardEulerNext_ = true;
}

bool CircuitTransientSession::SetComponentValue(const std::string& name, double newValue, double newAux) {
    bool found = false;
    for (auto& c : components_) {
        if (c.name == name) {
            c.value = newValue;
            if (c.kind == ComponentKind::Potentiometer || c.kind == ComponentKind::Switch ||
                c.kind == ComponentKind::PushButton || c.kind == ComponentKind::Vcvs ||
                c.kind == ComponentKind::Vccs) {
                c.auxValue = newAux;
            }
            found = true;
        }
    }
    if (found) forceBackwardEulerNext_ = true;
    return found;
}

bool CircuitTransientSession::ToggleSwitch(const std::string& name, bool closed) {
    bool found = false;
    for (auto& c : components_) {
        if (c.name == name) {
            c.auxValue = closed ? 1.0 : 0.0;
            found = true;
        }
    }
    if (found) forceBackwardEulerNext_ = true;
    return found;
}

bool CircuitTransientSession::StepOneInternal(double stepDt, std::string* outErr) {
    const MnaTopology topo = BuildTopology(components_);
    DeviceNonlinearState nl;
    nl.diodeVd = diodeVdPrev_;
    nl.bjtVbe = bjtVbePrev_;
    nl.bjtVbc = bjtVbcPrev_;
    nl.mosVgs = mosVgsPrev_;
    nl.mosVds = mosVdsPrev_;
    nl.opAmpDiff.assign(components_.size(), 0.0);

    auto getVFrom = [&](const std::vector<std::complex<double>>& sv, int n) -> double {
        return (n > 0 && n <= topo.maxNode) ? sv[static_cast<std::size_t>(n - 1)].real() : 0.0;
    };

    double effectiveDt = stepDt;
    bool mainUseTrap = useTrapezoidal_;
    if (forceBackwardEulerNext_) {
        if (!useTrapezoidal_) {
            mainUseTrap = false;
        } else if (simTimeSec_ == 0.0 && timeRing_.empty()) {
            // Step 0 at t=0 already uses actualDt = stepDt * 1e-6; run BE for this tiny step.
            mainUseTrap = false;
        } else {
            // Live discontinuity at t > 0 (ToggleSwitch / SetComponentValue / SetSignalGenerator):
            // Refresh nonlinear & reactive prev states via a dt * 1e-6 BE micro-step without
            // damping LC energy or advancing Fuse/Motor/SCR/Relay/FlipFlop states.
            const double dtMicro = stepDt * 1e-6;
            TransientHistory tranMicro;
            tranMicro.isTransient = true;
            tranMicro.useTrapezoidal = false;
            tranMicro.stepDt = dtMicro;
            tranMicro.t = simTimeSec_;
            tranMicro.capVoltPrev = capVoltPrev_;
            tranMicro.capCurrPrev = capCurrPrev_;
            tranMicro.indCurrPrev = indCurrPrev_;
            tranMicro.indVoltPrev = indVoltPrev_;
            tranMicro.motorOmegaPrev = motorOmegaPrev_;
            tranMicro.motorIaPrev = motorIaPrev_;
            tranMicro.relayClosed = relayClosed_;
            tranMicro.fuseBlown = fuseBlown_;
            tranMicro.scrLatched = scrLatched_;
            tranMicro.ffStateQ = ffStateQ_;
            tranMicro.ffPrevClk = ffPrevClk_;
            std::vector<std::complex<double>> solMicro;
            if (SolveNonlinearDcOrStep(components_, topo, nl, &tranMicro, &generators_, solMicro)) {
                for (std::size_t i = 0; i < components_.size(); ++i) {
                    const auto& c = components_[i];
                    if (!c.active) continue;
                    if (c.kind == ComponentKind::Capacitor || c.kind == ComponentKind::PolarizedCapacitor) {
                        const double cap = (std::max)(1e-15, std::abs(c.value));
                        const double vDrop = getVFrom(solMicro, c.nodePos) - getVFrom(solMicro, c.nodeNeg);
                        capCurrPrev_[i] = (cap / dtMicro) * (vDrop - capVoltPrev_[i]);
                        capVoltPrev_[i] = vDrop;
                    } else if (c.kind == ComponentKind::Inductor && topo.auxIndex[i] >= 0) {
                        const double iInd = solMicro[static_cast<std::size_t>(topo.maxNode + topo.auxIndex[i])].real();
                        indCurrPrev_[i] = iInd;
                        indVoltPrev_[i] = (getVFrom(solMicro, c.nodePos) - getVFrom(solMicro, c.nodeNeg)) - 1e-9 * iInd;
                    }
                }
            }
            effectiveDt = stepDt * (1.0 - 1e-6);
            mainUseTrap = useTrapezoidal_;
        }
    }

    TransientHistory tran;
    tran.isTransient = true;
    tran.useTrapezoidal = mainUseTrap;
    tran.stepDt = effectiveDt;
    tran.t = simTimeSec_;
    if (mainUseTrap) {
        tran.capUseTrap.assign(components_.size(), true);
        tran.indUseTrap.assign(components_.size(), true);
    }
    tran.capVoltPrev = capVoltPrev_;
    tran.capCurrPrev = capCurrPrev_;
    tran.indCurrPrev = indCurrPrev_;
    tran.indVoltPrev = indVoltPrev_;
    tran.motorOmegaPrev = motorOmegaPrev_;
    tran.motorIaPrev = motorIaPrev_;
    tran.relayClosed = relayClosed_;
    tran.fuseBlown = fuseBlown_;
    tran.scrLatched = scrLatched_;
    tran.ffStateQ = ffStateQ_;
    tran.ffPrevClk = ffPrevClk_;

    const DeviceNonlinearState nlSaved = nl;
    std::vector<std::complex<double>> solVec;
    if (!SolveNonlinearDcOrStep(components_, topo, nl, &tran, &generators_, solVec)) {
        if (outErr) *outErr = "Fallo de convergencia transitoria en t=" + std::to_string(simTimeSec_);
        return false;
    }

    // 2-consecutive-reversal autonomous Trapezoidal lookahead (separates genuine LC oscillation from stiff RC/RL ringing)
    if (tran.useTrapezoidal) {
        std::vector<bool> capFlipped(components_.size(), false);
        std::vector<bool> indFlipped(components_.size(), false);
        std::vector<double> capVTrap(components_.size(), 0.0);
        std::vector<double> capITrap(components_.size(), 0.0);
        std::vector<double> indVTrap(components_.size(), 0.0);
        std::vector<double> indITrap(components_.size(), 0.0);
        bool anyFlipped = false;

        for (std::size_t i = 0; i < components_.size(); ++i) {
            const auto& c = components_[i];
            if (!c.active) continue;
            if (c.kind == ComponentKind::Capacitor || c.kind == ComponentKind::PolarizedCapacitor) {
                const double cap = (std::max)(1e-15, std::abs(c.value));
                const double vDrop = getVFrom(solVec, c.nodePos) - getVFrom(solVec, c.nodeNeg);
                const double iTrap = (2.0 * cap / effectiveDt) * (vDrop - capVoltPrev_[i]) - capCurrPrev_[i];
                capVTrap[i] = vDrop;
                capITrap[i] = iTrap;
                if (iTrap * capCurrPrev_[i] < 0.0) {
                    capFlipped[i] = true;
                    anyFlipped = true;
                }
            } else if (c.kind == ComponentKind::Inductor && topo.auxIndex[i] >= 0) {
                const double iLTrap = solVec[static_cast<std::size_t>(topo.maxNode + topo.auxIndex[i])].real();
                const double vLTrap = (getVFrom(solVec, c.nodePos) - getVFrom(solVec, c.nodeNeg)) - 1e-9 * iLTrap;
                indVTrap[i] = vLTrap;
                indITrap[i] = iLTrap;
                if (vLTrap * indVoltPrev_[i] < 0.0) {
                    indFlipped[i] = true;
                    anyFlipped = true;
                }
            }
        }

        if (anyFlipped) {
            bool needFallback = false;
            TransientHistory tranLook = tran;
            tranLook.t = simTimeSec_; // hold external sources constant for autonomous mode check
            tranLook.stepDt = effectiveDt;
            tranLook.useTrapezoidal = true;
            tranLook.capUseTrap.assign(components_.size(), true);
            tranLook.indUseTrap.assign(components_.size(), true);
            for (std::size_t i = 0; i < components_.size(); ++i) {
                const auto& c = components_[i];
                if (!c.active) continue;
                if (c.kind == ComponentKind::Capacitor || c.kind == ComponentKind::PolarizedCapacitor) {
                    tranLook.capVoltPrev[i] = capVTrap[i];
                    tranLook.capCurrPrev[i] = capITrap[i];
                } else if (c.kind == ComponentKind::Inductor && topo.auxIndex[i] >= 0) {
                    tranLook.indVoltPrev[i] = indVTrap[i];
                    tranLook.indCurrPrev[i] = indITrap[i];
                }
            }
            DeviceNonlinearState nlLook = nl;
            std::vector<std::complex<double>> solLook;
            if (SolveNonlinearDcOrStep(components_, topo, nlLook, &tranLook, &generators_, solLook)) {
                for (std::size_t i = 0; i < components_.size(); ++i) {
                    const auto& c = components_[i];
                    if (!c.active) continue;
                    if (capFlipped[i]) {
                        const double cap = (std::max)(1e-15, std::abs(c.value));
                        const double vLook = getVFrom(solLook, c.nodePos) - getVFrom(solLook, c.nodeNeg);
                        const double iLook = (2.0 * cap / effectiveDt) * (vLook - capVTrap[i]) - capITrap[i];
                        if (iLook * capITrap[i] < 0.0 &&
                            std::abs(capITrap[i]) <= std::abs(capCurrPrev_[i]) * (1.0 + 1e-6)) {
                            tran.capUseTrap[i] = false;
                            needFallback = true;
                        }
                    }
                    if (indFlipped[i]) {
                        const double iLLook = solLook[static_cast<std::size_t>(topo.maxNode + topo.auxIndex[i])].real();
                        const double vLLook = (getVFrom(solLook, c.nodePos) - getVFrom(solLook, c.nodeNeg)) - 1e-9 * iLLook;
                        if (vLLook * indVTrap[i] < 0.0 &&
                            std::abs(indVTrap[i]) <= std::abs(indVoltPrev_[i]) * (1.0 + 1e-6)) {
                            tran.indUseTrap[i] = false;
                            needFallback = true;
                        }
                    }
                }
            }

            // Linearized nlSaved tangent 2-step Trap lookahead for nonlinear turn-off stiffness
            bool hasUnmarkedFlip = false;
            for (std::size_t i = 0; i < components_.size(); ++i) {
                if ((capFlipped[i] && tran.capUseTrap[i]) || (indFlipped[i] && tran.indUseTrap[i])) {
                    hasUnmarkedFlip = true;
                    break;
                }
            }
            if (hasUnmarkedFlip && HasAnyNonlinear(components_)) {
                DeviceNonlinearState nlLin = nlSaved;
                TransientHistory tranLin1 = tran;
                tranLin1.t = simTimeSec_;
                tranLin1.stepDt = effectiveDt;
                tranLin1.useTrapezoidal = true;
                tranLin1.capUseTrap.assign(components_.size(), true);
                tranLin1.indUseTrap.assign(components_.size(), true);
                std::vector<std::complex<double>> solLin1;
                if (AssembleAndSolveStep(components_, topo, 0.0, 1e-12, 1.0, nlLin, nullptr, &tranLin1, &generators_, solLin1)) {
                    TransientHistory tranLin2 = tranLin1;
                    std::vector<double> capILin1(components_.size(), 0.0);
                    std::vector<double> indVLin1(components_.size(), 0.0);
                    for (std::size_t i = 0; i < components_.size(); ++i) {
                        const auto& c = components_[i];
                        if (!c.active) continue;
                        if (c.kind == ComponentKind::Capacitor || c.kind == ComponentKind::PolarizedCapacitor) {
                            const double cap = (std::max)(1e-15, std::abs(c.value));
                            const double v1 = getVFrom(solLin1, c.nodePos) - getVFrom(solLin1, c.nodeNeg);
                            capILin1[i] = (2.0 * cap / effectiveDt) * (v1 - capVoltPrev_[i]) - capCurrPrev_[i];
                            tranLin2.capVoltPrev[i] = v1;
                            tranLin2.capCurrPrev[i] = capILin1[i];
                        } else if (c.kind == ComponentKind::Inductor && topo.auxIndex[i] >= 0) {
                            const double iL1 = solLin1[static_cast<std::size_t>(topo.maxNode + topo.auxIndex[i])].real();
                            indVLin1[i] = (getVFrom(solLin1, c.nodePos) - getVFrom(solLin1, c.nodeNeg)) - 1e-9 * iL1;
                            tranLin2.indVoltPrev[i] = indVLin1[i];
                            tranLin2.indCurrPrev[i] = iL1;
                        }
                    }
                    std::vector<std::complex<double>> solLin2;
                    if (AssembleAndSolveStep(components_, topo, 0.0, 1e-12, 1.0, nlLin, nullptr, &tranLin2, &generators_, solLin2)) {
                        for (std::size_t i = 0; i < components_.size(); ++i) {
                            const auto& c = components_[i];
                            if (!c.active) continue;
                            if (capFlipped[i] && tran.capUseTrap[i]) {
                                const double cap = (std::max)(1e-15, std::abs(c.value));
                                const double v2 = getVFrom(solLin2, c.nodePos) - getVFrom(solLin2, c.nodeNeg);
                                const double iLin2 = (2.0 * cap / effectiveDt) * (v2 - tranLin2.capVoltPrev[i]) - capILin1[i];
                                if (capILin1[i] * capCurrPrev_[i] < 0.0 && iLin2 * capILin1[i] < 0.0 &&
                                    std::abs(capILin1[i]) <= std::abs(capCurrPrev_[i]) * (1.0 + 1e-6)) {
                                    tran.capUseTrap[i] = false;
                                    needFallback = true;
                                }
                            }
                            if (indFlipped[i] && tran.indUseTrap[i]) {
                                const double iL2 = solLin2[static_cast<std::size_t>(topo.maxNode + topo.auxIndex[i])].real();
                                const double vL2 = (getVFrom(solLin2, c.nodePos) - getVFrom(solLin2, c.nodeNeg)) - 1e-9 * iL2;
                                if (indVLin1[i] * indVoltPrev_[i] < 0.0 && vL2 * indVLin1[i] < 0.0 &&
                                    std::abs(indVLin1[i]) <= std::abs(indVoltPrev_[i]) * (1.0 + 1e-6)) {
                                    tran.indUseTrap[i] = false;
                                    needFallback = true;
                                }
                            }
                        }
                    }
                }
            }

            if (needFallback) {
                DeviceNonlinearState nlBe = nlSaved;
                std::vector<std::complex<double>> solBe;
                if (SolveNonlinearDcOrStep(components_, topo, nlBe, &tran, &generators_, solBe)) {
                    solVec = std::move(solBe);
                    nl = std::move(nlBe);
                }
            }
        }
    }

    diodeVdPrev_ = nl.diodeVd;
    bjtVbePrev_ = nl.bjtVbe;
    bjtVbcPrev_ = nl.bjtVbc;
    mosVgsPrev_ = nl.mosVgs;
    mosVdsPrev_ = nl.mosVds;
    forceBackwardEulerNext_ = false;

    auto getV = [&](int n) -> double {
        return getVFrom(solVec, n);
    };

    std::map<int, double> nodeV;
    for (int n = 1; n <= topo.maxNode; ++n) {
        const double v = getV(n);
        nodeV[n] = v;
        lastNodeVoltages_[static_cast<std::size_t>(n)] = v;
    }

    std::map<std::string, double> branchI;
    for (std::size_t i = 0; i < components_.size(); ++i) {
        const auto& c = components_[i];
        if (!c.active) continue;
        const double vDrop = getV(c.nodePos) - getV(c.nodeNeg);
        double iBr = 0.0;

        if (c.kind == ComponentKind::Capacitor || c.kind == ComponentKind::PolarizedCapacitor) {
            const double cap = (std::max)(1e-15, std::abs(c.value));
            const bool trap = (i < tran.capUseTrap.size()) ? tran.capUseTrap[i] : tran.useTrapezoidal;
            iBr = trap
                ? ((2.0 * cap / effectiveDt) * (vDrop - capVoltPrev_[i]) - capCurrPrev_[i])
                : ((cap / effectiveDt) * (vDrop - capVoltPrev_[i]));
            capCurrPrev_[i] = iBr;
            capVoltPrev_[i] = vDrop;
        } else if (c.kind == ComponentKind::Inductor && topo.auxIndex[i] >= 0) {
            iBr = solVec[static_cast<std::size_t>(topo.maxNode + topo.auxIndex[i])].real();
            indCurrPrev_[i] = iBr;
            indVoltPrev_[i] = vDrop - 1e-9 * iBr;
        } else if (c.kind == ComponentKind::Resistor) {
            iBr = vDrop / (std::max)(1e-9, std::abs(c.value));
        } else if (topo.auxIndex[i] >= 0) {
            iBr = solVec[static_cast<std::size_t>(topo.maxNode + topo.auxIndex[i])].real();
        } else if (c.kind == ComponentKind::Diode || c.kind == ComponentKind::Led ||
                   c.kind == ComponentKind::SchottkyDiode || c.kind == ComponentKind::ZenerDiode ||
                   c.kind == ComponentKind::Photodiode) {
            double isSat = 0.0, nFac = 1.35, vz = 0.0, iPh = 0.0;
            Detail::GetDiodeSpec(c, isSat, nFac, vz, iPh);
            const double vt = 0.02585 * nFac;
            iBr = isSat * (std::exp(std::clamp(vDrop / vt, -50.0, 65.0)) - 1.0) - iPh;
            if (vz > 0.5) iBr -= 1e-3 * std::exp(std::clamp(-(vDrop + vz) / vt, -50.0, 65.0));
        } else if (c.kind == ComponentKind::Scr || c.kind == ComponentKind::Triac) {
            const bool wasLatched = scrLatched_[i];
            iBr = vDrop * (wasLatched ? 2.0 : 1e-8);
            const double vGk = getV(c.nodeOut) - getV(c.nodeNeg);
            const double vGt = Detail::GetParam(c, "Vgt", 0.7);
            const double iHold = Detail::GetParam(c, "Ih", 5e-3);
            const bool isTriac = (c.kind == ComponentKind::Triac);
            if (!wasLatched) {
                if (isTriac ? (std::abs(vGk) >= vGt && std::abs(vDrop) > 0.5) : (vGk >= vGt && vDrop > 0.5)) {
                    scrLatched_[i] = true;
                }
            } else {
                if (!(isTriac ? (std::abs(iBr) >= iHold) : (iBr >= iHold))) {
                    scrLatched_[i] = false;
                }
            }
        } else if (c.kind == ComponentKind::Motor) {
            const double ra = Detail::GetParam(c, "Ra", (c.value > 0.1 ? c.value : 4.0));
            const double la = Detail::GetParam(c, "La", 5e-3);
            const double ke = Detail::GetParam(c, "Ke", 0.1);
            const double kt = Detail::GetParam(c, "Kt", ke);
            const double jRot = Detail::GetParam(c, "J", 1e-4);
            const double bVisc = Detail::GetParam(c, "B", 1e-4);
            const double vEmf = ke * motorOmegaPrev_[i];
            const double rEq = ra + la / effectiveDt;
            iBr = (vDrop - vEmf + (la / effectiveDt) * motorIaPrev_[i]) / (std::max)(1e-6, rEq);
            motorIaPrev_[i] = iBr;
            motorOmegaPrev_[i] = (jRot * motorOmegaPrev_[i] + effectiveDt * kt * iBr) /
                                 (std::max)(1e-12, jRot + bVisc * effectiveDt);
        } else if (c.kind == ComponentKind::Relay) {
            const double vCoil = std::abs(getV(c.nodeCtrlPos) - getV(c.nodeCtrlNeg));
            const double vNom = (c.value > 0.1) ? c.value : 5.0;
            if (!relayClosed_[i] && vCoil >= 0.7 * vNom) relayClosed_[i] = true;
            else if (relayClosed_[i] && vCoil <= 0.3 * vNom) relayClosed_[i] = false;
            iBr = vDrop * (relayClosed_[i] ? 100.0 : 1e-9);
        } else if (c.kind == ComponentKind::Fuse) {
            iBr = vDrop * (fuseBlown_[i] ? 1e-9 : 20.0);
            const double iRat = (c.value > 1e-4) ? c.value : 1.0;
            fuseThermalInt_[i] += (iBr * iBr) / (iRat * iRat) * effectiveDt;
            if (fuseThermalInt_[i] > 0.05) fuseBlown_[i] = true;
        } else if (c.kind == ComponentKind::FlipFlopD) {
            const double vClk = getV(c.nodeCtrlPos);
            if (ffPrevClk_[i] < 2.5 && vClk >= 2.5) ffStateQ_[i] = (getV(c.nodePos) >= 2.5);
            ffPrevClk_[i] = vClk;
        } else if (c.kind == ComponentKind::FlipFlopJK) {
            const double vClk = getV(c.nodeCtrlPos);
            if (ffPrevClk_[i] < 2.5 && vClk >= 2.5) {
                const bool j = getV(c.nodePos) >= 2.5;
                const bool kIn = getV(c.nodeCtrlNeg) >= 2.5;
                if (j && !kIn) ffStateQ_[i] = true;
                else if (!j && kIn) ffStateQ_[i] = false;
                else if (j && kIn) ffStateQ_[i] = !ffStateQ_[i];
            }
            ffPrevClk_[i] = vClk;
        }

        branchI[c.name] = iBr;
        if (c.kind == ComponentKind::Voltmeter || c.kind == ComponentKind::VoltageProbe ||
            c.kind == ComponentKind::Oscilloscope) {
            lastVoltmeters_[c.name] = vDrop;
        } else if (c.kind == ComponentKind::Ammeter || c.kind == ComponentKind::CurrentProbe) {
            lastAmmeters_[c.name] = iBr;
        }
    }

    PushSampleToRing(simTimeSec_, nodeV, branchI);
    return true;
}

void CircuitTransientSession::PushSampleToRing(
    double t,
    const std::map<int, double>& nodeV,
    const std::map<std::string, double>& branchI)
{
    timeRing_.push_back(t);
    for (const auto& kv : nodeV) {
        channelRing_["V(" + std::to_string(kv.first) + ")"].push_back(kv.second);
    }
    for (const auto& kv : branchI) {
        channelRing_["I(" + kv.first + ")"].push_back(kv.second);
    }
    if (timeRing_.size() > ringCapacity_) {
        const std::size_t excess = timeRing_.size() - ringCapacity_;
        timeRing_.erase(timeRing_.begin(), timeRing_.begin() + static_cast<std::ptrdiff_t>(excess));
        for (auto& ch : channelRing_) {
            if (ch.second.size() > ringCapacity_) {
                const std::size_t chEx = ch.second.size() - ringCapacity_;
                ch.second.erase(ch.second.begin(), ch.second.begin() + static_cast<std::ptrdiff_t>(chEx));
            }
        }
    }
}

bool CircuitTransientSession::StepFrame(
    double frameDurationSec,
    OscilloscopeFrame& outFrame,
    std::string* outErr)
{
    if (!initialized_ || frameDurationSec <= 0.0) {
        if (outErr) *outErr = "Sesion transitoria no inicializada.";
        return false;
    }

    const int subSteps = std::clamp(static_cast<int>(std::llround(frameDurationSec / dtSec_)), 1, 512);
    const double stepDt = frameDurationSec / static_cast<double>(subSteps);
    for (int s = 0; s < subSteps; ++s) {
        const double actualDt = (simTimeSec_ == 0.0 && timeRing_.empty()) ? (stepDt * 1e-6) : stepDt;
        if (!StepOneInternal(actualDt, outErr)) return false;
        simTimeSec_ += stepDt;
    }

    outFrame.simTimeSec = simTimeSec_;
    outFrame.timeRingSec = timeRing_;
    outFrame.channelRing = channelRing_;
    outFrame.voltmeterReadingsV = lastVoltmeters_;
    outFrame.ammeterReadingsA = lastAmmeters_;
    outFrame.vMax.clear();
    outFrame.vMin.clear();
    outFrame.vRms.clear();
    outFrame.vDcAvg.clear();
    outFrame.freqEstHz.clear();

    for (const auto& ch : channelRing_) {
        const auto& vals = ch.second;
        if (vals.empty()) continue;
        double mx = vals.front(), mn = vals.front(), sum = 0.0, sumSq = 0.0;
        for (double v : vals) {
            mx = (std::max)(mx, v);
            mn = (std::min)(mn, v);
            sum += v;
            sumSq += v * v;
        }
        const double invN = 1.0 / static_cast<double>(vals.size());
        const double dc = sum * invN;
        outFrame.vMax[ch.first] = mx;
        outFrame.vMin[ch.first] = mn;
        outFrame.vDcAvg[ch.first] = dc;
        outFrame.vRms[ch.first] = std::sqrt(sumSq * invN);

        // Estimate fundamental frequency from positive-slope mean-crossings
        std::vector<double> crossTimes;
        for (std::size_t k = 1; k < vals.size() && k < timeRing_.size(); ++k) {
            if (vals[k - 1] < dc && vals[k] >= dc) {
                crossTimes.push_back(timeRing_[k]);
            }
        }
        if (crossTimes.size() >= 2) {
            const double avgPeriod = (crossTimes.back() - crossTimes.front()) / static_cast<double>(crossTimes.size() - 1);
            outFrame.freqEstHz[ch.first] = (avgPeriod > 1e-9) ? (1.0 / avgPeriod) : 0.0;
        } else {
            outFrame.freqEstHz[ch.first] = 0.0;
        }
    }
    return true;
}

// -------- Symbolic Laplace Analysis (Bug #1 Fixed + Exact Matrix-Pencil DFT H(s)) --------

LaplaceSymbolicResult AnalyzeSymbolicLaplace(
    const std::vector<CircuitComponent>& components,
    int outputNode)
{
    LaplaceSymbolicResult res{};
    if (components.empty()) {
        res.errorUtf8 = "Circuito vacio.";
        return res;
    }

    const FullSymbolicCircuitReport full = AnalyzeFullSymbolicCircuit(components, outputNode, 0.0, {}, {}, 1.0, 1e6, 15);
    if (!full.ok) {
        res.errorUtf8 = full.errorUtf8;
        return res;
    }

    res.nodalEquationsLatex = full.nodalKclLatex;
    res.transferFunctionUtf8 = full.transferFunctionUtf8;
    res.transferFunctionLatex = full.transferFunctionLatex;
    res.symbolicHOfSLatex = full.symbolicHOfSLatex;
    res.dcGain = full.dcGain;
    res.ok = true;
    return res;
}

LaplaceSymbolicResult AnalyzeSymbolicLaplaceNetlist(
    const std::string& netlistUtf8,
    int outputNode)
{
    std::vector<CircuitComponent> comps;
    std::string err;
    if (!ParseNetlistText(netlistUtf8, comps, &err)) {
        LaplaceSymbolicResult r{};
        r.errorUtf8 = err;
        return r;
    }
    return AnalyzeSymbolicLaplace(comps, outputNode);
}

} // namespace DocStudio::Circuit
