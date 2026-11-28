#include "gemini_notebook_sync_store.hpp"
#include "../../core/utf8_path.hpp"
#include "../../core/ai_provider_vault.hpp"
#include "../../core/inline_url_parser.hpp"
#include "../../core/studio_profile_store.hpp"
#include "../../core/utf_conv.hpp"
#include "../../media/native_video_engine.hpp"

#include <nlohmann/json.hpp>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <bcrypt.h>
#endif

#include <algorithm>
#include <array>
#include <cctype>
#include <chrono>
#include <cstdio>
#include <ctime>
#include <cwctype>
#include <filesystem>
#include <fstream>
#include <map>
#include <regex>
#include <sstream>

namespace DocStudio::GeminiNotebook {

namespace {

using json = nlohmann::json;

std::string BytesToHexLower(const std::uint8_t* bytes, std::size_t len) {
    static constexpr char kHex[] = "0123456789abcdef";
    std::string out;
    out.reserve(len * 2u);
    for (std::size_t i = 0; i < len; ++i) {
        out.push_back(kHex[(bytes[i] >> 4u) & 0x0Fu]);
        out.push_back(kHex[bytes[i] & 0x0Fu]);
    }
    return out;
}

std::wstring LowerExt(const std::wstring& path) {
    std::wstring ext = std::filesystem::path(path).extension().wstring();
    std::transform(ext.begin(), ext.end(), ext.begin(), [](wchar_t c) {
        return static_cast<wchar_t>(std::towlower(c));
    });
    return ext;
}

std::string ToLowerAscii(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (unsigned char c : s) {
        out.push_back(static_cast<char>(std::tolower(c)));
    }
    return out;
}

bool AtomicWriteBytes(const std::filesystem::path& target, const std::uint8_t* data, std::size_t len, std::string* outErr) {
    std::error_code ec;
    std::filesystem::create_directories(target.parent_path(), ec);
    const std::filesystem::path tmp = target.wstring() + L".tmp";
    {
        std::ofstream ofs(tmp, std::ios::binary | std::ios::trunc);
        if (!ofs.is_open()) {
            if (outErr) *outErr = "No se pudo abrir archivo temporal para escritura";
            return false;
        }
        if (len > 0) {
            ofs.write(reinterpret_cast<const char*>(data), static_cast<std::streamsize>(len));
        }
        ofs.flush();
        if (!ofs.good()) {
            if (outErr) *outErr = "Error escribiendo datos temporales";
            return false;
        }
    }
#ifdef _WIN32
    if (!MoveFileExW(tmp.c_str(), target.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        std::filesystem::remove(tmp, ec);
        if (outErr) *outErr = "No se pudo reemplazar archivo de destino de forma atomica";
        return false;
    }
    return true;
#else
    std::filesystem::rename(tmp, target, ec);
    if (ec) {
        std::filesystem::remove(tmp, ec);
        if (outErr) *outErr = ec.message();
        return false;
    }
    return true;
#endif
}

bool AtomicWriteString(const std::filesystem::path& target, const std::string& content, std::string* outErr) {
    return AtomicWriteBytes(target,
        reinterpret_cast<const std::uint8_t*>(content.data()), content.size(), outErr);
}

std::string NormalizePathKey(const std::string& s) {
    std::string out = ToLowerAscii(s);
    std::replace(out.begin(), out.end(), '\\', '/');
    return out;
}

std::string MakeVersionedDisplayName(const std::string& baseDisplay, int versionNum, const std::string& ymd) {
    const std::string tag = "[v" + std::to_string((std::max)(2, versionNum)) + " " + ymd + "] ";
    return tag + baseDisplay;
}

} // namespace

std::string ComputeSha256HexBytes(const std::uint8_t* data, std::size_t len) {
#ifdef _WIN32
    BCRYPT_ALG_HANDLE hAlg = nullptr;
    BCRYPT_HASH_HANDLE hHash = nullptr;
    std::array<std::uint8_t, 32> digest{};
    if (BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_SHA256_ALGORITHM, nullptr, 0) == 0) {
        if (BCryptCreateHash(hAlg, &hHash, nullptr, 0, nullptr, 0, 0) == 0) {
            if (len > 0 && data) {
                BCryptHashData(hHash, const_cast<PUCHAR>(data), static_cast<ULONG>(len), 0);
            }
            BCryptFinishHash(hHash, digest.data(), static_cast<ULONG>(digest.size()), 0);
            BCryptDestroyHash(hHash);
        }
        BCryptCloseAlgorithmProvider(hAlg, 0);
        return BytesToHexLower(digest.data(), digest.size());
    }
#endif
    return {};
}

std::string ComputeSha256HexString(const std::string& utf8) {
    return ComputeSha256HexBytes(reinterpret_cast<const std::uint8_t*>(utf8.data()), utf8.size());
}

bool ComputeSha256HexFile(const std::wstring& filePath, std::string& outHex, std::uint64_t* outSizeBytes) {
    outHex.clear();
    if (outSizeBytes) *outSizeBytes = 0;
#ifdef _WIN32
    std::ifstream ifs(std::filesystem::path(filePath), std::ios::binary);
    if (!ifs.is_open()) return false;

    BCRYPT_ALG_HANDLE hAlg = nullptr;
    BCRYPT_HASH_HANDLE hHash = nullptr;
    if (BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_SHA256_ALGORITHM, nullptr, 0) != 0) return false;
    if (BCryptCreateHash(hAlg, &hHash, nullptr, 0, nullptr, 0, 0) != 0) {
        BCryptCloseAlgorithmProvider(hAlg, 0);
        return false;
    }

    std::vector<std::uint8_t> buf(65536u);
    std::uint64_t total = 0;
    bool ok = true;
    while (ifs.good()) {
        ifs.read(reinterpret_cast<char*>(buf.data()), static_cast<std::streamsize>(buf.size()));
        const std::streamsize got = ifs.gcount();
        if (got > 0) {
            total += static_cast<std::uint64_t>(got);
            if (BCryptHashData(hHash, buf.data(), static_cast<ULONG>(got), 0) != 0) {
                ok = false;
                break;
            }
        }
    }

    std::array<std::uint8_t, 32> digest{};
    if (ok) {
        ok = (BCryptFinishHash(hHash, digest.data(), static_cast<ULONG>(digest.size()), 0) == 0);
    }
    BCryptDestroyHash(hHash);
    BCryptCloseAlgorithmProvider(hAlg, 0);

