#pragma once

#include <algorithm>
#include <cctype>
#include <cmath>
#include <complex>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace DocStudio::Circuit {

inline constexpr double kPi = 3.14159265358979323846;

enum class ComponentKind : std::uint8_t {
    // --- Original 7 values (MUST preserve exact names and integer values 0..6) ---
    Resistor = 0,          // R <name> <n+> <n-> <ohms>
    Capacitor = 1,         // C <name> <n+> <n-> <farads>
    Inductor = 2,          // L <name> <n+> <n-> <henries>
    VoltageSource = 3,     // V <name> <n+> <n-> <mag> [phaseDeg]
    CurrentSource = 4,     // I <name> <n+> <n-> <mag> [phaseDeg]
    OpAmp = 5,             // OPAMP <name> <in+> <in-> <out> [model]
    Diode = 6,             // D <name> <anode+> <cathode-> [Is or model]

    // --- 46 missing tinta::TipoComponenteElectrico entries (7..52) ---
    Led = 7,                 // tinta::Led (6)
    BjtNpn = 8,              // tinta::BjtNpn (7)
    BjtPnp = 9,              // tinta::BjtPnp (8)
    MosfetN = 10,            // tinta::MosfetN (9)
    MosfetP = 11,            // tinta::MosfetP (10)
    Transformer = 12,        // tinta::Transformador (12)
    Switch = 13,             // tinta::Interruptor (13)
    Crystal = 14,            // tinta::Cristal (14)
    Relay = 15,              // tinta::Rele (15)
    Ground = 16,             // tinta::Tierra (16)
    Wire = 17,               // tinta::Cable (17)
    VoltageProbe = 18,       // tinta::ProbeVoltaje (18)
    CurrentProbe = 19,       // tinta::ProbeCorriente (19)
    GateAnd = 20,            // tinta::CompuertaAnd (20)
    GateOr = 21,             // tinta::CompuertaOr (21)
    GateNot = 22,            // tinta::CompuertaNot (22)
    GateNand = 23,           // tinta::CompuertaNand (23)
    GateNor = 24,            // tinta::CompuertaNor (24)
    GateXor = 25,            // tinta::CompuertaXor (25)
    Ic = 26,                 // tinta::Integrado (26)
    Connector = 27,          // tinta::Conector (27)
    PolarizedCapacitor = 28, // tinta::CapacitorPolarizado (28)
    ZenerDiode = 29,         // tinta::DiodoZener (29)
    Photodiode = 30,         // tinta::Fotodiodo (30)
    JfetN = 31,              // tinta::JfetN (31)
    JfetP = 32,              // tinta::JfetP (32)
    Scr = 33,                // tinta::Scr (33)
    Triac = 34,              // tinta::Triac (34)
    Comparator = 35,         // tinta::Comparador (35)
    AcSource = 36,           // tinta::FuenteAC (36)
    Vcvs = 37,               // tinta::FuenteVoltajeDependiente (37)
    Vccs = 38,               // tinta::FuenteCorrienteDependiente (38)
    PushButton = 39,         // tinta::Pulsador (39)
    Vcc = 40,                // tinta::Vcc (40)
    Battery = 41,            // tinta::Bateria (41)
    Fuse = 42,               // tinta::Fusible (42)
    Motor = 43,              // tinta::Motor (43)
    Antenna = 44,            // tinta::Antena (44)
    FlipFlopD = 45,          // tinta::FlipFlopD (45)
    FlipFlopJK = 46,         // tinta::FlipFlopJK (46)
    Multiplexer = 47,        // tinta::Multiplexor (47)
    Demultiplexer = 48,      // tinta::Demultiplexor (48)
    Potentiometer = 49,      // tinta::Potenciometro (49)
    Voltmeter = 50,          // tinta::Voltimetro (50)
    Ammeter = 51,            // tinta::Amperimetro (51)
    Oscilloscope = 52,       // tinta::Osciloscopio (52)

    // --- Extended SPICE/sensor specializations ---
    SchottkyDiode = 53,      // maps to tinta::Diodo (5)
    Ldr = 54,                // maps to tinta::Resistor (0)
    NtcThermistor = 55,      // maps to tinta::Resistor (0)
    VoltageRegulator = 56,   // maps to tinta::Integrado (26) (7805, LM317)
    PulseSource = 57,        // maps to tinta::FuenteVoltaje (3)
    Unknown = 255            // maps to tinta::Desconocido (53)
};

