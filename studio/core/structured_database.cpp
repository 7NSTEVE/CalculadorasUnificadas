#include "structured_database.hpp"

#include "utf_conv.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace fs = std::filesystem;
using json = nlohmann::json;

namespace DocStudio {

namespace {

std::string Trim(const std::string& s) {
    const auto a = s.find_first_not_of(" \t\r\n");
    if (a == std::string::npos) return {};
    const auto b = s.find_last_not_of(" \t\r\n");
    return s.substr(a, b - a + 1);
}

std::string ReplaceAllCopy(std::string s, const std::string& from, const std::string& to) {
    if (from.empty()) return s;
    std::size_t pos = 0;
    while ((pos = s.find(from, pos)) != std::string::npos) {
        s.replace(pos, from.size(), to);
        pos += to.size();
    }
    return s;
}

bool EqualsFolded(const std::string& a, const std::string& b) {
    return FoldUtf8ForSearch(Trim(a), false, true) == FoldUtf8ForSearch(Trim(b), false, true);
}

bool ContainsFolded(const std::string& haystack, const std::string& needle) {
    if (needle.empty()) return true;
    return FoldUtf8ForSearch(haystack, false, true).find(FoldUtf8ForSearch(needle, false, true)) != std::string::npos;
}

double SafeParseDouble(const std::string& s, double defVal = 0.0) {
    const std::string t = Trim(s);
    if (t.empty()) return defVal;
    try { return std::stod(t); } catch (...) { return defVal; }
}

std::string FormatDoubleCompact(double v) {
    if (std::abs(v - std::round(v)) < 1e-9) {
        return std::to_string(static_cast<long long>(std::llround(v)));
    }
    char buf[64]{};
    std::snprintf(buf, sizeof(buf), "%.4f", v);
    std::string s = buf;
    while (!s.empty() && s.back() == '0') s.pop_back();
    if (!s.empty() && s.back() == '.') s.pop_back();
    return s;
}

std::vector<std::string> SplitArgsTopLevel(const std::string& insideParens) {
    std::vector<std::string> args;
    std::string cur;
    int depth = 0;
    bool inQuotes = false;
    for (char ch : insideParens) {
        if (ch == '"') inQuotes = !inQuotes;
        if (!inQuotes) {
            if (ch == '(') ++depth;
            else if (ch == ')') --depth;
            else if (ch == ',' && depth == 0) {
                args.push_back(Trim(cur));
                cur.clear();
                continue;
            }
        }
        cur.push_back(ch);
    }
    if (!Trim(cur).empty() || !args.empty()) args.push_back(Trim(cur));
    return args;
}

std::string StripOuterQuotes(const std::string& s) {
    const std::string t = Trim(s);
    if (t.size() >= 2 && ((t.front() == '"' && t.back() == '"') || (t.front() == '\'' && t.back() == '\''))) {
        return t.substr(1, t.size() - 2);
    }
    return t;
}

} // namespace

// ---------------- Date Arithmetic Utilities ----------------

int StructuredDatabase::ParseIsoDateToEpochDays(const std::string& isoDateUtf8) {
    const std::string s = Trim(isoDateUtf8);
    if (s.size() < 10 || s[4] != '-' || s[7] != '-') return 0;
    int y = 0, m = 0, d = 0;
    try {
        y = std::stoi(s.substr(0, 4));
        m = std::stoi(s.substr(5, 2));
        d = std::stoi(s.substr(8, 2));
    } catch (...) {
        return 0;
    }
    // Civil calendar algorithm (Howard Hinnant days_from_civil)
    y -= (m <= 2) ? 1 : 0;
    const int era = (y >= 0 ? y : y - 399) / 400;
    const unsigned yoe = static_cast<unsigned>(y - era * 400);
    const unsigned doy = (153u * static_cast<unsigned>(m + (m > 2 ? -3 : 9)) + 2u) / 5u + static_cast<unsigned>(d) - 1u;
    const unsigned doe = yoe * 365u + yoe / 4u - yoe / 100u + doy;
    return era * 146097 + static_cast<int>(doe) - 719468;
}

std::string StructuredDatabase::FormatEpochDaysToIsoDate(int z) {
    z += 719468;
    const int era = (z >= 0 ? z : z - 146096) / 146097;
    const unsigned doe = static_cast<unsigned>(z - era * 146097);
    const unsigned yoe = (doe - doe / 1460u + doe / 36524u - doe / 146096u) / 365u;
    int y = static_cast<int>(yoe) + era * 400;
    const unsigned doy = doe - (365u * yoe + yoe / 4u - yoe / 100u);
    const unsigned mp = (5u * doy + 2u) / 153u;
    const unsigned d = doy - (153u * mp + 2u) / 5u + 1u;
    const int m = static_cast<int>(mp) + (mp < 10u ? 3 : -9);
    y += (m <= 2) ? 1 : 0;
    char buf[32]{};
    std::snprintf(buf, sizeof(buf), "%04d-%02d-%02u", y, m, d);
    return buf;
}

// ---------------- StructuredDatabase Core ----------------

StructuredDatabase::StructuredDatabase(std::string dbIdUtf8, std::string titleUtf8)
    : dbIdUtf8_(std::move(dbIdUtf8)), titleUtf8_(std::move(titleUtf8))
{
    DbViewConfig defView;
    defView.viewIdUtf8 = "view_table";
    defView.nameUtf8 = "Tabla Principal";
    defView.kind = DbViewKind::Table;
    views_.push_back(std::move(defView));
    activeViewId_ = "view_table";
}

void StructuredDatabase::AddColumn(DbColumnDef col) {
    if (col.idUtf8.empty()) col.idUtf8 = "col_" + std::to_string(columns_.size() + 1);
    for (auto& existing : columns_) {
        if (existing.idUtf8 == col.idUtf8) {
            existing = std::move(col);
            return;
        }
    }
    columns_.push_back(std::move(col));
}

const DbColumnDef* StructuredDatabase::FindColumn(const std::string& colIdUtf8) const {
    for (const auto& c : columns_) {
        if (c.idUtf8 == colIdUtf8 || EqualsFolded(c.nameUtf8, colIdUtf8)) return &c;
    }
    return nullptr;
}

std::string StructuredDatabase::AddRecord(DbRecord rec) {
    if (rec.recordIdUtf8.empty()) {
        rec.recordIdUtf8 = "rec_" + std::to_string(nextRecSeq_++);
    } else {
        ++nextRecSeq_;
    }
    if (rec.titleUtf8.empty()) rec.titleUtf8 = "Registro " + rec.recordIdUtf8;
    records_.push_back(std::move(rec));
    return records_.back().recordIdUtf8;
}

DbRecord* StructuredDatabase::FindRecordMutable(const std::string& recordIdUtf8) {
    for (auto& r : records_) {
        if (r.recordIdUtf8 == recordIdUtf8) return &r;
    }
    return nullptr;
}

const DbRecord* StructuredDatabase::FindRecord(const std::string& recordIdUtf8) const {
    for (const auto& r : records_) {
        if (r.recordIdUtf8 == recordIdUtf8) return &r;
    }
    return nullptr;
}

bool StructuredDatabase::RemoveRecord(const std::string& recordIdUtf8) {
    const auto it = std::remove_if(records_.begin(), records_.end(), [&](const DbRecord& r) {
        return r.recordIdUtf8 == recordIdUtf8;
    });
    if (it == records_.end()) return false;
    records_.erase(it, records_.end());
    return true;
}

bool StructuredDatabase::SetCellText(const std::string& recordIdUtf8, const std::string& colIdUtf8, const std::string& valUtf8) {
    DbRecord* r = FindRecordMutable(recordIdUtf8);
    if (!r) return false;
    auto& cell = r->cells[colIdUtf8];
    cell.textUtf8 = valUtf8;
    cell.numberVal = SafeParseDouble(valUtf8, 0.0);
    cell.boolVal = (valUtf8 == "true" || valUtf8 == "1");
    return true;
}

bool StructuredDatabase::SetCellNumber(const std::string& recordIdUtf8, const std::string& colIdUtf8, double val) {
    DbRecord* r = FindRecordMutable(recordIdUtf8);
    if (!r) return false;
    auto& cell = r->cells[colIdUtf8];
    cell.numberVal = val;
    cell.textUtf8 = FormatDoubleCompact(val);
    cell.boolVal = (std::abs(val) > 1e-12);
    return true;
}

bool StructuredDatabase::SetCellBool(const std::string& recordIdUtf8, const std::string& colIdUtf8, bool val) {
    DbRecord* r = FindRecordMutable(recordIdUtf8);
    if (!r) return false;
    auto& cell = r->cells[colIdUtf8];
    cell.boolVal = val;
    cell.numberVal = val ? 1.0 : 0.0;
    cell.textUtf8 = val ? "true" : "false";
    return true;
}

bool StructuredDatabase::SetCellList(const std::string& recordIdUtf8, const std::string& colIdUtf8, std::vector<std::string> listUtf8) {
    DbRecord* r = FindRecordMutable(recordIdUtf8);
    if (!r) return false;
    auto& cell = r->cells[colIdUtf8];
    std::ostringstream oss;
    for (std::size_t i = 0; i < listUtf8.size(); ++i) {
        if (i > 0) oss << ", ";
        oss << listUtf8[i];
    }
    cell.listUtf8 = std::move(listUtf8);
    cell.textUtf8 = oss.str();
    cell.numberVal = static_cast<double>(cell.listUtf8.size());
    cell.boolVal = !cell.listUtf8.empty();
    return true;
}

void StructuredDatabase::ImportRecordsFromVaultNotes(const KnowledgeVault& vault, const std::string& folderFilterUtf8) {
    for (const auto& n : vault.ActiveNotes()) {
        if (!folderFilterUtf8.empty() && n.folderPathUtf8.find(folderFilterUtf8) != 0) continue;
        DbRecord rec;
        rec.recordIdUtf8 = "vrec_" + n.noteIdUtf8;
        rec.titleUtf8 = n.titleUtf8;
        rec.linkedMarkdownNoteIdUtf8 = n.noteIdUtf8;
        rec.bodyMarkdownUtf8 = n.bodyMarkdownUtf8;
        for (const auto& p : n.properties) {
            DbCellValue cv;
            cv.textUtf8 = p.valueUtf8;
            cv.numberVal = SafeParseDouble(p.valueUtf8, 0.0);
            cv.boolVal = (p.valueUtf8 == "true");
            cv.listUtf8 = p.listValuesUtf8;
            if (!cv.listUtf8.empty() && cv.textUtf8.empty()) {
                std::ostringstream oss;
                for (std::size_t i = 0; i < cv.listUtf8.size(); ++i) {
                    if (i > 0) oss << ", ";
                    oss << cv.listUtf8[i];
                }
                cv.textUtf8 = oss.str();
            }
            rec.cells[p.keyUtf8] = std::move(cv);
        }
        AddRecord(std::move(rec));
    }
}

void StructuredDatabase::SyncRecordsToVaultNotes(KnowledgeVault& vault) const {
    for (const auto& rec : records_) {
        std::string targetNoteId = rec.linkedMarkdownNoteIdUtf8;
        if (targetNoteId.empty() && !rec.titleUtf8.empty()) {
            if (const auto* existing = vault.FindNoteByTitleOrAlias(rec.titleUtf8)) {
                targetNoteId = existing->noteIdUtf8;
            } else {
                targetNoteId = vault.CreateNote(rec.titleUtf8, titleUtf8_.empty() ? "BaseDeDatos" : titleUtf8_, rec.bodyMarkdownUtf8);
            }
        }
        if (targetNoteId.empty()) continue;
        for (const auto& kv : rec.cells) {
            const DbColumnDef* col = FindColumn(kv.first);
            NotePropType pt = NotePropType::Text;
            if (col) {
                if (col->type == DbColumnType::Number || col->type == DbColumnType::Formula || col->type == DbColumnType::Rollup) pt = NotePropType::Number;
                else if (col->type == DbColumnType::Date) pt = NotePropType::Date;
                else if (col->type == DbColumnType::Checkbox) pt = NotePropType::Checkbox;
                else if (col->type == DbColumnType::MultiSelect) pt = NotePropType::List;
            }
            vault.SetNoteProperty(targetNoteId, { kv.first, pt, kv.second.textUtf8, kv.second.listUtf8 });
        }
    }
}

void StructuredDatabase::RegisterLinkedDatabase(const StructuredDatabase* targetDb) {
    if (!targetDb) return;
    linkedDbs_[targetDb->Id()] = targetDb;
}

DbCellValue StructuredDatabase::EvaluateFormulaForRecord(
    const DbRecord& rec,
    const std::string& exprUtf8,
    const KnowledgeVault* optionalVault) const
{
    static thread_local int s_evalDepth = 0;
    struct DepthGuard {
        int& d;
        explicit DepthGuard(int& ref) : d(ref) { ++d; }
        ~DepthGuard() { --d; }
    };
    DbCellValue out;
    if (s_evalDepth >= 16) return out;
    DepthGuard guard(s_evalDepth);

    std::string e = Trim(exprUtf8);
    if (!e.empty() && e.front() == '=') e = Trim(e.substr(1));
    // Strip balanced outer parentheses e.g. "(horas + 2)"
    while (e.size() >= 2 && e.front() == '(' && e.back() == ')') {
        int d = 0;
        bool inStr = false;
        bool wrapsAll = true;
        for (std::size_t i = 0; i + 1 < e.size(); ++i) {
            if (e[i] == '"') inStr = !inStr;
            if (inStr) continue;
            if (e[i] == '(') ++d;
            else if (e[i] == ')') { --d; if (d == 0) { wrapsAll = false; break; } }
        }
        if (!wrapsAll) break;
        e = Trim(e.substr(1, e.size() - 2));
    }
    if (e.empty()) return out;

    auto resolveProp = [&](const std::string& rawToken) -> DbCellValue {
        const std::string trimmedRaw = Trim(rawToken);
        if (trimmedRaw != e && trimmedRaw.find('(') != std::string::npos && !trimmedRaw.empty() && trimmedRaw.back() == ')') {
            return EvaluateFormulaForRecord(rec, trimmedRaw, optionalVault);
        }
        const std::string tok = StripOuterQuotes(rawToken);
        // Linked note property lookup: link("NoteTitle", "propKey") or linked.propKey
        if (tok.rfind("linked.", 0) == 0 && optionalVault) {
            const std::string propName = tok.substr(7);
            std::string targetNoteTitle;
            auto itRel = rec.cells.find("nota_enlazada");
            if (itRel != rec.cells.end()) targetNoteTitle = StripOuterQuotes(ReplaceAllCopy(ReplaceAllCopy(itRel->second.textUtf8, "[[", ""), "]]", ""));
            if (!targetNoteTitle.empty()) {
                if (const auto* vn = optionalVault->FindNoteByTitleOrAlias(targetNoteTitle)) {
                    for (const auto& np : vn->properties) {
                        if (EqualsFolded(np.keyUtf8, propName)) {
                            DbCellValue cv;
                            cv.textUtf8 = np.valueUtf8;
                            cv.numberVal = SafeParseDouble(np.valueUtf8, 0.0);
                            cv.boolVal = (np.valueUtf8 == "true");
                            cv.listUtf8 = np.listValuesUtf8;
                            return cv;
                        }
                    }
                }
            }
        }
        auto it = rec.cells.find(tok);
        if (it != rec.cells.end()) return it->second;
        for (const auto& c : columns_) {
            if (EqualsFolded(c.nameUtf8, tok) || EqualsFolded(c.idUtf8, tok)) {
                auto it2 = rec.cells.find(c.idUtf8);
                if (it2 != rec.cells.end()) return it2->second;
            }
        }
        DbCellValue lit;
        lit.textUtf8 = tok;
        lit.numberVal = SafeParseDouble(tok, 0.0);
        lit.boolVal = (tok == "true" || tok == "1");
        return lit;
    };

    // 1. Top-level infix arithmetic (+, -, *, /) outside parentheses/quotes (supports "prop(\"horas\") * 2", "(a + b) * c")
    auto evalInfix = [&](const char* ops) -> bool {
        int depth = 0;
        bool inStr = false;
        for (std::size_t i = e.size(); i > 0; --i) {
            const char ch = e[i - 1];
            if (ch == '"') inStr = !inStr;
            if (inStr) continue;
            if (ch == ')') ++depth;
            else if (ch == '(') --depth;
            else if (depth == 0 && i > 1) {
                for (const char* op = ops; *op; ++op) {
                    if (ch == *op) {
                        const std::string lhsStr = Trim(e.substr(0, i - 1));
                        const std::string rhsStr = Trim(e.substr(i));
                        if (lhsStr.empty() || rhsStr.empty()) continue;
                        const double a = EvaluateFormulaForRecord(rec, lhsStr, optionalVault).numberVal;
                        const double b = EvaluateFormulaForRecord(rec, rhsStr, optionalVault).numberVal;
                        double r = 0.0;
                        if (ch == '+') r = a + b;
                        else if (ch == '-') r = a - b;
                        else if (ch == '*') r = a * b;
                        else if (ch == '/') r = (std::abs(b) > 1e-12) ? (a / b) : 0.0;
                        out.numberVal = r;
                        out.textUtf8 = FormatDoubleCompact(r);
                        out.boolVal = (std::abs(r) > 1e-12);
                        return true;
                    }
                }
            }
        }
        return false;
    };
    if (evalInfix("+-")) return out;
    if (evalInfix("*/")) return out;

    // 2. Prefix function call: fn(args...) when closing ')' is at end of expression
    const auto lp = e.find('(');
    const auto rp = e.rfind(')');
    if (lp != std::string::npos && rp == e.size() - 1 && rp > lp) {
        const std::string fn = FoldUtf8ForSearch(Trim(e.substr(0, lp)), false, true);
        const auto args = SplitArgsTopLevel(e.substr(lp + 1, rp - lp - 1));

        // Explicit property accessor: prop("colId")
        if (fn == "prop" && !args.empty()) {
            return resolveProp(args[0]);
        }

        // Numeric functions: add(a,b), sub(a,b), mul(a,b), div(a,b), round(a)
        if ((fn == "add" || fn == "sub" || fn == "mul" || fn == "div") && args.size() >= 2) {
            const double a = resolveProp(args[0]).numberVal;
            const double b = resolveProp(args[1]).numberVal;
            double r = 0.0;
            if (fn == "add") r = a + b;
            else if (fn == "sub") r = a - b;
            else if (fn == "mul") r = a * b;
            else if (fn == "div") r = (std::abs(b) > 1e-12) ? (a / b) : 0.0;
            out.numberVal = r;
            out.textUtf8 = FormatDoubleCompact(r);
            out.boolVal = (std::abs(r) > 1e-12);
            return out;
        }
        // Date functions: dateAdd(dateCol, days), dateDiffDays(d1, d2)
        if (fn == "dateadd" && args.size() >= 2) {
            const int baseDays = ParseIsoDateToEpochDays(resolveProp(args[0]).textUtf8);
            const int delta = static_cast<int>(std::llround(resolveProp(args[1]).numberVal));
            out.textUtf8 = FormatEpochDaysToIsoDate(baseDays + delta);
            out.numberVal = static_cast<double>(baseDays + delta);
            return out;
        }
        if (fn == "datediffdays" && args.size() >= 2) {
            const int d1 = ParseIsoDateToEpochDays(resolveProp(args[0]).textUtf8);
            const int d2 = ParseIsoDateToEpochDays(resolveProp(args[1]).textUtf8);
            const int diff = d2 - d1;
            out.numberVal = static_cast<double>(diff);
            out.textUtf8 = std::to_string(diff);
            return out;
        }
        // Text functions: concat(a, b, ...), upper(a), lower(a)
        if (fn == "concat") {
            std::string acc;
            for (const auto& a : args) acc += resolveProp(a).textUtf8;
            out.textUtf8 = acc;
            return out;
        }
        if (fn == "upper" && !args.empty()) {
            std::string s = resolveProp(args[0]).textUtf8;
            for (char& ch : s) if (static_cast<unsigned char>(ch) < 0x80) ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
            out.textUtf8 = s;
            return out;
        }
        if (fn == "lower" && !args.empty()) {
            std::string s = resolveProp(args[0]).textUtf8;
            for (char& ch : s) if (static_cast<unsigned char>(ch) < 0x80) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
            out.textUtf8 = s;
            return out;
        }
        // Boolean / Logical functions: if(condProp, trueVal, falseVal), gt(a,b), eq(a,b), and(a,b), or(a,b)
        if (fn == "gt" && args.size() >= 2) {
            const bool b = resolveProp(args[0]).numberVal > resolveProp(args[1]).numberVal;
            out.boolVal = b; out.numberVal = b ? 1.0 : 0.0; out.textUtf8 = b ? "true" : "false";
            return out;
        }
        if (fn == "if" && args.size() >= 3) {
            const DbCellValue cond = EvaluateFormulaForRecord(rec, args[0], optionalVault);
            const bool isTrue = cond.boolVal || (cond.textUtf8 == "true") || (std::abs(cond.numberVal) > 1e-12 && cond.textUtf8 != "false");
            return resolveProp(isTrue ? args[1] : args[2]);
        }
        if ((fn == "and" || fn == "or") && args.size() >= 2) {
            const bool a = resolveProp(args[0]).boolVal;
            const bool b = resolveProp(args[1]).boolVal;
            const bool r = (fn == "and") ? (a && b) : (a || b);
            out.boolVal = r; out.numberVal = r ? 1.0 : 0.0; out.textUtf8 = r ? "true" : "false";
            return out;
        }
        // List functions: listCount(listCol), listJoin(listCol, sep), listContains(listCol, item)
        if (fn == "listcount" && !args.empty()) {
            const auto cv = resolveProp(args[0]);
            const std::size_t n = cv.listUtf8.size();
            out.numberVal = static_cast<double>(n);
            out.textUtf8 = std::to_string(n);
            return out;
        }
        if (fn == "listjoin" && args.size() >= 2) {
            const auto cv = resolveProp(args[0]);
            const std::string sep = StripOuterQuotes(args[1]);
            std::ostringstream oss;
            for (std::size_t i = 0; i < cv.listUtf8.size(); ++i) {
                if (i > 0) oss << sep;
                oss << cv.listUtf8[i];
            }
            out.textUtf8 = oss.str();
            return out;
        }
        if (fn == "listcontains" && args.size() >= 2) {
            const auto cv = resolveProp(args[0]);
            const std::string needle = StripOuterQuotes(args[1]);
            bool found = false;
            for (const auto& item : cv.listUtf8) {
                if (EqualsFolded(item, needle)) { found = true; break; }
            }
            out.boolVal = found; out.numberVal = found ? 1.0 : 0.0; out.textUtf8 = found ? "true" : "false";
            return out;
        }
    }

    return resolveProp(e);
}

void StructuredDatabase::RecomputeFormulasAndRollups(const KnowledgeVault* optionalVault) {
    // 1. Evaluate Rollups across linked databases
    for (const auto& col : columns_) {
        if (col.type != DbColumnType::Rollup) continue;
        const DbColumnDef* relCol = FindColumn(col.relationColumnIdUtf8);
        if (!relCol) continue;
        auto itDb = linkedDbs_.find(relCol->targetDatabaseIdUtf8);
        const StructuredDatabase* targetDb = (itDb != linkedDbs_.end()) ? itDb->second : this;

        for (auto& rec : records_) {
            std::vector<const DbRecord*> related;
            auto itCell = rec.cells.find(relCol->idUtf8);
            if (itCell != rec.cells.end()) {
                for (const auto& targetRecId : itCell->second.listUtf8) {
                    if (const auto* tr = targetDb->FindRecord(targetRecId)) related.push_back(tr);
                }
            }

            DbCellValue rollVal;
            if (col.rollupFunc == DbRollupFunc::Count) {
                rollVal.numberVal = static_cast<double>(related.size());
                rollVal.textUtf8 = std::to_string(related.size());
            } else if (col.rollupFunc == DbRollupFunc::PercentChecked) {
                std::size_t checked = 0;
                for (const auto* tr : related) {
                    auto itP = tr->cells.find(col.targetPropertyIdUtf8);
                    if (itP != tr->cells.end() && itP->second.boolVal) ++checked;
                }
                const double pct = related.empty() ? 0.0 : (100.0 * static_cast<double>(checked) / static_cast<double>(related.size()));
                rollVal.numberVal = pct;
                rollVal.textUtf8 = FormatDoubleCompact(pct) + "%";
            } else {
                double sum = 0.0;
                double minV = 1e18;
                double maxV = -1e18;
                std::size_t n = 0;
                for (const auto* tr : related) {
                    auto itP = tr->cells.find(col.targetPropertyIdUtf8);
                    if (itP != tr->cells.end()) {
                        const double v = itP->second.numberVal;
                        sum += v;
                        minV = (std::min)(minV, v);
                        maxV = (std::max)(maxV, v);
                        ++n;
                    }
                }
                double res = 0.0;
                if (col.rollupFunc == DbRollupFunc::Sum) res = sum;
                else if (col.rollupFunc == DbRollupFunc::Avg) res = (n > 0) ? (sum / static_cast<double>(n)) : 0.0;
                else if (col.rollupFunc == DbRollupFunc::Min) res = (n > 0) ? minV : 0.0;
                else if (col.rollupFunc == DbRollupFunc::Max) res = (n > 0) ? maxV : 0.0;
                rollVal.numberVal = res;
                rollVal.textUtf8 = FormatDoubleCompact(res);
            }
            rec.cells[col.idUtf8] = std::move(rollVal);
        }
    }

    // 2. Evaluate Formula columns
    for (const auto& col : columns_) {
        if (col.type != DbColumnType::Formula) continue;
        for (auto& rec : records_) {
            rec.cells[col.idUtf8] = EvaluateFormulaForRecord(rec, col.formulaExprUtf8, optionalVault);
        }
    }
}

void StructuredDatabase::AddOrUpdateView(DbViewConfig view) {
    if (view.viewIdUtf8.empty()) view.viewIdUtf8 = "view_" + std::to_string(views_.size() + 1);
    for (auto& v : views_) {
        if (v.viewIdUtf8 == view.viewIdUtf8) {
            v = std::move(view);
            return;
        }
    }
    views_.push_back(std::move(view));
}

const DbViewConfig* StructuredDatabase::FindView(const std::string& viewIdUtf8) const {
    for (const auto& v : views_) {
        if (v.viewIdUtf8 == viewIdUtf8 || EqualsFolded(v.nameUtf8, viewIdUtf8)) return &v;
    }
    return nullptr;
}

bool StructuredDatabase::SetActiveView(const std::string& viewIdUtf8) {
    if (const auto* v = FindView(viewIdUtf8)) {
        activeViewId_ = v->viewIdUtf8;
        return true;
    }
    return false;
}

std::vector<DbRecord> StructuredDatabase::QueryViewRecords(const DbViewConfig& view) const {
    auto evalRule = [&](const DbRecord& rec, const DbFilterRule& rule) -> bool {
        if (rule.columnIdUtf8 == "title") {
            if (rule.opUtf8 == "contains") return ContainsFolded(rec.titleUtf8, rule.valueUtf8);
            return EqualsFolded(rec.titleUtf8, rule.valueUtf8);
        }
        auto it = rec.cells.find(rule.columnIdUtf8);
        const DbCellValue empty{};
        const DbCellValue& cv = (it != rec.cells.end()) ? it->second : empty;

        if (rule.opUtf8 == "is_checked") return cv.boolVal;
        if (rule.opUtf8 == "contains") {
            if (ContainsFolded(cv.textUtf8, rule.valueUtf8)) return true;
            for (const auto& item : cv.listUtf8) if (ContainsFolded(item, rule.valueUtf8)) return true;
            return false;
        }
        if (rule.opUtf8 == "!=") return !EqualsFolded(cv.textUtf8, rule.valueUtf8);
        if (rule.opUtf8 == ">" || rule.opUtf8 == ">=" || rule.opUtf8 == "<" || rule.opUtf8 == "<=") {
            const DbColumnDef* col = FindColumn(rule.columnIdUtf8);
            if (col && col->type == DbColumnType::Date) {
                const int dA = ParseIsoDateToEpochDays(cv.textUtf8);
                const int dB = ParseIsoDateToEpochDays(rule.valueUtf8);
                if (rule.opUtf8 == ">") return dA > dB;
                if (rule.opUtf8 == ">=") return dA >= dB;
                if (rule.opUtf8 == "<") return dA < dB;
                return dA <= dB;
            }
            const double nA = cv.numberVal;
            const double nB = SafeParseDouble(rule.valueUtf8, 0.0);
            if (rule.opUtf8 == ">") return nA > nB;
            if (rule.opUtf8 == ">=") return nA >= nB;
            if (rule.opUtf8 == "<") return nA < nB;
            return nA <= nB;
        }
        return EqualsFolded(cv.textUtf8, rule.valueUtf8);
    };

    std::vector<DbRecord> out;
    for (const auto& rec : records_) {
        bool keep = view.filters.empty() ? true : view.filterConjunctionAnd;
        for (const auto& f : view.filters) {
            const bool m = evalRule(rec, f);
            if (view.filterConjunctionAnd) {
                if (!m) { keep = false; break; }
            } else {
                if (m) { keep = true; break; }
            }
        }
        if (keep) out.push_back(rec);
    }

    if (!view.sorts.empty()) {
        std::stable_sort(out.begin(), out.end(), [&](const DbRecord& a, const DbRecord& b) {
            for (const auto& s : view.sorts) {
                if (s.columnIdUtf8 == "title") {
                    if (a.titleUtf8 != b.titleUtf8) {
                        return s.ascending ? (a.titleUtf8 < b.titleUtf8) : (a.titleUtf8 > b.titleUtf8);
                    }
                    continue;
                }
                const auto itA = a.cells.find(s.columnIdUtf8);
                const auto itB = b.cells.find(s.columnIdUtf8);
                const DbColumnDef* col = FindColumn(s.columnIdUtf8);
                if (col && (col->type == DbColumnType::Number || col->type == DbColumnType::Rollup || col->type == DbColumnType::Formula)) {
                    const double nA = (itA != a.cells.end()) ? itA->second.numberVal : 0.0;
                    const double nB = (itB != b.cells.end()) ? itB->second.numberVal : 0.0;
                    if (std::abs(nA - nB) > 1e-9) {
                        return s.ascending ? (nA < nB) : (nA > nB);
                    }
                } else {
                    const std::string tA = (itA != a.cells.end()) ? itA->second.textUtf8 : "";
                    const std::string tB = (itB != b.cells.end()) ? itB->second.textUtf8 : "";
                    if (tA != tB) {
                        return s.ascending ? (tA < tB) : (tA > tB);
                    }
                }
            }
            return a.recordIdUtf8 < b.recordIdUtf8;
        });
    }
    return out;
}

std::vector<DbGroupBucket> StructuredDatabase::QueryGroupedBuckets(
    const DbViewConfig& view,
    const std::string& sumColIdUtf8) const
{
    const auto filtered = QueryViewRecords(view);
    std::map<std::string, DbGroupBucket> buckets;
    for (const auto& rec : filtered) {
        std::string key = "Sin grupo";
        if (!view.groupByColumnIdUtf8.empty()) {
            auto it = rec.cells.find(view.groupByColumnIdUtf8);
            if (it != rec.cells.end() && !it->second.textUtf8.empty()) {
                key = it->second.textUtf8;
            }
        }
        auto& b = buckets[key];
        b.groupKeyUtf8 = key;
        if (!sumColIdUtf8.empty()) {
            auto itN = rec.cells.find(sumColIdUtf8);
            if (itN != rec.cells.end()) b.numericSum += itN->second.numberVal;
        }
        b.records.push_back(rec);
    }

    std::vector<DbGroupBucket> out;
    out.reserve(buckets.size());
    for (auto& kv : buckets) {
        if (!kv.second.records.empty()) {
            kv.second.numericAvg = kv.second.numericSum / static_cast<double>(kv.second.records.size());
        }
        out.push_back(std::move(kv.second));
    }
    return out;
}

bool StructuredDatabase::MoveKanbanCard(
    const std::string& recordIdUtf8,
    const std::string& statusColumnIdUtf8,
    const std::string& newStatusUtf8)
{
    return SetCellText(recordIdUtf8, statusColumnIdUtf8, newStatusUtf8);
}

std::vector<DbCalendarEntry> StructuredDatabase::BuildCalendarProjection(
    const DbViewConfig& view,
    const std::string& yearMonthPrefixUtf8) const
{
    std::vector<DbCalendarEntry> out;
    const std::string dateCol = view.dateColumnIdUtf8.empty() ? "fecha" : view.dateColumnIdUtf8;
    for (const auto& rec : QueryViewRecords(view)) {
        auto it = rec.cells.find(dateCol);
        if (it == rec.cells.end() || it->second.textUtf8.empty()) continue;
        if (!yearMonthPrefixUtf8.empty() && it->second.textUtf8.find(yearMonthPrefixUtf8) != 0) continue;
        out.push_back({ it->second.textUtf8, rec.recordIdUtf8, rec.titleUtf8 });
    }
    return out;
}

std::vector<DbTimelineBar> StructuredDatabase::BuildTimelineProjection(const DbViewConfig& view) const {
    std::vector<DbTimelineBar> out;
    const std::string sCol = view.dateColumnIdUtf8.empty() ? "inicio" : view.dateColumnIdUtf8;
    const std::string eCol = view.endDateColumnIdUtf8.empty() ? "fin" : view.endDateColumnIdUtf8;
    for (const auto& rec : QueryViewRecords(view)) {
        auto itS = rec.cells.find(sCol);
        auto itE = rec.cells.find(eCol);
        if (itS == rec.cells.end() || itS->second.textUtf8.empty()) continue;
        const std::string sDate = itS->second.textUtf8;
        const std::string eDate = (itE != rec.cells.end() && !itE->second.textUtf8.empty()) ? itE->second.textUtf8 : sDate;
        const int dur = (std::max)(1, ParseIsoDateToEpochDays(eDate) - ParseIsoDateToEpochDays(sDate) + 1);
        out.push_back({ rec.recordIdUtf8, rec.titleUtf8, sDate, eDate, dur });
    }
    return out;
}

std::vector<DbChartSeriesPoint> StructuredDatabase::BuildChartProjection(const DbViewConfig& view) const {
    std::vector<DbChartSeriesPoint> series;
    for (const auto& b : QueryGroupedBuckets(view, view.chartValueColumnIdUtf8)) {
        const double val = view.chartValueColumnIdUtf8.empty()
            ? static_cast<double>(b.records.size())
            : b.numericSum;
        series.push_back({ b.groupKeyUtf8, val });
    }
    return series;
}

DbDashboardSummary StructuredDatabase::BuildDashboardProjection(
    const DbViewConfig& view,
    const std::string& statusColIdUtf8,
    const std::string& numColIdUtf8) const
{
    DbDashboardSummary dash;
    const auto recs = QueryViewRecords(view);
    dash.totalRecords = recs.size();
    for (const auto& r : recs) {
        auto itS = r.cells.find(statusColIdUtf8);
        if (itS != r.cells.end() &&
            (EqualsFolded(itS->second.textUtf8, "Completado") ||
             EqualsFolded(itS->second.textUtf8, "Done") ||
             itS->second.boolVal)) {
            ++dash.completedCount;
        }
        auto itN = r.cells.find(numColIdUtf8);
        if (itN != r.cells.end()) {
            dash.primaryNumericSum += itN->second.numberVal;
        }
    }
    if (dash.totalRecords > 0) {
        dash.completionRatePct = (100.0 * static_cast<double>(dash.completedCount)) / static_cast<double>(dash.totalRecords);
        dash.primaryNumericAvg = dash.primaryNumericSum / static_cast<double>(dash.totalRecords);
    }
    DbViewConfig chartView = view;
    chartView.groupByColumnIdUtf8 = statusColIdUtf8;
    chartView.chartValueColumnIdUtf8 = numColIdUtf8;
    dash.breakdownByGroup = BuildChartProjection(chartView);
    return dash;
}

std::vector<DbMapPin> StructuredDatabase::BuildMapProjection(const DbViewConfig& view) const {
    std::vector<DbMapPin> pins;
    const std::string latCol = view.latColumnIdUtf8.empty() ? "lat" : view.latColumnIdUtf8;
    const std::string lonCol = view.lonColumnIdUtf8.empty() ? "lon" : view.lonColumnIdUtf8;
    for (const auto& rec : QueryViewRecords(view)) {
        auto itLat = rec.cells.find(latCol);
        auto itLon = rec.cells.find(lonCol);
        if (itLat == rec.cells.end() || itLon == rec.cells.end()) continue;
        const double lat = std::clamp(itLat->second.numberVal, -90.0, 90.0);
        const double lon = std::clamp(itLon->second.numberVal, -180.0, 180.0);
        pins.push_back({
            rec.recordIdUtf8,
            rec.titleUtf8,
            lat,
            lon,
            (lon + 180.0) / 360.0,
            (90.0 - lat) / 180.0
        });
    }
    return pins;
}

void StructuredDatabase::AddRecordTemplate(DbRecordTemplate tpl) {
    if (tpl.templateIdUtf8.empty()) tpl.templateIdUtf8 = "rtpl_" + std::to_string(templates_.size() + 1);
    templates_.push_back(std::move(tpl));
}

std::string StructuredDatabase::CreateRecordFromTemplate(
    const std::string& templateIdUtf8,
    const std::string& customTitleSuffixUtf8)
{
    const DbRecordTemplate* found = nullptr;
    for (const auto& t : templates_) {
        if (t.templateIdUtf8 == templateIdUtf8 || EqualsFolded(t.nameUtf8, templateIdUtf8)) {
            found = &t;
            break;
        }
    }
    DbRecord rec;
    if (found) {
        rec.titleUtf8 = found->defaultTitlePrefixUtf8 + (customTitleSuffixUtf8.empty() ? "" : (" " + customTitleSuffixUtf8));
        rec.bodyMarkdownUtf8 = found->defaultBodyMarkdownUtf8;
        rec.cells = found->defaultCells;
    } else {
        rec.titleUtf8 = customTitleSuffixUtf8.empty() ? "Nuevo Registro" : customTitleSuffixUtf8;
    }
    return AddRecord(std::move(rec));
}

void StructuredDatabase::RegisterForm(DbConnectedForm form) {
    if (form.formIdUtf8.empty()) form.formIdUtf8 = "form_" + std::to_string(forms_.size() + 1);
    for (auto& existing : forms_) {
        if (existing.formIdUtf8 == form.formIdUtf8) {
            existing = std::move(form);
            return;
        }
    }
    forms_.push_back(std::move(form));
}

bool StructuredDatabase::SubmitForm(
    const std::string& formIdUtf8,
    const std::string& recordTitleUtf8,
    const std::unordered_map<std::string, DbCellValue>& submittedValues,
    std::string* outCreatedRecordId,
    std::string* outValidationError)
{
    const DbConnectedForm* form = nullptr;
    for (const auto& f : forms_) {
        if (f.formIdUtf8 == formIdUtf8 || EqualsFolded(f.titleUtf8, formIdUtf8)) {
            form = &f;
            break;
        }
    }
    if (!form) {
        if (outValidationError) *outValidationError = "Formulario no registrado.";
        return false;
    }

    for (const auto& field : form->fields) {
        if (!field.required) continue;
        auto it = submittedValues.find(field.columnIdUtf8);
        if (it == submittedValues.end() || (it->second.textUtf8.empty() && it->second.listUtf8.empty())) {
            if (outValidationError) *outValidationError = "Campo obligatorio faltante: " + field.labelUtf8;
            return false;
        }
    }

    DbRecord rec;
    rec.titleUtf8 = recordTitleUtf8.empty() ? ("Env\xC3\xADo " + form->titleUtf8) : recordTitleUtf8;
    rec.cells = submittedValues;
    const std::string id = AddRecord(std::move(rec));
    if (outCreatedRecordId) *outCreatedRecordId = id;
    return true;
}

std::string StructuredDatabase::SerializeToBaseJson() const {
    json root;
    root["format"] = "cuadernu.obsidian.base.v1";
    root["dbId"] = dbIdUtf8_;
    root["title"] = titleUtf8_;
    root["activeViewId"] = activeViewId_;

    json cols = json::array();
    for (const auto& c : columns_) {
        cols.push_back({
            {"id", c.idUtf8},
            {"name", c.nameUtf8},
            {"type", static_cast<int>(c.type)},
            {"options", c.allowedOptionsUtf8},
            {"targetDb", c.targetDatabaseIdUtf8},
            {"relCol", c.relationColumnIdUtf8},
            {"targetProp", c.targetPropertyIdUtf8},
            {"rollupFunc", static_cast<int>(c.rollupFunc)},
            {"formula", c.formulaExprUtf8}
        });
    }
    root["columns"] = std::move(cols);

    json recs = json::array();
    for (const auto& r : records_) {
        json cj = json::object();
        for (const auto& kv : r.cells) {
            cj[kv.first] = {
                {"text", kv.second.textUtf8},
                {"num", kv.second.numberVal},
                {"bool", kv.second.boolVal},
                {"list", kv.second.listUtf8}
            };
        }
        recs.push_back({
            {"id", r.recordIdUtf8},
            {"title", r.titleUtf8},
            {"linkedNoteId", r.linkedMarkdownNoteIdUtf8},
            {"bodyMd", r.bodyMarkdownUtf8},
            {"cells", std::move(cj)}
        });
    }
    root["records"] = std::move(recs);

    json vws = json::array();
    for (const auto& v : views_) {
        json flt = json::array();
        for (const auto& f : v.filters) {
            flt.push_back({ {"col", f.columnIdUtf8}, {"op", f.opUtf8}, {"val", f.valueUtf8} });
        }
        json srt = json::array();
        for (const auto& s : v.sorts) {
            srt.push_back({ {"col", s.columnIdUtf8}, {"asc", s.ascending} });
        }
        vws.push_back({
            {"id", v.viewIdUtf8},
            {"name", v.nameUtf8},
            {"kind", static_cast<int>(v.kind)},
            {"groupBy", v.groupByColumnIdUtf8},
            {"dateCol", v.dateColumnIdUtf8},
            {"endDateCol", v.endDateColumnIdUtf8},
            {"latCol", v.latColumnIdUtf8},
            {"lonCol", v.lonColumnIdUtf8},
            {"chartValCol", v.chartValueColumnIdUtf8},
            {"and", v.filterConjunctionAnd},
            {"filters", std::move(flt)},
            {"sorts", std::move(srt)}
        });
    }
    root["views"] = std::move(vws);

    json fms = json::array();
    for (const auto& fm : forms_) {
        json flds = json::array();
        for (const auto& fl : fm.fields) {
            flds.push_back({ {"col", fl.columnIdUtf8}, {"label", fl.labelUtf8}, {"required", fl.required} });
        }
        fms.push_back({ {"id", fm.formIdUtf8}, {"title", fm.titleUtf8}, {"fields", std::move(flds)} });
    }
    root["forms"] = std::move(fms);
    return root.dump(2);
}

bool StructuredDatabase::DeserializeFromBaseJson(const std::string& rawJsonUtf8, std::string* outErr) {
    try {
        const json root = json::parse(rawJsonUtf8);
        dbIdUtf8_ = root.value("dbId", "db_main");
        titleUtf8_ = root.value("title", "Base de Datos");
        activeViewId_ = root.value("activeViewId", "view_table");
        columns_.clear();
        records_.clear();
        views_.clear();
        forms_.clear();

        if (root.contains("columns") && root["columns"].is_array()) {
            for (const auto& cj : root["columns"]) {
                DbColumnDef col;
                col.idUtf8 = cj.value("id", "");
                col.nameUtf8 = cj.value("name", "");
                col.type = static_cast<DbColumnType>(cj.value("type", 0));
                if (cj.contains("options") && cj["options"].is_array()) {
                    for (const auto& o : cj["options"]) col.allowedOptionsUtf8.push_back(o.get<std::string>());
                }
                col.targetDatabaseIdUtf8 = cj.value("targetDb", "");
                col.relationColumnIdUtf8 = cj.value("relCol", "");
                col.targetPropertyIdUtf8 = cj.value("targetProp", "");
                col.rollupFunc = static_cast<DbRollupFunc>(cj.value("rollupFunc", 0));
                col.formulaExprUtf8 = cj.value("formula", "");
                columns_.push_back(std::move(col));
            }
        }
        if (root.contains("records") && root["records"].is_array()) {
            for (const auto& rj : root["records"]) {
                DbRecord rec;
                rec.recordIdUtf8 = rj.value("id", "");
                rec.titleUtf8 = rj.value("title", "");
                rec.linkedMarkdownNoteIdUtf8 = rj.value("linkedNoteId", "");
                rec.bodyMarkdownUtf8 = rj.value("bodyMd", "");
                if (rj.contains("cells") && rj["cells"].is_object()) {
                    for (auto it = rj["cells"].begin(); it != rj["cells"].end(); ++it) {
                        DbCellValue cv;
                        cv.textUtf8 = it.value().value("text", "");
                        cv.numberVal = it.value().value("num", 0.0);
                        cv.boolVal = it.value().value("bool", false);
                        if (it.value().contains("list") && it.value()["list"].is_array()) {
                            for (const auto& l : it.value()["list"]) cv.listUtf8.push_back(l.get<std::string>());
                        }
                        rec.cells[it.key()] = std::move(cv);
                    }
                }
                records_.push_back(std::move(rec));
            }
        }
        if (root.contains("views") && root["views"].is_array()) {
            for (const auto& vj : root["views"]) {
                DbViewConfig v;
                v.viewIdUtf8 = vj.value("id", "");
                v.nameUtf8 = vj.value("name", "");
                v.kind = static_cast<DbViewKind>(vj.value("kind", 0));
                v.groupByColumnIdUtf8 = vj.value("groupBy", "");
                v.dateColumnIdUtf8 = vj.value("dateCol", "");
                v.endDateColumnIdUtf8 = vj.value("endDateCol", "");
                v.latColumnIdUtf8 = vj.value("latCol", "");
                v.lonColumnIdUtf8 = vj.value("lonCol", "");
                v.chartValueColumnIdUtf8 = vj.value("chartValCol", "");
                v.filterConjunctionAnd = vj.value("and", true);
                if (vj.contains("filters") && vj["filters"].is_array()) {
                    for (const auto& fj : vj["filters"]) {
                        v.filters.push_back({ fj.value("col", ""), fj.value("op", "=="), fj.value("val", "") });
                    }
                }
                if (vj.contains("sorts") && vj["sorts"].is_array()) {
                    for (const auto& sj : vj["sorts"]) {
                        v.sorts.push_back({ sj.value("col", ""), sj.value("asc", true) });
                    }
                }
                views_.push_back(std::move(v));
            }
        }
        if (root.contains("forms") && root["forms"].is_array()) {
            for (const auto& fj : root["forms"]) {
                DbConnectedForm fm;
                fm.formIdUtf8 = fj.value("id", "");
                fm.titleUtf8 = fj.value("title", "");
                if (fj.contains("fields") && fj["fields"].is_array()) {
                    for (const auto& flj : fj["fields"]) {
                        fm.fields.push_back({ flj.value("col", ""), flj.value("label", ""), flj.value("required", false) });
                    }
                }
                if (!fm.formIdUtf8.empty()) forms_.push_back(std::move(fm));
            }
        }
        return true;
    } catch (const std::exception& ex) {
        if (outErr) *outErr = ex.what();
        return false;
    }
}

bool StructuredDatabase::SaveToBaseFile(const std::wstring& path, std::string* outErr) const {
    try {
        const fs::path p(WideToUtf8(path));
        if (p.has_parent_path()) fs::create_directories(p.parent_path());
        std::ofstream ofs(p, std::ios::binary | std::ios::trunc);
        if (!ofs) {
            if (outErr) *outErr = "No se pudo guardar archivo .base.";
            return false;
        }
        const std::string s = SerializeToBaseJson();
        ofs.write(s.data(), static_cast<std::streamsize>(s.size()));
        ofs.flush();
        return static_cast<bool>(ofs);
    } catch (const std::exception& ex) {
        if (outErr) *outErr = ex.what();
        return false;
    }
}

bool StructuredDatabase::LoadFromBaseFile(const std::wstring& path, std::string* outErr) {
    try {
        const fs::path p(WideToUtf8(path));
        std::ifstream ifs(p, std::ios::binary | std::ios::ate);
        if (!ifs) {
            if (outErr) *outErr = "No se pudo abrir archivo .base.";
            return false;
        }
        const auto sz = ifs.tellg();
        if (sz < 0) return false;
        std::string raw(static_cast<std::size_t>(sz), '\0');
        ifs.seekg(0, std::ios::beg);
        if (!raw.empty()) ifs.read(raw.data(), static_cast<std::streamsize>(raw.size()));
        return DeserializeFromBaseJson(raw, outErr);
    } catch (const std::exception& ex) {
        if (outErr) *outErr = ex.what();
        return false;
    }
}

std::uint64_t StructuredDatabase::SyncToDocumentTableObject(
    DocumentModel& doc,
    CommandStack& cmdStack,
    ObjectTarget target,
    double x,
    double y,
    bool includeCompanionChart) const
{
    const DbViewConfig* v = FindView(activeViewId_);
    DbViewConfig fallback;
    const DbViewConfig& activeV = v ? *v : fallback;
    const auto recs = QueryViewRecords(activeV);

    json tableJson;
    json rowsArr = json::array();
    json headerRow = json::array();
    headerRow.push_back("Titulo");
    for (const auto& c : columns_) headerRow.push_back(c.nameUtf8);
    rowsArr.push_back(std::move(headerRow));

    std::ostringstream plainText;
    plainText << titleUtf8_ << " [" << activeV.nameUtf8 << "]\n";
    for (std::size_t i = 0; i < recs.size() && i < 25; ++i) {
        json rj = json::array();
        rj.push_back(recs[i].titleUtf8);
        plainText << recs[i].titleUtf8;
        for (const auto& c : columns_) {
            auto it = recs[i].cells.find(c.idUtf8);
            const std::string val = (it != recs[i].cells.end()) ? it->second.textUtf8 : "";
            rj.push_back(val);
            plainText << " | " << val;
        }
        plainText << "\n";
        rowsArr.push_back(std::move(rj));
    }
    tableJson["rows"] = std::move(rowsArr);
    tableJson["basePayload"] = SerializeToBaseJson();

    AnnotationObject tbl;
    tbl.kind = ObjectKind::TableBlock;
    tbl.layerId = doc.ActiveLayerId();
    tbl.bounds = { x, y, 440.0, 180.0 };
    tbl.captionUtf8 = titleUtf8_ + " (" + std::to_string(recs.size()) + " registros)";
    tbl.textUtf8 = plainText.str();
    tbl.tableJsonUtf8 = tableJson.dump();
    if (!cmdStack.Execute(doc, std::make_unique<AddObjectCommand>(target, std::move(tbl)))) return 0;
    const auto& vec = target.isCanvas ? doc.CanvasObjects() : doc.Pages()[target.pageIndex].objects;
    const std::uint64_t tblId = vec.empty() ? 0ull : vec.back().id;

    if (includeCompanionChart) {
        AnnotationObject chart;
        chart.kind = ObjectKind::PlotBlock;
        chart.layerId = doc.ActiveLayerId();
        chart.bounds = { x + 456.0, y, 320.0, 180.0 };
        chart.captionUtf8 = "Gr\xC3\xA1""fica: " + titleUtf8_;
        chart.formulaExprUtf8 = "x";
        json pcfg;
        pcfg["expr"] = "x";
        pcfg["xMin"] = 0.0;
        pcfg["xMax"] = static_cast<double>((std::max<std::size_t>)(1, recs.size()));
        chart.plotConfigUtf8 = pcfg.dump();
        cmdStack.Execute(doc, std::make_unique<AddObjectCommand>(target, std::move(chart)));
    }
    return tblId;
}

} // namespace DocStudio