    if (!ok) return false;
    outHex = BytesToHexLower(digest.data(), digest.size());
    if (outSizeBytes) *outSizeBytes = total;
    return true;
#else
    (void)filePath;
    return false;
#endif
}

std::string CurrentUtcIso8601() {
    const auto now = std::chrono::system_clock::now();
    const std::time_t tt = std::chrono::system_clock::to_time_t(now);
    std::tm gmt{};
#ifdef _WIN32
    gmtime_s(&gmt, &tt);
#else
    gmtime_r(&tt, &gmt);
#endif
    char buf[32]{};
    std::snprintf(buf, sizeof(buf), "%04d-%02d-%02dT%02d:%02d:%02dZ",
        gmt.tm_year + 1900, gmt.tm_mon + 1, gmt.tm_mday,
        gmt.tm_hour, gmt.tm_min, gmt.tm_sec);
    return buf;
}

std::string CurrentUtcDateYmd() {
    const std::string iso = CurrentUtcIso8601();
    return iso.size() >= 10 ? iso.substr(0, 10) : "2026-01-01";
}

std::string BuildWebNotebookUrl(const std::string& resourceNameUtf8) {
    if (resourceNameUtf8.empty()) return "https://notebooklm.google.com/";
    const std::string prefix = "notebooks/";
    std::string id = resourceNameUtf8;
    if (id.rfind(prefix, 0) == 0) {
        id = id.substr(prefix.size());
    }
    const std::size_t slash = id.find('/');
    if (slash != std::string::npos) id = id.substr(0, slash);
    if (id.empty()) return "https://notebooklm.google.com/";
    return "https://notebooklm.google.com/notebook/" + id;
}

std::string SanitizeSecretFromText(const std::string& textUtf8, const GeminiNotebookAuthTokens& tokens) {
    std::string clean = textUtf8;
    auto scrub = [&](const std::string& secret) {
        if (secret.size() < 6) return;
        std::size_t pos = 0;
        while ((pos = clean.find(secret, pos)) != std::string::npos) {
            clean.replace(pos, secret.size(), "[REDACTED]");
            pos += 10;
        }
    };
    scrub(tokens.accessTokenUtf8);
    scrub(tokens.refreshTokenUtf8);
    scrub(tokens.oauthClientSecretUtf8);
    return clean;
}

bool IsSupportedNotebookFileExtension(const std::wstring& extLower) {
    return extLower == L".pdf"  || extLower == L".docx" || extLower == L".pptx" ||
           extLower == L".txt"  || extLower == L".md"   || extLower == L".markdown" ||
           extLower == L".csv"  || extLower == L".epub" ||
           extLower == L".png"  || extLower == L".jpg"  || extLower == L".jpeg" || extLower == L".webp" ||
           extLower == L".mp3"  || extLower == L".wav"  || extLower == L".m4a"  || extLower == L".ogg";
}

std::string DetectSupportedMimeType(const std::wstring& filePath) {
    const std::wstring ext = LowerExt(filePath);
    if (ext == L".pdf")  return "application/pdf";
    if (ext == L".docx") return "application/vnd.openxmlformats-officedocument.wordprocessingml.document";
    if (ext == L".pptx") return "application/vnd.openxmlformats-officedocument.presentationml.presentation";
    if (ext == L".txt")  return "text/plain";
    if (ext == L".md" || ext == L".markdown") return "text/markdown";
    if (ext == L".csv")  return "text/csv";
    if (ext == L".epub") return "application/epub+zip";
    if (ext == L".png")  return "image/png";
    if (ext == L".jpg" || ext == L".jpeg") return "image/jpeg";
    if (ext == L".webp") return "image/webp";
    if (ext == L".mp3")  return "audio/mpeg";
    if (ext == L".wav")  return "audio/wav";
    if (ext == L".m4a")  return "audio/mp4";
    if (ext == L".ogg")  return "audio/ogg";
    return {};
}

