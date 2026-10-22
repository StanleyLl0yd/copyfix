#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <shellapi.h>

#include "text_engine.h"

#include <cstddef>
#include <algorithm>
#include <cstring>
#include <limits>
#include <iterator>
#include <new>
#include <string>
#include <utility>

namespace {
constexpr UINT kTrayMessage = WM_APP + 1;
constexpr UINT kHotkeyA = 100;
constexpr UINT kHotkeyB = 101;
constexpr UINT kMaxChars = 1000000;
constexpr UINT kMaxPreviewChars = 100000;
constexpr UINT kMenuFix = 200;
constexpr UINT kMenuPreview = 201;
constexpr UINT kMenuUndo = 202;
constexpr UINT kMenuGentle = 203;
constexpr UINT kMenuSmart = 204;
constexpr UINT kMenuDeep = 205;
constexpr UINT kMenuSettings = 206;
constexpr UINT kMenuAbout = 207;
constexpr UINT kMenuExit = 208;
constexpr UINT kMenuOneLine = 209;
constexpr UINT kPreviewAccept = 300;
constexpr UINT kPreviewCancel = 301;
constexpr UINT kSettingsSave = 400;
constexpr UINT kSettingsCancel = 401;
constexpr UINT kComboHotkey = 402;
constexpr UINT kComboProfile = 403;

struct Settings {
    copyfix::Profile profile = copyfix::Profile::Gentle;
    int hotkey = 0;
};
struct ClipSnapshot {
    std::wstring text;
    DWORD sequence = 0;
    bool rich_formats = false;
};
struct Pending {
    ClipSnapshot before;
    copyfix::Result after;
};
struct Undo {
    std::wstring before;
    std::wstring after;
    DWORD after_sequence = 0;
    bool available = false;
};

HINSTANCE g_instance = nullptr;
HWND g_message = nullptr;
HWND g_preview = nullptr;
HWND g_settings_window = nullptr;
NOTIFYICONDATAW g_tray{};
Settings g_settings;
Pending g_pending;
Undo g_undo;
HANDLE g_instance_mutex = nullptr;
UINT g_hotkey_id = kHotkeyA;
UINT g_taskbar_created = 0;
bool g_hotkey_registered = false;

const wchar_t* HotkeyName(int i) {
    switch (i) {
    case 1: return L"Ctrl + Shift + F9";
    case 2: return L"Ctrl + Alt + C";
    default: return L"Ctrl + Alt + F9";
    }
}
UINT HotkeyModifiers(int i) { return i == 1 ? MOD_CONTROL | MOD_SHIFT : MOD_CONTROL | MOD_ALT; }
UINT HotkeyVirtualKey(int i) { return i == 2 ? L'C' : VK_F9; }

std::wstring ConfigPath() {
    try {
        // Avoid a 64-KiB stack buffer (MSVC /analyze C6262).
        std::wstring path(32768, L'\0');
        const DWORD size = GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));
        if (!size || size >= path.size()) return {};
        path.resize(size);
        const auto slash = path.find_last_of(L"\\/");
        if (slash == std::wstring::npos) return {};
        path.resize(slash + 1);
        return path + L"copyfix.ini";
    } catch (const std::bad_alloc&) {
        return {}; // Settings stay in memory if a configuration path cannot be allocated.
    }
}

void LoadSettings() {
    const auto path = ConfigPath();
    if (path.empty()) return;
    const int p = GetPrivateProfileIntW(L"CopyFix", L"Profile", 0, path.c_str());
    const int h = GetPrivateProfileIntW(L"CopyFix", L"Hotkey", 0, path.c_str());
    if (p >= 0 && p <= 2) g_settings.profile = static_cast<copyfix::Profile>(p);
    if (h >= 0 && h <= 2) g_settings.hotkey = h;
}

