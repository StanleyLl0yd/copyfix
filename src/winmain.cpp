#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <shellapi.h>
#include <commctrl.h>
#include <richedit.h>
#include "copyfix_version.h"
#include "diff_spans.h"
#include "resource_ids.h"

#include "text_engine.h"

#include <cstddef>
#include <algorithm>
#include <cstring>
#include <limits>
#include <iterator>
#include <new>
#include <string>
#include <string_view>
#include <utility>

namespace {
constexpr UINT kTrayMessage = WM_APP + 1;
constexpr UINT kHotkeyA = 100;
constexpr UINT kHotkeyB = 101;
constexpr UINT kHotkeyProbe = 102;
constexpr UINT kSettingsConflictMessage = WM_APP + 2;
constexpr UINT kSettingsNormalKeyMessage = WM_APP + 3;
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
constexpr UINT kPreviewBeforeEdit = 510;
constexpr UINT kPreviewAfterEdit = 511;
constexpr UINT kPreviewShowBreaks = 512;
constexpr UINT kPreviewShowSpaces = 513;
constexpr UINT kPreviewAccept = 300;
constexpr UINT kPreviewCancel = 301;
constexpr UINT kSettingsSave = 400;
constexpr UINT kSettingsCancel = 401;
constexpr UINT kHotkeyInput = 402;
constexpr UINT kComboProfile = 403;
constexpr UINT kPreviewTitle = 410;
constexpr UINT kPreviewSubtitle = 411;
constexpr UINT kPreviewBeforeLabel = 412;
constexpr UINT kPreviewAfterLabel = 413;
constexpr UINT kPreviewNote = 414;
constexpr UINT kSettingsTitle = 415;
constexpr UINT kSettingsSubtitle = 416;
constexpr UINT kSettingsFeedback = 417;

constexpr wchar_t kAboutVersion[] = L"CopyFix " COPYFIX_VERSION_WSTR;
constexpr wchar_t kAboutWebsite[] = L"https://stanleyll0yd.github.io/apps/copyfix/";
constexpr wchar_t kAboutWebsiteLink[] =
    L"<a href=\"https://stanleyll0yd.github.io/apps/copyfix/\">https://stanleyll0yd.github.io/apps/copyfix/</a>";
constexpr wchar_t kAboutDescription[] =
    L"Нативная офлайн-утилита для очистки текста.\n"
    L"Автоматически сохраняет структурированные фрагменты.\n"
    L"Изменение форматов HTML/RTF требует подтверждения.";

void CenterWindowOnMonitor(HWND dialog, HMONITOR monitor) noexcept {
    MONITORINFO info{sizeof(info)};
    RECT window{};
    if (!monitor || !GetMonitorInfoW(monitor, &info) || !GetWindowRect(dialog, &window))
        return;
    const int width = window.right - window.left;
    const int height = window.bottom - window.top;
    const int workWidth = info.rcWork.right - info.rcWork.left;
    const int workHeight = info.rcWork.bottom - info.rcWork.top;
    if (width <= 0 || height <= 0 || workWidth <= 0 || workHeight <= 0)
        return;
    const int x = info.rcWork.left + (workWidth > width ? (workWidth - width) / 2 : 0);
    const int y = info.rcWork.top + (workHeight > height ? (workHeight - height) / 2 : 0);
    SetWindowPos(dialog, nullptr, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
}

HRESULT CALLBACK AboutCallback(HWND window, UINT notification, WPARAM, LPARAM data,
                              LONG_PTR callbackData) noexcept {
    if (notification == TDN_CREATED) {
        CenterWindowOnMonitor(window, reinterpret_cast<HMONITOR>(callbackData));
    } else if (notification == TDN_HYPERLINK_CLICKED && data) {
        const auto* url = reinterpret_cast<const wchar_t*>(data);
        if (wcscmp(url, kAboutWebsite) == 0)
            ShellExecuteW(window, L"open", kAboutWebsite, nullptr, nullptr, SW_SHOWNORMAL);
    }
    return S_OK;
}

void ShowAbout(HWND parent) noexcept {
    POINT cursor{};
    const HMONITOR monitor = GetCursorPos(&cursor)
        ? MonitorFromPoint(cursor, MONITOR_DEFAULTTONEAREST)
        : MonitorFromWindow(parent, MONITOR_DEFAULTTONEAREST);
    TASKDIALOGCONFIG config{};
    config.cbSize = sizeof(config);
    config.hwndParent = parent;
    config.dwFlags = TDF_ENABLE_HYPERLINKS;
    config.lpCallbackData = reinterpret_cast<LONG_PTR>(monitor);
    config.dwCommonButtons = TDCBF_OK_BUTTON;
    config.pszWindowTitle = L"О CopyFix";
    config.pszMainInstruction = kAboutVersion;
    config.pszMainIcon = TD_INFORMATION_ICON;
    config.pszContent = kAboutDescription;
    config.pszFooter = kAboutWebsiteLink;
    config.pfCallback = AboutCallback;
    using TaskDialogFunction = HRESULT (WINAPI*)(const TASKDIALOGCONFIG*, int*, int*, BOOL*);
    if (HMODULE controls = LoadLibraryExW(L"comctl32.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32)) {
        auto* show_dialog = reinterpret_cast<TaskDialogFunction>(
            GetProcAddress(controls, "TaskDialogIndirect"));
        const bool shown = show_dialog && SUCCEEDED(show_dialog(&config, nullptr, nullptr, nullptr));
        FreeLibrary(controls);
        if (shown) return;
    }
    constexpr wchar_t fallback[] =
        L"CopyFix " COPYFIX_VERSION_WSTR L"\n"
        L"Нативная офлайн-утилита для очистки текста.\n"
        L"Автоматически сохраняет структурированные фрагменты.\n"
        L"Изменение форматов HTML/RTF требует подтверждения.\n"
        L"https://stanleyll0yd.github.io/apps/copyfix/";
    MessageBoxW(parent, fallback, L"О CopyFix", MB_OK | MB_ICONINFORMATION);
}

constexpr COLORREF kCanvas = RGB(247, 249, 252);
constexpr COLORREF kSurface = RGB(255, 255, 255);
constexpr COLORREF kInk = RGB(29, 42, 64);
constexpr COLORREF kMuted = RGB(99, 113, 133);
constexpr COLORREF kBlue = RGB(37, 99, 235);
constexpr COLORREF kBorder = RGB(222, 228, 237);
HBRUSH g_canvas_brush = nullptr;
HBRUSH g_surface_brush = nullptr;
HFONT g_font_body = nullptr;
HFONT g_font_title = nullptr;
HFONT g_font_label = nullptr;
HFONT g_font_caption = nullptr;
int g_preview_dpi = 96;
int g_settings_dpi = 96;


struct HotkeyChord {
    UINT modifiers = MOD_CONTROL | MOD_ALT;
    UINT key = VK_F9;
    bool operator==(const HotkeyChord&) const = default;
};

// One source of truth for keyboard capture: physical Raw Input make/break
// events, not timing-sensitive GetAsyncKeyState or HOTKEY_CLASS text.
struct ShortcutCapture {
    UINT modifiers = 0;
    UINT heldKey = 0;
    bool winHeld = false;

    void Reset() noexcept {
        modifiers = 0;
        heldKey = 0;
        winHeld = false;
    }

    HotkeyChord Consume(UINT key, bool released) noexcept {
        UINT mask = 0;
        if (key == VK_CONTROL || key == VK_LCONTROL || key == VK_RCONTROL)
            mask = MOD_CONTROL;
        else if (key == VK_MENU || key == VK_LMENU || key == VK_RMENU)
            mask = MOD_ALT;
        else if (key == VK_SHIFT || key == VK_LSHIFT || key == VK_RSHIFT)
            mask = MOD_SHIFT;
        if (mask) {
            if (released) modifiers &= ~mask;
            else modifiers |= mask;
            return {0, 0};
        }
        if (key == VK_LWIN || key == VK_RWIN) {
            winHeld = !released;
            return {0, 0};
        }
        if (key == 0 || key == 0xFF) return {0, 0};
        if (released) {
            if (heldKey == key) heldKey = 0;
            return {0, 0};
        }
        if (key == heldKey) return {0, 0};
        heldKey = key;
        return winHeld ? HotkeyChord{0, 0} : HotkeyChord{modifiers, key};
    }
};
struct Settings {
    copyfix::Profile profile = copyfix::Profile::Deep;
    HotkeyChord hotkey;
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
HICON LoadAppIcon(int pixels) noexcept {
    auto* handle = LoadImageW(g_instance, MAKEINTRESOURCEW(IDI_COPYFIX_APP),
                              IMAGE_ICON, pixels, pixels, LR_DEFAULTCOLOR | LR_SHARED);
    return handle ? static_cast<HICON>(handle) : LoadIconW(nullptr, IDI_APPLICATION);
}
HWND g_message = nullptr;
HWND g_preview = nullptr;
HWND g_settings_window = nullptr;
NOTIFYICONDATAW g_tray{};
Settings g_settings;
Pending g_pending;
Undo g_undo;
HANDLE g_instance_mutex = nullptr;
HMODULE g_richedit_module = nullptr;
bool g_show_breaks = false;
bool g_show_spaces = false;
UINT g_hotkey_id = kHotkeyA;
UINT g_taskbar_created = 0;
bool g_hotkey_registered = false;
HotkeyChord g_registered_hotkey;
bool g_settings_raw_input = false;
bool g_settings_conflict_pending = false;
ShortcutCapture g_shortcut_capture;
HotkeyChord g_settings_candidate;
UINT g_settings_pending_key = 0;

int Scale(int px, int dpi) noexcept { return MulDiv(px, dpi, 96); }
HFONT MakeFont(int points, int weight, int dpi) {
    return CreateFontW(-MulDiv(points, dpi, 72), 0, 0, 0, weight,
                       FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                       OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                       DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
}
void ApplyFont(HWND hwnd, HFONT font) {
    if (hwnd && font) SendMessageW(hwnd, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
}
HWND MakeLabel(HWND parent, int id, const wchar_t* text, HFONT font) {
    HWND label = CreateWindowExW(0, L"STATIC", text, WS_CHILD | WS_VISIBLE | SS_LEFT,
                                0, 0, 1, 1, parent,
                                reinterpret_cast<HMENU>(static_cast<UINT_PTR>(id)),
                                g_instance, nullptr);
    ApplyFont(label, font);
    return label;
}
void Position(HWND parent, UINT id, int x, int y, int w, int h, int dpi) {
    if (auto child = GetDlgItem(parent, id))
        MoveWindow(child, Scale(x,dpi), Scale(y,dpi),
                   std::max(1, Scale(w,dpi)), std::max(1, Scale(h,dpi)), TRUE);
}
void DrawActionButton(const DRAWITEMSTRUCT* item) {
    if (!item || item->CtlType != ODT_BUTTON) return;
    const bool primary = item->CtlID == kPreviewAccept || item->CtlID == kSettingsSave;
    const bool pressed = (item->itemState & ODS_SELECTED) != 0;
    const auto fill = CreateSolidBrush(primary ? (pressed ? RGB(29, 78, 216) : kBlue)
                                             : (pressed ? RGB(233, 238, 246) : kSurface));
    const auto pen = CreatePen(PS_SOLID, 1, primary ? kBlue : kBorder);
    HGDIOBJ oldbrush = SelectObject(item->hDC, fill);
    HGDIOBJ oldpen = SelectObject(item->hDC, pen);
    RECT box = item->rcItem;
    RoundRect(item->hDC, box.left, box.top, box.right, box.bottom,
              Scale(12, g_preview_dpi), Scale(12, g_preview_dpi));
    SelectObject(item->hDC, oldbrush);
    SelectObject(item->hDC, oldpen);
    DeleteObject(fill);
    DeleteObject(pen);
    wchar_t text[100]{};
    GetWindowTextW(item->hwndItem, text, static_cast<int>(std::size(text)));
    SetBkMode(item->hDC, TRANSPARENT);
    SetTextColor(item->hDC, primary ? RGB(255,255,255) : kInk);
    const auto priorfont = SelectObject(item->hDC, g_font_label ? g_font_label : GetStockObject(DEFAULT_GUI_FONT));
    DrawTextW(item->hDC, text, -1, &box, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    SelectObject(item->hDC, priorfont);
    if (item->itemState & ODS_FOCUS) {
        RECT focus = box;
        InflateRect(&focus, -Scale(4,g_preview_dpi), -Scale(4,g_preview_dpi));
        DrawFocusRect(item->hDC, &focus);
    }
}



// Windows global shortcuts use modifiers plus exactly one virtual key.
constexpr UINT kHotkeyModifierMask = MOD_ALT | MOD_CONTROL | MOD_SHIFT;
constexpr DWORD kCustomHotkeyFlag = 0x00010000U;

bool IsCommonAppShortcut(HotkeyChord chord) noexcept {
    // Local editor shortcuts cannot be detected by RegisterHotKey. Reject
    // universal editing combinations before attempting global registration.
    if (chord.modifiers == MOD_CONTROL) {
        switch (chord.key) {
        case L'A': case L'C': case L'V': case L'X':
        case L'Z': case L'Y': return true;
        default: break;
        }
    }
    if (chord.modifiers == (MOD_CONTROL | MOD_SHIFT) &&
        chord.key == L'V') return true;
    // Windows uses these for task switching and system window controls.
    if (chord.modifiers == MOD_ALT &&
        (chord.key == VK_TAB || chord.key == VK_ESCAPE ||
         chord.key == VK_F4 || chord.key == VK_SPACE)) return true;
    if (chord.modifiers == (MOD_CONTROL | MOD_SHIFT) &&
        chord.key == VK_ESCAPE) return true;
    return false;
}

bool ValidHotkey(HotkeyChord chord) noexcept {
    if ((chord.modifiers & ~kHotkeyModifierMask) != 0 ||
        (chord.modifiers & (MOD_CONTROL | MOD_ALT)) == 0 ||
        chord.key < VK_BACK || chord.key > 0xFE ||
        IsCommonAppShortcut(chord)) return false;
    switch (chord.key) {
    case VK_SHIFT: case VK_CONTROL: case VK_MENU:
    case VK_LSHIFT: case VK_RSHIFT:
    case VK_LCONTROL: case VK_RCONTROL:
    case VK_LMENU: case VK_RMENU:
    case VK_LWIN: case VK_RWIN:
    case VK_F12: case VK_SNAPSHOT: case VK_PAUSE:
    case VK_CAPITAL: case VK_NUMLOCK: case VK_SCROLL:
        return false;
    default: return true;
    }
}
HotkeyChord DecodeHotkey(DWORD value) noexcept {
    if (value == 0) return {};
    if (value == 1) return {MOD_CONTROL | MOD_SHIFT, VK_F9};
    if (value == 2) return {MOD_CONTROL | MOD_ALT, L'C'};
    if ((value & kCustomHotkeyFlag) == 0 ||
        (value & ~(kCustomHotkeyFlag | 0x000007FFU)) != 0) return {};
    const HotkeyChord candidate{(value >> 8) & 7U, value & 0xFFU};
    return ValidHotkey(candidate) ? candidate : HotkeyChord{};
}
DWORD EncodeHotkey(HotkeyChord chord) noexcept {
    if (chord == HotkeyChord{}) return 0;
    if (chord == HotkeyChord{MOD_CONTROL | MOD_SHIFT, VK_F9}) return 1;
    if (chord == HotkeyChord{MOD_CONTROL | MOD_ALT, L'C'}) return 2;
    return kCustomHotkeyFlag | (chord.modifiers << 8) | chord.key;
}
// Extended virtual keys need the correct scan-code prefix for key names.
bool IsExtendedHotkeyKey(UINT key) noexcept {
    switch (key) {
    case VK_PRIOR: case VK_NEXT: case VK_END: case VK_HOME:
    case VK_LEFT: case VK_UP: case VK_RIGHT: case VK_DOWN:
    case VK_INSERT: case VK_DELETE: case VK_DIVIDE:
        return true;
    default: return false;
    }
}
void FormatHotkey(HotkeyChord chord, wchar_t (&text)[128]) noexcept {
    text[0] = L'\0';
    if (chord.modifiers & MOD_CONTROL) wcscat_s(text, L"Ctrl + ");
    if (chord.modifiers & MOD_ALT) wcscat_s(text, L"Alt + ");
    if (chord.modifiers & MOD_SHIFT) wcscat_s(text, L"Shift + ");
    wchar_t name[64]{};
    if ((chord.key >= L'A' && chord.key <= L'Z') ||
        (chord.key >= L'0' && chord.key <= L'9')) {
        swprintf_s(name, L"%lc", static_cast<wchar_t>(chord.key));
    } else {
        const UINT scan = MapVirtualKeyW(chord.key, MAPVK_VK_TO_VSC);
        const LONG param = static_cast<LONG>((scan & 0xFFU) << 16) |
            (IsExtendedHotkeyKey(chord.key) ? (1L << 24) : 0);
        if (!scan || !GetKeyNameTextW(param, name, static_cast<int>(std::size(name))))
            swprintf_s(name, L"VK %02X", chord.key);
    }
    wcscat_s(text, name);
}

// Per-user settings: no INI, files, elevated privileges, or roaming surprises.
constexpr const wchar_t* kSettingsKey = L"Software\\CopyFix";

void LoadSettings() noexcept {
    HKEY key = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, kSettingsKey, 0, KEY_QUERY_VALUE, &key) != ERROR_SUCCESS)
        return;
    auto read = [key](const wchar_t* name, DWORD fallback) noexcept {
        DWORD value = fallback;
        DWORD type = 0;
        DWORD length = sizeof(value);
        if (RegQueryValueExW(key, name, nullptr, &type,
                             reinterpret_cast<BYTE*>(&value), &length) != ERROR_SUCCESS ||
            type != REG_DWORD || length != sizeof(value)) return fallback;
        return value;
    };
    const DWORD profile = read(L"Profile", static_cast<DWORD>(copyfix::Profile::Deep));
    const DWORD hotkey = read(L"Hotkey", 0);
    if (profile <= 2) g_settings.profile = static_cast<copyfix::Profile>(profile);
    g_settings.hotkey = DecodeHotkey(hotkey);
    RegCloseKey(key);
}

bool SaveSettings() noexcept {
    HKEY key = nullptr;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, kSettingsKey, 0, nullptr,
                        REG_OPTION_NON_VOLATILE, KEY_SET_VALUE, nullptr, &key,
                        nullptr) != ERROR_SUCCESS) return false;
    const DWORD profile = static_cast<DWORD>(g_settings.profile);
    const DWORD hotkey = EncodeHotkey(g_settings.hotkey);
    const bool ok =
        RegSetValueExW(key, L"Profile", 0, REG_DWORD,
                       reinterpret_cast<const BYTE*>(&profile), sizeof(profile)) == ERROR_SUCCESS &&
        RegSetValueExW(key, L"Hotkey", 0, REG_DWORD,
                       reinterpret_cast<const BYTE*>(&hotkey), sizeof(hotkey)) == ERROR_SUCCESS;
    RegCloseKey(key);
    return ok;
}

void Notify(const wchar_t* message) {
    if (!g_tray.hWnd) return;
    g_tray.uFlags = NIF_INFO;
    wcscpy_s(g_tray.szInfoTitle, L"CopyFix");
    wcsncpy_s(g_tray.szInfo, message, _TRUNCATE);
    g_tray.dwInfoFlags = NIIF_INFO | NIIF_NOSOUND;
    Shell_NotifyIconW(NIM_MODIFY, &g_tray);
}

// Probe without touching the active shortcut. A successful probe is not a
// guarantee: another process may reserve the chord before Save.
bool HotkeyAvailable(HotkeyChord chord) noexcept {
    if (!ValidHotkey(chord)) return false;
    if (g_hotkey_registered && chord == g_registered_hotkey) return true;
    if (!g_message ||
        !RegisterHotKey(g_message, kHotkeyProbe, chord.modifiers | MOD_NOREPEAT, chord.key))
        return false;
    return UnregisterHotKey(g_message, kHotkeyProbe) != FALSE;
}

bool RegisterChosenHotkey(HotkeyChord chord) {
    if (!ValidHotkey(chord)) return false;
    if (g_hotkey_registered && chord == g_registered_hotkey) return true;
    const UINT other_id = g_hotkey_id == kHotkeyA ? kHotkeyB : kHotkeyA;
    // Preserve the old global registration until Windows accepts the new one.
    if (!RegisterHotKey(g_message, other_id, chord.modifiers | MOD_NOREPEAT, chord.key))
        return false;
    if (g_hotkey_registered) UnregisterHotKey(g_message, g_hotkey_id);
    g_hotkey_id = other_id;
    g_registered_hotkey = chord;
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
    if (result.sequence == 0) return false;
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

// Compare clipboard-normalized CRLF without allocating a second text buffer.
bool EqualsClipboardLineEndings(std::wstring_view source, std::wstring_view actual) noexcept {
    std::size_t offset = 0;
    for (std::size_t i = 0; i < source.size(); ++i) {
        const auto ch = source[i];
        if (ch == L'\r' || ch == L'\n') {
            if (actual.size() - offset < 2 || actual[offset] != L'\r' ||
                actual[offset + 1] != L'\n') return false;
            offset += 2;
            if (ch == L'\r' && i + 1 < source.size() && source[i + 1] == L'\n') ++i;
        } else if (offset == actual.size() || actual[offset++] != ch) {
            return false;
        }
    }
    return offset == actual.size();
}

void ClearPending() { g_pending = Pending{}; }
void ClearUndo() { g_undo = Undo{}; }

void AcceptPending() {
    try {
        const auto final_text = copyfix::ToWindowsLineEndings(g_pending.after.text);
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

// Read-only native EDIT needs Windows CRLF. LF-only produces visually joined
// lines on Windows even when the transformation preserves all the paragraphs.
void LayoutPreview(HWND window) {
    RECT client{}; GetClientRect(window, &client);
    const int dpi = g_preview_dpi;
    const int w = MulDiv(client.right, 96, dpi);
    const int h = MulDiv(client.bottom, 96, dpi);
    const int margin = 26, gap = 20;
    const int column = std::max(150, (w - 2 * margin - gap) / 2);
    const int edit_y = 146;
    const int edit_height = std::max(100, h - edit_y - 112);
    Position(window, kPreviewTitle, margin, 22, w - 2*margin, 39, dpi);
    Position(window, kPreviewSubtitle, margin, 64, w - 2*margin, 22, dpi);
    Position(window, kPreviewBeforeLabel, margin, 112, column, 28, dpi);
    Position(window, kPreviewAfterLabel, margin + column + gap, 112, column, 28, dpi);
    Position(window, kPreviewBeforeEdit, margin, edit_y, column, edit_height, dpi);
    Position(window, kPreviewAfterEdit, margin + column + gap, edit_y, column, edit_height, dpi);
    Position(window, kPreviewNote, margin, h - 98, std::max(160,w-300), 23, dpi);
    Position(window, kPreviewShowBreaks, margin, h - 70, 220, 25, dpi);
    Position(window, kPreviewShowSpaces, margin + 232, h - 70, 175, 25, dpi);
    Position(window, kPreviewCancel, w - 282, h - 84, 112, 44, dpi);
    Position(window, kPreviewAccept, w - 162, h - 84, 136, 44, dpi);
    InvalidateRect(window, nullptr, TRUE);
}

// RichEdit represents each hard paragraph break as a single CR, unlike
// native EDIT. Diff ranges must use exactly these RichEdit character offsets.
std::wstring MakePreviewText(const std::wstring& source, bool show_breaks) {
    std::wstring result;
    result.reserve(source.size() + (show_breaks ? source.size()/5 : 0));
    for (std::size_t i=0; i<source.size(); ++i) {
        if (source[i] == L'\r' || source[i] == L'\n') {
            if (show_breaks) result += L" \u21B5";
            result += L'\r';
            if (source[i] == L'\r' && i+1 < source.size() && source[i+1] == L'\n') ++i;
        } else result += source[i];
    }
    return result;
}

bool SetPreviewRichText(HWND control, const std::wstring& text) {
    if (!control) return false;
    SendMessageW(control, EM_EXLIMITTEXT, 0, static_cast<LPARAM>(kMaxPreviewChars*3));
    if (!SetWindowTextW(control, text.c_str())) return false;
    SendMessageW(control, EM_SETBKGNDCOLOR, 0, static_cast<LPARAM>(kSurface));
    return true;
}

void MarkChanges(HWND control, const std::vector<copyfix::DiffSpan>& spans, COLORREF background) {
    if (!control) return;
    SendMessageW(control, WM_SETREDRAW, FALSE, 0);
    const auto limit = std::min(spans.size(), static_cast<std::size_t>(240));
    for (std::size_t i=0; i<limit; ++i) {
        CHARRANGE range{static_cast<LONG>(spans[i].begin), static_cast<LONG>(spans[i].end)};
        SendMessageW(control, EM_EXSETSEL, 0, reinterpret_cast<LPARAM>(&range));
        CHARFORMAT2W fmt{};
        fmt.cbSize = sizeof(fmt);
        fmt.dwMask = CFM_BACKCOLOR;
        fmt.crBackColor = background;
        SendMessageW(control, EM_SETCHARFORMAT, SCF_SELECTION, reinterpret_cast<LPARAM>(&fmt));
    }
    CHARRANGE none{0, 0};
    SendMessageW(control, EM_EXSETSEL, 0, reinterpret_cast<LPARAM>(&none));
    SendMessageW(control, WM_SETREDRAW, TRUE, 0);
    InvalidateRect(control, nullptr, TRUE);
}

bool RenderPreviewText(HWND window) {
    const HWND before = GetDlgItem(window, kPreviewBeforeEdit);
    const HWND after = GetDlgItem(window, kPreviewAfterEdit);
    if (!before || !after) return false;
    try {
    // Toggling hard-break markers rebuilds the text in each RichEdit. Preserve
    // the user's independent reading positions rather than jumping to the top.
    POINT before_scroll{}, after_scroll{};
    const bool have_before_scroll = SendMessageW(before, EM_GETSCROLLPOS, 0,
        reinterpret_cast<LPARAM>(&before_scroll)) != 0;
    const bool have_after_scroll = SendMessageW(after, EM_GETSCROLLPOS, 0,
        reinterpret_cast<LPARAM>(&after_scroll)) != 0;
    const auto raw = MakePreviewText(g_pending.before.text, g_show_breaks);
    const auto cleaned = MakePreviewText(g_pending.after.text, g_show_breaks);
    const auto diff = copyfix::ComputePreviewDiff(raw, cleaned);
    // If either control rejects its text (for example under memory pressure),
    // the user must not be offered an incomplete confirmation surface.
    if (!SetPreviewRichText(before, raw) || !SetPreviewRichText(after, cleaned))
        return false;
    // Subtle formatting marks must be painted before stronger content marks.
    if (g_show_spaces) {
        MarkChanges(before, diff.minorRemoved, RGB(255, 246, 243));
        MarkChanges(after, diff.minorAdded, RGB(242, 250, 246));
    }
    MarkChanges(before, diff.removed, RGB(255, 225, 225));
    MarkChanges(after, diff.added, RGB(218, 247, 231));
    if (have_before_scroll) SendMessageW(before, EM_SETSCROLLPOS, 0,
                                          reinterpret_cast<LPARAM>(&before_scroll));
    if (have_after_scroll) SendMessageW(after, EM_SETSCROLLPOS, 0,
                                         reinterpret_cast<LPARAM>(&after_scroll));
    return true;
    } catch (const std::bad_alloc&) {
        return false; // Reject safely; the clipboard is never changed here.
    }
}

LRESULT CALLBACK PreviewProc(HWND window, UINT msg, WPARAM wparam, LPARAM lparam) {
    switch (msg) {
    case WM_CREATE: {
        g_preview_dpi = static_cast<int>(GetDpiForWindow(window));
        const wchar_t* profile = g_settings.profile == copyfix::Profile::Gentle ? L"бережный" :
                                 g_settings.profile == copyfix::Profile::Smart ? L"умный" : L"глубокий";
        MakeLabel(window, kPreviewTitle, L"Проверьте изменения", g_font_title);
        MakeLabel(window, kPreviewSubtitle,
                  L"Розовый: удалено  •  Зелёный: добавлено. Подсветка пробелов доступна отдельно.", g_font_caption);
        MakeLabel(window, kPreviewBeforeLabel, L"ИСХОДНЫЙ ТЕКСТ", g_font_label);
        MakeLabel(window, kPreviewAfterLabel, L"РЕЗУЛЬТАТ ОБРАБОТКИ", g_font_label);
        wchar_t status[128]{};
        std::size_t number = g_pending.after.changes;
        std::size_t digits = 0;
        do {
            status[digits++] = static_cast<wchar_t>(L'0' + number % 10);
            number /= 10;
        } while (number);
        std::reverse(status, status + digits);
        wcscat_s(status, L" изменений  |  Профиль: ");
        wcscat_s(status, profile);
        MakeLabel(window, kPreviewNote, status, g_font_caption);
        const DWORD style = WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_TABSTOP | ES_MULTILINE |
                            ES_AUTOVSCROLL | ES_READONLY | ES_NOHIDESEL;
        HWND before = CreateWindowExW(WS_EX_CLIENTEDGE, MSFTEDIT_CLASS, L"",
                                     style, 0, 0, 1, 1, window,
                                     reinterpret_cast<HMENU>(static_cast<UINT_PTR>(kPreviewBeforeEdit)),
                                     g_instance, nullptr);
        HWND after = CreateWindowExW(WS_EX_CLIENTEDGE, MSFTEDIT_CLASS, L"",
                                    style, 0, 0, 1, 1, window,
                                    reinterpret_cast<HMENU>(static_cast<UINT_PTR>(kPreviewAfterEdit)),
                                    g_instance, nullptr);
        if (!before || !after) return -1;
        ApplyFont(before, g_font_body);
        ApplyFont(after, g_font_body);
        SendMessageW(before, EM_SETMARGINS, EC_LEFTMARGIN | EC_RIGHTMARGIN, MAKELPARAM(12,12));
        SendMessageW(after, EM_SETMARGINS, EC_LEFTMARGIN | EC_RIGHTMARGIN, MAKELPARAM(12,12));
        HWND toggle = CreateWindowExW(0, L"BUTTON", L"↵ Показать переносы строк",
                                      WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX,
                                      0,0,1,1,window,
                                      reinterpret_cast<HMENU>(static_cast<UINT_PTR>(kPreviewShowBreaks)),
                                      g_instance,nullptr);
        ApplyFont(toggle, g_font_caption);
        HWND spaces = CreateWindowExW(0, L"BUTTON", L"Подсветка пробелов",
                                    WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX,
                                    0,0,1,1,window,
                                    reinterpret_cast<HMENU>(static_cast<UINT_PTR>(kPreviewShowSpaces)),
                                    g_instance,nullptr);
        ApplyFont(spaces, g_font_caption);
        g_show_breaks = false;
        g_show_spaces = false;
        if (!RenderPreviewText(window)) return -1;
        CreateWindowW(L"BUTTON", L"Применить", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,
                      0,0,1,1,window,reinterpret_cast<HMENU>(static_cast<UINT_PTR>(kPreviewAccept)),g_instance,nullptr);
        CreateWindowW(L"BUTTON", L"Отмена", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,
                      0,0,1,1,window,reinterpret_cast<HMENU>(static_cast<UINT_PTR>(kPreviewCancel)),g_instance,nullptr);
        LayoutPreview(window);
        return 0;
    }
    case WM_SIZE: LayoutPreview(window); return 0;
    case WM_DPICHANGED: {
        auto* suggested = reinterpret_cast<const RECT*>(lparam);
        SetWindowPos(window, nullptr, suggested->left, suggested->top,
                     suggested->right - suggested->left, suggested->bottom - suggested->top,
                     SWP_NOZORDER | SWP_NOACTIVATE);
        return 0;
    }
    case WM_CTLCOLORSTATIC: {
        auto dc = reinterpret_cast<HDC>(wparam);
        const UINT id = static_cast<UINT>(GetDlgCtrlID(reinterpret_cast<HWND>(lparam)));
        SetBkMode(dc, TRANSPARENT);
        SetTextColor(dc, id == kPreviewTitle || id == kPreviewBeforeLabel ||
                         id == kPreviewAfterLabel ? kInk : kMuted);
        return reinterpret_cast<LRESULT>(g_canvas_brush);
    }
    case WM_DRAWITEM: DrawActionButton(reinterpret_cast<const DRAWITEMSTRUCT*>(lparam)); return TRUE;
    case WM_PAINT: {
        PAINTSTRUCT ps{}; const auto dc = BeginPaint(window, &ps);
        RECT bounds{}; GetClientRect(window, &bounds);
        RECT line{Scale(26,g_preview_dpi), Scale(98,g_preview_dpi),
                  bounds.right - Scale(26,g_preview_dpi), Scale(99,g_preview_dpi)};
        HBRUSH border = CreateSolidBrush(kBorder); FillRect(dc, &line, border); DeleteObject(border);
        EndPaint(window, &ps);
        return 0;
    }
    case WM_GETMINMAXINFO: {
        auto* bounds = reinterpret_cast<MINMAXINFO*>(lparam);
        bounds->ptMinTrackSize = POINT{Scale(760,g_preview_dpi), Scale(510,g_preview_dpi)};
        return 0;
    }
    case WM_COMMAND: {
        const auto id = LOWORD(wparam);
        const auto notification = HIWORD(wparam);
        const auto from = reinterpret_cast<HWND>(lparam);
        // Multiline EDIT sends EN_VSCROLL/EN_HSCROLL as WM_COMMAND. Never
        // confuse child notifications with the standard IDCANCEL command (2).
        if (notification == BN_CLICKED && from && IsWindow(from)) {
            if (id == kPreviewShowSpaces && from == GetDlgItem(window, kPreviewShowSpaces)) {
                g_show_spaces = SendMessageW(from, BM_GETCHECK, 0, 0) == BST_CHECKED;
                if (!RenderPreviewText(window)) {
                    DestroyWindow(window);
                    Notify(L"Недостаточно памяти для предпросмотра. Буфер не изменён.");
                }
                return 0;
            }
            if (id == kPreviewShowBreaks && from == GetDlgItem(window, kPreviewShowBreaks)) {
                g_show_breaks = SendMessageW(from, BM_GETCHECK, 0, 0) == BST_CHECKED;
                if (!RenderPreviewText(window)) {
                    DestroyWindow(window);
                    Notify(L"Недостаточно памяти для предпросмотра. Буфер не изменён.");
                }
                return 0;
            }
            if (id == kPreviewAccept && from == GetDlgItem(window, kPreviewAccept)) {
                AcceptPending(); DestroyWindow(window); return 0;
            }
            if (id == kPreviewCancel && from == GetDlgItem(window, kPreviewCancel)) {
                DestroyWindow(window); return 0;
            }
        }
        // IsDialogMessage posts a synthetic ID=IDCANCEL with no source HWND.
        if (id == IDCANCEL && notification == 0 && !from) {
            DestroyWindow(window); return 0;
        }
        break;
    }
    case WM_CLOSE: DestroyWindow(window); return 0;
    case WM_DESTROY: g_preview = nullptr; ClearPending(); return 0;
    }
    return DefWindowProcW(window, msg, wparam, lparam);
}

void OpenPreview() {
    if (g_preview) { SetForegroundWindow(g_preview); return; }
    g_preview = CreateWindowExW(WS_EX_APPWINDOW, L"CopyFixPreview", L"CopyFix - проверка текста",
        WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN, CW_USEDEFAULT, CW_USEDEFAULT,
        Scale(1040,96), Scale(690,96), nullptr, nullptr, g_instance, nullptr);
    if (!g_preview) {
        ClearPending();
        Notify(L"Не удалось открыть предпросмотр.");
        return;
    }
    POINT cursor{};
    const HMONITOR monitor = GetCursorPos(&cursor)
        ? MonitorFromPoint(cursor, MONITOR_DEFAULTTONEAREST)
        : MonitorFromWindow(g_message, MONITOR_DEFAULTTONEAREST);
    CenterWindowOnMonitor(g_preview, monitor);
    ShowWindow(g_preview, SW_SHOWNORMAL);
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
    if (cleaned.text == snapshot.text || EqualsClipboardLineEndings(cleaned.text, snapshot.text)) {
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

void SetShortcutEditor(HWND window, HotkeyChord chord) noexcept {
    wchar_t label[128]{};
    FormatHotkey(chord, label);
    if (HWND input = GetDlgItem(window, kHotkeyInput))
        SetWindowTextW(input, label);
}

void ResetShortcutEditor(HWND window) noexcept {
    g_settings_candidate = g_settings.hotkey;
    SetShortcutEditor(window, g_settings_candidate);
}

void ShowShortcutWarning(HWND window, bool occupied) noexcept {
    if (!IsWindow(window)) return;
    ResetShortcutEditor(window);
    const wchar_t* message = occupied
        ? L"Сочетание уже занято другой программой или зарезервировано Windows. "
          L"Прежняя комбинация сохранена."
        : L"Эта комбинация недоступна для CopyFix. "
          L"Используйте Ctrl или Alt и другую клавишу, не занятую стандартными командами.";
    if (HWND status = GetDlgItem(window, kSettingsFeedback))
        SetWindowTextW(status, message);
    MessageBoxW(window, message, L"CopyFix — горячая клавиша", MB_OK | MB_ICONWARNING);
    // Modal dialogs consume key-up messages, so reset both capture paths.
    g_shortcut_capture.Reset();
    g_settings_pending_key = 0;
    ResetShortcutEditor(window);
}

bool AcceptShortcutCandidate(HWND window, HotkeyChord candidate) noexcept {
    if (!ValidHotkey(candidate)) return false;
    if (!HotkeyAvailable(candidate)) return false;
    g_settings_candidate = candidate;
    SetShortcutEditor(window, candidate);
    if (HWND status = GetDlgItem(window, kSettingsFeedback))
        SetWindowTextW(status, L"");
    return true;
}

// Both physical Raw Input and normal EDIT messages feed the same transaction.
// Native EDIT key messages provide a reliable path even if Raw Input is unavailable.
void ProcessShortcutCandidate(HWND window, HotkeyChord chord) noexcept {
    if (!chord.key || g_settings_conflict_pending ||
        g_settings_pending_key == chord.key) return;
    g_settings_pending_key = chord.key;
    if (ValidHotkey(chord) && AcceptShortcutCandidate(window, chord)) return;
    if ((chord.modifiers & (MOD_CONTROL | MOD_ALT)) == 0) {
        g_settings_pending_key = 0;
        if (HWND status = GetDlgItem(window, kSettingsFeedback))
            SetWindowTextW(status, L"Для сочетания удерживайте Ctrl или Alt.");
        return;
    }
    g_settings_conflict_pending = PostMessageW(
        window, kSettingsConflictMessage, ValidHotkey(chord) ? 1 : 0, 0) != FALSE;
}

void ObserveShortcutKey(HWND window, const RAWKEYBOARD& keyboard) noexcept {
    if (GetForegroundWindow() != window ||
        GetFocus() != GetDlgItem(window, kHotkeyInput)) {
        g_shortcut_capture.Reset();
        g_settings_pending_key = 0;
        return;
    }
    if ((keyboard.Flags & RI_KEY_BREAK) != 0 &&
        keyboard.VKey == g_settings_pending_key)
        g_settings_pending_key = 0;
    const HotkeyChord chord = g_shortcut_capture.Consume(
        keyboard.VKey, (keyboard.Flags & RI_KEY_BREAK) != 0);
    ProcessShortcutCandidate(window, chord);
}

UINT KeyboardModifiers() noexcept {
    UINT modifiers = 0;
    if (GetKeyState(VK_CONTROL) & 0x8000) modifiers |= MOD_CONTROL;
    if (GetKeyState(VK_MENU) & 0x8000) modifiers |= MOD_ALT;
    if (GetKeyState(VK_SHIFT) & 0x8000) modifiers |= MOD_SHIFT;
    return modifiers;
}

// Subclassing only this single EDIT avoids global hooks and captures ordinary
// virtual keys even when IsDialogMessage is translating dialog navigation.
LRESULT CALLBACK ShortcutEditorProc(HWND input, UINT msg, WPARAM wparam,
                                     LPARAM lparam, UINT_PTR, DWORD_PTR) {
    switch (msg) {
    case WM_GETDLGCODE:
        return DefSubclassProc(input, msg, wparam, lparam) | DLGC_WANTALLKEYS;
    case WM_KEYDOWN:
    case WM_SYSKEYDOWN:
        if (wparam >= VK_BACK && wparam <= 0xFE) {
            const UINT key = static_cast<UINT>(wparam);
            if (key != VK_CONTROL && key != VK_MENU && key != VK_SHIFT &&
                key != VK_LCONTROL && key != VK_RCONTROL &&
                key != VK_LMENU && key != VK_RMENU &&
                key != VK_LSHIFT && key != VK_RSHIFT) {
                PostMessageW(GetParent(input), kSettingsNormalKeyMessage,
                             KeyboardModifiers(), key);
            }
        }
        return 0;
    case WM_KEYUP:
    case WM_SYSKEYUP:
        if (g_settings_pending_key == wparam)
            g_settings_pending_key = 0;
        return 0;
    case WM_CHAR:
    case WM_SYSCHAR:
        return 0;
    case WM_NCDESTROY:
        RemoveWindowSubclass(input, ShortcutEditorProc, 1);
        break;
    }
    return DefSubclassProc(input, msg, wparam, lparam);
}

void LayoutSettings(HWND window) {
    RECT bounds{}; GetClientRect(window, &bounds);
    const int dpi = g_settings_dpi;
    const int w = MulDiv(bounds.right, 96, dpi);
    const int h = MulDiv(bounds.bottom, 96, dpi);
    Position(window, kSettingsTitle, 26, 22, w-52, 38, dpi);
    Position(window, kSettingsSubtitle, 26, 64, w-52, 40, dpi);
    Position(window, 420, 26, 124, w-52, 24, dpi);
    Position(window, kHotkeyInput, 26, 154, w-52, 34, dpi);
    Position(window, 421, 26, 222, w-52, 24, dpi);
    Position(window, kComboProfile, 26, 252, w-52, 250, dpi);
    Position(window, kSettingsFeedback, 26, h-113, w-52, 46, dpi);
    Position(window, kSettingsCancel, w-286, h-64, 122, 42, dpi);
    Position(window, kSettingsSave, w-156, h-64, 130, 42, dpi);
}

LRESULT CALLBACK SettingsProc(HWND window, UINT msg, WPARAM wparam, LPARAM lparam) {
    switch (msg) {
    case WM_CREATE: {
        g_settings_dpi = static_cast<int>(GetDpiForWindow(window));
        MakeLabel(window,kSettingsTitle,L"Настройки",g_font_title);
        MakeLabel(window,kSettingsSubtitle,
                  L"Нажмите сочетание в поле. Ctrl или Alt обязательны; Shift — дополнительно.",
                  g_font_caption);
        MakeLabel(window,420,L"ГОРЯЧАЯ КЛАВИША",g_font_label);
        HWND hotkey = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", nullptr,
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL | ES_READONLY,
            0, 0, 1, 1, window,
            reinterpret_cast<HMENU>(static_cast<UINT_PTR>(kHotkeyInput)), g_instance, nullptr);
        if (!hotkey || !SetWindowSubclass(hotkey, ShortcutEditorProc, 1, 0)) return -1;
        g_settings_candidate = g_settings.hotkey;
        SetShortcutEditor(window, g_settings_candidate);
        ApplyFont(hotkey,g_font_body);
        MakeLabel(window,421,L"ПРОФИЛЬ ОБРАБОТКИ",g_font_label);
        HWND profile=CreateWindowW(L"COMBOBOX",nullptr,WS_CHILD | WS_VISIBLE | WS_TABSTOP |
            CBS_DROPDOWNLIST | WS_VSCROLL,0,0,1,1,window,
            reinterpret_cast<HMENU>(static_cast<UINT_PTR>(kComboProfile)),g_instance,nullptr);
        for(const auto* name:{L"Бережный",L"Умный",L"Глубокий"})
            SendMessageW(profile,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(name));
        SendMessageW(profile,CB_SETCURSEL,static_cast<WPARAM>(g_settings.profile),0);
        ApplyFont(profile,g_font_body);
        MakeLabel(window,kSettingsFeedback,L"",g_font_caption);
        CreateWindowW(L"BUTTON",L"Сохранить",WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,
            0,0,1,1,window,reinterpret_cast<HMENU>(static_cast<UINT_PTR>(kSettingsSave)),g_instance,nullptr);
        CreateWindowW(L"BUTTON",L"Отмена",WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,
            0,0,1,1,window,reinterpret_cast<HMENU>(static_cast<UINT_PTR>(kSettingsCancel)),g_instance,nullptr);
        LayoutSettings(window);
        return 0;
    }
    case WM_SIZE: LayoutSettings(window); return 0;
    case WM_INPUT: {
        RAWINPUT input{};
        UINT size = sizeof(input);
        const UINT got = GetRawInputData(reinterpret_cast<HRAWINPUT>(lparam),
            RID_INPUT, &input, &size, sizeof(RAWINPUTHEADER));
        if (got != static_cast<UINT>(-1) && got == size &&
            input.header.dwType == RIM_TYPEKEYBOARD)
            ObserveShortcutKey(window, input.data.keyboard);
        return DefWindowProcW(window, msg, wparam, lparam);
    }
    case kSettingsConflictMessage:
        if (g_settings_conflict_pending) {
            g_settings_conflict_pending = false;
            ShowShortcutWarning(window, wparam != 0);
        }
        return 0;
    case kSettingsNormalKeyMessage:
        if (GetForegroundWindow() == window &&
            GetFocus() == GetDlgItem(window, kHotkeyInput))
            ProcessShortcutCandidate(window,
                {static_cast<UINT>(wparam), static_cast<UINT>(lparam)});
        return 0;
    case WM_ACTIVATE:
        if (LOWORD(wparam) == WA_INACTIVE) {
            g_shortcut_capture.Reset();
            g_settings_pending_key = 0;
        }
        break;
    case WM_CTLCOLORSTATIC: {
        auto dc = reinterpret_cast<HDC>(wparam);
        SetBkMode(dc,TRANSPARENT);
        const UINT id = static_cast<UINT>(GetDlgCtrlID(reinterpret_cast<HWND>(lparam)));
        SetTextColor(dc,id==kSettingsFeedback ? RGB(172,38,38) :
            (id==kSettingsTitle||id==420||id==421 ? kInk : kMuted));
        return reinterpret_cast<LRESULT>(g_canvas_brush);
    }
    case WM_DRAWITEM: DrawActionButton(reinterpret_cast<const DRAWITEMSTRUCT*>(lparam));return TRUE;
    case WM_COMMAND:
        if (LOWORD(wparam) == kHotkeyInput &&
            (HIWORD(wparam) == EN_SETFOCUS || HIWORD(wparam) == EN_KILLFOCUS)) {
            g_shortcut_capture.Reset();
            g_settings_pending_key = 0;
            return 0;
        }
        if (LOWORD(wparam) == kSettingsSave) {
            const HotkeyChord next_hotkey = g_settings_candidate;
            const int next_profile = static_cast<int>(SendMessageW(GetDlgItem(window, kComboProfile), CB_GETCURSEL, 0, 0));
            if (next_profile < 0 || next_profile > 2) return 0;
            if (!ValidHotkey(next_hotkey)) {
                MessageBoxW(window,
                    L"Комбинация недопустима: выберите Ctrl или Alt с другой клавишей. "
                    L"Стандартные команды (например, Ctrl+V), Win, F12 и кнопки мыши недоступны.",
                    L"CopyFix", MB_OK | MB_ICONWARNING);
                return 0;
            }
            if (!RegisterChosenHotkey(next_hotkey)) {
                ShowShortcutWarning(window, true);
                return 0;
            }
            g_settings.hotkey = next_hotkey;
            g_settings.profile = static_cast<copyfix::Profile>(next_profile);
            if (!SaveSettings())
                MessageBoxW(window,L"Не удалось сохранить параметры в HKCU. Они действуют до выхода из CopyFix.",
                            L"CopyFix",MB_OK | MB_ICONWARNING);
            DestroyWindow(window);
            return 0;
        }
        if (LOWORD(wparam) == kSettingsCancel || LOWORD(wparam) == IDCANCEL) {
            DestroyWindow(window); return 0;
        }
        break;
    case WM_CLOSE: DestroyWindow(window); return 0;
    case WM_DESTROY: {
        if (g_settings_raw_input) {
            RAWINPUTDEVICE remove{0x01, 0x06, RIDEV_REMOVE, nullptr};
            RegisterRawInputDevices(&remove, 1, sizeof(remove));
            g_settings_raw_input = false;
        }
        g_settings_conflict_pending = false;
        g_shortcut_capture.Reset();
        g_settings_pending_key = 0;
        g_settings_window = nullptr;
        return 0;
    }
    }
    return DefWindowProcW(window,msg,wparam,lparam);
}

void OpenSettings() {
    if (g_settings_window) { SetForegroundWindow(g_settings_window); return; }
    g_settings_window = CreateWindowExW(WS_EX_APPWINDOW,L"CopyFixSettings",L"CopyFix - настройки",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
        CW_USEDEFAULT,CW_USEDEFAULT,Scale(510,96),Scale(468,96),
        nullptr,nullptr,g_instance,nullptr);
    if (!g_settings_window) return;
    POINT cursor{};
    const HMONITOR monitor = GetCursorPos(&cursor)
        ? MonitorFromPoint(cursor, MONITOR_DEFAULTTONEAREST)
        : MonitorFromWindow(g_message, MONITOR_DEFAULTTONEAREST);
    CenterWindowOnMonitor(g_settings_window, monitor);
    g_settings_conflict_pending = false;
    g_shortcut_capture.Reset();
    g_settings_pending_key = 0;
    // Receive occupied combinations as well as ordinary focus-owned key events.
    // Only process Raw Input while Settings and its editor are foreground/focused.
    const RAWINPUTDEVICE keyboard{0x01, 0x06, RIDEV_INPUTSINK, g_settings_window};
    g_settings_raw_input = RegisterRawInputDevices(&keyboard, 1, sizeof(keyboard)) != FALSE;
    ShowWindow(g_settings_window, SW_SHOWNORMAL);
    SetForegroundWindow(g_settings_window);
    SetFocus(GetDlgItem(g_settings_window, kHotkeyInput));
    if (!g_settings_raw_input) {
        SetWindowTextW(GetDlgItem(g_settings_window, kSettingsFeedback),
                       L"Можно вводить сочетания. Проверка перехваченных комбинаций недоступна.");
    }
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
        g_tray.hIcon = LoadAppIcon(GetSystemMetrics(SM_CXSMICON));
        wcscpy_s(g_tray.szTip, L"CopyFix - очистка текста");
        Shell_NotifyIconW(NIM_ADD, &g_tray);
        g_tray.uVersion = NOTIFYICON_VERSION_4;
        Shell_NotifyIconW(NIM_SETVERSION, &g_tray);
        if (!hotkey_ok) Notify(L"Сочетание занято. Откройте настройки CopyFix.");
        return 0;
    }
    case WM_HOTKEY:
        if (wparam == g_hotkey_id) {
            // Shortcut capture must not accidentally process the clipboard.
            if (!g_settings_window || GetForegroundWindow() != g_settings_window)
                FixClipboard(false);
            return 0;
        }
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
            if (!SaveSettings()) Notify(L"Не удалось сохранить профиль в реестре HKCU.");
            break;
        case kMenuSettings: OpenSettings(); break;
        case kMenuAbout: ShowAbout(window); break;
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
    // Native high-DPI layout, with a graceful fallback on older Windows 10.
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
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
     g_richedit_module = LoadLibraryW(L"Msftedit.dll");
    if (!g_richedit_module) {
        MessageBoxW(nullptr, L"Не удалось загрузить системный редактор Msftedit.dll.",
                    L"CopyFix", MB_ICONERROR | MB_OK);
        CloseHandle(g_instance_mutex);
        return 1;
    }
    g_canvas_brush = CreateSolidBrush(kCanvas);
    g_surface_brush = CreateSolidBrush(kSurface);
    const int dpi = static_cast<int>(GetDpiForSystem());
    g_font_body = MakeFont(11, FW_NORMAL, dpi);
    g_font_title = MakeFont(22, FW_SEMIBOLD, dpi);
    g_font_label = MakeFont(10, FW_SEMIBOLD, dpi);
    g_font_caption = MakeFont(10, FW_NORMAL, dpi);
    g_taskbar_created = RegisterWindowMessageW(L"TaskbarCreated");
    const HICON large_icon = LoadAppIcon(GetSystemMetrics(SM_CXICON));
    const HICON small_icon = LoadAppIcon(GetSystemMetrics(SM_CXSMICON));
    const WNDCLASSEXW classes[] = {
        {sizeof(WNDCLASSEXW), 0, MessageProc, 0, 0, instance, large_icon, nullptr,
            nullptr, nullptr, L"CopyFixMessage", small_icon},
        {sizeof(WNDCLASSEXW), 0, PreviewProc, 0, 0, instance, large_icon, nullptr,
            g_canvas_brush, nullptr, L"CopyFixPreview", small_icon},
        {sizeof(WNDCLASSEXW), 0, SettingsProc, 0, 0, instance, large_icon, nullptr,
            g_canvas_brush, nullptr, L"CopyFixSettings", small_icon}
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
        if (g_preview && IsDialogMessageW(g_preview, &message)) continue;
        if (g_settings_window && IsDialogMessageW(g_settings_window, &message)) continue;
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
    if (g_font_body) DeleteObject(g_font_body);
    if (g_font_title) DeleteObject(g_font_title);
    if (g_font_label) DeleteObject(g_font_label);
    if (g_font_caption) DeleteObject(g_font_caption);
    if (g_canvas_brush) DeleteObject(g_canvas_brush);
    if (g_surface_brush) DeleteObject(g_surface_brush);
    if (g_richedit_module) FreeLibrary(g_richedit_module);
    CloseHandle(g_instance_mutex);
    return static_cast<int>(message.wParam);
}
