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
        !Within(src,diff.removed) || !Within(dst,diff.added)) return 1;
    if (!ComputePreviewDiff(src,src).removed.empty() || !ComputePreviewDiff(src,src).added.empty()) return 2;
    const std::wstring big(120000,L'a');
    auto fallback=ComputePreviewDiff(big,big+L"b");
    if (fallback.added.size()!=1 || fallback.added[0].begin!=big.size()) return 3;
    auto gone=ComputePreviewDiff(L"abc",L"");
    if (gone.removed.size()!=1 || !gone.added.empty()) return 4;
    const std::wstring unicode=L"Ё👩‍💻  слово";
    auto unicodeResult=ComputePreviewDiff(unicode,L"Ё👩‍💻 слово");
    if (!Within(unicode,unicodeResult.removed)) return 5;
    // Ordinary space-only edits should be reviewable on demand without
    // creating dozens of pink/green flecks in the default preview.
    const auto whitespace = ComputePreviewDiff(L"Сегодня   мы  проверяем",
                                               L"Сегодня мы проверяем");
    if (whitespace.removed.empty() ||
        !copyfix::MeaningfulPreviewSpans(L"Сегодня   мы  проверяем",
                                         whitespace.removed).empty() ||
        !copyfix::MeaningfulPreviewSpans(L"Сегодня мы проверяем",
                                         whitespace.added).empty()) return 6;
    // Physical line-break edits are semantically important, even when the
    // user has not enabled whitespace highlighting.
    const auto lines = ComputePreviewDiff(L"Первая строка\r\nвторая строка",
                                          L"Первая строка вторая строка");
    if (copyfix::MeaningfulPreviewSpans(L"Первая строка\r\nвторая строка",
                                        lines.removed).empty()) return 7;
    const auto real = ComputePreviewDiff(L"скопиро-\r\nван", L"скопирован");
    if (copyfix::MeaningfulPreviewSpans(L"скопиро-\r\nван",
                                        real.removed).empty()) return 8;
    const std::vector<copyfix::DiffSpan> spans{{0, 2}, {2, 5}, {5, 6}};
    const auto filtered = copyfix::MeaningfulPreviewSpans(L"  \tX  ", spans);
    if (filtered.size() != 1 || filtered[0].begin != 2 || filtered[0].end != 5) return 9;
    const auto significant_nbsp =
        copyfix::MeaningfulPreviewSpans(L"\u00a0", {{0,1}});
    if (significant_nbsp.size() != 1) return 10;
    std::cout << "PASS bounded change highlighting and UTF offsets\n";
    return 0;
}