enum class WaveformKind : std::uint8_t {
    Dc = 0,
    Sine = 1,
    Square = 2,
    Triangle = 3,
    Pulse = 4,
    Am = 5,
    Fm = 6
};

struct CircuitComponent {
    // --- Existing 7 fields in exact original order & defaults ---
    ComponentKind kind = ComponentKind::Resistor;
    std::string name;
    int nodePos = 0;         // 0 = GND (Collector / Drain / Anode / In+ / Vin / Port1+)
    int nodeNeg = 0;         // (Emitter / Source / Cathode / In- / Gnd / Port1- / Adj)
    int nodeOut = 0;         // OpAmp/Comp/Gate/Reg Out | BJT Base | MOS/JFET/SCR Gate | Pot Wiper | Xformer S+
    double value = 1000.0;   // R(Ohm), C(F), L(H), V/I mag, Zener Vz, Lux, TempC, Ratio n, etc.
    double phaseDeg = 0.0;   // AC phase (degrees)

    // --- Extended fields with defaults (preserves 7-field positional init) ---
    int nodeCtrlPos = 0;     // VCVS/VCCS ctrl+, Xformer S-, Logic InB, Mux S0, FF Clk, Relay Coil+, MOS Bulk
    int nodeCtrlNeg = 0;     // VCVS/VCCS ctrl-, Logic/Mux extra pin, FF Rst/K, Relay Coil-
    std::vector<int> extraNodes; // Multi-pin ICs, Mux (D0..Dn, Sel), Demux, multi-input gates
    double auxValue = 0.0;   // Pot wiper alpha [0..1], Switch closed (>0.5), Xformer Lm, VCVS Av, VCCS gm
    bool active = true;
    std::string modelName;   // "2N2222", "IRF540", "LM358", "LM741", "TL072", "7805", "LM317", "1N5819", etc.
    std::map<std::string, double> params; // Is, N, Bf, Br, Vaf, Kp, Vto, Lambda, Vz, Rs, Ron, Roff, Vdd, etc.
    WaveformKind waveform = WaveformKind::Dc;
    double freqHz = 0.0;
    double dcOffset = 0.0;
    double dutyCycle = 0.5;
    double modFreqHz = 0.0;
    double modIndex = 0.0;
    double riseSec = 1e-6;
    double fallSec = 1e-6;
};

struct BranchResult {
    std::string name;
    std::complex<double> currentA{ 0.0, 0.0 };
    std::complex<double> voltageDropV{ 0.0, 0.0 };
    double activePowerW = 0.0;
};

struct CircuitSolveResult {
    bool ok = false;
    double freqHz = 0.0; // 0 = DC operating point
    std::map<int, std::complex<double>> nodeVoltages; // node > 0 -> V
    std::vector<BranchResult> branches;
    double totalPowerSumW = 0.0; // KCL/Tellegen conservation check (~0.0)
    std::vector<std::string> kclKvlEquations;
    std::string summaryUtf8;
    std::string errorUtf8;
};

struct BodePoint {
    double freqHz = 1.0;
    double magDb = 0.0;
    double phaseDeg = 0.0;
};

struct KMapSolution {
    bool ok = false;
    int numVars = 4; // 2, 3, or 4
    std::vector<std::string> varNames{ "A", "B", "C", "D" };
    std::vector<int> gridValues; // size 2^numVars ordered linearly
    std::string minimizedSopUtf8; // e.g. "F = A'B + CD"
    std::vector<std::string> primeImplicants;
    std::string errorUtf8;
};

struct TransientWaveformResult {
    bool ok = false;
    double durationSec = 0.0;
    double dtSec = 0.0;
    int probedNode = 1;
    std::vector<double> timeSec;
    std::map<int, std::vector<double>> nodeWaveforms; // node > 0 -> V(t)
    std::map<std::string, std::vector<double>> branchCurrentWaveforms; // branch name -> I(t)
    double vMax = 0.0;
    double vMin = 0.0;
    double vRms = 0.0;
    double vDcAvg = 0.0;
    std::string summaryUtf8;
    std::string errorUtf8;
};

