#pragma once

#include <cstddef>
#include <string>

namespace copyfix {

enum class Profile { Gentle, Smart, Deep };

struct Result {
    std::wstring text;
    std::size_t changes = 0;
    bool requires_confirmation = false;
    bool protected_structure = false;
};

// Pure deterministic transform. Input/output use wchar_t (UTF-16 on Windows).
// Caller validates clipboard-supplied UTF-16 before invoking this function.
Result Clean(const std::wstring& source, Profile profile);

// Explicit, preview-only one-line transform. Never called by automatic cleaning.
Result OneLine(const std::wstring& source);

// For Windows native EDIT controls and CF_UNICODETEXT: CRLF and blank lines preserved.
std::wstring ToWindowsLineEndings(const std::wstring& source);

bool IsWellFormedUtf16(const std::wstring& text) noexcept;
bool IsStructured(const std::wstring& text);

}  // namespace copyfix