void SaveSettings() {
    const auto path = ConfigPath();
    if (path.empty()) return;
    const auto p = std::to_wstring(static_cast<int>(g_settings.profile));
    const auto h = std::to_wstring(g_settings.hotkey);
    // Portable: failures (e.g. read-only directories) keep in-memory settings.
    WritePrivateProfileStringW(L"CopyFix", L"Profile", p.c_str(), path.c_str());
    WritePrivateProfileStringW(L"CopyFix", L"Hotkey", h.c_str(), path.c_str());
}

void Notify(const wchar_t* message) {
    if (!g_tray.hWnd) return;
    g_tray.uFlags = NIF_INFO;
    wcscpy_s(g_tray.szInfoTitle, L"CopyFix");
    wcsncpy_s(g_tray.szInfo, message, _TRUNCATE);
    g_tray.dwInfoFlags = NIIF_INFO | NIIF_NOSOUND;
    Shell_NotifyIconW(NIM_MODIFY, &g_tray);
}

bool RegisterChosenHotkey(int index) {
    const UINT other_id = g_hotkey_id == kHotkeyA ? kHotkeyB : kHotkeyA;
    if (!RegisterHotKey(g_message, other_id, HotkeyModifiers(index) | MOD_NOREPEAT,
                        HotkeyVirtualKey(index))) return false;
    if (g_hotkey_registered) UnregisterHotKey(g_message, g_hotkey_id);
    g_hotkey_id = other_id;
    g_hotkey_registered = true;
    return true;
}

bool OpenClipboardBounded(HWND owner) {
    for (int tries = 0; tries < 4; ++tries) {
        if (OpenClipboard(owner)) return true;
        if (tries != 3) Sleep(25);
    }
    return false;
}

bool ReadUnderLock(ClipSnapshot& result) {
    if (!IsClipboardFormatAvailable(CF_UNICODETEXT)) return false;
    HANDLE handle = GetClipboardData(CF_UNICODETEXT);
    if (!handle) return false;
    const SIZE_T bytes = GlobalSize(handle);
    if (bytes < sizeof(wchar_t) || bytes % sizeof(wchar_t) != 0 ||
        bytes > (static_cast<SIZE_T>(kMaxChars) + 1) * sizeof(wchar_t)) return false;
    const auto* data = static_cast<const wchar_t*>(GlobalLock(handle));
    if (!data) return false;
    bool success = false;
    const SIZE_T maximum = bytes / sizeof(wchar_t);
    SIZE_T len = 0;
    while (len < maximum && data[len] != 0) ++len;
    if (len < maximum && len <= kMaxChars) {
        try {
            result.text.assign(data, len);
            success = copyfix::IsWellFormedUtf16(result.text);
        } catch (const std::bad_alloc&) {
            // GlobalUnlock and CloseClipboard must still run on allocation failure.
            success = false;
        }
    }
    GlobalUnlock(handle);
    if (!success) return false;
    result.sequence = GetClipboardSequenceNumber();
    result.rich_formats = false;
    UINT format = 0;
    while ((format = EnumClipboardFormats(format)) != 0) {
        if (format != CF_UNICODETEXT && format != CF_TEXT && format != CF_OEMTEXT &&
            format != CF_LOCALE) {
            result.rich_formats = true;
            break;
        }
    }
    return true;
}

bool Snapshot(ClipSnapshot& snapshot) {
    if (!OpenClipboardBounded(g_message)) return false;
    const bool success = ReadUnderLock(snapshot);
    CloseClipboard();
    return success;
}

HGLOBAL AllocateClipboardString(const std::wstring& value) {
    // All allocation and copying are completed BEFORE EmptyClipboard.
    if (value.size() > kMaxChars) return nullptr;
    const SIZE_T bytes = (value.size() + 1) * sizeof(wchar_t);
    HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE, bytes);
    if (!memory) return nullptr;
    auto* ptr = static_cast<wchar_t*>(GlobalLock(memory));
    if (!ptr) { GlobalFree(memory); return nullptr; }
    std::memcpy(ptr, value.c_str(), bytes);
    GlobalUnlock(memory);
    return memory;
}

