#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <shellapi.h>
#include <commctrl.h>
#include <richedit.h>
#include "copyfix_version.h"
#include "diff_spans.h"
#include "resource_ids.h"
#include "ui_theme.h"
#include <shellscalingapi.h>

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
constexpr UINT kSettingsCtrl = 404;
constexpr UINT kSettingsAlt = 405;
constexpr UINT kSettingsShift = 406;
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
constexpr UINT kSettingsExplanation = 422;
constexpr UINT kPreviewTitle = 410;
constexpr UINT kPreviewSubtitle = 411;
constexpr UINT kPreviewBeforeLabel = 412;
constexpr UINT kPreviewAfterLabel = 413;
constexpr UINT kPreviewNote = 414;
constexpr UINT kSettingsTitle = 415;
constexpr UINT kSettingsSubtitle = 416;
constexpr UINT kSettingsFeedback = 417;
constexpr UINT kProfileGentle = 430;
constexpr UINT kProfileSmart = 431;
constexpr UINT kProfileDeep = 432;
constexpr UINT kAboutIcon = 440;
constexpr UINT kAboutTitle = 441;
constexpr UINT kAboutVersionLabel = 442;
constexpr UINT kAboutDescriptionLabel = 443;
constexpr UINT kAboutProject = 444;
constexpr UINT kAboutGitHub = 445;
constexpr UINT kAboutLicense = 446;

constexpr wchar_t kAboutVersionString[] = L"v" COPYFIX_VERSION_WSTR;
constexpr wchar_t kAboutWebsite[] = L"https://stanleyll0yd.github.io/apps/copyfix/";
constexpr wchar_t kAboutGitHubUrl[] = L"https://github.com/StanleyLl0yd/copyfix";

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

void ShowAbout(HWND parent) noexcept;

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
HWND g_about_window = nullptr;
copyfix::Profile g_draft_profile = copyfix::Profile::Deep;
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
// Injectable only for deterministic window tests; production uses MessageBoxW.
auto g_hotkey_warning_dialog = &MessageBoxW;
int Scale(int px, int dpi) noexcept { return MulDiv(px, dpi, 96); }

struct SpawnRect {
    HMONITOR monitor = nullptr;
    int x = CW_USEDEFAULT;
    int y = CW_USEDEFAULT;
    int width = 0;
    int height = 0;
};

SpawnRect PositionedOnCursorMonitor(int widthDip, int heightDip, HWND fallback) noexcept {
    POINT cursor{};
    const HMONITOR monitor = GetCursorPos(&cursor)
        ? MonitorFromPoint(cursor,MONITOR_DEFAULTTONEAREST)
        : MonitorFromWindow(fallback,MONITOR_DEFAULTTONEAREST);
    UINT dpiX = 96, dpiY = 96;
    if (!monitor || FAILED(GetDpiForMonitor(monitor,MDT_EFFECTIVE_DPI,&dpiX,&dpiY)))
        dpiX = GetDpiForSystem();
    const int width = Scale(widthDip,static_cast<int>(dpiX));
    const int height = Scale(heightDip,static_cast<int>(dpiX));
    MONITORINFO info{sizeof(info)};
    if (!monitor || !GetMonitorInfoW(monitor,&info))
        return {monitor,CW_USEDEFAULT,CW_USEDEFAULT,width,height};
    const int spaceX = info.rcWork.right-info.rcWork.left-width;
    const int spaceY = info.rcWork.bottom-info.rcWork.top-height;
    return {monitor,info.rcWork.left+std::max(0,spaceX/2),
            info.rcWork.top+std::max(0,spaceY/2),width,height};
}


bool RenderPreviewText(HWND window);

void UpdateWindowTheme(HWND window) noexcept {
    if (!window) return;
    copyfix::ui::ThemeWindow(window);
    InvalidateRect(window, nullptr, TRUE);
    for (HWND child = GetWindow(window, GW_CHILD); child;
         child = GetWindow(child, GW_HWNDNEXT))
        InvalidateRect(child, nullptr, TRUE);
}

void SynchronizeSystemTheme() noexcept {
    const bool shouldDark = copyfix::ui::ReadSystemDark();
    if (shouldDark == copyfix::ui::dark) return;
    copyfix::ui::SetDark(shouldDark);
    g_canvas_brush = copyfix::ui::CanvasBrush();
    g_surface_brush = copyfix::ui::SurfaceBrush();
    UpdateWindowTheme(g_settings_window);
    UpdateWindowTheme(g_preview);
    UpdateWindowTheme(g_about_window);
    if (g_preview) RenderPreviewText(g_preview);
}

