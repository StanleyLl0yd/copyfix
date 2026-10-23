#pragma once

#include <cstddef>
#include <string_view>
#include <vector>

namespace copyfix {
struct DiffSpan {
    std::size_t begin = 0;
    std::size_t end = 0;
};
struct PreviewDiff {
    std::vector<DiffSpan> removed;
    std::vector<DiffSpan> added;
    bool coarse = false;
};
// Character offsets in the exact strings rendered in the preview.
// Bounded memory/time, with a conservative fallback on enormous texts.
PreviewDiff ComputePreviewDiff(std::wstring_view before, std::wstring_view after);

// Hide cosmetic changes made exclusively of ordinary ASCII spaces or tabs.
// Hard paragraph/line breaks, Unicode spaces and changes containing visible
// characters remain highlighted. Original diff and clipboard stay unchanged.
std::vector<DiffSpan> MeaningfulPreviewSpans(std::wstring_view text,
                                              const std::vector<DiffSpan>& spans);
}
