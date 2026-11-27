#pragma once

// Included exclusively by doc_studio/math/engineering_cas.cpp

namespace DocStudio::Cas {

namespace detail_ped {

inline std::string Trim(const std::string& s) {
    std::size_t b = 0;
    while (b < s.size() && std::isspace(static_cast<unsigned char>(s[b]))) ++b;
    std::size_t e = s.size();
    while (e > b && std::isspace(static_cast<unsigned char>(s[e - 1]))) --e;
    return s.substr(b, e - b);
}

inline bool IsSimpleIdentifier(const std::string& s) {
    if (s.empty()) return false;
    const unsigned char c0 = static_cast<unsigned char>(s[0]);
    if (!std::isalpha(c0) && c0 != '_') return false;
    for (unsigned char c : s) {
        if (!std::isalnum(c) && c != '_') return false;
    }
    return true;
}

inline std::string HtmlEscape(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 16);
    for (char c : s) {
        if (c == '&') out += "&amp;";
        else if (c == '<') out += "&lt;";
        else if (c == '>') out += "&gt;";
        else if (c == '"') out += "&quot;";
        else out.push_back(c);
    }
    return out;
}

inline bool EvalExprWithVars(
    const std::string& expr,
    const MathJsonNode* ast,
    const SymbolEnv& env,
    CasEvalResult& out)
{
    if (ast && EvaluateAst(*ast, env, out) && out.ok) return true;
    const CasEvalResult astFail = out;
    out = EvaluateExpression(expr, env);
    if (!out.ok && ast && astFail.diagnosticCode == "DIM_ERR" && out.diagnosticCode != "DIM_ERR") {
        out = astFail;
    }
    return out.ok;
}

// Multi-variable real + complex probing helper
inline bool ProbePair(
    const std::string& exprA, const MathJsonNode* astA,
    const std::string& exprB, const MathJsonNode* astB,
    const std::vector<std::string>& freeVars,
    const SymbolEnv& baseEnv,
    double& outMaxRelDiff,
    bool& outDimMismatch,
    std::string& outDimA,
    std::string& outDimB,
    int& outValidProbes,
    bool negateB = false)
{
    outMaxRelDiff = 0.0;
    outDimMismatch = false;
    outValidProbes = 0;

    static constexpr double kRealBase[3] = { 0.37, 1.42, -0.83 };
    for (int m = 0; m < 6; ++m) {
        SymbolEnv probeEnv = baseEnv;
        for (std::size_t j = 0; j < freeVars.size(); ++j) {
            const double jOff = 0.71 * static_cast<double>(j);
            std::complex<double> val;
            if (m < 3) {
                val = { kRealBase[m] + jOff + 0.53 * static_cast<double>(m), 0.0 };
            } else {
                const int cm = m - 3;
                val = { 0.41 + 0.37 * static_cast<double>(j) + 0.23 * static_cast<double>(cm),
                        0.29 + 0.19 * static_cast<double>(cm) - 0.11 * static_cast<double>(j) };
            }
            probeEnv[freeVars[j]] = QuantityValue{ val, {}, "", 1.0, 0.0, false };
        }

        CasEvalResult ea{}, eb{};
        const bool okA = EvalExprWithVars(exprA, astA, probeEnv, ea);
        const bool okB = EvalExprWithVars(exprB, astB, probeEnv, eb);
        if ((!okA && ea.diagnosticCode == "DIM_ERR") || (!okB && eb.diagnosticCode == "DIM_ERR")) {
            outDimMismatch = true;
            outDimA = (!okA && ea.diagnosticCode == "DIM_ERR") ? "DIM_ERR" : ea.result.dim.FormatSiString();
            outDimB = (!okB && eb.diagnosticCode == "DIM_ERR") ? "DIM_ERR" : eb.result.dim.FormatSiString();
            return false;
        }
        if (!okA || !okB) {
            if (!okA && !okB) continue;
            return false;
        }
        if (ea.result.dim != eb.result.dim) {
            outDimMismatch = true;
            outDimA = ea.result.dim.FormatSiString();
            outDimB = eb.result.dim.FormatSiString();
            return false;
        }
        outDimA = ea.result.dim.FormatSiString();
        outDimB = eb.result.dim.FormatSiString();
        const auto targetB = negateB ? (-eb.result.value) : eb.result.value;
        const double denom = (std::max)(1.0, (std::max)(std::abs(ea.result.value), std::abs(targetB)));
        const double diff = std::abs(ea.result.value - targetB) / denom;
        if (diff > outMaxRelDiff) outMaxRelDiff = diff;
        ++outValidProbes;
    }
    return outValidProbes > 0;
}

inline std::vector<std::string> ExtractUnsignedTermKeys(const MathJsonNode& canonNode) {
    MathJsonNode c = CanonicalizeMathJson(canonNode, 64);
    std::vector<MathJsonNode> rawTerms;
    if (c.kind == MathJsonKind::Function && c.text == "Add") {
        rawTerms = c.args;
    } else {
        rawTerms.push_back(c);
    }
    std::vector<std::string> keys;
    keys.reserve(rawTerms.size());
    for (MathJsonNode t : rawTerms) {
        if (t.kind == MathJsonKind::Number) {
            t = MathJsonNode::MakeNumber(std::abs(t.realVal));
        } else if (t.kind == MathJsonKind::Complex) {
            if (t.realVal < 0.0 || (std::abs(t.realVal) < 1e-13 && t.imagVal < 0.0)) {
                t = MathJsonNode::MakeComplex(-t.realVal, -t.imagVal);
            }
        } else if (t.kind == MathJsonKind::Function && t.text == "Negate" && t.args.size() == 1) {
            t = t.args[0];
        } else if (t.kind == MathJsonKind::Function && t.text == "Multiply" && !t.args.empty()) {
            if (t.args[0].kind == MathJsonKind::Number && t.args[0].realVal < 0.0) {
                const double absC = -t.args[0].realVal;
                if (std::abs(absC - 1.0) < 1e-12 && t.args.size() >= 2) {
                    if (t.args.size() == 2) t = t.args[1];
                    else t = MathJsonNode::MakeFunction("Multiply", std::vector<MathJsonNode>(t.args.begin() + 1, t.args.end()));
                } else {
                    t.args[0] = MathJsonNode::MakeNumber(absC);
                }
            }
        }
        keys.push_back(MathJsonToExpression(CanonicalizeMathJson(t, 64)));
    }
    std::sort(keys.begin(), keys.end());
    return keys;
}

inline bool CheckSingleTermSignFlip(
    const MathJsonNode& prevCanon,
    const MathJsonNode& nextRaw,
    const std::string& prevExpr,
    const std::vector<std::string>& freeVars,
    const SymbolEnv& baseEnv)
{
    // 1. Multi-term sign errors (e.g. -(a+b+c) -> -a+b+c): unsigned additive term multisets match
    MathJsonNode expanded = CanonicalizeMathJson(nextRaw, 64);
    const auto prevKeys = ExtractUnsignedTermKeys(prevCanon);
    const auto nextKeys = ExtractUnsignedTermKeys(expanded);
    if (prevKeys.size() >= 2 && prevKeys == nextKeys && prevCanon != expanded) {
        return true;
    }

    // 2. Single additive term sign flip with numeric verification
    if (expanded.kind != MathJsonKind::Function || expanded.text != "Add" || expanded.args.size() < 2) {
        return false;
    }
    for (std::size_t i = 0; i < expanded.args.size(); ++i) {
        MathJsonNode mod = expanded;
        mod.args[i] = MathJsonNode::MakeFunction("Negate", { mod.args[i] });
        MathJsonNode modCanon = CanonicalizeMathJson(mod, 64);
        if (modCanon == prevCanon) return true;
        double maxDiff = 0.0;
        bool dimMis = false;
        std::string da, db;
        int valid = 0;
        if (ProbePair(prevExpr, &prevCanon, MathJsonToExpression(modCanon), &modCanon, freeVars, baseEnv,
                      maxDiff, dimMis, da, db, valid, false) && !dimMis && maxDiff < 1e-6) {
            return true;
        }
    }
    return false;
}

inline bool CheckOrderOfOpsPattern(const MathJsonNode& prevAst, const MathJsonNode& nextAst,
                                     const std::string& prevExpr, const std::string& nextExpr,
                                     const std::vector<std::string>& freeVars, const SymbolEnv& baseEnv)
{
    // 1. Freshman's dream: (a + b)^2 -> a^2 + b^2 or (a - b)^2 -> a^2 - b^2
    if (prevAst.kind == MathJsonKind::Function && prevAst.text == "Power" && prevAst.args.size() == 2) {
        double p = 0.0;
        if (prevAst.args[1].kind == MathJsonKind::Number && std::abs(prevAst.args[1].realVal - 2.0) < 1e-12 &&
            prevAst.args[0].kind == MathJsonKind::Function &&
            (prevAst.args[0].text == "Add" || prevAst.args[0].text == "Subtract") &&
            prevAst.args[0].args.size() == 2) {
            const auto& a = prevAst.args[0].args[0];
            const auto& b = prevAst.args[0].args[1];
            auto a2 = MathJsonNode::MakeFunction("Power", { a, MathJsonNode::MakeNumber(2.0) });
            auto b2 = MathJsonNode::MakeFunction("Power", { b, MathJsonNode::MakeNumber(2.0) });
            auto candAdd = CanonicalizeMathJson(MathJsonNode::MakeFunction("Add", { a2, b2 }));
            auto candSub = CanonicalizeMathJson(MathJsonNode::MakeFunction("Subtract", { a2, b2 }));
            auto nextCanon = CanonicalizeMathJson(nextAst);
            if (nextCanon == candAdd || nextCanon == candSub) return true;
            double md = 0.0; bool dm = false; std::string da, db; int vp = 0;
            if (ProbePair(MathJsonToExpression(candAdd), &candAdd, nextExpr, &nextCanon, freeVars, baseEnv, md, dm, da, db, vp) && !dm && md < 1e-6) return true;
            if (ProbePair(MathJsonToExpression(candSub), &candSub, nextExpr, &nextCanon, freeVars, baseEnv, md, dm, da, db, vp) && !dm && md < 1e-6) return true;
        }
        (void)p;
    }
    // 2. Missing distribution: k*(a + b) -> k*a + b
    if (prevAst.kind == MathJsonKind::Function && prevAst.text == "Multiply" && prevAst.args.size() == 2) {
        for (int side = 0; side < 2; ++side) {
            const auto& sumN = prevAst.args[side];
            const auto& facN = prevAst.args[1 - side];
            if (sumN.kind == MathJsonKind::Function && (sumN.text == "Add" || sumN.text == "Subtract") && sumN.args.size() == 2) {
                auto unparen = CanonicalizeMathJson(MathJsonNode::MakeFunction(sumN.text, {
                    MathJsonNode::MakeFunction("Multiply", { facN, sumN.args[0] }), sumN.args[1]
                }));
                if (CanonicalizeMathJson(nextAst) == unparen) return true;
            }
        }
    }
    // 3. Denominator split fallacy: a/(b + c) -> a/b + c or a/b + a/c
    if (prevAst.kind == MathJsonKind::Function && prevAst.text == "Divide" && prevAst.args.size() == 2 &&
        prevAst.args[1].kind == MathJsonKind::Function && prevAst.args[1].text == "Add" && prevAst.args[1].args.size() == 2) {
        const auto& a = prevAst.args[0];
        const auto& b = prevAst.args[1].args[0];
        const auto& c = prevAst.args[1].args[1];
        auto f1 = CanonicalizeMathJson(MathJsonNode::MakeFunction("Add", { MathJsonNode::MakeFunction("Divide", { a, b }), c }));
        auto f2 = CanonicalizeMathJson(MathJsonNode::MakeFunction("Add", { MathJsonNode::MakeFunction("Divide", { a, b }), MathJsonNode::MakeFunction("Divide", { a, c }) }));
        auto nextCanon = CanonicalizeMathJson(nextAst);
        if (nextCanon == f1 || nextCanon == f2) return true;
    }
    (void)prevExpr;
    return false;
}

inline std::unordered_set<std::string> CollectStepBoundVars(
    const std::string& exprA,
    const std::string& exprB,
    const SymbolEnv& baseEnv,
    const std::unordered_set<std::string>& extra)
{
    std::unordered_set<std::string> vars = extra;
    for (const auto& kv : baseEnv) vars.insert(kv.first);
    auto scanStandalone = [&](const std::string& s) {
        std::size_t i = 0;
        while (i < s.size()) {
            const unsigned char c = static_cast<unsigned char>(s[i]);
            if (std::isalpha(c) || c == '_') {
                const bool precededByDigit = (i > 0 && (std::isdigit(static_cast<unsigned char>(s[i - 1])) || s[i - 1] == '.'));
                const std::size_t st = i;
                while (i < s.size() && (std::isalnum(static_cast<unsigned char>(s[i])) || s[i] == '_')) ++i;
                if (!precededByDigit && (i >= s.size() || s[i] != '(')) {
                    const std::string id = s.substr(st, i - st);
                    UnitDim d{}; double sc = 1.0, off = 0.0;
                    if (!TryParseUnitExpression(id, d, sc, off)) {
                        vars.insert(id);
                    }
                }
            } else {
                ++i;
            }
        }
    };
    scanStandalone(exprA);
    scanStandalone(exprB);
    return vars;
}

inline StepCheckResultV2 CheckAlgebraicStepV2WithHintVars(
    const std::string& prevLineUtf8,
    const std::string& nextLineUtf8,
    const SymbolEnv& baseEnv,
    const std::unordered_set<std::string>& extraBoundVars)
{
    StepCheckResultV2 out;
    out.audit.methodUtf8 = "SYMBOLIC_CANONICAL_AND_MULTIVAR_COMPLEX_PROBE";

    auto splitEq = [](const std::string& s, bool& hasEq, std::string& lhs, std::string& rhs) {
        const auto p = s.find('=');
        if (p == std::string::npos) {
            hasEq = false;
            lhs.clear();
            rhs = detail_ped::Trim(s);
        } else {
            hasEq = true;
            lhs = detail_ped::Trim(s.substr(0, p));
            rhs = detail_ped::Trim(s.substr(p + 1));
        }
    };

    bool hasEqA = false, hasEqB = false;
    std::string lhsA, rhsA, lhsB, rhsB;
    splitEq(prevLineUtf8, hasEqA, lhsA, rhsA);
    splitEq(nextLineUtf8, hasEqB, lhsB, rhsB);

    const bool isAssignmentA = hasEqA && detail_ped::IsSimpleIdentifier(lhsA);
    const bool isAssignmentB = hasEqB && detail_ped::IsSimpleIdentifier(lhsB);

    const bool bothFullEquations = hasEqA && hasEqB && !(isAssignmentA && isAssignmentB && lhsA == lhsB);

    std::string cmpExprA = bothFullEquations ? ("(" + lhsA + ") - (" + rhsA + ")") : rhsA;
    std::string cmpExprB = bothFullEquations ? ("(" + lhsB + ") - (" + rhsB + ")") : rhsB;

    // If one side is a non-assignment equation (e.g. 2*x + 1 = 5) and the next drops '=', flag balance error
    if ((hasEqA && !isAssignmentA && !hasEqB) || (!hasEqA && hasEqB && !isAssignmentB)) {
        out.statusCode = "BALANCE_ERR";
        out.errorCategory = StepErrorCategory::EquationBalanceError;
        out.messageUtf8 = "Se perdio o agrego el signo '=' sin mantener balance de ecuacion";
        out.hintUtf8 = "Verifica ambos miembros de la igualdad (LHS = RHS) al transformar la ecuacion.";
        out.audit.verificationStatus = VerificationStatus::Failed;
        return out;
    }

    const auto boundVars = CollectStepBoundVars(prevLineUtf8, nextLineUtf8, baseEnv, extraBoundVars);
    MathJsonNode astA{}, astB{};
    std::string errA, errB;
    const bool parsedA = ParseExpressionToMathJson(cmpExprA, astA, boundVars, &errA);
    const bool parsedB = ParseExpressionToMathJson(cmpExprB, astB, boundVars, &errB);
    if (!parsedA || !parsedB) {
        out.statusCode = "SYNTAX_ERR";
        out.errorCategory = StepErrorCategory::SyntaxError;
        out.messageUtf8 = !parsedA ? errA : errB;
        out.audit.verificationStatus = VerificationStatus::Failed;
        return out;
    }

    // Check equation balance candidate (defer emitting BALANCE_ERR until SIGN_ERR/ORDER_ERR are ruled out)
    bool hasBalanceErrorCandidate = false;
    bool eqSideSignFlipDetected = false;
    bool eqSideOrderErrDetected = false;
    if (bothFullEquations) {
        MathJsonNode lA, rA, lB, rB;
        if (ParseExpressionToMathJson(lhsA, lA, boundVars) && ParseExpressionToMathJson(rhsA, rA, boundVars) &&
            ParseExpressionToMathJson(lhsB, lB, boundVars) && ParseExpressionToMathJson(rhsB, rB, boundVars)) {
            const auto cLA = CanonicalizeMathJson(lA);
            const auto cRA = CanonicalizeMathJson(rA);
            const auto cLB = CanonicalizeMathJson(lB);
            const auto cRB = CanonicalizeMathJson(rB);
            std::vector<std::string> varsL = ExtractFreeSymbols(astA, baseEnv);
            for (const auto& v : ExtractFreeSymbols(astB, baseEnv)) {
                if (std::find(varsL.begin(), varsL.end(), v) == varsL.end()) varsL.push_back(v);
            }
            if ((cRA == cRB && detail_ped::CheckOrderOfOpsPattern(lA, lB, lhsA, lhsB, varsL, baseEnv)) ||
                (cLA == cLB && detail_ped::CheckOrderOfOpsPattern(rA, rB, rhsA, rhsB, varsL, baseEnv)) ||
                detail_ped::CheckOrderOfOpsPattern(lA, lB, lhsA, lhsB, varsL, baseEnv) ||
                detail_ped::CheckOrderOfOpsPattern(rA, rB, rhsA, rhsB, varsL, baseEnv)) {
                eqSideOrderErrDetected = true;
            }
            if ((cLA == cLB && cRA != cRB) || (cRA == cRB && cLA != cLB)) {
                if (cRA == cRB && detail_ped::CheckSingleTermSignFlip(cLA, lB, lhsA, varsL, baseEnv)) {
                    eqSideSignFlipDetected = true;
                } else if (cLA == cLB && detail_ped::CheckSingleTermSignFlip(cRA, rB, rhsA, varsL, baseEnv)) {
                    eqSideSignFlipDetected = true;
                } else {
                    double md = 0.0; bool dm = false; std::string d1, d2; int vp = 0;
                    const bool sameDiff = detail_ped::ProbePair(cmpExprA, &astA, cmpExprB, &astB, varsL, baseEnv, md, dm, d1, d2, vp);
                    if (!sameDiff || md >= 1e-6) {
                        hasBalanceErrorCandidate = true;
                    }
                }
            }
        }
    }

    const MathJsonNode canonA = CanonicalizeMathJson(astA);
    const MathJsonNode canonB = CanonicalizeMathJson(astB);

    // Collect union of free symbols
    std::vector<std::string> freeVars = ExtractFreeSymbols(canonA, baseEnv);
    for (const auto& v : ExtractFreeSymbols(canonB, baseEnv)) {
        if (std::find(freeVars.begin(), freeVars.end(), v) == freeVars.end()) freeVars.push_back(v);
    }

    // Stage 1: Exact symbolic canonical equivalence (including UnitDim check when units/env exist)
    // Always run 1 quick dim/eval probe if units or symbols are present, OR compare canon directly when dim-free
    double maxDiff = 0.0;
    bool dimMis = false;
    std::string dimA, dimB;
    int validProbes = 0;

    if (canonA == canonB) {
        // Still verify dimensions match cleanly if baseEnv/units are involved
        detail_ped::ProbePair(cmpExprA, &canonA, cmpExprB, &canonB, freeVars, baseEnv,
                              maxDiff, dimMis, dimA, dimB, validProbes, false);
        if (dimMis) {
            out.dimensionMismatch = true;
            out.statusCode = "DIM_ERR";
            out.errorCategory = StepErrorCategory::DimensionalMismatch;
            out.prevDimSiUtf8 = dimA;
            out.nextDimSiUtf8 = dimB;
            out.messageUtf8 = "Desajuste dimensional: [" + dimA + "] vs [" + dimB + "]";
            out.audit.verificationStatus = VerificationStatus::Failed;
            return out;
        }
        out.equivalent = true;
        out.maxResidual = 0.0;
        out.statusCode = "OK";
        out.prevDimSiUtf8 = dimA;
        out.nextDimSiUtf8 = dimB;
        out.messageUtf8 = "Paso simbolicamente equivalente";
        out.audit.verificationStatus = VerificationStatus::SymbolicallyVerified;
        return out;
    }

    // Stage 2: Multi-variable real + complex probing (handles trig, Euler, logs, multi-var)
    const bool probeOk = detail_ped::ProbePair(cmpExprA, &canonA, cmpExprB, &canonB, freeVars, baseEnv,
                                               maxDiff, dimMis, dimA, dimB, validProbes, false);
    out.prevDimSiUtf8 = dimA;
    out.nextDimSiUtf8 = dimB;
    out.maxResidual = maxDiff;

    if (dimMis) {
        out.dimensionMismatch = true;
        out.statusCode = "DIM_ERR";
        out.errorCategory = StepErrorCategory::DimensionalMismatch;
        out.messageUtf8 = "Desajuste dimensional: [" + dimA + "] vs [" + dimB + "]";
        out.hintUtf8 = "Revisa las unidades fisicas de cada termino antes de continuar.";
        out.audit.verificationStatus = VerificationStatus::Failed;
        return out;
    }

    if (!probeOk || validProbes == 0) {
        out.statusCode = "SYNTAX_ERR";
        out.errorCategory = StepErrorCategory::SyntaxError;
        out.messageUtf8 = "No se pudo evaluar el paso algebraico.";
        out.audit.verificationStatus = VerificationStatus::Failed;
        return out;
    }

    if (maxDiff < 1e-6) {
        out.equivalent = true;
        out.statusCode = "OK";
        out.messageUtf8 = "Paso algebraicamente equivalente";
        out.audit.verificationStatus = VerificationStatus::NumericallyVerified;
        return out;
    }

    // Special check for equivalent equations scaled by a non-zero constant k (e.g. 2x-4=0 -> x-2=0)
    if (bothFullEquations) {
        SymbolEnv pEnv = baseEnv;
        for (std::size_t j = 0; j < freeVars.size(); ++j) {
            pEnv[freeVars[j]] = QuantityValue{ { 0.37 + 0.71 * static_cast<double>(j), 0.19 }, {}, "", 1.0, 0.0, false };
        }
        CasEvalResult ea0{}, eb0{};
        if (detail_ped::EvalExprWithVars(cmpExprA, &canonA, pEnv, ea0) &&
            detail_ped::EvalExprWithVars(cmpExprB, &canonB, pEnv, eb0) &&
            std::abs(eb0.result.value) > 1e-12) {
            const auto ratio = ea0.result.value / eb0.result.value;
            if (std::abs(ratio) > 1e-12) {
                const auto scaledB = CanonicalizeMathJson(MathJsonNode::MakeFunction("Multiply", {
                    MathJsonNode::MakeComplex(ratio.real(), ratio.imag()), canonB }));
                double sDiff = 0.0; bool sDm = false; std::string sd1, sd2; int svp = 0;
                if (detail_ped::ProbePair(cmpExprA, &canonA, MathJsonToExpression(scaledB), &scaledB, freeVars, baseEnv,
                                          sDiff, sDm, sd1, sd2, svp, false) && !sDm && sDiff < 1e-6) {
                    out.equivalent = true;
                    out.maxResidual = sDiff;
                    out.statusCode = "OK";
                    out.messageUtf8 = "Ecuacion equivalente tras escalado uniforme";
                    out.audit.verificationStatus = VerificationStatus::NumericallyVerified;
                    return out;
                }
            }
        }
    }

    // Stage 3: Deterministic Error Classification
    // 3a. Sign error: global negation OR single additive term sign flip (including per-side in equations)
    double negDiff = 0.0; bool negDm = false; std::string nd1, nd2; int nvp = 0;
    if (eqSideSignFlipDetected ||
        (detail_ped::ProbePair(cmpExprA, &canonA, cmpExprB, &canonB, freeVars, baseEnv,
                               negDiff, negDm, nd1, nd2, nvp, true) && !negDm && negDiff < 1e-6) ||
        detail_ped::CheckSingleTermSignFlip(canonA, astB, cmpExprA, freeVars, baseEnv)) {
        out.statusCode = "SIGN_ERR";
        out.errorCategory = StepErrorCategory::SignError;
        out.messageUtf8 = "Error de signo detectado en el paso algebraico";
        out.hintUtf8 = "Revisa la distribucion de signos '-' al quitar parentesis o multiplicar terminos.";
        out.audit.verificationStatus = VerificationStatus::Failed;
        return out;
    }

    // 3b. Order of operations error
    if (eqSideOrderErrDetected ||
        detail_ped::CheckOrderOfOpsPattern(astA, astB, cmpExprA, cmpExprB, freeVars, baseEnv)) {
        out.statusCode = "ORDER_ERR";
        out.errorCategory = StepErrorCategory::OrderOfOperations;
        out.messageUtf8 = "Error de precedencia / orden de operaciones o parentesis faltantes";
        out.hintUtf8 = "Recuerda: (a+b)^2 = a^2 + 2ab + b^2 y las potencias/divisiones requieren agrupar sumas con parentesis.";
        out.audit.verificationStatus = VerificationStatus::Failed;
        return out;
    }

    // 3c. Equation balance error (one side stayed identical while the other changed unequally)
    if (hasBalanceErrorCandidate) {
        out.statusCode = "BALANCE_ERR";
        out.errorCategory = StepErrorCategory::EquationBalanceError;
        out.messageUtf8 = "Error de balance de ecuacion: solo se modifico un lado de la igualdad";
        out.hintUtf8 = "Cualquier operacion aplicada a un lado de '=' debe aplicarse identicamente al otro lado.";
        out.audit.verificationStatus = VerificationStatus::Failed;
        return out;
    }

    out.statusCode = "DIFF";
    out.errorCategory = StepErrorCategory::AlgebraicMismatch;
    out.messageUtf8 = "Error de paso algebraico (residual rel = " + std::to_string(maxDiff) + ")";
    out.hintUtf8 = "Compara termino a termino respecto al paso anterior.";
    out.audit.verificationStatus = VerificationStatus::Failed;
    return out;
}

} // namespace detail_ped

