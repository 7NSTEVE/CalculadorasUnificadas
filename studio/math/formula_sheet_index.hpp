#pragma once

#include <algorithm>
#include <cstdint>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace DocStudio::Cas {

// ---------------- F-172: Formula Structural AST Fingerprint & Alpha-Equivalence Index ----------------

struct FormulaFingerprint {
    std::uint64_t hash64 = 0;
    std::string canonicalAlphaExpr;
    std::vector<std::string> varOrder;
    bool ok = false;
};

namespace detail_fp {

inline std::uint64_t Fnv1a64(const std::string& s) noexcept {
    std::uint64_t h = 14695981039346656037ULL;
    for (unsigned char c : s) {
        h ^= static_cast<std::uint64_t>(c);
        h *= 1099511628211ULL;
    }
    return h;
}

inline bool IsReservedMathSymbol(const std::string& sym) {
    static const std::unordered_set<std::string> kRes{
        "pi", "e", "i", "j", "g0", "c0", "eps0", "mu0"
    };
    return kRes.find(sym) != kRes.end();
}

inline void CollectFormulaVars(const MathJsonNode& node, std::vector<std::string>& vars, std::unordered_set<std::string>& seen) {
    if (node.kind == MathJsonKind::Symbol) {
        if (!IsReservedMathSymbol(node.text) && seen.insert(node.text).second) {
            vars.push_back(node.text);
        }
        return;
    }
    if (node.kind == MathJsonKind::Function) {
        for (const auto& a : node.args) CollectFormulaVars(a, vars, seen);
    }
}

inline std::string SerializeWithFixedMap(
    const MathJsonNode& node,
    const std::unordered_map<std::string, std::string>& alphaMap,
    bool canonicalizeCommutative)
{
    switch (node.kind) {
    case MathJsonKind::Number: {
        std::ostringstream oss;
        oss.precision(12);
        oss << ((std::abs(node.realVal) < 1e-13) ? 0.0 : node.realVal);
        return "#" + oss.str();
    }
    case MathJsonKind::Complex: {
        std::ostringstream oss;
        oss.precision(12);
        oss << "#C(" << node.realVal << "," << node.imagVal << ")";
        return oss.str();
    }
    case MathJsonKind::String:
        return "\"" + node.text + "\"";
    case MathJsonKind::Symbol: {
        if (IsReservedMathSymbol(node.text)) return "$" + node.text;
        auto it = alphaMap.find(node.text);
        return (it != alphaMap.end()) ? it->second : node.text;
    }
    case MathJsonKind::Function: {
        std::vector<std::string> kids;
        kids.reserve(node.args.size());
        for (const auto& a : node.args) {
            kids.push_back(SerializeWithFixedMap(a, alphaMap, canonicalizeCommutative));
        }
        if (canonicalizeCommutative && (node.text == "Add" || node.text == "Multiply")) {
            std::sort(kids.begin(), kids.end());
        }
        std::string out = node.text + "(";
        for (std::size_t i = 0; i < kids.size(); ++i) {
            if (i > 0) out += ",";
            out += kids[i];
        }
        out += ")";
        return out;
    }
    }
    return "";
}

inline std::string SerializeCanonicalAlpha(
    const MathJsonNode& node,
    std::unordered_map<std::string, std::string>& alphaMap,
    std::vector<std::string>& varOrder,
    bool canonicalizeCommutative)
{
    if (!canonicalizeCommutative) {
        switch (node.kind) {
        case MathJsonKind::Number:
        case MathJsonKind::Complex:
        case MathJsonKind::String:
            return SerializeWithFixedMap(node, alphaMap, false);
        case MathJsonKind::Symbol: {
            if (IsReservedMathSymbol(node.text)) return "$" + node.text;
            auto it = alphaMap.find(node.text);
            if (it == alphaMap.end()) {
                const std::string vName = "v" + std::to_string(varOrder.size());
                alphaMap[node.text] = vName;
                varOrder.push_back(node.text);
                return vName;
            }
            return it->second;
        }
        case MathJsonKind::Function: {
            std::vector<std::string> kids;
            kids.reserve(node.args.size());
            for (const auto& a : node.args) {
                kids.push_back(SerializeCanonicalAlpha(a, alphaMap, varOrder, false));
            }
            std::string out = node.text + "(";
            for (std::size_t i = 0; i < kids.size(); ++i) {
                if (i > 0) out += ",";
                out += kids[i];
            }
            out += ")";
            return out;
        }
        }
        return "";
    }

    std::vector<std::string> vars;
    std::unordered_set<std::string> seen;
    CollectFormulaVars(node, vars, seen);
    if (vars.empty()) {
        return SerializeWithFixedMap(node, alphaMap, true);
    }

    // 1-WL structural color refinement: assign each variable a canonical structural color
    // completely independent of alphabetical variable names.
    std::unordered_map<std::string, int> color;
    for (const auto& v : vars) color[v] = 0;

    auto sigWithTarget = [&](const MathJsonNode& cur, const std::string& targetVar, auto& self) -> std::string {
        switch (cur.kind) {
        case MathJsonKind::Number: {
            std::ostringstream oss;
            oss.precision(12);
            oss << ((std::abs(cur.realVal) < 1e-13) ? 0.0 : cur.realVal);
            return "#" + oss.str();
        }
        case MathJsonKind::Complex: {
            std::ostringstream oss;
            oss.precision(12);
            oss << "#C(" << cur.realVal << "," << cur.imagVal << ")";
            return oss.str();
        }
        case MathJsonKind::String:
            return "\"" + cur.text + "\"";
        case MathJsonKind::Symbol:
            if (IsReservedMathSymbol(cur.text)) return "$" + cur.text;
            if (cur.text == targetVar) return "SELF";
            return "C" + std::to_string(color[cur.text]);
        case MathJsonKind::Function: {
            std::vector<std::string> ch;
            ch.reserve(cur.args.size());
            for (const auto& a : cur.args) ch.push_back(self(a, targetVar, self));
            if (cur.text == "Add" || cur.text == "Multiply") std::sort(ch.begin(), ch.end());
            std::string out = cur.text + "(";
            for (std::size_t i = 0; i < ch.size(); ++i) {
                if (i > 0) out += ",";
                out += ch[i];
            }
            out += ")";
            return out;
        }
        }
        return "";
    };

    std::size_t numColors = 1;
    for (std::size_t round = 0; round <= vars.size(); ++round) {
        std::vector<std::pair<std::string, std::string>> sigs;
        sigs.reserve(vars.size());
        for (const auto& v : vars) {
            sigs.push_back({ sigWithTarget(node, v, sigWithTarget), v });
        }
        std::sort(sigs.begin(), sigs.end(), [](const auto& a, const auto& b) {
            return a.first < b.first;
        });
        std::unordered_map<std::string, int> nextColor;
        int curC = 0;
        for (std::size_t i = 0; i < sigs.size(); ++i) {
            if (i > 0 && sigs[i].first != sigs[i - 1].first) ++curC;
            nextColor[sigs[i].second] = curC;
        }
        color = std::move(nextColor);
        const std::size_t nextNum = static_cast<std::size_t>(curC + 1);
        if (nextNum == numColors) break;
        numColors = nextNum;
    }

    // Group variables by structural color class
    std::vector<std::vector<std::string>> classes(numColors);
    for (const auto& v : vars) {
        classes[static_cast<std::size_t>(color[v])].push_back(v);
    }
    for (auto& cls : classes) std::sort(cls.begin(), cls.end());

    // Enumerate tied permutations across remaining symmetric color classes (bounded budget)
    // and pick the lexicographically minimum canonical serialization.
    std::string bestExpr;
    std::vector<std::string> bestOrder;
    std::unordered_map<std::string, std::string> bestMap;
    int evalBudget = 512;

    std::vector<std::string> curOrder(vars.size());
    auto search = [&](std::size_t clsIdx, std::size_t offset, auto& self) -> void {
        if (evalBudget <= 0) return;
        if (clsIdx >= classes.size()) {
            --evalBudget;
            std::unordered_map<std::string, std::string> tmpMap;
            for (std::size_t i = 0; i < curOrder.size(); ++i) {
                tmpMap[curOrder[i]] = "v" + std::to_string(i);
            }
            const std::string cand = SerializeWithFixedMap(node, tmpMap, true);
            if (bestExpr.empty() || cand < bestExpr) {
                bestExpr = cand;
                bestOrder = curOrder;
                bestMap = std::move(tmpMap);
            }
            return;
        }
        auto perm = classes[clsIdx];
        std::sort(perm.begin(), perm.end());
        do {
            for (std::size_t j = 0; j < perm.size(); ++j) {
                curOrder[offset + j] = perm[j];
            }
            self(clsIdx + 1, offset + perm.size(), self);
            if (evalBudget <= 0) break;
        } while (std::next_permutation(perm.begin(), perm.end()));
    };
    search(0, 0, search);

    alphaMap = std::move(bestMap);
    varOrder = std::move(bestOrder);
    return bestExpr;
}

} // namespace detail_fp

