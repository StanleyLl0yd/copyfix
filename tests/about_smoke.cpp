// Native Win32 smoke for the approved About C window, centered without a browser.
#define wWinMain CopyFixUnusedWinMainForAboutSmoke
#include "../src/winmain.cpp"
#undef wWinMain

#include <iostream>

int main() {
    const auto fail = [](int code) {
        std::cerr << "ABOUT_UI_FAILURE_" << code << "\n";
        return code;
    };
    g_instance = GetModuleHandleW(nullptr);
    g_canvas_brush = CreateSolidBrush(kCanvas);
    g_surface_brush = CreateSolidBrush(kSurface);
    const WNDCLASSEXW wc{
        sizeof(WNDCLASSEXW), 0, AboutProc, 0, 0, g_instance,
        nullptr, nullptr, g_canvas_brush, nullptr, L"CopyFixAbout", nullptr
    };
    if (!g_canvas_brush || !g_surface_brush || !RegisterClassExW(&wc))
        return fail(1);
    POINT cursor{};
    if (!GetCursorPos(&cursor)) return fail(2);
    const HMONITOR monitor = MonitorFromPoint(cursor, MONITOR_DEFAULTTONEAREST);
    MONITORINFO info{sizeof(info)};
    if (!monitor || !GetMonitorInfoW(monitor, &info)) return fail(3);

    ShowAbout(nullptr);
    const HWND window = g_about_window;
    RECT bounds{};
    if (!window || !GetWindowRect(window, &bounds)) return fail(4);
    const int width = bounds.right - bounds.left;
    const int height = bounds.bottom - bounds.top;
    const int workWidth = info.rcWork.right - info.rcWork.left;
    const int workHeight = info.rcWork.bottom - info.rcWork.top;
    const int expectedX = info.rcWork.left + (workWidth > width ? (workWidth - width) / 2 : 0);
    const int expectedY = info.rcWork.top + (workHeight > height ? (workHeight - height) / 2 : 0);
    if (bounds.left != expectedX || bounds.top != expectedY ||
        width <= 0 || height <= 0) return fail(5);

    wchar_t version[100]{};
    GetWindowTextW(GetDlgItem(window,kAboutVersionLabel),
                   version, static_cast<int>(std::size(version)));
    if (wcscmp(version,kAboutVersionString) != 0 ||
        !GetDlgItem(window,kAboutIcon) ||
        !GetDlgItem(window,kAboutProject) ||
        !GetDlgItem(window,kAboutGitHub) ||
        !GetDlgItem(window,kAboutLicense)) return fail(6);

    SendMessageW(window,WM_CLOSE,0,0);
    if (g_about_window || IsWindow(window)) return fail(7);
    UnregisterClassW(L"CopyFixAbout",g_instance);
    DeleteObject(g_canvas_brush);
    DeleteObject(g_surface_brush);
    std::cout << "PASS About C native window centering, dynamic version, icon, links and closure\n";
    return 0;
}