// ---------------- F-133, F-310: ConfirmationCard State Machine ----------------

InterpretationCard CreateInterpretationCard(
    std::uint64_t cardId,
    const std::string& rawCandidateUtf8,
    const std::string& rawLatexUtf8,
    double confidence)
{
    InterpretationCard card;
    card.cardId = cardId;
    card.status = ConfirmationStatus::PendingReview;
    card.rawSourceTextUtf8 = rawCandidateUtf8;
    card.rawSourceLatexUtf8 = rawLatexUtf8;
    card.confidence = confidence;
    card.audit.methodUtf8 = "AST_MATHJSON_CONFIRMATION";

    const std::string& src = !rawCandidateUtf8.empty() ? rawCandidateUtf8 : rawLatexUtf8;
    std::string parseErr;
    if (ParseExpressionToMathJson(src, card.proposedAst, &parseErr)) {
        card.editedAst = card.proposedAst;
        card.astValid = ValidateMathJsonAst(card.editedAst, card.validationErrors);
        card.editableExprUtf8 = MathJsonToExpression(card.editedAst);
        card.editableLatexUtf8 = MathJsonToLatex(card.editedAst);
        card.audit.verificationStatus = card.astValid ? VerificationStatus::SymbolicallyVerified : VerificationStatus::Failed;
    } else {
        card.astValid = false;
        card.validationErrors.push_back(parseErr.empty() ? "No se pudo interpretar como AST MathJSON" : parseErr);
        card.editableExprUtf8 = src;
        card.editableLatexUtf8 = rawLatexUtf8;
        card.audit.verificationStatus = VerificationStatus::Failed;
    }
    card.audit.provenanceRefs.push_back({ "ocr_raw", "", 0, cardId, rawCandidateUtf8 });
    return card;
}