inline FormulaFingerprint ComputeFormulaFingerprint(
    const std::string& exprUtf8,
    bool canonicalizeCommutative = true)
{
    FormulaFingerprint fp;
    std::string target = exprUtf8;
    // Strip optional "lhs = " assignment when indexing RHS formula shape
    const auto eq = target.find('=');
    if (eq != std::string::npos) {
        const std::string lhs = target.substr(0, eq);
        bool singleIdent = !lhs.empty();
        for (char c : lhs) {
            if (!std::isspace(static_cast<unsigned char>(c)) &&
                !std::isalnum(static_cast<unsigned char>(c)) && c != '_') {
                singleIdent = false;
                break;
            }
        }
        if (singleIdent) target = target.substr(eq + 1);
    }

    MathJsonNode ast{};
    if (!ParseExpressionToMathJson(target, ast)) return fp;
    MathJsonNode canon = CanonicalizeMathJson(ast);

    std::unordered_map<std::string, std::string> alphaMap;
    fp.canonicalAlphaExpr = detail_fp::SerializeCanonicalAlpha(canon, alphaMap, fp.varOrder, canonicalizeCommutative);
    fp.hash64 = detail_fp::Fnv1a64(fp.canonicalAlphaExpr);
    fp.ok = true;
    return fp;
}

