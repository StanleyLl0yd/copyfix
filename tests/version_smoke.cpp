#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winver.h>
#include "copyfix_version.h"

#include <cwchar>
#include <iostream>
#include <string_view>
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
    HMODULE image = LoadLibraryExW(argv[1], nullptr,
        LOAD_LIBRARY_AS_DATAFILE | LOAD_LIBRARY_AS_IMAGE_RESOURCE);
    if (!image) return fail(10);
    const HRSRC resource = FindResourceW(image, MAKEINTRESOURCEW(1), MAKEINTRESOURCEW(24));
    bool manifest_ok = false;
    if (resource) {
        const HGLOBAL resource_data = LoadResource(image, resource);
        const void* bytes_data = resource_data ? LockResource(resource_data) : nullptr;
        const DWORD bytes_size = SizeofResource(image, resource);
        if (bytes_data && bytes_size) {
            const std::string_view manifest(static_cast<const char*>(bytes_data), bytes_size);
            manifest_ok = manifest.find("Microsoft.Windows.Common-Controls") != std::string_view::npos
                && manifest.find("6.0.0.0") != std::string_view::npos;
        }
    }
    FreeLibrary(image);
    if (!manifest_ok) return fail(11);

    std::cout << "VersionInfo and Common Controls 6 manifest match CopyFix build metadata\n";
    return 0;
}