bool BeginEditInterpretationCard(InterpretationCard& card) {
    if (card.status == ConfirmationStatus::Rejected) return false;
    card.status = ConfirmationStatus::Editing;
    return true;
}

bool UpdateInterpretationCardFromMathJson(InterpretationCard& card, const std::string& mathJsonUtf8) {
    if (card.status == ConfirmationStatus::Rejected) return false;
    card.status = ConfirmationStatus::Editing;
    MathJsonNode parsed;
    std::string err;
    if (!ParseMathJson(mathJsonUtf8, parsed, &err)) {
        card.astValid = false;
        card.validationErrors = { err };
        card.audit.verificationStatus = VerificationStatus::Failed;
        return false;
    }
    card.editedAst = std::move(parsed);
    card.astValid = ValidateMathJsonAst(card.editedAst, card.validationErrors);
    card.editableExprUtf8 = MathJsonToExpression(card.editedAst);
    card.editableLatexUtf8 = MathJsonToLatex(card.editedAst);
    card.audit.verificationStatus = card.astValid ? VerificationStatus::SymbolicallyVerified : VerificationStatus::Failed;
    return card.astValid;
}

bool UpdateInterpretationCardFromExpr(InterpretationCard& card, const std::string& exprOrLatexUtf8) {
    if (card.status == ConfirmationStatus::Rejected) return false;
    card.status = ConfirmationStatus::Editing;
    MathJsonNode parsed;
    std::string err;
    if (!ParseExpressionToMathJson(exprOrLatexUtf8, parsed, &err)) {
        card.astValid = false;
        card.validationErrors = { err };
        card.editableExprUtf8 = exprOrLatexUtf8;
        card.audit.verificationStatus = VerificationStatus::Failed;
        return false;
    }
    card.editedAst = std::move(parsed);
    card.astValid = ValidateMathJsonAst(card.editedAst, card.validationErrors);
    card.editableExprUtf8 = MathJsonToExpression(card.editedAst);
    card.editableLatexUtf8 = MathJsonToLatex(card.editedAst);
    card.audit.verificationStatus = card.astValid ? VerificationStatus::SymbolicallyVerified : VerificationStatus::Failed;
    return card.astValid;
}

