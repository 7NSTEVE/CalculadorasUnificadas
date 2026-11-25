#include "mna_solver.hpp"

#include <cassert>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <iostream>
#include <string>
#include <vector>

namespace {

int gPassed = 0;
int gFailed = 0;

void CheckImpl(bool cond, const char* expr, int line) {
    if (cond) {
        ++gPassed;
    } else {
        ++gFailed;
        std::cerr << "[FAIL] Line " << line << ": " << expr << "\n";
    }
}

#define CHECK(expr) CheckImpl(static_cast<bool>(expr), #expr, __LINE__)

using namespace DocStudio::Circuit;

CircuitComponent MakeComp(
    ComponentKind kind,
    std::string name,
    int nPos,
    int nNeg,
    int nOut,
    double val,
    double phase = 0.0,
    std::string model = "")
{
    CircuitComponent c;
    c.kind = kind;
    c.name = std::move(name);
    c.nodePos = nPos;
    c.nodeNeg = nNeg;
    c.nodeOut = nOut;
    c.value = val;
    c.phaseDeg = phase;
    c.modelName = std::move(model);
    return c;
}

void TestBackwardsCompatibilityAndF10F11() {
    // Enum integer preservation (0..6)
    CHECK(static_cast<int>(ComponentKind::Resistor) == 0);
    CHECK(static_cast<int>(ComponentKind::Capacitor) == 1);
    CHECK(static_cast<int>(ComponentKind::Inductor) == 2);
    CHECK(static_cast<int>(ComponentKind::VoltageSource) == 3);
    CHECK(static_cast<int>(ComponentKind::CurrentSource) == 4);
    CHECK(static_cast<int>(ComponentKind::OpAmp) == 5);
    CHECK(static_cast<int>(ComponentKind::Diode) == 6);

    // 7-field positional compatibility & 3-field aggregate init of BodePoint
    CircuitComponent c7 = MakeComp(ComponentKind::Resistor, "R1", 1, 0, 0, 1000.0, 0.0);
    CHECK(c7.name == "R1" && c7.value == 1000.0 && c7.active);
    BodePoint bp{ 100.0, -3.0, -45.0 };
    CHECK(bp.freqHz == 100.0 && bp.magDb == -3.0 && bp.phaseDeg == -45.0);

    // F10 assertions (document_core_test.cpp:1180-1203)
    const auto dcDiv = SolveMnaNetlist("V1 1 0 12; R1 1 2 2k; R2 2 0 4k", 0.0);
    CHECK(dcDiv.ok && std::abs(dcDiv.nodeVoltages.at(2).real() - 8.0) < 1e-5);
    CHECK(std::abs(dcDiv.totalPowerSumW) < 1e-6);

    std::vector<CircuitComponent> rcComps;
    std::string nErr;
    CHECK(ParseNetlistText("V1 1 0 1; R1 1 2 1k; C1 2 0 1u", rcComps, &nErr));
    const double fc = 1.0 / (2.0 * kPi * 1000.0 * 1e-6);
    const auto rcAc = SolveMnaCircuit(rcComps, fc);
    CHECK(rcAc.ok && std::abs(std::abs(rcAc.nodeVoltages.at(2)) - 0.7071067) < 1e-3);

    const auto bode = ComputeBodeSweep(rcComps, 2, 10.0, 10000.0, 15);
    CHECK(bode.size() == 15 && bode.front().magDb > -0.5 && bode.back().magDb < -30.0);

    const auto opamp = SolveMnaNetlist("V1 1 0 1; R1 1 2 1k; R2 2 3 10k; OPAMP U1 0 2 3", 0.0);
    CHECK(opamp.ok && std::abs(opamp.nodeVoltages.at(3).real() + 10.0) < 1e-4);
    // Ideal OpAmp KCL summary must NOT emit I(U1) at node 2 (in-) and MUST emit I_U1_out at node 3
    CHECK(opamp.kclKvlEquations.size() >= 3);
    CHECK(opamp.kclKvlEquations[1].find("I(U1)") == std::string::npos);
    CHECK(opamp.kclKvlEquations[2].find("I_U1_out") != std::string::npos);

    const auto kmap = SimplifyKarnaughMap(4, { 0, 1, 2, 3, 8, 9, 10, 11 });
    CHECK(kmap.ok && kmap.minimizedSopUtf8 == "F = B'");

    CHECK(ExportCircuitToSvg(rcComps, &rcAc).find("<svg") != std::string::npos);
    CHECK(ExportCircuitToDxf(rcComps, &rcAc).find("ENTITIES") != std::string::npos);
    CHECK(ExportCircuitToKiCadNetlist(rcComps).find(".end") != std::string::npos);

    // F11 assertions (document_core_test.cpp:1358-1373)
    const auto fwd = SolveMnaNetlist("V1 1 0 5\nD1 1 2\nR1 2 0 1k", 0.0);
    CHECK(fwd.ok);
    const double vOutFwd = fwd.nodeVoltages.at(2).real();
    CHECK(vOutFwd > 4.1 && vOutFwd < 4.5);

    const auto rev = SolveMnaNetlist("V1 1 0 -5\nD1 1 2\nR1 2 0 1k", 0.0);
    CHECK(rev.ok && std::abs(rev.nodeVoltages.at(2).real()) < 1e-4);

    const auto tr = SimulateTransientNetlist("V1 1 0 10\nD1 1 2\nR1 2 0 1k", 0.035, 140, 60.0, 2);
    CHECK(tr.ok && tr.vMax > 8.8 && tr.vMin > -0.05 && tr.vRms > 4.0);

    const auto lap = AnalyzeSymbolicLaplaceNetlist("V1 1 0 1\nR1 1 2 1k\nC1 2 0 1u", 2);
    CHECK(lap.ok && !lap.nodalEquationsLatex.empty());
    CHECK(std::abs(lap.dcGain - 1.0) < 1e-4);
    CHECK(lap.transferFunctionUtf8.find("0.001*s") != std::string::npos);
}

void Test53ComponentsAndTintaBijection() {
    for (int t = 0; t <= 52; ++t) {
        const ComponentKind k = FromTintaComponentCode(t);
        CHECK(k != ComponentKind::Unknown);
        CHECK(ToTintaComponentCode(k) == t);
        CHECK(!ComponentKindLabel(k, false).empty());
        CHECK(!ComponentKindLabel(k, true).empty());
    }
    CHECK(FromTintaComponentCode(5) == ComponentKind::Diode);
    CHECK(FromTintaComponentCode(11) == ComponentKind::OpAmp);
    CHECK(ToTintaComponentCode(ComponentKind::OpAmp) == 11);
    CHECK(ToTintaComponentCode(ComponentKind::Diode) == 5);
    CHECK(FromTintaComponentCode(53) == ComponentKind::Unknown);
    CHECK(FromTintaComponentCode(-1) == ComponentKind::Unknown);

    // Extended netlist parser coverage
    std::vector<CircuitComponent> parsed;
    std::string err;
    const std::string megaNetlist =
        "VCC Vcc 1 5\n"
        "POT P1 1 0 2 10k 0.25\n"
        "LDR LD1 1 3 100\n"
        "NTC TH1 3 0 25\n"
        "LED L1 1 4\n"
        "DZ Z1 0 5 5.1\n"
        "DS S1 1 6\n"
        "Q1 7 8 0 2N2222\n"
        "M1 9 10 0 0 IRF540\n"
        "SW SW1 1 11 1\n"
        "VM Vm1 2 0\n"
        "AM Am1 11 12\n"
        "Rload 12 0 100\n";
    CHECK(ParseNetlistText(megaNetlist, parsed, &err));
    CHECK(parsed.size() == 13);
    if (parsed.size() >= 13) {
        CHECK(parsed[0].kind == ComponentKind::Vcc);
        CHECK(parsed[1].kind == ComponentKind::Potentiometer && std::abs(parsed[1].auxValue - 0.25) < 1e-6);
        CHECK(parsed[7].kind == ComponentKind::BjtNpn);
        CHECK(parsed[8].kind == ComponentKind::MosfetN);
    }

    // Negative parser tests
    std::vector<CircuitComponent> bad;
    CHECK(!ParseNetlistText("", bad, &err));
    CHECK(!ParseNetlistText("R1 1", bad, &err));
    CHECK(!ParseNetlistText("XYZ 1 2 100", bad, &err));
}

void TestNonlinearSemiconductorsActiveIcsAndAc() {
    // 1. Zener reverse clamping at ~ -5.1 V (anode at GND=0, cathode at node 2 pulled to +12V via R1)
    {
        const auto z = SolveMnaNetlist("V1 1 0 12; R1 1 2 1k; DZ Z1 0 2 5.1", 0.0);
        CHECK(z.ok);
        const double v2 = z.nodeVoltages.at(2).real();
        CHECK(v2 > 4.95 && v2 < 5.25);
    }

    // 2. Schottky vs LED forward voltage ordering
    {
        const auto sch = SolveMnaNetlist("V1 1 0 5; DS D1 1 2; R1 2 0 1k", 0.0);
        const auto led = SolveMnaNetlist("V1 1 0 5; LED L1 1 2; R1 2 0 1k", 0.0);
        CHECK(sch.ok && led.ok);
        const double vdSch = 5.0 - sch.nodeVoltages.at(2).real();
        const double vdLed = 5.0 - led.nodeVoltages.at(2).real();
        CHECK(vdSch > 0.2 && vdSch < 0.45);
        CHECK(vdLed > 1.4 && vdLed < 2.4);
        CHECK(vdSch < vdLed);
    }

    // 3. BJT NPN (2N2222) common-emitter bias + AC small-signal gain around DC bias
    {
        // Vcc=12V, Rc=1k (node 1->2), R1=100k (1->3), R2=22k (3->0), Re=470 (4->0), Q1 C=2 B=3 E=4
        // Plus AC coupling input at base via large C_in from Vin (node 5) -> node 3
        const std::string bjtNet =
            "VCC Vcc 1 12\n"
            "R1 1 3 100k\n"
            "R2 3 0 22k\n"
            "RC 1 2 1k\n"
            "RE 4 0 470\n"
            "Q1 2 3 4 2N2222\n"
            "V_in 5 0 0\n"
            "C_in 5 3 100u\n";
        const auto dcBjt = SolveMnaNetlist(bjtNet, 0.0);
        CHECK(dcBjt.ok);
        const double vB = dcBjt.nodeVoltages.at(3).real();
        const double vE = dcBjt.nodeVoltages.at(4).real();
        const double vC = dcBjt.nodeVoltages.at(2).real();
        CHECK(vB > 1.8 && vB < 2.5);
        CHECK((vB - vE) > 0.55 && (vB - vE) < 0.85);
        CHECK(vC > vE + 0.5 && vC < 11.5); // Active region!

        std::vector<CircuitComponent> acComps;
        CHECK(ParseNetlistText(bjtNet, acComps));
        for (auto& c : acComps) {
            if (c.name == "V_in") c.value = 1.0; // 1V AC test signal
        }
        const auto acBjt = SolveMnaCircuit(acComps, 10000.0);
        CHECK(acBjt.ok);
        const double gainMag = std::abs(acBjt.nodeVoltages.at(2));
        // Unbypassed emitter -> Av ~ -Rc / Re ~ -2.1
        CHECK(gainMag > 1.5 && gainMag < 2.8);
    }

    // 4. MOSFET N (IRF540) cutoff vs saturation & DC sweep
    {
        CircuitComponent vdd = MakeComp(ComponentKind::VoltageSource, "Vdd", 1, 0, 0, 12.0);
        CircuitComponent rd = MakeComp(ComponentKind::Resistor, "Rd", 1, 2, 0, 10.0);
        CircuitComponent vgs = MakeComp(ComponentKind::VoltageSource, "Vgs", 3, 0, 0, 1.0);
        CircuitComponent m1 = MakeComp(ComponentKind::MosfetN, "M1", 2, 0, 3, 0.0, 0.0, "IRF540");
        const std::vector<CircuitComponent> mosCkt{ vdd, rd, vgs, m1 };

        const auto cut = SolveMnaCircuit(mosCkt, 0.0);
        CHECK(cut.ok && cut.nodeVoltages.at(2).real() > 11.9); // Cutoff: Vd ~ 12V

        const auto swp = SimulateDcSweepMna(mosCkt, "Vgs", 0.0, 6.0, 13, 2);
        CHECK(swp.ok && swp.points.size() == 13);
        CHECK(swp.points.front().nodeVoltageV > 11.9);
        CHECK(swp.points.back().nodeVoltageV < 3.0); // Strong conduction at Vgs=6V
    }

    // 5. Real OpAmp (LM358 / LM741) rail saturation clamping
    {
        // Inverting -10x with 5V input on LM741 (rails +/-15V -> output clamps below +15/-15, not -50V)
        const auto sat = SolveMnaNetlist("V1 1 0 5; R1 1 2 1k; R2 2 3 10k; OPAMP U1 0 2 3 LM741", 0.0);
        CHECK(sat.ok);
        const double vo = sat.nodeVoltages.at(3).real();
        CHECK(vo < -12.0 && vo > -15.0);
    }

    // 6. Voltage Regulator 7805 (regulated vs dropout) & LM317
    {
        const auto regOk = SolveMnaNetlist("V1 1 0 12; REG U1 1 0 2 7805; R1 2 0 100", 0.0);
        CHECK(regOk.ok && std::abs(regOk.nodeVoltages.at(2).real() - 5.0) < 0.05);

        const auto regDrop = SolveMnaNetlist("V1 1 0 5.5; REG U1 1 0 2 7805; R1 2 0 100", 0.0);
        CHECK(regDrop.ok && regDrop.nodeVoltages.at(2).real() < 4.0);

        // LM317: Vout = 1.25 * (1 + R2/R1) = 1.25 * (1 + 240/240) = 2.5V (approx with I_adj)
        const auto lm317 = SolveMnaNetlist("V1 1 0 10; REG U1 1 3 2 LM317; R1 2 3 240; R2 3 0 240; RL 2 0 100", 0.0);
        CHECK(lm317.ok && std::abs(lm317.nodeVoltages.at(2).real() - 2.5) < 0.1);
    }

    // 7. 74HC Logic Gates truth tables (AND, OR, NOT, NAND, NOR, XOR)
    {
        const auto and11 = SolveMnaNetlist("VA 1 0 5; VB 2 0 5; AND U1 1 2 3", 0.0);
        const auto and10 = SolveMnaNetlist("VA 1 0 5; VB 2 0 0; AND U1 1 2 3", 0.0);
        const auto xor11 = SolveMnaNetlist("VA 1 0 5; VB 2 0 5; XOR U1 1 2 3", 0.0);
        const auto xor10 = SolveMnaNetlist("VA 1 0 5; VB 2 0 0; XOR U1 1 2 3", 0.0);
        const auto not1 = SolveMnaNetlist("VA 1 0 5; NOT U1 1 2", 0.0);
        CHECK(and11.ok && and11.nodeVoltages.at(3).real() > 4.8);
        CHECK(and10.ok && and10.nodeVoltages.at(3).real() < 0.2);
        CHECK(xor11.ok && xor11.nodeVoltages.at(3).real() < 0.2);
        CHECK(xor10.ok && xor10.nodeVoltages.at(3).real() > 4.8);
        CHECK(not1.ok && not1.nodeVoltages.at(2).real() < 0.2);
    }

    // 8. Transformer, VCVS, VCCS, Potentiometer, Crystal resonance
    {
        const auto xfmr = SolveMnaNetlist("V1 1 0 10; XFMR T1 1 0 2 0 2.5; R1 2 0 1k", 60.0);
        CHECK(xfmr.ok && std::abs(std::abs(xfmr.nodeVoltages.at(2)) - 25.0) < 0.1);

        const auto vcvs = SolveMnaNetlist("V1 1 0 2; R1 1 0 1k; E1 2 0 1 0 3.5; R2 2 0 1k", 0.0);
        CHECK(vcvs.ok && std::abs(vcvs.nodeVoltages.at(2).real() - 7.0) < 1e-4);

        const auto pot = SolveMnaNetlist("V1 1 0 10; POT P1 1 0 2 10k 0.3", 0.0);
        CHECK(pot.ok && std::abs(pot.nodeVoltages.at(2).real() - 3.0) < 1e-3);

        // Crystal series resonance peak at fs = 1 MHz
        const auto xtalOff = SolveMnaNetlist("V1 1 0 1; R1 1 2 50; XTAL X1 2 0 1e6", 500e3);
        const auto xtalRes = SolveMnaNetlist("V1 1 0 1; R1 1 2 50; XTAL X1 2 0 1e6", 1e6);
        CHECK(xtalOff.ok && xtalRes.ok);
        CHECK(std::abs(xtalRes.nodeVoltages.at(2)) < std::abs(xtalOff.nodeVoltages.at(2)));
    }
}

void TestStreamingTransient60HzAndSignalGenerator() {
    std::vector<CircuitComponent> comps;
    CHECK(ParseNetlistText("V1 1 0 5\nSW SW1 1 2 1\nR1 2 3 1k\nC1 3 0 1u\nRbleed 3 0 10k\nVM Vm1 3 0\nAM Am1 2 3", comps));

    CircuitTransientSession sess;
    std::string err;
    CHECK(sess.Initialize(comps, 1.0 / 2400.0, 2048, true, &err));

    SignalGeneratorConfig sg;
    sg.sourceName = "V1";
    sg.waveform = WaveformKind::Sine;
    sg.amplitude = 5.0;
    sg.dcOffset = 0.0;
    sg.freqHz = 60.0;
    sess.SetSignalGenerator(sg);

    OscilloscopeFrame frame{};
    for (int f = 0; f < 60; ++f) {
        CHECK(sess.StepFrame(1.0 / 60.0, frame, &err));
    }
    CHECK(std::abs(frame.simTimeSec - 1.0) < 1e-3);
    CHECK(frame.channelRing.count("V(3)") && !frame.channelRing["V(3)"].empty());
    CHECK(frame.vMax["V(3)"] > 3.5 && frame.vMin["V(3)"] < -3.5);
    CHECK(frame.freqEstHz["V(3)"] > 50.0 && frame.freqEstHz["V(3)"] < 70.0);
    CHECK(frame.voltmeterReadingsV.count("Vm1") == 1);

    // Live switch toggle opens circuit -> RC decays toward 0
    CHECK(sess.ToggleSwitch("SW1", false));
    for (int f = 0; f < 5; ++f) CHECK(sess.StepFrame(1.0 / 60.0, frame, &err));
    CHECK(std::abs(frame.channelRing["V(3)"].back()) < 0.5);

    // Signal generator waveform types check
    for (WaveformKind wf : { WaveformKind::Square, WaveformKind::Triangle, WaveformKind::Pulse, WaveformKind::Am, WaveformKind::Fm }) {
        sg.waveform = wf;
        sg.modFreqHz = 10.0;
        sg.modIndex = 0.5;
        sess.Reset();
        sess.SetSignalGenerator(sg);
        CHECK(sess.StepFrame(0.05, frame, &err));
        CHECK(std::isfinite(frame.vRms["V(1)"]));
    }
}

void TestSymbolicAnalysisAndBug1Fix() {
    // Bug #1 verification: OpAmp nodeOut & virtual short
    const auto lapOp = AnalyzeSymbolicLaplaceNetlist("V1 1 0 1; R1 1 2 1k; R2 2 3 10k; OPAMP U1 0 2 3", 3);
    CHECK(lapOp.ok);
    CHECK(std::abs(lapOp.dcGain + 10.0) < 1e-4);
    CHECK(lapOp.nodalEquationsLatex.size() >= 4);
    CHECK(lapOp.nodalEquationsLatex[2].find("I_{U1,\\text{out}}(s)") != std::string::npos);
    CHECK(lapOp.nodalEquationsLatex.back().find("= 0") != std::string::npos);

    // Full symbolic report: RC lowpass + Switch t=0 IC + 2-source DC superposition
    std::vector<CircuitComponent> rcComps;
    CHECK(ParseNetlistText("V1 1 0 1; R1 1 2 1k; C1 2 0 1u", rcComps));
    const auto rep = AnalyzeFullSymbolicCircuit(rcComps, 2, 159.155, { { "C1", 5.0 } }, {}, 1.0, 1e6, 25);
    CHECK(rep.ok);
    CHECK(!rep.mnaMatrixLatex.empty());
    CHECK(!rep.meshKvlEquationsLatex.empty());
    CHECK(rep.dcSuperposition.size() == 1);
    CHECK(rep.acNodePhasors.count(2) && std::abs(std::abs(rep.acNodePhasors.at(2)) - 0.7071) < 5e-3);
    CHECK(rep.transferFunctionUtf8.find("0.001*s") != std::string::npos);
    CHECK(!rep.symbolicHOfSLatex.empty() && rep.symbolicHOfSLatex.find("R1") != std::string::npos);
    CHECK(rep.stateSpace.ok && rep.stateSpace.A.size() == 1 && std::abs(rep.stateSpace.A[0][0] + 1000.0) < 1e-3);
    CHECK(rep.poles.size() == 1 && std::abs(rep.poles[0].real() + 1000.0) < 1e-2);
    CHECK(rep.isBibostable);
    CHECK(!rep.nyquistPoints.empty());
    CHECK(rep.rootLocusBranches.size() >= 5 && rep.rootLocusBranches[0].gainK == 0.0);
    CHECK(std::abs(rep.rootLocusBranches[0].closedLoopPoles[0].real() + 1000.0) < 1e-2);

    // Bode analysis -3dB cutoff frequency verification
    const auto bodeRes = AnalyzeBodeSweep(rcComps, 2, 10.0, 10000.0, 81);
    CHECK(bodeRes.ok && bodeRes.hasCutoff);
    CHECK(std::abs(bodeRes.cutoffHz - 159.155) < 10.0);

    // 2-source DC superposition verification + SwitchEvent t=0 Laplace IC
    std::vector<CircuitComponent> supComps;
    CHECK(ParseNetlistText("V1 1 0 10; R1 1 2 1k; R2 2 3 1k; V2 3 0 4", supComps));
    const auto supRep = AnalyzeFullSymbolicCircuit(supComps, 2);
    CHECK(supRep.ok && supRep.dcSuperposition.size() == 2);
    const double sumSup = supRep.dcSuperposition[0].nodeContributionsV.at(2) +
                          supRep.dcSuperposition[1].nodeContributionsV.at(2);
    CHECK(std::abs(sumSup - 7.0) < 1e-4);

    // SwitchEvent t=0: SW1 initially closed charges C1 to 10V at t=0^-, then opens at t=0^+
    // Exact Laplace: V_2(s) = (C1*v_C(0^-)) / (s*C1 + 1/R1) = 0.01 / (1 + 0.001*s) -> V_2(0)=0.01, lim_{s->inf} s*V_2(s)=10.0
    std::vector<CircuitComponent> swComps;
    CHECK(ParseNetlistText("V1 1 0 10; SW SW1 1 2 1; C1 2 0 1u; R1 2 0 1k", swComps));
    const auto swRep = AnalyzeFullSymbolicCircuit(swComps, 2, 0.0, {}, { { "SW1", 0.0, true, false } });
    CHECK(swRep.ok && !swRep.resolvedInitialConditions.empty());
    CHECK(std::abs(swRep.resolvedInitialConditions[0].value - 10.0) < 1e-3);
    CHECK(swRep.nodeVoltagesS.count(2));
    if (swRep.nodeVoltagesS.count(2)) {
        const auto& v2s = swRep.nodeVoltagesS.at(2);
        CHECK(std::abs(v2s.Eval({ 0.0, 0.0 }).real() - 0.01) < 1e-4);
        CHECK(std::abs((std::complex<double>(1e9, 0.0) * v2s.Eval({ 1e9, 0.0 })).real() - 10.0) < 1e-2);
    }
}

void TestTintaNucleoPorts() {
    // 1. Thevenin / Norton (resistive + active VCVS)
    {
        std::vector<CircuitComponent> div;
        CHECK(ParseNetlistText("V1 1 0 12; R1 1 2 2k; R2 2 0 4k", div));
        const auto th = ComputeTheveninNorton(div, 2, 0);
        CHECK(th.ok);
        CHECK(std::abs(th.vTheveninV - 8.0) < 1e-4);
        CHECK(std::abs(th.rTheveninOhm - (4000.0 / 3.0)) < 1.0);
        CHECK(std::abs(th.iNortonA - (8.0 / (4000.0 / 3.0))) < 1e-5);

        // Active circuit Thevenin/Norton (VCVS with output R_out = 500 Ohm -> V_th = 6V, R_th = 500 Ohm)
        std::vector<CircuitComponent> actCkt;
        CHECK(ParseNetlistText("V1 1 0 2; R1 1 0 1k; E1 2 0 1 0 3.0; Rout 2 3 500", actCkt));
        const auto actTh = ComputeTheveninNorton(actCkt, 3, 0);
        CHECK(actTh.ok && std::abs(actTh.vTheveninV - 6.0) < 1e-3 && std::abs(actTh.rTheveninOhm - 500.0) < 1.0);

        // Negative test: invalid port
        const auto badTh = ComputeTheveninNorton(div, 2, 2);
        CHECK(!badTh.ok);
    }

    // 2. 4/5/6-band color resistor decoders + negative test
    {
        const auto r4 = DecodeResistorColorBands({ 4, 7, 2, -1 }); // 4.7 kOhm +/- 5%
        CHECK(r4.ok && std::abs(r4.ohms - 4700.0) < 1e-6 && std::abs(r4.tolerancePct - 5.0) < 1e-6);

        const auto r6 = DecodeResistorColorNames({ "brown", "black", "black", "red", "brown", "blue" }); // 10 kOhm +/- 1% 10 ppm/K
        CHECK(r6.ok && std::abs(r6.ohms - 10000.0) < 1e-6 && std::abs(r6.tolerancePct - 1.0) < 1e-6 && r6.tcrPpmPerK == 10);

        const auto rBad = DecodeResistorColorBands({ 1, 2, 3 });
        CHECK(!rBad.ok);
    }

    // 3. SMD Resistor Decoder (000, 3-digit, 4-digit, R/K/M, EIA-96) + negative test
    {
        const auto s0 = DecodeSmdResistorCode("000");
        CHECK(s0.ok && s0.ohms == 0.0);
        const auto s3 = DecodeSmdResistorCode("472");
        CHECK(s3.ok && std::abs(s3.ohms - 4700.0) < 1e-6);
        const auto s4 = DecodeSmdResistorCode("1002");
        CHECK(s4.ok && std::abs(s4.ohms - 10000.0) < 1e-6 && s4.tolerancePct == 1.0);
        const auto sr = DecodeSmdResistorCode("4R7");
        CHECK(sr.ok && std::abs(sr.ohms - 4.7) < 1e-6);
        const auto se = DecodeSmdResistorCode("01A"); // 100 Ohm
        CHECK(se.ok && std::abs(se.ohms - 100.0) < 1e-6);
        const auto se2 = DecodeSmdResistorCode("96C"); // 976 * 100 = 97.6 kOhm
        CHECK(se2.ok && std::abs(se2.ohms - 97600.0) < 1e-3);
        const auto sBad = DecodeSmdResistorCode("ZZZ");
        CHECK(!sBad.ok);
    }

    // 4. Toolbox (9 functions)
    {
        CHECK(std::abs(Toolbox::VoltageDivider(10.0, 1000.0, 1000.0) - 5.0) < 1e-9);
        CHECK(std::abs(Toolbox::OpAmpNonInvertingGain(9000.0, 1000.0) - 10.0) < 1e-9);
        CHECK(std::abs(Toolbox::OpAmpInvertingGain(1000.0, 10000.0) + 10.0) < 1e-9);
        CHECK(std::abs(Toolbox::RcCutoffHz(1000.0, 1e-6) - 159.15494) < 1e-3);
        CHECK(std::abs(Toolbox::LcResonanceHz(1e-3, 1e-6) - 5032.9212) < 1e-2);
        CHECK(std::abs(Toolbox::RcTimeConstantSec(1000.0, 1e-6) - 0.001) < 1e-9);
        CHECK(std::abs(Toolbox::ToDecibels(10.0, false) - 20.0) < 1e-9);
        CHECK(std::abs(Toolbox::FromDecibels(20.0, false) - 10.0) < 1e-9);
        CHECK(Toolbox::FormatEngineeringNotation(4700.0, "Ohm").find("4.7 kOhm") != std::string::npos);
    }

    // 5. 14-Feature Stroke Recognizer + TrainableClassifier
    {
        StrokePolyline lineStroke;
        for (int i = 0; i <= 20; ++i) lineStroke.points.push_back({ static_cast<double>(i) * 10.0, 50.0 });
        const auto f14 = ExtractCircuitFeatures14({ lineStroke });
        for (float v : f14) CHECK(v >= 0.0f && v <= 1.0f);
        const auto rw = RecognizeCircuitStrokes({ lineStroke });
        CHECK(rw.kind == ComponentKind::Wire && rw.confidence > 0.7f);

        // Scale & translation invariance of ExtractCircuitFeatures14
        StrokePolyline scaledLine;
        for (const auto& p : lineStroke.points) scaledLine.points.push_back({ p.x * 3.5 + 120.0, p.y * 3.5 - 40.0 });
        const auto f14Scaled = ExtractCircuitFeatures14({ scaledLine });
        for (std::size_t k = 0; k < 14; ++k) CHECK(std::abs(f14[k] - f14Scaled[k]) < 0.05f);

        // Zig-zag resistor stroke
        StrokePolyline zig;
        const std::vector<Vec2> zigPts{
            {0,0},{20,0},{25,-12},{35,12},{45,-12},{55,12},{65,-12},{70,0},{90,0}
        };
        for (std::size_t i = 1; i < zigPts.size(); ++i) {
            for (int s = 0; s < 5; ++s) {
                const double t = static_cast<double>(s) / 5.0;
                zig.points.push_back({ zigPts[i - 1].x + (zigPts[i].x - zigPts[i - 1].x) * t,
                                       zigPts[i - 1].y + (zigPts[i].y - zigPts[i - 1].y) * t });
            }
        }
        zig.points.push_back(zigPts.back());
        const auto rRes = RecognizeCircuitStrokes({ zig });
        CHECK(rRes.kind == ComponentKind::Resistor && rRes.confidence > 0.55f);

        // Trainable recognizer + JSON roundtrip
        TrainableCircuitRecognizer rec;
        rec.AddExample(ComponentKind::MosfetN, { zig, lineStroke });
        const std::string json = rec.SerializeJson();
        TrainableCircuitRecognizer rec2;
        CHECK(rec2.DeserializeJson(json));
        CHECK(rec2.SampleCount() == 1);
        const auto cls = rec2.Classify({ zig, lineStroke }, 0.70f);
        CHECK(cls.fromUserTemplate && cls.kind == ComponentKind::MosfetN && cls.confidence > 0.9f);
    }
}

void TestSchematicTopologyOverlaysAndExporters() {
    // 1. Snapping, Manhattan A* routing around obstacle, DSU netlist compile, JSON round-trip
    SchematicDocument doc;
    doc.gridPitch = 20.0;

    auto makeSym = [&](std::uint64_t id, ComponentKind k, const std::string& nm, double val, double x, double y, int nPos = 0, int nNeg = 0) {
        SchematicSymbol s;
        s.id = id;
        s.component = MakeComp(k, nm, nPos, nNeg, 0, val);
        s.center = { x, y };
        s.EnsurePinsFromTemplate();
        return s;
    };
    SchematicSymbol sV = makeSym(1, ComponentKind::VoltageSource, "V1", 10.0, 100.0, 100.0);
    SchematicSymbol sR1 = makeSym(2, ComponentKind::Resistor, "R1", 1000.0, 260.0, 100.0);
    SchematicSymbol sR2 = makeSym(3, ComponentKind::Resistor, "R2", 1000.0, 420.0, 100.0);
    SchematicSymbol sGnd = makeSym(4, ComponentKind::Ground, "GND1", 0.0, 100.0, 160.0);
    doc.symbols = { sV, sR1, sR2, sGnd };

    // Magnetic snap near sV pin 1 (140, 100)
    const SnapResult sn = SnapPointToSchematic(doc, { 143.0, 98.0 });
    CHECK(sn.kind == SnapResult::Kind::Pin && sn.symbolId == 1);

    // Wire from sV pin 0 (60,100) to sGnd pin 0 (100,140)
    Wire wGnd;
    wGnd.id = 10;
    wGnd.vertices = RouteManhattanWire(sV.PinWorldPos(0), sGnd.PinWorldPos(0), doc.symbols);
    CHECK(wGnd.vertices.size() >= 2);
    // Wire from sV pin 1 (140,100) to sR1 pin 0 (220,100)
    Wire w1;
    w1.id = 11;
    w1.vertices = { sV.PinWorldPos(1), sR1.PinWorldPos(0) };
    // Wire from sR1 pin 1 (300,100) to sR2 pin 0 (380,100)
    Wire w2;
    w2.id = 12;
    w2.vertices = { sR1.PinWorldPos(1), sR2.PinWorldPos(0) };
    // Wire from sR2 pin 1 (460,100) back to GND via net label + T-junction drop onto wGnd
    Wire w3;
    w3.id = 13;
    w3.vertices = { sR2.PinWorldPos(1), { 460.0, 140.0 }, { 100.0, 140.0 } };
    // Extra T-junction drop onto middle of w1
    Wire wTap;
    wTap.id = 14;
    wTap.vertices = { { 180.0, 100.0 }, { 180.0, 60.0 } };
    doc.wires = { wGnd, w1, w2, w3, wTap };

    const auto compiled = CompileSchematicToMna(doc);
    CHECK(compiled.size() == 4);
    CHECK(!doc.computedJunctions.empty());
    const auto diags = ValidateSchematicTopology(doc);
    for (const auto& d : diags) {
        CHECK(d.code != TopologyDiagnostic::Code::MissingGround);
        CHECK(d.code != TopologyDiagnostic::Code::ShortedVoltageSource);
    }

    const auto sol = SolveMnaCircuit(compiled, 0.0);
    CHECK(sol.ok);

    // Overlay check (current arrows + polarity badges)
    const auto ov = BuildSchematicOverlay(doc, sol);
    CHECK(!ov.currentArrows.empty() && !ov.polarityBadges.empty());

    // Schematic JSON roundtrip & F-173 Search Metadata Extractor
    const std::string schJson = SchematicToJson(doc);
    SchematicDocument docReloaded;
    CHECK(SchematicFromJson(schJson, docReloaded));
    CHECK(docReloaded.symbols.size() == 4 && docReloaded.wires.size() == 5);

    const auto metaJson = ExtractCircuitIndexMetadata(schJson);
    CHECK(metaJson.ok && metaJson.searchableTextUtf8.find("R1") != std::string::npos);
    const auto metaNet = ExtractCircuitIndexMetadata("V1 1 0 5; R1 1 2 10k; OPAMP U1 0 2 3 LM358");
    CHECK(metaNet.ok && metaNet.searchableTextUtf8.find("LM358") != std::string::npos &&
          metaNet.searchableTextUtf8.find("10k") != std::string::npos);

    // Negative topology checks: MissingGround, FloatingNode, ShortedVoltageSource, CurrentSourceOpen
    {
        SchematicDocument badDoc;
        SchematicSymbol vShort = makeSym(1, ComponentKind::VoltageSource, "Vbad", 5.0, 100.0, 100.0, 1, 1);
        vShort.pins[0].resolvedNetId = 1;
        vShort.pins[1].resolvedNetId = 1;
        SchematicSymbol iOpen = makeSym(2, ComponentKind::CurrentSource, "Ibad", 1.0, 200.0, 100.0, 2, 3);
        iOpen.pins[0].resolvedNetId = 2;
        iOpen.pins[1].resolvedNetId = 3;
        badDoc.symbols = { vShort, iOpen };
        const auto badDiags = ValidateSchematicTopology(badDoc);
        bool sawNoGnd = false, sawShortV = false, sawOpenI = false, sawFloat = false;
        for (const auto& d : badDiags) {
            if (d.code == TopologyDiagnostic::Code::MissingGround) sawNoGnd = true;
            if (d.code == TopologyDiagnostic::Code::ShortedVoltageSource) sawShortV = true;
            if (d.code == TopologyDiagnostic::Code::CurrentSourceOpen) sawOpenI = true;
            if (d.code == TopologyDiagnostic::Code::FloatingNode) sawFloat = true;
        }
        CHECK(sawNoGnd && sawShortV && sawOpenI && sawFloat);
    }

    // All 53+ symbols IEC & ANSI geometry non-empty + IEC vs ANSI differentiation
    for (int t = 0; t <= 52; ++t) {
        const ComponentKind k = FromTintaComponentCode(t);
        const auto gIec = GetComponentSymbolGeometry(k, SymbolStandard::IEC);
        const auto gAnsi = GetComponentSymbolGeometry(k, SymbolStandard::ANSI);
        CHECK(!gIec.lines.empty() || !gIec.polys.empty() || !gIec.circles.empty());
        CHECK(!gAnsi.lines.empty() || !gAnsi.polys.empty() || !gAnsi.circles.empty());
    }
    CHECK(GetComponentSymbolGeometry(ComponentKind::Resistor, SymbolStandard::IEC).polys[0].pts.size() == 4);
    CHECK(GetComponentSymbolGeometry(ComponentKind::Resistor, SymbolStandard::ANSI).polys[0].pts.size() == 8);

    // CAD/EDA Exporters: SVG, DXF, KiCad S-Expr, SPICE .cir
    const std::string svg = ExportSchematicToSvg(doc, &sol);
    CHECK(svg.find("<svg") != std::string::npos && svg.find("class=\"wires\"") != std::string::npos &&
          svg.find("class=\"overlays\"") != std::string::npos);

    const std::string dxf = ExportSchematicToDxf(doc, &sol);
    CHECK(dxf.find("HEADER") != std::string::npos && dxf.find("TABLES") != std::string::npos &&
          dxf.find("LAYER") != std::string::npos && dxf.find("BLOCKS") != std::string::npos &&
          dxf.find("INSERT") != std::string::npos && dxf.find("EOF") != std::string::npos);

    const std::string kicadS = ExportCircuitToKiCadSExpr(compiled, "DividerTest");
    CHECK(kicadS.find("(export (version \"D\")") == 0 && kicadS.find("(nets") != std::string::npos);
    int bal = 0;
    for (char ch : kicadS) { if (ch == '(') ++bal; else if (ch == ')') --bal; }
    CHECK(bal == 0);

    const std::string cir = ExportCircuitToSpiceCir(compiled, "DividerTest", ".op");
    CHECK(cir.find(".model") != std::string::npos && cir.find(".op") != std::string::npos &&
          cir.find(".end") != std::string::npos);
}

void TestCodeBlockMcuCoSimAndControlFlowcharts() {
    // 1. CodeBlock Tokenizer
    {
        const std::string code =
            "#include <Arduino.h>\n"
            "// blink\n"
            "void setup() { pinMode(D13, OUTPUT); }\n"
            "void loop() { digitalWrite(D13, HIGH); delay(10); }\n";
        const auto hl = TokenizeCodeBlock(code, CodeLanguage::ArduinoIno);
        CHECK(hl.lineCount == 5 && hl.tokens.size() >= 15);
        bool sawPre = false, sawComment = false, sawType = false, sawApi = false;
        for (const auto& t : hl.tokens) {
            if (t.kind == CodeTokenKind::Preprocessor) sawPre = true;
            if (t.kind == CodeTokenKind::Comment) sawComment = true;
            if (t.kind == CodeTokenKind::Type) sawType = true;
            if (t.kind == CodeTokenKind::McuApi) sawApi = true;
        }
        CHECK(sawPre && sawComment && sawType && sawApi);

        const auto py = TokenizeCodeBlock("def f(x):\n  # hi\n  return x + 1", CodeLanguage::Python);
        CHECK(py.lineCount == 3 && !py.tokens.empty());
    }

    // 2. Deterministic MCU Co-Simulation (Arduino Uno D13 driving RC load)
    {
        std::vector<CircuitComponent> load;
        CHECK(ParseNetlistText("R1 1 2 1k; C1 2 0 1u", load));
        const std::string sketch =
            "void setup() {\n"
            "  pinMode(D13, OUTPUT);\n"
            "  Serial.println(\"BOOT\");\n"
            "}\n"
            "void loop() {\n"
            "  digitalWrite(D13, HIGH);\n"
            "  delay(5);\n"
            "  digitalWrite(D13, LOW);\n"
            "  delay(5);\n"
            "}\n";
        const std::vector<McuPinBinding> pins{ { "D13", 1, McuPinMode::Output } };
        const auto mcuRes = RunMcuCircuitCoSimulation(load, sketch, McuBoardKind::ArduinoUno, pins, 0.02, 200);
        CHECK(mcuRes.ok);
        CHECK(!mcuRes.serialLog.empty() && mcuRes.serialLog[0] == "BOOT");
        CHECK(mcuRes.pinVoltages.at("D13").size() == 201);
        CHECK(mcuRes.circuitWaveforms.vMax > 4.5 && mcuRes.circuitWaveforms.vMin < 0.5);

        // ESP32 PWM analogWrite(GPIO2, 128) ~ 50% of 3.3V = 1.65V average
        const std::string espCode =
            "void setup() { pinMode(GPIO2, OUTPUT); }\n"
            "void loop() { analogWrite(GPIO2, 128); delay(10); }\n";
        McuPinBinding espPin;
        espPin.mcuPinName = "GPIO2";
        espPin.circuitNode = 1;
        espPin.mode = McuPinMode::PwmOut;
        espPin.pwmFreqHz = 500.0;
        const auto espRes = RunMcuCircuitCoSimulation(load, espCode, McuBoardKind::Esp32, { espPin }, 0.02, 200);
        CHECK(espRes.ok);
        CHECK(espRes.circuitWaveforms.vDcAvg > 1.2 && espRes.circuitWaveforms.vDcAvg < 2.1);

        // Negative test: invalid pin on ATtiny85
        const auto badMcu = RunMcuCircuitCoSimulation(load, sketch, McuBoardKind::Attiny85, { { "GPIO39", 1, McuPinMode::Output } }, 0.01, 50);
        CHECK(!badMcu.ok);
    }

    // 3. Control Block Diagram: Negative feedback G(s) = 10/(s+2), H(s) = 1 -> 10/(s+12)
    {
        auto makeCtrlEdge = [](std::uint64_t id, std::uint64_t from, std::uint64_t to, int sgn) {
            ControlEdge e;
            e.id = id;
            e.fromId = from;
            e.toId = to;
            e.sign = sgn;
            return e;
        };
        ControlBlockDiagram diag;
        diag.title = "FirstOrderFeedback";
        diag.nodes = {
            { 1, ControlNodeKind::Input, "U(s)" },
            { 2, ControlNodeKind::SumJunction, "Sum" },
            { 3, ControlNodeKind::TransferFunction, "G(s)", {}, 96, 52, { Poly1D{ { 10.0 } }, Poly1D{ { 2.0, 1.0 } } } },
            { 4, ControlNodeKind::Output, "Y(s)" }
        };
        diag.edges = {
            makeCtrlEdge(10, 1, 2, +1),
            makeCtrlEdge(11, 2, 3, +1),
            makeCtrlEdge(12, 3, 4, +1),
            makeCtrlEdge(13, 3, 2, -1)
        };
        const auto ctrl = AnalyzeControlBlockDiagram(diag, 2.0, 200);
        CHECK(ctrl.ok && ctrl.isStableRouth);
        CHECK(ctrl.poles.size() == 1 && std::abs(ctrl.poles[0].real() + 12.0) < 1e-4);
        CHECK(std::abs(ctrl.closedLoopTf.Eval({ 0.0, 0.0 }).real() - (10.0 / 12.0)) < 1e-4);
        CHECK(!ctrl.stepOutput.empty() && std::abs(ctrl.stepOutput.back() - (10.0 / 12.0)) < 1e-3);
        CHECK(!ctrl.forwardPathsDesc.empty() && !ctrl.feedbackLoopsDesc.empty());

        // Negative stability test: positive-pole plant G(s) = 1/(s-5) open-loop / positive feedback -> unstable
        ControlBlockDiagram unsDiag;
        unsDiag.nodes = {
            { 1, ControlNodeKind::Input, "U" },
            { 2, ControlNodeKind::TransferFunction, "G", {}, 96, 52, { Poly1D{ { 1.0 } }, Poly1D{ { -5.0, 1.0 } } } },
            { 3, ControlNodeKind::Output, "Y" }
        };
        unsDiag.edges = { makeCtrlEdge(1, 1, 2, +1), makeCtrlEdge(2, 2, 3, +1) };
        const auto unsRes = AnalyzeControlBlockDiagram(unsDiag);
        CHECK(unsRes.ok && !unsRes.isStableRouth);
    }

    // 4. Flowchart validation & SVG export
    {
        auto makeFlowEdge = [](std::uint64_t id, std::uint64_t from, std::uint64_t to, const std::string& lbl) {
            FlowEdge e;
            e.id = id;
            e.fromId = from;
            e.toId = to;
            e.branchLabel = lbl;
            return e;
        };
        FlowchartDiagram fc;
        fc.nodes = {
            { 1, FlowNodeKind::StartEnd, "Inicio", { 200.0, 50.0 } },
            { 2, FlowNodeKind::Decision, "V > 5V?", { 200.0, 150.0 } },
            { 3, FlowNodeKind::Process, "Encender LED", { 100.0, 250.0 } },
            { 4, FlowNodeKind::Process, "Apagar LED", { 300.0, 250.0 } },
            { 5, FlowNodeKind::StartEnd, "Fin", { 200.0, 350.0 } }
        };
        fc.edges = {
            makeFlowEdge(1, 1, 2, ""),
            makeFlowEdge(2, 2, 3, "Si"),
            makeFlowEdge(3, 2, 4, "No"),
            makeFlowEdge(4, 3, 5, ""),
            makeFlowEdge(5, 4, 5, "")
        };
        const auto diags = ValidateFlowchart(fc);
        CHECK(diags.empty());
        const std::string svg = ExportFlowchartToSvg(fc);
        CHECK(svg.find("<svg") != std::string::npos && svg.find("Encender LED") != std::string::npos);

        // Negative test: missing StartEnd & Decision < 2 branches
        FlowchartDiagram badFc;
        badFc.nodes = { { 1, FlowNodeKind::Decision, "OnlyOne", { 100.0, 100.0 } } };
        const auto badDiags = ValidateFlowchart(badFc);
        CHECK(badDiags.size() >= 2);
    }
}

void TestIteration2Regressions() {
    // ===== Group A: Symbolic, State-Space, Bode/Nyquist, Mesh & Laplace =====
    // A1. Matrix Pencil DFT H(s) scaling on 1uH/1uF RLC + 5th/6th-order kOhm/uF RC + 6th-order Sallen-Key
    {
        std::vector<CircuitComponent> rlcComps;
        CHECK(ParseNetlistText("V1 1 0 1; R1 1 2 10; L1 2 3 1u; C1 3 0 1u", rlcComps));
        const auto rlcRep = AnalyzeFullSymbolicCircuit(rlcComps, 3);
        CHECK(rlcRep.ok);
        CHECK(rlcRep.transferFunction.den.Degree() == 2);
        CHECK(rlcRep.poles.size() == 2);
        const std::complex<double> sTest{ 0.0, 2.0 * kPi * 159154.94 };
        const auto rlcAc = SolveMnaCircuit(rlcComps, 159154.94);
        CHECK(rlcAc.ok && std::abs(rlcRep.transferFunction.Eval(sTest) - rlcAc.nodeVoltages.at(3)) < 1e-3);

        // 5th-order RC ladder (1kOhm / 1uF)
        std::vector<CircuitComponent> rc5;
        CHECK(ParseNetlistText("V1 1 0 1; R1 1 2 1k; C1 2 0 1u; R2 2 3 1k; C2 3 0 1u; R3 3 4 1k; C3 4 0 1u; R4 4 5 1k; C4 5 0 1u; R5 5 6 1k; C5 6 0 1u", rc5));
        const auto rc5Rep = AnalyzeFullSymbolicCircuit(rc5, 6);
        CHECK(rc5Rep.ok && rc5Rep.transferFunction.den.Degree() == 5 && rc5Rep.poles.size() == 5);
        CHECK(std::abs(rc5Rep.dcGain - 1.0) < 1e-3);
        const auto rc5Ac = SolveMnaCircuit(rc5, 25.0);
        CHECK(rc5Ac.ok && std::abs(rc5Rep.transferFunction.Eval({ 0.0, 2.0 * kPi * 25.0 }) - rc5Ac.nodeVoltages.at(6)) < 1e-3);

        // 6th-order RC ladder (10kOhm / 100nF)
        std::vector<CircuitComponent> rc6;
        CHECK(ParseNetlistText("V1 1 0 1; R1 1 2 10k; C1 2 0 100n; R2 2 3 10k; C2 3 0 100n; R3 3 4 10k; C3 4 0 100n; R4 4 5 10k; C4 5 0 100n; R5 5 6 10k; C5 6 0 100n; R6 6 7 10k; C6 7 0 100n", rc6));
        const auto rc6Rep = AnalyzeFullSymbolicCircuit(rc6, 7);
        CHECK(rc6Rep.ok && rc6Rep.transferFunction.den.Degree() == 6 && rc6Rep.poles.size() == 6);
        CHECK(std::abs(rc6Rep.dcGain - 1.0) < 1e-3);

        // 6th-order Sallen-Key lowpass cascade (3 ideal OpAmps, 6 caps)
        std::vector<CircuitComponent> sk6;
        CHECK(ParseNetlistText(
            "V1 1 0 1\n"
            "R1 1 2 10k; R2 2 3 10k; C1 2 4 10n; C2 3 0 10n; OPAMP U1 3 4 4\n"
            "R3 4 5 10k; R4 5 6 10k; C3 5 7 10n; C4 6 0 10n; OPAMP U2 6 7 7\n"
            "R5 7 8 10k; R6 8 9 10k; C5 8 10 10n; C6 9 0 10n; OPAMP U3 9 10 10\n", sk6));
        const auto sk6Rep = AnalyzeFullSymbolicCircuit(sk6, 10);
        CHECK(sk6Rep.ok && sk6Rep.transferFunction.den.Degree() == 6 && sk6Rep.poles.size() == 6);
        const auto sk6Ac = SolveMnaCircuit(sk6, 1591.55);
        CHECK(sk6Ac.ok && std::abs(sk6Rep.transferFunction.Eval({ 0.0, 2.0 * kPi * 1591.55 }) - sk6Ac.nodeVoltages.at(10)) < 1e-3);
    }

    // A2. ExtractCircuitStateSpace: Transformer + Ground symbol + high-numbered VCVS/Potentiometer
    {
        std::vector<CircuitComponent> xfmrSs;
        CHECK(ParseNetlistText("V1 1 0 10; R1 1 2 10; XFMR T1 2 0 3 0 2.0; R2 3 0 100; C1 3 0 1u", xfmrSs));
        const auto ssX = ExtractCircuitStateSpace(xfmrSs, 3);
        CHECK(ssX.ok && ssX.A.size() == 1);

        // Ground symbol tying capacitor terminal (node 3) to GND
        std::vector<CircuitComponent> gndSym;
        CHECK(ParseNetlistText("V1 1 0 1; R1 1 2 1k; C1 2 3 1u; GND G1 3", gndSym));
        const auto ssG = ExtractCircuitStateSpace(gndSym, 2);
        CHECK(ssG.ok && ssG.A.size() == 1 && std::abs(ssG.A[0][0] + 1000.0) < 1.0);

        // High-numbered VCVS control nodes and Potentiometer wiper node (OOB regression)
        CircuitComponent v1 = MakeComp(ComponentKind::VoltageSource, "V1", 1, 0, 0, 5.0);
        CircuitComponent pot = MakeComp(ComponentKind::Potentiometer, "P1", 1, 0, 5, 1000.0);
        pot.auxValue = 0.5;
        CircuitComponent vcvs = MakeComp(ComponentKind::Vcvs, "E1", 2, 0, 0, 2.0);
        vcvs.nodeCtrlPos = 5;
        vcvs.nodeCtrlNeg = 0;
        vcvs.auxValue = 2.0;
        CircuitComponent rL = MakeComp(ComponentKind::Resistor, "RL", 2, 0, 0, 1000.0);
        const auto ssHigh = ExtractCircuitStateSpace({ v1, pot, vcvs, rL }, 2);
        CHECK(ssHigh.ok && std::abs(ssHigh.D[0][0] - 1.0) < 1e-4);
    }

    // A3. Mesh KVL & Supermesh authenticity + OpAmp exclusion
    {
        // Two-loop shared current source: must NOT emit hardcoded I_{m,a} - I_{m,b}
        std::vector<CircuitComponent> m2;
        CHECK(ParseNetlistText("V1 1 0 10; R1 1 2 100; I1 2 0 1; R2 2 3 100; R3 3 0 100", m2));
        const auto m2Rep = AnalyzeFullSymbolicCircuit(m2, 2);
        CHECK(m2Rep.ok && m2Rep.meshKvlEquationsLatex.size() == 2);
        CHECK(m2Rep.meshKvlEquationsLatex[0].find("I_{m,a}") == std::string::npos);
        CHECK(m2Rep.meshKvlEquationsLatex[0].find("I_{m1}") != std::string::npos);
        CHECK(m2Rep.meshKvlEquationsLatex[0].find("I_{m2}") != std::string::npos);

        // Single-loop outer current source: must NOT emit "Supermalla"
        std::vector<CircuitComponent> m1;
        CHECK(ParseNetlistText("I1 0 1 2; R1 1 0 100", m1));
        const auto m1Rep = AnalyzeFullSymbolicCircuit(m1, 1);
        CHECK(m1Rep.ok && m1Rep.meshKvlEquationsLatex.size() == 1);
        CHECK(m1Rep.meshKvlEquationsLatex[0].find("Supermalla") == std::string::npos);
        CHECK(m1Rep.meshKvlEquationsLatex[0].find("I_{m1}") != std::string::npos);
    }

    // A4. AnalyzeBodeSweep High-Pass -3 dB cutoff & Inverting negative-DC phase canonicalization
    {
        std::vector<CircuitComponent> hpComps;
        CHECK(ParseNetlistText("V1 1 0 1; C1 1 2 1u; R1 2 0 1k", hpComps));
        const auto hpBode = AnalyzeBodeSweep(hpComps, 2, 1.0, 100000.0, 101);
        CHECK(hpBode.ok && hpBode.hasCutoff);
        CHECK(std::abs(hpBode.cutoffHz - 159.155) < 10.0);

        // Inverting OpAmp with 3-pole lag: initial DC negative phase canonicalized < 0, PM not > 180
        std::vector<CircuitComponent> inv3;
        CHECK(ParseNetlistText(
            "V1 1 0 1; R1 1 2 1k; R2 2 3 10k; OPAMP U1 0 2 3\n"
            "R3 3 4 1k; C1 4 0 1u; E1 5 0 4 0 1.0\n"
            "R4 5 6 1k; C2 6 0 1u; E2 7 0 6 0 1.0\n"
            "R5 7 8 1k; C3 8 0 1u\n", inv3));
        const auto invBode = AnalyzeBodeSweep(inv3, 8, 1.0, 100000.0, 101);
        CHECK(invBode.ok && !invBode.extendedPoints.empty());
        CHECK(invBode.extendedPoints.front().phaseUnwrappedDeg < 0.0);
        CHECK(invBode.hasPhaseMargin && invBode.phaseMarginDeg < 180.0);
    }

    // A5. DC Superposition with dcOffset & Laplace Switch/ManualIC deduplication + inactive branch sync
    {
        CircuitComponent v1 = MakeComp(ComponentKind::VoltageSource, "V1", 1, 0, 0, 5.0);
        v1.dcOffset = 3.0;
        CircuitComponent r1 = MakeComp(ComponentKind::Resistor, "R1", 1, 2, 0, 1000.0);
        CircuitComponent r2 = MakeComp(ComponentKind::Resistor, "R2", 2, 3, 0, 1000.0);
        CircuitComponent v2 = MakeComp(ComponentKind::VoltageSource, "V2", 3, 0, 0, 2.0);
        v2.dcOffset = 4.0;
        const auto supRep = AnalyzeFullSymbolicCircuit({ v1, r1, r2, v2 }, 2);
        const auto supMna = SolveMnaCircuit({ v1, r1, r2, v2 }, 0.0);
        CHECK(supRep.ok && supMna.ok && supRep.dcSuperposition.size() == 2);
        const double sumSup = supRep.dcSuperposition[0].nodeContributionsV.at(2) +
                              supRep.dcSuperposition[1].nodeContributionsV.at(2);
        CHECK(std::abs(sumSup - supMna.nodeVoltages.at(2).real()) < 1e-6);

        // Inactive component before L1 + manualIC override of switch-derived IC
        std::vector<CircuitComponent> lapComps;
        CHECK(ParseNetlistText("V1 1 0 10; R_off 1 0 1k; SW SW1 1 2 1; L1 2 3 10m; R1 3 0 100", lapComps));
        lapComps[1].active = false;
        const auto lapRep = AnalyzeFullSymbolicCircuit(lapComps, 3, 0.0, { { "L1", 0.25 } }, { { "SW1", 0.0, true, false } });
        CHECK(lapRep.ok);
        int l1Count = 0;
        double l1Val = 0.0;
        for (const auto& ic : lapRep.resolvedInitialConditions) {
            if (ic.componentName == "L1") { ++l1Count; l1Val = ic.value; }
        }
        CHECK(l1Count == 1 && std::abs(l1Val - 0.25) < 1e-9);

        // Switch-derived inductor IC (without manual override) despite prior inactive component
        const auto lapAuto = AnalyzeFullSymbolicCircuit(lapComps, 3, 0.0, {}, { { "SW1", 0.0, true, false } });
        CHECK(lapAuto.ok && !lapAuto.resolvedInitialConditions.empty());
        CHECK(std::abs(lapAuto.resolvedInitialConditions[0].value - 0.1) < 1e-4);
    }

    // ===== Group B: SPICE/MNA Physics, NR Convergence, Transient, Recognizer =====
    // B1. MosfetP (IRF9540) conducting states & Reverse Vds < 0 NMOS
    {
        for (double vg : { 0.0, 5.0, 10.0 }) {
            CircuitComponent vdd = MakeComp(ComponentKind::VoltageSource, "Vdd", 1, 0, 0, 10.0);
            CircuitComponent vgate = MakeComp(ComponentKind::VoltageSource, "Vg", 3, 0, 0, vg);
            CircuitComponent rd = MakeComp(ComponentKind::Resistor, "Rd", 2, 0, 0, 1.0);
            CircuitComponent mp = MakeComp(ComponentKind::MosfetP, "M1", 2, 1, 3, 0.0, 0.0, "IRF9540");
            const auto res = SolveMnaCircuit({ vdd, vgate, rd, mp }, 0.0);
            CHECK(res.ok);
            const double vd = res.nodeVoltages.at(2).real();
            if (vg == 0.0) CHECK(vd > 9.0);
            else if (vg == 5.0) CHECK(vd > 5.0 && vd < 9.95);
            else CHECK(vd < 1e-4);
        }
        // Reverse-biased NMOS (V_S=5V, V_G=10V, Drain to GND via Rd=10)
        CircuitComponent vs = MakeComp(ComponentKind::VoltageSource, "Vs", 1, 0, 0, 5.0);
        CircuitComponent vg = MakeComp(ComponentKind::VoltageSource, "Vg", 3, 0, 0, 10.0);
        CircuitComponent rd = MakeComp(ComponentKind::Resistor, "Rd", 2, 0, 0, 10.0);
        CircuitComponent mn = MakeComp(ComponentKind::MosfetN, "M1", 2, 1, 3, 0.0, 0.0, "IRF540");
        const auto revRes = SolveMnaCircuit({ vs, vg, rd, mn }, 0.0);
        CHECK(revRes.ok);
        const double vdRev = revRes.nodeVoltages.at(2).real();
        CHECK(vdRev > 0.5 && vdRev < 5.0);
    }

    // B2. Multiplexer & Demultiplexer channel switching (sel=0V vs sel=5V)
    {
        CircuitComponent v0 = MakeComp(ComponentKind::VoltageSource, "V0", 1, 0, 0, 2.0);
        CircuitComponent v1 = MakeComp(ComponentKind::VoltageSource, "V1", 2, 0, 0, 4.0);
        CircuitComponent vsel = MakeComp(ComponentKind::VoltageSource, "Vsel", 4, 0, 0, 0.0);
        CircuitComponent mux = MakeComp(ComponentKind::Multiplexer, "MX1", 1, 2, 3, 0.0);
        mux.nodeCtrlPos = 4;
        CircuitComponent rl = MakeComp(ComponentKind::Resistor, "RL", 3, 0, 0, 10000.0);
        const auto mSel0 = SolveMnaCircuit({ v0, v1, vsel, mux, rl }, 0.0);
        vsel.value = 5.0;
        const auto mSel1 = SolveMnaCircuit({ v0, v1, vsel, mux, rl }, 0.0);
        CHECK(mSel0.ok && std::abs(mSel0.nodeVoltages.at(3).real() - 2.0) < 0.05);
        CHECK(mSel1.ok && std::abs(mSel1.nodeVoltages.at(3).real() - 4.0) < 0.05);

        CircuitComponent vin = MakeComp(ComponentKind::VoltageSource, "Vin", 3, 0, 0, 5.0);
        CircuitComponent r0 = MakeComp(ComponentKind::Resistor, "R0", 1, 0, 0, 10000.0);
        CircuitComponent r1 = MakeComp(ComponentKind::Resistor, "R1", 2, 0, 0, 10000.0);
        CircuitComponent dmux = MakeComp(ComponentKind::Demultiplexer, "DM1", 1, 2, 3, 0.0);
        dmux.nodeCtrlPos = 4;
        vsel.value = 0.0;
        const auto dSel0 = SolveMnaCircuit({ vin, r0, r1, dmux, vsel }, 0.0);
        vsel.value = 5.0;
        const auto dSel1 = SolveMnaCircuit({ vin, r0, r1, dmux, vsel }, 0.0);
        CHECK(dSel0.ok && dSel0.nodeVoltages.at(1).real() > 4.9 && dSel0.nodeVoltages.at(2).real() < 0.05);
        CHECK(dSel1.ok && dSel1.nodeVoltages.at(2).real() > 4.9 && dSel1.nodeVoltages.at(1).real() < 0.05);
    }

    // B3. LM317 dropout convergence across Vin = 2V..10V
    {
        for (double vin : { 2.0, 4.0, 5.5, 6.5, 7.5, 10.0 }) {
            std::vector<CircuitComponent> lmComps;
            CHECK(ParseNetlistText("V1 1 0 10; REG U1 1 3 2 LM317; R1 2 3 240; R2 3 0 720; RL 2 0 100", lmComps));
            lmComps[0].value = vin;
            const auto r = SolveMnaCircuit(lmComps, 0.0);
            CHECK(r.ok);
            const double vout = r.nodeVoltages.at(2).real();
            if (vin >= 7.5) CHECK(std::abs(vout - 5.0) < 0.15);
            else CHECK(vout >= 0.0 && vout <= vin);
        }
    }

    // B4. SCR & TRIAC latching in both transient APIs
    {
        CircuitComponent vs = MakeComp(ComponentKind::VoltageSource, "V1", 1, 0, 0, 12.0);
        CircuitComponent rl = MakeComp(ComponentKind::Resistor, "R1", 1, 2, 0, 100.0);
        CircuitComponent vg = MakeComp(ComponentKind::PulseSource, "Vg", 3, 0, 0, 5.0);
        vg.waveform = WaveformKind::Pulse;
        vg.freqHz = 50.0; // 20ms period
        vg.dutyCycle = 0.1; // 2ms pulse
        CircuitComponent scr = MakeComp(ComponentKind::Scr, "S1", 2, 0, 3, 0.0);
        const std::vector<CircuitComponent> scrCkt{ vs, rl, vg, scr };
        const auto trScr = SimulateTransientMna(scrCkt, 0.01, 100, 0.0, 2);
        CHECK(trScr.ok && trScr.nodeWaveforms.at(2).back() < 0.5);

        CircuitTransientSession sess;
        CHECK(sess.Initialize(scrCkt, 1e-4, 512, false));
        OscilloscopeFrame fr{};
        CHECK(sess.StepFrame(0.01, fr));
        CHECK(fr.channelRing.at("V(2)").back() < 0.5);

        CircuitComponent triac = MakeComp(ComponentKind::Triac, "T1", 2, 0, 3, 0.0);
        const auto trTriac = SimulateTransientMna({ vs, rl, vg, triac }, 0.01, 100, 0.0, 2);
        CHECK(trTriac.ok && trTriac.nodeWaveforms.at(2).back() < 0.5);
    }

    // B5. Stiff RC Trapezoidal overshoot fix (maxVc <= 5.001V) & LC tank energy retention (>= 99.9%)
    {
        std::vector<CircuitComponent> stiffRc;
        CHECK(ParseNetlistText("V1 1 0 5; SW SW1 1 2 1; AM Am1 2 3; R1 3 4 10; C1 4 0 10u", stiffRc));
        CircuitTransientSession sess;
        CHECK(sess.Initialize(stiffRc, 1.0 / 2400.0, 2048, true));
        OscilloscopeFrame fr{};
        CHECK(sess.StepFrame(0.01, fr));
        CHECK(fr.vMax.at("V(4)") <= 5.001);

        // LC tank with initial capacitor voltage via step pulse or current kick: verify no damping
        CircuitComponent vPulse = MakeComp(ComponentKind::VoltageSource, "V1", 1, 0, 0, 0.0);
        CircuitComponent rSer = MakeComp(ComponentKind::Resistor, "R1", 1, 2, 0, 1e6);
        CircuitComponent cTank = MakeComp(ComponentKind::Capacitor, "C1", 2, 0, 0, 10e-6);
        CircuitComponent lTank = MakeComp(ComponentKind::Inductor, "L1", 2, 0, 0, 10e-3);
        CircuitTransientSession lcSess;
        CHECK(lcSess.Initialize({ MakeComp(ComponentKind::CurrentSource, "I1", 0, 2, 0, 0.25), cTank, lTank }, 5e-5, 2048, true));
        OscilloscopeFrame lcFr{};
        CHECK(lcSess.StepFrame(0.025, lcFr));
        const auto& vc = lcFr.channelRing.at("V(2)");
        double maxFirstHalf = 0.0, maxSecondHalf = 0.0;
        for (std::size_t k = 0; k < vc.size() / 2; ++k) maxFirstHalf = (std::max)(maxFirstHalf, std::abs(vc[k]));
        for (std::size_t k = vc.size() / 2; k < vc.size(); ++k) maxSecondHalf = (std::max)(maxSecondHalf, std::abs(vc[k]));
        CHECK(maxFirstHalf > 1.0 && maxSecondHalf > 0.99 * maxFirstHalf);
        (void)vPulse; (void)rSer;
    }

    // B6. Motor back-EMF ODE (current decays from stall to steady state)
    {
        std::vector<CircuitComponent> motCkt;
        CHECK(ParseNetlistText("V1 1 0 12; AM Am1 1 2; MOT M1 2 0 4", motCkt));
        CircuitTransientSession mSess;
        CHECK(mSess.Initialize(motCkt, 1e-3, 1024, false));
        OscilloscopeFrame mFr{};
        CHECK(mSess.StepFrame(0.5, mFr));
        const auto& ia = mFr.channelRing.at("I(Am1)");
        CHECK(ia.size() > 100);
        CHECK(ia[2] > 1.5);
        CHECK(ia.back() < 0.5 && ia.back() > 0.01);
    }

    // B7. Relay/Fuse in SimulateTransientMna, AcSource DC=.OP, ParseNetlistText, Branch currents & Tellegen
    {
        // Relay & Fuse in SimulateTransientMna
        CircuitComponent vs = MakeComp(ComponentKind::VoltageSource, "V1", 1, 0, 0, 10.0);
        CircuitComponent f1 = MakeComp(ComponentKind::Fuse, "F1", 1, 2, 0, 1.0);
        CircuitComponent rl = MakeComp(ComponentKind::Resistor, "R1", 2, 0, 0, 1.0); // 10A blows 1A fuse
        const auto fuseTr = SimulateTransientMna({ vs, f1, rl }, 0.02, 200, 0.0, 2);
        CHECK(fuseTr.ok && fuseTr.nodeWaveforms.at(2).back() < 1e-3);

        CircuitComponent vCoil = MakeComp(ComponentKind::VoltageSource, "Vc", 3, 0, 0, 5.0);
        CircuitComponent rly = MakeComp(ComponentKind::Relay, "K1", 1, 2, 0, 5.0);
        rly.nodeCtrlPos = 3; rly.nodeCtrlNeg = 0;
        CircuitComponent rLoad = MakeComp(ComponentKind::Resistor, "RL", 2, 0, 0, 100.0);
        const auto rlyTr = SimulateTransientMna({ vs, vCoil, rly, rLoad }, 0.01, 50, 0.0, 2);
        CHECK(rlyTr.ok && rlyTr.nodeWaveforms.at(2).back() > 4.5);

        // AcSource dcOffset=0 at DC .OP
        CircuitComponent vac = MakeComp(ComponentKind::AcSource, "Vac1", 1, 0, 0, 2.0);
        vac.dcOffset = 0.0;
        const auto acDcOp = SolveMnaCircuit({ vac, MakeComp(ComponentKind::Resistor, "R1", 1, 2, 0, 1000.0),
                                              MakeComp(ComponentKind::Resistor, "R2", 2, 0, 0, 1000.0) }, 0.0);
        CHECK(acDcOp.ok && std::abs(acDcOp.nodeVoltages.at(2).real()) < 1e-9);

        // ParseNetlistText fixes
        std::vector<CircuitComponent> p;
        CHECK(ParseNetlistText("RLY K1 1 2 3 0 5\nW1 2 4\nVAC V1 1 0 SINE 5 60 dcOffset=2.5\nXU1 0 4 5 OPAMP_IDEAL\nFF_D D1 1 3 6\nFF_JK J1 1 3 4 7\nMUX M1 1 2 8 3\nDMUX DM1 1 2 9 3\nVP Vp1 1 0\nIP Ip1 1 2", p));
        CHECK(p.size() == 10);
        CHECK(p[0].kind == ComponentKind::Relay && p[0].nodeCtrlPos == 3 && p[0].nodeCtrlNeg == 0 && std::abs(p[0].value - 5.0) < 1e-6);
        CHECK(p[1].kind == ComponentKind::Wire && p[1].nodePos == 2 && p[1].nodeNeg == 4);
        CHECK(p[2].kind == ComponentKind::AcSource && std::abs(p[2].dcOffset - 2.5) < 1e-6);
        CHECK(p[3].kind == ComponentKind::OpAmp && p[3].nodeOut == 5);
        CHECK(p[4].kind == ComponentKind::FlipFlopD && p[5].kind == ComponentKind::FlipFlopJK);
        CHECK(p[6].kind == ComponentKind::Multiplexer && p[7].kind == ComponentKind::Demultiplexer);
        CHECK(p[8].kind == ComponentKind::VoltageProbe && p[9].kind == ComponentKind::CurrentProbe);

        // Tellegen power conservation & non-zero branch currents for BJT NPN/PNP, RealOpAmp, Transformer, JfetP, Comparator, Antenna
        const auto bjtSol = SolveMnaNetlist("VCC Vcc 1 12; R1 1 3 100k; R2 3 0 22k; RC 1 2 1k; RE 4 0 470; Q1 2 3 4 2N2222", 0.0);
        CHECK(bjtSol.ok && std::abs(bjtSol.totalPowerSumW) < 1e-6);
        CHECK(std::abs(bjtSol.branches[5].currentA.real()) > 1e-4);

        const auto pnpSol = SolveMnaNetlist("VCC Vcc 1 12; R1 1 3 22k; R2 3 0 100k; RE 1 4 470; RC 2 0 1k; Q1 2 3 4 2N2907", 0.0);
        CHECK(pnpSol.ok && std::abs(pnpSol.totalPowerSumW) < 1e-6);
        CHECK(pnpSol.nodeVoltages.at(2).real() > 0.5 && pnpSol.nodeVoltages.at(2).real() < 11.5);

        const auto lm358Sol = SolveMnaNetlist("V1 1 0 2; R1 2 0 1k; R2 2 3 2k; OPAMP U1 1 2 3 LM358; RL 3 0 1k", 0.0);
        CHECK(lm358Sol.ok && std::abs(lm358Sol.totalPowerSumW) < 1e-6);
        CHECK(std::abs(lm358Sol.branches[3].currentA.real() - 8e-3) < 5e-4);

        const auto xfmrSol = SolveMnaNetlist("V1 1 0 10; XFMR T1 1 0 2 0 2.0; R1 2 0 100", 60.0);
        CHECK(xfmrSol.ok && std::abs(xfmrSol.totalPowerSumW) < 1e-4);
        CHECK(std::abs(xfmrSol.branches[1].currentA) > 0.1);

        const auto compHi = SolveMnaNetlist("V1 1 0 3; V2 2 0 1; COMP C1 1 2 3", 0.0);
        const auto compLo = SolveMnaNetlist("V1 1 0 1; V2 2 0 3; COMP C1 1 2 3", 0.0);
        CHECK(compHi.ok && compHi.nodeVoltages.at(3).real() > 4.9);
        CHECK(compLo.ok && compLo.nodeVoltages.at(3).real() < 0.1);

        const auto antSol = SolveMnaNetlist("V1 1 0 5; R1 1 2 50; ANT A1 2 0 50", 0.0);
        CHECK(antSol.ok && std::abs(antSol.nodeVoltages.at(2).real() - 2.5) < 1e-4);
        CHECK(std::abs(antSol.totalPowerSumW) < 1e-6);

        const auto jfetP = SolveMnaNetlist("V1 1 0 12; J1 2 3 1 PJF; VG 3 0 12; RD 2 0 1k", 0.0);
        CHECK(jfetP.ok && std::abs(jfetP.totalPowerSumW) < 1e-6);

        // Photodiode transient branch current
        CircuitTransientSession pdSess;
        std::vector<CircuitComponent> pdComps;
        CHECK(ParseNetlistText("PD D1 1 0 1000; R1 1 0 1k", pdComps));
        CHECK(pdSess.Initialize(pdComps, 1e-4, 64, false));
        OscilloscopeFrame pdFr{};
        CHECK(pdSess.StepFrame(1e-3, pdFr));
        CHECK(pdFr.channelRing.count("I(D1)") && std::abs(pdFr.channelRing.at("I(D1)").back()) > 1e-6);
    }

    // B8. ComputeTheveninNorton on JfetN, scale invariance at 0.01x, negative SMD rejection
    {
        std::vector<CircuitComponent> jfCkt;
        CHECK(ParseNetlistText("V1 1 0 12; RD 1 2 1k; J1 2 3 0 NJF; VG 3 0 0", jfCkt));
        const auto thJ = ComputeTheveninNorton(jfCkt, 2, 0);
        CHECK(thJ.ok && thJ.rTheveninOhm > 500.0 && thJ.rTheveninOhm < 1050.0);

        std::vector<CircuitComponent> jfLoaded = jfCkt;
        jfLoaded.push_back(MakeComp(ComponentKind::Resistor, "RL", 2, 0, 0, 1000.0));
        const auto solLoaded = SolveMnaCircuit(jfLoaded, 0.0);
        const double vlPred = thJ.vTheveninV * (1000.0 / (thJ.rTheveninOhm + 1000.0));
        CHECK(solLoaded.ok && std::abs(solLoaded.nodeVoltages.at(2).real() - vlPred) < 0.2);

        StrokePolyline zigBig, zigSmall;
        const std::vector<Vec2> pts{ {0,0},{20,0},{25,-12},{35,12},{45,-12},{55,12},{65,-12},{70,0},{90,0} };
        for (const auto& pt : pts) {
            zigBig.points.push_back(pt);
            zigSmall.points.push_back({ pt.x * 0.01, pt.y * 0.01 });
        }
        const auto fBig = ExtractCircuitFeatures14({ zigBig });
        const auto fSmall = ExtractCircuitFeatures14({ zigSmall });
        for (std::size_t k = 0; k < 14; ++k) CHECK(std::abs(fBig[k] - fSmall[k]) < 1e-4f);

        CHECK(!DecodeSmdResistorCode("-4R7").ok);
        CHECK(!DecodeSmdResistorCode("+4R7").ok);
    }

    // ===== Group C: Schematic, Exporters, MCU Co-Sim & Flowchart =====
    // C1-C3. Collinear obstacle avoidance, NetLabel no false JunctionDot, JSON full roundtrip & strict number, Overlay/DXF alignment, Multi-pin AutoLayout
    {
        SchematicSymbol obs;
        obs.id = 1;
        obs.component = MakeComp(ComponentKind::Resistor, "R_obs", 1, 2, 0, 1000.0);
        obs.center = { 200.0, 100.0 };
        obs.EnsurePinsFromTemplate();
        const auto path = RouteManhattanWire({ 80.0, 100.0 }, { 320.0, 100.0 }, { obs });
        CHECK(path.size() > 2);
        double bx0 = 0.0, by0 = 0.0, bx1 = 0.0, by1 = 0.0;
        obs.BoundingBox(bx0, by0, bx1, by1);
        RectBox box{ bx0, by0, bx1, by1 };
        CHECK(!PathIntersectsObstacles(path, { box }));

        // NetLabel on straight wire must NOT create a JunctionDot
        SchematicDocument lblDoc;
        SchematicSymbol sV; sV.id = 1; sV.component = MakeComp(ComponentKind::VoltageSource, "V1", 0, 0, 0, 5.0); sV.center = { 100.0, 100.0 }; sV.EnsurePinsFromTemplate();
        SchematicSymbol sR; sR.id = 2; sR.component = MakeComp(ComponentKind::Resistor, "R1", 0, 0, 0, 1000.0); sR.center = { 300.0, 100.0 }; sR.EnsurePinsFromTemplate();
        SchematicSymbol sG; sG.id = 3; sG.component = MakeComp(ComponentKind::Ground, "G1", 0, 0, 0, 0.0); sG.center = { 100.0, 160.0 }; sG.EnsurePinsFromTemplate();
        lblDoc.symbols = { sV, sR, sG };
        lblDoc.wires = {
            { 10, { sV.PinWorldPos(1), sR.PinWorldPos(0) }, 0, "", 0, "", -1 },
            { 11, { sV.PinWorldPos(0), sG.PinWorldPos(0) }, 0, "", 0, "", -1 },
            { 12, { sR.PinWorldPos(1), sG.PinWorldPos(0) }, 0, "", 0, "", -1 }
        };
        lblDoc.labels = { { 20, { 200.0, 100.0 }, "V_MID", false } };
        const auto lblMna = CompileSchematicToMna(lblDoc);
        CHECK(lblMna.size() == 3 && lblDoc.computedJunctions.empty());

        // JSON extended roundtrip + malformed JSON number rejection
        SchematicDocument extDoc;
        SchematicSymbol sExt;
        sExt.id = 9;
        sExt.component = MakeComp(ComponentKind::AcSource, "Vac", 1, 0, 0, 5.0, 30.0);
        sExt.component.nodeCtrlPos = 4;
        sExt.component.nodeCtrlNeg = 5;
        sExt.component.extraNodes = { 7, 8 };
        sExt.component.active = false;
        sExt.component.params["Ron"] = 0.25;
        sExt.component.waveform = WaveformKind::Pulse;
        sExt.component.freqHz = 1234.0;
        sExt.component.dcOffset = 1.5;
        sExt.component.dutyCycle = 0.35;
        sExt.component.modFreqHz = 10.0;
        sExt.component.modIndex = 0.4;
        sExt.component.riseSec = 2e-6;
        sExt.component.fallSec = 3e-6;
        sExt.EnsurePinsFromTemplate();
        extDoc.symbols.push_back(sExt);
        const std::string extJson = SchematicToJson(extDoc);
        SchematicDocument extBack;
        CHECK(SchematicFromJson(extJson, extBack) && extBack.symbols.size() == 1);
        const auto& cBack = extBack.symbols[0].component;
        CHECK(cBack.nodeCtrlPos == 4 && cBack.nodeCtrlNeg == 5 && !cBack.active);
        CHECK(cBack.extraNodes.size() == 2 && cBack.extraNodes[0] == 7 && cBack.extraNodes[1] == 8);
        CHECK(std::abs(cBack.params.at("Ron") - 0.25) < 1e-9);
        CHECK(cBack.waveform == WaveformKind::Pulse && std::abs(cBack.freqHz - 1234.0) < 1e-6);
        CHECK(std::abs(cBack.dcOffset - 1.5) < 1e-6 && std::abs(cBack.dutyCycle - 0.35) < 1e-6);
        SchematicDocument badNumDoc;
        CHECK(!SchematicFromJson("{\"format\":\"docstudio_schematic_v1\",\"gridPitch\":1e+}", badNumDoc));

        // Inactive symbol alignment in BuildSchematicOverlay & ExportSchematicToDxf
        SchematicDocument ovDoc;
        SchematicSymbol syOff; syOff.id = 1; syOff.component = MakeComp(ComponentKind::Resistor, "R_OFF", 1, 0, 0, 100.0); syOff.component.active = false; syOff.EnsurePinsFromTemplate();
        SchematicSymbol syV; syV.id = 2; syV.component = MakeComp(ComponentKind::VoltageSource, "V1", 1, 0, 0, 10.0); syV.EnsurePinsFromTemplate();
        SchematicSymbol syR; syR.id = 3; syR.component = MakeComp(ComponentKind::Resistor, "R_ON", 1, 0, 0, 1000.0); syR.EnsurePinsFromTemplate();
        ovDoc.symbols = { syOff, syV, syR };
        const auto ovSol = SolveMnaCircuit({ syOff.component, syV.component, syR.component }, 0.0);
        const auto ovRes = BuildSchematicOverlay(ovDoc, ovSol);
        CHECK(ovRes.currentArrows.size() == 2 && ovRes.currentArrows[0].componentName == "V1" && ovRes.currentArrows[1].componentName == "R_ON");
    }

    // C4. Exporters (KiCad multi-pin + 1-pin GND/VCC, SPICE .cir roundtrip, DXF normalized ARC, FET geometry)
    {
        std::vector<CircuitComponent> exComps;
        CHECK(ParseNetlistText("V1 1 0 10; POT P1 1 0 2 10k 0.4; OPAMP U1 2 3 3; R1 3 0 1k; GND G1 0", exComps));
        const std::string kicad = ExportCircuitToKiCadSExpr(exComps, "Test\"Quote");
        CHECK(kicad.find("\\\"") != std::string::npos);
        CHECK(kicad.find("(node (ref \"P1\") (pin \"3\"))") != std::string::npos);
        CHECK(kicad.find("(node (ref \"G1\") (pin \"2\"))") == std::string::npos);

        const std::string cir = ExportCircuitToSpiceCir(exComps, "RoundTrip");
        std::vector<CircuitComponent> rtComps;
        CHECK(ParseNetlistText(cir, rtComps));
        const auto rtSol = SolveMnaCircuit(rtComps, 0.0);
        CHECK(rtSol.ok && std::abs(rtSol.nodeVoltages.at(3).real() - 4.0) < 1e-3);

        const auto gMn = GetComponentSymbolGeometry(ComponentKind::MosfetN);
        const auto gMp = GetComponentSymbolGeometry(ComponentKind::MosfetP);
        const auto gJn = GetComponentSymbolGeometry(ComponentKind::JfetN);
        const auto gJp = GetComponentSymbolGeometry(ComponentKind::JfetP);
        CHECK(gMn.polys[0].pts[0].x != gMp.polys[0].pts[0].x);
        CHECK(gJn.lines.size() != gMn.lines.size());
        CHECK(gJn.polys[0].pts[0].x != gJp.polys[0].pts[0].x);
    }

    // C5. MiniMcuVm: pinMode in setup() on Input binding, for, while (guarded), else if, pin var, parens, 1e-3, 10*-5
    {
        std::vector<CircuitComponent> load;
        CHECK(ParseNetlistText("R1 1 0 1k", load));
        const std::string code =
            "const int ledPin = 13;\n"
            "void setup() {\n"
            "  pinMode(ledPin, OUTPUT);\n"
            "  int a = (2 + 3) * 4;\n"
            "  double s = 1e-3;\n"
            "  int neg = 10 * -5;\n"
            "  Serial.println(a);\n"
            "  Serial.println(s);\n"
            "  Serial.println(neg);\n"
            "  for (int i = 0; i < 3; i++) { Serial.println(i); }\n"
            "  if (a == 99) { Serial.println(100); }\n"
            "  else if (a == 20) { Serial.println(200); }\n"
            "  else { Serial.println(300); }\n"
            "}\n"
            "void loop() {\n"
            "  while (1) {\n"
            "    digitalWrite(ledPin, HIGH);\n"
            "    delay(5);\n"
            "    digitalWrite(ledPin, LOW);\n"
            "    delay(5);\n"
            "  }\n"
            "}\n";
        const std::vector<McuPinBinding> pins{ { "D13", 1, McuPinMode::Input } };
        const auto mcu = RunMcuCircuitCoSimulation(load, code, McuBoardKind::ArduinoUno, pins, 0.02, 200);
        CHECK(mcu.ok);
        CHECK(mcu.circuitWaveforms.vMax > 4.5 && mcu.circuitWaveforms.vMin < 0.5);
        CHECK(mcu.serialLog.size() == 7);
        if (mcu.serialLog.size() == 7) {
            CHECK(mcu.serialLog[0] == "20");
            CHECK(std::abs(std::stod(mcu.serialLog[1]) - 0.001) < 1e-9);
            CHECK(mcu.serialLog[2] == "-50");
            CHECK(mcu.serialLog[3] == "0" && mcu.serialLog[4] == "1" && mcu.serialLog[5] == "2");
            CHECK(mcu.serialLog[6] == "200");
        }

        // Infinite while(1) with NO delay must terminate cleanly via step cap
        const std::string tightWhile = "void setup() { pinMode(D13, OUTPUT); } void loop() { while (1) { digitalWrite(D13, HIGH); } }";
        const auto tightRes = RunMcuCircuitCoSimulation(load, tightWhile, McuBoardKind::ArduinoUno, pins, 0.005, 50);
        CHECK(tightRes.ok && tightRes.circuitWaveforms.vMax > 4.5);
    }

    // C6. Flowchart infinite cycle without End detection, XML escaping & Control Saturation clamp
    {
        FlowchartDiagram cycFc;
        cycFc.nodes = {
            { 1, FlowNodeKind::StartEnd, "Start <A&B>", { 100.0, 50.0 } },
            { 2, FlowNodeKind::Process, "LoopA", { 100.0, 150.0 } },
            { 3, FlowNodeKind::Process, "LoopB", { 100.0, 250.0 } }
        };
        cycFc.edges = {
            { 1, 1, 2, "x<5&y>0", {} },
            { 2, 2, 3, "", {} },
            { 3, 3, 2, "", {} }
        };
        const auto cycDiags = ValidateFlowchart(cycFc);
        bool sawInfCycle = false;
        for (const auto& d : cycDiags) {
            if (d.isError && d.messageUtf8.find("Ciclo infinito") != std::string::npos) sawInfCycle = true;
        }
        CHECK(sawInfCycle);
        const std::string svg = ExportFlowchartToSvg(cycFc);
        CHECK(svg.find("Start &lt;A&amp;B&gt;") != std::string::npos);
        CHECK(svg.find("x&lt;5&amp;y&gt;0") != std::string::npos);

        ControlBlockDiagram satDiag;
        satDiag.nodes = {
            { 1, ControlNodeKind::Input, "U" },
            { 2, ControlNodeKind::Gain, "K", {}, 96, 52, { Poly1D{ { 1.0 } }, Poly1D{ { 1.0 } } }, 10.0 },
            { 3, ControlNodeKind::Saturation, "Sat", {}, 96, 52, { Poly1D{ { 1.0 } }, Poly1D{ { 1.0 } } }, 1.0, 1.0, 0.0, 0.0, -2.0, 2.0 },
            { 4, ControlNodeKind::Output, "Y" }
        };
        satDiag.edges = { { 1, 1, 2, 1, {} }, { 2, 2, 3, 1, {} }, { 3, 3, 4, 1, {} } };
        const auto satRes = AnalyzeControlBlockDiagram(satDiag, 1.0, 50);
        CHECK(satRes.ok && !satRes.stepOutput.empty() && satRes.stepOutput.back() <= 2.0 + 1e-9);
    }

    // Negative error-path tests
    {
        CHECK(!SolveMnaNetlist("V1 1 0 5; V2 1 0 10", 0.0).ok);
        CHECK(!AnalyzeBodeSweep({}, 1, 1.0, 100.0).ok);
        CHECK(!SimulateDcSweepMna({}, "V1", 0.0, 5.0, 10, 1).ok);
        ControlBlockDiagram badCtrl;
        badCtrl.nodes = { { 1, ControlNodeKind::Gain, "K" } };
        CHECK(!AnalyzeControlBlockDiagram(badCtrl).ok);
    }
}

void TestIteration3Regressions() {
    // 1. Lossless LC tank (10 mH, 10 uF) at dt = 1/2400 s, 100 us, 50 us (with and without ToggleSwitch)
    {
        for (double dt : { 1.0 / 2400.0, 100e-6, 50e-6 }) {
            std::vector<CircuitComponent> lcSw;
            CHECK(ParseNetlistText("V1 1 0 5; SW SW1 1 2 1; R1 2 3 1; L1 3 0 10m; C1 3 0 10u", lcSw));
            CircuitTransientSession sess;
            CHECK(sess.Initialize(lcSw, dt, 4096, true));
            OscilloscopeFrame fr{};
            CHECK(sess.StepFrame(0.002, fr));
            auto calcEnergy = [&](const OscilloscopeFrame& f, std::size_t idx) -> double {
                const double vc = f.channelRing.at("V(3)")[idx];
                const double il = f.channelRing.at("I(L1)")[idx];
                return 0.5 * 10e-6 * vc * vc + 0.5 * 10e-3 * il * il;
            };
            const double eBefore = calcEnergy(fr, fr.timeRingSec.size() - 1);
            CHECK(eBefore > 1e-5);
            const std::size_t idxPre = fr.timeRingSec.size();

            CHECK(sess.ToggleSwitch("SW1", false));
            CHECK(sess.StepFrame(0.020, fr));
            const double ePostSwitch = calcEnergy(fr, idxPre);
            const double eEnd = calcEnergy(fr, fr.timeRingSec.size() - 1);
            CHECK(eEnd / eBefore > 0.99);
            CHECK(eEnd / ePostSwitch > 0.99);
        }

        // Lossless LC tank without ToggleSwitch at default dt = 1/2400 s
        CircuitComponent iKick = MakeComp(ComponentKind::CurrentSource, "I1", 0, 2, 0, 0.25);
        CircuitComponent cTank = MakeComp(ComponentKind::Capacitor, "C1", 2, 0, 0, 10e-6);
        CircuitComponent lTank = MakeComp(ComponentKind::Inductor, "L1", 2, 0, 0, 10e-3);
        CircuitTransientSession lcNoSw;
        CHECK(lcNoSw.Initialize({ iKick, cTank, lTank }, 1.0 / 2400.0, 2048, true));
        OscilloscopeFrame fr0{};
        CHECK(lcNoSw.StepFrame(0.025, fr0));
        const auto& vcArr = fr0.channelRing.at("V(2)");
        const auto& ilArr = fr0.channelRing.at("I(L1)");
        double maxV1 = 0.0, maxV2 = 0.0, maxE1 = 0.0, maxE2 = 0.0;
        const std::size_t half = vcArr.size() / 2;
        for (std::size_t k = 0; k < vcArr.size(); ++k) {
            const double eK = 0.5 * 10e-6 * vcArr[k] * vcArr[k] + 0.5 * 10e-3 * ilArr[k] * ilArr[k];
            if (k < half) { maxV1 = (std::max)(maxV1, std::abs(vcArr[k])); maxE1 = (std::max)(maxE1, eK); }
            else { maxV2 = (std::max)(maxV2, std::abs(vcArr[k])); maxE2 = (std::max)(maxE2, eK); }
        }
        CHECK(maxV1 > 5.0 && maxV2 / maxV1 > 0.99 && maxE2 / maxE1 > 0.99);
    }

    // 2. Stiff RC across dt = 210 us, 250 us, 1/2400 s, 1000 us (plain & SW+AM): maxVc <= 5.0 & monotonic
    {
        for (const char* net : { "V1 1 0 5; R1 1 2 10; C1 2 0 10u",
                                 "V1 1 0 5; SW SW1 1 3 1; AM Am1 3 4; R1 4 2 10; C1 2 0 10u" }) {
            for (double dt : { 210e-6, 250e-6, 1.0 / 2400.0, 1000e-6 }) {
                std::vector<CircuitComponent> comps;
                CHECK(ParseNetlistText(net, comps));
                CircuitTransientSession sess;
                CHECK(sess.Initialize(comps, dt, 2048, true));
                OscilloscopeFrame fr{};
                CHECK(sess.StepFrame(0.01, fr));
                const auto& vc = fr.channelRing.at("V(2)");
                CHECK(!vc.empty() && fr.vMax.at("V(2)") <= 5.0);
                bool mono = true;
                for (std::size_t k = 1; k < vc.size(); ++k) {
                    if (vc[k] < vc[k - 1] - 1e-12) { mono = false; break; }
                }
                CHECK(mono);
            }
        }
    }

    // 3. Stiff RL (V=5V, R=10, L=100uH, dt=1/2400 s): monotonic, no overshoot
    {
        std::vector<CircuitComponent> rlComps;
        CHECK(ParseNetlistText("V1 1 0 5; R1 1 2 10; L1 2 0 100u", rlComps));
        CircuitTransientSession rlSess;
        CHECK(rlSess.Initialize(rlComps, 1.0 / 2400.0, 2048, true));
        OscilloscopeFrame fr{};
        CHECK(rlSess.StepFrame(0.01, fr));
        const auto& il = fr.channelRing.at("I(L1)");
        CHECK(!il.empty() && fr.vMax.at("I(L1)") <= 0.500001 && fr.vMin.at("I(L1)") >= 0.0);
        bool mono = true;
        for (std::size_t k = 1; k < il.size(); ++k) {
            if (il[k] < il[k - 1] - 1e-12) { mono = false; break; }
        }
        CHECK(mono);
    }

    // 4. Mixed circuit (LC tank + stiff RC in same session) & Nonlinear Diode + Stiff RC
    {
        std::vector<CircuitComponent> mix;
        CHECK(ParseNetlistText(
            "I1 0 1 0.25; C_lc 1 0 10u; L_lc 1 0 10m\n"
            "V2 2 0 5; R_rc 2 3 10; C_rc 3 0 10u\n", mix));
        CircuitTransientSession mixSess;
        CHECK(mixSess.Initialize(mix, 1.0 / 2400.0, 2048, true));
        OscilloscopeFrame fr{};
        CHECK(mixSess.StepFrame(0.02, fr));
        const auto& vcMix = fr.channelRing.at("V(1)");
        const auto& ilMix = fr.channelRing.at("I(L_lc)");
        double e1Max = 0.0, e2Max = 0.0;
        for (std::size_t k = 0; k < vcMix.size(); ++k) {
            const double eK = 0.5 * 10e-6 * vcMix[k] * vcMix[k] + 0.5 * 10e-3 * ilMix[k] * ilMix[k];
            if (k < vcMix.size() / 2) e1Max = (std::max)(e1Max, eK);
            else e2Max = (std::max)(e2Max, eK);
        }
        CHECK(e1Max > 1e-4 && e2Max / e1Max > 0.99);
        CHECK(fr.vMax.at("V(3)") <= 5.0);

        // Nonlinear Diode + Stiff RC
        std::vector<CircuitComponent> dRc;
        CHECK(ParseNetlistText("V1 1 0 10; D1 1 2; R1 2 3 10; C1 3 0 10u; RL 3 0 100k", dRc));
        CircuitTransientSession dSess;
        CHECK(dSess.Initialize(dRc, 1.0 / 2400.0, 2048, true));
        OscilloscopeFrame dFr{};
        CHECK(dSess.StepFrame(0.01, dFr));
        CHECK(dFr.vMax.at("V(3)") <= 10.0);
    }

    // 5. DecodeSmdResistorCode validation (reject bare letters/malformed, keep valid)
    {
        CHECK(!DecodeSmdResistorCode("R").ok);
        CHECK(!DecodeSmdResistorCode("K").ok);
        CHECK(!DecodeSmdResistorCode("M").ok);
        CHECK(!DecodeSmdResistorCode("RK").ok);
        CHECK(!DecodeSmdResistorCode("4R7K").ok);
        CHECK(!DecodeSmdResistorCode("4R7E2").ok);
        CHECK(DecodeSmdResistorCode("4R7").ok && std::abs(DecodeSmdResistorCode("4R7").ohms - 4.7) < 1e-9);
        CHECK(DecodeSmdResistorCode("R22").ok && std::abs(DecodeSmdResistorCode("R22").ohms - 0.22) < 1e-9);
        CHECK(DecodeSmdResistorCode("2K2").ok && std::abs(DecodeSmdResistorCode("2K2").ohms - 2200.0) < 1e-9);
        CHECK(DecodeSmdResistorCode("1M5").ok && std::abs(DecodeSmdResistorCode("1M5").ohms - 1.5e6) < 1e-9);
        CHECK(DecodeSmdResistorCode("10R").ok && std::abs(DecodeSmdResistorCode("10R").ohms - 10.0) < 1e-9);
        CHECK(DecodeSmdResistorCode("10K").ok && std::abs(DecodeSmdResistorCode("10K").ohms - 10000.0) < 1e-9);
        CHECK(DecodeSmdResistorCode("1M").ok && std::abs(DecodeSmdResistorCode("1M").ohms - 1e6) < 1e-9);
    }

    // 6. SPICE .cir Export / ParseNetlistText Round-Trip (models, polarities, numeric tokens, all 58 kinds)
    {
        // Verify Q1 2 3 4 2N2907 parses specifically as BjtPnp
        std::vector<CircuitComponent> pnpParse;
        CHECK(ParseNetlistText("Q1 2 3 4 2N2907", pnpParse));
        CHECK(pnpParse.size() == 1 && pnpParse[0].kind == ComponentKind::BjtPnp && pnpParse[0].modelName == "2N2907");

        // Export -> Parse round-trip for BjtPnp, MosfetP, JfetP, VoltageRegulator, Diodes, Led, Ic, PulseSource
        std::vector<CircuitComponent> semi;
        semi.push_back(MakeComp(ComponentKind::VoltageSource, "V1", 1, 0, 0, 15.0));
        semi.push_back(MakeComp(ComponentKind::BjtPnp, "Q1", 2, 1, 3, 0.0, 0.0, ""));
        semi.push_back(MakeComp(ComponentKind::MosfetP, "M1", 4, 1, 5, 0.0, 0.0, ""));
        semi.push_back(MakeComp(ComponentKind::JfetP, "J1", 6, 1, 7, 0.0, 0.0, "2N5460"));
        semi.push_back(MakeComp(ComponentKind::VoltageRegulator, "U1", 1, 0, 8, 5.0, 0.0, "7812"));
        semi.push_back(MakeComp(ComponentKind::Ic, "U2", 1, 0, 9, 5.0, 0.0, "LM317"));
        semi.push_back(MakeComp(ComponentKind::Led, "L1", 1, 10, 0, 0.0, 0.0, ""));
        semi.push_back(MakeComp(ComponentKind::SchottkyDiode, "D1", 1, 11, 0, 0.0, 0.0, ""));
        semi.push_back(MakeComp(ComponentKind::Diode, "D2", 1, 12, 0, 0.0, 0.0, "1N4148"));
        CircuitComponent vp = MakeComp(ComponentKind::PulseSource, "Vp1", 13, 0, 0, 5.0);
        vp.waveform = WaveformKind::Pulse; vp.freqHz = 100.0; vp.dutyCycle = 0.25; vp.dcOffset = 1.0;
        semi.push_back(vp);
        semi.push_back(MakeComp(ComponentKind::PolarizedCapacitor, "Cpol1", 14, 0, 0, 47e-6));

        const std::string semiCir = ExportCircuitToSpiceCir(semi, "SemiRT");
        std::vector<CircuitComponent> semiBack;
        CHECK(ParseNetlistText(semiCir, semiBack));
        CHECK(semiBack.size() == semi.size());
        if (semiBack.size() == semi.size()) {
            for (std::size_t i = 0; i < semi.size(); ++i) {
                CHECK(semiBack[i].kind == semi[i].kind);
            }
            CHECK(semiBack[4].modelName == "7812");
            CHECK(semiBack[5].modelName == "LM317");
            CHECK(semiBack[9].waveform == WaveformKind::Pulse && std::abs(semiBack[9].freqHz - 100.0) < 1e-6);
            CHECK(std::abs(semiBack[9].dutyCycle - 0.25) < 1e-6 && std::abs(semiBack[9].dcOffset - 1.0) < 1e-6);
        }

        // Verify 7812 regulator regulates at 12V and Led keeps ~1.8V forward drop after .cir round-trip
        const auto reg12 = SolveMnaNetlist("V1 1 0 16; REG U1 1 0 2 7812; RL 2 0 100", 0.0);
        CHECK(reg12.ok && std::abs(reg12.nodeVoltages.at(2).real() - 12.0) < 0.1);

        const std::string ledCir = ExportCircuitToSpiceCir({
            MakeComp(ComponentKind::VoltageSource, "V1", 1, 0, 0, 5.0),
            MakeComp(ComponentKind::Led, "L1", 1, 2, 0, 0.0),
            MakeComp(ComponentKind::Resistor, "R1", 2, 0, 0, 1000.0)
        });
        std::vector<CircuitComponent> ledRt;
        CHECK(ParseNetlistText(ledCir, ledRt));
        const auto ledSol = SolveMnaCircuit(ledRt, 0.0);
        CHECK(ledSol.ok);
        const double vdLed = 5.0 - ledSol.nodeVoltages.at(2).real();
        CHECK(vdLed > 1.4 && vdLed < 2.4);

        // Custom .model card (below component line) + numeric-leading models (2N2907, 2N3906, 2N7000, J174, D1N5819)
        std::vector<CircuitComponent> customMod;
        CHECK(ParseNetlistText(
            "Q1 2 3 4 MY_PNP\n"
            "M1 5 6 0 0 MY_PMOS\n"
            "J1 7 8 9 J2N5460\n"
            "J2 7 8 9 J174\n"
            "DZ1 10 0 DZ_CUSTOM\n"
            ".model MY_PNP PNP(Bf=150 Vaf=80)\n"
            ".model MY_PMOS PMOS(Vto=-3.0 Kp=4.0)\n"
            ".model DZ_CUSTOM D(Bv=7.5 Ibv=1e-3)\n", customMod));
        CHECK(customMod.size() == 5);
        if (customMod.size() == 5) {
            CHECK(customMod[0].kind == ComponentKind::BjtPnp && std::abs(customMod[0].params.at("Bf") - 150.0) < 1e-6);
            CHECK(customMod[1].kind == ComponentKind::MosfetP && std::abs(customMod[1].params.at("Kp") - 4.0) < 1e-6);
            CHECK(customMod[2].kind == ComponentKind::JfetP);
            CHECK(customMod[3].kind == ComponentKind::JfetP);
            CHECK(customMod[4].kind == ComponentKind::ZenerDiode && std::abs(customMod[4].value - 7.5) < 1e-6);
        }

        // Full 58-ComponentKind (0..57, excluding Ground=16 which SPICE omits) .cir round-trip
        std::vector<CircuitComponent> all57;
        auto defaultNameFor = [](ComponentKind k, int idx) -> std::string {
            const std::string s = std::to_string(idx);
            switch (k) {
            case ComponentKind::Resistor: return "R" + s;
            case ComponentKind::Capacitor: return "C" + s;
            case ComponentKind::Inductor: return "L" + s;
            case ComponentKind::VoltageSource: return "V" + s;
            case ComponentKind::CurrentSource: return "I" + s;
            case ComponentKind::Diode: return "D" + s;
            case ComponentKind::BjtNpn:
            case ComponentKind::BjtPnp: return "Q" + s;
            case ComponentKind::MosfetN:
            case ComponentKind::MosfetP: return "M" + s;
            case ComponentKind::JfetN:
            case ComponentKind::JfetP: return "J" + s;
            case ComponentKind::Vcvs: return "E" + s;
            case ComponentKind::Vccs: return "G" + s;
            default: return "K" + s;
            }
        };
        for (int raw = 0; raw <= 57; ++raw) {
            const auto k = static_cast<ComponentKind>(raw);
            if (k == ComponentKind::Ground) continue;
            CircuitComponent c = MakeComp(k, defaultNameFor(k, raw), 1, 2, 3, 1.0);
            c.nodeCtrlPos = 4;
            c.nodeCtrlNeg = 5;
            c.auxValue = 1.0;
            if (k == ComponentKind::ZenerDiode) c.value = 5.1;
            if (k == ComponentKind::PulseSource) { c.waveform = WaveformKind::Pulse; c.freqHz = 50.0; }
            all57.push_back(c);
        }
        const std::string allCir = ExportCircuitToSpiceCir(all57, "All58");
        std::vector<CircuitComponent> allBack;
        CHECK(ParseNetlistText(allCir, allBack));
        CHECK(allBack.size() == all57.size());
        if (allBack.size() == all57.size()) {
            for (std::size_t i = 0; i < all57.size(); ++i) {
                CHECK(allBack[i].kind == all57[i].kind);
            }
        }
    }

    // 7. ControlBlockDiagram internal & closed-loop Saturation (G7.1, G7.2, G7.2b, algebraic loop, PID+Sat)
    {
        // G7.1 Open-loop saturation
        ControlBlockDiagram g71;
        g71.nodes = {
            { 1, ControlNodeKind::Input, "U" },
            { 2, ControlNodeKind::Gain, "K", {}, 96, 52, { { { 1.0 } }, { { 1.0 } } }, 10.0 },
            { 3, ControlNodeKind::Saturation, "Sat", {}, 96, 52, { { { 1.0 } }, { { 1.0 } } }, 1.0, 1.0, 0.0, 0.0, -2.0, 2.0 },
            { 4, ControlNodeKind::Output, "Y" }
        };
        g71.edges = { { 1, 1, 2, 1, {} }, { 2, 2, 3, 1, {} }, { 3, 3, 4, 1, {} } };
        const auto r71 = AnalyzeControlBlockDiagram(g71, 1.0, 50);
        CHECK(r71.ok && !r71.stepOutput.empty() && std::abs(r71.stepOutput.back() - 2.0) < 1e-6);

        // G7.2 Pre-plant Saturation: Input -> Gain(10) -> Sat[-2,2] -> 0.5/(s+1) -> Output
        ControlBlockDiagram g72;
        g72.nodes = {
            { 1, ControlNodeKind::Input, "U" },
            { 2, ControlNodeKind::Gain, "K", {}, 96, 52, { { { 1.0 } }, { { 1.0 } } }, 10.0 },
            { 3, ControlNodeKind::Saturation, "Sat", {}, 96, 52, { { { 1.0 } }, { { 1.0 } } }, 1.0, 1.0, 0.0, 0.0, -2.0, 2.0 },
            { 4, ControlNodeKind::TransferFunction, "G", {}, 96, 52, { Poly1D{ { 0.5 } }, Poly1D{ { 1.0, 1.0 } } } },
            { 5, ControlNodeKind::Output, "Y" }
        };
        g72.edges = { { 1, 1, 2, 1, {} }, { 2, 2, 3, 1, {} }, { 3, 3, 4, 1, {} }, { 4, 4, 5, 1, {} } };
        const auto r72 = AnalyzeControlBlockDiagram(g72, 5.0, 200);
        CHECK(r72.ok && !r72.stepOutput.empty());
        CHECK(std::abs(r72.stepOutput.back() - (1.0 - std::exp(-5.0))) < 1e-4);

        // G7.2b Closed-loop actuator Saturation: Input -> Sum(+/-) -> Gain(20) -> Sat[-2,2] -> Integrator -> Output
        ControlBlockDiagram g72b;
        g72b.nodes = {
            { 1, ControlNodeKind::Input, "U" },
            { 2, ControlNodeKind::SumJunction, "Sum" },
            { 3, ControlNodeKind::Gain, "K", {}, 96, 52, { { { 1.0 } }, { { 1.0 } } }, 20.0 },
            { 4, ControlNodeKind::Saturation, "Sat", {}, 96, 52, { { { 1.0 } }, { { 1.0 } } }, 1.0, 1.0, 0.0, 0.0, -2.0, 2.0 },
            { 5, ControlNodeKind::Integrator, "Int" },
            { 6, ControlNodeKind::Output, "Y" }
        };
        g72b.edges = {
            { 1, 1, 2, 1, {} }, { 2, 2, 3, 1, {} }, { 3, 3, 4, 1, {} },
            { 4, 4, 5, 1, {} }, { 5, 5, 6, 1, {} }, { 6, 6, 2, -1, {} }
        };
        const auto r72b = AnalyzeControlBlockDiagram(g72b, 1.0, 200); // dt = 0.005 -> sample 50 is t = 0.25s
        CHECK(r72b.ok && r72b.stepOutput.size() > 50);
        CHECK(std::abs(r72b.stepOutput[50] - 0.50) < 1e-4);
        CHECK(std::abs(r72b.stepOutput.back() - 1.0) < 5e-4);

        // Algebraic loop with Saturation: Input -> Sum(+/-) -> Gain(2) -> Sat[-1,1] -> Gain(0.5) -> Output (-feedback)
        ControlBlockDiagram algSat;
        algSat.nodes = {
            { 1, ControlNodeKind::Input, "U" },
            { 2, ControlNodeKind::SumJunction, "Sum" },
            { 3, ControlNodeKind::Gain, "K2", {}, 96, 52, { { { 1.0 } }, { { 1.0 } } }, 2.0 },
            { 4, ControlNodeKind::Saturation, "Sat", {}, 96, 52, { { { 1.0 } }, { { 1.0 } } }, 1.0, 1.0, 0.0, 0.0, -1.0, 1.0 },
            { 5, ControlNodeKind::Gain, "K05", {}, 96, 52, { { { 1.0 } }, { { 1.0 } } }, 0.5 },
            { 6, ControlNodeKind::Output, "Y" }
        };
        algSat.edges = {
            { 1, 1, 2, 1, {} }, { 2, 2, 3, 1, {} }, { 3, 3, 4, 1, {} },
            { 4, 4, 5, 1, {} }, { 5, 5, 6, 1, {} }, { 6, 6, 2, -1, {} }
        };
        const auto rAlg = AnalyzeControlBlockDiagram(algSat, 1.0, 20);
        // Unsaturated: y = 0.5 * 2 * (1 - y) = 1 - y => y = 0.5, sat input = 2*(1 - 0.5) = 1.0 in [-1, 1]
        CHECK(rAlg.ok && !rAlg.stepOutput.empty() && std::abs(rAlg.stepOutput.back() - 0.5) < 1e-6);

        // PID + Saturation + Plant
        ControlBlockDiagram pidSat;
        pidSat.nodes = {
            { 1, ControlNodeKind::Input, "U" },
            { 2, ControlNodeKind::SumJunction, "Sum" },
            { 3, ControlNodeKind::Pid, "PID", {}, 96, 52, { { { 1.0 } }, { { 1.0 } } }, 1.0, 2.0, 5.0, 0.05 },
            { 4, ControlNodeKind::Saturation, "Sat", {}, 96, 52, { { { 1.0 } }, { { 1.0 } } }, 1.0, 1.0, 0.0, 0.0, -2.0, 2.0 },
            { 5, ControlNodeKind::TransferFunction, "Plant", {}, 96, 52, { Poly1D{ { 1.0 } }, Poly1D{ { 1.0, 1.0 } } } },
            { 6, ControlNodeKind::Output, "Y" }
        };
        pidSat.edges = {
            { 1, 1, 2, 1, {} }, { 2, 2, 3, 1, {} }, { 3, 3, 4, 1, {} },
            { 4, 4, 5, 1, {} }, { 5, 5, 6, 1, {} }, { 6, 6, 2, -1, {} }
        };
        const auto rPid = AnalyzeControlBlockDiagram(pidSat, 4.0, 200);
        CHECK(rPid.ok && !rPid.stepOutput.empty() && std::isfinite(rPid.stepOutput.back()));
        CHECK(rPid.stepOutput.back() > 0.8 && rPid.stepOutput.back() < 1.2);
    }

    // 8. ValidateFlowchart: Start -> Step -> Start (error) vs valid Decision -> End loop (0 errors)
    {
        FlowchartDiagram startLoop;
        startLoop.nodes = {
            { 1, FlowNodeKind::StartEnd, "Start", { 100.0, 50.0 } },
            { 2, FlowNodeKind::Process, "Step", { 100.0, 150.0 } }
        };
        startLoop.edges = {
            { 1, 1, 2, "", {} },
            { 2, 2, 1, "", {} }
        };
        const auto slDiags = ValidateFlowchart(startLoop);
        bool sawErr = false;
        for (const auto& d : slDiags) {
            if (d.isError && d.messageUtf8.find("Ciclo infinito") != std::string::npos) sawErr = true;
        }
        CHECK(sawErr);

        FlowchartDiagram validLoop;
        validLoop.nodes = {
            { 1, FlowNodeKind::StartEnd, "Start", { 100.0, 50.0 } },
            { 2, FlowNodeKind::Process, "Step", { 100.0, 150.0 } },
            { 3, FlowNodeKind::Decision, "Done?", { 100.0, 250.0 } },
            { 4, FlowNodeKind::StartEnd, "End", { 100.0, 350.0 } }
        };
        validLoop.edges = {
            { 1, 1, 2, "", {} },
            { 2, 2, 3, "", {} },
            { 3, 3, 2, "No", {} },
            { 4, 3, 4, "Yes", {} }
        };
        const auto vlDiags = ValidateFlowchart(validLoop);
        int vlErrCount = 0;
        for (const auto& d : vlDiags) {
            if (d.isError) ++vlErrCount;
        }
        CHECK(vlErrCount == 0);
    }

    // 9. Symbolic LaTeX formatting: top-level \frac{...}{...} and 10^{...} (no raw e-05 / e+06 ASCII dumps)
    {
        auto hasRawSci = [](const std::string& s) -> bool {
            for (std::size_t i = 0; i + 2 < s.size(); ++i) {
                if ((s[i] == 'e' || s[i] == 'E') && (s[i + 1] == '+' || s[i + 1] == '-') &&
                    std::isdigit(static_cast<unsigned char>(s[i + 2]))) {
                    return true;
                }
            }
            return false;
        };

        // Fast RC (R=1k, C=1n -> tau = 1e-6, state-space A = [[-1e6]], 1MHz omega0 = 6.283e6)
        std::vector<CircuitComponent> fastRc;
        CHECK(ParseNetlistText("V1 1 0 1; R1 1 2 1k; C1 2 0 1n", fastRc));
        const auto fastRep = AnalyzeFullSymbolicCircuit(fastRc, 2, 1e6);
        CHECK(fastRep.ok);
        CHECK(fastRep.transferFunctionLatex.find("\\frac{") != std::string::npos);
        CHECK(fastRep.transferFunctionLatex.find("10^{-6}") != std::string::npos);
        CHECK(!hasRawSci(fastRep.transferFunctionLatex));
        CHECK(fastRep.outputResponseSLatex.find("\\frac{") != std::string::npos);
        CHECK(!hasRawSci(fastRep.outputResponseSLatex));
        CHECK(fastRep.symbolicHOfSLatex.find("\\frac{") != std::string::npos);
        CHECK(!hasRawSci(fastRep.symbolicHOfSLatex));
        CHECK(fastRep.mnaMatrixLatex.find("10^{-9}") != std::string::npos);
        CHECK(!hasRawSci(fastRep.mnaMatrixLatex));
        CHECK(fastRep.stateSpace.equationsLatex.find("10^{6}") != std::string::npos);
        CHECK(!hasRawSci(fastRep.stateSpace.equationsLatex));
        CHECK(fastRep.acPhasorMatrixLatex.find("10^{6}") != std::string::npos);
        CHECK(!hasRawSci(fastRep.acPhasorMatrixLatex));
        for (const auto& ph : fastRep.acNodePhasorsLatex) CHECK(!hasRawSci(ph));

        // RLC (L=1uH, C=1uF -> LC = 1e-12)
        const auto rlcLap = AnalyzeSymbolicLaplaceNetlist("V1 1 0 1; R1 1 2 10; L1 2 3 1u; C1 3 0 1u", 3);
        CHECK(rlcLap.ok && rlcLap.transferFunctionLatex.find("\\frac{") != std::string::npos);
        CHECK(rlcLap.transferFunctionLatex.find("10^{-12}") != std::string::npos);
        CHECK(!hasRawSci(rlcLap.transferFunctionLatex));

        // Legacy plain UTF-8 TestF11 string stays intact
        const auto legacyLap = AnalyzeSymbolicLaplaceNetlist("V1 1 0 1; R1 1 2 1k; C1 2 0 1u", 2);
        CHECK(legacyLap.ok && legacyLap.transferFunctionUtf8.find("0.001*s") != std::string::npos);
        CHECK(legacyLap.transferFunctionLatex.find("\\frac{") != std::string::npos);
        CHECK(!hasRawSci(legacyLap.transferFunctionLatex));

        // ControlBlockDiagram closedLoopLatex & masonFormulaLatex with 1e-5 coefficient
        ControlBlockDiagram smallCtrl;
        smallCtrl.nodes = {
            { 1, ControlNodeKind::Input, "U" },
            { 2, ControlNodeKind::TransferFunction, "G", {}, 96, 52, { Poly1D{ { 1.0 } }, Poly1D{ { 1.0, 1e-5 } } } },
            { 3, ControlNodeKind::Output, "Y" }
        };
        smallCtrl.edges = { { 1, 1, 2, 1, {} }, { 2, 2, 3, 1, {} } };
        const auto scRes = AnalyzeControlBlockDiagram(smallCtrl, 1.0, 20);
        CHECK(scRes.ok);
        CHECK(scRes.closedLoopLatex.find("\\frac{") != std::string::npos);
        CHECK(scRes.closedLoopLatex.find("10^{-5}") != std::string::npos);
        CHECK(!hasRawSci(scRes.closedLoopLatex));
        CHECK(!hasRawSci(scRes.masonFormulaLatex));
    }
}

void TestIteration4Regressions() {
    // 1. ControlBlockDiagram active-set Saturation exiting +1 into linear region in high-gain algebraic loop (K=1000)
    {
        ControlBlockDiagram diag;
        diag.nodes = {
            { 1, ControlNodeKind::Input, "U" },
            { 2, ControlNodeKind::TransferFunction, "LeadLag", {}, 96, 52, { Poly1D{ { 1.0, 10.0 } }, Poly1D{ { 1.0, 1.0 } } } },
            { 3, ControlNodeKind::SumJunction, "Sum" },
            { 4, ControlNodeKind::Gain, "K1000", {}, 96, 52, { { { 1.0 } }, { { 1.0 } } }, 1000.0 },
            { 5, ControlNodeKind::Saturation, "Sat", {}, 96, 52, { { { 1.0 } }, { { 1.0 } } }, 1.0, 1.0, 0.0, 0.0, -2.0, 2.0 },
            { 6, ControlNodeKind::Output, "Y" }
        };
        diag.edges = {
            { 1, 1, 2, 1, {} },
            { 2, 2, 3, 1, {} },
            { 3, 3, 4, 1, {} },
            { 4, 4, 5, 1, {} },
            { 5, 5, 6, 1, {} },
            { 6, 5, 3, -1, {} }
        };
        const auto res = AnalyzeControlBlockDiagram(diag, 5.0, 200);
        CHECK(res.ok && !res.stepOutput.empty());
        // Exact analytical value at t=5s: (1000/1001) * (1 + 9*exp(-5)) = 1.059582
        CHECK(std::abs(res.stepOutput.back() - 1.059582) < 1e-4);

        // Multi-saturation high-gain loop exiting saturation: K=500 -> Sat1[-2,2] -> K=2 -> Sat2[-3,3]
        ControlBlockDiagram dualSat;
        dualSat.nodes = {
            { 1, ControlNodeKind::Input, "U" },
            { 2, ControlNodeKind::TransferFunction, "LeadLag", {}, 96, 52, { Poly1D{ { 1.0, 10.0 } }, Poly1D{ { 1.0, 1.0 } } } },
            { 3, ControlNodeKind::SumJunction, "Sum" },
            { 4, ControlNodeKind::Gain, "K500", {}, 96, 52, { { { 1.0 } }, { { 1.0 } } }, 500.0 },
            { 5, ControlNodeKind::Saturation, "Sat1", {}, 96, 52, { { { 1.0 } }, { { 1.0 } } }, 1.0, 1.0, 0.0, 0.0, -2.0, 2.0 },
            { 6, ControlNodeKind::Gain, "K2", {}, 96, 52, { { { 1.0 } }, { { 1.0 } } }, 2.0 },
            { 7, ControlNodeKind::Saturation, "Sat2", {}, 96, 52, { { { 1.0 } }, { { 1.0 } } }, 1.0, 1.0, 0.0, 0.0, -3.0, 3.0 },
            { 8, ControlNodeKind::Output, "Y" }
        };
        dualSat.edges = {
            { 1, 1, 2, 1, {} },
            { 2, 2, 3, 1, {} },
            { 3, 3, 4, 1, {} },
            { 4, 4, 5, 1, {} },
            { 5, 5, 6, 1, {} },
            { 6, 6, 7, 1, {} },
            { 7, 7, 8, 1, {} },
            { 8, 7, 3, -1, {} }
        };
        const auto resDual = AnalyzeControlBlockDiagram(dualSat, 5.0, 200);
        CHECK(resDual.ok && !resDual.stepOutput.empty());
        CHECK(std::abs(resDual.stepOutput.back() - 1.059582) < 1e-4);
    }

    // 2. Empty-model & explicit-model Diode / SchottkyDiode / Led / Zener / Photodiode .cir DC round-trip
    {
        auto checkDiodeDcRt = [&](ComponentKind kind, const std::string& model, double val) {
            std::vector<CircuitComponent> orig = {
                MakeComp(ComponentKind::VoltageSource, "V1", 1, 0, 0, 5.0),
                MakeComp(kind, "D1", 1, 2, 0, val, 0.0, model),
                MakeComp(ComponentKind::Resistor, "R1", 2, 0, 0, 1000.0)
            };
            const auto sol1 = SolveMnaCircuit(orig, 0.0);
            const std::string cir = ExportCircuitToSpiceCir(orig, "DiodeDcRt");
            std::vector<CircuitComponent> back;
            const bool pok = ParseNetlistText(cir, back);
            const auto sol2 = SolveMnaCircuit(back, 0.0);
            CHECK(sol1.ok && pok && sol2.ok);
            CHECK(back.size() == orig.size());
            if (back.size() == orig.size()) {
                CHECK(back[1].kind == kind);
            }
            double maxVdiff = 0.0;
            for (const auto& kv : sol1.nodeVoltages) {
                const auto it = sol2.nodeVoltages.find(kv.first);
                CHECK(it != sol2.nodeVoltages.end());
                if (it != sol2.nodeVoltages.end()) {
                    maxVdiff = (std::max)(maxVdiff, std::abs(kv.second.real() - it->second.real()));
                }
            }
            CHECK(maxVdiff < 1e-9);
        };

        checkDiodeDcRt(ComponentKind::Diode, "", 1000.0);
        checkDiodeDcRt(ComponentKind::SchottkyDiode, "", 1000.0);
        checkDiodeDcRt(ComponentKind::Diode, "D1N4148", 0.0);
        checkDiodeDcRt(ComponentKind::Diode, "1N4148", 0.0);
        checkDiodeDcRt(ComponentKind::Diode, "D1N4007", 0.0);
        checkDiodeDcRt(ComponentKind::SchottkyDiode, "D1N5819", 0.0);
        checkDiodeDcRt(ComponentKind::SchottkyDiode, "1N5819", 0.0);
        checkDiodeDcRt(ComponentKind::Led, "", 0.0);
        checkDiodeDcRt(ComponentKind::Led, "BLUE", 0.0);
        checkDiodeDcRt(ComponentKind::ZenerDiode, "", 5.1);
        checkDiodeDcRt(ComponentKind::Photodiode, "", 1000.0);
    }

    // 3. acNodePhasorsLatex zero double-superscripts, tiny-angle parens, and meshKvlEquationsLatex \text{...}
    {
        auto hasDoubleSup = [](const std::string& s) -> bool {
            for (std::size_t i = 0; i + 2 < s.size(); ++i) {
                if (s[i] == '^' && s[i + 1] == '{') {
                    std::size_t j = i + 2;
                    int depth = 1;
                    while (j < s.size() && depth > 0) {
                        if (s[j] == '{') ++depth;
                        else if (s[j] == '}') --depth;
                        ++j;
                    }
                    if (depth == 0) {
                        while (j < s.size() && std::isspace(static_cast<unsigned char>(s[j]))) ++j;
                        if (j < s.size() && s[j] == '^') return true;
                    }
                }
            }
            return false;
        };
        auto checkLatexField = [&](const std::string& s) {
            CHECK(!hasDoubleSup(s));
            for (std::size_t i = 0; i + 2 < s.size(); ++i) {
                if ((s[i] == 'e' || s[i] == 'E') && (s[i + 1] == '+' || s[i + 1] == '-') &&
                    std::isdigit(static_cast<unsigned char>(s[i + 2]))) {
                    CHECK(false);
                }
            }
        };

        std::vector<CircuitComponent> multiComps;
        CHECK(ParseNetlistText(
            "V1 1 0 12\nI1 0 2 0.00002\nR1 1 2 10k\nC1 2 0 10n\n"
            "L1 2 3 1m\nR2 3 0 1k\nSW S1 1 2 1\nOPAMP U1 3 4 4", multiComps));
        const auto multiRep = AnalyzeFullSymbolicCircuit(multiComps, 3, 60.0);
        CHECK(multiRep.ok);
        CHECK(!multiRep.acNodePhasorsLatex.empty());
        if (!multiRep.acNodePhasorsLatex.empty()) {
            CHECK(multiRep.acNodePhasorsLatex[0] == "\\tilde{V}_{1} = 12\\angle 0^\\circ\\text{ V}");
        }
        for (const auto& ph : multiRep.acNodePhasorsLatex) checkLatexField(ph);
        for (const auto& kv : multiRep.meshKvlEquationsLatex) {
            CHECK(kv.rfind("\\text{Malla ", 0) == 0);
            checkLatexField(kv);
        }
        checkLatexField(multiRep.transferFunctionLatex);
        checkLatexField(multiRep.symbolicHOfSLatex);
        checkLatexField(multiRep.outputResponseSLatex);
        checkLatexField(multiRep.mnaMatrixLatex);
        checkLatexField(multiRep.acPhasorMatrixLatex);
        checkLatexField(multiRep.dcSuperpositionTotalLatex);
        checkLatexField(multiRep.stateSpace.equationsLatex);

        // Coupled mesh + current source KVL label uses \text{Malla ...}
        std::vector<CircuitComponent> coupledMeshComps;
        CHECK(ParseNetlistText("V1 1 0 10; R1 1 2 100; R2 2 0 200; I1 2 0 0.01", coupledMeshComps));
        const auto cmRep = AnalyzeFullSymbolicCircuit(coupledMeshComps, 2, 60.0);
        CHECK(cmRep.ok && !cmRep.meshKvlEquationsLatex.empty());
        for (const auto& kv : cmRep.meshKvlEquationsLatex) {
            CHECK(kv.rfind("\\text{Malla ", 0) == 0);
        }

        // Genuine tiny non-zero phase angle (~ -3.6e-5 deg) must parenthesize before ^\circ
        std::vector<CircuitComponent> tinyAngComps;
        CHECK(ParseNetlistText("V1 1 0 1; R1 1 2 1; C1 2 0 1e-7", tinyAngComps));
        const auto tinyRep = AnalyzeFullSymbolicCircuit(tinyAngComps, 2, 1.0);
        CHECK(tinyRep.ok && tinyRep.acNodePhasorsLatex.size() >= 2);
        if (tinyRep.acNodePhasorsLatex.size() >= 2) {
            const std::string& ph2 = tinyRep.acNodePhasorsLatex[1];
            checkLatexField(ph2);
            CHECK(ph2.find("\\left(") != std::string::npos);
            CHECK(ph2.find("\\right)^\\circ") != std::string::npos);
        }
    }
}

void TestIteration5Regressions() {
    auto checkCirRoundTrip = [&](std::vector<CircuitComponent> orig) {
        const auto sol1 = SolveMnaCircuit(orig, 0.0);
        const std::string cir = ExportCircuitToSpiceCir(orig, "Iter5RT");
        std::vector<CircuitComponent> back;
        const bool pok = ParseNetlistText(cir, back);
        const auto sol2 = SolveMnaCircuit(back, 0.0);
        CHECK(sol1.ok && pok && sol2.ok);
        CHECK(back.size() == orig.size());
        if (back.size() == orig.size()) {
            for (std::size_t i = 0; i < orig.size(); ++i) {
                CHECK(back[i].kind == orig[i].kind);
            }
        }
        double maxVdiff = 0.0;
        for (const auto& kv : sol1.nodeVoltages) {
            const auto it = sol2.nodeVoltages.find(kv.first);
            CHECK(it != sol2.nodeVoltages.end());
            if (it != sol2.nodeVoltages.end()) {
                maxVdiff = (std::max)(maxVdiff, std::abs(kv.second.real() - it->second.real()));
            }
        }
        CHECK(maxVdiff < 1e-9);
    };

    auto checkTheveninVsFiniteDiff = [&](const std::vector<CircuitComponent>& comps, int nodePort) {
        const auto th = ComputeTheveninNorton(comps, nodePort, 0);
        CHECK(th.ok);
        const double epsI = 1e-7;
        auto plusI = comps;
        auto minusI = comps;
        plusI.push_back(MakeComp(ComponentKind::CurrentSource, "ItestP", 0, nodePort, 0, epsI));
        minusI.push_back(MakeComp(ComponentKind::CurrentSource, "ItestM", 0, nodePort, 0, -epsI));
        const auto sP = SolveMnaCircuit(plusI, 0.0);
        const auto sM = SolveMnaCircuit(minusI, 0.0);
        CHECK(sP.ok && sM.ok);
        const double rFd = (sP.nodeVoltages.at(nodePort).real() - sM.nodeVoltages.at(nodePort).real()) / (2.0 * epsI);
        CHECK(std::abs(th.rTheveninOhm - rFd) <= 1e-4 * (std::max)(1.0, rFd));
    };

    // 1. Defect 1: ComponentKind::Diode + "1N5819" / "D1N5819" matches SchottkyDiode & .cir round-trip (kindsOk=1, maxVdiff < 1e-9)
    {
        for (const char* mod : { "1N5819", "D1N5819" }) {
            std::vector<CircuitComponent> diode1N = {
                MakeComp(ComponentKind::VoltageSource, "V1", 1, 0, 0, 5.0),
                MakeComp(ComponentKind::Diode, "D1", 1, 2, 0, 1000.0, 0.0, mod),
                MakeComp(ComponentKind::Resistor, "R1", 2, 0, 0, 1000.0)
            };
            std::vector<CircuitComponent> schottky1N = diode1N;
            schottky1N[1].kind = ComponentKind::SchottkyDiode;

            const auto sDiode = SolveMnaCircuit(diode1N, 0.0);
            const auto sSchottky = SolveMnaCircuit(schottky1N, 0.0);
            CHECK(sDiode.ok && sSchottky.ok);
            CHECK(std::abs(sDiode.nodeVoltages.at(2).real() - sSchottky.nodeVoltages.at(2).real()) < 1e-12);
            const double vd = 5.0 - sDiode.nodeVoltages.at(2).real();
            CHECK(vd > 0.1 && vd < 0.35);

            checkCirRoundTrip(diode1N);
            checkTheveninVsFiniteDiff(diode1N, 2);

            const std::string cir = ExportCircuitToSpiceCir(diode1N, "Diode1N5819");
            std::vector<CircuitComponent> back;
            CHECK(ParseNetlistText(cir, back));
            const auto th1 = ComputeTheveninNorton(diode1N, 2, 0);
            const auto th2 = ComputeTheveninNorton(back, 2, 0);
            CHECK(th1.ok && th2.ok && std::abs(th1.rTheveninOhm - th2.rTheveninOhm) < 1e-9);
        }
    }

    // 2. Defect 2: ComputeTheveninNorton parity on D1N4148, D1N4007, D1N5819, D_DEFAULT, DS_DEFAULT, custom Is/N
    {
        for (const char* mod : { "D1N4148", "1N4148", "D1N4007", "1N4007", "D_DEFAULT", "DS_DEFAULT" }) {
            checkTheveninVsFiniteDiff({
                MakeComp(ComponentKind::VoltageSource, "V1", 1, 0, 0, 5.0),
                MakeComp(ComponentKind::Diode, "D1", 1, 2, 0, 1000.0, 0.0, mod),
                MakeComp(ComponentKind::Resistor, "R1", 2, 0, 0, 1000.0)
            }, 2);
        }
        checkTheveninVsFiniteDiff({
            MakeComp(ComponentKind::VoltageSource, "V1", 1, 0, 0, 5.0),
            MakeComp(ComponentKind::SchottkyDiode, "D1", 1, 2, 0, 0.0, 0.0, "D1N5819"),
            MakeComp(ComponentKind::Resistor, "R1", 2, 0, 0, 1000.0)
        }, 2);

        CircuitComponent customD = MakeComp(ComponentKind::Diode, "D1", 1, 2, 0, 1000.0);
        customD.params["Is"] = 5e-10;
        customD.params["N"] = 2.1;
        checkTheveninVsFiniteDiff({
            MakeComp(ComponentKind::VoltageSource, "V1", 1, 0, 0, 5.0),
            customD,
            MakeComp(ComponentKind::Resistor, "R1", 2, 0, 0, 1000.0)
        }, 2);
    }

    // 3. Defect 3: ZenerDiode with params["Vz"]=6.8 and default value=1000.0 preserves Vz across .cir round-trip
    {
        CircuitComponent z6v8 = MakeComp(ComponentKind::ZenerDiode, "DZ1", 0, 2, 0, 1000.0);
        z6v8.params["Vz"] = 6.8;
        std::vector<CircuitComponent> zComps = {
            MakeComp(ComponentKind::VoltageSource, "V1", 1, 0, 0, 12.0),
            MakeComp(ComponentKind::Resistor, "R1", 1, 2, 0, 1000.0),
            z6v8
        };
        const auto sZ = SolveMnaCircuit(zComps, 0.0);
        CHECK(sZ.ok && sZ.nodeVoltages.at(2).real() > 6.6 && sZ.nodeVoltages.at(2).real() < 7.0);
        checkCirRoundTrip(zComps);
        checkTheveninVsFiniteDiff(zComps, 2);
    }

    // 4. Defect 4: Custom c.params on diodes (Is, N, Vz) preserved across .cir round-trip
    {
        CircuitComponent dCustom = MakeComp(ComponentKind::Diode, "D1", 1, 2, 0, 1000.0);
        dCustom.params["Is"] = 5e-10;
        dCustom.params["N"] = 2.1;
        checkCirRoundTrip({
            MakeComp(ComponentKind::VoltageSource, "V1", 1, 0, 0, 5.0),
            dCustom,
            MakeComp(ComponentKind::Resistor, "R1", 2, 0, 0, 1000.0)
        });

        CircuitComponent sCustom = MakeComp(ComponentKind::SchottkyDiode, "D1", 1, 2, 0, 0.0);
        sCustom.params["Is"] = 3.25e-6;
        sCustom.params["N"] = 1.15;
        checkCirRoundTrip({
            MakeComp(ComponentKind::VoltageSource, "V1", 1, 0, 0, 5.0),
            sCustom,
            MakeComp(ComponentKind::Resistor, "R1", 2, 0, 0, 1000.0)
        });

        CircuitComponent lCustom = MakeComp(ComponentKind::Led, "L1", 1, 2, 0, 0.0, 0.0, "GREEN");
        lCustom.params["Is"] = 2e-19;
        lCustom.params["N"] = 2.15;
        checkCirRoundTrip({
            MakeComp(ComponentKind::VoltageSource, "V1", 1, 0, 0, 5.0),
            lCustom,
            MakeComp(ComponentKind::Resistor, "R1", 2, 0, 0, 1000.0)
        });

        CircuitComponent pdZero = MakeComp(ComponentKind::Photodiode, "PD1", 2, 0, 0, 0.0);
        pdZero.params["Is"] = 2e-11;
        checkCirRoundTrip({
            MakeComp(ComponentKind::VoltageSource, "V1", 1, 0, 0, 5.0),
            MakeComp(ComponentKind::Resistor, "R1", 1, 2, 0, 1000.0),
            pdZero
        });
    }

    // 5. Defect 5: VoltageSource named "VCC" & other SPICE-prefix keyword collisions in ParseNetlistText
    {
        for (const char* vName : { "VCC", "VAC", "VPULSE", "VM", "VP" }) {
            checkCirRoundTrip({
                MakeComp(ComponentKind::VoltageSource, vName, 1, 0, 0, 12.0),
                MakeComp(ComponentKind::Resistor, "R1", 1, 2, 0, 1000.0),
                MakeComp(ComponentKind::Resistor, "R2", 2, 0, 0, 1000.0)
            });
        }
        checkCirRoundTrip({
            MakeComp(ComponentKind::VoltageSource, "VCC", 1, 0, 0, 10.0),
            MakeComp(ComponentKind::Resistor, "REG", 1, 2, 0, 1000.0),
            MakeComp(ComponentKind::Resistor, "RLY", 2, 0, 0, 2000.0),
            MakeComp(ComponentKind::Diode, "DZ", 2, 0, 0, 1000.0, 0.0, "1N4148")
        });
        checkCirRoundTrip({
            MakeComp(ComponentKind::CurrentSource, "IC", 0, 1, 0, 0.01),
            MakeComp(ComponentKind::Capacitor, "COMP", 1, 0, 0, 1e-6),
            MakeComp(ComponentKind::Inductor, "LED", 1, 2, 0, 1e-3),
            MakeComp(ComponentKind::Resistor, "R1", 2, 0, 0, 100.0)
        });

        // Verify genuine explicit keywords still parse cleanly
        std::vector<CircuitComponent> explicitComps;
        CHECK(ParseNetlistText(
            "VCC Vcc 1 5\n"
            "GND 0\n"
            "GND G1 0\n"
            "VAC Vac1 1 0 SINE 5 60\n"
            "LED L1 1 2\n"
            "DZ Z1 2 0 5.1\n"
            "REG U1 1 0 3 7805\n"
            "IC U2 1 0 4 LM317\n"
            "OPAMP U3 1 4 4\n", explicitComps));
        CHECK(explicitComps.size() == 9);
        if (explicitComps.size() == 9) {
            CHECK(explicitComps[0].kind == ComponentKind::Vcc && explicitComps[0].name == "Vcc");
            CHECK(explicitComps[1].kind == ComponentKind::Ground && explicitComps[2].kind == ComponentKind::Ground);
            CHECK(explicitComps[3].kind == ComponentKind::AcSource && explicitComps[4].kind == ComponentKind::Led);
            CHECK(explicitComps[5].kind == ComponentKind::ZenerDiode && explicitComps[6].kind == ComponentKind::VoltageRegulator);
            CHECK(explicitComps[7].kind == ComponentKind::Ic && explicitComps[8].kind == ComponentKind::OpAmp);
        }
    }
}

} // namespace