struct LaplaceSymbolicResult {
    bool ok = false;
    std::vector<std::string> nodalEquationsLatex; // Symbolic KCL in Laplace domain s
    std::string transferFunctionUtf8;             // Rational H(s) = V_out(s) / V_in(s)
    std::string transferFunctionLatex;
    std::string symbolicHOfSLatex;                // Symbolic parameter H(s) when available
    double dcGain = 0.0;
    std::string errorUtf8;
};

// -------- DC Sweep & ~60 Hz Streaming Oscilloscope Models --------

struct DcSweepPoint {
    double sourceValue = 0.0;
    double nodeVoltageV = 0.0;
    double branchCurrentA = 0.0;
};

struct DcSweepResult {
    bool ok = false;
    std::string sourceName;
    int probedNode = 1;
    std::vector<DcSweepPoint> points;
    std::string errorUtf8;
};

struct SignalGeneratorConfig {
    std::string sourceName = "V1";
    WaveformKind waveform = WaveformKind::Sine;
    double amplitude = 5.0;
    double dcOffset = 0.0;
    double freqHz = 60.0;
    double phaseDeg = 0.0;
    double dutyCycle = 0.5;
    double riseSec = 1e-6;
    double fallSec = 1e-6;
    double modFreqHz = 5.0;
    double modIndex = 0.5;
};

struct OscilloscopeFrame {
    double simTimeSec = 0.0;
    std::vector<double> timeRingSec;
    std::map<std::string, std::vector<double>> channelRing; // "V(n)" or "I(name)"
    std::map<std::string, double> vMax;
    std::map<std::string, double> vMin;
    std::map<std::string, double> vRms;
    std::map<std::string, double> vDcAvg;
    std::map<std::string, double> freqEstHz;
    std::map<std::string, double> voltmeterReadingsV; // per Voltmeter/VoltageProbe/Oscilloscope
    std::map<std::string, double> ammeterReadingsA;   // per Ammeter/CurrentProbe
};

// -------- Bijective tinta::TipoComponenteElectrico Mapping --------

inline int ToTintaComponentCode(ComponentKind kind) noexcept {
    switch (kind) {
    case ComponentKind::Resistor: return 0;
    case ComponentKind::Capacitor: return 1;
    case ComponentKind::Inductor: return 2;
    case ComponentKind::VoltageSource: return 3;
    case ComponentKind::CurrentSource: return 4;
    case ComponentKind::Diode: return 5;  // swapped vs ComponentKind::Diode (6)
    case ComponentKind::Led: return 6;
    case ComponentKind::BjtNpn: return 7;
    case ComponentKind::BjtPnp: return 8;
    case ComponentKind::MosfetN: return 9;
    case ComponentKind::MosfetP: return 10;
    case ComponentKind::OpAmp: return 11; // swapped vs ComponentKind::OpAmp (5)
    case ComponentKind::SchottkyDiode: return 5;
    case ComponentKind::Ldr: return 0;
    case ComponentKind::NtcThermistor: return 0;
    case ComponentKind::VoltageRegulator: return 26;
    case ComponentKind::PulseSource: return 3;
    case ComponentKind::Unknown: return 53;
    default: {
        const int raw = static_cast<int>(kind);
        if (raw >= 7 && raw <= 52) return raw;
        return 53;
    }
    }
}

inline ComponentKind FromTintaComponentCode(int tintaCode) noexcept {
    if (tintaCode < 0 || tintaCode > 52) return ComponentKind::Unknown;
    switch (tintaCode) {
    case 0: return ComponentKind::Resistor;
    case 1: return ComponentKind::Capacitor;
    case 2: return ComponentKind::Inductor;
    case 3: return ComponentKind::VoltageSource;
    case 4: return ComponentKind::CurrentSource;
    case 5: return ComponentKind::Diode;
    case 6: return ComponentKind::Led;
    case 7: return ComponentKind::BjtNpn;
    case 8: return ComponentKind::BjtPnp;
    case 9: return ComponentKind::MosfetN;
    case 10: return ComponentKind::MosfetP;
    case 11: return ComponentKind::OpAmp;
    default: return static_cast<ComponentKind>(tintaCode);
    }
}

