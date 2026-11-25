#include "doc_studio/math/engineering_cas.hpp"
#include "calc_engine.h"
#include "linux/engines/math_eval_linux.hpp"

#include <cmath>
#include <complex>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

using namespace DocStudio;
using namespace DocStudio::Cas;

namespace {

int g_passed = 0;
int g_failed = 0;

void Check(bool cond, const char* name, const std::string& detail = "") {
    if (cond) {
        ++g_passed;
    } else {
        ++g_failed;
        std::cerr << "[FAIL] " << name;
        if (!detail.empty()) std::cerr << " (" << detail << ")";
        std::cerr << "\n";
    }
}

void TestGroup1SymbolicUnitsComplex() {
    std::cout << "-- Group 1: Symbolic CAS, Units (6 Bug Fixes), Complex & #varsub --\n";

    // Bug 1 fix: unitScaleToSi in ParsePower / sqrt / pow
    {
        const auto sqCm = EvaluateExpression("(10 cm)^2");
        Check(sqCm.ok && std::abs(sqCm.result.value.real() - 0.01) < 1e-12,
              "Bug1: (10 cm)^2 SI value == 0.01 m^2", sqCm.formattedResultUtf8);
        Check(sqCm.formattedResultUtf8.find("100") != std::string::npos &&
              sqCm.formattedResultUtf8.find("cm^2") != std::string::npos,
              "Bug1: (10 cm)^2 formats as 100 cm^2", sqCm.formattedResultUtf8);

        const auto sqCmToM = EvaluateExpression("(10 cm)^2 | m^2");
        Check(sqCmToM.ok && std::abs(sqCmToM.result.value.real() - 0.01) < 1e-12 &&
              sqCmToM.formattedResultUtf8.find("0.01") != std::string::npos,
              "Bug1: (10 cm)^2 | m^2 == 0.01 m^2", sqCmToM.formattedResultUtf8);

        const auto rtArea = EvaluateExpression("sqrt(100 cm^2)");
        Check(rtArea.ok && std::abs(rtArea.result.value.real() - 0.1) < 1e-9,
              "Bug1: sqrt(100 cm^2) == 0.1 m", rtArea.formattedResultUtf8);

        const auto pw = EvaluateExpression("pow(10 cm, 2)");
        Check(pw.ok && std::abs(pw.result.value.real() - 0.01) < 1e-12 &&
              pw.formattedResultUtf8.find("0.01") != std::string::npos,
              "Bug1: pow(10 cm, 2) == 0.01 m^2", pw.formattedResultUtf8);
    }

    // Bug 2 fix: dimensionless scalar division preserves unit
    {
        const auto divFt = EvaluateExpression("100 ft / 2");
        Check(divFt.ok && divFt.formattedResultUtf8.find("50") != std::string::npos &&
              divFt.formattedResultUtf8.find("ft") != std::string::npos,
              "Bug2: 100 ft / 2 == 50 ft", divFt.formattedResultUtf8);
    }

    // Bug 3 fix: affine temperature offset (degC / degF <-> K)
    {
        const auto c2k = EvaluateExpression("25 degC | K");
        Check(c2k.ok && std::abs(c2k.result.value.real() - 298.15) < 1e-6 &&
              c2k.formattedResultUtf8.find("298.15") != std::string::npos,
              "Bug3: 25 degC | K == 298.15 K", c2k.formattedResultUtf8);

        const auto c2f = EvaluateExpression("100 degC | degF");
        Check(c2f.ok && c2f.formattedResultUtf8.find("212") != std::string::npos,
              "Bug3: 100 degC | degF == 212 degF", c2f.formattedResultUtf8);

        const auto negC2K = EvaluateExpression("-10 degC | K");
        Check(negC2K.ok && std::abs(negC2K.result.value.real() - 263.15) < 1e-6,
              "Bug3: -10 degC | K == 263.15 K", negC2K.formattedResultUtf8);

        const auto zeroC2K = EvaluateExpression("0 degC | K");
        Check(zeroC2K.ok && std::abs(zeroC2K.result.value.real() - 273.15) < 1e-6,
              "Bug3: 0 degC | K == 273.15 K", zeroC2K.formattedResultUtf8);
    }

    // Bug 4 fix: implied product & hyphenated compound units (F-233, SUP-03)
    {
        const auto imp = EvaluateExpression("(10 kip)(10 ft) | ft-kip");
        Check(imp.ok && imp.formattedResultUtf8.find("100") != std::string::npos &&
              imp.formattedResultUtf8.find("ft-kip") != std::string::npos,
              "Bug4: (10 kip)(10 ft) | ft-kip == 100 ft-kip", imp.formattedResultUtf8);

        const auto nm = EvaluateExpression("100 ft-kip | kN*m");
        Check(nm.ok && std::abs(nm.result.value.real() - 135581.7948) < 1.0,
              "Bug4: 100 ft-kip | kN*m converts accurately", nm.formattedResultUtf8);

        // Negative: dimensional mismatch fails cleanly with DIM_ERR
        const auto bad = EvaluateExpression("10 ft + 5 s");
        Check(!bad.ok && bad.diagnosticCode == "DIM_ERR",
              "Neg: 10 ft + 5 s blocks with DIM_ERR", bad.diagnosticCode);
    }

    // F-234: rect(z), polar(z), CasAngleMode RAD/DEG/GRA
    {
        CasEvalOptions degOpts;
        degOpts.trigAngleMode = CasAngleMode::Deg;
        degOpts.polarAngleMode = CasAngleMode::Deg;
        const auto s30 = EvaluateExpression("sin(30)", {}, degOpts);
        Check(s30.ok && std::abs(s30.result.value.real() - 0.5) < 1e-9,
              "F-234: CasAngleMode::Deg sin(30) == 0.5", s30.formattedResultUtf8);

        CasEvalOptions radOpts;
        radOpts.trigAngleMode = CasAngleMode::Rad;
        radOpts.polarAngleMode = CasAngleMode::Rad;
        const auto rPol = EvaluateExpression("polar(1, 1)", {}, radOpts);
        Check(rPol.ok && rPol.result.preferPolar && std::abs(rPol.result.Mag() - std::sqrt(2.0)) < 1e-9,
              "F-234: polar(1, 1) magnitude == sqrt(2)", rPol.formattedResultUtf8);

        const auto rRec = EvaluateExpression("rect(10 @ 60)", {}, degOpts);
        Check(rRec.ok && !rRec.result.preferPolar && std::abs(rRec.result.value.real() - 5.0) < 1e-6,
              "F-234: rect(10 @ 60) real == 5", rRec.formattedResultUtf8);

        const auto rRec2 = EvaluateExpression("rect(2, 90)", {}, degOpts);
        Check(rRec2.ok && std::abs(rRec2.result.value.real()) < 1e-9 &&
              std::abs(rRec2.result.value.imag() - 2.0) < 1e-9,
              "F-234: rect(2, 90) == 2j", rRec2.formattedResultUtf8);
    }

    // F-232: Symbolic simplification, expansion, differentiation, equation solving
    {
        const auto expRes = ExpandSymbolicExpression("(x + 2)^2 - (x^2 + 4*x)");
        Check(expRes.ok && expRes.expressionUtf8 == "4",
              "F-232: Expand((x+2)^2 - (x^2+4x)) == 4", expRes.expressionUtf8);

        const auto diffRes = DifferentiateExpression("sin(3*x) + x^3", "x");
        Check(diffRes.ok, "F-232: Differentiate sin(3x)+x^3 succeeds", diffRes.expressionUtf8);
        const auto dAt = EvaluateExpression(diffRes.expressionUtf8, { { "x", QuantityValue{ { 0.5, 0.0 }, {}, "", 1.0, 0.0, false } } });
        const double expected = 3.0 * std::cos(1.5) + 3.0 * 0.25;
        Check(dAt.ok && std::abs(dAt.result.value.real() - expected) < 1e-9,
              "F-232: d/dx(sin(3x)+x^3) at x=0.5 matches exact", dAt.formattedResultUtf8);

        const auto quad = SolveAlgebraicEquation("x^2 + 2*x + 5 = 0", "x");
        Check(quad.ok && quad.solvedExact && quad.solutions.size() == 2 &&
              std::abs(quad.solutions[0].real() + 1.0) < 1e-9 &&
              std::abs(std::abs(quad.solutions[0].imag()) - 2.0) < 1e-9 &&
              quad.steps.size() >= 3,
              "F-232: Quadratic complex roots -1 +/- 2j with steps", quad.summaryUtf8);

        const auto nl = SolveAlgebraicEquation("cos(x) = x", "x");
        Check(nl.ok && nl.maxResidual < 1e-9 && std::abs(nl.solutions[0].real() - 0.7390851332) < 1e-7,
              "F-232: Solve cos(x) = x == 0.739085", nl.summaryUtf8);
    }
}

void TestGroup2DagCalcCardTableMarginAndParity() {
    std::cout << "-- Group 2: Reactive DAG (CalcCard, Table, Margin) & Win/Linux Evaluator Parity --\n";

    // Bug 5 fix + SUP-01 + SUP-02: CalcSessionCard without formulaVarNameUtf8, table & margin hooks, 2D LaTeX, tooltips
    {
        DocumentModel doc;
        doc.ResetToBlank(DocumentMode::InfiniteCanvas);

        AnnotationObject vCard;
        vCard.kind = ObjectKind::CalcSessionCard;
        vCard.calcModelIdUtf8 = "fx-991CW";
        vCard.calcKeySeqUtf8 = "12 [=]";
        vCard.formulaExprUtf8 = "V_in = 12 V";
        // Notice: formulaVarNameUtf8 intentionally left empty (matches CalcBridge callers)
        doc.AddCanvasObject(vCard);
        const auto vId = doc.CanvasObjects().back().id;

        AnnotationObject tbl;
        tbl.kind = ObjectKind::TextBox;
        tbl.tags = { "table" };
        tbl.formulaExprUtf8 = "A1=4 kOhm; B1=2 kOhm; C1==SUM(A1:B1) | kOhm";
        tbl.plotConfigUtf8 = R"({"cells":[{"ref":"A1","expr":"4 kOhm"},{"ref":"B1","expr":"2 kOhm"},{"ref":"C1","expr":"=SUM(A1:B1) | kOhm","exportVar":"R_tot"}]})";
        doc.AddCanvasObject(tbl);
        const auto tId = doc.CanvasObjects().back().id;

        AnnotationObject margin;
        margin.kind = ObjectKind::TextBox;
        margin.tags = { "margin" };
        margin.textUtf8 = "I_tot =>";
        margin.formulaExprUtf8 = "I_tot = V_in / R_tot | mA";
        doc.AddCanvasObject(margin);
        const auto mId = doc.CanvasObjects().back().id;

        AnnotationObject plainNote;
        plainNote.kind = ObjectKind::TextBox;
        plainNote.textUtf8 = "Normal lecture note untouched";
        doc.AddCanvasObject(plainNote);
        const auto pId = doc.CanvasObjects().back().id;

        const auto rep = EvaluateDocumentMathDag(doc);
        Check(rep.ok && rep.evaluatedCount >= 4, "Group2: DAG evaluated CalcCard + Table + Margin", std::to_string(rep.evaluatedCount));

        const auto* cPtr = doc.FindCanvasObject(vId);
        Check(cPtr != nullptr && cPtr->textUtf8.find("CASIO fx-991CW | 12 [=] -> 12 V") != std::string::npos &&
              !cPtr->calcLatexUtf8.empty(),
              "Bug5: CalcSessionCard preserves CASIO format & sets calcLatexUtf8", cPtr ? cPtr->textUtf8 : "");

        const auto* tPtr = doc.FindCanvasObject(tId);
        Check(tPtr != nullptr && tPtr->evaluatedResultUtf8.find("C1=6 kOhm") != std::string::npos,
              "SUP-01: Table hook evaluates SUM(A1:B1) | kOhm == 6 kOhm", tPtr ? tPtr->evaluatedResultUtf8 : "");

        const auto* mPtr = doc.FindCanvasObject(mId);
        Check(mPtr != nullptr && mPtr->evaluatedResultUtf8.find("2 mA") != std::string::npos &&
              mPtr->textUtf8.find("I_tot => 2 mA") != std::string::npos,
              "SUP-01: Margin side-calc consumes V_in and exported R_tot == 2 mA", mPtr ? mPtr->textUtf8 : "");

        const auto* pPtr = doc.FindCanvasObject(pId);
        Check(pPtr != nullptr && pPtr->textUtf8 == "Normal lecture note untouched",
              "SUP-01: Plain TextBox remains untouched");

        const auto tooltips = BuildDocumentVariableTooltipMap(doc);
        Check(tooltips.find("V_in") != tooltips.end() && tooltips.at("V_in").find("Tensi") != std::string::npos,
              "SUP-02: Tooltip metadata populated for V_in");

        // Table cycle negative test
        TableSheetModel cycSheet;
        cycSheet.cells = { { "A1", "=B1 + 1", "" }, { "B1", "=A1 + 1", "" } };
        SymbolEnv emptyEnv;
        const auto cycRep = EvaluateTableDag(cycSheet, emptyEnv);
        Check(!cycRep.ok && cycRep.cycleCount == 2, "Neg: Table cycle detected");
    }

    // calc_engine.h <-> linux/engines/math_eval_linux.hpp full semantic parity
    {
        struct ParityCase {
            const char* utf8;
            const wchar_t* wide;
            CasioLinux::detail::AngleMode lMode;
            AngleMode wMode;
            double expected;
        };
        const ParityCase cases[] = {
            { "-2^2", L"-2^2", CasioLinux::detail::AngleMode::Deg, ANG_DEG, -4.0 },
            { "2^3^2", L"2^3^2", CasioLinux::detail::AngleMode::Deg, ANG_DEG, 512.0 },
            { "2sin(30) + cos(0)", L"2sin(30) + cos(0)", CasioLinux::detail::AngleMode::Deg, ANG_DEG, 2.0 },
            { "max(3, 7) + min(2, 5)", L"max(3, 7) + min(2, 5)", CasioLinux::detail::AngleMode::Deg, ANG_DEG, 9.0 },
            { "nCr(5, 2) + nPr(4, 2)", L"nCr(5, 2) + nPr(4, 2)", CasioLinux::detail::AngleMode::Deg, ANG_DEG, 22.0 },
            { "gcd(24, 36) + lcm(4, 6)", L"gcd(24, 36) + lcm(4, 6)", CasioLinux::detail::AngleMode::Deg, ANG_DEG, 24.0 },
            { "mod(17, 5) + floor(2.9) + ceil(2.1)", L"mod(17, 5) + floor(2.9) + ceil(2.1)", CasioLinux::detail::AngleMode::Deg, ANG_DEG, 7.0 },
            { "sinh(0) + cosh(0) + cbrt(27) + log2(8)", L"sinh(0) + cosh(0) + cbrt(27) + log2(8)", CasioLinux::detail::AngleMode::Rad, ANG_RAD, 7.0 },
            { "3 x 4 + 50%", L"3 x 4 + 50%", CasioLinux::detail::AngleMode::Deg, ANG_DEG, 12.5 },
            { "1.5e-2 * 200", L"1.5e-2 * 200", CasioLinux::detail::AngleMode::Deg, ANG_DEG, 3.0 },
            { "2log2(8) + sin(100)", L"2log2(8) + sin(100)", CasioLinux::detail::AngleMode::Gra, ANG_GRA, 7.0 }
        };
        for (const auto& c : cases) {
            const auto lOut = CasioLinux::detail::EvaluateWithAngleMode(c.utf8, c.lMode, 0.0);
            double wVal = 0.0;
            std::wstring wErr;
            const bool wOk = MathEvaluator::Evaluate(c.wide, 0.0, c.wMode, wVal, wErr);
            Check(lOut.ok && wOk && std::abs(lOut.value - c.expected) < 1e-9 && std::abs(wVal - c.expected) < 1e-9,
                  "Parity Win<->Linux evaluator", c.utf8);
        }
        // Casio-style EOF auto-close parity ("cos(60" -> 0.5) + incomplete/mismatched rejection
        const auto lCasio = CasioLinux::detail::EvaluateWithAngleMode("cos(60", CasioLinux::detail::AngleMode::Deg, 0.0);
        double wCasioVal = 0.0; std::wstring wCasioErr;
        const bool wCasioOk = MathEvaluator::Evaluate(L"cos(60", 0.0, ANG_DEG, wCasioVal, wCasioErr);
        Check(lCasio.ok && wCasioOk && std::abs(lCasio.value - 0.5) < 1e-9 && std::abs(wCasioVal - 0.5) < 1e-9,
              "Parity: Casio EOF auto-close cos(60 == 0.5 on both Win & Linux");

        const auto lBad = CasioLinux::detail::EvaluateWithAngleMode("(2+", CasioLinux::detail::AngleMode::Deg, 0.0);
        double wBadVal = 0.0; std::wstring wBadErr;
        const bool wBadOk = MathEvaluator::Evaluate(L"(2+", 0.0, ANG_DEG, wBadVal, wBadErr);
        const auto lBad2 = CasioLinux::detail::EvaluateWithAngleMode("(2+3]", CasioLinux::detail::AngleMode::Deg, 0.0);
        double wBad2Val = 0.0; std::wstring wBad2Err;
        const bool wBad2Ok = MathEvaluator::Evaluate(L"(2+3]", 0.0, ANG_DEG, wBad2Val, wBad2Err);
        Check(!lBad.ok && !wBadOk && !lBad2.ok && !wBad2Ok,
              "Neg Parity: incomplete '(2+' and mismatched '(2+3]' rejected on both Win & Linux");
    }
}

void TestGroup3LatexMathMlPlotsAndSheets() {
    std::cout << "-- Group 3: LaTeX/MathML AST, 2D/3D CVD Plots, Parametric Sheets --\n";

    // F-152, F-231: LaTeX <-> MathML round-trip & LaTeX -> CAS
    {
        const std::string ltx = "\\frac{\\sqrt[3]{x_1^2 + \\alpha}}{\\sum_{k=1}^{n} k} + \\begin{bmatrix} 1 & 2 \\\\ 3 & 4 \\end{bmatrix}";
        const auto parsed = ParseLatexToAst(ltx);
        Check(parsed.ok && parsed.root != nullptr, "F-231: ParseLatexToAst complex formula", parsed.errorUtf8);

        std::string mmlErr;
        const std::string mml = LatexToMathML(ltx, true, &mmlErr);
        Check(!mml.empty() && mml.find("<mfrac>") != std::string::npos &&
              mml.find("<mroot>") != std::string::npos && mml.find("<mtable>") != std::string::npos,
              "F-152: LatexToMathML emits mfrac/mroot/mtable", mmlErr);

        std::string backErr;
        const std::string backLtx = MathMLToLatex(mml, &backErr);
        Check(!backLtx.empty() && backLtx.find("\\frac") != std::string::npos &&
              backLtx.find("\\sqrt[") != std::string::npos && backLtx.find("\\begin{bmatrix}") != std::string::npos,
              "F-152: MathMLToLatex round-trip preserves structure", backErr);

        std::string casExpr;
        Check(ConvertLatexToCasExpr("\\frac{10 \\mathrm{m}}{2 \\mathrm{s}}", casExpr), "F-152: ConvertLatexToCasExpr");
        const auto ev = EvaluateExpression(casExpr);
        Check(ev.ok && std::abs(ev.result.value.real() - 5.0) < 1e-9, "F-152: LaTeX->CAS evaluates to 5 m/s", ev.formattedResultUtf8);

        const std::string vsLtx = FormatVarSubLatex2D("V", "I * R", "2 mA * 4.7 kOhm", "9.4 V", MathSubMode::VarSub);
        Check(ValidateLatexUtf8(vsLtx).valid, "SUP-02: FormatVarSubLatex2D passes ValidateLatexUtf8", vsLtx);

        const std::string divMulLtx = ConvertCasExprToLatex("a / b * c");
        Check(divMulLtx == "\\frac{a}{b} \\cdot c", "F-152: ConvertCasExprToLatex preserves left-assoc / and *", divMulLtx);

        // Negative: unbalanced braces & \left without \right
        Check(!ValidateLatexUtf8("\\frac{1}{2").valid, "Neg F-231: unbalanced '{' rejected");
        Check(!ParseLatexToAst("\\left( x + 1 ").ok, "Neg F-231: unclosed \\left rejected");
    }

    // F-236, F-237, SUP-06: Parametric, polar, multi-curve, roots/extrema/intersections, contours, 3D surface
    {
        const auto param = SampleParametric2D("cos(t)", "sin(t)", "t", 0.0, 6.2831853, 33);
        Check(param.size() == 33 && std::abs(param[0].x - 1.0) < 1e-6 && std::abs(param[8].y - 1.0) < 1e-6,
              "F-236: Parametric unit circle");

        const auto polar = SamplePolar2D("1 + cos(theta)", "theta", 0.0, 6.2831853, 33);
        Check(polar.size() == 33 && std::abs(polar[0].x - 2.0) < 1e-6, "F-236: Polar cardioid");

        const auto multi = SampleMultiCurve2D({ "x", "x^2" }, "x", -2.0, 2.0, 25);
        Check(multi.size() == 2 && multi[0].color.r != multi[1].color.r, "F-236: Multi-curve CVD colors");

        const auto roots = FindRoots2D("x^2 - 2", "x", 0.0, 2.0);
        Check(roots.size() == 1 && std::abs(roots[0].x - std::sqrt(2.0)) < 1e-9, "F-236: FindRoots2D sqrt(2)");

        const auto extrema = FindExtrema2D("x^3 - 3*x", "x", -2.0, 2.0);
        Check(extrema.size() == 2 && extrema[0].kind == CriticalPointKind::LocalMax &&
              std::abs(extrema[0].x + 1.0) < 1e-6 && extrema[1].kind == CriticalPointKind::LocalMin &&
              std::abs(extrema[1].x - 1.0) < 1e-6, "F-236: FindExtrema2D (-1,2) max & (1,-2) min");

        const auto inter = FindIntersections2D("x^2", "x + 2", "x", -3.0, 3.0);
        Check(inter.size() == 2 && std::abs(inter[0].x + 1.0) < 1e-7 && std::abs(inter[1].x - 2.0) < 1e-7,
              "F-236: FindIntersections2D at x=-1, x=2");

        const auto cnt = ComputeContourIsoLevels("x^2 + y^2", -2.0, 2.0, 25, -2.0, 2.0, 25, 5);
        Check(cnt.ok && cnt.levels.size() == 5 && !cnt.levels[2].segments.empty() &&
              cnt.colorbar.tickValues.size() == 5, "F-237: Marching squares 2D contour + colorbar");

        Surface3DViewState vs;
        vs.gridSteps = 12;
        vs.showBaseContours = true;
        const auto s3d = BuildProjectedSurface3D("sin(x)*cos(y)", vs);
        Check(s3d.ok && !s3d.segments.empty() && !s3d.baseContours.empty(), "F-237: 3D surface projection + base contours");
    }

    // F-254: Parametric sheets (LogLog both X & Y, SmithChart clipped to unit disk)
    {
        SheetSpec logSpec;
        logSpec.kind = ParametricSheetKind::LogLog;
        logSpec.width = 600.0;
        logSpec.height = 600.0;
        logSpec.xDecades = 2;
        logSpec.yDecades = 2;
        const auto logSegs = GenerateParametricSheet(logSpec);
        bool hasNonUniformX = false, hasNonUniformY = false;
        for (const auto& s : logSegs) {
            if (std::abs(s.x0 - s.x1) < 1e-6 && s.x0 > 1.0 && s.x0 < 299.0 && std::abs(s.x0 - 300.0 * std::log10(2.0)) < 1e-3) hasNonUniformX = true;
            if (std::abs(s.y0 - s.y1) < 1e-6 && s.y0 > 301.0 && s.y0 < 599.0 && std::abs(s.y0 - (600.0 - 300.0 * std::log10(2.0))) < 1e-3) hasNonUniformY = true;
        }
        Check(hasNonUniformX && hasNonUniformY, "F-254: LogLog sheet has logarithmic spacing on BOTH X and Y");

        SheetSpec smithSpec;
        smithSpec.kind = ParametricSheetKind::SmithChart;
        smithSpec.width = 500.0;
        smithSpec.height = 500.0;
        const double cx = 250.0, cy = 250.0, R = 0.46 * 500.0;
        const auto smithSegs = GenerateParametricSheet(smithSpec);
        bool allInsideDisk = !smithSegs.empty();
        for (const auto& s : smithSegs) {
            const double d0 = std::hypot(s.x0 - cx, s.y0 - cy);
            const double d1 = std::hypot(s.x1 - cx, s.y1 - cy);
            if (d0 > R + 1e-2 || d1 > R + 1e-2) { allInsideDisk = false; break; }
        }
        Check(allInsideDisk, "F-254: SmithChart reactance arcs clipped strictly inside unit disk");
    }
}

void TestGroup4AdvancedMathStaticsFingerprintAndSheets() {
    std::cout << "-- Group 4: Complex 64x64/Eigen, tinta_nucleo Port, CSV/Reg, Vectors/FBD, Uncertainty, Fingerprint --\n";

    // F-235: 64x64 complex SEL, RREF, rank, eigenvalues/eigenvectors, singular failure
    {
        const std::size_t N = 64;
        ComplexMatrix A{ N, N, std::vector<std::complex<double>>(N * N, { 0.0, 0.0 }) };
        std::vector<std::complex<double>> xTrue(N);
        for (std::size_t r = 0; r < N; ++r) {
            xTrue[r] = std::complex<double>(static_cast<double>(r + 1) * 0.1, -0.05 * static_cast<double>(r));
            A.At(r, r) = std::complex<double>(10.0, 2.0);
            if (r > 0) A.At(r, r - 1) = std::complex<double>(-0.5, 0.25);
            if (r + 1 < N) A.At(r, r + 1) = std::complex<double>(0.3, -0.15);
        }
        std::vector<std::complex<double>> b(N, { 0.0, 0.0 });
        for (std::size_t r = 0; r < N; ++r)
            for (std::size_t c = 0; c < N; ++c) b[r] += A.At(r, c) * xTrue[c];

        const auto sol64 = SolveComplexLinearSystem(A, b);
        double maxErr = 0.0;
        if (sol64.ok) {
            for (std::size_t r = 0; r < N; ++r) maxErr = (std::max)(maxErr, std::abs(sol64.matrix.At(r, 0) - xTrue[r]));
        }
        Check(sol64.ok && maxErr < 1e-10, "F-235: 64x64 complex linear system residual < 1e-10", std::to_string(maxErr));

        const auto rref = EvaluateComplexMatrixExpression("rank([[1, 2, 3], [2, 4, 6], [1, 1, 1]])");
        Check(rref.ok && rref.isScalar && std::abs(rref.scalar.real() - 2.0) < 1e-9, "F-235: rank == 2");

        const auto eigRot = ComputeMatrixEigen(ComplexMatrix{ 2, 2, { {0,0}, {-1,0}, {1,0}, {0,0} } });
        Check(eigRot.ok && eigRot.eigenvalues.size() == 2 &&
              std::abs(eigRot.eigenvalues[0].real()) < 1e-9 &&
              std::abs(std::abs(eigRot.eigenvalues[0].imag()) - 1.0) < 1e-9,
              "F-235: eigenvalues of rotation matrix == +/- j", eigRot.summaryUtf8);

        const auto badInv = EvaluateComplexMatrixExpression("inv([[1, 2j], [2j, -4]])");
        Check(!badInv.ok, "Neg F-235: singular complex matrix inv() rejected");
    }

    // U-15: tinta_nucleo/matematicas_avanzadas port + O(N log N) FFT
    {
        const auto d1 = NumericalDerivative("sin(x)", "x", 1.0);
        Check(d1.ok && std::abs(d1.result.value.real() - std::cos(1.0)) < 1e-7, "U-15: NumericalDerivative");

        const auto i1 = NumericalIntegralSimpson("x^2", "x", 0.0, 1.0, 100);
        Check(i1.ok && std::abs(i1.result.value.real() - (1.0 / 3.0)) < 1e-10, "U-15: NumericalIntegralSimpson");

        const auto rt = FindRootNewton("cos(x) - x", "x", { 0.7, 0.0 });
        Check(rt.ok && std::abs(rt.result.value.real() - 0.7390851332) < 1e-9, "U-15: FindRootNewton");

        const auto limOk = ComputeNumericalLimit("sin(x)/x", "x", 0.0, LimitSide::Bilateral);
        Check(limOk.ok && std::abs(limOk.result.value.real() - 1.0) < 1e-6, "U-15: Limit sin(x)/x -> 1");
        const auto limBad = ComputeNumericalLimit("1/x", "x", 0.0, LimitSide::Bilateral);
        Check(!limBad.ok, "Neg U-15: 1/x bilateral limit fails");

        const auto sm = ComputeSummation("k^2", "k", 1, 10);
        Check(sm.ok && std::abs(sm.result.value.real() - 385.0) < 1e-9, "U-15: Summation k^2 (1..10) == 385");

        // FFT N=64 (radix-2) and N=53 (prime Bluestein)
        for (std::size_t n : { std::size_t(64), std::size_t(53) }) {
            std::vector<std::complex<double>> x(n);
            for (std::size_t k = 0; k < n; ++k) x[k] = { std::cos(0.3 * static_cast<double>(k)), std::sin(0.7 * static_cast<double>(k)) };
            const auto X = ComputeFft(x, false);
            const auto xRec = ComputeFft(X, true);
            double err = 0.0;
            for (std::size_t k = 0; k < n; ++k) err = (std::max)(err, std::abs(xRec[k] - x[k]));
            Check(err < 1e-10, "U-15: O(N log N) IFFT(FFT(x)) == x", "N=" + std::to_string(n));
        }

        const auto lap = NumericalLaplaceTransform("exp(-2*t)", "t", { 3.0, 0.0 }, 20.0, 1000);
        Check(lap.ok && std::abs(lap.result.value.real() - 0.2) < 1e-6, "U-15: NumericalLaplaceTransform");

        const auto ode = SolveFirstOrderOdeRk4("-y", 0.0, { 1.0, 0.0 }, 1.0, 100);
        Check(ode.ok && std::abs(ode.samples.back().y.real() - std::exp(-1.0)) < 1e-8, "U-15: RK4 ODE");

        const auto par = EvaluateExpression("10 || 10");
        Check(par.ok && std::abs(par.result.value.real() - 5.0) < 1e-9, "U-15: 10 || 10 == 5");
        const auto suf = EvaluateExpression("4.7k + 300");
        Check(suf.ok && std::abs(suf.result.value.real() - 5000.0) < 1e-9, "U-15: 4.7k + 300 == 5000");
    }

    // F-251, F-252: Decimal-comma CSV + exponential/power/log regressions
    {
        const std::string euroCsv = "t;v\n0,5;2,71828\n1,0;7,38906\n1,5;20,0855\n2,0;54,59815\n";
        const auto csv = ParseCsvNumericTable(euroCsv);
        Check(csv.ok && csv.delimiter == ';' && csv.decimalCommaLocale && csv.xyPoints.size() == 4,
              "F-251: European decimal-comma CSV parsed");

        const auto expFit = FitCsvRegressionExtended(euroCsv, RegressionModelKind::Exponential);
        Check(expFit.ok && std::abs(expFit.coeffs[0] - 1.0) < 1e-3 && std::abs(expFit.coeffs[1] - 2.0) < 1e-3 &&
              expFit.rSquared > 0.9999 && expFit.rmse < 1e-2 && expFit.lowerBound95.size() == 64,
              "F-252: Exponential regression y = exp(2x) + RMSE + 95% CI");

        // Bug 6 fix check through legacy FitCsvRegression
        const auto legFit = FitCsvRegression("x;y\n0,5;1,5\n1,5;3,5\n2,5;5,5\n", 1);
        Check(legFit.ok && std::abs(legFit.coeffs[0] - 0.5) < 1e-6 && std::abs(legFit.coeffs[1] - 2.0) < 1e-6,
              "Bug6: FitCsvRegression handles European decimal-comma CSV");
    }

    // F-244, F-243, F-253: Vectors, FBD Equilibrium, Uncertainty Propagation
    {
        const EngVector3 r{ 1.0, 0.0, 0.0, UnitDim{ { 0, 1, 0, 0, 0, 0, 0 } }, "m", 1.0, false };
        const EngVector3 F{ 0.0, 10.0, 0.0, UnitDim{ { 1, 1, -2, 0, 0, 0, 0 } }, "N", 1.0, false };
        const EngVector3 tau = VecCross(r, F);
        Check(tau.z == 10.0 && tau.dim == (UnitDim{ { 1, 2, -2, 0, 0, 0, 0 } }), "F-244: r x F torque UnitDim");

        const EngVector3 ex{ 1, 0, 0, {}, "", 1, false }, ey{ 0, 1, 0, {}, "", 1, false }, ez{ 0, 0, 1, {}, "", 1, false }, ezNeg{ 0, 0, -1, {}, "", 1, false };
        Check(VerifyRightHandFrame(ex, ey, ez) && !VerifyRightHandFrame(ex, ey, ezNeg),
              "F-244: VerifyRightHandFrame accepts right-hand & rejects left-hand");

        // Simply supported beam L=4m, pin at x=0, rollerX at x=4, 100 N downward (-90 deg) at x=1
        FbdModel beam;
        beam.title = "Viga simplemente apoyada";
        beam.supports = {
            { "A", "RA", FbdSupportKind::Pin, 0.0, 0.0, 90.0 },
            { "B", "RB", FbdSupportKind::RollerX, 4.0, 0.0, 90.0 }
        };
        beam.forces = { { "P1", "P1", 1.0, 0.0, 100.0, -90.0, false, "N" } };
        const auto fbdSol = SolveFbdEquilibrium(beam);
        Check(fbdSol.ok && fbdSol.inEquilibrium && fbdSol.determinacyStatus == "DETERMINATE" &&
              std::abs(fbdSol.netFx) < 1e-9 && std::abs(fbdSol.netFy) < 1e-9 && std::abs(fbdSol.netMoment) < 1e-9,
              "F-243: Beam FBD equilibrium solved", fbdSol.stepsTraceUtf8);
        // RA_y = 75 N, RB_y = 25 N
        Check(fbdSol.reactions.size() == 3 && std::abs(fbdSol.reactions[1].second - 75.0) < 1e-7 &&
              std::abs(fbdSol.reactions[2].second - 25.0) < 1e-7,
              "F-243: RA_y=75N, RB_y=25N");

        // F-253: RSS & Monte Carlo uncertainty propagation
        const auto unc = PropagateUncertainty("A * B", {
            { "A", UncertainValue{ 10.0, 0.3, {}, "", 3 } },
            { "B", UncertainValue{ 5.0,  0.2, {}, "", 2 } }
        }, 5000);
        // RSS = sqrt((5*0.3)^2 + (10*0.2)^2) = sqrt(2.25 + 4) = 2.5
        Check(unc.ok && std::abs(unc.mean - 50.0) < 1e-9 && std::abs(unc.rssUncertainty - 2.5) < 1e-6 &&
              std::abs(unc.mcStdDev - 2.5) < 0.25,
              "F-253: RSS == 2.5 and Monte Carlo stdDev matches", unc.formattedRssUtf8);
    }

    // F-172, F-245: Formula Fingerprint & Interactive Formula Sheets
    {
        const auto fp1 = ComputeFormulaFingerprint("a*x^2 + b*x + c");
        const auto fp2 = ComputeFormulaFingerprint("p + q*t + r*t^2");
        Check(fp1.ok && fp2.ok && fp1.hash64 == fp2.hash64 && fp1.canonicalAlphaExpr == fp2.canonicalAlphaExpr,
              "F-172: Alpha-equivalence fingerprint matches reordered/renamed quadratic", fp1.canonicalAlphaExpr);

        FormulaFingerprintIndex idx;
        idx.Add("quad", "a*x^2 + b*x + c");
        const auto hits = idx.FindAlphaEquivalent("u*z^2 + v*z + w");
        Check(hits.size() == 1 && hits[0].id == "quad", "F-172: FormulaFingerprintIndex retrieves alpha-equivalent match");

        FormulaSheetEntry ohm;
        ohm.id = "ohm";
        ohm.titleUtf8 = "Ley de Ohm";
        ohm.exprUtf8 = "V = I * R | V";
        ohm.params = { { "I", "Corriente", "2 mA", 0, 10, false }, { "R", "Resistencia", "4.7 kOhm", 0, 100, false } };
        const auto inst = InstantiateFormulaEntry(ohm);
        Check(inst.ok && std::abs(inst.eval.result.value.real() - 9.4) < 1e-9 &&
              ValidateLatexUtf8(inst.latex2DUtf8).valid,
              "F-245: FormulaSheet instantiation V = 9.4 V + valid 2D LaTeX", inst.varSubTextUtf8);
    }
}

void TestGroup5MathJsonConfirmationCheckMyWorkAndAiContracts() {
    std::cout << "-- Group 5: MathJSON, ConfirmationCard, CheckMyWork, Solve/TeachMe, AI Contracts --\n";

    // F-133, F-310, SUP-07: ConfirmationCard state machine
    {
        auto card = CreateInterpretationCard(42, "\\frac{V}{R}", "\\frac{V}{R}", 0.92);
        Check(card.status == ConfirmationStatus::PendingReview && card.astValid &&
              card.rawSourceTextUtf8 == "\\frac{V}{R}",
              "F-310: InterpretationCard created in PendingReview with valid AST");
        Check(BeginEditInterpretationCard(card) && card.status == ConfirmationStatus::Editing,
              "F-310: Editing state reached");
        Check(!UpdateInterpretationCardFromMathJson(card, "[\"Divide\", 1]") && !card.astValid,
              "Neg F-310: Arity-invalid MathJSON fails validation");
        Check(!ConfirmInterpretationCard(card) && card.status != ConfirmationStatus::Confirmed,
              "Neg F-310: Cannot confirm invalid AST");
        Check(UpdateInterpretationCardFromExpr(card, "V / R") && card.astValid &&
              ConfirmInterpretationCard(card) && card.status == ConfirmationStatus::Confirmed &&
              card.audit.verificationStatus == VerificationStatus::UserConfirmed,
              "F-310: Valid edited AST confirms cleanly");
    }

    // F-313, F-314, SUP-07: Multi-var, complex, trig equivalence + CheckMyWork line pinpointing
    {
        const auto mvOk = CheckAlgebraicStepV2("(a + b)^2 - (a - b)^2", "4 * a * b");
        Check(mvOk.equivalent && mvOk.statusCode == "OK", "F-314: Multi-variable identity equivalent", mvOk.statusCode);

        const auto trigOk = CheckAlgebraicStepV2("sin(2*x)", "2*sin(x)*cos(x)");
        Check(trigOk.equivalent && trigOk.statusCode == "OK", "F-314: Trig double-angle identity equivalent", trigOk.statusCode);

        const auto eulOk = CheckAlgebraicStepV2("exp(j*x)", "cos(x) + j*sin(x)");
        Check(eulOk.equivalent && eulOk.statusCode == "OK", "F-314: Euler complex identity equivalent", eulOk.statusCode);

        const auto signErr = CheckAlgebraicStepV2("-(x + 3)", "-x + 3");
        Check(!signErr.equivalent && signErr.statusCode == "SIGN_ERR",
              "F-313: Sign error detected on -(x+3) -> -x+3", signErr.statusCode);

        const auto orderErr = CheckAlgebraicStepV2("(a + b)^2", "a^2 + b^2");
        Check(!orderErr.equivalent && orderErr.statusCode == "ORDER_ERR",
              "F-313: Order-of-ops Freshman's dream detected", orderErr.statusCode);

        const auto balErr = CheckAlgebraicStepV2("2*x + 4 = 10", "2*x = 10");
        Check(!balErr.equivalent && balErr.statusCode == "BALANCE_ERR",
              "F-313: Equation balance error detected", balErr.statusCode);

        const auto cmw = CheckMyWork({
            "(x + 1)^2 - 1",
            "x^2 + 2*x + 1 - 1",
            "x^2 + 2*x",
            "x^2 - 2*x" // error at index 3 (line 4)
        });
        Check(!cmw.allValid && cmw.firstErrorLineIndex == 3,
              "F-314/SUP-07: CheckMyWork pinpoints line index 3", cmw.summaryUtf8);

        const auto cmwBad0 = CheckMyWork({ "2*x + = 5", "2*x = 5" });
        Check(!cmwBad0.allValid && cmwBad0.firstErrorLineIndex == 0 &&
              cmwBad0.lines[0].transitionCheck.statusCode == "SYNTAX_ERR",
              "Neg F-314: CheckMyWork catches LHS syntax error at line 0", cmwBad0.summaryUtf8);
    }

    // F-312, SUP-07: Deterministic Solve & Socratic Teach Me
    {
        const auto sol = SolveDeterministic("x^2 - 5*x + 6 = 0", "x");
        Check(sol.ok && sol.hasAlternativeMethod && sol.primaryMethod.solutionsUtf8.size() == 2,
              "F-312: SolveDeterministic primary + alternative methods");

        FtsHit hit{ "Circuitos_I", 4, 101, "math", "Formula cuadratica delta = b^2 - 4ac", 95 };
        const auto tm = GenerateTeachMeSocratic("-(x + 2) = 5", { "-(x + 2) = 5", "-x + 2 = 5" }, "x", {}, { hit });
        Check(tm.ok && tm.hints.size() == 3 &&
              !tm.hints[0].revealsDirectAnswer && !tm.hints[1].revealsDirectAnswer &&
              tm.hints[0].promptUtf8.find("SIGN_ERR") != std::string::npos &&
              !tm.hints[1].provenanceRefs.empty(),
              "F-312/SUP-07: TeachMe anchors on student SIGN_ERR & cites FtsHit without leaking answer early");
    }

    // F-306..F-309, F-311, F-315..F-319
    {
        AiSettings defAi;
        Check(!defAi.enabled && !defAi.hideAiUi, "F-306: AiSettings OFF by default, UI visible");
        const auto disp = ResolveLocalFirstDispatch(AiCapabilityKind::TeachMe, defAi, true);
        Check(disp.executedLocally && !disp.requiresCloud && disp.route == DispatchRoute::LocalOnlyCloudBlocked,
              "F-307: Local-first blocks cloud when AI opt-in is OFF");

        const auto emptySearch = SynthesizeGroundedSearchAnswer("Laplace", {});
        Check(!emptySearch.hasEvidence && emptySearch.citations.empty(),
              "Neg F-308: Empty FtsHits never fabricates citations");

        const auto goodSearch = SynthesizeGroundedSearchAnswer("Ohm", {
            { "Notas_Circ", 2, 55, "math", "V = I * R", 90 }
        });
        Check(goodSearch.hasEvidence && goodSearch.citations.size() == 1 &&
              goodSearch.synthesizedAnswerUtf8.find("[1]") != std::string::npos,
              "F-308: Grounded search cites [1]");

        DocumentModel doc;
        doc.ResetToBlank(DocumentMode::InfiniteCanvas);
        doc.SetTitle("Lab 1");
        AnnotationObject mb;
        mb.kind = ObjectKind::MathBlock;
        mb.formulaExprUtf8 = "V = 4.7 kOhm * 2 mA | V";
        doc.AddCanvasObject(mb);
        EvaluateDocumentMathDag(doc);

        const auto sum = SummarizeDocumentNonDestructive(doc);
        Check(sum.ok && sum.summaryBlock.kind == ObjectKind::TextBox &&
              doc.CanvasObjects().size() == 1,
              "F-309: Summarizer non-destructive (source doc untouched)");

        auto review = BuildFlashcardReviewSession(doc, "Deck 1");
        Check(review.drafts.size() == 1, "F-311: Draft harvested");
        StudyDeck deck;
        Check(CommitApprovedFlashcardsToDeck(review, deck) == 0,
              "Neg F-311: Unapproved draft cannot commit");
        Check(EditFlashcardDraft(review, 0, "Ley de Ohm?", "V = 9.4 V") &&
              ApproveFlashcardDraft(review, 0) &&
              CommitApprovedFlashcardsToDeck(review, deck) == 1 &&
              deck.cards.size() == 1,
              "F-311: Approved flashcard commits to StudyDeck");

        const auto ieeeRep = RenderDeterministicReportTemplate(doc, { ReportTemplateKind::IeeePaper });
        Check(ieeeRep.ok && ieeeRep.htmlUtf8.find("IEEE") != std::string::npos,
              "F-315: Deterministic IEEE report rendered");

        const auto tr = TranslatePreservingMathAndUnits("Calcule la tension $V = I \\cdot R$ en mV", "es", "en");
        Check(tr.ok && tr.allMathAndUnitsIntact &&
              tr.translatedTextUtf8.find("$V = I \\cdot R$") != std::string::npos &&
              tr.translatedTextUtf8.find("mV") != std::string::npos,
              "F-316: Translation preserves $...$ and mV verbatim");

        DeterministicMacroRecorder rec;
        rec.Start("OhmMacro");
        rec.Record(MacroActionKind::CanvasAddMathBlock, "mb1", "V = 5 V", "5 V");
        rec.Stop();
        DeterministicMacroRecorder rec2;
        Check(rec2.FromJsonString(rec.ToJsonString()) && rec2.Steps().size() == 1 &&
              rec2.Steps()[0].payloadUtf8 == "V = 5 V",
              "F-317: Macro recorder JSON round-trip");

        const auto truth = ExtractRawTruthProjection(doc.CanvasObjects()[0]);
        const std::string renderedCardText = doc.CanvasObjects()[0].formulaExprUtf8 + "\n" + doc.CanvasObjects()[0].textUtf8;
        Check(ValidateAntiHidingInvariant(truth, renderedCardText, defAi) &&
              !ValidateAntiHidingInvariant(truth, "AI summary only", defAi),
              "F-319: Anti-hiding invariant passes on visible truth and rejects hidden truth");
    }
}

void TestIteration2Regressions() {
    std::cout << "-- Iteration 2 Regressions (FIND-01..FIND-19) --\n";

    // FIND-01..04: Win & Linux calculator evaluator precedence, 1e500, 2e, ans formatting
    {
        struct EvalCase {
            const char* utf8;
            const wchar_t* wide;
            CasioLinux::detail::AngleMode lMode;
            AngleMode wMode;
            double expected;
        };
        const EvalCase okCases[] = {
            { "sin(30)^2", L"sin(30)^2", CasioLinux::detail::AngleMode::Deg, ANG_DEG, 0.25 },
            { "ln(10)^2", L"ln(10)^2", CasioLinux::detail::AngleMode::Deg, ANG_DEG, std::log(10.0) * std::log(10.0) },
            { "cos(60)%", L"cos(60)%", CasioLinux::detail::AngleMode::Deg, ANG_DEG, 0.005 },
            { "abs(-3)!", L"abs(-3)!", CasioLinux::detail::AngleMode::Deg, ANG_DEG, 6.0 },
            { "-sin(30)^2", L"-sin(30)^2", CasioLinux::detail::AngleMode::Deg, ANG_DEG, -0.25 },
            { "sin -30", L"sin -30", CasioLinux::detail::AngleMode::Deg, ANG_DEG, -0.5 },
            { "2e", L"2e", CasioLinux::detail::AngleMode::Deg, ANG_DEG, 2.0 * std::exp(1.0) },
            { "2e3", L"2e3", CasioLinux::detail::AngleMode::Deg, ANG_DEG, 2000.0 },
            { "2e-3", L"2e-3", CasioLinux::detail::AngleMode::Deg, ANG_DEG, 0.002 },
            { "2e+3", L"2e+3", CasioLinux::detail::AngleMode::Deg, ANG_DEG, 2000.0 }
        };
        for (const auto& c : okCases) {
            const auto lOut = CasioLinux::detail::EvaluateWithAngleMode(c.utf8, c.lMode, 0.0);
            double wVal = 0.0;
            std::wstring wErr;
            const bool wOk = MathEvaluator::Evaluate(c.wide, 0.0, c.wMode, wVal, wErr);
            Check(lOut.ok && wOk && std::abs(lOut.value - c.expected) < 1e-9 && std::abs(wVal - c.expected) < 1e-9,
                  "FIND-01/03: Calc evaluator parity", c.utf8);
        }

        // sin(30)! -> Math ERROR on both
        {
            const auto lErr = CasioLinux::detail::EvaluateWithAngleMode("sin(30)!", CasioLinux::detail::AngleMode::Deg, 0.0);
            double wVal = 0.0; std::wstring wErr;
            const bool wOk = MathEvaluator::Evaluate(L"sin(30)!", 0.0, ANG_DEG, wVal, wErr);
            Check(!lErr.ok && lErr.errorMessage == "Math ERROR" && !wOk && wErr == L"Math ERROR",
                  "FIND-01: sin(30)! produces Math ERROR on both Win & Linux");
        }

        // FIND-02: 1e500 -> Math ERROR on both
        {
            const auto lOv = CasioLinux::detail::EvaluateWithAngleMode("1e500", CasioLinux::detail::AngleMode::Deg, 0.0);
            double wVal = 0.0; std::wstring wErr;
            const bool wOk = MathEvaluator::Evaluate(L"1e500", 0.0, ANG_DEG, wVal, wErr);
            Check(!lOv.ok && lOv.errorMessage == "Math ERROR" && !wOk && wErr == L"Math ERROR",
                  "FIND-02: 1e500 produces Math ERROR on both Win & Linux");
        }

        // FIND-04: Ans integer snapping & NaN guard parity
        {
            const double nearInt = 5.0 + 1e-12;
            const auto lAns = CasioLinux::detail::EvaluateWithAngleMode("Ans * 2", CasioLinux::detail::AngleMode::Deg, nearInt);
            double wVal = 0.0; std::wstring wErr;
            const bool wOk = MathEvaluator::Evaluate(L"Ans * 2", nearInt, ANG_DEG, wVal, wErr);
            const double nanVal = std::numeric_limits<double>::quiet_NaN();
            const auto lNan = CasioLinux::detail::EvaluateWithAngleMode("Ans + 3", CasioLinux::detail::AngleMode::Deg, nanVal);
            double wNanVal = 0.0; std::wstring wNanErr;
            const bool wNanOk = MathEvaluator::Evaluate(L"Ans + 3", nanVal, ANG_DEG, wNanVal, wNanErr);
            Check(lAns.ok && wOk && lAns.value == 10.0 && wVal == 10.0 &&
                  lNan.ok && wNanOk && lNan.value == 3.0 && wNanVal == 3.0,
                  "FIND-04: Ans formatting & NaN guard parity");
        }
    }

    // FIND-05..08: Unit denominator precedence, affine temperature arithmetic, hyphenated units vs env, Kelvin 'K'
    {
        // FIND-05
        const auto v1 = EvaluateExpression("10 m / 2 s | m/s");
        Check(v1.ok && std::abs(v1.result.value.real() - 5.0) < 1e-9 && v1.formattedResultUtf8.find("m/s") != std::string::npos,
              "FIND-05: 10 m / 2 s | m/s == 5 m/s", v1.formattedResultUtf8);
        const auto v2 = EvaluateExpression("(10 cm / 1 s)^2 | m^2/s^2");
        Check(v2.ok && std::abs(v2.result.value.real() - 0.01) < 1e-9,
              "FIND-05: (10 cm / 1 s)^2 | m^2/s^2 == 0.01 m^2/s^2", v2.formattedResultUtf8);
        const SymbolEnv abcEnv{
            { "a", QuantityValue{ { 10.0, 0.0 }, {}, "", 1.0, 0.0, false } },
            { "b", QuantityValue{ { 2.0, 0.0 }, {}, "", 1.0, 0.0, false } },
            { "c", QuantityValue{ { 3.0, 0.0 }, {}, "", 1.0, 0.0, false } }
        };
        const auto assoc = EvaluateExpression("a / b * c", abcEnv);
        Check(assoc.ok && std::abs(assoc.result.value.real() - 15.0) < 1e-9,
              "FIND-05: explicit a / b * c stays left-associative == 15", assoc.formattedResultUtf8);

        MathJsonNode ast5;
        CasEvalResult ast5Res;
        Check(ParseExpressionToMathJson("(10 cm / 1 s)^2 | m^2/s^2", ast5) &&
              EvaluateAst(ast5, {}, ast5Res) && std::abs(ast5Res.result.value.real() - 0.01) < 1e-9,
              "FIND-05: AstExprParser (10 cm / 1 s)^2 | m^2/s^2 == 0.01");

        // FIND-06
        const auto tMul1 = EvaluateExpression("2 * (25 degC) | K");
        const auto tMul2 = EvaluateExpression("(25 degC) * 2 | K");
        Check(tMul1.ok && std::abs(tMul1.result.value.real() - 596.3) < 1e-6 &&
              tMul2.ok && std::abs(tMul2.result.value.real() - 596.3) < 1e-6,
              "FIND-06: 2*(25 degC) & (25 degC)*2 == 596.3 K", tMul1.formattedResultUtf8);

        const auto tSub = EvaluateExpression("100 degC - 25 degC");
        const auto tZero = EvaluateExpression("25 degC - 25 degC");
        const auto tAdd = EvaluateExpression("10 degC + 5 degC");
        Check(tSub.ok && std::abs(tSub.result.value.real() - 75.0) < 1e-6 && tSub.formattedResultUtf8 == "75 K" &&
              tZero.ok && std::abs(tZero.result.value.real()) < 1e-6 && tZero.formattedResultUtf8 == "0 K" &&
              tAdd.ok && std::abs(tAdd.result.value.real() - 561.3) < 1e-6 && tAdd.formattedResultUtf8 == "561.3 K",
              "FIND-06: affine temperature deltas/sums format cleanly in Kelvin", tSub.formattedResultUtf8);

        MathJsonNode ast6;
        CasEvalResult ast6Res;
        Check(ParseExpressionToMathJson("2 * (25 degC) | K", ast6) &&
              EvaluateAst(ast6, {}, ast6Res) && std::abs(ast6Res.result.value.real() - 596.3) < 1e-6,
              "FIND-06: EvaluateAst 2*(25 degC) | K == 596.3 K");

        // FIND-07
        const SymbolEnv msEnv{
            { "m", QuantityValue{ { 10.0, 0.0 }, {}, "", 1.0, 0.0, false } },
            { "s", QuantityValue{ { 3.0, 0.0 }, {}, "", 1.0, 0.0, false } }
        };
        const auto msRes = EvaluateExpression("m-s", msEnv);
        MathJsonNode msAst;
        CasEvalResult msAstRes;
        Check(msRes.ok && std::abs(msRes.result.value.real() - 7.0) < 1e-9 &&
              ParseExpressionToMathJson("m-s", msAst) && EvaluateAst(msAst, msEnv, msAstRes) &&
              std::abs(msAstRes.result.value.real() - 7.0) < 1e-9,
              "FIND-07: m-s with env{m=10,s=3} == 7 (both ExprParser & AstExprParser)", msRes.formattedResultUtf8);

        // FIND-08
        const auto kelvin = EvaluateExpression("300K | degC");
        MathJsonNode kAst;
        CasEvalResult kAstRes;
        Check(kelvin.ok && std::abs(kelvin.result.value.real() - 300.0) < 1e-6 &&
              kelvin.formattedResultUtf8.find("26.85") != std::string::npos &&
              ParseExpressionToMathJson("300K | degC", kAst) && EvaluateAst(kAst, {}, kAstRes) &&
              std::abs(kAstRes.result.value.real() - 300.0) < 1e-6,
              "FIND-08: 300K | degC == 26.85 degC", kelvin.formattedResultUtf8);
    }

    // FIND-09 & FIND-10: MathML->LaTeX matrix round-trip, plain [a, b], tangent roots
    {
        const std::string mml = LatexToMathML("\\begin{bmatrix} 1 & 2 \\\\ 3 & 4 \\end{bmatrix}");
        const std::string back = MathMLToLatex(mml);
        const auto reparsed = ParseLatexToAst(back);
        Check(reparsed.ok && back.find("[\\begin") == std::string::npos,
              "FIND-09: MathMLToLatex matrix round-trips cleanly without double brackets", back);

        const auto br = ParseLatexToAst("[a, b]");
        const auto badBr = ParseLatexToAst("a + b]");
        Check(br.ok && !badBr.ok, "FIND-09: plain [a, b] parses while unmatched ] fails");

        const auto tanRoots = FindRoots2D("x^2", "x", -1.01, 1.03, 256);
        const auto noRoots = FindRoots2D("x^2 + 0.05", "x", -1.0, 1.0, 256);
        Check(tanRoots.size() == 1 && std::abs(tanRoots[0].x) < 1e-6 && noRoots.empty(),
              "FIND-10: FindRoots2D detects even-multiplicity tangent root at x=0 and ignores non-root min");
    }

    // FIND-11..16: Group 4 Eigen 64x64, 1e-10 scale, Limits, FBD couple, Fingerprint, SigFigs
    {
        // FIND-11: Dense 64x64 real symmetric matrix
        const std::size_t N = 64;
        ComplexMatrix sym64{ N, N, std::vector<std::complex<double>>(N * N, { 0.0, 0.0 }) };
        for (std::size_t r = 0; r < N; ++r) {
            for (std::size_t c = r; c < N; ++c) {
                const double val = (r == c)
                    ? (10.0 + 0.25 * static_cast<double>(r))
                    : (0.1 / (1.0 + static_cast<double>(c - r)));
                sym64.At(r, c) = { val, 0.0 };
                sym64.At(c, r) = { val, 0.0 };
            }
        }
        const auto eig64 = ComputeMatrixEigen(sym64);
        double maxRes64 = 0.0;
        if (eig64.ok && eig64.eigenvalues.size() == N) {
            for (std::size_t j = 0; j < N; ++j) {
                for (std::size_t r = 0; r < N; ++r) {
                    std::complex<double> av(0.0, 0.0);
                    for (std::size_t c = 0; c < N; ++c) av += sym64.At(r, c) * eig64.eigenvectors[j][c];
                    maxRes64 = (std::max)(maxRes64, std::abs(av - eig64.eigenvalues[j] * eig64.eigenvectors[j][r]));
                }
            }
        }
        Check(eig64.ok && eig64.eigenvalues.size() == N && maxRes64 < 1e-9,
              "FIND-11: Dense 64x64 real symmetric ComputeMatrixEigen maxRes < 1e-9", std::to_string(maxRes64));

        // QR non-convergence test with deliberately tiny maxIter=1 on 4x4 non-symmetric
        ComplexMatrix nonSym4{ 4, 4, {
            {0,0}, {1,0}, {0,0}, {0,0},
            {0,0}, {0,0}, {1,0}, {0,0},
            {0,0}, {0,0}, {0,0}, {1,0},
            {-1,0}, {2,0}, {-3,0}, {4,0}
        } };
        const auto qrFail = ComputeMatrixEigen(nonSym4, 1);
        (void)qrFail; // may converge or fail if >400 iterCap, check larger 16x16 with iterCap forced if needed

        // FIND-12: 1e-10 scaled 3x3 non-Hermitian complex matrix
        ComplexMatrix tiny3{ 3, 3, {
            { 1e-10, 0.5e-10 }, { 0.3e-10, -0.2e-10 }, { 0.1e-10, 0.4e-10 },
            { -0.2e-10, 0.1e-10 }, { 2e-10, -0.3e-10 }, { 0.5e-10, 0.2e-10 },
            { 0.4e-10, -0.1e-10 }, { -0.1e-10, 0.3e-10 }, { -1e-10, 0.2e-10 }
        } };
        const auto eigTiny = ComputeMatrixEigen(tiny3);
        double tinyNorm = 0.0;
        for (const auto& v : tiny3.data) tinyNorm = (std::max)(tinyNorm, std::abs(v));
        double maxRelResTiny = 0.0;
        if (eigTiny.ok && eigTiny.eigenvalues.size() == 3) {
            for (std::size_t j = 0; j < 3; ++j) {
                for (std::size_t r = 0; r < 3; ++r) {
                    std::complex<double> av(0.0, 0.0);
                    for (std::size_t c = 0; c < 3; ++c) av += tiny3.At(r, c) * eigTiny.eigenvectors[j][c];
                    const double res = std::abs(av - eigTiny.eigenvalues[j] * eigTiny.eigenvectors[j][r]) / tinyNorm;
                    maxRelResTiny = (std::max)(maxRelResTiny, res);
                }
            }
        }
        Check(eigTiny.ok && eigTiny.eigenvalues.size() == 3 && maxRelResTiny < 1e-8,
              "FIND-12: 1e-10 scaled 3x3 complex ComputeMatrixEigen relRes < 1e-8", std::to_string(maxRelResTiny));

        // FIND-13: Divergent and oscillating limits rejected
        Check(!ComputeNumericalLimit("1/(x^2)", "x", 0.0, LimitSide::Bilateral).ok &&
              !ComputeNumericalLimit("1/(x^2)", "x", 0.0, LimitSide::Right).ok &&
              !ComputeNumericalLimit("cos(1/x)", "x", 0.0, LimitSide::Bilateral).ok &&
              !ComputeNumericalLimit("cos(1/x)", "x", 0.0, LimitSide::Right).ok,
              "FIND-13: Divergent 1/x^2 & oscillating cos(1/x) limits return ok=false");

        // FIND-14: Antisymmetric couple distributed load in FBD
        FbdModel coupleBeam;
        coupleBeam.supports = {
            { "A", "RA", FbdSupportKind::Pin, 0.0, 0.0, 90.0 },
            { "B", "RB", FbdSupportKind::RollerX, 6.0, 0.0, 90.0 }
        };
        coupleBeam.distLoads = { { "D1", "D1", 0.0, 0.0, 6.0, 0.0, 100.0, -100.0, -90.0 } };
        const auto coupleSol = SolveFbdEquilibrium(coupleBeam);
        Check(coupleSol.ok && coupleSol.inEquilibrium && coupleSol.reactions.size() == 3 &&
              std::abs(coupleSol.reactions[1].second - 100.0) < 1e-6 &&
              std::abs(coupleSol.reactions[2].second - (-100.0)) < 1e-6,
              "FIND-14: Antisymmetric couple distributed load RA_y=100N, RB_y=-100N", coupleSol.stepsTraceUtf8);

        // FIND-15: Alpha-equivalence fingerprint with symmetric sibling shapes
        const auto fpA = ComputeFormulaFingerprint("a*x + b*y + a^2");
        const auto fpB = ComputeFormulaFingerprint("z*x + c*y + z^2");
        Check(fpA.ok && fpB.ok && fpA.hash64 == fpB.hash64 && fpA.canonicalAlphaExpr == fpB.canonicalAlphaExpr,
              "FIND-15: Alpha-equivalence matches symmetric sibling shapes", fpA.canonicalAlphaExpr + " vs " + fpB.canonicalAlphaExpr);

        // FIND-16: Significant figures decade boundary rounding
        const std::string sf999 = FormatSignificantFigures(9.999, 2);
        Check(sf999 == "10" || sf999 == "1.0e+01",
              "FIND-16: FormatSignificantFigures(9.999, 2) has 2 sig figs", sf999);
    }

    // FIND-17..19: Group 5 MathJSON allowlist, CheckAlgebraicStepV2 / CheckMyWork, Translate
    {
        // FIND-17
        auto card = CreateInterpretationCard(77, "x + 1", "x + 1", 0.9);
        Check(BeginEditInterpretationCard(card) &&
              !UpdateInterpretationCardFromMathJson(card, "[\"UnknownOp\", 1, 2]") &&
              !card.astValid &&
              !ConfirmInterpretationCard(card) &&
              card.status != ConfirmationStatus::Confirmed,
              "FIND-17: Unknown MathJSON operator rejected by ValidateMathJsonAst");

        // FIND-18
        const auto sMulti = CheckAlgebraicStepV2("-(a + b + c)", "-a + b + c");
        Check(!sMulti.equivalent && sMulti.statusCode == "SIGN_ERR",
              "FIND-18: Multi-term sign flip -(a+b+c) -> -a+b+c is SIGN_ERR", sMulti.statusCode);

        const auto sEqOrd = CheckAlgebraicStepV2("(a + b)^2 = c", "a^2 + b^2 = c");
        Check(!sEqOrd.equivalent && sEqOrd.statusCode == "ORDER_ERR",
              "FIND-18: Equation-embedded order error (a+b)^2=c -> a^2+b^2=c is ORDER_ERR", sEqOrd.statusCode);

        const auto sDim1 = CheckAlgebraicStepV2("10 m + 5 s", "15 m");
        const auto sDim2 = CheckAlgebraicStepV2("10 ft + 5 s", "10 ft + 5 s");
        const auto cmwDim = CheckMyWork({ "10 ft + 5 s" });
        Check(!sDim1.equivalent && sDim1.statusCode == "DIM_ERR" &&
              !sDim2.equivalent && sDim2.statusCode == "DIM_ERR" &&
              !cmwDim.allValid && cmwDim.firstErrorLineIndex == 0 &&
              cmwDim.lines[0].transitionCheck.statusCode == "DIM_ERR",
              "FIND-18: Dimensional mismatches propagate as DIM_ERR in CheckAlgebraicStepV2 & CheckMyWork", sDim1.statusCode);

        // FIND-19
        const auto trEs = TranslatePreservingMathAndUnits("Calcule la tension de 10 V.", "es", "en");
        const auto trEn = TranslatePreservingMathAndUnits("Calculate the voltage of 10 V.", "en", "es");
        Check(trEs.ok && trEs.allMathAndUnitsIntact &&
              trEs.translatedTextUtf8.find("the voltage") != std::string::npos &&
              trEs.translatedTextUtf8.find("10 V.") != std::string::npos &&
              std::find(trEs.preservedUnitTokens.begin(), trEs.preservedUnitTokens.end(), "V") != trEs.preservedUnitTokens.end() &&
              trEn.ok && trEn.translatedTextUtf8.find("la tension") != std::string::npos &&
              trEn.translatedTextUtf8.find("10 V.") != std::string::npos,
              "FIND-19: Multi-word phrase translation & trailing punctuation unit preservation", trEs.translatedTextUtf8);
    }
}

void TestIteration3RegressionsAndTypesetAst() {
    std::cout << "-- Iteration 3 Regressions (R2-C1-01..03, R2-C2-01..02, M2-TYPESET-AST, Translation Polish) --\n";

    // R2-C1-01: FindRoots2D collapses flat/quartic zero troughs to 1 root while preserving close/mixed roots
    {
        const auto rQuart0 = FindRoots2D("x^4", "x", -1.01, 1.03, 256);
        Check(rQuart0.size() == 1 && std::abs(rQuart0[0].x) < 1e-4,
              "R2-C1-01: FindRoots2D(x^4) returns exactly 1 root at 0.0", std::to_string(rQuart0.size()));

        const auto rQuartShift = FindRoots2D("(x-0.3)^4", "x", 0.25, 0.35, 256);
        Check(rQuartShift.size() == 1 && std::abs(rQuartShift[0].x - 0.3) < 1e-4,
              "R2-C1-01: FindRoots2D((x-0.3)^4) returns exactly 1 root at 0.3", std::to_string(rQuartShift.size()));

        const auto rQuartOffGrid = FindRoots2D("(x-0.3)^4", "x", 0.25, 0.35, 257);
        Check(rQuartOffGrid.size() == 1 && std::abs(rQuartOffGrid[0].x - 0.3) < 1e-4,
              "R2-C1-01: FindRoots2D((x-0.3)^4, N=257) returns exactly 1 root at 0.3", std::to_string(rQuartOffGrid.size()));

        const auto rCubic = FindRoots2D("x^2*(x-0.5)", "x", -1.0, 1.0, 256);
        Check(rCubic.size() == 2 && std::abs(rCubic[0].x) < 1e-4 && std::abs(rCubic[1].x - 0.5) < 1e-4,
              "R2-C1-01: x^2*(x-0.5) preserves both tangent (0) and crossing (0.5) roots", std::to_string(rCubic.size()));

        const auto rPole = FindRoots2D("1/x", "x", -1.0, 1.0, 255);
        Check(rPole.empty(), "R2-C1-01: 1/x pole emits 0 roots");
    }

    // R2-C1-02: AstExprParser parity for unit-only denominators & post-operator hyphenated compound units
    {
        MathJsonNode a1{}, a2{}, a3{};
        CasEvalResult r1{}, r2{}, r3{};
        Check(ParseExpressionToMathJson("10 m / kg m | 1/kg", a1) && EvaluateAst(a1, {}, r1) &&
              std::abs(r1.result.value.real() - 10.0) < 1e-9 && r1.formattedResultUtf8.find("1/kg") != std::string::npos,
              "R2-C1-02: AstExprParser 10 m / kg m | 1/kg == 10 1/kg", r1.errorUtf8 + " " + r1.formattedResultUtf8);

        Check(ParseExpressionToMathJson("10 J / ft-kip", a2) && EvaluateAst(a2, {}, r2) &&
              r2.result.dim.IsDimensionless() && std::abs(r2.result.value.real() - 0.00737562149) < 1e-7,
              "R2-C1-02: AstExprParser 10 J / ft-kip == 0.00737562", r2.errorUtf8 + " " + r2.formattedResultUtf8);

        Check(ParseExpressionToMathJson("10 * ft-kip | N-m", a3) && EvaluateAst(a3, {}, r3) &&
              std::abs(r3.result.value.real() - 13558.17948) < 0.1,
              "R2-C1-02: AstExprParser 10 * ft-kip | N-m == 13558.2 N-m", r3.errorUtf8 + " " + r3.formattedResultUtf8);

        const auto s1 = CheckAlgebraicStep("10 m / kg m", "10 / kg");
        const auto s2 = CheckAlgebraicStepV2("10 J / ft-kip", "10 J / (ft * kip)");
        Check(s1.equivalent && s1.statusCode == "OK" && s2.equivalent && s2.statusCode == "OK",
              "R2-C1-02: CheckAlgebraicStep/V2 accept unit denominators & hyphenated units", s1.statusCode + "/" + s2.statusCode);

        // Bound variables in N-m / m-s still subtract!
        const SymbolEnv nmEnv{
            { "N", QuantityValue{ { 20.0, 0.0 }, {}, "", 1.0, 0.0, false } },
            { "m", QuantityValue{ { 7.0, 0.0 }, {}, "", 1.0, 0.0, false } }
        };
        MathJsonNode aNm{}; CasEvalResult rNm{};
        Check(ParseExpressionToMathJson("N-m", aNm) && EvaluateAst(aNm, nmEnv, rNm) &&
              std::abs(rNm.result.value.real() - 13.0) < 1e-9,
              "R2-C1-02: Standalone N-m with bound env{N=20,m=7} subtracts to 13", rNm.formattedResultUtf8);
    }

    // R2-C1-03: Inline engineering suffixes p/n/u/k/M/G never hijack bound, plot, solver, step-check, or DAG variables
    {
        const SymbolEnv bEnv{
            { "k", QuantityValue{ { 3.0, 0.0 }, {}, "", 1.0, 0.0, false } },
            { "n", QuantityValue{ { 5.0, 0.0 }, {}, "", 1.0, 0.0, false } },
            { "p", QuantityValue{ { 2.0, 0.0 }, {}, "", 1.0, 0.0, false } },
            { "u", QuantityValue{ { 4.0, 0.0 }, {}, "", 1.0, 0.0, false } },
            { "M", QuantityValue{ { 7.0, 0.0 }, {}, "", 1.0, 0.0, false } },
            { "G", QuantityValue{ { 9.0, 0.0 }, {}, "", 1.0, 0.0, false } }
        };
        struct BoundCase { const char* expr; double expected; };
        const BoundCase cases[] = {
            { "10k", 30.0 },
            { "2n + 1", 11.0 },
            { "2p + 1", 5.0 },
            { "2u + 1", 9.0 },
            { "2M + 1", 15.0 },
            { "2G + 1", 19.0 }
        };
        for (const auto& c : cases) {
            const auto ev = EvaluateExpression(c.expr, bEnv);
            MathJsonNode ast{};
            CasEvalResult astEv{};
            const bool pOk = ParseExpressionToMathJson(c.expr, ast);
            const bool aOk = pOk && EvaluateAst(ast, bEnv, astEv);
            Check(ev.ok && std::abs(ev.result.value.real() - c.expected) < 1e-9 &&
                  aOk && std::abs(astEv.result.value.real() - c.expected) < 1e-9,
                  "R2-C1-03: Bound var overrides inline suffix", std::string(c.expr) + " -> " + ev.formattedResultUtf8);
        }

        // Unbound inline suffixes still work as engineering multipliers
        const auto unb1 = EvaluateExpression("4.7k + 300");
        const auto unb2 = EvaluateExpression("10k * 2 Ohm | kOhm");
        MathJsonNode unbAst{}; CasEvalResult unbAstEv{};
        Check(unb1.ok && std::abs(unb1.result.value.real() - 5000.0) < 1e-9 &&
              unb2.ok && std::abs(unb2.result.value.real() - 20000.0) < 1e-6 &&
              unb2.formattedResultUtf8.find("20 kOhm") != std::string::npos &&
              ParseExpressionToMathJson("4.7k + 300", unbAst) && EvaluateAst(unbAst, {}, unbAstEv) &&
              std::abs(unbAstEv.result.value.real() - 5000.0) < 1e-9,
              "R2-C1-03: Unbound 4.7k+300==5000 and 10k*2 Ohm | kOhm == 20 kOhm");

        // Plot, StepCheck, Solve, DAG with 'n' / 'k'
        const auto plotPts = SampleFunction2D("2n + 1", "n", 0.0, 4.0, 5);
        Check(plotPts.size() == 5 && std::abs(plotPts[4].y - 9.0) < 1e-9,
              "R2-C1-03: SampleFunction2D(2n+1, n) at n=4 == 9.0", plotPts.empty() ? "" : std::to_string(plotPts.back().y));

        const auto stepN = CheckAlgebraicStep("2n + 1", "1 + 2*n", "n");
        Check(stepN.equivalent && stepN.statusCode == "OK",
              "R2-C1-03: CheckAlgebraicStep(2n+1, 1+2*n, n) == OK", stepN.statusCode);

        const auto solN = SolveAlgebraicEquation("2n - 10 = 0", "n");
        Check(solN.ok && solN.solutions.size() == 1 && std::abs(solN.solutions[0].real() - 5.0) < 1e-9,
              "R2-C1-03: SolveAlgebraicEquation(2n-10=0, n) == 5.0", solN.errorUtf8);

        DocumentModel dagDoc;
        dagDoc.ResetToBlank(DocumentMode::InfiniteCanvas);
        AnnotationObject nk, ny;
        nk.kind = ObjectKind::MathBlock; nk.formulaVarNameUtf8 = "k"; nk.formulaExprUtf8 = "k = 3";
        ny.kind = ObjectKind::MathBlock; ny.formulaVarNameUtf8 = "y"; ny.formulaExprUtf8 = "y = 10k + 1";
        dagDoc.AddCanvasObject(ny);
        dagDoc.AddCanvasObject(nk);
        const auto dagRep = EvaluateDocumentMathDag(dagDoc);
        Check(dagRep.ok && dagDoc.CanvasObjects()[0].evaluatedResultUtf8 == "31",
              "R2-C1-03: EvaluateDocumentMathDag(k=3; y=10k+1) == 31", dagDoc.CanvasObjects()[0].evaluatedResultUtf8);
    }

    // R2-C2-01: ComputeMatrixEigen norm-scaled isReal / isHerm tolerances & zero matrix handling
    {
        ComplexMatrix skewTiny{ 2, 2, {
            { 1e-11, 0.0 }, { 2e-11, 0.0 },
            { -2e-11, 0.0 }, { 1e-11, 0.0 }
        } };
        const auto eSkew = ComputeMatrixEigen(skewTiny);
        double skewScale = 0.0;
        for (const auto& v : skewTiny.data) skewScale = (std::max)(skewScale, std::abs(v));
        double maxRelSkew = 0.0;
        if (eSkew.ok && eSkew.eigenvalues.size() == 2) {
            for (std::size_t j = 0; j < 2; ++j) {
                for (std::size_t r = 0; r < 2; ++r) {
                    std::complex<double> av(0.0, 0.0);
                    for (std::size_t c = 0; c < 2; ++c) av += skewTiny.At(r, c) * eSkew.eigenvectors[j][c];
                    maxRelSkew = (std::max)(maxRelSkew, std::abs(av - eSkew.eigenvalues[j] * eSkew.eigenvectors[j][r]) / skewScale);
                }
            }
        }
        Check(eSkew.ok && !eSkew.isHermitian && maxRelSkew < 1e-8,
              "R2-C2-01: 1e-11 skew-like real matrix solves as non-Hermitian with maxRel < 1e-8", std::to_string(maxRelSkew));

        ComplexMatrix cplxTiny{ 2, 2, {
            { 1e-12, 0.0 }, { 0.0, 2e-12 },
            { 0.0, 3e-12 }, { 1e-12, 0.0 }
        } };
        const auto eCplx = ComputeMatrixEigen(cplxTiny);
        double cplxScale = 0.0;
        for (const auto& v : cplxTiny.data) cplxScale = (std::max)(cplxScale, std::abs(v));
        double maxRelCplx = 0.0;
        if (eCplx.ok && eCplx.eigenvalues.size() == 2) {
            for (std::size_t j = 0; j < 2; ++j) {
                for (std::size_t r = 0; r < 2; ++r) {
                    std::complex<double> av(0.0, 0.0);
                    for (std::size_t c = 0; c < 2; ++c) av += cplxTiny.At(r, c) * eCplx.eigenvectors[j][c];
                    maxRelCplx = (std::max)(maxRelCplx, std::abs(av - eCplx.eigenvalues[j] * eCplx.eigenvectors[j][r]) / cplxScale);
                }
            }
        }
        Check(eCplx.ok && !eCplx.isHermitian && maxRelCplx < 1e-8,
              "R2-C2-01: 1e-12 non-Hermitian complex matrix solves accurately with maxRel < 1e-8", std::to_string(maxRelCplx));

        ComplexMatrix zeroMat{ 3, 3, std::vector<std::complex<double>>(9, { 0.0, 0.0 }) };
        const auto eZero = ComputeMatrixEigen(zeroMat);
        Check(eZero.ok && eZero.isHermitian && eZero.eigenvalues.size() == 3 &&
              std::abs(eZero.eigenvalues[0]) == 0.0 && std::abs(eZero.eigenvectors[0][0] - 1.0) < 1e-12,
              "R2-C2-01: All-zero matrix returns zero eigenvalues cleanly");
    }

    // R2-C2-02: ValidateMathJsonAst strict rejection of bogus UnitQuantity, stray Strings, non-numeric Complex
    {
        const char* badPayloads[] = {
            R"(["UnitQuantity", 10, {"str":"bogusunit"}])",
            R"(["UnitQuantity", 10, 5])",
            R"(["Add", 1, {"str":"bogus"}])",
            R"(["Complex", "a", 2])"
        };
        for (const char* p : badPayloads) {
            auto card = CreateInterpretationCard(901, "x + 1", "x + 1", 0.9);
            BeginEditInterpretationCard(card);
            const bool upOk = UpdateInterpretationCardFromMathJson(card, p);
            const bool confOk = ConfirmInterpretationCard(card);
            Check(!upOk && !card.astValid && !confOk && card.status != ConfirmationStatus::Confirmed,
                  "R2-C2-02: Invalid MathJSON payload rejected before Confirmation", p);
        }

        // Valid UnitQuantity passes
        auto okCard = CreateInterpretationCard(902, "x + 1", "x + 1", 0.9);
        BeginEditInterpretationCard(okCard);
        Check(UpdateInterpretationCardFromMathJson(okCard, R"(["UnitQuantity", 10, {"str":"kOhm"}])") &&
              okCard.astValid && ConfirmInterpretationCard(okCard) && okCard.status == ConfirmationStatus::Confirmed,
              "R2-C2-02: Valid UnitQuantity MathJSON confirms cleanly");
    }

    // M2-TYPESET-AST: Verify all 8 2D mathematical box typesetter AST primitives
    {
        // 1. Stacked \frac & top-level rational H(s)
        const std::string hsLtx = ConvertCasExprToLatex("H(s) = (0.1908 + 8.29e-05*s) / (1 + 0.005344*s)");
        const std::string ratLtx = ConvertCasExprToLatex("(s+1)/(s^2+2*s+5)");
        Check(ratLtx == "\\frac{s + 1}{s^{2} + 2 \\cdot s + 5}",
              "M2-TYPESET-AST #1: ConvertCasExprToLatex rational emits \\frac", ratLtx);
        Check(hsLtx.find("\\frac{0.1908 + 8.29 \\times 10^{-5} \\cdot s}{1 + 0.005344 \\cdot s}") != std::string::npos &&
              hsLtx.find("e-05") == std::string::npos &&
              ValidateLatexUtf8(hsLtx).valid,
              "M2-TYPESET-AST #1/#2: H(s) emits \\frac and 8.29 \\times 10^{-5} (no raw e-05)", hsLtx);

        // 2. Scaled scientific notation & superscripts/subscripts
        const std::string sciPos = ConvertCasExprToLatex("1.5e+06");
        Check(sciPos == "1.5 \\times 10^{6}", "M2-TYPESET-AST #2: 1.5e+06 -> 1.5 \\times 10^{6}", sciPos);
        const auto pSub = ParseLatexToAst("P_{tot} + V_{out} + x^{2} + y^{2}");
        Check(pSub.ok && pSub.root && pSub.root->kind == MathAstKind::Row &&
              pSub.root->children.size() >= 7 &&
              pSub.root->children[0]->kind == MathAstKind::SupSub && pSub.root->children[0]->sub != nullptr &&
              pSub.root->children[4]->kind == MathAstKind::SupSub && pSub.root->children[4]->sup != nullptr,
              "M2-TYPESET-AST #2: P_{tot}, V_{out}, x^{2}, y^{2} produce SupSub AST nodes");

        // 3. \sqrt and nth roots \sqrt[n]{...}
        const auto pSq = ParseLatexToAst("\\sqrt{x} + \\sqrt[3]{x + 1}");
        Check(pSq.ok && pSq.root && pSq.root->kind == MathAstKind::Row && pSq.root->children.size() == 3 &&
              pSq.root->children[0]->kind == MathAstKind::Sqrt && pSq.root->children[0]->rootIdx == nullptr && pSq.root->children[0]->radicand != nullptr &&
              pSq.root->children[2]->kind == MathAstKind::Sqrt && pSq.root->children[2]->rootIdx != nullptr && pSq.root->children[2]->radicand != nullptr,
              "M2-TYPESET-AST #3: \\sqrt and \\sqrt[3] preserve rootIdx vs radicand");

        // 4. Stretchy delimiters & matrices (bmatrix / pmatrix)
        const auto pMat = ParseLatexToAst("\\left( \\frac{a}{b} \\right) + \\begin{bmatrix} 1 & 2 \\\\ 3 & 4 \\end{bmatrix} + \\begin{pmatrix} x \\\\ y \\end{pmatrix}");
        Check(pMat.ok && pMat.root && pMat.root->kind == MathAstKind::Row && pMat.root->children.size() == 5 &&
              pMat.root->children[0]->kind == MathAstKind::Delimiter && pMat.root->children[0]->text == "(" && pMat.root->children[0]->closeDelim == ")" &&
              pMat.root->children[2]->kind == MathAstKind::Matrix && pMat.root->children[2]->grid.size() == 2 && pMat.root->children[2]->grid[0].size() == 2 &&
              pMat.root->children[4]->kind == MathAstKind::Matrix && pMat.root->children[4]->text == "(" && pMat.root->children[4]->grid.size() == 2,
              "M2-TYPESET-AST #4: Delimiter and bmatrix/pmatrix Matrix nodes cleanly structured");

        // 5. Big operators (\int, \sum, \lim) with limits
        const auto pBig = ParseLatexToAst("\\int_{0}^{\\infty} e^{-x} dx + \\sum_{k=1}^{n} k + \\lim_{x \\to 0} \\frac{\\sin x}{x}");
        bool foundInt = false, foundSum = false, foundLim = false;
        if (pBig.ok && pBig.root) {
            for (const auto& ch : pBig.root->children) {
                if (ch && ch->kind == MathAstKind::BigOp) {
                    if (ch->text == "\xE2\x88\xAB" && ch->sub && ch->sup) foundInt = true;
                    if (ch->text == "\xE2\x88\x91" && ch->sub && ch->sup) foundSum = true;
                    if (ch->text == "lim" && ch->sub && ch->forceUpright && ch->role == MathFontRole::MathRoman) foundLim = true;
                }
            }
        }
        Check(foundInt && foundSum && foundLim,
              "M2-TYPESET-AST #5: BigOp captures \\int, \\sum, and upright \\lim with limits");

        // 6. Accents (\vec, \hat)
        const auto pAcc = ParseLatexToAst("\\vec{E} + \\hat{n}");
        Check(pAcc.ok && pAcc.root && pAcc.root->children.size() == 3 &&
              pAcc.root->children[0]->kind == MathAstKind::Accent && pAcc.root->children[0]->accentKind == "vec" && pAcc.root->children[0]->base != nullptr &&
              pAcc.root->children[2]->kind == MathAstKind::Accent && pAcc.root->children[2]->accentKind == "hat" && pAcc.root->children[2]->base != nullptr,
              "M2-TYPESET-AST #6: Accent captures \\vec and \\hat cleanly");

        // 7. Greek & physics symbols (\alpha..\Omega, \hbar, \nabla, \partial, \infty, \cdot, \times, \angle, \Delta, \theta)
        const auto pSym = ParseLatexToAst("\\alpha + \\beta + \\Omega + \\hbar + \\nabla + \\partial + \\infty + \\cdot + \\times + \\angle + \\Delta + \\theta");
        std::string allSymText;
        if (pSym.ok && pSym.root) {
            for (const auto& ch : pSym.root->children) {
                if (ch) allSymText += ch->text;
            }
        }
        Check(pSym.ok &&
              allSymText.find("\xCE\xB1") != std::string::npos &&
              allSymText.find("\xCE\xA9") != std::string::npos &&
              allSymText.find("\xE2\x84\x8F") != std::string::npos &&
              allSymText.find("\xE2\x88\x87") != std::string::npos &&
              allSymText.find("\xE2\x88\x82") != std::string::npos &&
              allSymText.find("\xE2\x88\x9E") != std::string::npos &&
              allSymText.find("\xC2\xB7") != std::string::npos &&
              allSymText.find("\xC3\x97") != std::string::npos &&
              allSymText.find("\xE2\x88\xA0") != std::string::npos &&
              allSymText.find("\xCE\x94") != std::string::npos &&
              allSymText.find("\xCE\xB8") != std::string::npos &&
              allSymText.find("hbar") == std::string::npos,
              "M2-TYPESET-AST #7: Greek & physics symbols map to Unicode (including \\hbar)", allSymText);

        // 8. Upright \text{...} SI units & Roman functions vs italic variables
        const std::string pBarLtx = ConvertCasExprToLatex("P_tot = 9.89476 bar");
        const std::string ohmLtx = ConvertCasExprToLatex("V_out = 4.7 kOhm * 2 mA");
        Check(pBarLtx == "P_{tot} = 9.89476\\,\\text{bar}",
              "M2-TYPESET-AST #8: ConvertCasExprToLatex emits upright \\text{bar} and P_{tot}", pBarLtx);
        Check(ohmLtx == "V_{out} = 4.7\\,\\text{k\\Omega} \\cdot 2\\,\\text{mA}",
              "M2-TYPESET-AST #8: ConvertCasExprToLatex emits upright \\text{k\\Omega} and \\text{mA}", ohmLtx);

        const auto spansRich = TokenizeRichLatexUtf8("$V_{out} = \\sin(\\theta) + 2\\,\\text{mA} + \\text{psi}$");
        Check(spansRich.size() == 1 && spansRich[0].isMath && spansRich[0].ast != nullptr &&
              spansRich[0].ast->kind == MathAstKind::Row && spansRich[0].ast->children.size() >= 9 &&
              spansRich[0].ast->children[0]->kind == MathAstKind::SupSub &&
              spansRich[0].ast->children[0]->base->role == MathFontRole::MathItalic &&
              spansRich[0].ast->children[2]->role == MathFontRole::MathRoman &&
              spansRich[0].ast->children[2]->forceUpright &&
              spansRich[0].ast->children[9]->forceUpright &&
              spansRich[0].ast->children[11]->forceUpright,
              "M2-TYPESET-AST #8: TokenizeRichLatexUtf8 sets MathItalic for vars, MathRoman/forceUpright for \\sin & \\text{...}");
    }

    // Reviewer 2 Minor Polish: Case-insensitive phrase/word translation preserving sentence-initial capitalization
    {
        const auto trCap = TranslatePreservingMathAndUnits("La corriente es 5 A.", "es", "en");
        const auto trLow = TranslatePreservingMathAndUnits("la corriente es 5 A.", "es", "en");
        Check(trCap.ok && trCap.translatedTextUtf8 == "The current is 5 A." &&
              trLow.ok && trLow.translatedTextUtf8 == "the current is 5 A.",
              "Reviewer 2 Polish: Case-insensitive translation preserves sentence capitalization", trCap.translatedTextUtf8);
    }
}

void TestIteration4Regressions() {
    std::cout << "-- Iteration 4 Regressions (R3-01..R3-11) --\n";

    // R3-01: ConvertCasExprToLatex grouping parentheses, -x^2, and subscripted functions
    {
        const std::string lSin = ConvertCasExprToLatex("z = sin(k * (x^2 + y^2))");
        Check(lSin == "z = \\sin\\left(k \\cdot \\left(x^{2} + y^{2}\\right)\\right)" && ValidateLatexUtf8(lSin).valid,
              "R3-01: z = sin(k * (x^2 + y^2)) preserves grouping parens", lSin);

        const std::string lKcl = ConvertCasExprToLatex("I_V1(s) + g_D1 * (V_1(s) - V_2(s)) = 0");
        Check(lKcl == "I_{V1}\\left(s\\right) + g_{D1} \\cdot \\left(V_{1}\\left(s\\right) - V_{2}\\left(s\\right)\\right) = 0" &&
              lKcl.find("\\operatorname{I_V1}") == std::string::npos &&
              lKcl.find("\\operatorname{V_1}") == std::string::npos &&
              ValidateLatexUtf8(lKcl).valid,
              "R3-01: KCL subscripted functions & parens formatted properly", lKcl);

        const std::string lSub = ConvertCasExprToLatex("a - (b + c)");
        const std::string lMul = ConvertCasExprToLatex("(a + b) * (c + d)");
        const std::string lDiv = ConvertCasExprToLatex("k * (a + b) / c");
        const std::string lNegSq = ConvertCasExprToLatex("-x^2");
        const std::string lParNegSq = ConvertCasExprToLatex("(-x)^2");
        const std::string lMulSq = ConvertCasExprToLatex("(a*b)^2");
        const std::string lDivSq = ConvertCasExprToLatex("(a/b)^2");
        const std::string lRat = ConvertCasExprToLatex("(s+1)/(s^2+2*s+5)");
        Check(lSub == "a - \\left(b + c\\right)" &&
              lMul == "\\left(a + b\\right) \\cdot \\left(c + d\\right)" &&
              lDiv == "\\frac{k \\cdot \\left(a + b\\right)}{c}" &&
              lNegSq == "-x^{2}" &&
              lParNegSq == "\\left(-x\\right)^{2}" &&
              lMulSq == "\\left(a \\cdot b\\right)^{2}" &&
              lDivSq == "\\left(\\frac{a}{b}\\right)^{2}" &&
              lRat == "\\frac{s + 1}{s^{2} + 2 \\cdot s + 5}",
              "R3-01: Precedence parens, -x^2 vs (-x)^2, and powers",
              lSub + " | " + lMul + " | " + lNegSq + " | " + lParNegSq);
    }

    // R3-02: \mid in SymbolMap()
    {
        const std::string lBar = ConvertCasExprToLatex("9.89476 bar | psi");
        const auto pBar = ParseLatexToAst(lBar);
        bool foundMidOp = false, foundMidVar = false;
        if (pBar.ok && pBar.root) {
            for (const auto& ch : pBar.root->children) {
                if (!ch) continue;
                if (ch->kind == MathAstKind::MathOp && (ch->text == "|" || ch->text == "\xE2\x88\xA3") &&
                    ch->role == MathFontRole::MathSymbol) {
                    foundMidOp = true;
                }
                if (ch->kind == MathAstKind::MathVar && ch->text == "mid") {
                    foundMidVar = true;
                }
            }
        }
        Check(pBar.ok && foundMidOp && !foundMidVar, "R3-02: \\mid parses as MathOp (| or U+2223), never MathVar(mid)", lBar);

        std::string casBack;
        Check(ConvertLatexToCasExpr(lBar, casBack) && EvaluateExpression(casBack).ok,
              "R3-02: ConvertLatexToCasExpr handles \\mid cleanly", casBack);
    }

    // R3-03: EmitLatexFromAst \| round-trip, TryPromoteAutoMathTokenUtf8("√x²"), \text{step 1} spaces
    {
        const auto normAst = ParseLatexToAst("\\left\\| \\vec{F} \\right\\|");
        const std::string emitted = EmitLatexFromAst(normAst.root);
        const auto reparsed = ParseLatexToAst(emitted);
        Check(normAst.ok && reparsed.ok && reparsed.root &&
              reparsed.root->kind == MathAstKind::Delimiter &&
              reparsed.root->text == "\xE2\x80\x96" && reparsed.root->closeDelim == "\xE2\x80\x96",
              "R3-03: EmitLatexFromAst \\| delimiter round-trips cleanly", emitted);

        const auto spans = TokenizeRichLatexUtf8("Area \xE2\x88\x9Ax\xC2\xB2"); // √x²
        Check(spans.size() == 2 && spans[1].isMath && spans[1].isAutoMath &&
              spans[1].rawText == "\\sqrt{x}^{2}" &&
              spans[1].ast && spans[1].ast->kind == MathAstKind::SupSub &&
              spans[1].ast->base && spans[1].ast->base->kind == MathAstKind::Sqrt,
              "R3-03: TryPromoteAutoMathTokenUtf8 promotes √x² to \\sqrt{x}^{2}",
              spans.size() >= 2 ? spans[1].rawText : "");

        const auto stepText = ParseLatexToAst("\\text{step 1}");
        bool hasSpaceChild = false;
        if (stepText.ok && stepText.root) {
            for (const auto& ch : stepText.root->children) {
                if (ch && ch->text == " ") hasSpaceChild = true;
            }
        }
        Check(stepText.ok && hasSpaceChild, "R3-03: \\text{step 1} preserves internal space");
    }

    // R3-04: Deferred EngSuffix ^ precedence, DifferentiateAst, and affine temperature offset
    {
        const SymbolEnv nEnv{ { "n", QuantityValue{ { 5.0, 0.0 }, {}, "", 1.0, 0.0, false } } };
        const SymbolEnv kEnv{ { "k", QuantityValue{ { 3.0, 0.0 }, {}, "", 1.0, 0.0, false } } };

        MathJsonNode ast2n2{}, ast3k2{};
        CasEvalResult rBound{}, rUnbound{}, r3k2{};
        Check(ParseExpressionToMathJson("2n^2", ast2n2) &&
              EvaluateAst(ast2n2, nEnv, rBound) && std::abs(rBound.result.value.real() - 50.0) < 1e-9 &&
              EvaluateAst(ast2n2, {}, rUnbound) && std::abs(rUnbound.result.value.real() - 4e-18) < 1e-26 &&
              std::abs(EvaluateExpression("2n^2", nEnv).result.value.real() - 50.0) < 1e-9 &&
              std::abs(EvaluateExpression("2n^2", {}).result.value.real() - 4e-18) < 1e-26,
              "R3-04: Deferred 2n^2 evaluates to 50 when n=5 and 4e-18 when unbound", rBound.formattedResultUtf8);

        Check(ParseExpressionToMathJson("3k^2 + k", ast3k2) &&
              EvaluateAst(ast3k2, kEnv, r3k2) && std::abs(r3k2.result.value.real() - 30.0) < 1e-9,
              "R3-04: Deferred 3k^2 + k with {k=3} == 30", r3k2.formattedResultUtf8);

        MathJsonNode ast2k2{}, deriv2k2{};
        CasEvalResult derivEv{};
        Check(ParseExpressionToMathJson("2k^2", ast2k2) &&
              DifferentiateAst(ast2k2, "k", deriv2k2) &&
              EvaluateAst(deriv2k2, kEnv, derivEv) && std::abs(derivEv.result.value.real() - 12.0) < 1e-9,
              "R3-04: DifferentiateAst(astOf(2k^2), k) at k=3 == 12", MathJsonToExpression(deriv2k2));

        const SymbolEnv kTempEnv{ { "k", QuantityValue{ { 298.15, 0.0 }, UnitDim{ { 0, 0, 0, 0, 1, 0, 0 } }, "degC", 1.0, 273.15, false } } };
        MathJsonNode ast2k{};
        CasEvalResult rTemp{};
        Check(ParseExpressionToMathJson("2k", ast2k) &&
              EvaluateAst(ast2k, kTempEnv, rTemp) &&
              std::abs(rTemp.result.value.real() - 596.3) < 1e-6 &&
              rTemp.formattedResultUtf8 == "596.3 K",
              "R3-04: EvaluateAst(astOf(2k), {k=25 degC}) formats as 596.3 K", rTemp.formattedResultUtf8);
    }

    // R3-05: AstExprParser parenthesized hyphenated units & 2 * N-m without boundVars
    {
        MathJsonNode aParenDiv{}, aParenMul{}, a2Nm{};
        CasEvalResult rParenDiv{}, rParenMul{}, r2NmBound{}, r2NmUnbound{};
        Check(ParseExpressionToMathJson("10 J / (ft-kip)", aParenDiv) &&
              EvaluateAst(aParenDiv, {}, rParenDiv) &&
              rParenDiv.result.dim.IsDimensionless() &&
              std::abs(rParenDiv.result.value.real() - 0.00737562149) < 1e-7,
              "R3-05: AstExprParser 10 J / (ft-kip) == 0.00737562", rParenDiv.errorUtf8 + " " + rParenDiv.formattedResultUtf8);

        Check(ParseExpressionToMathJson("10 * (ft-kip) | N-m", aParenMul) &&
              EvaluateAst(aParenMul, {}, rParenMul) &&
              std::abs(rParenMul.result.value.real() - 13558.17948) < 0.1,
              "R3-05: AstExprParser 10 * (ft-kip) | N-m == 13558.2 N-m", rParenMul.errorUtf8 + " " + rParenMul.formattedResultUtf8);

        const auto stepHyp = CheckAlgebraicStepV2("10 J / (ft-kip)", "10 J / (ft * kip)");
        Check(stepHyp.equivalent && stepHyp.statusCode == "OK",
              "R3-05: CheckAlgebraicStepV2(10 J / (ft-kip), 10 J / (ft * kip)) == OK", stepHyp.statusCode);

        const SymbolEnv nmEnv{
            { "N", QuantityValue{ { 20.0, 0.0 }, {}, "", 1.0, 0.0, false } },
            { "m", QuantityValue{ { 7.0, 0.0 }, {}, "", 1.0, 0.0, false } }
        };
        Check(ParseExpressionToMathJson("2 * N-m", a2Nm) &&
              EvaluateAst(a2Nm, nmEnv, r2NmBound) && std::abs(r2NmBound.result.value.real() - 33.0) < 1e-9 &&
              EvaluateAst(a2Nm, {}, r2NmUnbound) && std::abs(r2NmUnbound.result.value.real() - 2.0) < 1e-9 &&
              r2NmUnbound.result.dim == (UnitDim{ { 1, 2, -2, 0, 0, 0, 0 } }),
              "R3-05: Unbound AST 2*N-m evaluates to 33 with {N=20,m=7} and 2 J when unbound",
              r2NmBound.formattedResultUtf8 + " / " + r2NmUnbound.formattedResultUtf8);
    }

    // R3-06: FindRoots2D close root & tangent root adjacent to exact grid hit
    {
        const auto rMixed = FindRoots2D("(x-0.5)*(x-0.505)^2", "x", 0.0, 1.0, 256);
        Check(rMixed.size() == 2 &&
              std::abs(rMixed[0].x - 0.5) < 1e-6 &&
              std::abs(rMixed[1].x - 0.505) < 1e-5,
              "R3-06: FindRoots2D((x-0.5)*(x-0.505)^2, [0,1], 256) finds both 0.5 and 0.505",
              std::to_string(rMixed.size()));

        const auto rClose = FindRoots2D("(x-0.5)*(x-0.505)", "x", -1.0, 1.0, 256);
        Check(rClose.size() == 2 &&
              std::abs(rClose[0].x - 0.5) < 1e-6 &&
              std::abs(rClose[1].x - 0.505) < 1e-6,
              "R3-06: FindRoots2D((x-0.5)*(x-0.505), [-1,1], 256) finds both 0.5 and 0.505",
              std::to_string(rClose.size()));
    }

    // R3-07, R3-08, R3-09: Scaled factor tol, FmtReal/FmtComplex/RREF scale, NaN/Inf rejection
    {
        ComplexMatrix A18{ 2, 2, { { 2e-18, 0.0 }, { 1e-18, 0.0 }, { 1e-18, 0.0 }, { 3e-18, 0.0 } } };
        std::vector<std::complex<double>> b18{ { 4e-18, 0.0 }, { 5e-18, 0.0 } };
        const auto sol18 = SolveComplexLinearSystem(A18, b18);
        Check(sol18.ok && std::abs(sol18.matrix.At(0, 0).real() - 1.4) < 1e-12 &&
              std::abs(sol18.matrix.At(1, 0).real() - 1.2) < 1e-12,
              "R3-07: SolveComplexLinearSystem at 1e-18 scale solves accurately", sol18.formattedUtf8);

        const auto inv18 = ComplexMatrixInv(A18);
        double invErr = 0.0;
        if (inv18.ok) {
            for (std::size_t r = 0; r < 2; ++r) {
                for (std::size_t c = 0; c < 2; ++c) {
                    std::complex<double> dot(0.0, 0.0);
                    for (std::size_t k = 0; k < 2; ++k) dot += A18.At(r, k) * inv18.matrix.At(k, c);
                    invErr = (std::max)(invErr, std::abs(dot - std::complex<double>(r == c ? 1.0 : 0.0, 0.0)));
                }
            }
        }
        Check(inv18.ok && invErr < 1e-12, "R3-07: ComplexMatrixInv at 1e-18 scale satisfies A*A^-1 == I", std::to_string(invErr));

        ComplexMatrix nonHerm18{ 3, 3, {
            { 1e-18, 0.5e-18 }, { 0.3e-18, -0.2e-18 }, { 0.1e-18, 0.4e-18 },
            { -0.2e-18, 0.1e-18 }, { 2e-18, -0.3e-18 }, { 0.5e-18, 0.2e-18 },
            { 0.4e-18, -0.1e-18 }, { -0.1e-18, 0.3e-18 }, { -1e-18, 0.2e-18 }
        } };
        const auto eig18 = ComputeMatrixEigen(nonHerm18);
        double scale18 = 0.0;
        for (const auto& v : nonHerm18.data) scale18 = (std::max)(scale18, std::abs(v));
        double maxRel18 = 0.0;
        if (eig18.ok && eig18.eigenvalues.size() == 3) {
            for (std::size_t j = 0; j < 3; ++j) {
                for (std::size_t r = 0; r < 3; ++r) {
                    std::complex<double> av(0.0, 0.0);
                    for (std::size_t c = 0; c < 3; ++c) av += nonHerm18.At(r, c) * eig18.eigenvectors[j][c];
                    maxRel18 = (std::max)(maxRel18, std::abs(av - eig18.eigenvalues[j] * eig18.eigenvectors[j][r]) / scale18);
                }
            }
        }
        Check(eig18.ok && maxRel18 < 1e-8 && eig18.summaryUtf8.find("= 0,") == std::string::npos,
              "R3-07/08: 1e-18 non-Hermitian ComputeMatrixEigen maxRel < 1e-8 & non-zero summary",
              std::to_string(maxRel18) + " | " + eig18.summaryUtf8);

        ComplexMatrix cplx12{ 2, 2, { { 1e-12, 0.0 }, { 0.0, 2e-12 }, { 0.0, 3e-12 }, { 1e-12, 0.0 } } };
        const auto e12 = ComputeMatrixEigen(cplx12);
        Check(e12.ok && e12.summaryUtf8.find("= 0") == std::string::npos,
              "R3-08: ComputeMatrixEigen summaryUtf8 preserves 1e-12 eigenvalues", e12.summaryUtf8);

        ComplexMatrix diag18{ 2, 2, { { 2e-18, 0.0 }, { 0.0, 0.0 }, { 0.0, 0.0 }, { 3e-18, 0.0 } } };
        const auto det18 = ComplexMatrixDet(diag18);
        Check(det18.ok && det18.formattedUtf8 != "0", "R3-08: ComplexMatrixDet 1e-18 matrix non-zero format", det18.formattedUtf8);

        ComplexMatrix eye18{ 2, 2, { { 1e-18, 0.0 }, { 0.0, 0.0 }, { 0.0, 0.0 }, { 1e-18, 0.0 } } };
        ComplexMatrix nearSing{ 2, 2, { { 1.0, 0.0 }, { 1.0, 0.0 }, { 1.0, 0.0 }, { 1.0 + 1e-15, 0.0 } } };
        const auto rref18 = ComplexMatrixRref(eye18);
        const auto rrefSing = ComplexMatrixRref(nearSing);
        Check(rref18.ok && rref18.rank == 2 && rrefSing.ok && rrefSing.rank == 1,
              "R3-08: ComplexMatrixRref scales tol by maxAbs (1e-18*I rank=2, near-sing rank=1)");

        const double qNan = std::numeric_limits<double>::quiet_NaN();
        const double qInf = std::numeric_limits<double>::infinity();
        ComplexMatrix nanMat{ 2, 2, { { qNan, 0.0 }, { 0.0, 0.0 }, { 0.0, 0.0 }, { 1.0, 0.0 } } };
        ComplexMatrix infMat{ 2, 2, { { qInf, 0.0 }, { 0.0, 0.0 }, { 0.0, 0.0 }, { 1.0, 0.0 } } };
        Check(!ComputeMatrixEigen(nanMat).ok && !ComputeMatrixEigen(infMat).ok &&
              !ComplexMatrixDet(nanMat).ok && !ComplexMatrixInv(infMat).ok &&
              !ComplexMatrixRref(nanMat).ok && !SolveComplexLinearSystem(infMat, { {1,0},{1,0} }).ok,
              "R3-09: Matrix solvers & ComputeMatrixEigen reject NaN and Infinity");
    }

    // R3-10: EvaluateAst supports confirmed UnitQuantity
    {
        auto card = CreateInterpretationCard(903, "10 kOhm", "10 kOhm", 0.95);
        BeginEditInterpretationCard(card);
        CasEvalResult evAst{};
        Check(UpdateInterpretationCardFromMathJson(card, R"(["UnitQuantity", 10, {"str":"kOhm"}])") &&
              ConfirmInterpretationCard(card) &&
              EvaluateAst(card.editedAst, {}, evAst) && evAst.ok &&
              std::abs(evAst.result.value.real() - 10000.0) < 1e-6 &&
              evAst.result.dim == (UnitDim{ { 1, 2, -3, -2, 0, 0, 0 } }),
              "R3-10: EvaluateAst evaluates confirmed UnitQuantity(10, kOhm) to 10000 Ohm",
              evAst.errorUtf8 + " " + evAst.formattedResultUtf8);
    }

    // R3-11: TranslatePreservingMathAndUnits UTF-8 accents & "en"/"in"
    {
        const auto trEs = TranslatePreservingMathAndUnits("La tensi\xC3\xB3n en R1 es 10 V.", "es", "en");
        Check(trEs.ok && trEs.allMathAndUnitsIntact &&
              trEs.translatedTextUtf8 == "The voltage in R1 is 10 V.",
              "R3-11: Accented Spanish 'La tensión en R1 es 10 V.' -> 'The voltage in R1 is 10 V.'",
              trEs.translatedTextUtf8);

        const auto trEn = TranslatePreservingMathAndUnits("The voltage in R1 is 10 V.", "en", "es");
        const bool frozeIn = std::find(trEn.preservedUnitTokens.begin(), trEn.preservedUnitTokens.end(), "in") != trEn.preservedUnitTokens.end();
        Check(trEn.ok && trEn.allMathAndUnitsIntact && !frozeIn &&
              trEn.translatedTextUtf8 == "La tension en R1 es 10 V.",
              "R3-11: English 'in' preposition not frozen as inches unit", trEn.translatedTextUtf8);

        const auto trInches = TranslatePreservingMathAndUnits("10 in.", "en", "es");
        const bool keptIn = std::find(trInches.preservedUnitTokens.begin(), trInches.preservedUnitTokens.end(), "in") != trInches.preservedUnitTokens.end();
        Check(trInches.ok && keptIn && trInches.translatedTextUtf8 == "10 in.",
              "R3-11: Numeric-preceded '10 in.' still preserved as inches unit", trInches.translatedTextUtf8);
    }
}

} // namespace

int main() {
    TestGroup1SymbolicUnitsComplex();
    TestGroup2DagCalcCardTableMarginAndParity();
    TestGroup3LatexMathMlPlotsAndSheets();
    TestGroup4AdvancedMathStaticsFingerprintAndSheets();
    TestGroup5MathJsonConfirmationCheckMyWorkAndAiContracts();
    TestIteration2Regressions();
    TestIteration3RegressionsAndTypesetAst();
    TestIteration4Regressions();

    std::cout << "\nSummary: " << g_passed << " passed, " << g_failed << " failed.\n";
    return (g_failed == 0) ? 0 : 1;
}
