#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include "resource_ids.h"

#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <vector>

namespace {
constexpr std::array<unsigned, 7> kSizes{16, 24, 32, 48, 64, 128, 256};
constexpr std::array<BYTE, 8> kPngSignature{0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A};

WORD Read16(const BYTE* p) noexcept {
    return static_cast<WORD>(p[0] | static_cast<unsigned>(p[1]) << 8);
}
DWORD Read32(const BYTE* p) noexcept {
    return static_cast<DWORD>(p[0]) | (static_cast<DWORD>(p[1]) << 8) |
           (static_cast<DWORD>(p[2]) << 16) | (static_cast<DWORD>(p[3]) << 24);
}
DWORD Read32Big(const BYTE* p) noexcept {
    return (static_cast<DWORD>(p[0]) << 24) | (static_cast<DWORD>(p[1]) << 16) |
           (static_cast<DWORD>(p[2]) << 8) | static_cast<DWORD>(p[3]);
}
}

int wmain(int argc, wchar_t** argv) {
    if (argc != 3) return 1;
    const HMODULE image = LoadLibraryExW(argv[1], nullptr,
                                         LOAD_LIBRARY_AS_DATAFILE | LOAD_LIBRARY_AS_IMAGE_RESOURCE);
    if (!image) return 2;
    const auto fail = [image](int code) {
        FreeLibrary(image);
        return code;
    };
    const HRSRC group = FindResourceW(image, MAKEINTRESOURCEW(IDI_COPYFIX_APP), RT_GROUP_ICON);
    if (!group) return fail(3);
    const DWORD groupSize = SizeofResource(image, group);
    const HGLOBAL handle = LoadResource(image, group);
    const auto* data = handle ? static_cast<const BYTE*>(LockResource(handle)) : nullptr;
    if (!data || groupSize < 6 || Read16(data) != 0 || Read16(data + 2) != 1 ||
        Read16(data + 4) != kSizes.size() || groupSize < 6 + kSizes.size() * 14)
        return fail(4);
    std::ifstream trayFile(std::filesystem::path(argv[2]), std::ios::binary);
    if (!trayFile) return fail(5);
    const std::vector<char> tray(std::istreambuf_iterator<char>{trayFile},
                                 std::istreambuf_iterator<char>{});
    if (tray.size() < 24) return fail(6);
    for (std::size_t idx = 0; idx < kSizes.size(); ++idx) {
        const BYTE* entry = data + 6 + 14 * idx;
        const unsigned size = entry[0] == 0 ? 256U : entry[0];
        const unsigned height = entry[1] == 0 ? 256U : entry[1];
        if (size != kSizes[idx] || height != size) return fail(7);
        const WORD iconId = Read16(entry + 12);
        const DWORD byteCount = Read32(entry + 8);
        const HRSRC resource = FindResourceW(image, MAKEINTRESOURCEW(iconId), RT_ICON);
        if (!resource || SizeofResource(image, resource) != byteCount || byteCount < 24)
            return fail(8);
        const HGLOBAL iconHandle = LoadResource(image, resource);
        const auto* png = iconHandle ? static_cast<const BYTE*>(LockResource(iconHandle)) : nullptr;
        if (!png) return fail(9);
        for (std::size_t pos = 0; pos < kPngSignature.size(); ++pos) {
            if (png[pos] != kPngSignature[pos]) return fail(10);
        }
        if (Read32Big(png + 16) != size || Read32Big(png + 20) != size)
            return fail(11);
        if (size == 16) {
            if (tray.size() != byteCount) return fail(12);
            for (std::size_t pos = 0; pos < tray.size(); ++pos) {
                if (png[pos] != static_cast<BYTE>(tray[pos])) return fail(13);
            }
        }
    }
    FreeLibrary(image);
    std::cout << "PASS seven embedded Windows ICO sizes and exact approved 16x16 artwork\n";
    return 0;
}
