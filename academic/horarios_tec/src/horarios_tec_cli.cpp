#include "academic/horarios_tec/exporter.hpp"
#include "academic/horarios_tec/parser.hpp"
#include "academic/horarios_tec/solver.hpp"
#include "academic/horarios_tec/types.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <winhttp.h>
#pragma comment(lib, "winhttp.lib")
#else
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace fs = std::filesystem;
using namespace academic::horarios_tec;

namespace {

std::string Trim(std::string s) {
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front()))) s.erase(s.begin());
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back()))) s.pop_back();
    return s;
}

bool ReadFileToString(const fs::path& p, std::string& out, std::string* err = nullptr) {
    std::ifstream ifs(p, std::ios::binary);
    if (!ifs.is_open()) {
        if (err) *err = "failed to open file: " + p.u8string();
        return false;
    }
    std::ostringstream oss;
    oss << ifs.rdbuf();
    out = oss.str();
    return true;
}

bool WriteStringToFile(const fs::path& p, const std::string& content, std::string* err = nullptr) {
    std::error_code ec;
    if (p.has_parent_path()) fs::create_directories(p.parent_path(), ec);
    std::ofstream ofs(p, std::ios::binary | std::ios::trunc);
    if (!ofs.is_open()) {
        if (err) *err = "failed to open output: " + p.u8string();
        return false;
    }
    ofs.write(content.data(), static_cast<std::streamsize>(content.size()));
    ofs.flush();
    if (!ofs.good()) {
        if (err) *err = "failed writing output: " + p.u8string();
        return false;
    }
    return true;
}

bool FetchSimpleHttpGet(const std::string& url, std::string& outBody, std::string* outErr = nullptr) {
    const auto schemePos = url.find("://");
    if (schemePos == std::string::npos) {
        if (outErr) *outErr = "invalid URL";
        return false;
    }
    const std::string scheme = url.substr(0, schemePos);
    const bool isHttps = (scheme == "https");
    const size_t hostStart = schemePos + 3;
    const size_t pathPos = url.find('/', hostStart);
    const std::string authority = (pathPos == std::string::npos)
        ? url.substr(hostStart)
        : url.substr(hostStart, pathPos - hostStart);
    const std::string path = (pathPos == std::string::npos) ? "/" : url.substr(pathPos);

    std::string host = authority;
    int port = isHttps ? 443 : 80;
    const auto colon = authority.rfind(':');
    if (colon != std::string::npos) {
        host = authority.substr(0, colon);
        try { port = std::stoi(authority.substr(colon + 1)); } catch (...) { port = isHttps ? 443 : 80; }
    }

#ifdef _WIN32
    auto toWide = [](const std::string& s) -> std::wstring {
        if (s.empty()) return {};
        int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), nullptr, 0);
        std::wstring w(static_cast<size_t>(std::max(0, n)), L'\0');
        if (n > 0) MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), w.data(), n);
        return w;
    };
    HINTERNET hSession = WinHttpOpen(L"HorariosTEC/1.0", WINHTTP_ACCESS_TYPE_NO_PROXY,
                                     WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) return false;
    HINTERNET hConnect = WinHttpConnect(hSession, toWide(host).c_str(), static_cast<INTERNET_PORT>(port), 0);
    if (!hConnect) { WinHttpCloseHandle(hSession); return false; }
    DWORD flags = isHttps ? WINHTTP_FLAG_SECURE : 0;
    HINTERNET hReq = WinHttpOpenRequest(hConnect, L"GET", toWide(path).c_str(), nullptr,
                                        WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
    if (!hReq) { WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession); return false; }
    bool ok = false;
    if (WinHttpSendRequest(hReq, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0) &&
        WinHttpReceiveResponse(hReq, nullptr)) {
        DWORD status = 0, statusSize = sizeof(status);
        WinHttpQueryHeaders(hReq, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                            WINHTTP_HEADER_NAME_BY_INDEX, &status, &statusSize, WINHTTP_NO_HEADER_INDEX);
        if (status >= 200 && status < 300) {
            outBody.clear();
            DWORD avail = 0;
            while (WinHttpQueryDataAvailable(hReq, &avail) && avail > 0) {
                std::string buf(avail, '\0');
                DWORD read = 0;
                if (!WinHttpReadData(hReq, buf.data(), avail, &read) || read == 0) break;
                outBody.append(buf.data(), read);
            }
            ok = true;
        }
    }
    WinHttpCloseHandle(hReq);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    return ok;