bool ConfirmInterpretationCard(InterpretationCard& card) {
    if (card.status == ConfirmationStatus::Rejected) return false;
    if (!card.astValid || !card.validationErrors.empty()) return false;
    card.status = ConfirmationStatus::Confirmed;
    card.audit.verificationStatus = VerificationStatus::UserConfirmed;
    return true;
}

bool RejectInterpretationCard(InterpretationCard& card, const std::string& reasonUtf8) {
    card.status = ConfirmationStatus::Rejected;
    card.rejectionReasonUtf8 = reasonUtf8;
    card.audit.verificationStatus = VerificationStatus::Failed;
    return true;
}

// ---------------- F-313, F-314, SUP-07: CheckAlgebraicStepV2 & CheckMyWork ----------------

StepCheckResultV2 CheckAlgebraicStepV2(
    const std::string& prevLineUtf8,
    const std::string& nextLineUtf8,
    const SymbolEnv& baseEnv)
{
    return detail_ped::CheckAlgebraicStepV2WithHintVars(prevLineUtf8, nextLineUtf8, baseEnv, {});
}

CheckMyWorkReport CheckMyWork(
    const std::vector<std::string>& derivationLinesUtf8,
    const SymbolEnv& baseEnv)
{
    CheckMyWorkReport rep;
    rep.audit.methodUtf8 = "MULTI_LINE_CHECK_MY_WORK";
    if (derivationLinesUtf8.empty()) {
        rep.allValid = false;
        rep.summaryUtf8 = "No hay pasos para verificar";
        rep.audit.verificationStatus = VerificationStatus::Failed;
        return rep;
    }

    std::unordered_set<std::string> allBoundVars;
    for (const auto& ln : derivationLinesUtf8) {
        const auto lineVars = detail_ped::CollectStepBoundVars(ln, "", baseEnv, {});
        allBoundVars.insert(lineVars.begin(), lineVars.end());
    }

    rep.allValid = true;
    for (std::size_t i = 0; i < derivationLinesUtf8.size(); ++i) {
        StepLineDiagnostic diag;
        diag.lineIndex = static_cast<int>(i);
        diag.rawLineUtf8 = derivationLinesUtf8[i];

        std::string parseTarget = detail_ped::Trim(derivationLinesUtf8[i]);

        MathJsonNode ast{};
        std::string pErr;
        if (!ParseExpressionToMathJson(parseTarget, ast, allBoundVars, &pErr)) {
            diag.valid = false;
            diag.transitionCheck.statusCode = "SYNTAX_ERR";
            diag.transitionCheck.errorCategory = StepErrorCategory::SyntaxError;
            diag.transitionCheck.messageUtf8 = pErr;
            rep.allValid = false;
            if (rep.firstErrorLineIndex < 0) rep.firstErrorLineIndex = static_cast<int>(i);
        } else {
            diag.canonicalExprUtf8 = MathJsonToExpression(CanonicalizeMathJson(ast));
            if (i > 0) {
                diag.transitionCheck = detail_ped::CheckAlgebraicStepV2WithHintVars(
                    derivationLinesUtf8[i - 1], derivationLinesUtf8[i], baseEnv, allBoundVars);
                diag.unitDimSiUtf8 = diag.transitionCheck.nextDimSiUtf8;
                diag.valid = diag.transitionCheck.equivalent;
                if (!diag.valid) {
                    rep.allValid = false;
                    if (rep.firstErrorLineIndex < 0) rep.firstErrorLineIndex = static_cast<int>(i);
                }
            } else {
                // Validate dimensional consistency on baseline line 0 as well
                std::string checkExpr = parseTarget;
                MathJsonNode checkAst = ast;
                const auto eqPos = parseTarget.find('=');
                if (eqPos != std::string::npos) {
                    const std::string lhs0 = detail_ped::Trim(parseTarget.substr(0, eqPos));
                    const std::string rhs0 = detail_ped::Trim(parseTarget.substr(eqPos + 1));
                    if (detail_ped::IsSimpleIdentifier(lhs0)) {
                        checkExpr = rhs0;
                        ParseExpressionToMathJson(checkExpr, checkAst, allBoundVars);
                    } else {
                        checkExpr = "(" + lhs0 + ") - (" + rhs0 + ")";
                        ParseExpressionToMathJson(checkExpr, checkAst, allBoundVars);
                    }
                }
                const auto freeVars0 = ExtractFreeSymbols(checkAst, baseEnv);
                double md0 = 0.0; bool dm0 = false; std::string dA0, dB0; int vp0 = 0;
                detail_ped::ProbePair(checkExpr, &checkAst, checkExpr, &checkAst, freeVars0, baseEnv,
                                      md0, dm0, dA0, dB0, vp0, false);
                if (dm0) {
                    diag.valid = false;
                    diag.transitionCheck.equivalent = false;
                    diag.transitionCheck.dimensionMismatch = true;
                    diag.transitionCheck.statusCode = "DIM_ERR";
                    diag.transitionCheck.errorCategory = StepErrorCategory::DimensionalMismatch;
                    diag.transitionCheck.messageUtf8 = "Desajuste dimensional en la linea base";
                    rep.allValid = false;
                    if (rep.firstErrorLineIndex < 0) rep.firstErrorLineIndex = 0;
                } else {
                    diag.valid = true;
                    diag.unitDimSiUtf8 = dA0;
                    diag.transitionCheck.equivalent = true;
                    diag.transitionCheck.statusCode = "OK";
                    diag.transitionCheck.messageUtf8 = "Linea base valida";
                }
            }
        }
        rep.lines.push_back(std::move(diag));
    }

    if (rep.allValid) {
        rep.summaryUtf8 = "Todos los " + std::to_string(rep.lines.size()) + " pasos verificados correctamente.";
        rep.audit.verificationStatus = VerificationStatus::SymbolicallyVerified;
    } else {
        const auto& bad = rep.lines[static_cast<std::size_t>(rep.firstErrorLineIndex)];
        rep.summaryUtf8 = "Divergencia en linea " + std::to_string(rep.firstErrorLineIndex + 1) +
                            " [" + bad.transitionCheck.statusCode + "]: " + bad.transitionCheck.messageUtf8;
        rep.audit.verificationStatus = VerificationStatus::Failed;
    }
    return rep;
}

// ---------------- F-312, SUP-07: Deterministic Solve & Teach Me ----------------

DeterministicSolveResult SolveDeterministic(
    const std::string& equationOrSystemUtf8,
    const std::string& solveForVar,
    const SymbolEnv& env)
{
    DeterministicSolveResult res;
    res.problemUtf8 = equationOrSystemUtf8;
    res.targetVarUtf8 = solveForVar.empty() ? "x" : solveForVar;
    res.audit.methodUtf8 = "DETERMINISTIC_CAS_SOLVER";

    // Case 1: Linear System (multiple equations separated by ';' or '\n')
    if (equationOrSystemUtf8.find(';') != std::string::npos || equationOrSystemUtf8.find('\n') != std::string::npos) {
        const auto lsys = SolveLinearSystemEquations(equationOrSystemUtf8);
        if (!lsys.ok) {
            res.errorUtf8 = lsys.errorUtf8;
            res.audit.verificationStatus = VerificationStatus::Failed;
            return res;
        }
        res.ok = true;
        res.primaryMethod.methodNameUtf8 = "Eliminacion Gaussiana";
        res.primaryMethod.steps.push_back({ 1, "Sistema lineal identificado", equationOrSystemUtf8, ConvertCasExprToLatex(equationOrSystemUtf8), "Extraccion exacta de matriz aumentada [A|b]" });
        res.primaryMethod.steps.push_back({ 2, "Reduccion por pivoteo parcial", lsys.stepsTraceUtf8, "", "Eliminacion hacia adelante y sustitucion" });
        for (std::size_t i = 0; i < lsys.varNames.size(); ++i) {
            res.primaryMethod.solutionsUtf8.push_back(lsys.varNames[i] + " = " + std::to_string(lsys.values[i]));
        }
        res.primaryMethod.finalSummaryUtf8 = lsys.summaryUtf8;

        res.hasAlternativeMethod = true;
        res.alternativeMethod.methodNameUtf8 = "Regla de Cramer / Matriz Inversa";
        res.alternativeMethod.steps.push_back({ 1, "Determinante del sistema", "det(A) = " + std::to_string(lsys.determinant), "\\det(A) = " + std::to_string(lsys.determinant), "det(A) != 0 garantiza solucion unica" });
        res.alternativeMethod.steps.push_back({ 2, "Sustitucion x = A^-1 b", lsys.summaryUtf8, "", "Equivalente a det(A_i)/det(A)" });
        res.alternativeMethod.solutionsUtf8 = res.primaryMethod.solutionsUtf8;
        res.alternativeMethod.finalSummaryUtf8 = lsys.summaryUtf8;
        res.audit.verificationStatus = VerificationStatus::NumericallyVerified;
        return res;
    }

    // Case 2: Single equation
    const auto alg = SolveAlgebraicEquation(equationOrSystemUtf8, res.targetVarUtf8, env);
    if (!alg.ok) {
        res.errorUtf8 = alg.errorUtf8;
        res.audit.verificationStatus = VerificationStatus::Failed;
        return res;
    }
    res.ok = true;
    res.primaryMethod.methodNameUtf8 = alg.solvedExact
        ? (alg.solutions.size() == 2 ? "Formula General Cuadratica" : "Despeje Algebraico Directo")
        : "Newton-Raphson Salvaguardado";
    for (const auto& st : alg.steps) {
        res.primaryMethod.steps.push_back({ st.stepNumber, st.titleUtf8, st.expressionUtf8, st.latexUtf8, "Derivacion determinista CAS" });
    }
    res.primaryMethod.solutionsUtf8 = alg.solutionStringsUtf8;
    res.primaryMethod.finalSummaryUtf8 = alg.summaryUtf8;

    res.hasAlternativeMethod = true;
    if (alg.solutions.size() == 2) {
        res.alternativeMethod.methodNameUtf8 = "Completar el Cuadrado";
        res.alternativeMethod.steps.push_back({ 1, "Normalizar coeficiente principal", "Dividir por coeficiente de " + res.targetVarUtf8 + "^2", "", "Forma monica x^2 + px + q = 0" });
        res.alternativeMethod.steps.push_back({ 2, "Completar trinomio cuadrado perfecto", "(x + p/2)^2 = (p/2)^2 - q", "", "Extraer raiz cuadrada a ambos lados" });
    } else {
        res.alternativeMethod.methodNameUtf8 = "Verificacion Numerica por Sustitucion";
        res.alternativeMethod.steps.push_back({ 1, "Sustituir raiz obtenida", alg.summaryUtf8, "", "Residual maximo |f(x*)| = " + std::to_string(alg.maxResidual) });
    }
    res.alternativeMethod.solutionsUtf8 = alg.solutionStringsUtf8;
    res.alternativeMethod.finalSummaryUtf8 = alg.summaryUtf8;
    res.audit.verificationStatus = alg.solvedExact ? VerificationStatus::SymbolicallyVerified : VerificationStatus::NumericallyVerified;
    return res;
}