bool WriteIfUnchanged(const ClipSnapshot& expected, const std::wstring& text,
                      DWORD* written_sequence) {
    // Unicode clipboard ownership is not transactionally atomic; a best-effort
    // text restore exists for the extremely rare SetClipboardData failure.
    HGLOBAL prepared = AllocateClipboardString(text);
    HGLOBAL backup = AllocateClipboardString(expected.text);
    if (!prepared || !backup) {
        if (prepared) GlobalFree(prepared);
        if (backup) GlobalFree(backup);
        return false;
    }
    if (!OpenClipboardBounded(g_message)) {
        GlobalFree(prepared); GlobalFree(backup);
        return false;
    }
    ClipSnapshot actual;
    if (GetClipboardSequenceNumber() != expected.sequence ||
        !ReadUnderLock(actual) || actual.text != expected.text ||
        actual.rich_formats != expected.rich_formats) {
        CloseClipboard();
        GlobalFree(prepared); GlobalFree(backup);
        return false;
    }
    bool written = false;
    if (EmptyClipboard()) {
        if (SetClipboardData(CF_UNICODETEXT, prepared)) {
            prepared = nullptr;  // Windows now owns the HGLOBAL.
            written = true;
        } else if (SetClipboardData(CF_UNICODETEXT, backup)) {
            backup = nullptr;  // Attempt to retain the original plain text.
        }
    }
    CloseClipboard();
    if (prepared) GlobalFree(prepared);
    if (backup) GlobalFree(backup);
    if (written && GetClipboardOwner() != g_message) return false;
    if (written && written_sequence) *written_sequence = GetClipboardSequenceNumber();
    return written;
}

std::wstring ToClipboardLineEndings(const std::wstring& source) {
    std::wstring out;
    out.reserve(source.size());
    for (std::size_t i = 0; i < source.size(); ++i) {
        if (source[i] == L'\n' && (i == 0 || source[i - 1] != L'\r')) out += L'\r';
        out += source[i];
    }
    return out;
}

void ClearPending() { g_pending = Pending{}; }
void ClearUndo() { g_undo = Undo{}; }

void AcceptPending() {
    try {
        const auto final_text = ToClipboardLineEndings(g_pending.after.text);
        // Prepare Undo before mutating clipboard; OOM must not leave an
        // already-modified clipboard without a valid Undo snapshot.
        Undo next_undo;
        next_undo.before = g_pending.before.text;
        next_undo.after = final_text;
        DWORD sequence = 0;
        if (WriteIfUnchanged(g_pending.before, final_text, &sequence)) {
            next_undo.after_sequence = sequence;
            next_undo.available = true;
            g_undo = std::move(next_undo);
            Notify(L"Исправленный текст находится в буфере обмена.");
        } else Notify(L"Запись не завершена. Проверьте содержимое буфера обмена.");
    } catch (const std::bad_alloc&) {
        Notify(L"Недостаточно памяти. Буфер не изменён.");
    }
    ClearPending();
}