#else
    if (isHttps) {
        if (outErr) *outErr = "HTTPS seats-url not supported in minimal POSIX GET helper";
        return false;
    }
    struct addrinfo hints{};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    struct addrinfo* res = nullptr;
    const std::string portStr = std::to_string(port);
    if (getaddrinfo(host.c_str(), portStr.c_str(), &hints, &res) != 0 || !res) {
        if (outErr) *outErr = "DNS/connect failed";
        return false;
    }
    int fd = -1;
    for (auto* p = res; p != nullptr; p = p->ai_next) {
        fd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (fd < 0) continue;
        if (connect(fd, p->ai_addr, p->ai_addrlen) == 0) break;
        close(fd);
        fd = -1;
    }
    freeaddrinfo(res);
    if (fd < 0) return false;

    std::string req = "GET " + path + " HTTP/1.1\r\nHost: " + authority + "\r\nConnection: close\r\n\r\n";
    size_t sent = 0;
    while (sent < req.size()) {
        ssize_t n = send(fd, req.data() + sent, req.size() - sent, 0);
        if (n <= 0) { close(fd); return false; }
        sent += static_cast<size_t>(n);
    }
    std::string raw;
    char buf[4096];
    ssize_t n = 0;
    while ((n = recv(fd, buf, sizeof(buf), 0)) > 0) {
        raw.append(buf, static_cast<size_t>(n));
    }
    close(fd);
    auto hdrEnd = raw.find("\r\n\r\n");
    if (hdrEnd == std::string::npos) return false;
    if (raw.find(" 200 ") == std::string::npos && raw.find(" 200\r\n") == std::string::npos) return false;
    outBody = raw.substr(hdrEnd + 4);
    return true;
#endif
}

bool ParseBusySpec(const std::string& spec, BusySlot& out) {
    // Format: "K:09:30-12:00"
    const auto c1 = spec.find(':');
    if (c1 == std::string::npos) return false;
    const std::string dayTok = Trim(spec.substr(0, c1));
    const std::string rest = Trim(spec.substr(c1 + 1));
    const auto dash = rest.find('-');
    if (dash == std::string::npos) return false;
    DayOfWeek d = DayOfWeek::Monday;
    int sMin = 0, eMin = 0;
    if (!TryParseDay(dayTok, d)) return false;
    if (!TryParseTimeHHMM(Trim(rest.substr(0, dash)), sMin)) return false;
    if (!TryParseTimeHHMM(Trim(rest.substr(dash + 1)), eMin)) return false;
    if (sMin >= eMin) return false;
    out.day = d;
    out.startMinute = sMin;
    out.endMinute = eMin;
    out.labelUtf8 = spec;
    out.sourceUtf8 = "cli";
    return true;
}

nlohmann::json CatalogToNormalizedJson(const CourseCatalog& cat) {
    nlohmann::json courses = nlohmann::json::array();
    for (const auto& c : cat.courses) {
        nlohmann::json grps = nlohmann::json::array();
        for (const auto& g : c.groups) {
            nlohmann::json sess = nlohmann::json::array();
            for (const auto& t : g.sessions) {
                sess.push_back({
                    {"day", DayToShortEs(t.day)},
                    {"start", FormatTimeHHMM(t.startMinute)},
                    {"end", FormatTimeHHMM(t.endMinute)},
                    {"room", t.roomUtf8}
                });
            }
            grps.push_back({
                {"id", g.idUtf8},
                {"professor", g.professorUtf8},
                {"modality", g.modalityUtf8},
                {"seats", g.seatsAvailable},
                {"sessions", sess}
            });
        }
        courses.push_back({
            {"code", c.codeUtf8},
            {"name", c.nameUtf8},
            {"credits", c.credits},
            {"groups", grps}
        });
    }
    return nlohmann::json({{"title", cat.titleUtf8}, {"term", cat.termUtf8}, {"courses", courses}});
}

nlohmann::json BuildCliSolveJson(const ScheduleResult& res) {
    nlohmann::json scheds = nlohmann::json::array();
    for (const auto& s : res.schedules) {
        nlohmann::json courses = nlohmann::json::array();
        for (const auto& sel : s.selections) {
            nlohmann::json sess = nlohmann::json::array();
            for (const auto& t : sel.group.sessions) {
                sess.push_back({
                    {"day", DayToShortEs(t.day)},
                    {"dayName", DayToSpanishName(t.day)},
                    {"start", FormatTimeHHMM(t.startMinute)},
                    {"end", FormatTimeHHMM(t.endMinute)},
                    {"room", t.roomUtf8}
                });
            }
            courses.push_back({
                {"code", sel.courseCodeUtf8},
                {"name", sel.courseNameUtf8},
                {"group_id", sel.group.idUtf8},
                {"professor", sel.group.professorUtf8},
                {"seats", sel.group.seatsAvailable},
                {"sessions", sess}
            });
        }
        scheds.push_back({
            {"rank", s.rank},
            {"score", s.metrics.totalScore},
            {"active_days", s.metrics.activeDaysCount},
            {"gap_minutes", s.metrics.totalGapMinutes},
            {"courses", courses}
        });
    }

    std::string statusStr = "Ok";
    if (res.status == SolveStatus::NoConflictFreeSchedule) {
        statusStr = "NO_CONFLICT_FREE_SCHEDULES";
    } else if (res.status == SolveStatus::InvalidInput) {
        statusStr = "INVALID_INPUT";
    } else if (res.status == SolveStatus::SearchBudgetExceeded) {
        statusStr = "SEARCH_BUDGET_EXCEEDED";
    }

    nlohmann::json courseDiags = nlohmann::json::array();
    for (const auto& cd : res.diagnostics.courseDiagnostics) {
        courseDiags.push_back({
            {"code", cd.courseCodeUtf8},
            {"initial_groups", cd.initialGroupCount},
            {"after_filter_groups", cd.afterFilterGroupCount},
            {"reason", cd.reasonUtf8}
        });
    }

    return nlohmann::json({
        {"ok", res.ok},
        {"status", statusStr},
        {"scheduleCount", res.schedules.size()},
        {"schedules", scheds},
        {"warnings", res.diagnostics.warningsUtf8},
        {"errors", res.diagnostics.errorsUtf8},
        {"courseDiagnostics", courseDiags}
    });
}

