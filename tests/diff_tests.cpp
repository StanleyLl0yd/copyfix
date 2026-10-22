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
    std::cout << "PASS bounded change highlighting and UTF offsets\n";
    return 0;
}