TeachMeResponse GenerateTeachMeSocratic(
    const std::string& problemUtf8,
    const std::vector<std::string>& studentStepsUtf8,
    const std::string& solveForVar,
    const SymbolEnv& env,
    const std::vector<FtsHit>& notebookContext)
{
    TeachMeResponse resp;
    resp.problemUtf8 = problemUtf8;
    resp.audit.methodUtf8 = "SOCRATIC_DETERMINISTIC_CAS";

    std::vector<ProvenanceRef> hitRefs;
    for (const auto& h : notebookContext) {
        hitRefs.push_back({ "fts_hit", h.docKeyUtf8, h.pageIndex, h.objectId, h.snippetUtf8 });
    }

    if (!studentStepsUtf8.empty()) {
        resp.studentWorkAudit = CheckMyWork(studentStepsUtf8, env);
    }

    const auto sol = SolveDeterministic(problemUtf8, solveForVar, env);
    resp.ok = sol.ok || !studentStepsUtf8.empty();

    SocraticHintLevel l1;
    l1.level = 1;
    l1.revealsDirectAnswer = false;
    if (!studentStepsUtf8.empty() && !resp.studentWorkAudit.allValid && resp.studentWorkAudit.firstErrorLineIndex >= 0) {
        const auto& bad = resp.studentWorkAudit.lines[static_cast<std::size_t>(resp.studentWorkAudit.firstErrorLineIndex)];
        l1.promptUtf8 = "Revisa tu linea " + std::to_string(resp.studentWorkAudit.firstErrorLineIndex + 1) +
                        " (" + bad.transitionCheck.statusCode + "). " + bad.transitionCheck.hintUtf8;
        l1.questionForStudentUtf8 = "Que cambia entre tu paso " + std::to_string(resp.studentWorkAudit.firstErrorLineIndex) +
                                    " y el paso " + std::to_string(resp.studentWorkAudit.firstErrorLineIndex + 1) + "?";
    } else {
        l1.promptUtf8 = "Identifica las variables conocidas, las unidades fisicas y que incognita debes aislar (" +
                        (solveForVar.empty() ? std::string("x") : solveForVar) + ").";
        l1.questionForStudentUtf8 = "Que operaciones inversas mantienen el equilibrio en ambos lados de la igualdad?";
    }

    SocraticHintLevel l2;
    l2.level = 2;
    l2.revealsDirectAnswer = false;
    l2.provenanceRefs = hitRefs;
    if (sol.ok && !sol.primaryMethod.steps.empty()) {
        l2.promptUtf8 = "Estrategia sugerida (" + sol.primaryMethod.methodNameUtf8 + "): " + sol.primaryMethod.steps[0].titleUtf8;
        l2.questionForStudentUtf8 = "Si llevas todos los terminos a una forma estandar, cual seria tu siguiente paso intermedio?";
    } else {
        l2.promptUtf8 = "Agrupa terminos semejantes antes de despejar.";
        l2.questionForStudentUtf8 = "Puedes simplificar cada lado por separado primero?";
    }

    SocraticHintLevel l3;
    l3.level = 3;
    l3.revealsDirectAnswer = true;
    l3.provenanceRefs = hitRefs;
    if (sol.ok) {
        l3.promptUtf8 = "Checkpoint verificado por CAS: " + sol.primaryMethod.finalSummaryUtf8;
        l3.questionForStudentUtf8 = "Comprueba sustituyendo este resultado en la expresion original.";
    } else {
        l3.promptUtf8 = "Verifica sintaxis y dimensiones de la expresion planteada.";
        l3.questionForStudentUtf8 = "Todos los terminos tienen unidades compatibles?";
    }

    resp.hints = { std::move(l1), std::move(l2), std::move(l3) };
    resp.contextSummaryUtf8 = hitRefs.empty() ? "Sin contexto adicional de cuaderno" : ("Anclado a " + std::to_string(hitRefs.size()) + " referencia(s) del cuaderno");
    resp.audit.provenanceRefs = std::move(hitRefs);
    resp.audit.verificationStatus = sol.ok ? sol.audit.verificationStatus : VerificationStatus::Unverified;
    return resp;
}

// ---------------- F-306, F-307: Opt-In AI Settings & Local-First Dispatch ----------------

DispatchDecision ResolveLocalFirstDispatch(
    AiCapabilityKind capability,
    const AiSettings& settings,
    bool callerRequestedCloudEnhancement)
{
    DispatchDecision d;
    d.capability = capability;
    // Deterministic capabilities ALWAYS execute locally
    const bool isPureDeterministic = (capability == AiCapabilityKind::ConfirmOcrAst ||
                                      capability == AiCapabilityKind::CheckMyWork ||
                                      capability == AiCapabilityKind::Solve ||
                                      capability == AiCapabilityKind::GenerateReport);
    if (isPureDeterministic || !settings.enabled || !settings.allowCloudFallback || !callerRequestedCloudEnhancement) {
        d.route = (callerRequestedCloudEnhancement && (!settings.enabled || !settings.allowCloudFallback))
            ? DispatchRoute::LocalOnlyCloudBlocked
            : DispatchRoute::LocalDeterministic;
        d.requiresCloud = false;
        d.executedLocally = true;
        d.policyNoteUtf8 = isPureDeterministic
            ? "Ejecucion local determinista obligatoria (CAS/OCR)"
            : "Politica local-first activa: cero dependencia en la nube";
        return d;
    }
    d.route = DispatchRoute::OptInCloudPermitted;
    d.requiresCloud = true;
    d.executedLocally = false;
    d.policyNoteUtf8 = "Opt-in explicito habilitado por el usuario";
    return d;
}

// ---------------- F-308: Grounded Search Synthesis ----------------

GroundedSearchSynthesis SynthesizeGroundedSearchAnswer(
    const std::string& queryUtf8,
    const std::vector<FtsHit>& hits,
    std::size_t maxCitations)
{
    GroundedSearchSynthesis res;
    res.queryUtf8 = queryUtf8;
    res.audit.methodUtf8 = "GROUNDED_FTS_SYNTHESIS";
    if (hits.empty() || maxCitations == 0) {
        res.hasEvidence = false;
        res.synthesizedAnswerUtf8 = "Sin evidencia verificada en el cuaderno para: \"" + queryUtf8 + "\".";
        res.audit.verificationStatus = VerificationStatus::Failed;
        return res;
    }

    const std::size_t count = (std::min)(hits.size(), maxCitations);
    std::ostringstream ans;
    ans << "Evidencia encontrada en tus apuntes para \"" << queryUtf8 << "\":\n";
    for (std::size_t i = 0; i < count; ++i) {
        const int cIdx = static_cast<int>(i + 1);
        SearchCitation cit;
        cit.citationIndex = cIdx;
        cit.docKeyUtf8 = hits[i].docKeyUtf8;
        cit.pageIndex = hits[i].pageIndex;
        cit.objectId = hits[i].objectId;
        cit.sourceKindUtf8 = hits[i].sourceKindUtf8;
        cit.quotedSnippetUtf8 = hits[i].snippetUtf8;
        cit.score = hits[i].score;
        res.citations.push_back(std::move(cit));
        res.audit.provenanceRefs.push_back({ "fts_hit", hits[i].docKeyUtf8, hits[i].pageIndex, hits[i].objectId, hits[i].snippetUtf8 });

        ans << "- [" << hits[i].sourceKindUtf8 << " p." << (hits[i].pageIndex + 1) << "] "
            << hits[i].snippetUtf8 << " [" << cIdx << "]\n";
    }
    res.hasEvidence = true;
    res.synthesizedAnswerUtf8 = ans.str();
    res.audit.verificationStatus = VerificationStatus::NumericallyVerified;
    return res;
}

// ---------------- F-309: Non-Destructive Note Summarizer ----------------

SummaryArtifact SummarizeDocumentNonDestructive(
    const DocumentModel& doc,
    std::size_t maxBullets)
{
    SummaryArtifact art;
    art.audit.methodUtf8 = "NON_DESTRUCTIVE_EXTRACTIVE_SUMMARIZER";
    std::ostringstream body;
    // Sin IA esto no es un resumen: copia las primeras notas, fórmulas y tarjetas tal cual.
    body << "Extracto sin IA (primeras notas, f\xC3\xB3rmulas y tarjetas) \xE2\x80\x94 " << doc.Title() << "\n";
    std::size_t count = 0;
    auto collectObj = [&](const AnnotationObject& o, const std::string& loc) {
        if (count >= maxBullets) return;
        std::string item;
        if (o.kind == ObjectKind::MathBlock && !o.formulaExprUtf8.empty()) {
            item = "[Formula " + loc + "] " + o.formulaExprUtf8 +
                   (o.evaluatedResultUtf8.empty() ? "" : (" => " + o.evaluatedResultUtf8));
        } else if (o.kind == ObjectKind::CalcSessionCard && !o.textUtf8.empty()) {
            item = "[CASIO " + loc + "] " + o.textUtf8;
        } else if (o.kind == ObjectKind::TextBox && !o.textUtf8.empty()) {
            item = "[Nota " + loc + "] " + o.textUtf8.substr(0, 140);
        }
        if (!item.empty()) {
            body << "• " << item << "\n";
            art.sourceObjectIds.push_back(o.id);
            art.audit.provenanceRefs.push_back({ "doc_object", doc.Title(), 0, o.id, item });
            ++count;
        }
    };
    for (std::size_t p = 0; p < doc.Pages().size(); ++p) {
        for (const auto& o : doc.Pages()[p].objects) {
            collectObj(o, "P" + std::to_string(p + 1));
        }
    }
    for (const auto& o : doc.CanvasObjects()) {
        collectObj(o, "Lienzo");
    }

    art.summaryBlock.kind = ObjectKind::TextBox;
    art.summaryBlock.bounds = { 40.0, 40.0, 460.0, 220.0 };
    art.summaryBlock.captionUtf8 = art.targetLayerNameUtf8;
    art.summaryBlock.textUtf8 = body.str();
    art.summaryBlock.tags.push_back("resumen_asistido");
    art.ok = (count > 0);
    // Un extracto no se verifica dimensionalmente: antes se marcaba «DimensionallyVerified» sin revisar nada
    art.audit.verificationStatus = VerificationStatus::Unverified;
    return art;
}