struct FormulaIndexHit {
    std::string id;
    std::string exprUtf8;
    FormulaFingerprint fingerprint{};
};

class FormulaFingerprintIndex {
public:
    bool Add(const std::string& id, const std::string& exprUtf8) {
        const auto fp = ComputeFormulaFingerprint(exprUtf8, true);
        if (!fp.ok) return false;
        Remove(id);
        entries_[id] = FormulaIndexHit{ id, exprUtf8, fp };
        byHash_[fp.hash64].push_back(id);
        return true;
    }

    void Remove(const std::string& id) {
        auto it = entries_.find(id);
        if (it == entries_.end()) return;
        const std::uint64_t h = it->second.fingerprint.hash64;
        entries_.erase(it);
        auto hit = byHash_.find(h);
        if (hit != byHash_.end()) {
            auto& vec = hit->second;
            vec.erase(std::remove(vec.begin(), vec.end(), id), vec.end());
            if (vec.empty()) byHash_.erase(hit);
        }
    }

    std::vector<FormulaIndexHit> FindAlphaEquivalent(const std::string& exprUtf8) const {
        std::vector<FormulaIndexHit> out;
        const auto fp = ComputeFormulaFingerprint(exprUtf8, true);
        if (!fp.ok) return out;
        auto it = byHash_.find(fp.hash64);
        if (it == byHash_.end()) return out;
        for (const auto& id : it->second) {
            auto eit = entries_.find(id);
            if (eit != entries_.end() && eit->second.fingerprint.canonicalAlphaExpr == fp.canonicalAlphaExpr) {
                out.push_back(eit->second);
            }
        }
        return out;
    }