inline std::string ToUpperAscii(std::string s) {
    for (char& ch : s) {
        ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
    }
    return s;
}

inline std::string ComponentKindLabel(ComponentKind kind, bool spanish = false) {
    switch (kind) {
    case ComponentKind::Resistor: return spanish ? "Resistor" : "Resistor";
    case ComponentKind::Capacitor: return "Capacitor";
    case ComponentKind::Inductor: return "Inductor";
    case ComponentKind::VoltageSource: return spanish ? "Fuente de Voltaje" : "Voltage Source";
    case ComponentKind::CurrentSource: return spanish ? "Fuente de Corriente" : "Current Source";
    case ComponentKind::OpAmp: return spanish ? "Amplificador Operacional" : "Operational Amplifier";
    case ComponentKind::Diode: return spanish ? "Diodo" : "Diode";
    case ComponentKind::Led: return "LED";
    case ComponentKind::BjtNpn: return "BJT NPN";
    case ComponentKind::BjtPnp: return "BJT PNP";
    case ComponentKind::MosfetN: return "MOSFET N";
    case ComponentKind::MosfetP: return "MOSFET P";
    case ComponentKind::Transformer: return spanish ? "Transformador" : "Transformer";
    case ComponentKind::Switch: return spanish ? "Interruptor" : "Switch";
    case ComponentKind::Crystal: return spanish ? "Cristal" : "Crystal";
    case ComponentKind::Relay: return spanish ? "Rele" : "Relay";
    case ComponentKind::Ground: return spanish ? "Tierra" : "Ground";
    case ComponentKind::Wire: return spanish ? "Cable" : "Wire";
    case ComponentKind::VoltageProbe: return spanish ? "Sonda de Voltaje" : "Voltage Probe";
    case ComponentKind::CurrentProbe: return spanish ? "Sonda de Corriente" : "Current Probe";
    case ComponentKind::GateAnd: return spanish ? "Compuerta AND" : "AND Gate";
    case ComponentKind::GateOr: return spanish ? "Compuerta OR" : "OR Gate";
    case ComponentKind::GateNot: return spanish ? "Compuerta NOT" : "NOT Gate";
    case ComponentKind::GateNand: return spanish ? "Compuerta NAND" : "NAND Gate";
    case ComponentKind::GateNor: return spanish ? "Compuerta NOR" : "NOR Gate";
    case ComponentKind::GateXor: return spanish ? "Compuerta XOR" : "XOR Gate";
    case ComponentKind::Ic: return spanish ? "Circuito Integrado" : "Integrated Circuit";
    case ComponentKind::Connector: return spanish ? "Conector" : "Connector";
    case ComponentKind::PolarizedCapacitor: return spanish ? "Capacitor Polarizado" : "Polarized Capacitor";
    case ComponentKind::ZenerDiode: return spanish ? "Diodo Zener" : "Zener Diode";
    case ComponentKind::Photodiode: return spanish ? "Fotodiodo" : "Photodiode";
    case ComponentKind::JfetN: return "JFET N";
    case ComponentKind::JfetP: return "JFET P";
    case ComponentKind::Scr: return "SCR";
    case ComponentKind::Triac: return "TRIAC";
    case ComponentKind::Comparator: return spanish ? "Comparador" : "Comparator";
    case ComponentKind::AcSource: return spanish ? "Fuente AC" : "AC Source";
    case ComponentKind::Vcvs: return spanish ? "Fuente Voltaje Dependiente" : "VCVS";
    case ComponentKind::Vccs: return spanish ? "Fuente Corriente Dependiente" : "VCCS";
    case ComponentKind::PushButton: return spanish ? "Pulsador" : "Push Button";
    case ComponentKind::Vcc: return "VCC";
    case ComponentKind::Battery: return spanish ? "Bateria" : "Battery";
    case ComponentKind::Fuse: return spanish ? "Fusible" : "Fuse";
    case ComponentKind::Motor: return "Motor";
    case ComponentKind::Antenna: return spanish ? "Antena" : "Antenna";
    case ComponentKind::FlipFlopD: return "Flip-Flop D";
    case ComponentKind::FlipFlopJK: return "Flip-Flop JK";
    case ComponentKind::Multiplexer: return spanish ? "Multiplexor" : "Multiplexer";
    case ComponentKind::Demultiplexer: return spanish ? "Demultiplexor" : "Demultiplexer";
    case ComponentKind::Potentiometer: return spanish ? "Potenciometro" : "Potentiometer";
    case ComponentKind::Voltmeter: return spanish ? "Voltimetro" : "Voltmeter";
    case ComponentKind::Ammeter: return spanish ? "Amperimetro" : "Ammeter";
    case ComponentKind::Oscilloscope: return spanish ? "Osciloscopio" : "Oscilloscope";
    case ComponentKind::SchottkyDiode: return spanish ? "Diodo Schottky" : "Schottky Diode";
    case ComponentKind::Ldr: return spanish ? "Fotorresistencia LDR" : "Photoresistor LDR";
    case ComponentKind::NtcThermistor: return spanish ? "Termistor NTC" : "NTC Thermistor";
    case ComponentKind::VoltageRegulator: return spanish ? "Regulador de Voltaje" : "Voltage Regulator";
    case ComponentKind::PulseSource: return spanish ? "Generador de Pulsos" : "Pulse Source";
    case ComponentKind::Unknown: return spanish ? "Desconocido" : "Unknown";
    }
    return spanish ? "Desconocido" : "Unknown";
}