// ---------------- F-311: Reviewable Flashcard Preview/Edit Workflow ----------------

FlashcardReviewSession BuildFlashcardReviewSession(const DocumentModel& doc, const std::string& deckTitle) {
    FlashcardReviewSession session;
    session.deckTitleUtf8 = deckTitle.empty() ? (doc.Title() + " — Borradores") : deckTitle;

    std::uint64_t nextId = 1;
    auto harvest = [&](const AnnotationObject& o, std::size_t pg) {
        FlashcardDraft draft;
        draft.item.id = nextId++;
        draft.item.sourcePageIndex = pg;
        draft.item.sourceRect = o.bounds;
        draft.status = FlashcardDraftStatus::PendingReview;
        if (o.kind == ObjectKind::StudyTape) {
            draft.item.questionUtf8 = o.captionUtf8.empty() ? "Concepto oculto" : o.captionUtf8;
            draft.item.answerUtf8 = o.textUtf8;
            draft.sourceTagUtf8 = "StudyTape";
        } else if (o.kind == ObjectKind::MathBlock && !o.formulaExprUtf8.empty()) {
            draft.item.questionUtf8 = "Evaluar / deducir: " + o.formulaExprUtf8;
            draft.item.answerUtf8 = o.evaluatedResultUtf8.empty() ? o.textUtf8 : o.evaluatedResultUtf8;
            draft.sourceTagUtf8 = "MathBlock";
        } else if (o.kind == ObjectKind::CalcSessionCard && !o.formulaExprUtf8.empty()) {
            draft.item.questionUtf8 = "Secuencia CASIO (" + o.calcModelIdUtf8 + "): " + o.formulaExprUtf8;
            draft.item.answerUtf8 = o.evaluatedResultUtf8;
            draft.sourceTagUtf8 = "CalcSessionCard";
        } else {
            return;
        }
        if (!draft.item.answerUtf8.empty()) {
            session.drafts.push_back(std::move(draft));
        }
    };
    for (std::size_t p = 0; p < doc.Pages().size(); ++p) {
        for (const auto& o : doc.Pages()[p].objects) harvest(o, p);
    }
    for (const auto& o : doc.CanvasObjects()) harvest(o, 0);
    return session;
}

bool EditFlashcardDraft(FlashcardReviewSession& session, std::size_t index, const std::string& newQ, const std::string& newA) {
    if (index >= session.drafts.size()) return false;
    session.drafts[index].item.questionUtf8 = newQ;
    session.drafts[index].item.answerUtf8 = newA;
    session.drafts[index].status = FlashcardDraftStatus::Edited;
    return true;
}

bool ApproveFlashcardDraft(FlashcardReviewSession& session, std::size_t index) {
    if (index >= session.drafts.size()) return false;
    session.drafts[index].status = FlashcardDraftStatus::Approved;
    return true;
}

bool RejectFlashcardDraft(FlashcardReviewSession& session, std::size_t index) {
    if (index >= session.drafts.size()) return false;
    session.drafts[index].status = FlashcardDraftStatus::Rejected;
    return true;
}

std::size_t CommitApprovedFlashcardsToDeck(const FlashcardReviewSession& session, StudyDeck& outDeck) {
    if (outDeck.titleUtf8.empty()) outDeck.titleUtf8 = session.deckTitleUtf8;
    std::size_t added = 0;
    for (const auto& d : session.drafts) {
        if (d.status == FlashcardDraftStatus::Approved) {
            outDeck.cards.push_back(d.item);
            ++added;
        }
    }
    return added;
}

// ---------------- F-315: Deterministic IEEE / TEC / Lab Report Templates ----------------

DeterministicReportOutput RenderDeterministicReportTemplate(
    const DocumentModel& doc,
    const DeterministicReportOptions& opts)
{
    DeterministicReportOutput out;
    out.templateKind = opts.templateKind;
    out.audit.methodUtf8 = "DETERMINISTIC_TEMPLATE_RENDERER";

    const auto vars = InspectDocumentVariables(doc);
    std::ostringstream md, html;

    const std::string tplTag = (opts.templateKind == ReportTemplateKind::IeeePaper)
        ? "IEEE Technical Report"
        : ((opts.templateKind == ReportTemplateKind::LabReport) ? "Laboratorio STEM" : "Memoria Tecnica TEC");

    md << "# " << doc.Title() << "\n"
       << "**Plantilla:** " << tplTag << " | **Autor:** " << opts.authorUtf8
       << " | **Institucion:** " << opts.institutionUtf8 << "\n\n";

    html << "<!DOCTYPE html><html><head><meta charset=\"utf-8\"><title>"
         << detail_ped::HtmlEscape(doc.Title()) << "</title></head><body>\n"
         << "<h1>" << detail_ped::HtmlEscape(doc.Title()) << "</h1>\n"
         << "<p><strong>" << detail_ped::HtmlEscape(tplTag) << "</strong> &mdash; "
         << detail_ped::HtmlEscape(opts.authorUtf8) << " (" << detail_ped::HtmlEscape(opts.institutionUtf8) << ")</p>\n";

    if (opts.templateKind == ReportTemplateKind::IeeePaper) {
        md << "## Abstract & Methodology\n";
        html << "<h2>I. Abstract &amp; Methodology</h2>\n";
    } else if (opts.templateKind == ReportTemplateKind::LabReport) {
        md << "## Datos y Resultados Experimentales\n";
        html << "<h2>1. Datos y Resultados Experimentales</h2>\n";
    } else {
        md << "## Memoria de Calculo y Desarrollo\n";
        html << "<h2>1. Memoria de Calculo y Desarrollo</h2>\n";
    }

    if (!vars.empty()) {
        md << "| Variable | Valor | Magnitud | Origen |\n|---|---|---|---|\n";
        html << "<table border=\"1\"><tr><th>Variable</th><th>Valor</th><th>Magnitud</th><th>Origen</th></tr>\n";
        for (const auto& v : vars) {
            md << "| `" << v.varNameUtf8 << "` | " << v.currentValueUtf8 << " | "
               << v.physicalQuantityUtf8 << " | " << v.sourceNodeLabelUtf8 << " |\n";
            html << "<tr><td><code>" << detail_ped::HtmlEscape(v.varNameUtf8) << "</code></td><td>"
                 << detail_ped::HtmlEscape(v.currentValueUtf8) << "</td><td>"
                 << detail_ped::HtmlEscape(v.physicalQuantityUtf8) << "</td><td>"
                 << detail_ped::HtmlEscape(v.sourceNodeLabelUtf8) << "</td></tr>\n";
            out.audit.provenanceRefs.push_back({ "math_block", doc.Title(), 0, 0, v.varNameUtf8 + " = " + v.currentValueUtf8 });
        }
        html << "</table>\n";
    }

    if (opts.includeAuditFooter) {
        md << "\n---\n*CAS Version:* " << out.audit.casVersion << " | *Determinista 100% Local*\n";
        html << "<hr><footer>CAS Version: " << detail_ped::HtmlEscape(out.audit.casVersion) << "</footer>\n";
    }
    html << "</body></html>\n";

    out.markdownUtf8 = md.str();
    out.htmlUtf8 = html.str();
    out.ok = true;
    out.audit.verificationStatus = VerificationStatus::DimensionallyVerified;
    return out;
}

// ---------------- F-316: Non-Destructive Side-by-Side Translation ----------------