LRESULT CALLBACK PreviewProc(HWND window, UINT msg, WPARAM wparam, LPARAM lparam) {
    switch (msg) {
    case WM_CREATE: {
        CreateWindowW(L"STATIC", L"Исходный текст", WS_CHILD | WS_VISIBLE, 12, 12, 300, 20,
                      window, nullptr, g_instance, nullptr);
        CreateWindowW(L"STATIC", L"Предлагаемый результат", WS_CHILD | WS_VISIBLE, 410, 12, 300, 20,
                      window, nullptr, g_instance, nullptr);
        const auto count_label = L"Изменений: " + std::to_wstring(g_pending.after.changes);
        CreateWindowW(L"STATIC", count_label.c_str(), WS_CHILD | WS_VISIBLE,
                      12, 436, 400, 20, window, reinterpret_cast<HMENU>(3), g_instance, nullptr);
        const DWORD style = WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE |
                            ES_AUTOVSCROLL | ES_READONLY;
        HWND before = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", g_pending.before.text.c_str(),
                                     style, 12, 36, 382, 380, window, reinterpret_cast<HMENU>(1), g_instance, nullptr);
        HWND after = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", g_pending.after.text.c_str(),
                                    style, 410, 36, 382, 380, window, reinterpret_cast<HMENU>(2), g_instance, nullptr);
        const auto font = static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
        SendMessageW(before, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
        SendMessageW(after, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
        CreateWindowW(L"BUTTON", L"Применить", WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
                      564, 432, 110, 32, window, reinterpret_cast<HMENU>(static_cast<UINT_PTR>(kPreviewAccept)), g_instance, nullptr);
        CreateWindowW(L"BUTTON", L"Отмена", WS_CHILD | WS_VISIBLE,
                      684, 432, 110, 32, window, reinterpret_cast<HMENU>(static_cast<UINT_PTR>(kPreviewCancel)), g_instance, nullptr);
        return 0;
    }
    case WM_SIZE: {
        const int w = LOWORD(lparam), h = HIWORD(lparam);
        const int gap = 16, left = 12, half = std::max(1, (w - 2 * left - gap) / 2);
        MoveWindow(GetDlgItem(window, 1), left, 36, half, (h > 110 ? h - 110 : 1), TRUE);
        MoveWindow(GetDlgItem(window, 2), left + half + gap, 36, half, (h > 110 ? h - 110 : 1), TRUE);
        MoveWindow(GetDlgItem(window, 3), 12, h - 43, 400, 25, TRUE);
        MoveWindow(GetDlgItem(window, kPreviewAccept), w - 240, h - 48, 106, 30, TRUE);
        MoveWindow(GetDlgItem(window, kPreviewCancel), w - 122, h - 48, 106, 30, TRUE);
        return 0;
    }
    case WM_GETMINMAXINFO: {
        auto* bounds = reinterpret_cast<MINMAXINFO*>(lparam);
        bounds->ptMinTrackSize = POINT{600, 350};
        return 0;
    }
    case WM_COMMAND:
        if (LOWORD(wparam) == kPreviewAccept) {
            AcceptPending();
            DestroyWindow(window);
            return 0;
        }
        if (LOWORD(wparam) == kPreviewCancel) { DestroyWindow(window); return 0; }
        break;
    case WM_CLOSE: DestroyWindow(window); return 0;
    case WM_DESTROY:
        g_preview = nullptr;
        ClearPending();
        return 0;
    }
    return DefWindowProcW(window, msg, wparam, lparam);
}

void OpenPreview() {
    if (g_preview) { SetForegroundWindow(g_preview); return; }
    g_preview = CreateWindowExW(WS_EX_APPWINDOW, L"CopyFixPreview", L"CopyFix: предпросмотр",
        WS_OVERLAPPEDWINDOW | WS_VISIBLE, CW_USEDEFAULT, CW_USEDEFAULT, 830, 530,
        nullptr, nullptr, g_instance, nullptr);
    if (!g_preview) { ClearPending(); Notify(L"Не удалось открыть предпросмотр."); }
}

void FixClipboard(bool force_preview, bool one_line = false) {
    if (g_preview) { SetForegroundWindow(g_preview); return; }
    ClipSnapshot snapshot;
    if (!Snapshot(snapshot)) { Notify(L"Нет подходящего текста или буфер недоступен."); return; }
    if (snapshot.text.empty()) { Notify(L"Буфер содержит пустой текст."); return; }
    copyfix::Result cleaned;
    try {
        cleaned = one_line ? copyfix::OneLine(snapshot.text)
                           : copyfix::Clean(snapshot.text, g_settings.profile);
    } catch (const std::bad_alloc&) {
        Notify(L"Недостаточно памяти. Буфер не изменён.");
        return;
    }
    if (cleaned.protected_structure) { Notify(L"Обнаружен структурированный текст: оставлен без изменений."); return; }
    if (cleaned.text == snapshot.text || ToClipboardLineEndings(cleaned.text) == snapshot.text) {
        Notify(L"Изменений не найдено.");
        return;
    }
    g_pending = Pending{std::move(snapshot), std::move(cleaned)};
    if (force_preview || g_pending.after.requires_confirmation || g_pending.before.rich_formats ||
        g_settings.profile == copyfix::Profile::Deep) {
        if (g_pending.before.text.size() > kMaxPreviewChars ||
            g_pending.after.text.size() > kMaxPreviewChars) {
            ClearPending();
            Notify(L"Текст слишком велик для безопасного предпросмотра. Разделите его на части.");
            return;
        }
        OpenPreview();
    } else {
        AcceptPending();
    }
}

