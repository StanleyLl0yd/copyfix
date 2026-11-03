// CI-only Win32 regression for per-user settings and hotkey rollback.
// Refuses to touch a pre-existing real CopyFix profile. The test runner is
// disposable; successful tests delete the test-created HKCU settings key.
#define wWinMain CopyFixUnusedWinMainForSettingsSmoke
#include "../src/winmain.cpp"
#undef wWinMain

#include <iostream>

namespace {
struct Cleanup {
    bool created_key = false;
    HWND blocker = nullptr;
    bool blocked_combo = false;
    ~Cleanup() {
        if (blocked_combo && blocker) UnregisterHotKey(blocker, 750);
        if (blocker) DestroyWindow(blocker);
        if (g_hotkey_registered && g_message) UnregisterHotKey(g_message, g_hotkey_id);
        g_hotkey_registered = false;
        if (g_message) DestroyWindow(g_message);
        g_message = nullptr;
        if (created_key) RegDeleteTreeW(HKEY_CURRENT_USER, kSettingsKey);
    }
};
}

int main() {
    const auto fail = [](int code) {
        std::cerr << "SETTINGS_SMOKE_FAILURE_" << code << "\n";
        return code;
    };
    HKEY existing = nullptr;
    const LONG existing_result = RegOpenKeyExW(
        HKEY_CURRENT_USER, kSettingsKey, 0, KEY_READ, &existing);
    if (existing_result == ERROR_SUCCESS) {
        RegCloseKey(existing);
        std::cout << "SKIP: existing user CopyFix registry settings left untouched\n";
        return 0;
    }
    if (existing_result != ERROR_FILE_NOT_FOUND) return fail(1);

    Cleanup cleanup;
    g_instance = GetModuleHandleW(nullptr);
    g_settings = Settings{};
    if (g_settings.profile != copyfix::Profile::Deep || g_settings.hotkey != HotkeyChord{})
        return fail(2);

    g_settings.profile = copyfix::Profile::Smart;
    g_settings.hotkey = {MOD_CONTROL | MOD_ALT, L'C'};
    cleanup.created_key = true;  // Also clean a partially created key on failure.
    if (!SaveSettings()) return fail(3);

    HKEY key = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, kSettingsKey, 0,
                      KEY_QUERY_VALUE | KEY_SET_VALUE, &key) != ERROR_SUCCESS)
        return fail(4);
    DWORD profile = 0, hotkey = 0, type = 0;
    DWORD size = sizeof(DWORD);
    const bool valid_profile =
        RegQueryValueExW(key, L"Profile", nullptr, &type,
                         reinterpret_cast<BYTE*>(&profile), &size) == ERROR_SUCCESS &&
        type == REG_DWORD && size == sizeof(DWORD) && profile == 1;
    type = 0; size = sizeof(DWORD);
    const bool valid_hotkey =
        RegQueryValueExW(key, L"Hotkey", nullptr, &type,
                         reinterpret_cast<BYTE*>(&hotkey), &size) == ERROR_SUCCESS &&
        type == REG_DWORD && size == sizeof(DWORD) && hotkey == 2;
    if (!valid_profile || !valid_hotkey) { RegCloseKey(key); return fail(5); }

    g_settings = Settings{};
    LoadSettings();
    if (g_settings.profile != copyfix::Profile::Smart || g_settings.hotkey != HotkeyChord{MOD_CONTROL | MOD_ALT, L'C'}) {
        RegCloseKey(key); return fail(6);
    }

    // Corrupt/unsupported registry values must fail closed to safe defaults.
    const DWORD invalid = 999;
    if (RegSetValueExW(key, L"Profile", 0, REG_DWORD,
                       reinterpret_cast<const BYTE*>(&invalid),
                       sizeof(invalid)) != ERROR_SUCCESS ||
        RegSetValueExW(key, L"Hotkey", 0, REG_DWORD,
                       reinterpret_cast<const BYTE*>(&invalid),
                       sizeof(invalid)) != ERROR_SUCCESS) {
        RegCloseKey(key); return fail(7);
    }
    g_settings = Settings{};
    LoadSettings();
    if (g_settings.profile != copyfix::Profile::Deep || g_settings.hotkey != HotkeyChord{}) {
        RegCloseKey(key); return fail(8);
    }
    const wchar_t not_a_number[] = L"deep";
    if (RegSetValueExW(key, L"Profile", 0, REG_SZ,
                       reinterpret_cast<const BYTE*>(not_a_number),
                       sizeof(not_a_number)) != ERROR_SUCCESS) {
        RegCloseKey(key); return fail(9);
    }
    g_settings = Settings{};
    LoadSettings();
    if (g_settings.profile != copyfix::Profile::Deep) {
        RegCloseKey(key); return fail(10);
    }
    // Preserve old DWORD presets and reject corrupt custom values.
    const HotkeyChord custom{MOD_CONTROL | MOD_SHIFT, VK_F8};
    if (!ValidHotkey(custom) || EncodeHotkey(custom) <= 2 ||
        DecodeHotkey(EncodeHotkey(custom)) != custom ||
        DecodeHotkey(999) != HotkeyChord{} ||
        ValidHotkey({0, L'A'}) ||
        ValidHotkey({MOD_SHIFT, L'A'}) ||
        ValidHotkey({MOD_WIN | MOD_CONTROL, L'A'}) ||
        ValidHotkey({MOD_CONTROL, VK_F12}) ||
        ValidHotkey({MOD_ALT, VK_LBUTTON}) ||
        ValidHotkey({MOD_CONTROL, L'V'}) ||
        ValidHotkey({MOD_CONTROL, L'C'}) ||
        ValidHotkey({MOD_CONTROL, L'X'}) ||
        ValidHotkey({MOD_CONTROL | MOD_SHIFT, L'V'}) ||
        ValidHotkey({MOD_ALT, VK_TAB}) ||
        ValidHotkey({MOD_ALT, VK_F4}) ||
        !ValidHotkey({MOD_CONTROL | MOD_ALT, VK_F8}) ||
        !ValidHotkey({MOD_CONTROL | MOD_SHIFT, VK_F8})) {
        RegCloseKey(key); return fail(14);
    }
    g_settings.hotkey = custom;
    if (!SaveSettings()) { RegCloseKey(key); return fail(15); }
    g_settings = Settings{};
    LoadSettings();
    if (g_settings.hotkey != custom) { RegCloseKey(key); return fail(16); }

    // Event-driven capture must reconstruct a chord even when Windows consumes
    // regular key messages, without consulting GetAsyncKeyState.
    ShortcutCapture capture;
    if (capture.Consume(VK_CONTROL, false).key ||
        capture.Consume(VK_MENU, false).key ||
        capture.Consume(VK_F8, false) != HotkeyChord{MOD_CONTROL | MOD_ALT, VK_F8} ||
        capture.Consume(VK_F8, false).key ||
        capture.Consume(VK_F8, true).key ||
        capture.Consume(VK_F8, false) != HotkeyChord{MOD_CONTROL | MOD_ALT, VK_F8})
        return fail(17);
    capture.Consume(VK_F8, true);
    capture.Consume(VK_MENU, true);
    if (capture.Consume(L'Q', false) != HotkeyChord{MOD_CONTROL, L'Q'})
        return fail(18);
    capture.Reset();
    if (capture.Consume(L'Q', false) != HotkeyChord{0, L'Q'} ||
        capture.Consume(VK_LWIN, false).key ||
        capture.Consume(L'N', false).key ||
        capture.Consume(VK_LWIN, true).key)
        return fail(19);
    wchar_t label[128]{};
    FormatHotkey(custom, label);
    if (!wcsstr(label, L"Ctrl") || !wcsstr(label, L"Shift") ||
        !wcsstr(label, L"F8")) return fail(20);
    const HotkeyChord examples[] = {
        {MOD_CONTROL, L'K'}, {MOD_ALT, L'9'},
        {MOD_CONTROL | MOD_ALT | MOD_SHIFT, VK_F8},
        {MOD_CONTROL | MOD_SHIFT, VK_LEFT},
        {MOD_CONTROL | MOD_ALT, VK_DIVIDE},
        {MOD_ALT | MOD_SHIFT, VK_OEM_3}
    };
    for (const auto& example : examples) {
        if (!ValidHotkey(example) || DecodeHotkey(EncodeHotkey(example)) != example)
            return fail(20);
        FormatHotkey(example, label);
        if (label[0] == L'\0') return fail(20);
    }
    if (DecodeHotkey(1) != HotkeyChord{MOD_CONTROL | MOD_SHIFT, VK_F9} ||
        DecodeHotkey(2) != HotkeyChord{MOD_CONTROL | MOD_ALT, L'C'} ||
        !IsExtendedHotkeyKey(VK_LEFT) || IsExtendedHotkeyKey(VK_F8))
        return fail(20);
    RegCloseKey(key);

    g_message = CreateWindowExW(0, L"STATIC", L"CopyFix settings smoke",
                                WS_OVERLAPPED, 0, 0, 1, 1,
                                nullptr, nullptr, g_instance, nullptr);
    if (!g_message) return fail(11);

    // Exercise the real Settings entry point, its native field, and monitor placement.
    g_canvas_brush = CreateSolidBrush(kCanvas);
    const WNDCLASSEXW settings_class{sizeof(WNDCLASSEXW), 0, SettingsProc, 0, 0, g_instance,
        nullptr, nullptr, g_canvas_brush, nullptr, L"CopyFixSettings", nullptr};
    if (!g_canvas_brush || !RegisterClassExW(&settings_class)) return fail(21);
    OpenSettings();
    if (!g_settings_window || !IsWindow(g_settings_window)) return fail(22);
    // Raw keyboard events are intentionally accepted only while the settings
    // window exists; no persistent hook is installed.
    const bool raw_registered = g_settings_raw_input;
    POINT cursor{};
    if (!GetCursorPos(&cursor)) return fail(23);
    const HMONITOR monitor = MonitorFromPoint(cursor, MONITOR_DEFAULTTONEAREST);
    MONITORINFO info{sizeof(info)};
    RECT bounds{};
    if (!monitor || !GetMonitorInfoW(monitor, &info) ||
        !GetWindowRect(g_settings_window, &bounds)) return fail(24);
    const int width = bounds.right - bounds.left;
    const int height = bounds.bottom - bounds.top;
    const int workWidth = info.rcWork.right - info.rcWork.left;
    const int workHeight = info.rcWork.bottom - info.rcWork.top;
    const int expectedX = info.rcWork.left + (workWidth > width ? (workWidth - width) / 2 : 0);
    const int expectedY = info.rcWork.top + (workHeight > height ? (workHeight - height) / 2 : 0);
    wchar_t feedback[128]{};
    const HWND status = GetDlgItem(g_settings_window, kSettingsFeedback);
    HWND editor = GetDlgItem(g_settings_window, kHotkeyInput);
    wchar_t visible[128]{}, expected[128]{};
    FormatHotkey(g_settings.hotkey, expected);
    if (bounds.left != expectedX || bounds.top != expectedY ||
        !status || GetWindowTextW(status, feedback, 128) != 0 ||
        !editor || !GetWindowTextW(editor, visible, 128) ||
        wcscmp(visible, expected) != 0) return fail(25);
    wchar_t editorClass[32]{};
    GetClassNameW(editor, editorClass, static_cast<int>(std::size(editorClass)));
    if (_wcsicmp(editorClass, L"Edit") != 0) return fail(25);
    DestroyWindow(g_settings_window);
    if (g_settings_window || (raw_registered && g_settings_raw_input)) return fail(26);
    // Global hotkeys may be used by the CI desktop; only enforce registration
    // and rollback when the initial combination is actually available.
    const HotkeyChord original{MOD_CONTROL | MOD_ALT, VK_F9};
    if (RegisterChosenHotkey(original)) {
        const UINT original_id = g_hotkey_id;
        cleanup.blocker = CreateWindowExW(0, L"STATIC", L"Occupied hotkey",
                                          WS_OVERLAPPED, 0, 0, 1, 1,
                                          nullptr, nullptr, g_instance, nullptr);
        if (!cleanup.blocker) return fail(12);
        const HotkeyChord blocked{MOD_CONTROL | MOD_SHIFT, VK_F9};
        cleanup.blocked_combo = RegisterHotKey(
            cleanup.blocker, 750, blocked.modifiers | MOD_NOREPEAT, blocked.key) != FALSE;
        if (cleanup.blocked_combo) {
            // Verify the editor never loses its displayed value on a conflict.
            OpenSettings();
            if (!g_settings_window) return fail(27);
            HWND input = GetDlgItem(g_settings_window, kHotkeyInput);
            wchar_t before[128]{}, after[128]{};
            if (!input || !GetWindowTextW(input, before, 128)) return fail(28);
            if (HotkeyAvailable(blocked) || !HotkeyAvailable(original) ||
                AcceptShortcutCandidate(g_settings_window, blocked) ||
                RegisterChosenHotkey(blocked) || !g_hotkey_registered ||
                g_hotkey_id != original_id || g_registered_hotkey != original ||
                !GetWindowTextW(input, after, 128) || wcscmp(before, after) != 0)
                return fail(13);
            DestroyWindow(g_settings_window);
        }
    }
    UnregisterClassW(L"CopyFixSettings", g_instance);
    DeleteObject(g_canvas_brush);
    g_canvas_brush = nullptr;
    std::cout << "PASS deterministic Raw Input capture, occupied-shortcut editor preservation, settings centering and hotkey rollback\n";
    return 0;
}