    std::size_t Size() const noexcept { return entries_.size(); }

private:
    std::unordered_map<std::string, FormulaIndexHit> entries_;
    std::unordered_map<std::uint64_t, std::vector<std::string>> byHash_;
};

// ---------------- F-245: Interactive Formula Sheets Model ----------------

struct FormulaSheetParam {
    std::string name;
    std::string labelUtf8;
    std::string defaultExprUtf8;
    double minVal = 0.0;
    double maxVal = 0.0;
    bool hasSlider = false;
};

struct FormulaSheetEntry {
    std::string id;
    std::string titleUtf8;
    std::string categoryUtf8;
    std::string exprUtf8;
    std::string courseRefUtf8;
    std::uint64_t astHash64 = 0;
    std::vector<FormulaSheetParam> params;
};

struct FormulaSheetModel {
    std::string id;
    std::string titleUtf8;
    std::string courseRefUtf8;
    std::vector<FormulaSheetEntry> entries;
};

struct FormulaInstantiationResult {
    bool ok = false;
    CasEvalResult eval{};
    std::string varSubTextUtf8;
    std::string latex2DUtf8;
    std::string errorUtf8;
};

inline FormulaInstantiationResult InstantiateFormulaEntry(
    const FormulaSheetEntry& entry,
    const SymbolEnv& overrides = {})
{
    FormulaInstantiationResult res;
    SymbolEnv env;
    for (const auto& p : entry.params) {
        if (!p.defaultExprUtf8.empty()) {
            const auto pev = EvaluateExpression(p.defaultExprUtf8, env);
            if (pev.ok) env[p.name] = pev.result;
        }
    }
    for (const auto& kv : overrides) {
        env[kv.first] = kv.second;
    }

    res.eval = EvaluateExpression(entry.exprUtf8, env);
    if (!res.eval.ok) {
        res.errorUtf8 = res.eval.errorUtf8;
        return res;
    }

    const auto eqPos = entry.exprUtf8.find('=');
    const std::string lhs = (eqPos == std::string::npos) ? "" : entry.exprUtf8.substr(0, eqPos);
    const std::string rhs = (eqPos == std::string::npos) ? entry.exprUtf8 : entry.exprUtf8.substr(eqPos + 1);
    auto trim = [](std::string s) {
        while (!s.empty() && s.front() == ' ') s.erase(s.begin());
        while (!s.empty() && s.back() == ' ') s.pop_back();
        return s;
    };
    const std::string tLhs = trim(lhs);
    const std::string tRhs = trim(rhs);

    if (!tLhs.empty() && tRhs != res.eval.varSubExpansionUtf8) {
        res.varSubTextUtf8 = tLhs + " = " + tRhs + " = " + res.eval.varSubExpansionUtf8 + " = " + res.eval.formattedResultUtf8;
    } else if (!tLhs.empty()) {
        res.varSubTextUtf8 = tLhs + " = " + res.eval.formattedResultUtf8;
    } else {
        res.varSubTextUtf8 = res.eval.varSubExpansionUtf8 + " = " + res.eval.formattedResultUtf8;
    }
    res.latex2DUtf8 = FormatVarSubLatex2D(tLhs, tRhs, res.eval.varSubExpansionUtf8, res.eval.formattedResultUtf8, 0);
    res.ok = true;
    return res;
}

std::string FormulaSheetModelToJson(const FormulaSheetModel& sheet);
bool FormulaSheetModelFromJson(const std::string& jsonUtf8, FormulaSheetModel& outSheet, std::string* outErr = nullptr);

} // namespace DocStudio::Cas
