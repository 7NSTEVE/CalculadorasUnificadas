#include "notion_markdown.hpp"

#include <algorithm>
#include <cctype>
#include <memory>

#include <nlohmann/json.hpp>

#ifdef _WIN32
#include <windows.h>
#include <winhttp.h>
#endif

namespace DocStudio::NotionMarkdown {

std::string PageIdFromReference(std::string_view reference) {
    const auto first = reference.find_first_not_of(" \t\r\n");
    if (first == std::string_view::npos) return {};
    const auto last = reference.find_last_not_of(" \t\r\n");
    reference = reference.substr(first, last - first + 1);
    const bool link = reference.rfind("https://", 0) == 0;
    if (link) {
        reference = reference.substr(0, reference.find_first_of("?#"));
        while (!reference.empty() && reference.back() == '/') reference.remove_suffix(1);
        const auto slash = reference.rfind('/');
        if (slash == std::string_view::npos || slash < 8) return {};
        reference.remove_prefix(slash + 1);
    }

    std::string_view candidate = reference;
    if (link && candidate.size() > 36 && candidate[candidate.size() - 37] == '-' &&
        candidate[candidate.size() - 28] == '-') {
        candidate = candidate.substr(candidate.size() - 36);
    } else if (link && candidate.size() > 32 && candidate[candidate.size() - 33] == '-') {
        candidate = candidate.substr(candidate.size() - 32);
    }
    if (candidate.size() != 32 && candidate.size() != 36) return {};

    std::string id;
    id.reserve(36);
    const bool withHyphens = candidate.size() == 36;
    for (std::size_t i = 0; i < candidate.size(); ++i) {
        const unsigned char ch = candidate[i];
        if (withHyphens && (i == 8 || i == 13 || i == 18 || i == 23)) {
            if (ch != '-') return {};
        } else {
            if (!std::isxdigit(ch)) return {};
            const auto count = id.size();
            if (count == 8 || count == 13 || count == 18 || count == 23) id.push_back('-');
            id.push_back(static_cast<char>(std::tolower(ch)));
        }
    }
    return id;
}

bool DecodePage(std::string_view body, std::string_view expectedPageId,
                std::string& markdown, std::string& error) {
    markdown.clear();
    error.clear();
    const auto page = nlohmann::json::parse(body.begin(), body.end(), nullptr, false);
    if (!page.is_object() || !page.contains("object") || page["object"] != "page_markdown" ||
        !page.contains("id") || !page["id"].is_string() ||
        PageIdFromReference(page["id"].get_ref<const std::string&>()) != expectedPageId ||
        !page.contains("markdown") || !page["markdown"].is_string() ||
        (page.contains("truncated") && !page["truncated"].is_boolean()) ||
        (page.contains("unknown_block_ids") && !page["unknown_block_ids"].is_array())) {
        error = "Notion devolvió una respuesta inesperada para la página solicitada.";
        return false;
    }
    if ((page.contains("truncated") && page["truncated"] == true) ||
        (page.contains("unknown_block_ids") && !page["unknown_block_ids"].empty())) {
        error = "Notion indica que la página está incompleta. Use una exportación de origen o revise los permisos.";
        return false;
    }
    markdown = page["markdown"].get_ref<const std::string&>();
    return true;
}

bool FetchPage(std::string_view reference, std::wstring_view token,
               std::string& markdown, std::string& error) {
    markdown.clear();
    error.clear();
    const std::string id = PageIdFromReference(reference);
    if (id.empty()) {
        error = "Introduzca un enlace de página de Notion o su identificador.";
        return false;
    }
    if (token.empty() || token.size() > 4096 || !std::all_of(token.begin(), token.end(), [](wchar_t ch) {
            return (ch >= L'a' && ch <= L'z') || (ch >= L'A' && ch <= L'Z') ||
                (ch >= L'0' && ch <= L'9') || ch == L'_' || ch == L'-' || ch == L'.';
        })) {
        error = "La clave local de la conexión de Notion está vacía o tiene un formato inválido.";
        return false;
    }
#ifdef _WIN32
    using InternetHandle = std::unique_ptr<void, decltype(&WinHttpCloseHandle)>;
    const auto fail = [&](const char* message) {
        error = message;
        return false;
    };
    InternetHandle session(WinHttpOpen(L"CuadernU", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
        WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0), WinHttpCloseHandle);
    if (!session || !WinHttpSetTimeouts(session.get(), 4000, 4000, 8000, 12000)) {
        return fail("No se pudo iniciar la conexión a Notion.");
    }
    InternetHandle connection(WinHttpConnect(session.get(), L"api.notion.com",
        INTERNET_DEFAULT_HTTPS_PORT, 0), WinHttpCloseHandle);
    if (!connection) return fail("No se pudo conectar con Notion.");

    const std::wstring resource = L"/v1/pages/" + std::wstring(id.begin(), id.end()) + L"/markdown";
    InternetHandle request(WinHttpOpenRequest(connection.get(), L"GET", resource.c_str(),
        nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE), WinHttpCloseHandle);
    if (!request) return fail("No se pudo preparar la lectura de Notion.");
    DWORD disable = WINHTTP_DISABLE_REDIRECTS | WINHTTP_DISABLE_COOKIES;
    if (!WinHttpSetOption(request.get(), WINHTTP_OPTION_DISABLE_FEATURE, &disable, sizeof(disable))) {
        return fail("No se pudo configurar la conexión segura con Notion.");
    }
    std::wstring headers = L"Accept: application/json\r\nNotion-Version: 2026-03-11\r\nAuthorization: Bearer ";
    headers.append(token);
    headers += L"\r\n";
    const BOOL sent = WinHttpSendRequest(request.get(), headers.c_str(), static_cast<DWORD>(headers.size()),
        WINHTTP_NO_REQUEST_DATA, 0, 0, 0);
    SecureZeroMemory(headers.data(), headers.size() * sizeof(wchar_t));
    if (!sent || !WinHttpReceiveResponse(request.get(), nullptr)) {
        return fail("Notion no respondió. Revise la red e intente de nuevo.");
    }

    DWORD status = 0;
    DWORD length = sizeof(status);
    if (!WinHttpQueryHeaders(request.get(), WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
            WINHTTP_HEADER_NAME_BY_INDEX, &status, &length, WINHTTP_NO_HEADER_INDEX)) {
        return fail("No se pudo leer el resultado de Notion.");
    }
    if (status == 401) return fail("Notion rechazó la clave de la conexión.");
    if (status == 403 || status == 404) return fail("La página no existe o la conexión no tiene acceso a ella.");
    if (status == 429) return fail("Notion alcanzó su cuota temporal. Intente más tarde.");
    if (status != 200) {
        error = "Notion respondió con HTTP " + std::to_string(status) + ".";
        return false;
    }
    constexpr std::size_t maxResponseBytes = 16u * 1024u * 1024u;
    std::string response;
    char buffer[8192];
    for (;;) {
        DWORD bytes = 0;
        if (!WinHttpReadData(request.get(), buffer, sizeof(buffer), &bytes)) {
            return fail("La descarga de la página de Notion se interrumpió.");
        }
        if (bytes == 0) break;
        if (bytes > maxResponseBytes - response.size()) {
            return fail("La página de Notion supera el tamaño admitido en una sola importación.");
        }
        response.append(buffer, bytes);
    }
    return DecodePage(response, id, markdown, error);
#else
    error = "La conexión a Notion está disponible en CuadernU para Windows.";
    return false;
#endif
}

} // namespace DocStudio::NotionMarkdown
