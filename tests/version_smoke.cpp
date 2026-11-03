#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winver.h>
#include "copyfix_version.h"

#include <cwchar>
#include <iostream>
#include <vector>

int wmain(int argc, wchar_t** argv) {
    const auto fail = [](int code) {
        std::cerr << "VERSION_RESOURCE_FAILURE_" << code << "\n";
        return code;
    };
    if (argc != 2) return fail(1);

    DWORD unused = 0;
    const DWORD size = GetFileVersionInfoSizeW(argv[1], &unused);
    if (size == 0) return fail(2);

    std::vector<BYTE> bytes(size);
    if (!GetFileVersionInfoW(argv[1], 0, size, bytes.data())) return fail(3);

    void* raw = nullptr;
    UINT length = 0;
    if (!VerQueryValueW(bytes.data(), L"\\", &raw, &length) ||
        !raw || length < sizeof(VS_FIXEDFILEINFO)) return fail(4);
    const auto* fixed = static_cast<const VS_FIXEDFILEINFO*>(raw);
    if (fixed->dwSignature != 0xFEEF04BD ||
        HIWORD(fixed->dwFileVersionMS) != COPYFIX_VERSION_MAJOR ||
        LOWORD(fixed->dwFileVersionMS) != COPYFIX_VERSION_MINOR ||
        HIWORD(fixed->dwFileVersionLS) != COPYFIX_VERSION_PATCH ||
        LOWORD(fixed->dwFileVersionLS) != 0 ||
        fixed->dwFileVersionMS != fixed->dwProductVersionMS ||
        fixed->dwFileVersionLS != fixed->dwProductVersionLS ||
        fixed->dwFileType != VFT_APP) return fail(5);

    struct Property { const wchar_t* key; const wchar_t* expected; };
    constexpr Property properties[] = {
        {L"CompanyName", L"Stanley Lloyd"},
        {L"FileDescription", L"CopyFix - Offline Clipboard Text Cleanup"},
        {L"FileVersion", COPYFIX_VERSION_WSTR},
        {L"InternalName", L"CopyFix"},
        {L"LegalCopyright", L"Copyright (c) 2026 Stanley Lloyd"},
        {L"OriginalFilename", L"CopyFix.exe"},
        {L"ProductName", L"CopyFix"},
        {L"ProductVersion", COPYFIX_VERSION_WSTR},
        {L"Comments", L"https://stanleyll0yd.github.io/apps/copyfix/"}
    };

    for (const auto& property : properties) {
        wchar_t path[160]{};
        if (swprintf_s(path, L"\\StringFileInfo\\040904B0\\%ls", property.key) < 0)
            return fail(6);
        raw = nullptr;
        length = 0;
        if (!VerQueryValueW(bytes.data(), path, &raw, &length) ||
            !raw || !length || wcscmp(static_cast<const wchar_t*>(raw), property.expected) != 0)
            return fail(7);
    }

    raw = nullptr;
    length = 0;
    if (!VerQueryValueW(bytes.data(), L"\\VarFileInfo\\Translation", &raw, &length) ||
        !raw || length < 4) return fail(8);
    const auto* translation = static_cast<const WORD*>(raw);
    if (translation[0] != 0x0409 || translation[1] != 1200) return fail(9);
    std::cout << "CopyFix version metadata matches CMake project version and publisher fields\n";
    return 0;
}
