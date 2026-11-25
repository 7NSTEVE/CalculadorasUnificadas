#pragma once
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0601
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <string>

enum class Fx991CwState {
    NotFound = 0,
    Stopped = 1,
    Launching = 2,
    Ready = 3,
    Failed = 4
};

class Fx991CwHost {
public:
    static bool IsRuntimeAvailable();
    static Fx991CwState GetState();
    static std::wstring GetLastError();
    static HWND GetChildHwnd();

    static bool EnsureLaunched(HWND hParent, const RECT& viewportRect, bool visible);
    static void PollAttach(HWND hParent, const RECT& viewportRect, bool visible);
    static void SyncLayout(HWND hParent, const RECT& viewportRect, bool visible);

    static bool ForwardKey(WPARAM vk, bool down);
    static bool ClickNormalized(double normX, double normY);
    static bool IsChildFocused();

    static HBITMAP CaptureBitmap(bool lcdOnly, int& outW, int& outH);
    static bool Restart(HWND hParent, const RECT& viewportRect, bool visible, bool resetRam);
    static void Shutdown();
};