std::uint64_t EstimateWordCountFromUtf8(const std::string& textUtf8) noexcept {
    std::uint64_t words = 0;
    bool inWord = false;
    for (unsigned char c : textUtf8) {
        const bool ws = (c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\f' || c == '\v');
        if (ws) {
            inWord = false;
        } else if (!inWord) {
            inWord = true;
            ++words;
        }
    }
    return words;
}

std::uint64_t EstimateWordCountForLocalFile(const std::wstring& filePath, const std::string& mimeTypeUtf8, std::uint64_t sizeBytes) {
    if (sizeBytes == 0) return 0;
    if (mimeTypeUtf8.rfind("text/", 0) == 0) {
        std::ifstream ifs(std::filesystem::path(filePath), std::ios::binary);
        if (ifs.is_open()) {
            std::uint64_t words = 0;
            bool inWord = false;
            std::vector<char> buf(65536u);
            while (ifs.good()) {
                ifs.read(buf.data(), static_cast<std::streamsize>(buf.size()));
                const std::streamsize got = ifs.gcount();
                for (std::streamsize i = 0; i < got; ++i) {
                    const unsigned char c = static_cast<unsigned char>(buf[static_cast<std::size_t>(i)]);
                    const bool ws = (c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\f' || c == '\v');
                    if (ws) inWord = false;
                    else if (!inWord) { inWord = true; ++words; }
                }
            }
            return words;
        }
        return sizeBytes / 6ull;
    }
    if (mimeTypeUtf8.rfind("image/", 0) == 0 || mimeTypeUtf8.rfind("audio/", 0) == 0) {
        return 0; // Media files are bounded primarily by 200 MiB file size
    }
    // Conservative heuristic for compressed binary docs (.pdf / .docx / .pptx / .epub):
    // Typical PDF averages ~25-40 bytes/word after images/streams; cap conservatively without false-blocking normal PDFs.
    return sizeBytes / 32ull;
}

PreFlightReport ValidateSourceForUpload(
    const AcademicMaterialCandidate& candidate,
    const GeminiNotebookConfig& cfg,
    bool computeHash)
{
    PreFlightReport rep{};

    if (candidate.inputKind == SourceInputKind::WebUrl || candidate.inputKind == SourceInputKind::YouTubeUrl) {
        if (!IsSafeExternalLinkUrl(candidate.sourceUrlUtf8)) {
            rep.ok = false;
            rep.issues.push_back({ PreFlightSeverity::Error, "UNSAFE_URL", "URL no permitida (solo se aceptan enlaces http:// o https://)." });
            return rep;
        }
        const std::string low = ToLowerAscii(candidate.sourceUrlUtf8);
        if (low.rfind("mailto:", 0) == 0) {
            rep.ok = false;
            rep.issues.push_back({ PreFlightSeverity::Error, "MAILTO_UNSUPPORTED", "Los enlaces mailto: no pueden agregarse como fuente web." });
            return rep;
        }
        rep.isYouTube = IsYouTubeUrlUtf8(candidate.sourceUrlUtf8);
        rep.detectedMimeTypeUtf8 = rep.isYouTube ? "video/youtube" : "text/html";
        rep.sizeBytes = candidate.sourceUrlUtf8.size();
        rep.sha256Hex = !candidate.precomputedSha256Hex.empty()
            ? candidate.precomputedSha256Hex
            : ComputeSha256HexString("url:" + candidate.sourceUrlUtf8);
        return rep;
    }

    if (candidate.inputKind == SourceInputKind::InlineTextOrMarkdown) {
        rep.detectedMimeTypeUtf8 = candidate.inlineMimeTypeUtf8.empty() ? "text/markdown" : candidate.inlineMimeTypeUtf8;
        rep.sizeBytes = candidate.inlineMarkdownUtf8.size();
        if (rep.sizeBytes == 0) {
            rep.ok = false;
            rep.issues.push_back({ PreFlightSeverity::Error, "EMPTY_TEXT", "El contenido de texto o Markdown esta vacio." });
            return rep;
        }
        if (rep.sizeBytes > cfg.maxUploadBytes) {
            rep.ok = false;
            rep.issues.push_back({ PreFlightSeverity::Error, "EXCEEDS_200_MIB", "El texto supera el limite de 200 MiB por fuente." });
        }
        rep.estimatedWords = EstimateWordCountFromUtf8(candidate.inlineMarkdownUtf8);
        if (rep.estimatedWords > cfg.maxWordsPerSource) {
            rep.ok = false;
            rep.issues.push_back({ PreFlightSeverity::Error, "EXCEEDS_500K_WORDS", "El texto supera el limite oficial de 500,000 palabras por fuente." });
        }
        rep.sha256Hex = !candidate.precomputedSha256Hex.empty()
            ? candidate.precomputedSha256Hex
            : ComputeSha256HexString(candidate.inlineMarkdownUtf8);
        return rep;
    }

    // SourceInputKind::LocalFile
    const std::wstring wPath = Utf8ToWide(candidate.localFilePathUtf8);
    if (wPath.empty()) {
        rep.ok = false;
        rep.issues.push_back({ PreFlightSeverity::Error, "MISSING_PATH", "Ruta de archivo vacia." });
        return rep;
    }

    const std::string mime = DetectSupportedMimeType(wPath);
    if (mime.empty()) {
        rep.ok = false;
        rep.issues.push_back({ PreFlightSeverity::Error, "UNSUPPORTED_FORMAT",
            "Formato de archivo no compatible (admitidos: PDF, DOCX, PPTX, MD, TXT, CSV, EPUB, imagenes y audio)." });
        return rep;
    }
    rep.detectedMimeTypeUtf8 = mime;

    std::error_code ec;
    const std::filesystem::path fsPath(wPath);
    const bool exists = std::filesystem::is_regular_file(fsPath, ec);
    if (!exists) {
        if (candidate.precomputedSizeBytes > 0 && !computeHash) {
            rep.sizeBytes = candidate.precomputedSizeBytes;
        } else {
            rep.ok = false;
            rep.issues.push_back({ PreFlightSeverity::Error, "FILE_NOT_FOUND", "El archivo local no existe o no es accesible." });
            return rep;
        }
    } else {
        rep.sizeBytes = std::filesystem::file_size(fsPath, ec);
    }

    if (rep.sizeBytes == 0) {
        rep.ok = false;
        rep.issues.push_back({ PreFlightSeverity::Error, "EMPTY_FILE", "El archivo tiene 0 bytes y no puede cargarse." });
        return rep;
    }
    if (rep.sizeBytes > cfg.maxUploadBytes) {
        rep.ok = false;
        rep.issues.push_back({ PreFlightSeverity::Error, "EXCEEDS_200_MIB",
            "El archivo excede el limite oficial de 200 MiB por carga." });
    }

    if (exists) {
        rep.estimatedWords = EstimateWordCountForLocalFile(wPath, mime, rep.sizeBytes);
        if (rep.estimatedWords > cfg.maxWordsPerSource) {
            rep.ok = false;
            rep.issues.push_back({ PreFlightSeverity::Error, "EXCEEDS_500K_WORDS",
                "El archivo excede el limite estimado de 500,000 palabras por fuente." });
        }
        if (computeHash) {
            if (!candidate.precomputedSha256Hex.empty()) {
                rep.sha256Hex = candidate.precomputedSha256Hex;
            } else if (!ComputeSha256HexFile(wPath, rep.sha256Hex)) {
                rep.ok = false;
                rep.issues.push_back({ PreFlightSeverity::Error, "HASH_READ_ERROR", "No se pudo leer el archivo para calcular su huella SHA-256." });
            }
        }
    }
    return rep;
}

std::string ExtractChapterLabelUtf8(const std::string& folderPathUtf8, const std::string& fileNameUtf8) {
    const std::string combined = folderPathUtf8 + " / " + fileNameUtf8;
    static const std::regex kPatterns[] = {
        std::regex(R"((?:cap[ií]tulo|cap\.?)\s*([0-9]{1,2}|[ivx]{1,4}))", std::regex::icase),
        std::regex(R"((?:semana|sem\.?)\s*([0-9]{1,2}))", std::regex::icase),
        std::regex(R"((?:unidad|tema|m[oó]dulo|lecci[oó]n)\s*([0-9]{1,2}|[ivx]{1,4}))", std::regex::icase),
        std::regex(R"((?:parcial|examen)\s*([0-9]{1,2}))", std::regex::icase),
        std::regex(R"((?:laboratorio|lab\.?)\s*([0-9]{1,2}))", std::regex::icase)
    };
    static const char* kLabels[] = { "Capitulo ", "Semana ", "Unidad ", "Parcial ", "Laboratorio " };
    for (std::size_t i = 0; i < 5; ++i) {
        std::smatch m;
        if (std::regex_search(combined, m, kPatterns[i]) && m.size() >= 2) {
            return std::string(kLabels[i]) + m[1].str();
        }
    }
    // Fall back to deepest meaningful folder segment
    if (!folderPathUtf8.empty()) {
        std::string norm = folderPathUtf8;
        std::replace(norm.begin(), norm.end(), '\\', '/');
        while (!norm.empty() && norm.back() == '/') norm.pop_back();
        const std::size_t pos = norm.rfind('/');
        const std::string seg = (pos != std::string::npos) ? norm.substr(pos + 1) : norm;
        if (!seg.empty() && seg != "." && seg != "documentos" && seg != "Documentos") {
            return seg;
        }
    }
    return "General";
}

std::string DetectMaterialCategoryUtf8(const std::string& folderPathUtf8, const std::string& fileNameUtf8) {
    const std::string low = ToLowerAscii(folderPathUtf8 + " " + fileNameUtf8);
    if (low.find("lab") != std::string::npos || low.find("experimento") != std::string::npos) return "Laboratorio";
    if (low.find("examen") != std::string::npos || low.find("parcial") != std::string::npos ||
        low.find("quiz") != std::string::npos || low.find("eval") != std::string::npos) return "Evaluacion";
    if (low.find("practica") != std::string::npos || low.find("pr\xC3\xA1""ctica") != std::string::npos ||
        low.find("tarea") != std::string::npos || low.find("ejercicio") != std::string::npos) return "Practica";
    if (low.find("presentacion") != std::string::npos || low.find("clase") != std::string::npos ||
        low.find("teoria") != std::string::npos || low.find("diapositiva") != std::string::npos) return "Teoria";
    return "Notas";
}

std::vector<ClassifiedChapterGroup> ClassifyAcademicMaterials(
    const std::vector<AcademicMaterialCandidate>& candidates,
    std::size_t recommendedCapPerNotebook)
{
    std::map<std::pair<std::string, std::string>, ClassifiedChapterGroup> groups;
    for (const auto& c : candidates) {
        const std::string course = c.tecCourseUtf8.empty() ? "Cuaderno General" : c.tecCourseUtf8;
        const std::string chapter = ExtractChapterLabelUtf8(c.folderPathUtf8, c.displayNameUtf8);
        const auto key = std::make_pair(course, chapter);
        auto& g = groups[key];
        if (g.courseNameUtf8.empty()) {
            g.courseNameUtf8 = course;
            g.chapterLabelUtf8 = chapter;
            g.suggestedNotebookTitleUtf8 = (chapter == "General") ? course : (course + " — " + chapter);
        }
        g.items.push_back(c);
    }

    std::vector<ClassifiedChapterGroup> result;
    result.reserve(groups.size());
    for (auto& kv : groups) {
        kv.second.exceedsRecommendedCap = (kv.second.items.size() > recommendedCapPerNotebook);
        result.push_back(std::move(kv.second));
    }
    return result;
}

ChapterBundleResult BuildChapterMarkdownBundle(
    const std::string& courseNameUtf8,
    const std::string& chapterLabelUtf8,
    const std::vector<AcademicMaterialCandidate>& candidates,
    std::uint64_t maxWords)
{
    ChapterBundleResult res{};
    res.displayNameUtf8 = (courseNameUtf8.empty() ? "Notas" : courseNameUtf8) +
        " — " + (chapterLabelUtf8.empty() ? "Capitulo" : chapterLabelUtf8) + " (Compilado).md";

    std::ostringstream oss;
    oss << "# " << (courseNameUtf8.empty() ? "Material de Estudio" : courseNameUtf8)
        << " — " << (chapterLabelUtf8.empty() ? "General" : chapterLabelUtf8) << "\n\n";
    oss << "_Compilado automaticamente por CuadernU para optimizar el cupo de fuentes._\n\n";

    std::uint64_t totalWords = EstimateWordCountFromUtf8(oss.str());
    for (const auto& c : candidates) {
        std::string body;
        if (c.inputKind == SourceInputKind::InlineTextOrMarkdown) {
            body = c.inlineMarkdownUtf8;
        } else if (c.inputKind == SourceInputKind::LocalFile) {
            const std::wstring wp = Utf8ToWide(c.localFilePathUtf8);
            const std::wstring ext = LowerExt(wp);
            if (ext != L".md" && ext != L".markdown" && ext != L".txt" && ext != L".csv") continue;
            std::ifstream ifs(std::filesystem::path(wp), std::ios::binary);
            if (!ifs.is_open()) continue;
            body.assign(std::istreambuf_iterator<char>(ifs), std::istreambuf_iterator<char>());
        } else {
            continue;
        }
        if (body.empty()) continue;

        const std::uint64_t w = EstimateWordCountFromUtf8(body);
        if (totalWords + w > maxWords && res.bundledFileCount > 0) {
            break; // Respect safety word cap
        }
        oss << "---\n\n## " << (c.displayNameUtf8.empty() ? "Nota" : c.displayNameUtf8) << "\n\n";
        oss << body << "\n\n";
        totalWords += w;
        res.bundledFileCount++;
    }

    if (res.bundledFileCount == 0) {
        res.ok = false;
        res.errorUtf8 = "No se encontraron archivos de texto o Markdown legibles para compilar.";
        return res;
    }

    res.markdownUtf8 = oss.str();
    res.estimatedWords = totalWords;
    res.sha256Hex = ComputeSha256HexString(res.markdownUtf8);
    res.ok = true;
    return res;
}

std::string ExportDocumentToSyncMarkdown(const DocumentModel& doc) {
    std::ostringstream oss;
    const std::string title = doc.Title().empty() ? "Cuaderno CuadernU" : doc.Title();
    oss << "# " << title << "\n\n";

    auto emitObjects = [&](const std::vector<AnnotationObject>& objs) {
        for (const auto& o : objs) {
            if (o.kind == ObjectKind::TextBox && !o.textUtf8.empty()) {
                oss << o.textUtf8 << "\n\n";
            } else if (o.kind == ObjectKind::MathBlock) {
                oss << "**Formula**: `" << o.formulaExprUtf8 << "`";
                if (!o.evaluatedResultUtf8.empty()) oss << "  =>  `" << o.evaluatedResultUtf8 << "`";
                oss << "\n\n";
            } else if (o.kind == ObjectKind::CodeBlock && !o.textUtf8.empty()) {
                oss << "```" << o.codeLanguageUtf8 << "\n" << o.textUtf8 << "\n```\n\n";
            } else if (o.kind == ObjectKind::LinkCard && !o.linkUrlUtf8.empty()) {
                const std::string lbl = o.linkTitleUtf8.empty() ? o.linkUrlUtf8 : o.linkTitleUtf8;
                oss << "- [" << lbl << "](" << o.linkUrlUtf8 << ")";
                if (!o.linkDescriptionUtf8.empty()) oss << " — " << o.linkDescriptionUtf8;
                oss << "\n";
            } else if (o.kind == ObjectKind::AudioClip && !o.captionUtf8.empty()) {
                oss << "> Nota de audio: " << o.captionUtf8 << "\n\n";
            } else if (o.kind == ObjectKind::CalcSessionCard &&
                       (!o.calcLatexUtf8.empty() || !o.calcKeySeqUtf8.empty())) {
                oss << "- Cálculo (" << o.calcModelIdUtf8 << "): "
                    << (o.calcLatexUtf8.empty() ? o.calcKeySeqUtf8 : o.calcLatexUtf8) << "\n";
            }
        }
    };

    if (doc.Mode() == DocumentMode::InfiniteCanvas) {
        oss << "## Lienzo Infinito\n\n";
        emitObjects(doc.CanvasObjects());
    } else {
        for (std::size_t i = 0; i < doc.Pages().size(); ++i) {
            oss << "## Pagina " << (i + 1) << "\n\n";
            emitObjects(doc.Pages()[i].objects);
        }
    }
    return oss.str();
}

// ---------------- UploadRateLimiter ----------------

UploadRateLimiter::UploadRateLimiter(std::size_t maxRequestsPerMinute, ClockMsFn clockMs)
    : maxPerMin_((std::max<std::size_t>)(1u, maxRequestsPerMinute)),
      clockMs_(std::move(clockMs)) {}

std::uint64_t UploadRateLimiter::NowMs() const {
    if (clockMs_) return clockMs_();
    return static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count());
}

void UploadRateLimiter::SetMaxRequestsPerMinute(std::size_t maxPerMin) {
    std::lock_guard<std::mutex> lk(mtx_);
    maxPerMin_ = (std::max<std::size_t>)(1u, maxPerMin);
}

std::size_t UploadRateLimiter::MaxRequestsPerMinute() const noexcept {
    std::lock_guard<std::mutex> lk(mtx_);
    return maxPerMin_;
}

std::uint64_t UploadRateLimiter::TryAcquireOrWaitMs() {
    std::lock_guard<std::mutex> lk(mtx_);
    const std::uint64_t now = NowMs();
    constexpr std::uint64_t kWindowMs = 60000ull;

    if (retryBlockedUntilMs_ > now) {
        return retryBlockedUntilMs_ - now;
    }
    while (!timestampsMs_.empty() && now >= timestampsMs_.front() + kWindowMs) {
        timestampsMs_.pop_front();
    }
    if (timestampsMs_.size() < maxPerMin_) {
        timestampsMs_.push_back(now);
        return 0ull;
    }
    const std::uint64_t oldestExpire = timestampsMs_.front() + kWindowMs;
    return (oldestExpire > now) ? (oldestExpire - now) : 1ull;
}

void UploadRateLimiter::NoteRetryAfterMs(std::uint64_t cooldownMs) {
    std::lock_guard<std::mutex> lk(mtx_);
    const std::uint64_t until = NowMs() + cooldownMs;
    if (until > retryBlockedUntilMs_) retryBlockedUntilMs_ = until;
}

std::size_t UploadRateLimiter::UsedInCurrentWindow() const {
    std::lock_guard<std::mutex> lk(mtx_);
    const std::uint64_t now = NowMs();
    std::size_t count = 0;
    for (std::uint64_t ts : timestampsMs_) {
        if (now < ts + 60000ull) ++count;
    }
    return count;
}

// ---------------- GeminiNotebookSyncStore ----------------

std::wstring GeminiNotebookSyncStore::DefaultStorageRootDir() {
    return (std::filesystem::path(StudioProfileStore::DefaultProfileRootDir()) / L"gemini_notebook").wstring();
}

GeminiNotebookSyncStore::GeminiNotebookSyncStore(std::wstring storageRootDir)
    : rootDir_(storageRootDir.empty() ? DefaultStorageRootDir() : std::move(storageRootDir)) {}

bool GeminiNotebookSyncStore::LoadAll(std::string* outErr) {
    std::error_code ec;
    const std::filesystem::path root(rootDir_);
    std::filesystem::create_directories(root, ec);

    // 1. Load config.json (non-secret settings only)
    {
        const std::filesystem::path cfgPath = root / L"config.json";
        if (std::filesystem::exists(cfgPath, ec)) {
            std::ifstream ifs(cfgPath);
            if (ifs.is_open()) {
                try {
                    const json j = json::parse(ifs);
                    config_.enabled = j.value("enabled", config_.enabled);
                    config_.apiBaseUrlUtf8 = j.value("api_base_url", config_.apiBaseUrlUtf8);
                    config_.uploadBaseUrlUtf8 = j.value("upload_base_url", config_.uploadBaseUrlUtf8);
                    config_.mcpEndpointUrlUtf8 = j.value("mcp_endpoint_url", config_.mcpEndpointUrlUtf8);
                    config_.projectIdUtf8 = j.value("project_id", config_.projectIdUtf8);
                    config_.oauthClientIdUtf8 = j.value("oauth_client_id", config_.oauthClientIdUtf8);
                    config_.bundleSmallNotesByChapter = j.value("bundle_small_notes", config_.bundleSmallNotesByChapter);
                    config_.maxUploadBytes = j.value("max_upload_bytes", config_.maxUploadBytes);
                    config_.maxWordsPerSource = j.value("max_words_per_source", config_.maxWordsPerSource);
                    config_.maxUploadsPerMinute = j.value("max_uploads_per_minute", config_.maxUploadsPerMinute);
                    config_.recommendedMaxSourcesPerNotebook = j.value("recommended_max_sources", config_.recommendedMaxSourcesPerNotebook);
                } catch (...) {
                    if (outErr) *outErr = "Configuracion local de Gemini Notebook corrupta; se usaron valores por defecto.";
                }
            }
        }
    }

    // 2. Load sync_manifest.json
    notebooks_.clear();
    sources_.clear();
    history_.clear();
    {
        const std::filesystem::path manPath = root / L"sync_manifest.json";
        if (std::filesystem::exists(manPath, ec)) {
            std::ifstream ifs(manPath);
            if (ifs.is_open()) {
                try {
                    const json j = json::parse(ifs);
                    if (j.contains("notebooks") && j["notebooks"].is_array()) {
                        for (const auto& nb : j["notebooks"]) {
                            LocalNotebookMapping m{};
                            m.localKeyUtf8 = nb.value("local_key", "");
                            m.displayNameUtf8 = nb.value("display_name", "");
                            m.remoteNotebookNameUtf8 = nb.value("remote_name", "");
                            m.webUrlUtf8 = nb.value("web_url", "");
                            m.tecCourseUtf8 = nb.value("tec_course", "");
                            m.lastSyncUtc = nb.value("last_sync_utc", "");
                            if (!m.localKeyUtf8.empty()) notebooks_.push_back(std::move(m));
                        }
                    }
                    if (j.contains("sources") && j["sources"].is_array()) {
                        for (const auto& s : j["sources"]) {
                            LocalSyncSourceEntry e{};
                            e.localIdUtf8 = s.value("local_id", "");
                            e.notebookKeyUtf8 = s.value("notebook_key", "");
                            e.remoteNotebookNameUtf8 = s.value("remote_notebook", "");
                            e.remoteSourceNameUtf8 = s.value("remote_source", "");
                            e.displayNameUtf8 = s.value("display_name", "");
                            e.inputKind = static_cast<SourceInputKind>(s.value("input_kind", 0));
                            e.localFilePathUtf8 = s.value("local_path", "");
                            e.sourceUrlUtf8 = s.value("source_url", "");
                            e.mimeTypeUtf8 = s.value("mime_type", "");
                            e.sha256Hex = s.value("sha256", "");
                            e.sizeBytes = s.value("size_bytes", 0ull);
                            e.estimatedWords = s.value("estimated_words", 0ull);
                            e.tecCourseUtf8 = s.value("tec_course", "");
                            e.chapterLabelUtf8 = s.value("chapter_label", "");
                            e.materialCategoryUtf8 = s.value("category", "");
                            e.state = SyncSourceStateFromString(s.value("state", "local_only"));
                            e.versionNumber = s.value("version", 1);
                            e.syncedAtUtc = s.value("synced_at_utc", "");
                            e.lastErrorUtf8 = s.value("last_error", "");
                            sources_.push_back(std::move(e));
                        }
                    }
                    if (j.contains("history") && j["history"].is_array()) {
                        for (const auto& h : j["history"]) {
                            SyncHistoryRecord r{};
                            r.timestampUtc = h.value("timestamp_utc", "");
                            r.notebookDisplayNameUtf8 = h.value("notebook", "");
                            r.uploadedCount = h.value("uploaded", 0u);
                            r.skippedUnchangedCount = h.value("skipped", 0u);
                            r.versionedUpdateCount = h.value("versioned", 0u);
                            r.failedCount = h.value("failed", 0u);
                            r.summaryUtf8 = h.value("summary", "");
                            history_.push_back(std::move(r));
                        }
                    }
                } catch (...) {
                    if (outErr) *outErr = "Historial local de sincronizacion corrupto.";
                }
            }
        }
    }

    // 3. Load encrypted DPAPI tokens if present (best-effort)
    LoadTokensDpapi(nullptr);
    return true;
}

bool GeminiNotebookSyncStore::SaveConfig(std::string* outErr) const {
    json j = {
        { "enabled", config_.enabled },
        { "api_base_url", config_.apiBaseUrlUtf8 },
        { "upload_base_url", config_.uploadBaseUrlUtf8 },
        { "mcp_endpoint_url", config_.mcpEndpointUrlUtf8 },
        { "project_id", config_.projectIdUtf8 },
        { "oauth_client_id", config_.oauthClientIdUtf8 },
        { "bundle_small_notes", config_.bundleSmallNotesByChapter },
        { "max_upload_bytes", config_.maxUploadBytes },
        { "max_words_per_source", config_.maxWordsPerSource },
        { "max_uploads_per_minute", config_.maxUploadsPerMinute },
        { "recommended_max_sources", config_.recommendedMaxSourcesPerNotebook }
    };
    return AtomicWriteString(std::filesystem::path(rootDir_) / L"config.json", j.dump(2), outErr);
}

bool GeminiNotebookSyncStore::SaveManifest(std::string* outErr) const {
    json jNb = json::array();
    for (const auto& nb : notebooks_) {
        jNb.push_back({
            { "local_key", nb.localKeyUtf8 },
            { "display_name", nb.displayNameUtf8 },
            { "remote_name", nb.remoteNotebookNameUtf8 },
            { "web_url", nb.webUrlUtf8 },
            { "tec_course", nb.tecCourseUtf8 },
            { "last_sync_utc", nb.lastSyncUtc }
        });
    }
    json jSrc = json::array();
    for (const auto& s : sources_) {
        jSrc.push_back({
            { "local_id", s.localIdUtf8 },
            { "notebook_key", s.notebookKeyUtf8 },
            { "remote_notebook", s.remoteNotebookNameUtf8 },
            { "remote_source", s.remoteSourceNameUtf8 },
            { "display_name", s.displayNameUtf8 },
            { "input_kind", static_cast<int>(s.inputKind) },
            { "local_path", s.localFilePathUtf8 },
            { "source_url", s.sourceUrlUtf8 },
            { "mime_type", s.mimeTypeUtf8 },
            { "sha256", s.sha256Hex },
            { "size_bytes", s.sizeBytes },
            { "estimated_words", s.estimatedWords },
            { "tec_course", s.tecCourseUtf8 },
            { "chapter_label", s.chapterLabelUtf8 },
            { "category", s.materialCategoryUtf8 },
            { "state", SyncSourceStateToString(s.state) },
            { "version", s.versionNumber },
            { "synced_at_utc", s.syncedAtUtc },
            { "last_error", s.lastErrorUtf8 }
        });
    }
    json jHist = json::array();
    for (const auto& h : history_) {
        jHist.push_back({
            { "timestamp_utc", h.timestampUtc },
            { "notebook", h.notebookDisplayNameUtf8 },
            { "uploaded", h.uploadedCount },
            { "skipped", h.skippedUnchangedCount },
            { "versioned", h.versionedUpdateCount },
            { "failed", h.failedCount },
            { "summary", h.summaryUtf8 }
        });
    }
    json root = {
        { "schema_version", 1 },
        { "updated_at_utc", CurrentUtcIso8601() },
        { "notebooks", std::move(jNb) },
        { "sources", std::move(jSrc) },
        { "history", std::move(jHist) }
    };
    return AtomicWriteString(std::filesystem::path(rootDir_) / L"sync_manifest.json", root.dump(2), outErr);
}

bool GeminiNotebookSyncStore::SaveTokensDpapi(std::string* outErr) const {
    const json j = {
        { "access_token", tokens_.accessTokenUtf8 },
        { "refresh_token", tokens_.refreshTokenUtf8 },
        { "client_secret", tokens_.oauthClientSecretUtf8.empty() ? config_.oauthClientSecretUtf8 : tokens_.oauthClientSecretUtf8 },
        { "expires_at", tokens_.expiresAtEpochSec },
        { "account_email", tokens_.accountEmailUtf8 },
        { "workspace_rejected", tokens_.workspaceRejectionSeen }
    };
    const std::string plainStr = j.dump();
    const std::vector<std::uint8_t> plainBytes(plainStr.begin(), plainStr.end());
    std::vector<std::uint8_t> encBytes;
    if (!AiProviderVault::ProtectBytesDpapi(plainBytes, encBytes, outErr)) {
        return false;
    }
    return AtomicWriteBytes(std::filesystem::path(rootDir_) / L"oauth_tokens.dpapi",
        encBytes.data(), encBytes.size(), outErr);
}

bool GeminiNotebookSyncStore::LoadTokensDpapi(std::string* outErr) {
    const std::filesystem::path tokPath = std::filesystem::path(rootDir_) / L"oauth_tokens.dpapi";
    std::error_code ec;
    if (!std::filesystem::exists(tokPath, ec)) return false;
    std::ifstream ifs(tokPath, std::ios::binary);
    if (!ifs.is_open()) return false;
    std::vector<std::uint8_t> encBytes((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
    if (encBytes.empty()) return false;

    std::vector<std::uint8_t> plainBytes;
    if (!AiProviderVault::UnprotectBytesDpapi(encBytes, plainBytes, outErr)) {
        return false;
    }
    try {
        const json j = json::parse(plainBytes.begin(), plainBytes.end());
        tokens_.accessTokenUtf8 = j.value("access_token", "");
        tokens_.refreshTokenUtf8 = j.value("refresh_token", "");
        tokens_.oauthClientSecretUtf8 = j.value("client_secret", "");
        if (!tokens_.oauthClientSecretUtf8.empty()) {
            config_.oauthClientSecretUtf8 = tokens_.oauthClientSecretUtf8;
        }
        tokens_.expiresAtEpochSec = j.value("expires_at", 0ll);
        tokens_.accountEmailUtf8 = j.value("account_email", "");
        tokens_.workspaceRejectionSeen = j.value("workspace_rejected", false);
        return true;
    } catch (...) {
        if (outErr) *outErr = "No se pudo interpretar el contenedor cifrado de credenciales.";
        return false;
    }
}

bool GeminiNotebookSyncStore::ClearTokens(std::string* outErr) {
    tokens_ = GeminiNotebookAuthTokens{};
    config_.oauthClientSecretUtf8.clear();
    std::error_code ec;
    std::filesystem::remove(std::filesystem::path(rootDir_) / L"oauth_tokens.dpapi", ec);
    if (ec && outErr) *outErr = ec.message();
    return !ec;
}

const LocalNotebookMapping* GeminiNotebookSyncStore::FindNotebookByLocalKey(const std::string& localKeyUtf8) const {
    for (const auto& nb : notebooks_) {
        if (nb.localKeyUtf8 == localKeyUtf8) return &nb;
    }
    return nullptr;
}

const LocalNotebookMapping* GeminiNotebookSyncStore::FindNotebookByRemoteName(const std::string& remoteNameUtf8) const {
    for (const auto& nb : notebooks_) {
        if (nb.remoteNotebookNameUtf8 == remoteNameUtf8) return &nb;
    }
    return nullptr;
}

void GeminiNotebookSyncStore::UpsertNotebookMapping(const LocalNotebookMapping& mapping) {
    for (auto& nb : notebooks_) {
        if (nb.localKeyUtf8 == mapping.localKeyUtf8 ||
            (!mapping.remoteNotebookNameUtf8.empty() && nb.remoteNotebookNameUtf8 == mapping.remoteNotebookNameUtf8)) {
            nb = mapping;
            return;
        }
    }
    notebooks_.push_back(mapping);
}

const LocalSyncSourceEntry* GeminiNotebookSyncStore::FindSourceBySha256(
    const std::string& notebookKeyUtf8,
    const std::string& sha256Hex) const
{
    if (sha256Hex.empty()) return nullptr;
    for (const auto& s : sources_) {
        if (s.notebookKeyUtf8 == notebookKeyUtf8 && s.sha256Hex == sha256Hex &&
            (s.state == SyncSourceState::ActiveSynced || s.state == SyncSourceState::ProcessingRemote)) {
            return &s;
        }
    }
    return nullptr;
}

const LocalSyncSourceEntry* GeminiNotebookSyncStore::FindLatestSourceByOrigin(
    const std::string& notebookKeyUtf8,
    const std::string& localPathOrUrlUtf8) const
{
    if (localPathOrUrlUtf8.empty()) return nullptr;
    const std::string normTarget = NormalizePathKey(localPathOrUrlUtf8);
    const LocalSyncSourceEntry* best = nullptr;
    for (const auto& s : sources_) {
        if (s.notebookKeyUtf8 != notebookKeyUtf8) continue;
        const std::string origin = !s.localFilePathUtf8.empty() ? s.localFilePathUtf8 : s.sourceUrlUtf8;
        if (NormalizePathKey(origin) == normTarget) {
            if (!best || s.versionNumber >= best->versionNumber) {
                best = &s;
            }
        }
    }
    return best;
}

std::vector<LocalSyncSourceEntry> GeminiNotebookSyncStore::SourcesForNotebook(const std::string& notebookKeyUtf8) const {
    std::vector<LocalSyncSourceEntry> out;
    for (const auto& s : sources_) {
        if (s.notebookKeyUtf8 == notebookKeyUtf8) out.push_back(s);
    }
    return out;
}

void GeminiNotebookSyncStore::UpsertSourceEntry(const LocalSyncSourceEntry& entry) {
    for (auto& s : sources_) {
        if ((!entry.localIdUtf8.empty() && s.localIdUtf8 == entry.localIdUtf8) ||
            (!entry.remoteSourceNameUtf8.empty() && s.remoteSourceNameUtf8 == entry.remoteSourceNameUtf8)) {
            s = entry;
            return;
        }
    }
    sources_.push_back(entry);
}

void GeminiNotebookSyncStore::MarkPriorSourcesSuperseded(
    const std::string& notebookKeyUtf8,
    const std::string& localPathOrUrlUtf8,
    const std::string& exceptSha256Hex)
{
    const std::string normTarget = NormalizePathKey(localPathOrUrlUtf8);
    for (auto& s : sources_) {
        if (s.notebookKeyUtf8 != notebookKeyUtf8) continue;
        const std::string origin = !s.localFilePathUtf8.empty() ? s.localFilePathUtf8 : s.sourceUrlUtf8;
        if (NormalizePathKey(origin) == normTarget && s.sha256Hex != exceptSha256Hex) {
            if (s.state == SyncSourceState::ActiveSynced || s.state == SyncSourceState::ProcessingRemote) {
                s.state = SyncSourceState::SupersededLocal;
            }
        }
    }
}

void GeminiNotebookSyncStore::AppendHistory(const SyncHistoryRecord& rec) {
    history_.push_back(rec);
    if (history_.size() > 100u) {
        history_.erase(history_.begin(), history_.begin() + static_cast<std::ptrdiff_t>(history_.size() - 100u));
    }
}

SyncDiffPlan GeminiNotebookSyncStore::ComputeIncrementalSyncPlan(
    const std::string& targetLocalNotebookKeyUtf8,
    const std::string& targetNotebookDisplayNameUtf8,
    const std::vector<AcademicMaterialCandidate>& candidates,
    const std::string& dateTagYmdOverride) const
{
    SyncDiffPlan plan{};
    plan.targetLocalNotebookKeyUtf8 = targetLocalNotebookKeyUtf8;
    plan.targetNotebookDisplayNameUtf8 = targetNotebookDisplayNameUtf8;

    if (const auto* nb = FindNotebookByLocalKey(targetLocalNotebookKeyUtf8)) {
        plan.existingRemoteNotebookNameUtf8 = nb->remoteNotebookNameUtf8;
    }

    for (const auto& s : sources_) {
        if (s.notebookKeyUtf8 == targetLocalNotebookKeyUtf8 &&
            (s.state == SyncSourceState::ActiveSynced || s.state == SyncSourceState::ProcessingRemote || s.state == SyncSourceState::SupersededLocal)) {
            plan.existingRemoteSourceCount++;
        }
    }

    const std::string ymd = dateTagYmdOverride.empty() ? CurrentUtcDateYmd() : dateTagYmdOverride;
    std::map<std::string, bool> seenHashInBatch;

    for (const auto& c : candidates) {
        SyncDiffItem item{};
        item.candidate = c;
        item.preflight = ValidateSourceForUpload(c, config_, true);
        item.targetDisplayNameUtf8 = c.displayNameUtf8.empty()
            ? (c.inputKind == SourceInputKind::LocalFile
                ? PathToUtf8(std::filesystem::path(Utf8ToWide(c.localFilePathUtf8)).filename())
                : (!c.sourceUrlUtf8.empty() ? ExtractUrlDomainUtf8(c.sourceUrlUtf8) : "Fuente CuadernU"))
            : c.displayNameUtf8;

        if (!item.preflight.ok) {
            item.action = SyncDiffAction::BlockedByPreFlight;
            item.reasonUtf8 = item.preflight.issues.empty()
                ? "Bloqueado por validacion previa."
                : item.preflight.issues.front().messageUtf8;
            plan.countBlocked++;
            plan.items.push_back(std::move(item));
            continue;
        }

        // Deduplicate within the same batch AND against already synced SHA-256
        if (!item.preflight.sha256Hex.empty() &&
            (seenHashInBatch[item.preflight.sha256Hex] ||
             FindSourceBySha256(targetLocalNotebookKeyUtf8, item.preflight.sha256Hex) != nullptr)) {
            item.action = SyncDiffAction::SkipUnchanged;
            item.reasonUtf8 = "Sin cambios (misma huella SHA-256 ya sincronizada).";
            plan.countUnchanged++;
            plan.items.push_back(std::move(item));
            continue;
        }
        seenHashInBatch[item.preflight.sha256Hex] = true;

        const std::string origin = !c.localFilePathUtf8.empty() ? c.localFilePathUtf8 : c.sourceUrlUtf8;
        if (const auto* prior = FindLatestSourceByOrigin(targetLocalNotebookKeyUtf8, origin)) {
            if (prior->sha256Hex != item.preflight.sha256Hex) {
                item.action = SyncDiffAction::UploadVersionedCopy;
                item.nextVersionNumber = prior->versionNumber + 1;
                item.priorRemoteSourceNameUtf8 = prior->remoteSourceNameUtf8;
                item.targetDisplayNameUtf8 = MakeVersionedDisplayName(item.targetDisplayNameUtf8, item.nextVersionNumber, ymd);
                item.reasonUtf8 = "Archivo modificado localmente; se subira como nueva version sin eliminar el anterior.";
                plan.countVersioned++;
                plan.items.push_back(std::move(item));
                continue;
            }
        }

        item.action = SyncDiffAction::CreateNew;
        item.nextVersionNumber = 1;
        item.reasonUtf8 = "Fuente nueva lista para cargar.";
        plan.countNew++;
        plan.items.push_back(std::move(item));
    }

    const std::size_t projectedTotal = plan.existingRemoteSourceCount + plan.countNew + plan.countVersioned;
    plan.quotaWarningExceeds50 = (projectedTotal > config_.recommendedMaxSourcesPerNotebook);
    return plan;
}

} // namespace DocStudio::GeminiNotebook
