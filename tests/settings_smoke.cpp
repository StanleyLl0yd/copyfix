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
    if (g_settings.profile != copyfix::Profile::Deep || g_settings.hotkey != 0)
        return fail(2);

    g_settings.profile = copyfix::Profile::Smart;
    g_settings.hotkey = 2;
    if (!SaveSettings()) return fail(3);
    cleanup.created_key = true;

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
    if (g_settings.profile != copyfix::Profile::Smart || g_settings.hotkey != 2) {
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
    if (g_settings.profile != copyfix::Profile::Deep || g_settings.hotkey != 0) {
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
    RegCloseKey(key);

    g_message = CreateWindowExW(0, L"STATIC", L"CopyFix settings smoke",
                                WS_OVERLAPPED, 0, 0, 1, 1,
                                nullptr, nullptr, g_instance, nullptr);
    if (!g_message) return fail(11);

    // Global hotkeys may be used by the CI desktop; only enforce registration
    // and rollback when the initial combination is actually available.
    if (RegisterChosenHotkey(0)) {
        const UINT original_id = g_hotkey_id;
        cleanup.blocker = CreateWindowExW(0, L"STATIC", L"Occupied hotkey",
                                          WS_OVERLAPPED, 0, 0, 1, 1,
                                          nullptr, nullptr, g_instance, nullptr);
        if (!cleanup.blocker) return fail(12);
        cleanup.blocked_combo = RegisterHotKey(
            cleanup.blocker, 750, HotkeyModifiers(1) | MOD_NOREPEAT,
            HotkeyVirtualKey(1)) != FALSE;
        if (cleanup.blocked_combo) {
            if (RegisterChosenHotkey(1) || !g_hotkey_registered ||
                g_hotkey_id != original_id) return fail(13);
        }
    }
    std::cout << "PASS HKCU DWORD persistence, invalid-value fallback and hotkey rollback\n";
    return 0;
}
