#include "text_engine.h"

#include <cwctype>
#include <utility>
#include <vector>

namespace copyfix {
namespace {

bool IsAsciiSpace(wchar_t c) noexcept { return c == L' '; }
bool IsLinebreak(wchar_t c) noexcept { return c == L'\r' || c == L'\n'; }
bool IsBasicLetter(wchar_t c) noexcept {
    return (c >= L'A' && c <= L'Z') || (c >= L'a' && c <= L'z') ||
           (c >= L'А' && c <= L'я') || c == L'Ё' || c == L'ё';
}
bool IsLower(wchar_t c) noexcept {
    return (c >= L'a' && c <= L'z') || (c >= L'а' && c <= L'я') || c == L'ё';
}
bool IsDigit(wchar_t c) noexcept { return c >= L'0' && c <= L'9'; }

std::wstring NormalizeEol(const std::wstring& s, std::size_t& count) {
    std::wstring out;
    out.reserve(s.size());
    for (std::size_t i = 0; i < s.size(); ++i) {
        if (s[i] == L'\r') {
            if (i + 1 < s.size() && s[i + 1] == L'\n') ++i;
            out += L'\n';
            ++count;
        } else {
            out += s[i];
        }
    }
    return out;
}

std::vector<std::wstring> SplitLines(const std::wstring& s) {
    std::vector<std::wstring> out;
    std::size_t last = 0;
    for (std::size_t i = 0; i < s.size(); ++i) {
        if (s[i] == L'\n') {
            out.emplace_back(s.substr(last, i - last));
            last = i + 1;
        }
    }
    out.emplace_back(s.substr(last));
    return out;
}

std::wstring JoinLines(const std::vector<std::wstring>& lines) {
    std::wstring out;
    for (std::size_t i = 0; i < lines.size(); ++i) {
        if (i) out += L'\n';
        out += lines[i];
    }
    return out;
}

std::size_t FirstNonSpace(const std::wstring& s) noexcept {
    const auto pos = s.find_first_not_of(L" \t");
    return pos == std::wstring::npos ? s.size() : pos;
}

bool LooksLikeList(const std::wstring& s) noexcept {
    const auto first = FirstNonSpace(s);
    if (first == s.size()) return false;
    if (s.compare(first, 2, L"- ") == 0 || s.compare(first, 2, L"* ") == 0 ||
        s.compare(first, 2, L"+ ") == 0 || s.compare(first, 2, L"> ") == 0 ||
        s.compare(first, 2, L"• ") == 0) return true;
    auto pos = first;
    while (pos < s.size() && IsDigit(s[pos])) ++pos;
    return pos > first && pos - first <= 4 && pos + 1 < s.size() &&
           (s[pos] == L'.' || s[pos] == L')') && s[pos + 1] == L' ';
}

bool HasSuspiciousStructure(const std::wstring& line) noexcept {
    if (line.find(L'\t') != std::wstring::npos ||
        line.find(L"```") != std::wstring::npos ||
        line.find(L"://") != std::wstring::npos ||
        line.find(L"\\") != std::wstring::npos ||
        line.find(L"</") != std::wstring::npos ||
        line.find(L"=>") != std::wstring::npos ||
        line.find(L"==") != std::wstring::npos ||
        line.find(L"; ") != std::wstring::npos ||
        line.find(L'|') != std::wstring::npos ||
        LooksLikeList(line)) return true;
    const auto pos = FirstNonSpace(line);
    if (pos == line.size()) return false;
    if (pos >= 4) return true;  // Indented code blocks and aligned columns.
    if (line[pos] == L'#' || line[pos] == L'{' || line[pos] == L'}' ||
        line[pos] == L'[' || line[pos] == L']' || line[pos] == L'<') return true;
    // Fail closed on lines that contain an email or typed command assignment.
    if (line.find(L'@') != std::wstring::npos ||
        line.find(L'=') != std::wstring::npos ||
        (line.find(L'"') != std::wstring::npos && line.find(L':') != std::wstring::npos) ||
        line.find(L"&&") != std::wstring::npos ||
        line.find(L"print(") != std::wstring::npos ||
        line.find(L"{ ") != std::wstring::npos) return true;
    // Guard IPv4-like addresses and ISO timestamp log prefixes.
    if (line.size() >= 19 && IsDigit(line[0]) && IsDigit(line[1]) &&
        IsDigit(line[2]) && IsDigit(line[3]) && line[4] == L'-' &&
        line[7] == L'-' && line[10] == L' ' && line[13] == L':' &&
        line[16] == L':') return true;
    std::size_t dots = 0;
    for (const auto c : line) if (c == L'.') ++dots;
    if (dots >= 3) {
        for (std::size_t i = 0; i < line.size(); ++i) {
            if (IsDigit(line[i])) {
                std::size_t j = i;
                unsigned parts = 0;
                while (j < line.size() && parts < 4) {
                    unsigned digits = 0;
                    while (j < line.size() && IsDigit(line[j]) && digits < 4) {
                        ++digits; ++j;
                    }
                    if (!digits || digits > 3) break;
                    ++parts;
                    if (parts == 4) break;
                    if (j >= line.size() || line[j] != L'.') break;
                    ++j;
                }
                if (parts == 4 && (j == line.size() || !IsDigit(line[j]))) return true;
            }
        }
    }
    return false;
}

std::wstring NormalizePlainLine(const std::wstring& line, std::size_t& count) {
    if (line.empty()) return line;
    std::wstring out;
    out.reserve(line.size());
    std::size_t start = 0;
    while (start < line.size() && IsAsciiSpace(line[start])) { ++start; ++count; }
    auto end = line.size();
    while (end > start && IsAsciiSpace(line[end - 1])) { --end; ++count; }
    for (std::size_t i = start; i < end; ++i) {
        if (IsAsciiSpace(line[i]) && !out.empty() && out.back() == L' ') {
            ++count;
            continue;
        }
        // Initial BOM is common after pasted UTF-8/UTF-16 conversion.
        if (out.empty() && line[i] == L'\uFEFF') { ++count; continue; }
        out += line[i];
    }
    return out;
}

bool CanJoin(const std::wstring& prior, const std::wstring& next, Profile profile) noexcept {
    if (prior.empty() || next.empty()) return false;
    if (prior.size() < (profile == Profile::Deep ? 12U : 38U)) return false;
    if (LooksLikeList(prior) || LooksLikeList(next)) return false;
    if (HasSuspiciousStructure(prior) || HasSuspiciousStructure(next)) return false;
    const auto a = prior.back();
    const auto b = next.front();
    if (!IsLower(b)) return false;
    if (a == L'.' || a == L'!' || a == L'?' || a == L':' || a == L';' ||
        a == L'\u2014' || a == L'\u2013' || a == L',' || a == L'(') return false;
    if (a == L'-') return profile == Profile::Deep && prior.size() >= 2 &&
                            IsBasicLetter(prior[prior.size() - 2]);
    return IsBasicLetter(a) || IsDigit(a) || a == L')';
}

}  // namespace

bool IsWellFormedUtf16(const std::wstring& text) noexcept {
    // Unit tests also run on Unix hosts where wchar_t is 32-bit.
    for (std::size_t i = 0; i < text.size(); ++i) {
        const auto u = static_cast<unsigned int>(text[i]);
        if constexpr (sizeof(wchar_t) == 2) {
            if (u >= 0xD800U && u <= 0xDBFFU) {
                if (++i >= text.size()) return false;
                const auto v = static_cast<unsigned int>(text[i]);
                if (v < 0xDC00U || v > 0xDFFFU) return false;
            } else if (u >= 0xDC00U && u <= 0xDFFFU) {
                return false;
            }
        } else {
            if (u > 0x10FFFFU || (u >= 0xD800U && u <= 0xDFFFU)) return false;
        }
    }
    return true;
}

bool IsStructured(const std::wstring& s) {
    // An intentionally broad shield, not a parser. False positives are safe.
    if (s.find(L'\0') != std::wstring::npos) return true;
    if (s.find(L'\t') != std::wstring::npos) return true;
    const auto lines = SplitLines(s);
    for (const auto& line : lines) {
        if (HasSuspiciousStructure(line)) return true;
    }
    // Even a single CSV row may have significant whitespace in its fields.
    for (const auto& line : lines) {
        std::size_t commas = 0;
        for (const auto c : line) if (c == L',') ++commas;
        if (commas >= 2) return true;
    }
    if (lines.size() > 1) {
        // Markdown hard line breaks: two trailing spaces are intentional.
        for (const auto& line : lines) {
            if (line.size() >= 2 && line.substr(line.size() - 2) == L"  ") return true;
        }
        // YAML-style repeated key/value lines; don't mistake a single colon in prose.
        std::size_t yaml_lines = 0;
        for (const auto& line : lines) if (line.find(L":  ") != std::wstring::npos) ++yaml_lines;
        if (yaml_lines >= 2) return true;
        // Common CSV: repeated delimiters on adjacent rows.
        std::size_t comma_lines = 0;
        for (const auto& line : lines) {
            std::size_t commas = 0;
            for (const auto c : line) if (c == L',') ++commas;
            if (commas >= 2) ++comma_lines;
        }
        if (comma_lines >= 2) return true;
    }
    return false;
}

Result Clean(const std::wstring& source, Profile profile) {
    Result result{source};
    if (source.empty() || !IsWellFormedUtf16(source)) return result;
    if (IsStructured(source)) {
        result.protected_structure = true;
        return result;
    }
    std::size_t count = 0;
    auto normalized = NormalizeEol(source, count);
    auto lines = SplitLines(normalized);
    for (auto& line : lines) line = NormalizePlainLine(line, count);

    // Collapse only runs of >=3 LF: preserve one blank line between paragraphs.
    std::vector<std::wstring> reduced;
    reduced.reserve(lines.size());
    unsigned blank_run = 0;
    for (auto& line : lines) {
        if (line.empty()) {
            if (++blank_run > 1 && reduced.size() && reduced.back().empty()) {
                ++count;
                continue;
            }
        } else blank_run = 0;
        reduced.push_back(std::move(line));
    }

    if (profile != Profile::Gentle) {
        std::vector<std::wstring> joined;
        joined.reserve(reduced.size());
        for (auto& line : reduced) {
            if (!joined.empty() && CanJoin(joined.back(), line, profile)) {
                auto& previous = joined.back();
                if (previous.back() == L'-') previous.pop_back();
                else previous += L' ';
                previous += line;
                ++count;
                result.requires_confirmation = true;
            } else joined.emplace_back(std::move(line));
        }
        reduced.swap(joined);
    }
    result.text = JoinLines(reduced);
    if (result.text != source) result.changes = count ? count : 1;
    if (profile == Profile::Deep && result.text != source) result.requires_confirmation = true;
    return result;
}

Result OneLine(const std::wstring& source) {
    Result r{source};
    if (source.empty() || !IsWellFormedUtf16(source) || IsStructured(source)) {
        r.protected_structure = IsStructured(source);
        return r;
    }
    r = Clean(source, Profile::Gentle);
    if (r.text.find_first_of(L"\r\n") == std::wstring::npos) return r;
    std::wstring out;
    out.reserve(r.text.size());
    for (std::size_t i = 0; i < r.text.size(); ++i) {
        const auto ch = r.text[i];
        if (IsLinebreak(ch)) {
            if (!out.empty() && out.back() != L' ') out += L' ';
        } else if (ch != L' ' || out.empty() || out.back() != L' ') out += ch;
    }
    r.text = std::move(out);
    r.changes += 1;
    r.requires_confirmation = true;
    return r;
}

}  // namespace copyfix