// -------- SPICE Nonlinear Helpers & Waveform Evaluation --------

namespace Detail {

inline double GetParam(const CircuitComponent& c, const char* key, double fallback) {
    auto it = c.params.find(key);
    if (it != c.params.end() && std::isfinite(it->second)) return it->second;
    const std::string uk = ToUpperAscii(key);
    for (const auto& kv : c.params) {
        if (ToUpperAscii(kv.first) == uk && std::isfinite(kv.second)) return kv.second;
    }
    return fallback;
}

inline void GetDiodeSpec(
    const CircuitComponent& c,
    double& isSat,
    double& nFactor,
    double& vz,
    double& iPhoto)
{
    const std::string m = ToUpperAscii(c.modelName);
    isSat = 1e-12;
    nFactor = 1.35;
    vz = 0.0;
    iPhoto = 0.0;
    if (c.kind == ComponentKind::Led) {
        isSat = 1e-20;
        nFactor = (m == "BLUE" || m == "WHITE") ? 2.6 : (m == "GREEN" ? 2.0 : 1.75);
    } else if (c.kind == ComponentKind::SchottkyDiode ||
               (c.kind == ComponentKind::Diode &&
                (m == "1N5819" || m == "D1N5819" || m == "DS_DEFAULT" || m == "BAT54"))) {
        isSat = (m == "1N5819" || m == "D1N5819") ? 2.0e-5 : 1.0e-6;
        nFactor = 1.08;
    } else if (c.kind == ComponentKind::Diode) {
        if (m == "1N4148" || m == "D1N4148") { isSat = 2.52e-9; nFactor = 1.752; }
        else if (m == "1N4007" || m == "D1N4007") { isSat = 1.4e-8; nFactor = 1.98; }
        else if (m != "D_DEFAULT" && c.value > 0.0 && c.value < 1.0) {
            isSat = (std::max)(1e-16, c.value);
        }
    } else if (c.kind == ComponentKind::ZenerDiode) {
        isSat = 1e-11;
        nFactor = 1.5;
        vz = GetParam(c, "Vz", (c.value > 0.5 && c.value < 200.0) ? c.value : 5.1);
    } else if (c.kind == ComponentKind::Photodiode) {
        isSat = 1e-11;
        nFactor = 1.3;
        iPhoto = (std::max)(0.0, c.value) * 1e-6; // 1000 lux -> 1 mA
    }
    isSat = GetParam(c, "Is", isSat);
    nFactor = GetParam(c, "N", nFactor);
}

inline double PnjLim(double vNew, double vOld, double vt, double vCrit) noexcept {
    if (vNew > vCrit && std::abs(vNew - vOld) > 2.0 * vt) {
        if (vOld > 0.0) {
            const double arg = 1.0 + (vNew - vOld) / vt;
            return (arg > 0.0) ? (vOld + vt * std::log(arg)) : vCrit;
        }
        return vt * std::log((std::max)(1e-12, vNew / vt));
    }
    return vNew;
}

inline double PnjLimZener(double vNew, double vOld, double vt, double vCrit, double vz) noexcept {
    double v = PnjLim(vNew, vOld, vt, vCrit);
    if (vz > 0.5) {
        const double vRevNew = -(v + vz);
        const double vRevOld = -(vOld + vz);
        if (vRevNew > vCrit && std::abs(vRevNew - vRevOld) > 2.0 * vt) {
            const double limitedRev = PnjLim(vRevNew, vRevOld, vt, vCrit);
            v = -limitedRev - vz;
        }
    }
    return v;
}

inline double FetLim(double vNew, double vOld, double vTo) noexcept {
    const double vtstHi = std::abs(vOld - vTo) + 2.0;
    const double vtstLo = vtstHi * 0.5 + 2.0;
    const double vtox = vTo + 3.5;
    double dv = vNew - vOld;
    if (vOld >= vTo) {
        if (vOld >= vtox) {
            if (dv <= 0.0) {
                if (vNew < vtox) {
                    return (std::max)(vTo, vOld - vtstLo);
                }
                return (std::max)(vtox, vNew);
            }
            return (std::min)(vOld + vtstHi, vNew);
        }
        if (dv <= 0.0) return (std::max)(vTo - 0.5, vNew);
        return (std::min)(vOld + vtstLo, vNew);
    }
    if (dv >= 0.0) return (std::min)(vTo + 0.5, vNew);
    return (std::max)(vOld - vtstHi, vNew);
}

inline double EvalWaveform(
    WaveformKind wf,
    double amp,
    double dcOffset,
    double freqHz,
    double phaseDeg,
    double dutyCycle,
    double riseSec,
    double fallSec,
    double modFreqHz,
    double modIndex,
    double t) noexcept
{
    const double phi = phaseDeg * (kPi / 180.0);
    if (wf == WaveformKind::Dc && freqHz <= 0.0) {
        return dcOffset + amp;
    }
    const double f = (std::max)(0.0, freqHz);
    const WaveformKind effectiveWf = (wf == WaveformKind::Dc && f > 0.0) ? WaveformKind::Sine : wf;
    switch (effectiveWf) {
    case WaveformKind::Dc:
        return dcOffset + amp;
    case WaveformKind::Sine:
        return dcOffset + amp * std::sin(2.0 * kPi * f * t + phi);
    case WaveformKind::Square: {
        const double duty = std::clamp(dutyCycle, 0.01, 0.99);
        const double cyc = f * t + phi / (2.0 * kPi);
        const double frac = cyc - std::floor(cyc);
        return dcOffset + amp * (frac < duty ? 1.0 : -1.0);
    }
    case WaveformKind::Triangle: {
        const double cyc = f * t + phi / (2.0 * kPi) + 0.25;
        const double frac = cyc - std::floor(cyc);
        const double tri = 1.0 - 4.0 * std::abs(frac - 0.5);
        return dcOffset + amp * tri;
    }
    case WaveformKind::Pulse: {
        if (f <= 0.0) return dcOffset + amp;
        const double period = 1.0 / f;
        const double tr = std::clamp(riseSec, 1e-9, 0.4 * period);
        const double tf = std::clamp(fallSec, 1e-9, 0.4 * period);
        const double pw = std::clamp(dutyCycle, 0.01, 0.99) * period;
        double tau = std::fmod(t + (phi / (2.0 * kPi)) * period, period);
        if (tau < 0.0) tau += period;
        double level = 0.0;
        if (tau < tr) level = tau / tr;
        else if (tau < tr + pw) level = 1.0;
        else if (tau < tr + pw + tf) level = 1.0 - (tau - tr - pw) / tf;
        else level = 0.0;
        return dcOffset + amp * level;
    }
    case WaveformKind::Am: {
        const double env = 1.0 + modIndex * std::sin(2.0 * kPi * modFreqHz * t);
        return dcOffset + amp * env * std::sin(2.0 * kPi * f * t + phi);
    }
    case WaveformKind::Fm: {
        const double mod = modIndex * std::sin(2.0 * kPi * modFreqHz * t);
        return dcOffset + amp * std::sin(2.0 * kPi * f * t + mod + phi);
    }
    }
    return dcOffset + amp;
}

} // namespace Detail

