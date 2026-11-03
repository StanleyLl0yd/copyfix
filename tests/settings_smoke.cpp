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
        ValidHotkey({MOD_ALT, VK_LBUTTON})) {
        RegCloseKey(key); return fail(14);
    }
    g_settings.hotkey = custom;
    if (!SaveSettings()) { RegCloseKey(key); return fail(15); }
    g_settings = Settings{};
    LoadSettings();
    if (g_settings.hotkey != custom) { RegCloseKey(key); return fail(16); }

    const INITCOMMONCONTROLSEX controls{sizeof(INITCOMMONCONTROLSEX), ICC_HOTKEY_CLASS};
    if (!InitCommonControlsEx(&controls)) { RegCloseKey(key); return fail(17); }
    HWND host = CreateWindowExW(0, L"STATIC", L"hotkey test",
                              WS_OVERLAPPED, 0, 0, 200, 50,
                              nullptr, nullptr, g_instance, nullptr);
    if (!host) { RegCloseKey(key); return fail(18); }
    HWND editor = CreateWindowExW(0, HOTKEY_CLASSW, nullptr,
        WS_CHILD | WS_VISIBLE, 0, 0, 200, 24, host, nullptr, g_instance, nullptr);
    if (!editor) { DestroyWindow(host); RegCloseKey(key); return fail(19); }
    SendMessageW(editor, HKM_SETHOTKEY, HotkeyControlValue(custom), 0);
    const bool control_ok = ReadHotkeyControl(editor) == custom;
    DestroyWindow(host);
    if (!control_ok) { RegCloseKey(key); return fail(20); }
    RegCloseKey(key);

    g_message = CreateWindowExW(0, L"STATIC", L"CopyFix settings smoke",
                                WS_OVERLAPPED, 0, 0, 1, 1,
                                nullptr, nullptr, g_instance, nullptr);
    if (!g_message) return fail(11);

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
            if (RegisterChosenHotkey(blocked) || !g_hotkey_registered ||
                g_hotkey_id != original_id || g_registered_hotkey != original)
                return fail(13);
        }
    }
    std::cout << "PASS legacy/custom HKCU chords, native input and rollback\n";
    return 0;
}