void InitWindowTheme(HWND window) noexcept {
    copyfix::ui::ThemeWindow(window);
}

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
void DrawNativeCheckBox(HWND window, HDC dc) noexcept {
    RECT box{}; GetClientRect(window,&box);
    FillRect(dc,&box,g_surface_brush ? g_surface_brush
                                    : static_cast<HBRUSH>(GetStockObject(WHITE_BRUSH)));
    const auto& c = copyfix::ui::colors;
    const int dpi = static_cast<int>(GetDpiForWindow(window));
    const int tick = Scale(17,dpi);
    const int left = Scale(3,dpi);
    const int y = (box.bottom-tick)/2;
    const bool checked = SendMessageW(window,BM_GETCHECK,0,0) == BST_CHECKED;
    const bool enabled = IsWindowEnabled(window) != FALSE;
    copyfix::ui::Rounded(dc,RECT{left,y,left+tick,y+tick},
        checked ? c.blue : c.surface,
        checked ? c.blue : c.border,Scale(5,dpi));
    if (checked) {
        HPEN mark = CreatePen(PS_SOLID,std::max(1,Scale(2,dpi)),RGB(255,255,255));
        if (mark) {
            const auto old = SelectObject(dc,mark);
            MoveToEx(dc,left+tick/5,y+tick/2,nullptr);
            LineTo(dc,left+tick*2/5,y+tick*3/4);
            LineTo(dc,left+tick*4/5,y+tick/4);
            SelectObject(dc,old);
            DeleteObject(mark);
        }
    }
    wchar_t label[100]{};
    GetWindowTextW(window,label,static_cast<int>(std::size(label)));
    SetBkMode(dc,TRANSPARENT);
    SetTextColor(dc,enabled ? c.ink : c.muted);
    const HGDIOBJ oldFont = SelectObject(dc,
        g_font_body ? g_font_body : GetStockObject(DEFAULT_GUI_FONT));
    RECT textBox{left+tick+Scale(8,dpi),0,box.right,box.bottom};
    DrawTextW(dc,label,-1,&textBox,DT_LEFT|DT_VCENTER|DT_SINGLELINE);
    SelectObject(dc,oldFont);
    if (GetFocus() == window) {
        InflateRect(&textBox,-1,-Scale(4,dpi));
        DrawFocusRect(dc,&textBox);
    }
}

LRESULT CALLBACK CheckBoxSubclass(HWND window,UINT msg,WPARAM wparam,
                                  LPARAM lparam,UINT_PTR id,DWORD_PTR) {
    switch (msg) {
    case WM_ERASEBKGND: return 1;
    case WM_PAINT: {
        PAINTSTRUCT ps{};
        HDC dc = BeginPaint(window,&ps);
        DrawNativeCheckBox(window,dc);
        EndPaint(window,&ps);
        return 0;
    }
    case WM_PRINTCLIENT:
        DrawNativeCheckBox(window,reinterpret_cast<HDC>(wparam));
        return 0;
    case WM_SETFOCUS:
    case WM_KILLFOCUS:
    case WM_ENABLE: {
        const LRESULT result = DefSubclassProc(window,msg,wparam,lparam);
        InvalidateRect(window,nullptr,TRUE);
        return result;
    }
    case WM_NCDESTROY:
        RemoveWindowSubclass(window,CheckBoxSubclass,id);
        break;
    }
    return DefSubclassProc(window,msg,wparam,lparam);
}

bool StyleCheckBox(HWND window) noexcept {
    return window && SetWindowSubclass(window,CheckBoxSubclass,1,0) != FALSE;
}

void DrawNativeCombo(const DRAWITEMSTRUCT* item) noexcept {
    if (!item || item->CtlType != ODT_COMBOBOX) return;
    const auto& c = copyfix::ui::colors;
    const bool selected = (item->itemState & ODS_SELECTED) != 0;
    const COLORREF fill = selected ? c.selected : c.surface;
    HBRUSH brush = CreateSolidBrush(fill);
    if (!brush) return;
    FillRect(item->hDC,&item->rcItem,brush);
    DeleteObject(brush);
    if (item->itemID != static_cast<UINT>(-1)) {
        wchar_t value[80]{};
        SendMessageW(item->hwndItem,CB_GETLBTEXT,item->itemID,
                     reinterpret_cast<LPARAM>(value));
        SetBkMode(item->hDC,TRANSPARENT);
        SetTextColor(item->hDC,c.ink);
        const auto old = SelectObject(item->hDC,
            g_font_body ? g_font_body : GetStockObject(DEFAULT_GUI_FONT));
        RECT text = item->rcItem;
        text.left += Scale(11,static_cast<int>(GetDpiForWindow(item->hwndItem)));
        DrawTextW(item->hDC,value,-1,&text,
                  DT_LEFT|DT_SINGLELINE|DT_VCENTER);
        SelectObject(item->hDC,old);
        if (item->itemState & ODS_FOCUS) DrawFocusRect(item->hDC,&item->rcItem);
    }
}