// -------- Core MNA / SPICE Free Functions & Streaming Session --------

bool ParseNetlistText(
    const std::string& netlistUtf8,
    std::vector<CircuitComponent>& outComponents,
    std::string* outErr = nullptr);

CircuitSolveResult SolveMnaCircuit(
    const std::vector<CircuitComponent>& components,
    double freqHz = 0.0);

CircuitSolveResult SolveMnaNetlist(
    const std::string& netlistUtf8,
    double freqHz = 0.0);

std::vector<BodePoint> ComputeBodeSweep(
    const std::vector<CircuitComponent>& components,
    int outputNode,
    double fStartHz,
    double fEndHz,
    std::size_t numPoints = 41);

DcSweepResult SimulateDcSweepMna(
    const std::vector<CircuitComponent>& components,
    const std::string& sourceNameOrIndex,
    double startVal,
    double endVal,
    std::size_t numPoints,
    int probedNode = 1);

TransientWaveformResult SimulateTransientMna(
    const std::vector<CircuitComponent>& components,
    double durationSec,
    std::size_t numSteps = 200,
    double sourceFreqHz = 0.0,
    int probedNode = 1);

TransientWaveformResult SimulateTransientNetlist(
    const std::string& netlistUtf8,
    double durationSec,
    std::size_t numSteps = 200,
    double sourceFreqHz = 0.0,
    int probedNode = 1);