int main() {
    std::cout << "[1] BackwardsCompat\n" << std::flush;
    TestBackwardsCompatibilityAndF10F11();
    std::cout << "[2] 53Components\n" << std::flush;
    Test53ComponentsAndTintaBijection();
    std::cout << "[3] Nonlinear\n" << std::flush;
    TestNonlinearSemiconductorsActiveIcsAndAc();
    std::cout << "[4] Streaming60Hz\n" << std::flush;
    TestStreamingTransient60HzAndSignalGenerator();
    std::cout << "[5] Symbolic\n" << std::flush;
    TestSymbolicAnalysisAndBug1Fix();
    std::cout << "[6] TintaPorts\n" << std::flush;
    TestTintaNucleoPorts();
    std::cout << "[7] SchematicExporters\n" << std::flush;
    TestSchematicTopologyOverlaysAndExporters();
    std::cout << "[8] CodeMcuControl\n" << std::flush;
    TestCodeBlockMcuCoSimAndControlFlowcharts();
    std::cout << "[9] Iteration2Regressions\n" << std::flush;
    TestIteration2Regressions();
    std::cout << "[10] Iteration3Regressions\n" << std::flush;
    TestIteration3Regressions();
    std::cout << "[11] Iteration4Regressions\n" << std::flush;
    TestIteration4Regressions();
    std::cout << "[12] Iteration5Regressions\n" << std::flush;
    TestIteration5Regressions();

    std::cout << "m1_circuit_test: passed=" << gPassed << ", failed=" << gFailed << "\n";
    return (gFailed == 0) ? 0 : 1;
}