void UndoLast() {
    if (!g_undo.available) { Notify(L"Нет исправления для отмены."); return; }
    ClipSnapshot current;
    if (!Snapshot(current) || current.sequence != g_undo.after_sequence ||
        current.text != g_undo.after) {
        ClearUndo();
        Notify(L"Буфер изменился: отмена невозможна.");
        return;
    }
    const bool restored = WriteIfUnchanged(current, g_undo.before, nullptr);
    if (restored) {
        ClearUndo();
        Notify(L"Исходный текст восстановлен (только plain text).");
    } else Notify(L"Не удалось восстановить текст, буфер не изменён или занят.");
}

LRESULT CALLBACK SettingsProc(HWND window, UINT msg, WPARAM wparam, LPARAM lparam) {
    switch (msg) {
    case WM_CREATE: {
        CreateWindowW(L"STATIC", L"Горячая клавиша:", WS_CHILD | WS_VISIBLE,
                      20, 22, 250, 20, window, nullptr, g_instance, nullptr);
        HWND h = CreateWindowW(L"COMBOBOX", nullptr,
            WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
            20, 45, 310, 200, window, reinterpret_cast<HMENU>(static_cast<UINT_PTR>(kComboHotkey)), g_instance, nullptr);
        for (int i = 0; i < 3; ++i) SendMessageW(h, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(HotkeyName(i)));
        SendMessageW(h, CB_SETCURSEL, static_cast<WPARAM>(g_settings.hotkey), 0);
        CreateWindowW(L"STATIC", L"Профиль:", WS_CHILD | WS_VISIBLE,
                      20, 90, 250, 20, window, nullptr, g_instance, nullptr);
        HWND p = CreateWindowW(L"COMBOBOX", nullptr,
            WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
            20, 113, 310, 200, window, reinterpret_cast<HMENU>(static_cast<UINT_PTR>(kComboProfile)), g_instance, nullptr);
        for (const auto* s : {L"Бережный", L"Умный", L"Глубокий"})
            SendMessageW(p, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(s));
        SendMessageW(p, CB_SETCURSEL, static_cast<WPARAM>(g_settings.profile), 0);
        CreateWindowW(L"BUTTON", L"Сохранить", WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
                      100, 174, 110, 32, window, reinterpret_cast<HMENU>(static_cast<UINT_PTR>(kSettingsSave)), g_instance, nullptr);
        CreateWindowW(L"BUTTON", L"Отмена", WS_CHILD | WS_VISIBLE,
                      220, 174, 110, 32, window, reinterpret_cast<HMENU>(static_cast<UINT_PTR>(kSettingsCancel)), g_instance, nullptr);
        return 0;
    }
    case WM_COMMAND:
        if (LOWORD(wparam) == kSettingsSave) {
            const int next_hotkey = static_cast<int>(SendMessageW(GetDlgItem(window, kComboHotkey), CB_GETCURSEL, 0, 0));
            const int next_profile = static_cast<int>(SendMessageW(GetDlgItem(window, kComboProfile), CB_GETCURSEL, 0, 0));
            if (next_hotkey < 0 || next_hotkey > 2 || next_profile < 0 || next_profile > 2) return 0;
            if ((!g_hotkey_registered || next_hotkey != g_settings.hotkey) &&
                !RegisterChosenHotkey(next_hotkey)) {
                MessageBoxW(window, L"Сочетание занято другой программой. Выберите другое.",
                            L"CopyFix", MB_ICONWARNING);
                return 0;
            }
            g_settings.hotkey = next_hotkey;
            g_settings.profile = static_cast<copyfix::Profile>(next_profile);
            SaveSettings();
            DestroyWindow(window);
            return 0;
        }
        if (LOWORD(wparam) == kSettingsCancel) { DestroyWindow(window); return 0; }
        break;
    case WM_CLOSE: DestroyWindow(window); return 0;
    case WM_DESTROY: g_settings_window = nullptr; return 0;
    }
    return DefWindowProcW(window, msg, wparam, lparam);
}

