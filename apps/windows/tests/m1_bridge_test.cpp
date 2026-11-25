#include "bridge/calc_bridge.hpp"
#include "../linux/engines/tests/u13_lcd_font_helper.hpp"

#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <set>
#include <string>
#include <thread>
#include <vector>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#endif

namespace fs = std::filesystem;
using namespace DocStudio;
using namespace DocStudio::CalcBridge;

namespace {

int g_passed = 0;
int g_failed = 0;

void Check(bool cond, const std::string& label, const std::string& detail = {})
{
    if (cond) {
        ++g_passed;
        std::cout << "[PASS] " << label;
        if (!detail.empty()) std::cout << " (" << detail << ")";
        std::cout << "\n";
    } else {
        ++g_failed;
        std::cerr << "[FAIL] " << label;
        if (!detail.empty()) std::cerr << " -- " << detail;
        std::cerr << "\n";
    }
}

std::uint64_t CurrentPid()
{
#ifdef _WIN32
    return static_cast<std::uint64_t>(GetCurrentProcessId());
#else
    return static_cast<std::uint64_t>(::getpid());
#endif
}

void TestSuite1_BackwardCompatibility()
{
    std::cout << "\n=== Suite 1: Backward Compatibility ===\n";
    CalcStepRecord rec;
    rec.modelIdUtf8 = "fx-991CW";
    rec.keySeqUtf8 = "4.7 [x10^x] 3 [x] 2 [x10^x] [(-)] 3 [EXE]";
    rec.expressionUtf8 = "4.7 kOhm * 2 mA | V";
    rec.assignVarUtf8 = "V_out";

    AnnotationObject card = BuildCalcSessionCard(rec, {10.0, 20.0, 330.0, 108.0});
    Check(card.kind == ObjectKind::CalcSessionCard &&
          card.evaluatedResultUtf8.find("9.4") != std::string::npos &&
          card.calcLatexUtf8.find('$') != std::string::npos,
          "BuildCalcSessionCard evaluates engineering units", card.evaluatedResultUtf8);

    card.formulaExprUtf8 = "12 V / (3 kOhm) | mA";
    Check(RefreshCalcSessionCard(card) &&
          card.evaluatedResultUtf8.find("4") != std::string::npos,
          "RefreshCalcSessionCard re-evaluates formulaExprUtf8", card.evaluatedResultUtf8);

    const auto sel = TranslateExpressionToCasioKeys("2*x + y = 7; x - y = 2", "fx-991CW");
    Check(sel.ok && sel.appModeUtf8.find("Equation") != std::string::npos &&
          sel.keySequenceUtf8.find("Simultan") != std::string::npos,
          "TranslateExpressionToCasioKeys SEL fx-991CW", sel.keySequenceUtf8);

    const auto mat = TranslateExpressionToCasioKeys("det([[2, 1], [1, 3]])", "fx-991CW");
    Check(mat.ok && mat.appModeUtf8 == "Matrix" &&
          mat.keySequenceUtf8.find("Determinant") != std::string::npos,
          "TranslateExpressionToCasioKeys Matrix fx-991CW", mat.keySequenceUtf8);

    const auto unitPlan = TranslateExpressionToCasioKeys("V_out = 4.7 kOhm * 2 mA | V", "fx-991CW");
    Check(unitPlan.ok && unitPlan.keySequenceUtf8.find("[x10^x]") != std::string::npos,
          "TranslateExpressionToCasioKeys units fx-991CW", unitPlan.keySequenceUtf8);

    const std::string ph = FormatPhasorAllFormsUtf8({3.0, 4.0}, "V");
    Check(ph.find("Rect:") != std::string::npos &&
          ph.find("Polar:") != std::string::npos &&
          ph.find("Exp:") != std::string::npos,
          "FormatPhasorAllFormsUtf8 formats Rect/Polar/Exp", ph);
}

void TestSuite2_FourModelTranslation()
{
    std::cout << "\n=== Suite 2: 4-Model Keystroke Translation ===\n";
    const char* models[4] = {"fx-570LA X", "fx-570VN PLUS", "fx-CG50", "fx-991CW"};

    for (const char* m : models) {
        const auto sc = TranslateExpressionToCasioKeys("V_out = 4.7 kOhm * 2 mA | V", m);
        Check(sc.ok && !sc.steps.empty() && sc.evaluatedUtf8.find("9.4") != std::string::npos,
              std::string("4-model scalar+units ") + m, sc.keySequenceUtf8);

        const auto ph = TranslateExpressionToCasioKeys("10@30 + 5@60", m);
        Check(ph.ok && ph.appModeUtf8 == "Complex" && !ph.steps.empty(),
              std::string("4-model phasor ") + m, ph.keySequenceUtf8);

        const auto mat = TranslateExpressionToCasioKeys("det([[2, 1], [1, 3]])", m);
        Check(mat.ok && mat.appModeUtf8 == "Matrix" && !mat.steps.empty(),
              std::string("4-model matrix ") + m, mat.keySequenceUtf8);

        const auto sel = TranslateExpressionToCasioKeys("2*x + y = 7; x - y = 2", m);
        Check(sel.ok && sel.appModeUtf8 == "Equation (Simultan)" && !sel.steps.empty(),
              std::string("4-model SEL ") + m, sel.keySequenceUtf8);
    }

    const auto pLax = TranslateExpressionToCasioKeys("2*x + y = 7; x - y = 2", "fx-570LA X");
    const auto pVn = TranslateExpressionToCasioKeys("2*x + y = 7; x - y = 2", "fx-570VN PLUS");
    const auto pCg = TranslateExpressionToCasioKeys("2*x + y = 7; x - y = 2", "fx-CG50");
    const auto pCw = TranslateExpressionToCasioKeys("2*x + y = 7; x - y = 2", "fx-991CW");
    Check(pLax.model == CasioModel::Fx570LaX &&
          pVn.model == CasioModel::Fx570VnPlus &&
          pCg.model == CasioModel::FxCg50 &&
          pCw.model == CasioModel::Fx991Cw &&
          pVn.keySequenceUtf8.find("[MODE] [5]") != std::string::npos &&
          pCg.keySequenceUtf8.find("[MENU] [A]") != std::string::npos &&
          pCg.keySequenceUtf8.find("[F1]") != std::string::npos &&
          pCw.keySequenceUtf8.find("[HOME]") != std::string::npos,
          "Distinct hardware model menus & key tokens verified across 4 models");
}

void TestSuite3_SessionCardPngAndReplay()
{
    std::cout << "\n=== Suite 3: SessionState, PNG Encoding, Replay & JSON ===\n";
    // 1) Pure C++17 PNG encoding check (16x8 synthetic RGBA LCD image)
    const int W = 16, H = 8;
    std::vector<std::uint8_t> rgba(static_cast<std::size_t>(W) * H * 4u, 220u);
    for (int x = 2; x < 14; ++x) {
        const std::size_t idx = (static_cast<std::size_t>(3) * W + x) * 4u;
        rgba[idx + 0] = 20u; rgba[idx + 1] = 35u; rgba[idx + 2] = 55u; rgba[idx + 3] = 255u;
    }
    const std::vector<std::uint8_t> png = EncodeRgbaToPng(W, H, rgba);
    Check(png.size() > 32 &&
          png[0] == 0x89u && png[1] == 'P' && png[2] == 'N' && png[3] == 'G' &&
          png[4] == 0x0Du && png[5] == 0x0Au && png[6] == 0x1Au && png[7] == 0x0Au,
          "EncodeRgbaToPng generates valid PNG signature", "bytes=" + std::to_string(png.size()));

    // 2) Replay without formulaExprUtf8
    AnnotationObject replayObj;
    replayObj.kind = ObjectKind::CalcSessionCard;
    replayObj.calcModelIdUtf8 = "fx-991CW";
    replayObj.calcKeySeqUtf8 = "[HOME] Calculate [OK] 2 [+] 3 [x] 4 [EXE]";
    replayObj.formulaExprUtf8.clear();
    Check(RefreshCalcSessionCard(replayObj) && replayObj.evaluatedResultUtf8 == "14",
          "RefreshCalcSessionCard replays calcKeySeqUtf8 when formulaExprUtf8 is empty",
          replayObj.evaluatedResultUtf8);

    // 3) Live session registry + CalcSessionCard + JSON round-trip
    CalcSessionState st;
    st.calcSessionIdUtf8 = "sess_m1_4_live";
    st.modelIdUtf8 = "fx-570LA X";
    st.appModeUtf8 = "Calculate";
    st.inputSequenceUtf8 = "[MENU] [1] (Calcular) 7 [x] 6 [=]";
    st.formulaSourceUtf8 = "7 * 6";
    st.resultUtf8 = "42";
    st.shiftAlphaState.shiftActive = true;
    st.shiftAlphaState.angleModeUtf8 = "DEG";
    st.shiftAlphaState = NormalizeShiftAlphaState(st.shiftAlphaState, st.modelIdUtf8);
    st.steps.push_back(CasioKeyStep{
        0, st.modelIdUtf8, "7", "7", KeyLayerKind::Normal,
        MakeShiftAlphaBadge(KeyLayerKind::Normal), "Calculate", "7", "Nota docente", 100
    });
    CalcDisplayFrame frame;
    frame.timestampMs = 150;
    frame.stepIndex = 0;
    frame.keyNameUtf8 = "IGUAL";
    frame.lcdTextUtf8 = "7*6";
    frame.resultUtf8 = "42";
    frame.pixelWidth = W;
    frame.pixelHeight = H;
    frame.rgbaPixels = rgba;
    frame.shiftAlphaState = st.shiftAlphaState;
    st.displayFrames.push_back(frame);

    const std::string jsonStr = SerializeSessionStateToJson(st);
    CalcSessionState decoded;
    std::string jerr;
    Check(DeserializeSessionStateFromJson(jsonStr, decoded, &jerr) &&
          decoded.calcSessionIdUtf8 == st.calcSessionIdUtf8 &&
          decoded.displayFrames.size() == 1 &&
          decoded.displayFrames[0].lcdPngBytes.size() == png.size(),
          "SessionState JSON round-trip preserves auto-encoded PNG and metadata", jerr);

    RegisterLiveSessionState(decoded);
    AnnotationObject boundCard = BuildCalcSessionCard(decoded, {0, 0, 320, 100});
    Check(ExtractSessionIdFromCard(boundCard) == "sess_m1_4_live" &&
          !boundCard.pngBytes.empty() &&
          RefreshCalcSessionCard(boundCard) &&
          boundCard.evaluatedResultUtf8 == "42",
          "Live-bound CalcSessionCard syncs from session registry");
    UnregisterLiveSessionState("sess_m1_4_live");
}

void TestSuite4_BidirectionalLocalIpc()
{
    std::cout << "\n=== Suite 4: Bidirectional CalcIpcChannel (File Mailbox + Native Pipe/AF_UNIX) ===\n";
    std::error_code ec;
    const fs::path isoDir = fs::temp_directory_path(ec) /
        ("m1_bridge_ipc_" + std::to_string(CurrentPid()));
    fs::remove_all(isoDir, ec);
    fs::create_directories(isoDir, ec);

    CalcIpcChannel calcHost;
    CalcIpcChannel studio;
    CalcIpcConfig hostCfg;
    hostCfg.channelNameUtf8 = "test_m1_4";
    hostCfg.directoryOverrideUtf8 = isoDir.string();
    hostCfg.role = CalcIpcRole::CalculatorHost;

    CalcIpcConfig studioCfg = hostCfg;
    studioCfg.role = CalcIpcRole::DocumentStudio;

    std::string err;
    Check(calcHost.Open(hostCfg, &err) && studio.Open(studioCfg, &err),
          "CalcIpcChannel Open succeeds on both sides", err);

    CalcSessionState sess;
    sess.calcSessionIdUtf8 = "ipc_sess_01";
    sess.modelIdUtf8 = "fx-570VN PLUS";
    sess.inputSequenceUtf8 = "9 * 8";
    sess.formulaSourceUtf8 = "9 * 8";
    sess.resultUtf8 = "72";
    Check(calcHost.PublishSession(sess, &err), "CalculatorHost PublishSession", err);

    std::vector<std::uint8_t> rgba(16u * 8u * 4u, 230u);
    CalcDisplayFrame df;
    df.stepIndex = 1;
    df.keyNameUtf8 = "IGUAL";
    df.lcdTextUtf8 = "9*8";
    df.resultUtf8 = "72";
    df.pixelWidth = 16;
    df.pixelHeight = 8;
    df.rgbaPixels = rgba;
    Check(calcHost.PushLcdFrame("ipc_sess_01", df, &err), "CalculatorHost PushLcdFrame", err);

    const auto msgsToStudio = studio.Poll(16, &err);
    const auto active = studio.ListActiveSessions();
    Check(msgsToStudio.size() == 2 && active.size() == 1 && active[0] == "ipc_sess_01",
          "DocumentStudio Poll receives SessionStateSync & LcdFrameCapture");

    AnnotationObject card;
    card.kind = ObjectKind::CalcSessionCard;
    BindCardToSession(card, "ipc_sess_01");
    Check(studio.PullIntoCard(card, {}, &err) &&
          card.evaluatedResultUtf8 == "72" &&
          card.pngBytes.size() > 32 &&
          card.pngBytes[0] == 0x89u,
          "DocumentStudio PullIntoCard updates result and PNG bytes", err);

    Check(studio.RequestKeystrokeReplay("ipc_sess_01", "fx-570VN PLUS", "1 + 2 =", "1+2", &err),
          "DocumentStudio RequestKeystrokeReplay", err);
    const auto msgsToCalc = calcHost.Poll(16, &err);
    Check(msgsToCalc.size() == 1 &&
          msgsToCalc[0].type == CalcIpcMessageType::KeystrokeReplay &&
          msgsToCalc[0].keySeqUtf8 == "1 + 2 =",
          "CalculatorHost Poll receives KeystrokeReplay request");

    // Native stream pipe / AF_UNIX test (in OS temp/runtime dir, never /mnt/d)
    Check(studio.StartNativePipeServer(&err), "StartNativePipeServer succeeds", err);
    const std::string pipePayload = "{\"ping\":\"m1_4_native_ipc\",\"ok\":true}";
    Check(calcHost.SendNativePipeBytes(pipePayload, &err), "SendNativePipeBytes succeeds", err);
    std::string recvPayload;
    Check(studio.RecvNativePipeBytes(recvPayload, 1000, &err) && recvPayload == pipePayload,
          "RecvNativePipeBytes round-trips exact payload", err);

    calcHost.Close();
    studio.Close();
    UnregisterLiveSessionState("ipc_sess_01");
    fs::remove_all(isoDir, ec);
}

void TestSuite5_ModelComparison()
{
    std::cout << "\n=== Suite 5: CompareCalculatorModels Differential Engine ===\n";
    const auto repDiv = CompareCalculatorModels("1/7");
    Check(repDiv.ok && repDiv.models.size() == 4 && repDiv.allModelsSupported &&
          repDiv.allResultsAgreeWithinTol && repDiv.hasKeySequenceDifferences &&
          repDiv.models[0].displayResultUtf8 == "0,1428571429" &&
          repDiv.models[1].displayResultUtf8 == "0.1428571429" &&
          repDiv.models[2].displayResultUtf8 == "0.1428571429" &&
          repDiv.models[3].displayResultUtf8 == "0.1428571429",
          "CompareCalculatorModels 1/7 enforces LAX comma vs dot & 10-digit parity",
          repDiv.diffSummaryUtf8);

    const auto repSel4 = CompareCalculatorModels(
        "w + x + y + z = 10; 2*w - x + y - z = 1; w + 2*x - y + z = 6; w - x + 2*y + z = 9");
    Check(repSel4.ok && !repSel4.allModelsSupported &&
          repSel4.models[0].supported &&
          !repSel4.models[1].supported &&
          repSel4.models[2].supported &&
          repSel4.models[3].supported,
          "CompareCalculatorModels flags 4-unknown SEL unsupported on fx-570VN PLUS",
          repSel4.models[1].diffNoteUtf8);
}

void TestSuite6_KeyLogPresenterAndShiftAlpha()
{
    std::cout << "\n=== Suite 6: KeyLog Presenter & SHIFT/ALPHA Badges ===\n";
    const auto bN = MakeShiftAlphaBadge(KeyLayerKind::Normal);
    const auto bS = MakeShiftAlphaBadge(KeyLayerKind::Shift);
    const auto bA = MakeShiftAlphaBadge(KeyLayerKind::Alpha);
    const auto bSA = MakeShiftAlphaBadge(KeyLayerKind::ShiftAlpha);
    Check(bN.badgeTextUtf8 == "[N] NORMAL" && bS.badgeTextUtf8 == "[S] SHIFT" &&
          bA.badgeTextUtf8 == "[A] ALPHA" && bSA.badgeTextUtf8 == "[S+A] SHIFT+ALPHA",
          "Accessible non-color-only SHIFT/ALPHA badges");

    const auto plan = TranslateExpressionToCasioKeys("asin(0.5)", "fx-570VN PLUS");
    KeyLogPresenterState st = BuildKeyLogPresenterFromPlan(plan);
    Check(!st.steps.empty() && st.cursorIndex == 0, "BuildKeyLogPresenterFromPlan initialized");
    st.steps[0].noteUtf8 = "Privado docente";
    st.presenterCleanMode = false;
    const std::string withNote = FormatPresenterStepCalloutUtf8(st);
    st.presenterCleanMode = true;
    const std::string clean = FormatPresenterStepCalloutUtf8(st);
    Check(withNote.find("Privado docente") != std::string::npos &&
          clean.find("Privado docente") == std::string::npos,
          "presenterCleanMode hides private teacher step notes");

    Check(StepPresenterNext(st) && st.cursorIndex == 1 &&
          StepPresenterLast(st) && st.cursorIndex == st.steps.size() - 1 &&
          StepPresenterPrev(st) && StepPresenterFirst(st) && st.cursorIndex == 0 &&
          JumpPresenterToStep(st, 1) && CurrentPresenterStep(st) != nullptr,
          "KeyLogPresenterState transport controls First/Prev/Next/Last/Jump");
}

void TestSuite7_U13EighteenCaseDifferentialMatrix()
{
    std::cout << "\n=== Suite 7: U-13 18-Case Differential Matrix ===\n";
    const auto& cases = U13Matrix::Get18Cases();
    Check(cases.size() == 18, "U-13 matrix has 18 cases");
    int passed18 = 0;
    for (const auto& c : cases) {
        const auto rep = CompareCalculatorModels(c.expr);
        const std::string expLax = U13Matrix::ExpectedForLax(c.expectedDot);
        const bool okCase = rep.ok && rep.allModelsSupported && rep.allResultsAgreeWithinTol &&
            rep.models.size() == 4 &&
            rep.models[0].displayResultUtf8 == expLax &&
            rep.models[1].displayResultUtf8 == c.expectedDot &&
            rep.models[2].displayResultUtf8 == c.expectedDot &&
            rep.models[3].displayResultUtf8 == c.expectedDot;
        if (okCase) ++passed18;
        Check(okCase, std::string("U-13 CompareCalculatorModels [") + c.id + "] " + c.expr,
              "LAX=" + (rep.models.empty() ? std::string() : rep.models[0].displayResultUtf8) +
              " VN+=" + (rep.models.size() > 1 ? rep.models[1].displayResultUtf8 : std::string()));
    }
    Check(passed18 == 18, "U-13 18/18 differential parity across all 4 Casio models + CAS");
}

void TestSuite8_Iteration2Regressions()
{
    std::cout << "\n=== Suite 8: Iteration 2 Regressions (B-01..B-08, Angle Mode & SHIFT Round-Trip) ===\n";

    // 1. Angle-mode DEG/RAD/GRAD across TranslateExpressionToCasioKeys, ReplaySessionKeySequence, Build/RefreshCalcSessionCard
    {
        const auto trSin = TranslateExpressionToCasioKeys("sin(30)", "fx-991CW");
        Check(trSin.ok && trSin.evaluatedUtf8 == "0.5",
              "TranslateExpressionToCasioKeys('sin(30)') evaluates in DEG mode -> 0.5",
              trSin.evaluatedUtf8);

        CalcSessionState degSess;
        degSess.modelIdUtf8 = "fx-991CW";
        degSess.formulaSourceUtf8 = "sin(30)";
        degSess.shiftAlphaState.angleModeUtf8 = "DEG";
        Check(ReplaySessionKeySequence(degSess) && degSess.resultUtf8 == "0.5",
              "ReplaySessionKeySequence honors DEG -> sin(30)=0.5", degSess.resultUtf8);

        CalcSessionState radSess;
        radSess.modelIdUtf8 = "fx-991CW";
        radSess.formulaSourceUtf8 = "sin(30)";
        radSess.shiftAlphaState.angleModeUtf8 = "RAD";
        radSess.unitAssumptionsUtf8 = "RAD; SI";
        const double radVal = std::sin(30.0);
        Check(ReplaySessionKeySequence(radSess) &&
              std::abs(std::stod(radSess.resultUtf8) - radVal) < 1e-4,
              "ReplaySessionKeySequence honors RAD -> sin(30 rad)", radSess.resultUtf8);

        CalcSessionState gradSess;
        gradSess.modelIdUtf8 = "fx-991CW";
        gradSess.formulaSourceUtf8 = "sin(100)";
        gradSess.shiftAlphaState.angleModeUtf8 = "GRAD";
        Check(ReplaySessionKeySequence(gradSess) && gradSess.resultUtf8 == "1",
              "ReplaySessionKeySequence honors GRAD -> sin(100 grad)=1", gradSess.resultUtf8);

        CalcStepRecord recSin;
        recSin.modelIdUtf8 = "fx-991CW";
        recSin.expressionUtf8 = "sin(30)";
        AnnotationObject cardSin = BuildCalcSessionCard(recSin, {0, 0, 200, 80});
        Check(cardSin.evaluatedResultUtf8 == "0.5",
              "BuildCalcSessionCard defaults to DEG -> sin(30)=0.5", cardSin.evaluatedResultUtf8);
        cardSin.evaluatedResultUtf8.clear();
        Check(RefreshCalcSessionCard(cardSin) && cardSin.evaluatedResultUtf8 == "0.5",
              "RefreshCalcSessionCard defaults to DEG -> sin(30)=0.5", cardSin.evaluatedResultUtf8);
    }

    // 2. B-04: [SHIFT] round-trip (asin/acos/atan), VN+ phasor, CG50 i, [x10^x] precedence
    {
        for (const auto& pair : std::vector<std::pair<std::string, std::string>>{
                {"asin(0.5)", "30"}, {"acos(0.5)", "60"}, {"atan(1)", "45"}}) {
            const auto plan = TranslateExpressionToCasioKeys(pair.first, "fx-570VN PLUS");
            CalcSessionState s;
            s.modelIdUtf8 = "fx-570VN PLUS";
            s.inputSequenceUtf8 = plan.keySequenceUtf8;
            s.shiftAlphaState.angleModeUtf8 = "DEG";
            const std::string recon = KeySequenceToExpressionUtf8(plan.keySequenceUtf8);
            const bool rOk = ReplaySessionKeySequence(s);
            Check(rOk && s.resultUtf8 == pair.second,
                  "B-04 [SHIFT] inverse trig round-trip: " + pair.first,
                  "recon='" + recon + "' res='" + s.resultUtf8 + "'");
        }

        const auto vnPh = TranslateExpressionToCasioKeys("10@30", "fx-570VN PLUS");
        const std::string vnRecon = KeySequenceToExpressionUtf8(vnPh.keySequenceUtf8);
        Check(vnRecon == "10@30", "B-04 fx-570VN PLUS 10@30 key round-trip", vnRecon);

        const auto cgCplx = TranslateExpressionToCasioKeys("3 + 4*i", "fx-CG50");
        const std::string cgRecon = KeySequenceToExpressionUtf8(cgCplx.keySequenceUtf8);
        Check(cgRecon == "3+4*i", "B-04 fx-CG50 3+4*i key round-trip", cgRecon);

        CalcSessionState expPrec;
        expPrec.modelIdUtf8 = "fx-991CW";
        expPrec.inputSequenceUtf8 = "[HOME] Calculate [OK] 1 [/] 2 [x10^x] 3 [EXE]";
        const std::string precRecon = KeySequenceToExpressionUtf8(expPrec.inputSequenceUtf8);
        const bool precOk = ReplaySessionKeySequence(expPrec);
        Check(precOk &&
              std::abs(std::stod(expPrec.resultUtf8) - 0.0005) < 1e-9,
              "B-04 [x10^x] division precedence 1/(2*10^3) == 0.0005",
              "recon='" + precRecon + "' res='" + expPrec.resultUtf8 + "'");
    }

    // 3. B-03 & B-08: Scientific notation, unary minus, unknown symbols, '|' unit scaling
    {
        const auto sci = TranslateExpressionToCasioKeys("1.5e-3 * 2e6", "fx-991CW");
        const std::string sciRecon = KeySequenceToExpressionUtf8(sci.keySequenceUtf8);
        const auto sciEv = Cas::EvaluateExpression(sciRecon);
        Check(sci.ok && sci.evaluatedUtf8 == "3000" &&
              sci.keySequenceUtf8.find("[x10^x]") != std::string::npos &&
              sciEv.ok && std::abs(sciEv.result.value.real() - 3000.0) < 1e-6,
              "B-03 scientific notation 1.5e-3 * 2e6", "keys='" + sci.keySequenceUtf8 + "' recon='" + sciRecon + "'");

        const auto negPlan = TranslateExpressionToCasioKeys("-5 + 3", "fx-991CW");
        Check(negPlan.ok && negPlan.keySequenceUtf8.find("[(-)] 5 [+] 3") != std::string::npos,
              "B-03 unary minus emits [(-)] for -5 + 3", negPlan.keySequenceUtf8);

        const auto phNeg = TranslateExpressionToCasioKeys("10@-45", "fx-991CW");
        Check(phNeg.ok && phNeg.keySequenceUtf8.find("[(-)] 4 5") != std::string::npos,
              "B-03 unary minus emits [(-)] in 10@-45", phNeg.keySequenceUtf8);

        const auto unkFoo = TranslateExpressionToCasioKeys("foo(3)", "fx-991CW");
        Check(!unkFoo.ok && !unkFoo.warningsUtf8.empty() &&
              unkFoo.keySequenceUtf8.find("foo") != std::string::npos,
              "B-03 unknown function foo(3) warns and preserves token", unkFoo.keySequenceUtf8);

        const auto unkX = TranslateExpressionToCasioKeys("x + 2", "fx-991CW");
        Check(!unkX.ok && !unkX.warningsUtf8.empty() &&
              unkX.keySequenceUtf8.find("x") != std::string::npos,
              "B-03 unbound variable x + 2 warns and preserves token", unkX.keySequenceUtf8);

        const auto mvPlan = TranslateExpressionToCasioKeys("4.7 kOhm * 2 mA | mV", "fx-991CW");
        const std::string mvRecon = KeySequenceToExpressionUtf8(mvPlan.keySequenceUtf8);
        const auto mvEv = Cas::EvaluateExpression(mvRecon);
        Check(mvPlan.ok && mvPlan.evaluatedUtf8.find("9400") != std::string::npos &&
              mvEv.ok && std::abs(mvEv.result.value.real() - 9400.0) < 1e-6,
              "B-08 '|' target unit scaling 4.7 kOhm * 2 mA | mV -> 9400",
              "keys='" + mvPlan.keySequenceUtf8 + "' recon='" + mvRecon + "'");
    }

    // 4. B-05 & B-07: CompareCalculatorModels (1/0, complex agreement, 5e-13) & StepPresenterNext SIZE_MAX
    {
        const auto repZeroDiv = CompareCalculatorModels("1/0");
        Check(!repZeroDiv.ok && !repZeroDiv.allModelsSupported &&
              !repZeroDiv.models.empty() && !repZeroDiv.models[0].supported,
              "B-05 CompareCalculatorModels('1/0') sets allModelsSupported=false");

        const auto repCplx = CompareCalculatorModels("sin(30) + 1i");
        Check(repCplx.ok && repCplx.allModelsSupported && repCplx.allResultsAgreeWithinTol,
              "B-05 CompareCalculatorModels('sin(30) + 1i') complex DEG agreement",
              repCplx.casReferenceResultUtf8);

        const auto repTiny = CompareCalculatorModels("1 / 2000000000000");
        Check(repTiny.ok && repTiny.models.size() == 4 &&
              repTiny.models[1].displayResultUtf8 != "0" &&
              std::abs(repTiny.models[1].numericValue - 5e-13) < 1e-18,
              "B-05 FormatCasioDisplayValue preserves 5e-13 without zero clamp",
              repTiny.models[1].displayResultUtf8);

        KeyLogPresenterState pst;
        pst.steps.push_back(CasioKeyStep{});
        pst.cursorIndex = (std::numeric_limits<std::size_t>::max)();
        Check(!StepPresenterNext(pst) &&
              pst.cursorIndex == (std::numeric_limits<std::size_t>::max)(),
              "B-07 StepPresenterNext guards SIZE_MAX overflow");
    }

    // 5. B-01, B-02 & B-06: Concurrent PushLcdFrame, SanitizeIdentifier non-collision, Native Pipe/AF_UNIX lifecycle
    {
        std::error_code ec;
        const fs::path isoDir = fs::temp_directory_path(ec) /
            ("m1_bridge_r2_" + std::to_string(CurrentPid()));
        fs::remove_all(isoDir, ec);
        fs::create_directories(isoDir, ec);

        CalcIpcChannel host;
        CalcIpcChannel studio;
        CalcIpcConfig hCfg;
        hCfg.channelNameUtf8 = "r2_regress";
        hCfg.directoryOverrideUtf8 = isoDir.string();
        hCfg.role = CalcIpcRole::CalculatorHost;
        CalcIpcConfig sCfg = hCfg;
        sCfg.role = CalcIpcRole::DocumentStudio;

        std::string err;
        Check(host.Open(hCfg, &err) && studio.Open(sCfg, &err),
              "B-01/B-06 IPC Open for regression tests", err);

        // B-06: SanitizeIdentifier collision check & traversal containment
        CalcSessionState sSlash;
        sSlash.calcSessionIdUtf8 = "a/b";
        sSlash.resultUtf8 = "111";
        CalcSessionState sUnder;
        sUnder.calcSessionIdUtf8 = "a_b";
        sUnder.resultUtf8 = "222";
        CalcSessionState sTrav;
        sTrav.calcSessionIdUtf8 = "../../etc/passwd";
        sTrav.resultUtf8 = "333";

        Check(host.PublishSession(sSlash, &err) &&
              host.PublishSession(sUnder, &err) &&
              host.PublishSession(sTrav, &err),
              "B-06 PublishSession a/b, a_b, ../../etc/passwd", err);
        CalcSessionState gotSlash, gotUnder, gotTrav;
        Check(studio.ReadSession("a/b", gotSlash, &err) && gotSlash.resultUtf8 == "111" &&
              studio.ReadSession("a_b", gotUnder, &err) && gotUnder.resultUtf8 == "222" &&
              studio.ReadSession("../../etc/passwd", gotTrav, &err) && gotTrav.resultUtf8 == "333",
              "B-06 a/b and a_b do not collide and traversal stays contained");

        // B-01: Concurrent PushLcdFrame (2 threads x 20 frames -> 40/40 preserved)
        CalcSessionState baseSess;
        baseSess.calcSessionIdUtf8 = "concurrent_frames";
        host.PublishSession(baseSess, &err);
        auto pushWorker = [&](int tid) {
            for (int k = 0; k < 20; ++k) {
                CalcDisplayFrame f;
                f.stepIndex = static_cast<std::size_t>(tid * 20 + k);
                f.keyNameUtf8 = "K";
                f.lcdTextUtf8 = std::to_string(tid) + ":" + std::to_string(k);
                host.PushLcdFrame("concurrent_frames", f, nullptr);
            }
        };
        std::thread t0(pushWorker, 0);
        std::thread t1(pushWorker, 1);
        t0.join();
        t1.join();
        CalcSessionState afterConc;
        err.clear();
        const bool concReadOk = studio.ReadSession("concurrent_frames", afterConc, &err);
        Check(concReadOk && afterConc.displayFrames.size() == 40,
              "B-01 concurrent PushLcdFrame preserves 40/40 frames",
              "frames=" + std::to_string(afterConc.displayFrames.size()));

        // B-02: Native Pipe / AF_UNIX lifecycle + 16 MiB cap + >64 KiB same-thread
        err.clear();
        Check(studio.StartNativePipeServer(&err), "B-02 StartNativePipeServer", err);
        const std::string oversized((16u * 1024u * 1024u) + 1u, 'Z');
        err.clear();
        Check(!host.SendNativePipeBytes(oversized, &err),
              "B-02 SendNativePipeBytes rejects >16 MiB payload", err);

        std::string r1, r2, r3;
        err.clear();
        const bool p1 = host.SendNativePipeBytes("first", &err) &&
                        studio.RecvNativePipeBytes(r1, 1000, &err) && r1 == "first" &&
                        host.SendNativePipeBytes("after_recv", &err) &&
                        studio.RecvNativePipeBytes(r2, 1000, &err) && r2 == "after_recv";
        Check(p1, "B-02 Native Pipe resend after Recv without server restart", err);

        err.clear();
        const bool p2 = host.SendNativePipeBytes("queued_1", &err) &&
                        host.SendNativePipeBytes("queued_2", &err) &&
                        studio.RecvNativePipeBytes(r1, 1000, &err) && r1 == "queued_1" &&
                        studio.RecvNativePipeBytes(r2, 1000, &err) && r2 == "queued_2";
        Check(p2, "B-02 Native Pipe two consecutive sends before Recv", err);

        err.clear();
        const std::string big100k(100u * 1024u, 'B');
        const bool p3 = host.SendNativePipeBytes(big100k, &err) &&
                        studio.RecvNativePipeBytes(r3, 2000, &err) && r3 == big100k;
        Check(p3, "B-02 Native Pipe 100 KiB same-thread send/recv without deadlock", err);

        host.Close();
        studio.Close();
        UnregisterLiveSessionState("a/b");
        UnregisterLiveSessionState("a_b");
        UnregisterLiveSessionState("../../etc/passwd");
        UnregisterLiveSessionState("concurrent_frames");
        fs::remove_all(isoDir, ec);
    }
}

void TestSuite9_Iteration3Regressions()
{
    std::cout << "\n=== Suite 9: Iteration 3 Regressions (Pipe Concurrency, Shutdown Wakeup, Locks, KeySeq, Units, SEL) ===\n";

    // 1. KeySequenceToExpressionUtf8: [sin] 3 0 [)] [x10^x] 2 -> 50.0, [SHIFT] [x10^x] -> pi
    {
        const std::string sinExpExpr = KeySequenceToExpressionUtf8(
            "[HOME] Calculate [OK] [sin] 3 0 [)] [x10^x] 2 [EXE]");
        Cas::CasEvalOptions degOpts;
        degOpts.trigAngleMode = Cas::CasAngleMode::Deg;
        const auto sinExpEv = Cas::EvaluateExpression(sinExpExpr, {}, degOpts);
        Check(sinExpEv.ok && std::abs(sinExpEv.result.value.real() - 50.0) < 1e-6,
              "R2-06 [sin] 3 0 [)] [x10^x] 2 evaluates to 50.0",
              "expr='" + sinExpExpr + "' res='" + sinExpEv.formattedResultUtf8 + "'");

        const std::string piMulExpr = KeySequenceToExpressionUtf8("2 [x] [SHIFT] [x10^x] [EXE]");
        const auto piMulEv = Cas::EvaluateExpression(piMulExpr);
        Check(piMulExpr == "2*pi" && piMulEv.ok &&
              std::abs(piMulEv.result.value.real() - (2.0 * 3.14159265358979323846)) < 1e-5,
              "R2-07 2 [x] [SHIFT] [x10^x] reconstructs 2*pi",
              "expr='" + piMulExpr + "' res='" + piMulEv.formattedResultUtf8 + "'");

        const std::string piImplExpr = KeySequenceToExpressionUtf8("2 [SHIFT] [x10^x] [EXE]");
        const auto piImplEv = Cas::EvaluateExpression(piImplExpr);
        Check(piImplExpr == "2*pi" && piImplEv.ok &&
              std::abs(piImplEv.result.value.real() - (2.0 * 3.14159265358979323846)) < 1e-5,
              "R2-07 2 [SHIFT] [x10^x] implicit * reconstructs 2*pi",
              "expr='" + piImplExpr + "'");
    }

    // 2. TranslateExpressionToCasioKeys: denominator units & offset temperature
    {
        const auto speedPlan = TranslateExpressionToCasioKeys("10 km / 2 h | m/s", "fx-991CW");
        const std::string speedRecon = KeySequenceToExpressionUtf8(speedPlan.keySequenceUtf8);
        const auto speedEv = Cas::EvaluateExpression(speedRecon);
        Check(speedPlan.ok && speedEv.ok &&
              std::abs(speedEv.result.value.real() - (10000.0 / 7200.0)) < 1e-4,
              "R2-08 10 km / 2 h | m/s replays to 1.38889",
              "keys='" + speedPlan.keySequenceUtf8 + "' recon='" + speedRecon + "' res='" + speedEv.formattedResultUtf8 + "'");

        const auto tempPlan = TranslateExpressionToCasioKeys("100 degC | K", "fx-991CW");
        const std::string tempRecon = KeySequenceToExpressionUtf8(tempPlan.keySequenceUtf8);
        const auto tempEv = Cas::EvaluateExpression(tempRecon);
        Check(tempPlan.ok && tempEv.ok &&
              std::abs(tempEv.result.value.real() - 373.15) < 1e-6,
              "R2-09 100 degC | K replays to 373.15",
              "keys='" + tempPlan.keySequenceUtf8 + "' recon='" + tempRecon + "' res='" + tempEv.formattedResultUtf8 + "'");
    }

    // 3. CompareCalculatorModels: 4-unknown SEL sets allResultsAgreeWithinTol = false
    {
        const auto repSel4 = CompareCalculatorModels(
            "w + x + y + z = 10; 2*w - x + y - z = 1; w + 2*x - y + z = 6; w - x + 2*y + z = 9");
        Check(repSel4.ok && !repSel4.allModelsSupported && !repSel4.allResultsAgreeWithinTol,
              "Reviewer2-F3 CompareCalculatorModels 4-unknown SEL sets allResultsAgreeWithinTol=false");
    }

    // 4. Pipe concurrency, 512 KiB same-thread, stalled mid-read Close(), Recv wakeup, Lock rejection & ReadSession stress
    {
        std::error_code ec;
        const fs::path isoDir = fs::temp_directory_path(ec) /
            ("m1_bridge_r3_" + std::to_string(CurrentPid()));
        fs::remove_all(isoDir, ec);
        fs::create_directories(isoDir, ec);

        CalcIpcChannel host;
        CalcIpcChannel studio;
        CalcIpcConfig hCfg;
        hCfg.channelNameUtf8 = "r3_stress";
        hCfg.directoryOverrideUtf8 = isoDir.string();
        hCfg.role = CalcIpcRole::CalculatorHost;
        CalcIpcConfig sCfg = hCfg;
        sCfg.role = CalcIpcRole::DocumentStudio;

        std::string err;
        Check(host.Open(hCfg, &err) && studio.Open(sCfg, &err),
              "R3 IPC Open succeeds", err);

        // 4a. Unexpired .lock rejection (R2-05)
        {
            CalcSessionState orig;
            orig.calcSessionIdUtf8 = "locked-sess";
            orig.resultUtf8 = "ORIGINAL";
            Check(host.PublishSession(orig, &err), "R2-05 PublishSession initial locked-sess", err);

            const fs::path lockFile = isoDir / "sessions" / "locked-sess.lock";
            {
                std::ofstream lofs(lockFile, std::ios::binary | std::ios::trunc);
                lofs << "external_holder";
            }

            CalcSessionState overwrite = orig;
            overwrite.resultUtf8 = "OVERWRITTEN";
            std::string lockErr;
            const bool pubBlocked = !host.PublishSession(overwrite, &lockErr);

            CalcDisplayFrame df;
            df.stepIndex = 1;
            df.resultUtf8 = "OVERWRITTEN";
            std::string pushErr;
            const bool pushBlocked = !host.PushLcdFrame("locked-sess", df, &pushErr);

            CalcSessionState afterRead;
            const bool stillOriginal = studio.ReadSession("locked-sess", afterRead, &err) &&
                                       afterRead.resultUtf8 == "ORIGINAL" &&
                                       afterRead.displayFrames.empty();
            fs::remove(lockFile, ec);
            Check(pubBlocked && pushBlocked && stillOriginal,
                  "R2-05 unexpired .lock rejects PublishSession & PushLcdFrame without overwriting",
                  lockErr);
        }

        // 4b. Concurrent ReadSession + PushLcdFrame + ListActiveSessions (R2-03)
        {
            CalcSessionState initSess;
            initSess.calcSessionIdUtf8 = "conc_rw_sess";
            initSess.resultUtf8 = "0";
            host.PublishSession(initSess, &err);

            std::atomic<int> readFails{0};
            std::atomic<int> listZero{0};
            std::atomic<bool> writerDone{false};

            std::thread writer([&]() {
                for (int k = 0; k < 40; ++k) {
                    CalcDisplayFrame df;
                    df.stepIndex = static_cast<std::size_t>(k);
                    df.lcdTextUtf8 = "frame_" + std::to_string(k);
                    df.resultUtf8 = std::to_string(k);
                    host.PushLcdFrame("conc_rw_sess", df, nullptr);
                }
                writerDone.store(true);
            });

            std::thread reader([&]() {
                int iters = 0;
                while (!writerDone.load() || iters < 30) {
                    CalcSessionState rs;
                    if (!studio.ReadSession("conc_rw_sess", rs, nullptr)) {
                        readFails.fetch_add(1);
                    }
                    const auto act = studio.ListActiveSessions();
                    if (act.empty()) {
                        listZero.fetch_add(1);
                    }
                    ++iters;
                }
            });

            writer.join();
            reader.join();
            CalcSessionState finalSt;
            const bool okFinal = studio.ReadSession("conc_rw_sess", finalSt, &err);
            Check(okFinal && finalSt.displayFrames.size() == 40 &&
                  readFails.load() == 0 && listZero.load() == 0,
                  "R2-03 concurrent ReadSession + PushLcdFrame + ListActiveSessions readFails==0",
                  "readFails=" + std::to_string(readFails.load()) +
                  " frames=" + std::to_string(finalSt.displayFrames.size()));
        }

        // 4c. 512 KiB same-thread SendNativePipeBytes -> RecvNativePipeBytes (R2-02)
        {
            Check(studio.StartNativePipeServer(&err), "R2-02 StartNativePipeServer", err);
            const std::string big512(512u * 1024u, 'Q');
            std::string recv512;
            const bool ok512 = host.SendNativePipeBytes(big512, &err) &&
                               studio.RecvNativePipeBytes(recv512, 5000, &err) &&
                               recv512 == big512;
            Check(ok512, "R2-02 512 KiB same-thread SendNativePipeBytes -> RecvNativePipeBytes", err);
        }

        // 4d. 24-client 128 KiB concurrent burst (R2-01)
        {
            constexpr int kClients = 24;
            std::atomic<int> sendOk{0};
            std::vector<std::thread> clients;
            clients.reserve(kClients);
            for (int c = 0; c < kClients; ++c) {
                clients.emplace_back([&, c]() {
                    std::string msg(128u * 1024u, static_cast<char>('A' + (c % 26)));
                    const std::string tag = "CID:" + std::to_string(c) + ":";
                    std::memcpy(msg.data(), tag.data(), tag.size());
                    if (host.SendNativePipeBytes(msg, nullptr)) {
                        sendOk.fetch_add(1);
                    }
                });
            }
            for (auto& th : clients) th.join();

            std::set<int> seenIds;
            bool recvAll = true;
            for (int c = 0; c < kClients; ++c) {
                std::string got;
                if (!studio.RecvNativePipeBytes(got, 5000, &err) || got.size() != 128u * 1024u) {
                    recvAll = false;
                    break;
                }
                const auto p1 = got.find(':');
                const auto p2 = (p1 != std::string::npos) ? got.find(':', p1 + 1) : std::string::npos;
                if (p1 != std::string::npos && p2 != std::string::npos) {
                    seenIds.insert(std::stoi(got.substr(p1 + 1, p2 - (p1 + 1))));
                }
            }
            Check(sendOk.load() == kClients && recvAll && static_cast<int>(seenIds.size()) == kClients,
                  "R2-01 24-client 128 KiB concurrent pipe burst (24/24)",
                  "sendOk=" + std::to_string(sendOk.load()) + " uniqueRecv=" + std::to_string(seenIds.size()));
        }

        // 4e. Close() wakes blocked RecvNativePipeBytes in < 500ms (R2-04)
        {
            std::atomic<bool> recvRet{true};
            std::atomic<long long> elapsedMs{99999};
            std::thread waiter([&]() {
                std::string tmp;
                std::string rerr;
                const auto t0 = std::chrono::steady_clock::now();
                const bool ok = studio.RecvNativePipeBytes(tmp, 3000, &rerr);
                const auto dt = std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now() - t0).count();
                recvRet.store(ok);
                elapsedMs.store(dt);
            });
            std::this_thread::sleep_for(std::chrono::milliseconds(40));
            studio.Close();
            waiter.join();
            Check(!recvRet.load() && elapsedMs.load() < 500,
                  "R2-04 Close() wakes blocked RecvNativePipeBytes in < 500ms",
                  "elapsedMs=" + std::to_string(elapsedMs.load()));
        }

        // 4f. Mid-read stalled client + Close() pipe safety (CancelIoEx + GetOverlappedResult drain)
        {
            err.clear();
            Check(studio.Open(sCfg, &err) && studio.StartNativePipeServer(&err),
                  "R3 reopen for mid-read stalled client test", err);
            const std::string pipeName = studio.NativePipeNameUtf8();

            bool clientConnected = false;
#ifdef _WIN32
            const std::wstring wName(pipeName.begin(), pipeName.end());
            HANDLE hRaw = CreateFileW(wName.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr,
                                      OPEN_EXISTING, 0, nullptr);
            if (hRaw != INVALID_HANDLE_VALUE) {
                const std::uint8_t partial[2] = {0x10u, 0x00u}; // 2 of 4 length bytes, then stall!
                DWORD written = 0;
                clientConnected = (WriteFile(hRaw, partial, 2, &written, nullptr) && written == 2);
            }
#else
            const int rawFd = ::socket(AF_UNIX, SOCK_STREAM, 0);
            if (rawFd >= 0) {
                sockaddr_un addr{};
                addr.sun_family = AF_UNIX;
                std::memcpy(addr.sun_path, pipeName.c_str(), pipeName.size() + 1);
                if (::connect(rawFd, reinterpret_cast<const sockaddr*>(&addr), sizeof(addr)) == 0) {
                    const std::uint8_t partial[2] = {0x10u, 0x00u};
                    const ssize_t wn = ::send(rawFd, partial, 2, 0);
                    clientConnected = (wn == 2);
                }
            }
#endif
            std::this_thread::sleep_for(std::chrono::milliseconds(30));
            const auto tClose0 = std::chrono::steady_clock::now();
            studio.Close();
            const auto closeMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - tClose0).count();

#ifdef _WIN32
            if (hRaw != INVALID_HANDLE_VALUE) CloseHandle(hRaw);
#else
            if (rawFd >= 0) ::close(rawFd);
#endif
            Check(clientConnected && closeMs < 500,
                  "Reviewer2-F1 mid-read stalled client + Close() drains cleanly in < 500ms",
                  "closeMs=" + std::to_string(closeMs));

            // Rapid 20x Start/Close cycle leak/deadlock guard
            bool cyclesOk = true;
            for (int k = 0; k < 20; ++k) {
                if (!studio.Open(sCfg, &err) || !studio.StartNativePipeServer(&err)) {
                    cyclesOk = false;
                    break;
                }
                studio.Close();
            }
            Check(cyclesOk, "R3 20x rapid StartNativePipeServer/Close cycles clean");
        }

