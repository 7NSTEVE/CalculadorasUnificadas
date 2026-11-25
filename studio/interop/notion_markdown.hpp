#pragma once

#include <string>
#include <string_view>

namespace DocStudio::NotionMarkdown {

std::string PageIdFromReference(std::string_view reference);

bool DecodePage(std::string_view body, std::string_view expectedPageId,
                std::string& markdown, std::string& error);

bool FetchPage(std::string_view reference, std::wstring_view token,
               std::string& markdown, std::string& error);

} // namespace DocStudio::NotionMarkdown
