// Native Windows positioning smoke for the real About callback, without opening a browser.
#define wWinMain CopyFixUnusedWinMainForAboutSmoke
#include "../src/winmain.cpp"
#undef wWinMain

#include <iostream>

int main() {
    const auto fail = [](int code) {
        std::cerr << "ABOUT_POSITION_FAILURE_" << code << "\n";
        return code;
    };
    POINT cursor{};
    if (!GetCursorPos(&cursor)) return fail(1);
    const HMONITOR monitor = MonitorFromPoint(cursor, MONITOR_DEFAULTTONEAREST);
    MONITORINFO info{sizeof(info)};
    if (!monitor || !GetMonitorInfoW(monitor, &info)) return fail(2);

    HWND window = CreateWindowExW(0, L"STATIC", L"CopyFix About positioning test",
                                WS_OVERLAPPEDWINDOW, 10, 20, 480, 300,
                                nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
    if (!window) return fail(3);
    RECT before{}, after{};
    const bool measured = GetWindowRect(window, &before);
    const HRESULT callback = measured
        ? AboutCallback(window, TDN_CREATED, 0, 0, reinterpret_cast<LONG_PTR>(monitor))
        : E_FAIL;
    const bool positioned = GetWindowRect(window, &after);
    DestroyWindow(window);
    if (!measured || FAILED(callback) || !positioned) return fail(4);

    const int width = before.right - before.left;
    const int height = before.bottom - before.top;
    const int workWidth = info.rcWork.right - info.rcWork.left;
    const int workHeight = info.rcWork.bottom - info.rcWork.top;
    const int expectedX = info.rcWork.left + (workWidth > width ? (workWidth - width) / 2 : 0);
    const int expectedY = info.rcWork.top + (workHeight > height ? (workHeight - height) / 2 : 0);
    if (after.left != expectedX || after.top != expectedY ||
        after.right - after.left != width || after.bottom - after.top != height)
        return fail(5);

    std::cout << "PASS About callback centers the measured window on its chosen monitor\n";
    return 0;
}
