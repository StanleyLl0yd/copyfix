#include "text_engine.h"

#include <cwctype>
#include <string_view>
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
        if (IsLinebreak(s[i])) {
            out.emplace_back(s.substr(last, i - last));
            // Clipboard text usually contains CRLF. Do not keep its CR in
            // each line: CSV and prose classification must be EOL-agnostic.
            if (s[i] == L'\r' && i + 1 < s.size() && s[i + 1] == L'\n') ++i;
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

// A complete prose sentence can enumerate several comma-separated phrases.
// Requiring a space after every comma, sentence punctuation and a multiword
// first phrase avoids treating CSV headers such as "Name, Price, Quantity" or
// compact data rows as prose. Repeated CSV-shaped rows remain protected below.
bool LooksLikeProseEnumeration(const std::wstring& line) noexcept {
    const auto last = line.find_last_not_of(L" \t");
    if (last == std::wstring::npos) return false;
    const auto end = line[last];
    if (end != L'.' && end != L'!' && end != L'?') return false;
    const auto comma = line.find(L',');
    if (comma == std::wstring::npos || comma + 1 >= line.size()) return false;
    const auto first = line.substr(0, comma);
    const auto space = first.find(L' ');
    if (space == std::wstring::npos || space == 0 || space + 1 == first.size()) return false;
    for (auto pos = comma; pos != std::wstring::npos; pos = line.find(L',', pos + 1)) {
        if (pos + 1 >= line.size() || line[pos + 1] != L' ') return false;
    }
    return true;
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

// Joining a physical PDF line is ambiguous. Smart joins unhyphenated
// lower-case continuations; Deep additionally previews dehyphenation.
// Never guess across a blank paragraph boundary, across terminal sentence
// punctuation, or into capitalized sentences. Recognized compound prefixes
// retain the semantic hyphen (Санкт-Петербург, северо-западный).
enum class JoinKind { No, WithSpace, WithoutHyphen, KeepHyphen };

bool IsUpper(wchar_t c) noexcept {
    return (c >= L'A' && c <= L'Z') || (c >= L'А' && c <= L'Я') || c == L'Ё';
}

bool KeepCompoundHyphen(const std::wstring& prior, wchar_t next) noexcept {
    if (prior.empty() || prior.back() != L'-') return false;
    const auto start = prior.find_last_of(L" \t([{");
    const auto stem = prior.substr(start == std::wstring::npos ? 0 : start + 1,
                                   prior.size() - (start == std::wstring::npos ? 0 : start + 1) - 1);
    // All-uppercase initial word in a proper name: keep the dash.
    if (IsUpper(next) && (stem == L"Санкт" || stem == L"Нью" || stem == L"Усть" ||
                           stem == L"Ростов" || stem == L"Йошкар")) return true;
    if (!IsLower(next)) return false;
    // Compare common prefixes case-insensitively, including Cyrillic names
    // at the beginning of a sentence. Locale-independent and allocation-free.
    const auto matches = [&stem](std::wstring_view expected) noexcept {
        if (stem.size() != expected.size()) return false;
        for (std::size_t i = 0; i < stem.size(); ++i) {
            wchar_t ch = stem[i];
            if (ch >= L'A' && ch <= L'Z') ch = static_cast<wchar_t>(ch + (L'a' - L'A'));
            if (ch >= L'А' && ch <= L'Я') ch = static_cast<wchar_t>(ch + (L'а' - L'А'));
            if (ch == L'Ё') ch = L'ё';
            if (ch != expected[i]) return false;
        }
        return true;
    };
    // These prefix compounds normally require a semantic hyphen, even when
    // a PDF wraps the word after the first component. This is deliberately
    // not a dictionary: other ambiguous cases still need human confirmation.
    return matches(L"северо") || matches(L"юго") || matches(L"восточно") ||
           matches(L"западно") || matches(L"социально") || matches(L"научно") ||
           matches(L"военно") || matches(L"русско") || matches(L"англо") ||
           matches(L"историко") || matches(L"медико") ||
           matches(L"интернет") || matches(L"онлайн") || matches(L"офлайн") ||
           matches(L"бизнес") || matches(L"веб") || matches(L"мини") ||
           matches(L"макси") || matches(L"пресс") || matches(L"вице") ||
           matches(L"экс") || matches(L"арт") || matches(L"штаб");
}

JoinKind DecideJoin(const std::wstring& prior, const std::wstring& next,
                    Profile profile) noexcept {
    if (profile == Profile::Gentle || prior.empty() || next.empty()) return JoinKind::No;
    if (LooksLikeList(prior) || LooksLikeList(next) ||
        HasSuspiciousStructure(prior) || HasSuspiciousStructure(next)) return JoinKind::No;
    const wchar_t a = prior.back(), b = next.front();
    if (a == L'.' || a == L'!' || a == L'?' || a == L':' || a == L';' ||
        a == L'\u2014' || a == L'\u2013' || a == L',' || a == L'(') return JoinKind::No;
    if (a == L'-') {
        if (profile != Profile::Deep || prior.size() < 2 ||
            !IsBasicLetter(prior[prior.size() - 2])) return JoinKind::No;
        if (KeepCompoundHyphen(prior, b)) return JoinKind::KeepHyphen;
        if (IsLower(b)) return JoinKind::WithoutHyphen;
        return JoinKind::No;
    }
    if (!IsLower(b)) return JoinKind::No;
    if (prior.size() < (profile == Profile::Smart ? 20U : 10U)) return JoinKind::No;
    return IsBasicLetter(a) || IsDigit(a) || a == L')' ? JoinKind::WithSpace
                                                        : JoinKind::No;
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
        if (commas >= 2 && !LooksLikeProseEnumeration(line)) return true;
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
            // An empty physical line is a semantic paragraph boundary.
            if (!joined.empty()) {
                const auto decision = DecideJoin(joined.back(), line, profile);
                if (decision != JoinKind::No) {
                    auto& previous = joined.back();
                    if (decision == JoinKind::WithoutHyphen) previous.pop_back();
                    if (decision == JoinKind::WithSpace) previous += L' ';
                    // KeepHyphen: concatenate without adding space.
                    previous += line;
                    ++count;
                    result.requires_confirmation = true;
                    continue;
                }
            }
            joined.emplace_back(std::move(line));
        }
        reduced.swap(joined);
    }
    result.text = JoinLines(reduced);
    if (result.text != source) result.changes = count ? count : 1;
    if (profile == Profile::Deep && result.text != source) result.requires_confirmation = true;
    return result;
}

std::wstring ToWindowsLineEndings(const std::wstring& source) {
    std::wstring out;
    out.reserve(source.size());
    for (std::size_t i = 0; i < source.size(); ++i) {
        if (source[i] == L'\r') {
            out += L'\r';
            out += L'\n';
            if (i + 1 < source.size() && source[i + 1] == L'\n') ++i;
        } else if (source[i] == L'\n') {
            out += L'\r';
            out += L'\n';
        } else out += source[i];
    }
    return out;
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