        host.Close();
        studio.Close();
        UnregisterLiveSessionState("locked-sess");
        UnregisterLiveSessionState("conc_rw_sess");
        fs::remove_all(isoDir, ec);
    }
}

void TestSuite10_Iteration4Regressions()
{
    std::cout << "\n=== Suite 10: Iteration 4 Regressions (Pipe FIFO, Scale-1 Units, Target-Unit Compare, Mode Prefix) ===\n";

    // 1. Bug 1: Sequential Mixed-Size (128 KiB / 64 B) & 24x128 KiB Pipe FIFO + Concurrent Close/Send
    {
        std::error_code ec;
        const fs::path isoDir = fs::temp_directory_path(ec) /
            ("m1_bridge_r4_" + std::to_string(CurrentPid()));
        fs::remove_all(isoDir, ec);
        fs::create_directories(isoDir, ec);

        CalcIpcChannel host;
        CalcIpcChannel studio;
        CalcIpcConfig hCfg;
        hCfg.channelNameUtf8 = "r4_fifo";
        hCfg.directoryOverrideUtf8 = isoDir.string();
        hCfg.role = CalcIpcRole::CalculatorHost;
        CalcIpcConfig sCfg = hCfg;
        sCfg.role = CalcIpcRole::DocumentStudio;

        std::string err;
        Check(host.Open(hCfg, &err) && studio.Open(sCfg, &err) && studio.StartNativePipeServer(&err),
              "R4 IPC Open + StartNativePipeServer", err);

        // 1a. Mixed-size alternating 128 KiB / 64 B sequential FIFO
        bool mixOk = true;
        std::string mixDetail;
        for (int i = 0; i < 20; ++i) {
            const std::size_t sz = (i % 2 == 0) ? (128u * 1024u) : 64u;
            std::string msg(sz, static_cast<char>('A' + (i % 26)));
            const std::string tag = "MIX:" + std::to_string(i) + ":";
            std::memcpy(msg.data(), tag.data(), tag.size());
            if (!host.SendNativePipeBytes(msg, &err)) {
                mixOk = false;
                mixDetail = "send failed idx=" + std::to_string(i) + " err=" + err;
                break;
            }
        }
        if (mixOk) {
            for (int i = 0; i < 20; ++i) {
                std::string got;
                const std::size_t expSz = (i % 2 == 0) ? (128u * 1024u) : 64u;
                const std::string expTag = "MIX:" + std::to_string(i) + ":";
                if (!studio.RecvNativePipeBytes(got, 5000, &err)) {
                    mixOk = false;
                    mixDetail = "recv failed idx=" + std::to_string(i) + " err=" + err;
                    break;
                }
                if (got.size() != expSz || got.compare(0, expTag.size(), expTag) != 0) {
                    mixOk = false;
                    mixDetail = "expected idx=" + std::to_string(i) + " got='" + got.substr(0, 16) + "' sz=" + std::to_string(got.size());
                    break;
                }
            }
        }
        Check(mixOk, "R4-B1 Sequential MIX:0..19 alternating 128 KiB / 64 B preserves strict FIFO", mixDetail);

        // 1b. 5x trials of 24 x 128 KiB sequential FIFO
        bool seqTrialsOk = true;
        std::string seqDetail;
        for (int trial = 0; trial < 5 && seqTrialsOk; ++trial) {
            for (int i = 0; i < 24; ++i) {
                std::string msg(128u * 1024u, static_cast<char>('0' + (i % 10)));
                const std::string tag = "SEQ128:" + std::to_string(i) + ":";
                std::memcpy(msg.data(), tag.data(), tag.size());
                if (!host.SendNativePipeBytes(msg, &err)) {
                    seqTrialsOk = false;
                    seqDetail = "trial=" + std::to_string(trial) + " send=" + std::to_string(i);
                    break;
                }
            }
            for (int i = 0; i < 24 && seqTrialsOk; ++i) {
                std::string got;
                const std::string expTag = "SEQ128:" + std::to_string(i) + ":";
                if (!studio.RecvNativePipeBytes(got, 5000, &err) ||
                    got.size() != 128u * 1024u ||
                    got.compare(0, expTag.size(), expTag) != 0) {
                    seqTrialsOk = false;
                    seqDetail = "trial=" + std::to_string(trial) + " expected=" + expTag + " got=" + got.substr(0, 16);
                    break;
                }
            }
        }
        Check(seqTrialsOk, "R4-B1 5x trials of 24x128 KiB sequential FIFO pass 100%", seqDetail);

        // 1c. Concurrent Close() + SendNativePipeBytes() race safety
        {
            std::thread sender([&]() {
                for (int k = 0; k < 50; ++k) {
                    std::string serr;
                    host.SendNativePipeBytes("race_ping", &serr);
                }
            });
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
            host.Close();
            sender.join();
            Check(!host.IsOpen(), "R4-B1-Caveat concurrent Close() + SendNativePipeBytes() clean");
        }

        host.Close();
        studio.Close();
        fs::remove_all(isoDir, ec);
    }

    // 2. Bug 2: TranslateExpressionToCasioKeys Scale-1 SI Unit Elision
    {
        struct UnitCase {
            const char* expr;
            double expected;
        };
        const UnitCase cases[] = {
            {"10 m/s | km/h", 36.0},
            {"10 m / s^2", 10.0},
            {"5 * m", 5.0},
            {"10 m / 2 s", 5.0},
            {"(10 m)/(2 s) | km/h", 18.0},
            {"s^2", 1.0},
            {"(2 s)^2", 4.0},
            {"10 m^2", 10.0}
        };
        for (const auto& uc : cases) {
            const auto plan = TranslateExpressionToCasioKeys(uc.expr, "fx-991CW");
            const std::string recon = KeySequenceToExpressionUtf8(plan.keySequenceUtf8);
            const auto ev = Cas::EvaluateExpression(recon);
            const bool ok = plan.ok && ev.ok && ev.result.IsReal() &&
                            std::abs(ev.result.value.real() - uc.expected) < 1e-4;
            Check(ok, std::string("R4-B2 scale-1 SI unit keys evaluate cleanly: ") + uc.expr,
                  "keys='" + plan.keySequenceUtf8 + "' recon='" + recon + "' res='" + ev.formattedResultUtf8 + "'");
        }
    }

    // 3. Bug 3: CompareCalculatorModels Target-Unit Scaling/Offset
    {
        const auto repMv = CompareCalculatorModels("4.7 kOhm * 2 mA | mV");
        Check(repMv.ok && repMv.casReferenceResultUtf8 == "9400 mV" &&
              repMv.hasCasReferenceNumeric && std::abs(repMv.casReferenceNumeric - 9400.0) < 1e-6 &&
              repMv.allModelsSupported && repMv.allResultsAgreeWithinTol &&
              repMv.models.size() == 4 && repMv.models[1].displayResultUtf8 == "9400",
              "R4-B3 CompareCalculatorModels('4.7 kOhm * 2 mA | mV') agrees at 9400 mV",
              repMv.diffSummaryUtf8);

        const auto repDegC = CompareCalculatorModels("100 degC | degC");
        Check(repDegC.ok && repDegC.casReferenceResultUtf8 == "100 degC" &&
              repDegC.hasCasReferenceNumeric && std::abs(repDegC.casReferenceNumeric - 100.0) < 1e-6 &&
              repDegC.allModelsSupported && repDegC.allResultsAgreeWithinTol &&
              repDegC.models.size() == 4 && repDegC.models[1].displayResultUtf8 == "100",
              "R4-B3 CompareCalculatorModels('100 degC | degC') agrees at 100 degC",
              repDegC.diffSummaryUtf8);

        const auto repDegF = CompareCalculatorModels("100 degC | degF");
        Check(repDegF.ok && repDegF.hasCasReferenceNumeric &&
              std::abs(repDegF.casReferenceNumeric - 212.0) < 1e-6 &&
              repDegF.allResultsAgreeWithinTol,
              "R4-B3 CompareCalculatorModels('100 degC | degF') agrees at 212 degF",
              repDegF.diffSummaryUtf8);
    }

    // 4. Bug 4: KeySequenceToExpressionUtf8 Mode-Prefix Stripper
    {
        const std::string e1 = KeySequenceToExpressionUtf8("[MENU] [1] (Calcular) (2+3) [=]");
        const auto r1 = Cas::EvaluateExpression(e1);
        Check(e1 == "(2+3)" && r1.ok && std::abs(r1.result.value.real() - 5.0) < 1e-9,
              "R4-B4 [MENU] [1] (Calcular) (2+3) [=] preserves (2+3) -> 5", "expr='" + e1 + "'");

        const std::string e2 = KeySequenceToExpressionUtf8("[MENU] [1] (Calcular) [(] 2 [+] 3 [)] [=]");
        const auto r2 = Cas::EvaluateExpression(e2);
        Check(e2 == "(2+3)" && r2.ok && std::abs(r2.result.value.real() - 5.0) < 1e-9,
              "R4-B4 [MENU] [1] (Calcular) [(] 2 [+] 3 [)] [=] -> (2+3) == 5", "expr='" + e2 + "'");

        const std::string e3 = KeySequenceToExpressionUtf8("[MENU] [3] 2 [+] 3 [=]");
        const auto r3 = Cas::EvaluateExpression(e3);
        Check(e3 == "2+3" && r3.ok && std::abs(r3.result.value.real() - 5.0) < 1e-9,
              "R4-B4 [MENU] [3] 2 [+] 3 [=] preserves 2+3 -> 5", "expr='" + e3 + "'");

        const std::string e4 = KeySequenceToExpressionUtf8("[MENU] [1] (2+3) [=]");
        Check(e4 == "(2+3)", "R4-B4 [MENU] [1] (2+3) [=] does not swallow (2+3)", "expr='" + e4 + "'");
    }
}

} // namespace

int main()
{
    std::cout << std::unitbuf;
    TestSuite1_BackwardCompatibility();
    TestSuite2_FourModelTranslation();
    TestSuite3_SessionCardPngAndReplay();
    TestSuite4_BidirectionalLocalIpc();
    TestSuite5_ModelComparison();
    TestSuite6_KeyLogPresenterAndShiftAlpha();
    TestSuite7_U13EighteenCaseDifferentialMatrix();
    TestSuite8_Iteration2Regressions();
    TestSuite9_Iteration3Regressions();
    TestSuite10_Iteration4Regressions();

    std::cout << "\n=== m1_bridge_test Summary: passed=" << g_passed
              << " failed=" << g_failed << " ===\n";
    return (g_failed == 0 && g_passed > 0) ? 0 : 1;
}