void OpenSettings() {
    if (g_settings_window) { SetForegroundWindow(g_settings_window); return; }
    g_settings_window = CreateWindowExW(WS_EX_APPWINDOW, L"CopyFixSettings", L"Настройки CopyFix",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_VISIBLE,
        CW_USEDEFAULT, CW_USEDEFAULT, 366, 252,
        nullptr, nullptr, g_instance, nullptr);
}

void ShowMenu() {
    HMENU menu = CreatePopupMenu();
    HMENU profiles = CreatePopupMenu();
    if (!menu || !profiles) {
        if (profiles) DestroyMenu(profiles);
        if (menu) DestroyMenu(menu);
        return;
    }
    AppendMenuW(menu, MF_STRING, kMenuFix, L"Исправить текст");
    AppendMenuW(menu, MF_STRING, kMenuPreview, L"Предпросмотр изменений");
    AppendMenuW(menu, MF_STRING, kMenuOneLine, L"В одну строку (предпросмотр)");
    AppendMenuW(menu, MF_STRING | (g_undo.available ? 0 : MF_GRAYED), kMenuUndo, L"Отменить исправление");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(profiles, MF_STRING, kMenuGentle, L"Бережный");
    AppendMenuW(profiles, MF_STRING, kMenuSmart, L"Умный");
    AppendMenuW(profiles, MF_STRING, kMenuDeep, L"Глубокий");
    CheckMenuRadioItem(profiles, kMenuGentle, kMenuDeep,
                       kMenuGentle + static_cast<UINT>(g_settings.profile), MF_BYCOMMAND);
    AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(profiles), L"Профиль");
    AppendMenuW(menu, MF_STRING, kMenuSettings, L"Настройки");
    AppendMenuW(menu, MF_STRING, kMenuAbout, L"О программе");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, kMenuExit, L"Выход");
    POINT p{};
    GetCursorPos(&p);
    SetForegroundWindow(g_message);
    TrackPopupMenu(menu, TPM_RIGHTBUTTON | TPM_LEFTALIGN, p.x, p.y, 0, g_message, nullptr);
    PostMessageW(g_message, WM_NULL, 0, 0);
    DestroyMenu(menu);  // also destroys nested menu
}

