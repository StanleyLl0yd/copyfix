// GDI-only theme regressions. No external UI runtime and no user preferences written.
#include "../src/ui_theme.h"
#include <iostream>

int main() {
    const auto fail = [](int id) {
        std::cerr << "THEME_SMOKE_FAILURE_" << id << "\n";
        return id;
    };
    using namespace copyfix::ui;
    SetDark(false);
    const HBRUSH light = CanvasBrush();
    const HBRUSH surface = SurfaceBrush();
    if (!light || !surface) return fail(1);
    LOGBRUSH info{};
    if (GetObjectW(light,sizeof(info),&info) != sizeof(info) ||
        info.lbColor != Light.canvas) return fail(2);
    if (GetObjectW(surface,sizeof(info),&info) != sizeof(info) ||
        info.lbColor != Light.surface) return fail(3);
    SetDark(true);
    const HBRUSH darkBrush = CanvasBrush();
    if (!darkBrush || darkBrush == light ||
        GetObjectW(darkBrush,sizeof(info),&info) != sizeof(info) ||
        info.lbColor != Dark.canvas ||
        colors.ink == Light.ink) return fail(4);

    const HDC screen = GetDC(nullptr);
    if (!screen) return fail(5);
    const HDC dc = CreateCompatibleDC(screen);
    const HBITMAP bitmap = CreateCompatibleBitmap(screen,64,64);
    ReleaseDC(nullptr,screen);
    if (!dc || !bitmap) return fail(6);
    const auto old = SelectObject(dc,bitmap);
    RECT frame{0,0,64,64};
    FillRect(dc,&frame,darkBrush);
    Rounded(dc,RECT{8,8,56,56},Dark.surface,Dark.border,14);
    if (GetPixel(dc,32,32) != Dark.surface ||
        GetPixel(dc,0,0) != Dark.canvas) return fail(7);
    SetDark(false);
    FillRect(dc,&frame,CanvasBrush());
    Rounded(dc,RECT{8,8,56,56},Light.surface,Light.border,14);
    if (GetPixel(dc,32,32) != Light.surface ||
        GetPixel(dc,0,0) != Light.canvas) return fail(8);
    SetHighContrast(true);
    if (!highContrast || colors.canvas!=GetSysColor(COLOR_BTNFACE) ||
        colors.ink!=GetSysColor(COLOR_WINDOWTEXT) ||
        colors.surface!=GetSysColor(COLOR_WINDOW) ||
        CanvasBrush()!=GetSysColorBrush(COLOR_BTNFACE) ||
        SurfaceBrush()!=GetSysColorBrush(COLOR_WINDOW)) return fail(9);
    SetHighContrast(false);
    if (highContrast || colors.canvas!=Light.canvas) return fail(10);
    if (Mix(RGB(0,0,0),RGB(100,100,100),50)!=RGB(50,50,50))
        return fail(11);
    SelectObject(dc,old);
    DeleteObject(bitmap);
    DeleteDC(dc);
    FreeBrushes();
    std::cout << "PASS native light/dark palette and rounded GDI surfaces\n";
    return 0;
}