std::string GetArgVal(const std::vector<std::string>& args, const std::string& key,
                      const std::string& def = "") {
    for (size_t i = 0; i + 1 < args.size(); ++i) {
        if (args[i] == key) return args[i + 1];
    }
    return def;
}

std::vector<std::string> GetAllArgVals(const std::vector<std::string>& args, const std::string& key) {
    std::vector<std::string> out;
    for (size_t i = 0; i + 1 < args.size(); ++i) {
        if (args[i] == key) out.push_back(args[i + 1]);
    }
    return out;
}

bool HasFlag(const std::vector<std::string>& args, const std::string& key) {
    for (const auto& a : args) {
        if (a == key) return true;
    }
    return false;
}

int CmdParse(const std::vector<std::string>& args) {
    const std::string catPath = GetArgVal(args, "--catalog");
    const std::string outPath = GetArgVal(args, "--out");
    if (catPath.empty()) {
        std::cerr << "parse requires --catalog <file>\n";
        return 64;
    }
    std::string raw, err;
    if (!ReadFileToString(fs::u8path(catPath), raw, &err)) {
        std::cerr << err << "\n";
        return 2;
    }
    auto parsed = ParseCatalogJson(raw);
    if (!parsed.ok || parsed.catalog.courses.empty()) {
        std::cerr << "parse_failed: " << parsed.errorUtf8 << "\n";
        return 2;
    }
    const std::string outStr = CatalogToNormalizedJson(parsed.catalog).dump(2);
    if (!outPath.empty()) {
        if (!WriteStringToFile(fs::u8path(outPath), outStr, &err)) {
            std::cerr << err << "\n";
            return 2;
        }
    } else {
        std::cout << outStr << "\n";
    }
    return 0;
}

