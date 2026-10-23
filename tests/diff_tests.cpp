#include "diff_spans.h"
#include <cstdlib>
#include <iostream>
#include <string>

using copyfix::ComputePreviewDiff;

static bool Within(const std::wstring& s, const std::vector<copyfix::DiffSpan>& ranges) {
    for (const auto r : ranges) {
        if (!(r.begin < r.end && r.end <= s.size())) return false;
    }
    return true;
}
int main() {
    const std::wstring src=L"Первый  абзац\r\nскопиро-\r\nван текст\r\n\r\nСанкт-\r\nПетербург";
    const std::wstring dst=L"Первый абзац\r\nскопирован текст\r\n\r\nСанкт-Петербург";
    const auto diff=ComputePreviewDiff(src,dst);
    if (diff.coarse || diff.removed.empty() || diff.added.empty() ||
        !Within(src,diff.removed) || !Within(dst,diff.added) ||
        !Within(src,diff.minorRemoved) || !Within(dst,diff.minorAdded)) return 1;
    if (!ComputePreviewDiff(src,src).removed.empty() || !ComputePreviewDiff(src,src).added.empty()) return 2;
    const std::wstring big(120000,L'a');
    auto fallback=ComputePreviewDiff(big,big+L"b");
    if (fallback.added.size()!=1 || fallback.added[0].begin!=big.size()) return 3;
    auto gone=ComputePreviewDiff(L"abc",L"");
    if (gone.removed.size()!=1 || !gone.added.empty()) return 4;
    const std::wstring unicode=L"Ё👩‍💻  слово";
    auto unicodeResult=ComputePreviewDiff(unicode,L"Ё👩‍💻 слово");
    if (!Within(unicode,unicodeResult.removed)) return 5;
    // Frequent whitespace normalization must be visible, but not presented
    // as substantive word replacement in the UI.
    const auto spaces = ComputePreviewDiff(L"Слово    другое", L"Слово другое");
    if (!spaces.removed.empty() || !spaces.added.empty() ||
        spaces.minorRemoved.empty() || spaces.minorAdded.empty() ||
        !Within(L"Слово    другое", spaces.minorRemoved) ||
        !Within(L"Слово другое", spaces.minorAdded)) return 6;
    const auto newlines = ComputePreviewDiff(L"Первая строка\r\n\r\nВторая",
                                              L"Первая строка\r\nВторая");
    if (newlines.removed.empty() || !newlines.minorRemoved.empty()) return 7;
    const auto words = ComputePreviewDiff(L"Документ скопиро-\r\nван.",
                                           L"Документ скопирован.");
    if (words.removed.empty() || words.added.empty() ||
        !Within(L"Документ скопиро-\r\nван.", words.removed) ||
        !Within(L"Документ скопирован.", words.added)) return 8;
    // Large and maliciously repetitive input stays on the bounded path.
    const auto huge = ComputePreviewDiff(std::wstring(110000, L'x'),
                                         std::wstring(110000, L'y'));
    if (!huge.coarse || huge.removed.size() != 1 || huge.added.size() != 1 ||
        huge.removed[0].end != 110000 || huge.added[0].end != 110000) return 9;
    const auto onlyBreaks = ComputePreviewDiff(L"A\r\nB", L"A B");
    if (onlyBreaks.removed.empty() || onlyBreaks.minorAdded.empty()) return 10;
    const auto tabs = ComputePreviewDiff(L"X\t\tY", L"X Y");
    if (!tabs.removed.empty() || tabs.minorRemoved.empty() ||
        !tabs.added.empty() || tabs.minorAdded.empty()) return 11;
    std::cout << "PASS semantic/subtle change highlighting, bounded complexity and UTF offsets\n";
    return 0;
}
