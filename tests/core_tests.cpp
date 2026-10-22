#include "text_engine.h"

#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

using copyfix::Clean;
using copyfix::Profile;

struct Case {
    const char* name;
    std::wstring source;
    Profile profile;
    std::wstring expected;
    bool structure = false;
};

int main() {
    const std::vector<Case> cases{
#include "golden_cases.inc"
        {"spaces", L"  Привет   мир  ", Profile::Gentle, L"Привет мир"},
        {"eol", L"Один\r\nДва\rТри", Profile::Gentle, L"Один\nДва\nТри"},
        {"paragraphs", L"Один\n\n\n\nДва", Profile::Gentle, L"Один\n\nДва"},
        {"yo", L"  Ёжик   ёж  ", Profile::Gentle, L"Ёжик ёж"},
        {"nbsp", L"Цена\u00a0100   ₽", Profile::Gentle, L"Цена\u00a0100 ₽"},
        {"emoji", L"🙂   Привет", Profile::Gentle, L"🙂 Привет"},
        {"combining", L"A\u0301   B", Profile::Gentle, L"A\u0301 B"},
        {"json", LR"({"hello":  "world"})", Profile::Gentle, LR"({"hello":  "world"})", true},
        {"xml", L"<a>  текст </a>", Profile::Smart, L"<a>  текст </a>", true},
        {"markdown", L"# Header\n- item  one", Profile::Deep, L"# Header\n- item  one", true},
        {"code", L"if ( x ) {  return x; }", Profile::Deep, L"if ( x ) {  return x; }", true},
        {"url", L"https://example.org/a  b", Profile::Deep, L"https://example.org/a  b", true},
        {"tabs", L"name\t  value", Profile::Gentle, L"name\t  value", true},
        {"csv", L"a,b,c\n1, 2,3\n4,5,6", Profile::Smart, L"a,b,c\n1, 2,3\n4,5,6", true},
        {"list", L"1. First   point\n2. Second   point", Profile::Smart,
            L"1. First   point\n2. Second   point", true},
        {"log", L"2026-10-22 12:00:00  WARN", Profile::Gentle,
            L"2026-10-22 12:00:00  WARN", true},
        {"empty", L"", Profile::Gentle, L""},
        {"pdf-safe", L"Это достаточно длинная строка, разбитая в конце слова\nпродолжение текста", Profile::Smart,
            L"Это достаточно длинная строка, разбитая в конце слова продолжение текста"},
        {"pdf-deep", L"В соответствии с действующими нормативными докумен-\nтами организация обязана", Profile::Deep,
            L"В соответствии с действующими нормативными документами организация обязана"},
        {"hyphen-gentle", L"северо-\nзападный", Profile::Gentle, L"северо-\nзападный"},
        {"hyphen-smart", L"северо-\nзападный", Profile::Smart, L"северо-\nзападный"},
        {"soft-hyphen", L"до\u00adкумент", Profile::Gentle, L"до\u00adкумент"},
        {"zwj", L"👩‍💻  Работает", Profile::Gentle, L"👩‍💻 Работает"},
        {"bom", L"\ufeffТекст", Profile::Gentle, L"Текст"},
        {"private", L"000.000.000.000  127.0.0.1", Profile::Gentle, L"000.000.000.000  127.0.0.1", true},
    };
    int failed = 0;
    for (const auto& item : cases) {
        const auto actual = Clean(item.source, item.profile);
        if (actual.text != item.expected || actual.protected_structure != item.structure) {
            std::cerr << "FAIL: " << item.name << "\n";
            ++failed;
        }
        const auto once_more = Clean(actual.text, item.profile);
        if (once_more.text != actual.text) {
            std::cerr << "FAIL idempotency: " << item.name << "\n";
            ++failed;
        }
        if (Clean(item.source, item.profile).text != actual.text) {
            std::cerr << "FAIL determinism: " << item.name << "\n";
            ++failed;
        }
        if (!copyfix::IsWellFormedUtf16(actual.text)) {
            std::cerr << "FAIL unicode: " << item.name << "\n";
            ++failed;
        }
    }
    // Regression: the user's first natural-language QA sample must not be
    // classified as a CSV merely because a Unicode enumeration has commas.
    const std::wstring qa_sample =
        L"Сегодня   мы  решили   проверить  работу новой\n"
        L"программы для  автоматической   обработки текста.\n\n"
        L"Это    обычный текст,  который был скопиро-\n"
        L"ван из PDF-документа.   В нём встречаются\n"
        L"случайные  переносы строк,  лишние    пробелы\n"
        L"и непра-\nвильно разделённые слова.\n\n\n"
        L"Некоторые предложения  разрываются в самых\n"
        L"неожиданных местах, хотя на самом деле\n"
        L"должны составлять  один связный абзац.\n\n"
        L"Например, Санкт-\nПетербург является одним из крупнейших\n"
        L"городов России.  Его архитектура  известна\n"
        L"далеко за пределами страны.\n\n\n"
        L"   Ещё одна проблема -  пробелы в начале строк.\n"
        L"Иногда     их слишком много.\n"
        L"А иногда между абзацами остаётся\n\n\n\n"
        L"четыре или пять пустых строк.\n\n"
        L"Программа должна  сохранять числа:  123 456,78\n"
        L"и даты: 22.10.2026, 01.12.1977.\n\n"
        L"Также необходимо сохранить  символы:\n"
        L"Ё ё, № 42, 50%, €100, ₽2500, © 2026.\n\n"
        L"Unicode: Привет, мир! 🌍 🚀 😀\n"
        L"English text   with    extra spaces.\n\n"
        L"Это   последний абзац  тестового\n"
        L"документа.   Проверка завершена!";
    for (const auto profile : {Profile::Gentle, Profile::Smart, Profile::Deep}) {
        const auto r = Clean(qa_sample, profile);
        if (r.protected_structure || r.text == qa_sample || r.changes == 0 ||
            !copyfix::IsWellFormedUtf16(r.text) || Clean(r.text, profile).text != r.text) {
            std::cerr << "FAIL full plain-text QA sample" << "\n";
            ++failed;
        }
    }
    // Real Windows clipboard data from Notepad uses CRLF, unlike the LF-only
    // source fixture. A classifier must treat the same prose identically.
    for (const auto profile : {Profile::Gentle, Profile::Smart, Profile::Deep}) {
        for (const auto eol : {L"\r\n", L"\r"}) {
            std::wstring windows_sample;
            for (const auto ch : qa_sample) {
                if (ch == L'\n') windows_sample += eol;
                else windows_sample += ch;
            }
            const auto r = Clean(windows_sample, profile);
            if (r.protected_structure || r.text == windows_sample || r.changes == 0 ||
                !copyfix::IsWellFormedUtf16(r.text) ||
                Clean(r.text, profile).text != r.text) {
                std::cerr << "FAIL Windows EOL clipboard QA sample" << "\n";
                ++failed;
            }
        }
    }

    // Regression for Windows EDIT: LF-only output was displayed as one joined
    // paragraph, apparently removing word boundaries and empty lines.
    const std::wstring preview_in = L"Абзац один\nстрока два\n\nАбзац два 🌍\r\nещё текст\rконец";
    const std::wstring preview_out = L"Абзац один\r\nстрока два\r\n\r\nАбзац два 🌍\r\nещё текст\r\nконец";
    if (copyfix::ToWindowsLineEndings(preview_in) != preview_out ||
        copyfix::ToWindowsLineEndings(preview_out) != preview_out) {
        std::cerr << "FAIL native EDIT CRLF layout/blank line regression\n";
        ++failed;
    }
    if (copyfix::ToWindowsLineEndings(L"a\nb").find(L"a\r\nb") == std::wstring::npos) {
        std::cerr << "FAIL word-boundary rendering regression\n";
        ++failed;
    }
    // Property-like stable tests spanning Russian text, whitespace and line endings.
    for (int n = 0; n < 300; ++n) {
        const std::wstring sample = L"  Тест" + std::wstring(static_cast<std::size_t>(n % 19), L' ') +
            L" текста\r\nЭто ещё  один  тест" + std::wstring(static_cast<std::size_t>(n % 7), L'\n');
        for (const auto profile : {Profile::Gentle, Profile::Smart, Profile::Deep}) {
            const auto result = Clean(sample, profile);
            if (Clean(result.text, profile).text != result.text) {
                std::cerr << "FAIL randomized idempotency " << n << "\n";
                ++failed;
            }
        }
    }
    // Explicit one-line mode: preview required, structured sources untouched.
    const auto oneline = copyfix::OneLine(L"Один\nДва\nТри");
    if (oneline.text != L"Один Два Три" || !oneline.requires_confirmation) ++failed;
    const auto protected_line = copyfix::OneLine(L"1. Item one\n2. Item two");
    if (!protected_line.protected_structure || protected_line.text != L"1. Item one\n2. Item two") ++failed;
    if (copyfix::IsWellFormedUtf16(std::wstring(1, static_cast<wchar_t>(0xD800)))) ++failed;
    if constexpr (sizeof(wchar_t) == 2) {
        std::wstring smile;
        smile += static_cast<wchar_t>(0xD83D);
        smile += static_cast<wchar_t>(0xDE00);
        if (!copyfix::IsWellFormedUtf16(smile) || Clean(smile, Profile::Gentle).text != smile) ++failed;
    }
    if (!failed) std::cout << "PASS " << cases.size() << " named + 900 property cases\n";
    return failed ? EXIT_FAILURE : EXIT_SUCCESS;
}
