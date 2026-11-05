#pragma once

#include <windows.h>
#include <dwmapi.h>

namespace copyfix::ui {

struct Palette {
    COLORREF canvas, surface, raised, paper, ink, muted, blue, border;
    COLORREF removed, added, minorRemoved, minorAdded, selected, warning;
};

inline constexpr Palette Light {
    RGB(244,248,253), RGB(255,255,255), RGB(233,242,255),
    RGB(255,255,255), RGB(28,42,64), RGB(95,111,136),
    RGB(24,104,239), RGB(227,234,242), RGB(255,220,225),
    RGB(216,245,228), RGB(255,246,243), RGB(242,250,246),
    RGB(222,237,255), RGB(166,40,49)
};

inline constexpr Palette Dark {
    RGB(23,33,49), RGB(33,46,65), RGB(44,62,85),
    RGB(27,37,53), RGB(240,247,255), RGB(175,193,217),
    RGB(35,138,251), RGB(69,85,105), RGB(98,53,69),
    RGB(39,91,74), RGB(60,44,56), RGB(34,66,58),
    RGB(34,81,139), RGB(255,156,156)
};

// Shared DIP metrics. Win32 RoundRect expects the corner ellipse diameter.
inline constexpr int CardCorner = 14;
inline constexpr int ControlCorner = 12;
inline constexpr int SegmentCorner = 12;
inline constexpr int ControlHeight = 34;

inline COLORREF Mix(COLORREF from, COLORREF to, int percent) noexcept {
    const int a = std::max(0, std::min(100, percent));
    return RGB((GetRValue(from)*(100-a)+GetRValue(to)*a)/100,
               (GetGValue(from)*(100-a)+GetGValue(to)*a)/100,
               (GetBValue(from)*(100-a)+GetBValue(to)*a)/100);
}

inline bool ReadHighContrast() noexcept {
    HIGHCONTRASTW info{sizeof(info)};
    return SystemParametersInfoW(SPI_GETHIGHCONTRAST,sizeof(info),&info,0) &&
           (info.dwFlags&HCF_HIGHCONTRASTON)!=0;
}

inline Palette ContrastPalette() noexcept {
    const COLORREF bg=GetSysColor(COLOR_WINDOW);
    const COLORREF ink=GetSysColor(COLOR_WINDOWTEXT);
    const COLORREF face=GetSysColor(COLOR_BTNFACE);
    const COLORREF accent=GetSysColor(COLOR_HIGHLIGHT);
    const COLORREF frame=GetSysColor(COLOR_WINDOWFRAME);
    return {face,bg,face,bg,ink,ink,accent,frame,
            accent,face,accent,face,accent,ink};
}

inline Palette colors = Light;
inline bool dark = false;
inline bool highContrast = false;
inline HBRUSH lightCanvas = nullptr;
inline HBRUSH darkCanvas = nullptr;
inline HBRUSH lightSurface = nullptr;
inline HBRUSH darkSurface = nullptr;

inline bool ReadSystemDark() noexcept {
    DWORD value = 1;
    DWORD size = sizeof(value);
    const LONG result = RegGetValueW(HKEY_CURRENT_USER,
        L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
        L"AppsUseLightTheme", RRF_RT_REG_DWORD, nullptr, &value, &size);
    return result == ERROR_SUCCESS && value == 0;
}

inline void SetDark(bool value) noexcept {
    dark = value;
    colors = highContrast ? ContrastPalette() : (value ? Dark : Light);
}
inline void SetHighContrast(bool value) noexcept {
    highContrast = value;
    SetDark(dark);
}

inline void EnsureBrushes() noexcept {
    if (!lightCanvas) lightCanvas = CreateSolidBrush(Light.canvas);
    if (!darkCanvas) darkCanvas = CreateSolidBrush(Dark.canvas);
    if (!lightSurface) lightSurface = CreateSolidBrush(Light.surface);
    if (!darkSurface) darkSurface = CreateSolidBrush(Dark.surface);
}

inline HBRUSH CanvasBrush() noexcept {
    if (highContrast) return GetSysColorBrush(COLOR_BTNFACE);
    EnsureBrushes();
    return dark ? darkCanvas : lightCanvas;
}
inline HBRUSH SurfaceBrush() noexcept {
    if (highContrast) return GetSysColorBrush(COLOR_WINDOW);
    EnsureBrushes();
    return dark ? darkSurface : lightSurface;
}
inline void FreeBrushes() noexcept {
    HBRUSH brushes[]{lightCanvas, darkCanvas, lightSurface, darkSurface};
    for (HBRUSH brush : brushes)
        if (brush) DeleteObject(brush);
    lightCanvas = darkCanvas = lightSurface = darkSurface = nullptr;
}

inline void ThemeWindow(HWND window) noexcept {
    if (!window) return;
    const BOOL enable = (dark && !highContrast) ? TRUE : FALSE;
    DwmSetWindowAttribute(window, DWMWA_USE_IMMERSIVE_DARK_MODE,
                          &enable, sizeof(enable));
    const DWM_WINDOW_CORNER_PREFERENCE corners = DWMWCP_ROUND;
    DwmSetWindowAttribute(window, DWMWA_WINDOW_CORNER_PREFERENCE,
                          &corners, sizeof(corners));
}

inline void Rounded(HDC dc, RECT r, COLORREF fill, COLORREF outline,
                    int radius) noexcept {
    HBRUSH brush = CreateSolidBrush(fill);
    HPEN pen = CreatePen(PS_SOLID, 1, outline);
    if (!brush || !pen) {
        if (brush) DeleteObject(brush);
        if (pen) DeleteObject(pen);
        return;
    }
    auto oldBrush = SelectObject(dc, brush);
    auto oldPen = SelectObject(dc, pen);
    RoundRect(dc, r.left, r.top, r.right, r.bottom, radius, radius);
    SelectObject(dc, oldPen);
    SelectObject(dc, oldBrush);
    DeleteObject(pen);
    DeleteObject(brush);
}

inline void Badge(HDC dc, int x, int y, int diameter, int dpi) noexcept {
    // Use the Windows font rasterizer rather than two aliased GDI line
    // segments. The checkmark stays crisp at 100%, 150% and 200% DPI.
    Rounded(dc, RECT{x,y,x+diameter,y+diameter},
            colors.raised, colors.raised, diameter);
    HFONT font = CreateFontW(-MulDiv(13, dpi, 72), 0, 0, 0, FW_NORMAL,
                             FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                             OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                             CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS,
                             L"Segoe UI Symbol");
    if (!font) return;
    const HGDIOBJ previous = SelectObject(dc,font);
    const int previousMode = SetBkMode(dc,TRANSPARENT);
    const COLORREF previousColor = SetTextColor(dc,colors.blue);
    RECT bounds{x,y,x+diameter,y+diameter};
    DrawTextW(dc,L"✓",1,&bounds,DT_CENTER|DT_VCENTER|DT_SINGLELINE|DT_NOPREFIX);
    SetTextColor(dc,previousColor);
    SetBkMode(dc,previousMode);
    SelectObject(dc,previous);
    DeleteObject(font);
}

} // namespace copyfix::ui
