// Win32-only integration regression. Compile the real PreviewProc into a
// standalone test executable; it is NOT packaged with CopyFix.exe.
#define wWinMain CopyFixUnusedWinMainForSmokeTest
#include "../src/winmain.cpp"
#undef wWinMain

#include <iostream>

int main() {
    g_instance = GetModuleHandleW(nullptr);
    g_richedit_module = LoadLibraryW(L"Msftedit.dll");
    if (!g_richedit_module) return 9;
    g_canvas_brush = CreateSolidBrush(kCanvas);
    g_surface_brush = CreateSolidBrush(kSurface);
    g_font_body = MakeFont(11, FW_NORMAL, 96);
    g_font_title = MakeFont(22, FW_SEMIBOLD, 96);
    g_font_label = MakeFont(10, FW_SEMIBOLD, 96);
    g_font_caption = MakeFont(10, FW_NORMAL, 96);
    g_pending.before.text = L"First paragraph\r\n\r\nSecond paragraph\r\n";
    for (int i=0; i<200; ++i) {
        g_pending.before.text += L"More preview text across multiple lines.\r\n";
    }
    g_pending.after.text = g_pending.before.text;
    const WNDCLASSEXW klass{sizeof(WNDCLASSEXW), 0, PreviewProc, 0, 0, g_instance,
        nullptr, nullptr, g_canvas_brush, nullptr, L"CopyFixPreview", nullptr};
    if (!RegisterClassExW(&klass)) return 10;
    HWND preview = CreateWindowExW(0, L"CopyFixPreview", L"Preview regression",
        WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN, CW_USEDEFAULT, CW_USEDEFAULT,
        920, 650, nullptr, nullptr, g_instance, nullptr);
    if (!preview) return 11;
    g_preview = preview;
    const auto before = GetDlgItem(preview, kPreviewBeforeEdit);
    const auto after = GetDlgItem(preview, kPreviewAfterEdit);
    if (!before || !after || GetDlgCtrlID(after) == IDCANCEL ||
        GetDlgCtrlID(before) == GetDlgCtrlID(after)) return 12;
    wchar_t klass[60]{};
    GetClassNameW(after, klass, static_cast<int>(std::size(klass)));
    if (wcscmp(klass, L"RICHEDIT50W") != 0) return 13;
    if (SendMessageW(after, EM_GETLINECOUNT, 0, 0) < 100) return 14;
    const auto bg = static_cast<COLORREF>(SendMessageW(after, EM_GETBKGNDCOLOR, 0, 0));
    if (bg != kSurface) return 15;
    const auto toggle = GetDlgItem(preview, kPreviewShowBreaks);
    if (!toggle) return 19;
    SendMessageW(toggle, BM_CLICK, 0, 0);
    if (!g_show_breaks || !IsWindow(preview)) return 20;
    SendMessageW(toggle, BM_CLICK, 0, 0);
    if (g_show_breaks || !IsWindow(preview)) return 21;

    // Win32 EDIT sends scroll notifications to its parent. These must never
    // be interpreted as Cancel/Apply, and wheel must not destroy the preview.
    for (int i=0; i<20; ++i) {
        const HWND edits[] = {before, after};
        for (const HWND edit : edits) {
            SendMessageW(edit, WM_VSCROLL, MAKEWPARAM(SB_LINEDOWN, 0), 0);
            SendMessageW(edit, WM_MOUSEWHEEL,
                         MAKEWPARAM(0, static_cast<WORD>(static_cast<short>(-WHEEL_DELTA))),
                         MAKELPARAM(120, 120));
            SendMessageW(preview, WM_COMMAND,
                         MAKEWPARAM(static_cast<WORD>(GetDlgCtrlID(edit)), EN_VSCROLL),
                         reinterpret_cast<LPARAM>(edit));
            if (!IsWindow(preview) || !IsWindow(before) || !IsWindow(after)) return 16;
        }
    }
    // A forged button command must not trigger Accept or Cancel.
    SendMessageW(preview, WM_COMMAND, MAKEWPARAM(kPreviewCancel, EN_VSCROLL),
                 reinterpret_cast<LPARAM>(after));
    if (!IsWindow(preview)) return 17;
    // Explicit ESC can still dismiss the modeless preview.
    SendMessageW(preview, WM_COMMAND, MAKEWPARAM(IDCANCEL, 0), 0);
    if (IsWindow(preview) || g_preview) return 18;
    DeleteObject(g_font_body);
    DeleteObject(g_font_title);
    DeleteObject(g_font_label);
    DeleteObject(g_font_caption);
    DeleteObject(g_surface_brush);
    DeleteObject(g_canvas_brush);
    FreeLibrary(g_richedit_module);
    std::cout << "PASS RichEdit background, change markers, scrolling, wheel, command routing and ESC\n";
    return 0;
}