LRESULT CALLBACK MessageProc(HWND window, UINT msg, WPARAM wparam, LPARAM lparam) {
    if (g_taskbar_created && msg == g_taskbar_created) {
        g_tray.uFlags = NIF_ICON | NIF_TIP | NIF_MESSAGE;
        Shell_NotifyIconW(NIM_ADD, &g_tray);
        g_tray.uVersion = NOTIFYICON_VERSION_4;
        Shell_NotifyIconW(NIM_SETVERSION, &g_tray);
        return 0;
    }
    switch (msg) {
    case WM_CREATE: {
        g_message = window;
        const bool hotkey_ok = RegisterChosenHotkey(g_settings.hotkey);
        g_tray = {};
        g_tray.cbSize = sizeof(g_tray);
        g_tray.hWnd = window;
        g_tray.uID = 1;
        g_tray.uFlags = NIF_ICON | NIF_TIP | NIF_MESSAGE;
        g_tray.uCallbackMessage = kTrayMessage;
        g_tray.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
        wcscpy_s(g_tray.szTip, L"CopyFix - очистка текста");
        Shell_NotifyIconW(NIM_ADD, &g_tray);
        g_tray.uVersion = NOTIFYICON_VERSION_4;
        Shell_NotifyIconW(NIM_SETVERSION, &g_tray);
        if (!hotkey_ok) Notify(L"Сочетание занято. Откройте настройки CopyFix.");
        return 0;
    }
    case WM_HOTKEY:
        if (wparam == g_hotkey_id) { FixClipboard(false); return 0; }
        break;
    case kTrayMessage:
        if (LOWORD(lparam) == WM_CONTEXTMENU || LOWORD(lparam) == NIN_SELECT ||
            LOWORD(lparam) == WM_RBUTTONUP || LOWORD(lparam) == WM_LBUTTONUP) {
            ShowMenu();
        }
        return 0;
    case WM_COMMAND:
        switch (LOWORD(wparam)) {
        case kMenuFix: FixClipboard(false); break;
        case kMenuPreview: FixClipboard(true); break;
        case kMenuOneLine: FixClipboard(true, true); break;
        case kMenuUndo: UndoLast(); break;
        case kMenuGentle: case kMenuSmart: case kMenuDeep:
            g_settings.profile = static_cast<copyfix::Profile>(LOWORD(wparam) - kMenuGentle);
            SaveSettings(); break;
        case kMenuSettings: OpenSettings(); break;
        case kMenuAbout:
            MessageBoxW(window, L"CopyFix 0.1.0\nНативная офлайн-утилита для очистки текста.\n"
                L"Автоматически сохраняет структурированные фрагменты.\n"
                L"Изменение форматов HTML/RTF требует подтверждения.",
                L"О CopyFix", MB_OK | MB_ICONINFORMATION); break;
        case kMenuExit: DestroyWindow(window); break;
        default: break;
        }
        return 0;
    case WM_DESTROY:
        if (g_preview) DestroyWindow(g_preview);
        if (g_settings_window) DestroyWindow(g_settings_window);
        if (g_hotkey_registered) UnregisterHotKey(window, g_hotkey_id);
        Shell_NotifyIconW(NIM_DELETE, &g_tray);
        ClearPending(); ClearUndo();
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(window, msg, wparam, lparam);
}
}  // namespace

int WINAPI wWinMain(_In_ HINSTANCE instance, _In_opt_ HINSTANCE, _In_ PWSTR, _In_ int) {
    g_instance = instance;
    g_instance_mutex = CreateMutexW(nullptr, FALSE, L"Local\\CopyFix_Portable_Instance_8A69E7C9");
    if (!g_instance_mutex) return 1;
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        CloseHandle(g_instance_mutex);
        MessageBoxW(nullptr,
                    L"CopyFix уже запущен. Чтобы использовать новую версию, "
                    L"закройте работающий экземпляр через значок в трее "
                    L"и затем запустите этот файл снова.",
                    L"CopyFix - уже запущен", MB_OK | MB_ICONINFORMATION);
        return 0;
    }
    LoadSettings();
    g_taskbar_created = RegisterWindowMessageW(L"TaskbarCreated");
    const WNDCLASSEXW classes[] = {
        {sizeof(WNDCLASSEXW), 0, MessageProc, 0, 0, instance, nullptr, nullptr,
            nullptr, nullptr, L"CopyFixMessage", nullptr},
        {sizeof(WNDCLASSEXW), 0, PreviewProc, 0, 0, instance, nullptr, nullptr,
            reinterpret_cast<HBRUSH>(COLOR_BTNFACE + 1), nullptr, L"CopyFixPreview", nullptr},
        {sizeof(WNDCLASSEXW), 0, SettingsProc, 0, 0, instance, nullptr, nullptr,
            reinterpret_cast<HBRUSH>(COLOR_BTNFACE + 1), nullptr, L"CopyFixSettings", nullptr}
    };
    for (const auto& wc : classes) {
        if (!RegisterClassExW(&wc)) { CloseHandle(g_instance_mutex); return 2; }
    }
    if (!CreateWindowExW(0, L"CopyFixMessage", L"CopyFix", WS_OVERLAPPED,
                         0, 0, 0, 0, nullptr, nullptr, instance, nullptr)) {
        CloseHandle(g_instance_mutex); return 3;
    }
    MSG message{};
    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
    CloseHandle(g_instance_mutex);
    return static_cast<int>(message.wParam);
}
