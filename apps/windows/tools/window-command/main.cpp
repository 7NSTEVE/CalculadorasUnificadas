#include <windows.h>

#include <cwchar>
#include <iostream>

struct SearchContext {
    DWORD processId = 0;
    UINT commandId = IDOK;
    int matched = 0;
};

BOOL CALLBACK SendCommandToProcessWindow(HWND window, LPARAM parameter)
{
    auto* context = reinterpret_cast<SearchContext*>(parameter);
    DWORD windowProcessId = 0;
    GetWindowThreadProcessId(window, &windowProcessId);
    if (windowProcessId != context->processId || !IsWindowVisible(window)) {
        return TRUE;
    }

    wchar_t className[128]{};
    GetClassNameW(window, className, static_cast<int>(std::size(className)));
    if (std::wcscmp(className, L"#32770") != 0) {
        return TRUE;
    }

    wchar_t title[512]{};
    GetWindowTextW(window, title, static_cast<int>(std::size(title)));
    std::wcout << L"window=0x" << std::hex
               << reinterpret_cast<std::uintptr_t>(window) << std::dec
               << L" class=\"" << className << L"\""
               << L" title=\"" << title << L"\"\n";

    SendMessageW(window, WM_COMMAND, MAKEWPARAM(context->commandId, BN_CLICKED), 0);
    ++context->matched;
    return TRUE;
}

int wmain(int argc, wchar_t** argv)
{
    if (argc < 2 || argc > 3) {
        std::wcerr << L"Usage: WindowCommand <pid> [command-id]\n";
        return 2;
    }

    SearchContext context{};
    context.processId = static_cast<DWORD>(std::wcstoul(argv[1], nullptr, 10));
    if (argc == 3) {
        context.commandId = static_cast<UINT>(std::wcstoul(argv[2], nullptr, 0));
    }

    if (context.processId == 0) {
        std::wcerr << L"Invalid PID\n";
        return 2;
    }

    EnumWindows(SendCommandToProcessWindow, reinterpret_cast<LPARAM>(&context));
    std::wcout << L"matched=" << context.matched << L"\n";
    return context.matched > 0 ? 0 : 1;
}