int CmdSolve(const std::vector<std::string>& args) {
    const std::string catPath = GetArgVal(args, "--catalog");
    const std::string outJson = GetArgVal(args, "--out-json");
    const std::string outCsv = GetArgVal(args, "--out-csv");
    const std::string seatsUrl = GetArgVal(args, "--seats-url");
    if (catPath.empty()) {
        std::cerr << "solve requires --catalog <file>\n";
        return 64;
    }
    std::string raw, err;
    if (!ReadFileToString(fs::u8path(catPath), raw, &err)) {
        std::cerr << err << "\n";
        return 2;
    }

    auto parsed = ParseCatalogJson(raw);
    if (!parsed.ok || parsed.catalog.courses.empty()) {
        std::cerr << "invalid_catalog: " << parsed.errorUtf8 << "\n";
        return 2;
    }

    // Strict validation: detect invalid sessions from raw JSON (e.g. start >= end or hour >= 24)
    try {
        auto jraw = nlohmann::json::parse(raw);
        const nlohmann::json* cArr = nullptr;
        if (jraw.is_array()) cArr = &jraw;
        else if (jraw.is_object() && jraw.contains("courses") && jraw["courses"].is_array()) cArr = &jraw["courses"];
        if (cArr) {
            for (const auto& c : *cArr) {
                if (!c.is_object() || !c.contains("groups") || !c["groups"].is_array()) continue;
                for (const auto& g : c["groups"]) {
                    if (!g.is_object() || !g.contains("sessions") || !g["sessions"].is_array()) continue;
                    for (const auto& s : g["sessions"]) {
                        if (!s.is_object()) continue;
                        if (s.contains("day") && s["day"].is_string()) {
                            DayOfWeek d{};
                            if (!TryParseDay(s["day"].get<std::string>(), d)) {
                                std::cerr << "invalid_day_token\n";
                                return 2;
                            }
                        }
                        if (s.contains("start") && s.contains("end") && s["start"].is_string() && s["end"].is_string()) {
                            int sm = 0, em = 0;
                            if (!TryParseTimeHHMM(s["start"].get<std::string>(), sm) ||
                                !TryParseTimeHHMM(s["end"].get<std::string>(), em) || sm >= em) {
                                std::cerr << "invalid_session_time_range\n";
                                return 2;
                            }
                        }
                    }
                }
            }
        }
    } catch (...) {
        std::cerr << "malformed_json\n";
        return 2;
    }

    CourseCatalog catalog = std::move(parsed.catalog);

    // Optional live/mock seats overlay
    if (!seatsUrl.empty()) {
        std::string seatsBody, httpErr;
        if (FetchSimpleHttpGet(seatsUrl, seatsBody, &httpErr)) {
            try {
                auto sj = nlohmann::json::parse(seatsBody);
                if (sj.is_object()) {
                    for (auto& c : catalog.courses) {
                        for (auto& g : c.groups) {
                            const std::string key = c.codeUtf8 + ":" + g.idUtf8;
                            if (sj.contains(key) && sj[key].is_number_integer()) {
                                g.seatsAvailable = sj[key].get<int>();
                            }
                        }
                    }
                }
            } catch (...) {}
        }
    }

    std::vector<BusySlot> busySlots;
    for (const auto& bs : GetAllArgVals(args, "--busy")) {
        BusySlot slot;
        if (!ParseBusySpec(bs, slot)) {
            std::cerr << "invalid --busy spec: " << bs << "\n";
            return 2;
        }
        busySlots.push_back(slot);
    }

    SolverPreferences prefs;
    prefs.filters.requireAvailableSeats = HasFlag(args, "--require-available-seats");
    const bool useCompactness = HasFlag(args, "--compactness");
    prefs.weights.legacySessionScoreOnly = !useCompactness;
    if (HasFlag(args, "--prefer-morning")) {
        prefs.weights.morningWeight = 45.0;
    }
    if (useCompactness) {
        prefs.weights.compactnessWeight = 0.5;
        prefs.weights.gapPenaltyPerMinute = 0.5;
    }
    const std::string maxResStr = GetArgVal(args, "--max-results");
    if (!maxResStr.empty()) {
        try {
            prefs.maxResults = static_cast<size_t>(std::max(1, std::stoi(maxResStr)));
        } catch (...) {}
    }

    for (const auto& inc : GetAllArgVals(args, "--include")) {
        const auto colon = inc.find(':');
        if (colon == std::string::npos) continue;
        const std::string code = Trim(inc.substr(0, colon));
        std::string rest = inc.substr(colon + 1);
        std::stringstream ss(rest);
        std::string item;
        while (std::getline(ss, item, ',')) {
            item = Trim(item);
            if (!item.empty()) {
                prefs.filters.allowedGroupIdsByCourse[code].push_back(item);
            }
        }
    }

    ScheduleResult res = HorariosSolver::Solve(catalog, busySlots, prefs);
    // Record any empty-group courses explicitly in warnings
    for (const auto& c : catalog.courses) {
        if (c.groups.empty()) {
            res.diagnostics.warningsUtf8.push_back("Course " + c.codeUtf8 + " has no groups");
        }
    }

    const nlohmann::json outJ = BuildCliSolveJson(res);
    const std::string outJsonStr = outJ.dump(2);
    if (!outJson.empty()) {
        if (!WriteStringToFile(fs::u8path(outJson), outJsonStr, &err)) {
            std::cerr << err << "\n";
            return 2;
        }
    } else {
        std::cout << outJsonStr << "\n";
    }

    if (!outCsv.empty()) {
        const std::string csvStr = ExportCsv(res);
        if (!WriteStringToFile(fs::u8path(outCsv), csvStr, &err)) {
            std::cerr << err << "\n";
            return 2;
        }
    }

    if (!res.diagnostics.warningsUtf8.empty()) {
        for (const auto& w : res.diagnostics.warningsUtf8) {
            std::cerr << "WARNING: " << w << "\n";
        }
    }
    if (res.status == SolveStatus::InvalidInput) {
        for (const auto& e : res.diagnostics.errorsUtf8) {
            std::cerr << "ERROR: " << e << "\n";
        }
        return 2;
    }
    return 0;
}

}  // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: horarios_tec_cli <parse|solve> [options]\n";
        return 64;
    }
    const std::string cmd = argv[1];
    std::vector<std::string> args;
    for (int i = 2; i < argc; ++i) args.emplace_back(argv[i]);

    if (cmd == "parse") return CmdParse(args);
    if (cmd == "solve") return CmdSolve(args);

    std::cerr << "Unknown command: " << cmd << "\n";
    return 64;
}