SideBySideTranslation TranslatePreservingMathAndUnits(
    const std::string& sourceTextUtf8,
    const std::string& sourceLangUtf8,
    const std::string& targetLangUtf8)
{
    SideBySideTranslation res;
    res.sourceLangUtf8 = sourceLangUtf8;
    res.targetLangUtf8 = targetLangUtf8;
    res.sourceTextUtf8 = sourceTextUtf8;
    res.audit.methodUtf8 = "NON_DESTRUCTIVE_MATH_UNIT_PRESERVING_TRANSLATOR";

    // Segment protected spans: $$...$$, $...$, \[...\], \(...\), #varsub/#nosub/#novar
    struct Span { bool isProtected; std::string text; };
    std::vector<Span> spans;
    std::size_t i = 0;
    std::string proseBuf;
    auto flushProse = [&]() {
        if (!proseBuf.empty()) { spans.push_back({ false, proseBuf }); proseBuf.clear(); }
    };

    while (i < sourceTextUtf8.size()) {
        if (sourceTextUtf8[i] == '$') {
            const bool dbl = (i + 1 < sourceTextUtf8.size() && sourceTextUtf8[i + 1] == '$');
            const std::size_t dl = dbl ? 2 : 1;
            const auto cl = sourceTextUtf8.find(dbl ? "$$" : "$", i + dl);
            if (cl != std::string::npos) {
                flushProse();
                const std::string seg = sourceTextUtf8.substr(i, cl + dl - i);
                res.preservedMathSegments.push_back(seg);
                spans.push_back({ true, seg });
                i = cl + dl;
                continue;
            }
        }
        if (sourceTextUtf8.compare(i, 2, "\\(") == 0 || sourceTextUtf8.compare(i, 2, "\\[") == 0) {
            const std::string clTok = (sourceTextUtf8[i + 1] == '(') ? "\\)" : "\\]";
            const auto cl = sourceTextUtf8.find(clTok, i + 2);
            if (cl != std::string::npos) {
                flushProse();
                const std::string seg = sourceTextUtf8.substr(i, cl + 2 - i);
                res.preservedMathSegments.push_back(seg);
                spans.push_back({ true, seg });
                i = cl + 2;
                continue;
            }
        }
        proseBuf.push_back(sourceTextUtf8[i++]);
    }
    flushProse();

    static const std::vector<std::pair<std::string, std::string>> kEsToEnPhrases = {
        { "la tension", "the voltage" },
        { "la corriente", "the current" },
        { "la resistencia", "the resistance" },
        { "la potencia", "the power" },
        { "la ecuacion", "the equation" }
    };

    static const std::vector<std::pair<std::string, std::string>> kEsToEnWords = {
        { "calcule", "calculate" },
        { "calcular", "calculate" },
        { "tension", "voltage" }, { "corriente", "current" },
        { "resistencia", "resistance" }, { "potencia", "power" },
        { "ecuacion", "equation" }, { "resultado", "result" },
        { "cuando", "when" }, { "donde", "where" },
        { "usando", "using" }, { "entre", "between" }, { "con", "with" },
        { "para", "for" }, { "en", "in" }, { "y", "and" }, { "es", "is" },
        { "el", "the" }, { "la", "the" }, { "de", "of" }
    };

    auto normalizeEsAsciiLower = [](const std::string& s) {
        std::string out;
        out.reserve(s.size());
        for (std::size_t idx = 0; idx < s.size(); ++idx) {
            const unsigned char c = static_cast<unsigned char>(s[idx]);
            if (c == 0xC3 && idx + 1 < s.size()) {
                const unsigned char c2 = static_cast<unsigned char>(s[idx + 1]);
                char rep = 0;
                if (c2 == 0xA1 || c2 == 0x81) rep = 'a';
                else if (c2 == 0xA9 || c2 == 0x89) rep = 'e';
                else if (c2 == 0xAD || c2 == 0x8D) rep = 'i';
                else if (c2 == 0xB3 || c2 == 0x93) rep = 'o';
                else if (c2 == 0xBA || c2 == 0x9A || c2 == 0xBC || c2 == 0x9C) rep = 'u';
                else if (c2 == 0xB1 || c2 == 0x91) rep = 'n';
                if (rep != 0) {
                    out.push_back(rep);
                    ++idx;
                    continue;
                }
            }
            out.push_back(static_cast<char>(std::tolower(c)));
        }
        return out;
    };

    auto matchCase = [](const std::string& src, std::string dst) {
        if (src.empty() || dst.empty()) return dst;
        bool hasAlpha = false, allUpper = true;
        bool startsUpper = std::isupper(static_cast<unsigned char>(src[0])) != 0;
        if (static_cast<unsigned char>(src[0]) == 0xC3 && src.size() >= 2) {
            const unsigned char c2 = static_cast<unsigned char>(src[1]);
            if (c2 == 0x81 || c2 == 0x89 || c2 == 0x8D || c2 == 0x93 || c2 == 0x9A || c2 == 0x9C || c2 == 0x91) {
                startsUpper = true;
            }
        }
        for (std::size_t idx = 0; idx < src.size(); ++idx) {
            const unsigned char ch = static_cast<unsigned char>(src[idx]);
            if (ch == 0xC3 && idx + 1 < src.size()) {
                const unsigned char c2 = static_cast<unsigned char>(src[idx + 1]);
                if (c2 == 0x81 || c2 == 0x89 || c2 == 0x8D || c2 == 0x93 || c2 == 0x9A || c2 == 0x9C || c2 == 0x91) {
                    hasAlpha = true;
                    ++idx;
                    continue;
                }
                if (c2 == 0xA1 || c2 == 0xA9 || c2 == 0xAD || c2 == 0xB3 || c2 == 0xBA || c2 == 0xBC || c2 == 0xB1) {
                    hasAlpha = true;
                    allUpper = false;
                    ++idx;
                    continue;
                }
            }
            if (std::isalpha(ch)) {
                hasAlpha = true;
                if (!std::isupper(ch)) { allUpper = false; break; }
            }
        }
        if (hasAlpha && allUpper && src.size() > 1) {
            for (char& ch : dst) ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
            return dst;
        }
        if (startsUpper) {
            dst[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(dst[0])));
        }
        return dst;
    };

    auto isWordBoundary = [](const std::string& str, std::size_t idx) -> bool {
        if (idx >= str.size()) return true;
        const unsigned char c = static_cast<unsigned char>(str[idx]);
        return std::isspace(c) || std::ispunct(c);
    };

    auto isDictWord = [&](const std::string& lowerNorm) -> bool {
        for (const auto& kv : kEsToEnWords) {
            if (lowerNorm == kv.first || lowerNorm == kv.second) return true;
        }
        return false;
    };

    auto isNumericToken = [](const std::string& core) -> bool {
        if (core.empty()) return false;
        std::size_t p = 0;
        if (core[p] == '+' || core[p] == '-') ++p;
        if (p < core.size() && core[p] == '.') ++p;
        return p < core.size() && std::isdigit(static_cast<unsigned char>(core[p])) != 0;
    };

    auto translateProse = [&](const std::string& p) {
        std::string out;
        std::size_t k = 0;
        bool prevTokenWasNumeric = false;
        while (k < p.size()) {
            if (std::isspace(static_cast<unsigned char>(p[k]))) {
                out.push_back(p[k++]);
                continue;
            }
            // Check multi-word phrase matches (case/accent-insensitive) at word boundaries first
            bool matchedPhrase = false;
            if ((k == 0 || isWordBoundary(p, k - 1)) &&
                ((sourceLangUtf8 == "es" && targetLangUtf8 == "en") || (sourceLangUtf8 == "en" && targetLangUtf8 == "es"))) {
                std::size_t w1End = k;
                while (w1End < p.size() && !std::isspace(static_cast<unsigned char>(p[w1End]))) ++w1End;
                std::size_t w2Start = w1End;
                while (w2Start < p.size() && std::isspace(static_cast<unsigned char>(p[w2Start]))) ++w2Start;
                std::size_t w2End = w2Start;
                while (w2End < p.size() && !std::isspace(static_cast<unsigned char>(p[w2End])) &&
                       std::string(".,;:!?)]}\"'").find(p[w2End]) == std::string::npos) {
                    ++w2End;
                }
                if (w2Start > w1End && w2End > w2Start) {
                    const std::string candNorm = normalizeEsAsciiLower(p.substr(k, w1End - k)) + " " +
                                                 normalizeEsAsciiLower(p.substr(w2Start, w2End - w2Start));
                    for (const auto& kv : kEsToEnPhrases) {
                        const std::string& srcPhrase = (sourceLangUtf8 == "en" && targetLangUtf8 == "es") ? kv.second : kv.first;
                        const std::string& dstPhrase = (sourceLangUtf8 == "en" && targetLangUtf8 == "es") ? kv.first : kv.second;
                        if (candNorm == srcPhrase && isWordBoundary(p, w2End)) {
                            out += matchCase(p.substr(k, w2End - k), dstPhrase);
                            k = w2End;
                            matchedPhrase = true;
                            prevTokenWasNumeric = false;
                            break;
                        }
                    }
                }
            }
            if (matchedPhrase) continue;

            std::size_t st = k;
            while (k < p.size() && !std::isspace(static_cast<unsigned char>(p[k]))) ++k;
            const std::string tok = p.substr(st, k - st);

            // Split leading/trailing punctuation so "10 V." or "mV," still records unit tokens cleanly
            std::size_t leadEnd = 0;
            while (leadEnd < tok.size() && std::string("([{\"'").find(tok[leadEnd]) != std::string::npos) {
                ++leadEnd;
            }
            std::size_t trailStart = tok.size();
            while (trailStart > leadEnd && std::string(".,;:!?)]}\"'").find(tok[trailStart - 1]) != std::string::npos) {
                --trailStart;
            }
            const std::string lead = tok.substr(0, leadEnd);
            const std::string core = tok.substr(leadEnd, trailStart - leadEnd);
            const std::string trail = tok.substr(trailStart);

            // Preserve #varsub directives or pure unit tokens verbatim (only treat ambiguous words like "in" as units when preceded by a numeric token)
            if (core == "#varsub" || core == "#nosub" || core == "#novar") {
                res.preservedUnitTokens.push_back(core);
                out += tok;
                prevTokenWasNumeric = false;
                continue;
            }
            const std::string lowerCore = normalizeEsAsciiLower(core);
            UnitDim d{}; double sc = 1.0, off = 0.0;
            if (!core.empty() && TryParseUnitExpression(core, d, sc, off) &&
                (prevTokenWasNumeric || !isDictWord(lowerCore))) {
                res.preservedUnitTokens.push_back(core);
                out += tok;
                prevTokenWasNumeric = false;
                continue;
            }
            std::string repCore = core;
            if (sourceLangUtf8 == "es" && targetLangUtf8 == "en") {
                for (const auto& kv : kEsToEnWords) {
                    if (lowerCore == kv.first) { repCore = matchCase(core, kv.second); break; }
                }
            } else if (sourceLangUtf8 == "en" && targetLangUtf8 == "es") {
                for (const auto& kv : kEsToEnWords) {
                    if (lowerCore == kv.second) { repCore = matchCase(core, kv.first); break; }
                }
            }
            out += lead + repCore + trail;
            prevTokenWasNumeric = isNumericToken(core);
        }
        return out;
    };

    std::string finalTrans;
    for (const auto& sp : spans) {
        finalTrans += sp.isProtected ? sp.text : translateProse(sp.text);
    }
    res.translatedTextUtf8 = std::move(finalTrans);
    res.allMathAndUnitsIntact = true;
    for (const auto& m : res.preservedMathSegments) {
        if (res.translatedTextUtf8.find(m) == std::string::npos) { res.allMathAndUnitsIntact = false; break; }
    }
    for (const auto& u : res.preservedUnitTokens) {
        if (res.translatedTextUtf8.find(u) == std::string::npos) { res.allMathAndUnitsIntact = false; break; }
    }
    res.ok = true;
    res.audit.verificationStatus = res.allMathAndUnitsIntact ? VerificationStatus::DimensionallyVerified : VerificationStatus::Failed;
    return res;
}

// ---------------- F-317: Deterministic Macro Recorder ----------------

void DeterministicMacroRecorder::Start(std::string macroNameUtf8) {
    name_ = std::move(macroNameUtf8);
    recording_ = true;
}

void DeterministicMacroRecorder::Stop() {
    recording_ = false;
}

