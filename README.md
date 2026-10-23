# CopyFix

**CopyFix** - лёгкая автономная утилита для очистки скопированного текста в Windows. Работает локально, не отправляет текст в сеть, не записывает историю буфера обмена и не использует перехват клавиатуры.

> Status: **Windows prototype 0.1.0 / NOT RELEASE READY**. MSVC v143 x64 build, Windows CTest, static-runtime import checks and PE hardening **PASS** in [GitHub Actions](https://github.com/StanleyLl0yd/copyfix/actions/runs/39775790874). Interactive GUI/tray/clipboard and clean Windows 10/11 portable runtime remain **NOT VERIFIED**.

## Use

1. Copy text in another application.
2. Press **Ctrl+Alt+F9** (configurable), or choose **Исправить текст** in the tray menu.
3. Paste with Ctrl+V.
4. When potentially ambiguous changes or rich clipboard formats are involved, confirm the proposed result in a two-column preview. You can cancel without changing the clipboard.
5. To undo, choose **Отменить исправление** before copying anything new. Undo restores only plain Unicode text, not HTML/RTF representations.

Profiles:

- **Бережный**: normalize ASCII spaces and line endings, remove leading BOM, limit extra blank lines. Structured text is passed through unchanged.
- **Умный**: предлагает объединять переносы внутри обычных абзацев, если следующая строка начинается со строчной буквы (с обязательным предпросмотром).
- **Глубокий** (default for new users; existing registry choice is respected): дополнительно предлагает исправлять переносы слов: `скопиро-\nван` -> `скопирован`; сохраняет дефис в распознаваемых составных словах вроде `Санкт-Петербург`, `северо-западный`, `бизнес-план`, `веб-сайт` и `интернет-магазин`. Неоднозначные изменения требуют подтверждения.

No spelling correction, AI, OCR, auto-paste, clipboard monitoring, telemetry, advertisements, external DLLs or registry state. No license has been selected.

## Building on Windows 10/11 x64

Requires **Visual Studio 2022 Build Tools** (MSVC v143 and Windows SDK) and CMake 3.21+. Build-time dependencies only, not end-user requirements.

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure
```

The Windows GUI is `build/Release/CopyFix.exe`. Release uses C++20 and static MSVC runtime `/MT`; PE imports and clean-system runtime behavior must be separately verified. `tools/check-pe.ps1` checks imports and basic PE hardening and writes SHA-256 diagnostics.

## Portable configuration

Built-in defaults require no files. Settings are stored exclusively in the per-user Windows Registry (`HKEY_CURRENT_USER\Software\CopyFix`, DWORD values `Profile` and `Hotkey`). No `copyfix.ini` is read or created. If the registry cannot be written, settings still apply for the current run. No administrator rights are required.

## Windows interface and test ZIP

The native Windows RichEdit preview (built-in `Msftedit.dll`, no bundled dependency) has side-by-side selectable text with pink highlights for deleted content, green highlights for added content and light neutral-tinted marks for whitespace-only changes. Toggle `↵ Показать настоящие переносы строк` to distinguish source hard line breaks from visual wrapping; markers are for viewing only and are not applied to the clipboard. Actual clipboard serialization still uses CRLF; paragraph boundaries are preserved. The GitHub Actions test artifact ZIP contains **only `CopyFix.exe`**; PE audit results remain visible in CI logs. No installer or GitHub Release is produced.

## Security and limitations

- The Windows clipboard is not an atomic transaction. Before replacement CopyFix checks the sequence number **and** source text under the clipboard lock, preallocates output memory, and attempts restoration if an OS write unexpectedly fails. The clipboard APIs cannot provide absolute rollback guarantees after `EmptyClipboard()` succeeds.
- If the clipboard has formats other than plain text, a preview is required because accepting replaces the other representations with plain Unicode text. Undo does not recover rich representations.
- Clipboard LF newlines are serialized as CRLF for Windows consumers. Previews over 100,000 characters are refused without modifying the clipboard to prevent an uninspectable confirmation.
- A sequence change invalidates Undo, even if the visible text happens to match; Undo cannot overwrite newly copied content.
- Structured content detection is deliberately conservative, **not a complete format parser**; it may skip editable plain text. No fixed heuristics can safely infer every real paragraph boundary or preserve every compound word, so potentially destructive joins require explicit confirmation.
- Avoid copying passwords or secrets if you do not wish them to pass through the CopyFix process memory. CopyFix does not retain a clipboard history or log input/output text.
- Smart/Deep PDF reflow is heuristic and cannot determine every hyphenated compound without a dictionary or semantic context; do not accept ambiguous transformations without reviewing the highlighted result.
- No claim of clean-system runtime compatibility or complete security audit is made before actual testing.

## Repository

- `src/text_engine.*`: platform-independent text transformations.
- `src/winmain.cpp`: native Win32 shell, clipboard handling and dialogs.
- `tests/core_tests.cpp`: standalone framework-free tests; 143 deterministic golden cases plus 900 property-like inputs.
- `tests/golden_cases.json`: UTF-8 shared golden corpus for future Android parity.
- `tests/perf_tests.cpp`: 10k/100k/1M-unit integrity and timing probe.
- `docs/QA-WINDOWS.md`: manual Windows 10/11 integration checklist.
- `docs/STATUS.md`: verified vs unverified milestone gates.
- `.github/workflows/ci.yml`: MSVC x64 Release build, tests, PE audit and artifact upload (not GitHub Release).
- `tools/check-pe.ps1`: dependency, size, PE security and hash diagnostics.

Android (Kotlin + Compose) is **deferred until Windows 1.0 is accepted**. No backend, shared heavyweight framework, or network permission is planned.
