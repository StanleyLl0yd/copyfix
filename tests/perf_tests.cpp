#include "text_engine.h"
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <string>

int main() {
    for (const std::size_t length : {10000U, 100000U, 1000000U}) {
        std::wstring content;
        content.reserve(length);
        while (content.size() + 11 <= length) content += L"Тест  text  ";
        content.resize(length, L'а');
        const auto begin = std::chrono::steady_clock::now();
        const auto transformed = copyfix::Clean(content, copyfix::Profile::Gentle);
        const auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now() - begin).count();
        if (transformed.text.empty() || transformed.text.size() > content.size() ||
            !copyfix::IsWellFormedUtf16(transformed.text)) return EXIT_FAILURE;
        std::cout << length << " wchar units: " << elapsed << " us\n";
    }
    // A single large line must be linear, not recursively parsed.
    const std::wstring long_line(1000000, L'Я');
    if (copyfix::Clean(long_line, copyfix::Profile::Deep).text != long_line) return EXIT_FAILURE;
    return EXIT_SUCCESS;
}