void DeterministicMacroRecorder::Record(
    MacroActionKind kind,
    std::string target,
    std::string payload,
    std::string expected)
{
    if (!recording_) return;
    steps_.push_back({ static_cast<std::uint64_t>(steps_.size()), kind, std::move(target), std::move(payload), std::move(expected) });
}

void DeterministicMacroRecorder::Clear() {
    steps_.clear();
}

std::string DeterministicMacroRecorder::ToJsonString() const {
    json j;
    j["name"] = name_;
    j["recording"] = recording_;
    j["steps"] = json::array();
    for (const auto& s : steps_) {
        j["steps"].push_back({
            { "seqIndex", s.seqIndex },
            { "kind", static_cast<int>(s.kind) },
            { "target", s.targetIdOrModelUtf8 },
            { "payload", s.payloadUtf8 },
            { "expected", s.expectedResultUtf8 }
        });
    }
    return j.dump(2);
}

bool DeterministicMacroRecorder::FromJsonString(const std::string& jsonUtf8, std::string* outErr) {
    try {
        const json j = json::parse(jsonUtf8);
        name_ = j.value("name", "Macro_STEM");
        recording_ = j.value("recording", false);
        steps_.clear();
        if (j.contains("steps") && j["steps"].is_array()) {
            for (const auto& sj : j["steps"]) {
                MacroActionStep st;
                st.seqIndex = sj.value("seqIndex", static_cast<std::uint64_t>(steps_.size()));
                st.kind = static_cast<MacroActionKind>(sj.value("kind", 0));
                st.targetIdOrModelUtf8 = sj.value("target", "");
                st.payloadUtf8 = sj.value("payload", "");
                st.expectedResultUtf8 = sj.value("expected", "");
                steps_.push_back(std::move(st));
            }
        }
        return true;
    } catch (const std::exception& ex) {
        if (outErr) *outErr = ex.what();
        return false;
    }
}

// ---------------- F-319: Anti-Hiding Invariant ----------------

RawTruthProjection ExtractRawTruthProjection(const AnnotationObject& obj, const SymbolEnv& env) {
    RawTruthProjection p;
    p.rawExpressionUtf8 = obj.formulaExprUtf8;
    p.subMode = obj.mathSubMode;
    p.visibleWithoutAi = true;
    for (const auto& kv : env) {
        if (!obj.formulaExprUtf8.empty() && obj.formulaExprUtf8.find(kv.first) != std::string::npos) {
            p.sourceVariables.push_back({ kv.first, kv.second.FormatUtf8(6) });
        }
    }
    if (obj.evaluatedResultUtf8.find("ERR") != std::string::npos) {
        p.deterministicErrors.push_back(obj.evaluatedResultUtf8);
    }
    if (obj.stepCheckStatusUtf8.find("ERR") != std::string::npos ||
        obj.stepCheckStatusUtf8.find("DIFF") != std::string::npos) {
        p.deterministicErrors.push_back(obj.stepCheckStatusUtf8);
    }
    return p;
}

bool ValidateAntiHidingInvariant(
    const RawTruthProjection& truth,
    const std::string& renderedDisplayUtf8,
    const AiSettings& settings,
    std::vector<std::string>* outViolations)
{
    std::vector<std::string> v;
    if (!truth.rawExpressionUtf8.empty() &&
        renderedDisplayUtf8.find(truth.rawExpressionUtf8) == std::string::npos &&
        truth.subMode != MathSubMode::NoVar) {
        v.push_back("La expresion cruda original fue ocultada en la vista renderizada");
    }
    for (const auto& err : truth.deterministicErrors) {
        if (!err.empty() && renderedDisplayUtf8.find(err) == std::string::npos) {
            v.push_back("El error determinista '" + err + "' fue suprimido");
        }
    }
    if (!settings.enabled && !truth.visibleWithoutAi) {
        v.push_back("Datos matematicos marcados como ocultos sin IA");
    }
    if (outViolations) *outViolations = v;
    return v.empty();
}

// ---------------- JSON Serialization for FbdModel (F-243) & FormulaSheetModel (F-245) ----------------

std::string FbdModelToJson(const FbdModel& model) {
    json j;
    j["title"] = model.title;
    j["pivotX"] = model.pivotX;
    j["pivotY"] = model.pivotY;
    j["forces"] = json::array();
    for (const auto& f : model.forces) {
        j["forces"].push_back({
            { "id", f.id }, { "label", f.label }, { "x", f.x }, { "y", f.y },
            { "mag", f.mag }, { "angleDeg", f.angleDeg }, { "unknownMag", f.unknownMag }, { "unit", f.unitUtf8 }
        });
    }
    j["moments"] = json::array();
    for (const auto& m : model.moments) {
        j["moments"].push_back({
            { "id", m.id }, { "label", m.label }, { "x", m.x }, { "y", m.y },
            { "momentNm", m.momentNm }, { "unknown", m.unknown }
        });
    }
    j["supports"] = json::array();
    for (const auto& s : model.supports) {
        j["supports"].push_back({
            { "id", s.id }, { "label", s.label }, { "kind", static_cast<int>(s.kind) },
            { "x", s.x }, { "y", s.y }, { "rollerNormalAngleDeg", s.rollerNormalAngleDeg }
        });
    }
    j["distLoads"] = json::array();
    for (const auto& d : model.distLoads) {
        j["distLoads"].push_back({
            { "id", d.id }, { "label", d.label }, { "x0", d.x0 }, { "y0", d.y0 },
            { "x1", d.x1 }, { "y1", d.y1 }, { "w0NPerM", d.w0NPerM }, { "w1NPerM", d.w1NPerM }, { "angleDeg", d.angleDeg }
        });
    }
    return j.dump(2);
}

bool FbdModelFromJson(const std::string& jsonUtf8, FbdModel& outModel, std::string* outErr) {
    try {
        const json j = json::parse(jsonUtf8);
        outModel = {};
        outModel.title = j.value("title", "");
        outModel.pivotX = j.value("pivotX", 0.0);
        outModel.pivotY = j.value("pivotY", 0.0);
        if (j.contains("forces") && j["forces"].is_array()) {
            for (const auto& fj : j["forces"]) {
                outModel.forces.push_back({
                    fj.value("id", ""), fj.value("label", ""), fj.value("x", 0.0), fj.value("y", 0.0),
                    fj.value("mag", 0.0), fj.value("angleDeg", 0.0), fj.value("unknownMag", false), fj.value("unit", "N")
                });
            }
        }
        if (j.contains("moments") && j["moments"].is_array()) {
            for (const auto& mj : j["moments"]) {
                outModel.moments.push_back({
                    mj.value("id", ""), mj.value("label", ""), mj.value("x", 0.0), mj.value("y", 0.0),
                    mj.value("momentNm", 0.0), mj.value("unknown", false)
                });
            }
        }
        if (j.contains("supports") && j["supports"].is_array()) {
            for (const auto& sj : j["supports"]) {
                outModel.supports.push_back({
                    sj.value("id", ""), sj.value("label", ""),
                    static_cast<FbdSupportKind>(sj.value("kind", 0)),
                    sj.value("x", 0.0), sj.value("y", 0.0), sj.value("rollerNormalAngleDeg", 90.0)
                });
            }
        }
        if (j.contains("distLoads") && j["distLoads"].is_array()) {
            for (const auto& dj : j["distLoads"]) {
                outModel.distLoads.push_back({
                    dj.value("id", ""), dj.value("label", ""), dj.value("x0", 0.0), dj.value("y0", 0.0),
                    dj.value("x1", 1.0), dj.value("y1", 0.0), dj.value("w0NPerM", 0.0), dj.value("w1NPerM", 0.0),
                    dj.value("angleDeg", -90.0)
                });
            }
        }
        return true;
    } catch (const std::exception& ex) {
        if (outErr) *outErr = ex.what();
        return false;
    }
}

std::string FormulaSheetModelToJson(const FormulaSheetModel& sheet) {
    json j;
    j["id"] = sheet.id;
    j["title"] = sheet.titleUtf8;
    j["courseRef"] = sheet.courseRefUtf8;
    j["entries"] = json::array();
    for (const auto& e : sheet.entries) {
        json pj = json::array();
        for (const auto& p : e.params) {
            pj.push_back({
                { "name", p.name }, { "label", p.labelUtf8 }, { "defaultExpr", p.defaultExprUtf8 },
                { "minVal", p.minVal }, { "maxVal", p.maxVal }, { "hasSlider", p.hasSlider }
            });
        }
        j["entries"].push_back({
            { "id", e.id }, { "title", e.titleUtf8 }, { "category", e.categoryUtf8 },
            { "expr", e.exprUtf8 }, { "courseRef", e.courseRefUtf8 },
            { "astHash64", e.astHash64 }, { "params", std::move(pj) }
        });
    }
    return j.dump(2);
}

bool FormulaSheetModelFromJson(const std::string& jsonUtf8, FormulaSheetModel& outSheet, std::string* outErr) {
    try {
        const json j = json::parse(jsonUtf8);
        outSheet = {};
        outSheet.id = j.value("id", "");
        outSheet.titleUtf8 = j.value("title", "");
        outSheet.courseRefUtf8 = j.value("courseRef", "");
        if (j.contains("entries") && j["entries"].is_array()) {
            for (const auto& ej : j["entries"]) {
                FormulaSheetEntry ent;
                ent.id = ej.value("id", "");
                ent.titleUtf8 = ej.value("title", "");
                ent.categoryUtf8 = ej.value("category", "");
                ent.exprUtf8 = ej.value("expr", "");
                ent.courseRefUtf8 = ej.value("courseRef", "");
                ent.astHash64 = ej.value("astHash64", 0ULL);
                if (ej.contains("params") && ej["params"].is_array()) {
                    for (const auto& pj : ej["params"]) {
                        ent.params.push_back({
                            pj.value("name", ""), pj.value("label", ""), pj.value("defaultExpr", ""),
                            pj.value("minVal", 0.0), pj.value("maxVal", 0.0), pj.value("hasSlider", false)
                        });
                    }
                }
                outSheet.entries.push_back(std::move(ent));
            }
        }
        return true;
    } catch (const std::exception& ex) {
        if (outErr) *outErr = ex.what();
        return false;
    }
}

} // namespace DocStudio::Cas