class CircuitTransientSession {
public:
    bool Initialize(
        const std::vector<CircuitComponent>& comps,
        double dtSec = 1.0 / 2400.0,
        std::size_t ringCapacity = 2048,
        bool useTrapezoidal = true,
        std::string* outErr = nullptr);

    void SetSignalGenerator(const SignalGeneratorConfig& cfg);
    bool SetComponentValue(const std::string& name, double newValue, double newAux = 0.0);
    bool ToggleSwitch(const std::string& name, bool closed);
    void Reset();

    bool StepFrame(
        double frameDurationSec,
        OscilloscopeFrame& outFrame,
        std::string* outErr = nullptr);

    bool IsInitialized() const noexcept { return initialized_; }
    double SimTimeSec() const noexcept { return simTimeSec_; }
    const std::vector<CircuitComponent>& Components() const noexcept { return components_; }

private:
    bool StepOneInternal(double stepDt, std::string* outErr);
    void PushSampleToRing(
        double t,
        const std::map<int, double>& nodeV,
        const std::map<std::string, double>& branchI);

    bool initialized_ = false;
    bool useTrapezoidal_ = true;
    bool forceBackwardEulerNext_ = true;
    double dtSec_ = 1.0 / 2400.0;
    double simTimeSec_ = 0.0;
    std::size_t ringCapacity_ = 2048;
    int maxNode_ = 0;

    std::vector<CircuitComponent> initialComponents_;
    std::vector<CircuitComponent> components_;
    std::map<std::string, SignalGeneratorConfig> generators_;

    std::vector<double> capVoltPrev_;
    std::vector<double> capCurrPrev_;
    std::vector<double> indCurrPrev_;
    std::vector<double> indVoltPrev_;
    std::vector<double> diodeVdPrev_;
    std::vector<double> bjtVbePrev_;
    std::vector<double> bjtVbcPrev_;
    std::vector<double> mosVgsPrev_;
    std::vector<double> mosVdsPrev_;
    std::vector<double> motorOmegaPrev_;
    std::vector<double> motorIaPrev_;
    std::vector<double> fuseThermalInt_;
    std::vector<bool> fuseBlown_;
    std::vector<bool> relayClosed_;
    std::vector<bool> scrLatched_;
    std::vector<bool> ffStateQ_;
    std::vector<double> ffPrevClk_;
    std::vector<double> lastNodeVoltages_;

    std::vector<double> timeRing_;
    std::map<std::string, std::vector<double>> channelRing_;
    std::map<std::string, double> lastVoltmeters_;
    std::map<std::string, double> lastAmmeters_;
};

} // namespace DocStudio::Circuit