void DrawActionButton(const DRAWITEMSTRUCT* item) {
    if (!item) return;
    if (item->CtlType == ODT_COMBOBOX) {
        DrawNativeCombo(item);
        return;
    }
    if (item->CtlType != ODT_BUTTON) return;
    const auto& c = copyfix::ui::colors;
    const bool primary = item->CtlID == kPreviewAccept || item->CtlID == kSettingsSave;
    const bool segment = item->CtlID >= kProfileGentle && item->CtlID <= kProfileDeep;
    const bool selected = segment &&
        item->CtlID == kProfileGentle + static_cast<UINT>(g_draft_profile);
    const bool pressed = (item->itemState & ODS_SELECTED) != 0;
    const COLORREF fill = (primary || selected) ? (pressed ? RGB(16,78,190) : c.blue)
                                              : (pressed ? c.raised : c.surface);
    const COLORREF border = (primary || selected) ? c.blue : c.border;
    const int dpi = static_cast<int>(GetDpiForWindow(item->hwndItem));
    copyfix::ui::Rounded(item->hDC, item->rcItem, fill, border, Scale(13,dpi));
    wchar_t text[100]{};
    GetWindowTextW(item->hwndItem, text, static_cast<int>(std::size(text)));
    SetBkMode(item->hDC, TRANSPARENT);
    SetTextColor(item->hDC, (primary || selected) ? RGB(255,255,255) : c.ink);
    const auto old = SelectObject(item->hDC,
        g_font_label ? g_font_label : GetStockObject(DEFAULT_GUI_FONT));
    RECT label = item->rcItem;
    DrawTextW(item->hDC, text, -1, &label, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    SelectObject(item->hDC, old);
    if (item->itemState & ODS_FOCUS) {
        RECT focus = item->rcItem;
        InflateRect(&focus, -Scale(4,dpi), -Scale(4,dpi));
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

struct HotkeyKeyItem { const wchar_t* name; UINT key; };

constexpr HotkeyKeyItem kSpecialHotkeyKeys[] = {
    {L"Backspace", VK_BACK}, {L"Tab", VK_TAB}, {L"Enter", VK_RETURN},
    {L"Space", VK_SPACE}, {L"Esc", VK_ESCAPE},
    {L"Home", VK_HOME}, {L"End", VK_END},
    {L"Page Up", VK_PRIOR}, {L"Page Down", VK_NEXT},
    {L"Insert", VK_INSERT}, {L"Delete", VK_DELETE},
    {L"Left", VK_LEFT}, {L"Right", VK_RIGHT},
    {L"Up", VK_UP}, {L"Down", VK_DOWN},
    {L"Numpad 0", VK_NUMPAD0}, {L"Numpad 1", VK_NUMPAD1},
    {L"Numpad 2", VK_NUMPAD2}, {L"Numpad 3", VK_NUMPAD3},
    {L"Numpad 4", VK_NUMPAD4}, {L"Numpad 5", VK_NUMPAD5},
    {L"Numpad 6", VK_NUMPAD6}, {L"Numpad 7", VK_NUMPAD7},
    {L"Numpad 8", VK_NUMPAD8}, {L"Numpad 9", VK_NUMPAD9},
    {L"Numpad +", VK_ADD}, {L"Numpad -", VK_SUBTRACT},
    {L"Numpad *", VK_MULTIPLY}, {L"Numpad /", VK_DIVIDE},
    {L"Numpad .", VK_DECIMAL},
    {L"OEM ;", VK_OEM_1}, {L"OEM +", VK_OEM_PLUS},
    {L"OEM ,", VK_OEM_COMMA}, {L"OEM -", VK_OEM_MINUS},
    {L"OEM .", VK_OEM_PERIOD}, {L"OEM /", VK_OEM_2},
    {L"OEM ~", VK_OEM_3}, {L"OEM [", VK_OEM_4},
    {L"OEM \\", VK_OEM_5}, {L"OEM ]", VK_OEM_6},
    {L"OEM quote", VK_OEM_7},
};

void AddHotkeyOption(HWND combo, const wchar_t* name, UINT key) noexcept {
    const auto i = SendMessageW(combo, CB_ADDSTRING, 0,
                                reinterpret_cast<LPARAM>(name));
    if (i != CB_ERR && i != CB_ERRSPACE)
        SendMessageW(combo, CB_SETITEMDATA, static_cast<WPARAM>(i), key);
}

void PopulateHotkeyKeys(HWND combo, UINT selectedKey) noexcept {
    wchar_t name[40]{};
    for (UINT key = L'A'; key <= L'Z'; ++key) {
        swprintf_s(name, L"%lc", static_cast<wchar_t>(key));
        AddHotkeyOption(combo, name, key);
    }
    for (UINT key = L'0'; key <= L'9'; ++key) {
        swprintf_s(name, L"%lc", static_cast<wchar_t>(key));
        AddHotkeyOption(combo, name, key);
    }
    for (UINT key = VK_F1; key <= VK_F24; ++key) {
        if (key == VK_F12) continue;
        swprintf_s(name, L"F%u", key - VK_F1 + 1);
        AddHotkeyOption(combo, name, key);
    }
    for (const auto& item : kSpecialHotkeyKeys)
        AddHotkeyOption(combo, item.name, item.key);
    const int count = static_cast<int>(SendMessageW(combo, CB_GETCOUNT, 0, 0));
    for (int i = 0; i < count; ++i) {
        if (static_cast<UINT>(SendMessageW(combo, CB_GETITEMDATA, i, 0)) ==
            selectedKey) {
            SendMessageW(combo, CB_SETCURSEL, i, 0);
            return;
        }
    }
    SendMessageW(combo, CB_SETCURSEL, 0, 0);
}

HotkeyChord ReadHotkeyChoice(HWND window) noexcept {
    UINT mods = 0;
    if (SendDlgItemMessageW(window, kSettingsCtrl, BM_GETCHECK, 0, 0) == BST_CHECKED)
        mods |= MOD_CONTROL;
    if (SendDlgItemMessageW(window, kSettingsAlt, BM_GETCHECK, 0, 0) == BST_CHECKED)
        mods |= MOD_ALT;
    if (SendDlgItemMessageW(window, kSettingsShift, BM_GETCHECK, 0, 0) == BST_CHECKED)
        mods |= MOD_SHIFT;
    const HWND combo = GetDlgItem(window, kHotkeyInput);
    const LRESULT index = SendMessageW(combo, CB_GETCURSEL, 0, 0);
    if (index == CB_ERR) return {0, 0};
    const LRESULT data = SendMessageW(combo, CB_GETITEMDATA, index, 0);
    return data == CB_ERR ? HotkeyChord{0, 0} : HotkeyChord{mods, static_cast<UINT>(data)};
}

void ResetHotkeyChoice(HWND window) noexcept {
    const UINT mods = g_settings.hotkey.modifiers;
    SendDlgItemMessageW(window, kSettingsCtrl, BM_SETCHECK,
        (mods & MOD_CONTROL) ? BST_CHECKED : BST_UNCHECKED, 0);
    SendDlgItemMessageW(window, kSettingsAlt, BM_SETCHECK,
        (mods & MOD_ALT) ? BST_CHECKED : BST_UNCHECKED, 0);
    SendDlgItemMessageW(window, kSettingsShift, BM_SETCHECK,
        (mods & MOD_SHIFT) ? BST_CHECKED : BST_UNCHECKED, 0);
    const HWND combo = GetDlgItem(window, kHotkeyInput);
    const int count = static_cast<int>(SendMessageW(combo, CB_GETCOUNT, 0, 0));
    for (int i = 0; i < count; ++i) {
        if (static_cast<UINT>(SendMessageW(combo, CB_GETITEMDATA, i, 0)) ==
            g_settings.hotkey.key) {
            SendMessageW(combo, CB_SETCURSEL, i, 0);
            return;
        }
    }
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

// Match the existing native notification style; a failed hotkey must not
// claim that the utility is ready for keyboard use.
void NotifyStartup(bool hotkey_registered) {
    Notify(hotkey_registered
        ? L"CopyFix запущен и готов к работе."
        : L"Горячая клавиша недоступна. Откройте настройки CopyFix.");
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
    const int margin = 25, gap = 18;
    const int column = std::max(150, (w - 2 * margin - gap) / 2);
    const int edit_y = 149;
    const int edit_height = std::max(100, h - edit_y - 140);
    Position(window, kPreviewTitle, 86, 22, w-112, 39, dpi);
    Position(window, kPreviewSubtitle, 86, 63, w-112, 25, dpi);
    Position(window, kPreviewBeforeLabel, margin + 14, 113, column-28, 27, dpi);
    Position(window, kPreviewAfterLabel, margin+column+gap+14, 113, column-28, 27, dpi);
    Position(window, kPreviewBeforeEdit, margin+12, edit_y, column-24, edit_height, dpi);
    Position(window, kPreviewAfterEdit, margin+column+gap+12, edit_y, column-24, edit_height, dpi);
    Position(window, kPreviewNote, margin+5, h-108, std::max(150,w-300), 24, dpi);
    Position(window, kPreviewShowBreaks, margin+5, h-72, 220, 25, dpi);
    Position(window, kPreviewShowSpaces, margin+235, h-72, 190, 25, dpi);
    Position(window, kPreviewCancel, w-288, h-83, 113, 44, dpi);
    Position(window, kPreviewAccept, w-160, h-83, 134, 44, dpi);
    InvalidateRect(window, nullptr, TRUE);
}

void DrawPreviewSurface(HWND window, HDC dc) noexcept {
    RECT client{}; GetClientRect(window, &client);
    FillRect(dc, &client, g_canvas_brush ? g_canvas_brush
                                        : static_cast<HBRUSH>(GetStockObject(WHITE_BRUSH)));
    const int dpi = g_preview_dpi;
    const int margin = Scale(25,dpi), gap = Scale(18,dpi);
    const int width = client.right;
    const int column = std::max(Scale(150,dpi), (width-2*margin-gap)/2);
    const auto& colors = copyfix::ui::colors;
    copyfix::ui::Badge(dc, Scale(28,dpi), Scale(26,dpi), Scale(39,dpi));
    const int top = Scale(107,dpi);
    const int bottom = client.bottom-Scale(122,dpi);
    copyfix::ui::Rounded(dc,RECT{margin, top, margin+column, bottom},
                          colors.surface, colors.border, Scale(15,dpi));
    copyfix::ui::Rounded(dc,
        RECT{margin+column+gap, top, margin+column*2+gap, bottom},
        colors.surface, colors.border, Scale(15,dpi));
    HBRUSH stroke = CreateSolidBrush(colors.border);
    if (stroke) {
        RECT line{margin,client.bottom-Scale(116,dpi),
                  client.right-margin,client.bottom-Scale(115,dpi)};
        FillRect(dc,&line,stroke);
        DeleteObject(stroke);
    }
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
    SendMessageW(control, EM_SETBKGNDCOLOR, 0,
                 static_cast<LPARAM>(copyfix::ui::colors.paper));
    CHARFORMAT2W format{};
    format.cbSize = sizeof(format);
    format.dwMask = CFM_COLOR;
    format.crTextColor = copyfix::ui::colors.ink;
    SendMessageW(control, EM_SETCHARFORMAT, SCF_ALL,
                 reinterpret_cast<LPARAM>(&format));
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
        MarkChanges(before, diff.minorRemoved, copyfix::ui::colors.minorRemoved);
        MarkChanges(after, diff.minorAdded, copyfix::ui::colors.minorAdded);
    }
    MarkChanges(before, diff.removed, copyfix::ui::colors.removed);
    MarkChanges(after, diff.added, copyfix::ui::colors.added);
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
        InitWindowTheme(window);
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
        HWND before = CreateWindowExW(0, MSFTEDIT_CLASS, L"",
                                     style, 0, 0, 1, 1, window,
                                     reinterpret_cast<HMENU>(static_cast<UINT_PTR>(kPreviewBeforeEdit)),
                                     g_instance, nullptr);
        HWND after = CreateWindowExW(0, MSFTEDIT_CLASS, L"",
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
        StyleCheckBox(toggle);
        HWND spaces = CreateWindowExW(0, L"BUTTON", L"Подсветка пробелов",
                                    WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX,
                                    0,0,1,1,window,
                                    reinterpret_cast<HMENU>(static_cast<UINT_PTR>(kPreviewShowSpaces)),
                                    g_instance,nullptr);
        ApplyFont(spaces, g_font_caption);
        StyleCheckBox(spaces);
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
        g_preview_dpi = HIWORD(wparam);
        auto* suggested = reinterpret_cast<const RECT*>(lparam);
        SetWindowPos(window, nullptr, suggested->left, suggested->top,
                     suggested->right - suggested->left, suggested->bottom - suggested->top,
                     SWP_NOZORDER | SWP_NOACTIVATE);
        return 0;
    }
    case WM_ERASEBKGND: return 1;
    case WM_CTLCOLORSTATIC: {
        auto dc = reinterpret_cast<HDC>(wparam);
        const UINT id = static_cast<UINT>(GetDlgCtrlID(reinterpret_cast<HWND>(lparam)));
        SetBkMode(dc, TRANSPARENT);
        SetTextColor(dc, id == kPreviewTitle || id == kPreviewBeforeLabel ||
                         id == kPreviewAfterLabel ? copyfix::ui::colors.ink
                                                 : copyfix::ui::colors.muted);
        const bool onCard = id == kPreviewBeforeLabel || id == kPreviewAfterLabel;
        return reinterpret_cast<LRESULT>(onCard ? g_surface_brush : g_canvas_brush);
    }
    case WM_CTLCOLORBTN: {
        auto dc = reinterpret_cast<HDC>(wparam);
        SetTextColor(dc, copyfix::ui::colors.ink);
        SetBkColor(dc, copyfix::ui::colors.canvas);
        return reinterpret_cast<LRESULT>(g_canvas_brush);
    }
    case WM_SETTINGCHANGE:
    case WM_THEMECHANGED:
        SynchronizeSystemTheme();
        UpdateWindowTheme(window);
        return 0;
    case WM_DRAWITEM: DrawActionButton(reinterpret_cast<const DRAWITEMSTRUCT*>(lparam)); return TRUE;
    case WM_PAINT: {
        PAINTSTRUCT ps{};
        const auto dc = BeginPaint(window, &ps);
        DrawPreviewSurface(window, dc);
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
    SynchronizeSystemTheme();
    if (g_preview) { SetForegroundWindow(g_preview); return; }
    const auto spawn = PositionedOnCursorMonitor(1040,690,g_message);
    g_preview = CreateWindowExW(WS_EX_APPWINDOW, L"CopyFixPreview", L"CopyFix - проверка текста",
        WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN, spawn.x, spawn.y,
        spawn.width, spawn.height, nullptr, nullptr, g_instance, nullptr);
    if (!g_preview) {
        ClearPending();
        Notify(L"Не удалось открыть предпросмотр.");
        return;
    }
    CenterWindowOnMonitor(g_preview, spawn.monitor);
    InitWindowTheme(g_preview);
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

void ShowShortcutWarning(HWND window, bool occupied) noexcept {
    if (!IsWindow(window)) return;
    ResetHotkeyChoice(window);
    const wchar_t* message = occupied
        ? L"Сочетание уже занято другой программой или зарезервировано Windows. "
          L"Прежняя комбинация сохранена."
        : L"Комбинация недопустима. Выберите Ctrl или Alt, при желании Shift, "
          L"и клавишу из списка.";
    if (HWND status = GetDlgItem(window, kSettingsFeedback))
        SetWindowTextW(status, message);
    g_hotkey_warning_dialog(window, message, L"CopyFix — горячая клавиша",
                            MB_OK | MB_ICONWARNING);
}

const wchar_t* ProfileHelpText(copyfix::Profile profile) noexcept {
    switch (profile) {
    case copyfix::Profile::Gentle:
        return L"Бережная очистка: сохраняет абзацы и сложное форматирование.";
    case copyfix::Profile::Smart:
        return L"Оптимальный баланс: исправляет типичные переносы и пробелы.";
    case copyfix::Profile::Deep:
        return L"Глубокая очистка: больше исправлений с предпросмотром.";
    }
    return L"";
}

void DrawSettingsSurface(HWND window, HDC dc) noexcept {
    RECT client{};
    GetClientRect(window, &client);
    FillRect(dc, &client, g_canvas_brush ? g_canvas_brush
                                        : static_cast<HBRUSH>(GetStockObject(WHITE_BRUSH)));
    const int dpi = g_settings_dpi;
    const int w = client.right;
    const auto& c = copyfix::ui::colors;
    copyfix::ui::Badge(dc, Scale(27,dpi), Scale(26,dpi), Scale(40,dpi));
    copyfix::ui::Rounded(dc,
        RECT{Scale(22,dpi), Scale(112,dpi), w-Scale(22,dpi), Scale(213,dpi)},
        c.surface, c.border, Scale(16,dpi));
    copyfix::ui::Rounded(dc,
        RECT{Scale(22,dpi), Scale(225,dpi), w-Scale(22,dpi), Scale(337,dpi)},
        c.surface, c.border, Scale(16,dpi));
    const RECT separator{Scale(22,dpi), client.bottom-Scale(80,dpi),
                         w-Scale(22,dpi), client.bottom-Scale(79,dpi)};
    HBRUSH stroke = CreateSolidBrush(c.border);
    if (stroke) {
        FillRect(dc, &separator, stroke);
        DeleteObject(stroke);
    }
}

void LayoutSettings(HWND window) {
    RECT bounds{}; GetClientRect(window, &bounds);
    const int dpi = g_settings_dpi;
    const int w = MulDiv(bounds.right, 96, dpi);
    const int h = MulDiv(bounds.bottom, 96, dpi);
    Position(window, kSettingsTitle, 82, 22, w-110, 39, dpi);
    Position(window, kSettingsSubtitle, 82, 64, w-106, 42, dpi);
    Position(window, 420, 38, 121, w-72, 24, dpi);
    Position(window, kSettingsCtrl, 38, 157, 86, 29, dpi);
    Position(window, kSettingsAlt, 138, 157, 81, 29, dpi);
    Position(window, kSettingsShift, 224, 157, 85, 29, dpi);
    Position(window, kHotkeyInput, w-172, 153, 132, 220, dpi);
    Position(window, 421, 38, 233, w-72, 24, dpi);
    const int segmentWidth = std::max(95, (w - 80)/3);
    Position(window, kProfileGentle, 38, 265, segmentWidth, 37, dpi);
    Position(window, kProfileSmart, 38+segmentWidth, 265, segmentWidth, 37, dpi);
    Position(window, kProfileDeep, 38+2*segmentWidth, 265, segmentWidth, 37, dpi);
    Position(window, kSettingsExplanation, 38, 308, w-76, 27, dpi);
    Position(window, kSettingsFeedback, 27, h-119, w-54, 43, dpi);
    Position(window, kSettingsCancel, w-280, h-65, 120, 42, dpi);
    Position(window, kSettingsSave, w-153, h-65, 130, 42, dpi);
}

LRESULT CALLBACK SettingsProc(HWND window, UINT msg, WPARAM wparam, LPARAM lparam) {
    switch (msg) {
    case WM_CREATE: {
        g_settings_dpi = static_cast<int>(GetDpiForWindow(window));
        InitWindowTheme(window);
        g_draft_profile = g_settings.profile;
        MakeLabel(window,kSettingsTitle,L"Настройки",g_font_title);
        MakeLabel(window,kSettingsSubtitle,
                  L"Отметьте Ctrl или Alt, при необходимости Shift, затем выберите клавишу.",
                  g_font_caption);
        MakeLabel(window,420,L"ГОРЯЧАЯ КЛАВИША",g_font_label);
        const HotkeyKeyItem modifiers[] = {
            {L"Ctrl", kSettingsCtrl}, {L"Alt", kSettingsAlt},
            {L"Shift", kSettingsShift},
        };
        for (const auto& option : modifiers) {
            HWND control = CreateWindowW(L"BUTTON", option.name,
                WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX,
                0, 0, 1, 1, window,
                reinterpret_cast<HMENU>(static_cast<UINT_PTR>(option.key)),
                g_instance, nullptr);
            if (!control) return -1;
            ApplyFont(control, g_font_body);
            StyleCheckBox(control);
        }
        HWND hotkey = CreateWindowW(L"COMBOBOX", nullptr,
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST |
            CBS_OWNERDRAWFIXED | WS_VSCROLL,
            0, 0, 1, 1, window,
            reinterpret_cast<HMENU>(static_cast<UINT_PTR>(kHotkeyInput)),
            g_instance, nullptr);
        if (!hotkey) return -1;
        PopulateHotkeyKeys(hotkey, g_settings.hotkey.key);
        ResetHotkeyChoice(window);
        ApplyFont(hotkey,g_font_body);
        MakeLabel(window,421,L"ПРОФИЛЬ ОБРАБОТКИ",g_font_label);
        const wchar_t* profileNames[]{L"Бережный", L"Умный", L"Глубокий"};
        for (UINT index = 0; index < 3; ++index) {
            const HWND button = CreateWindowW(L"BUTTON", profileNames[index],
                WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,
                0,0,1,1,window,
                reinterpret_cast<HMENU>(static_cast<UINT_PTR>(kProfileGentle+index)),
                g_instance,nullptr);
            if (!button) return -1;
            ApplyFont(button,g_font_body);
        }
        MakeLabel(window,kSettingsExplanation,ProfileHelpText(g_draft_profile),g_font_caption);
        MakeLabel(window,kSettingsFeedback,L"",g_font_caption);
        CreateWindowW(L"BUTTON",L"Сохранить",WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,
            0,0,1,1,window,reinterpret_cast<HMENU>(static_cast<UINT_PTR>(kSettingsSave)),g_instance,nullptr);
        CreateWindowW(L"BUTTON",L"Отмена",WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,
            0,0,1,1,window,reinterpret_cast<HMENU>(static_cast<UINT_PTR>(kSettingsCancel)),g_instance,nullptr);
        LayoutSettings(window);
        return 0;
    }
    case WM_SIZE: LayoutSettings(window); return 0;
    case WM_ERASEBKGND:
        return 1;
    case WM_PAINT: {
        PAINTSTRUCT ps{};
        const auto dc = BeginPaint(window, &ps);
        DrawSettingsSurface(window, dc);
        EndPaint(window, &ps);
        return 0;
    }
    case WM_CTLCOLORSTATIC: {
        auto dc = reinterpret_cast<HDC>(wparam);
        SetBkMode(dc,TRANSPARENT);
        const UINT id = static_cast<UINT>(GetDlgCtrlID(reinterpret_cast<HWND>(lparam)));
        const bool insideCard = id == 420 || id == 421 || id == kSettingsExplanation;
        SetTextColor(dc,id==kSettingsFeedback ? copyfix::ui::colors.warning :
            (id==kSettingsTitle || id==420 || id==421
              ? copyfix::ui::colors.ink : copyfix::ui::colors.muted));
        return reinterpret_cast<LRESULT>(insideCard ? g_surface_brush : g_canvas_brush);
    }
    case WM_CTLCOLORBTN:
    case WM_CTLCOLORLISTBOX: {
        const auto dc = reinterpret_cast<HDC>(wparam);
        SetTextColor(dc, copyfix::ui::colors.ink);
        SetBkColor(dc, copyfix::ui::colors.surface);
        return reinterpret_cast<LRESULT>(g_surface_brush);
    }
    case WM_MEASUREITEM: {
        auto* measure = reinterpret_cast<MEASUREITEMSTRUCT*>(lparam);
        if (measure && measure->CtlID == kHotkeyInput) {
            measure->itemHeight = static_cast<UINT>(Scale(34,g_settings_dpi));
            return TRUE;
        }
        break;
    }
    case WM_DRAWITEM: DrawActionButton(reinterpret_cast<const DRAWITEMSTRUCT*>(lparam));return TRUE;
    case WM_SETTINGCHANGE:
    case WM_THEMECHANGED:
        SynchronizeSystemTheme();
        UpdateWindowTheme(window);
        return 0;
    case WM_DPICHANGED: {
        g_settings_dpi = HIWORD(wparam);
        const auto* proposed = reinterpret_cast<const RECT*>(lparam);
        SetWindowPos(window,nullptr,proposed->left,proposed->top,
                     proposed->right-proposed->left,proposed->bottom-proposed->top,
                     SWP_NOZORDER | SWP_NOACTIVATE);
        LayoutSettings(window);
        return 0;
    }
    case WM_COMMAND:
        if (LOWORD(wparam) >= kProfileGentle && LOWORD(wparam) <= kProfileDeep &&
            HIWORD(wparam) == BN_CLICKED &&
            reinterpret_cast<HWND>(lparam) == GetDlgItem(window, LOWORD(wparam))) {
            g_draft_profile = static_cast<copyfix::Profile>(LOWORD(wparam) - kProfileGentle);
            if (HWND info = GetDlgItem(window,kSettingsExplanation))
                SetWindowTextW(info,ProfileHelpText(g_draft_profile));
            for (UINT id = kProfileGentle; id <= kProfileDeep; ++id)
                InvalidateRect(GetDlgItem(window,id),nullptr,TRUE);
            return 0;
        }
        if (LOWORD(wparam) == kSettingsSave) {
            const HotkeyChord next_hotkey = ReadHotkeyChoice(window);
            const auto next_profile = g_draft_profile;
            if (!ValidHotkey(next_hotkey)) {
                ShowShortcutWarning(window, false);
                return 0;
            }
            if (!RegisterChosenHotkey(next_hotkey)) {
                ShowShortcutWarning(window, true);
                return 0;
            }
            g_settings.hotkey = next_hotkey;
            g_settings.profile = next_profile;
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
    case WM_DESTROY: g_settings_window = nullptr; return 0;
    }
    return DefWindowProcW(window,msg,wparam,lparam);
}

void OpenSettings() {
    SynchronizeSystemTheme();
    if (g_settings_window) { SetForegroundWindow(g_settings_window); return; }
    const auto spawn = PositionedOnCursorMonitor(510,475,g_message);
    g_settings_window = CreateWindowExW(WS_EX_APPWINDOW,L"CopyFixSettings",L"CopyFix - настройки",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_CLIPCHILDREN,
        spawn.x,spawn.y,spawn.width,spawn.height,
        nullptr,nullptr,g_instance,nullptr);
    if (!g_settings_window) return;
    CenterWindowOnMonitor(g_settings_window, spawn.monitor);
    ShowWindow(g_settings_window, SW_SHOWNORMAL);
}

HWND MakeCenteredLabel(HWND parent, UINT id, const wchar_t* text,
                       HFONT font) {
    HWND child = CreateWindowExW(0, L"STATIC", text,
        WS_CHILD | WS_VISIBLE | SS_CENTER,
        0,0,1,1,parent,
        reinterpret_cast<HMENU>(static_cast<UINT_PTR>(id)),
        g_instance,nullptr);
    ApplyFont(child,font);
    return child;
}

void LayoutAbout(HWND window) {
    RECT bounds{}; GetClientRect(window, &bounds);
    const int dpi = static_cast<int>(GetDpiForWindow(window));
    const int w = MulDiv(bounds.right,96,dpi);
    Position(window,kAboutIcon,(w-72)/2,31,72,72,dpi);
    Position(window,kAboutTitle,30,113,w-60,45,dpi);
    Position(window,kAboutVersionLabel,30,161,w-60,26,dpi);
    Position(window,kAboutDescriptionLabel,28,197,w-56,42,dpi);
    Position(window,kAboutProject,33,256,w-66,43,dpi);
    Position(window,kAboutGitHub,33,308,w-66,43,dpi);
    Position(window,kAboutLicense,24,367,w-48,44,dpi);
}

void DrawAboutSurface(HWND window, HDC dc) noexcept {
    RECT client{}; GetClientRect(window,&client);
    FillRect(dc,&client,g_canvas_brush ? g_canvas_brush
                                      : static_cast<HBRUSH>(GetStockObject(WHITE_BRUSH)));
    const auto& p = copyfix::ui::colors;
    const int dpi = static_cast<int>(GetDpiForWindow(window));
    copyfix::ui::Rounded(dc,
        RECT{Scale(20,dpi),Scale(18,dpi),
             client.right-Scale(20,dpi),client.bottom-Scale(18,dpi)},
        p.surface,p.border,Scale(20,dpi));
}

LRESULT CALLBACK AboutProc(HWND window, UINT msg, WPARAM wparam, LPARAM lparam) {
    switch (msg) {
    case WM_CREATE: {
        InitWindowTheme(window);
        HWND icon = CreateWindowExW(0,L"STATIC",nullptr,
            WS_CHILD | WS_VISIBLE | SS_ICON | SS_REALSIZEIMAGE,
            0,0,1,1,window,
            reinterpret_cast<HMENU>(static_cast<UINT_PTR>(kAboutIcon)),
            g_instance,nullptr);
        if (!icon) return -1;
        SendMessageW(icon,STM_SETICON,reinterpret_cast<WPARAM>(LoadAppIcon(64)),0);
        MakeCenteredLabel(window,kAboutTitle,L"CopyFix",g_font_title);
        MakeCenteredLabel(window,kAboutVersionLabel,kAboutVersionString,g_font_label);
        MakeCenteredLabel(window,kAboutDescriptionLabel,
            L"Умная обработка скопированного текста",g_font_body);
        HWND project = CreateWindowExW(0,L"BUTTON",L"ⓘ   О проекте",
            WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_OWNERDRAW,
            0,0,1,1,window,
            reinterpret_cast<HMENU>(static_cast<UINT_PTR>(kAboutProject)),
            g_instance,nullptr);
        HWND github = CreateWindowExW(0,L"BUTTON",L"GitHub   ↗",
            WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_OWNERDRAW,
            0,0,1,1,window,
            reinterpret_cast<HMENU>(static_cast<UINT_PTR>(kAboutGitHub)),
            g_instance,nullptr);
        if (!project || !github) return -1;
        ApplyFont(project,g_font_body);
        ApplyFont(github,g_font_body);
        MakeCenteredLabel(window,kAboutLicense,
            L"© 2026 Stanley Lloyd   ·   Лицензия MIT",g_font_caption);
        LayoutAbout(window);
        return 0;
    }
    case WM_ERASEBKGND: return 1;
    case WM_PAINT: {
        PAINTSTRUCT ps{};
        const auto dc = BeginPaint(window,&ps);
        DrawAboutSurface(window,dc);
        EndPaint(window,&ps);
        return 0;
    }
    case WM_CTLCOLORSTATIC: {
        auto dc = reinterpret_cast<HDC>(wparam);
        SetBkMode(dc,TRANSPARENT);
        const UINT id = static_cast<UINT>(GetDlgCtrlID(reinterpret_cast<HWND>(lparam)));
        SetTextColor(dc,id == kAboutTitle ? copyfix::ui::colors.ink
            : (id == kAboutVersionLabel ? copyfix::ui::colors.blue
                                        : copyfix::ui::colors.muted));
        return reinterpret_cast<LRESULT>(g_surface_brush);
    }
    case WM_DRAWITEM:
        DrawActionButton(reinterpret_cast<const DRAWITEMSTRUCT*>(lparam));
        return TRUE;
    case WM_COMMAND:
        if (HIWORD(wparam) == BN_CLICKED &&
            LOWORD(wparam) == kAboutProject &&
            reinterpret_cast<HWND>(lparam) == GetDlgItem(window,kAboutProject)) {
            ShellExecuteW(window,L"open",kAboutWebsite,nullptr,nullptr,SW_SHOWNORMAL);
            return 0;
        }
        if (HIWORD(wparam) == BN_CLICKED &&
            LOWORD(wparam) == kAboutGitHub &&
            reinterpret_cast<HWND>(lparam) == GetDlgItem(window,kAboutGitHub)) {
            ShellExecuteW(window,L"open",kAboutGitHubUrl,nullptr,nullptr,SW_SHOWNORMAL);
            return 0;
        }
        if (LOWORD(wparam) == IDCANCEL && HIWORD(wparam) == 0 && !lparam) {
            DestroyWindow(window);
            return 0;
        }
        break;
    case WM_SETTINGCHANGE:
    case WM_THEMECHANGED:
        SynchronizeSystemTheme();
        UpdateWindowTheme(window);
        return 0;
    case WM_DPICHANGED: {
        const auto* rect = reinterpret_cast<const RECT*>(lparam);
        SetWindowPos(window,nullptr,rect->left,rect->top,
                     rect->right-rect->left,rect->bottom-rect->top,
                     SWP_NOZORDER|SWP_NOACTIVATE);
        LayoutAbout(window);
        return 0;
    }
    case WM_CLOSE: DestroyWindow(window); return 0;
    case WM_DESTROY: g_about_window = nullptr; return 0;
    }
    return DefWindowProcW(window,msg,wparam,lparam);
}

void ShowAbout(HWND parent) noexcept {
    SynchronizeSystemTheme();
    if (g_about_window) {
        SetForegroundWindow(g_about_window);
        return;
    }
    const auto spawn = PositionedOnCursorMonitor(420,460,parent);
    g_about_window = CreateWindowExW(WS_EX_APPWINDOW,L"CopyFixAbout",
        L"CopyFix — о программе",WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_CLIPCHILDREN,
        spawn.x,spawn.y,spawn.width,spawn.height,
        nullptr,nullptr,g_instance,nullptr);
    if (!g_about_window) return;
    CenterWindowOnMonitor(g_about_window,spawn.monitor);
    InitWindowTheme(g_about_window);
    ShowWindow(g_about_window,SW_SHOWNORMAL);
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
        NotifyStartup(hotkey_ok);
        return 0;
    }
    case WM_SETTINGCHANGE:
    case WM_THEMECHANGED:
        SynchronizeSystemTheme();
        return 0;
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
        if (g_about_window) DestroyWindow(g_about_window);
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
    copyfix::ui::SetDark(copyfix::ui::ReadSystemDark());
    g_canvas_brush = copyfix::ui::CanvasBrush();
    g_surface_brush = copyfix::ui::SurfaceBrush();
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
            g_canvas_brush, nullptr, L"CopyFixSettings", small_icon},
        {sizeof(WNDCLASSEXW), 0, AboutProc, 0, 0, instance, large_icon, nullptr,
            g_canvas_brush, nullptr, L"CopyFixAbout", small_icon}
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
        if (g_about_window && IsDialogMessageW(g_about_window, &message)) continue;
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
    if (g_font_body) DeleteObject(g_font_body);
    if (g_font_title) DeleteObject(g_font_title);
    if (g_font_label) DeleteObject(g_font_label);
    if (g_font_caption) DeleteObject(g_font_caption);
    copyfix::ui::FreeBrushes();
    g_canvas_brush = g_surface_brush = nullptr;
    if (g_richedit_module) FreeLibrary(g_richedit_module);
    CloseHandle(g_instance_mutex);
    return static_cast<int>(message.wParam);
}
