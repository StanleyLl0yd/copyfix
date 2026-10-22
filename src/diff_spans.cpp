#include "diff_spans.h"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <vector>

namespace copyfix {
namespace {
struct Token { std::size_t begin, end; };

bool IsBreak(wchar_t ch) { return ch == L'\r' || ch == L'\n'; }
bool IsSpace(wchar_t ch) { return ch == L' ' || ch == L'\t'; }
bool IsPunctuation(wchar_t ch) {
    switch (ch) {
    case L'.': case L',': case L';': case L':': case L'!': case L'?':
    case L'-': case L'(': case L')': case L'[': case L']':
    case L'{': case L'}': case L'"': case L'\'': case L'/':
    case L'\\': case L'=': case L'|': case L'*': return true;
    default: return false;
    }
}
std::vector<Token> Split(std::wstring_view input) {
    std::vector<Token> result;
    for (std::size_t i = 0; i < input.size();) {
        const auto start = i;
        if (IsBreak(input[i])) {
            if (input[i] == L'\r' && i + 1 < input.size() && input[i+1] == L'\n') i += 2;
            else ++i;
        } else if (IsSpace(input[i])) {
            do { ++i; } while (i < input.size() && IsSpace(input[i]));
        } else if (IsPunctuation(input[i])) {
            ++i;
        } else {
            do { ++i; } while (i < input.size() && !IsBreak(input[i]) &&
                                 !IsSpace(input[i]) && !IsPunctuation(input[i]));
        }
        result.push_back({start, i});
    }
    return result;
}
void Append(std::vector<DiffSpan>& out, std::size_t begin, std::size_t end) {
    if (begin == end) return;
    if (!out.empty() && out.back().end == begin) out.back().end = end;
    else out.push_back({begin, end});
}
}

PreviewDiff ComputePreviewDiff(std::wstring_view before, std::wstring_view after) {
    PreviewDiff result;
    if (before == after) return result;
    const auto a = Split(before), b = Split(after);
    const auto n = a.size(), m = b.size();
    // The normal window is <= 100k characters, but LCS complexity must not
    // become quadratic for pasted logs or adversarial alternating characters.
    if (n == 0 || m == 0 || (n == 1 && m == 1) || n > 900 || m > 900 || n * m > 500000) {
        std::size_t prefix = 0;
        while (prefix < before.size() && prefix < after.size() && before[prefix] == after[prefix]) ++prefix;
        std::size_t suffix = 0;
        while (suffix < before.size() - prefix && suffix < after.size() - prefix &&
               before[before.size()-1-suffix] == after[after.size()-1-suffix]) ++suffix;
        Append(result.removed, prefix, before.size() - suffix);
        Append(result.added, prefix, after.size() - suffix);
        result.coarse = true;
        return result;
    }
    const auto columns = m + 1;
    std::vector<std::uint16_t> dp((n + 1) * columns);
    auto same = [&](std::size_t i, std::size_t j) {
        const auto x = a[i], y = b[j];
        return before.substr(x.begin, x.end-x.begin) == after.substr(y.begin, y.end-y.begin);
    };
    for (std::size_t i = n; i-- > 0;) {
        for (std::size_t j = m; j-- > 0;) {
            dp[i*columns+j] = same(i,j) ? static_cast<std::uint16_t>(1 + dp[(i+1)*columns+j+1])
                : std::max(dp[(i+1)*columns+j], dp[i*columns+j+1]);
        }
    }
    std::size_t i=0, j=0;
    while (i < n || j < m) {
        if (i < n && j < m && same(i,j)) { ++i; ++j; }
        else if (i < n && (j == m || dp[(i+1)*columns+j] >= dp[i*columns+j+1])) {
            Append(result.removed, a[i].begin, a[i].end);
            ++i;
        } else {
            Append(result.added, b[j].begin, b[j].end);
            ++j;
        }
    }
    return result;
}
}
