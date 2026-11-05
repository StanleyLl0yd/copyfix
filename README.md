# CopyFix

**CopyFix** - лёгкая автономная утилита для очистки скопированного текста в Windows. Работает локально, не отправляет текст в сеть, не записывает историю буфера обмена и не использует перехват клавиатуры.

> Status: **Windows 1.2.0 RELEASED / ACCEPTED** — [Download one unsigned portable `CopyFix.exe`](https://github.com/StanleyLl0yd/copyfix/releases/download/v1.2.0/CopyFix.exe) ([Release v1.2.0](https://github.com/StanleyLl0yd/copyfix/releases/tag/v1.2.0)). **348,160 bytes**, SHA-256 `21a16cc89fbcd285732ad2e19ddfe722ee471279901f5e701f2df011fcec20e8`. [PR #40](https://github.com/StanleyLl0yd/copyfix/pull/40) merged via protected squash; [exact-main CI](https://github.com/StanleyLl0yd/copyfix/actions/runs/42041741692) Windows 10/10 and Clang 3/3 PASS. Owner visually accepted Windows 11 light/dark layouts and dark About fix. Earlier releases v1.1.0 and v1.0.0 remain available.

> **Windows 1.2.0** ships the approved Settings C, Preview A and About C windows with native Windows light/dark themes, plus RU/EN language and automatic/light/dark appearance selectors. See [design and verification record](docs/FLUENT-REDESIGN-1.2.0.md).

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

Windows **1.1.0** supports custom global keyboard shortcuts using native Ctrl / Alt / Shift checkboxes and a full key list. Press **Сохранить** to validate through Windows `RegisterHotKey`; an occupied/reserved chord produces an explicit warning and leaves the former active shortcut and Registry setting unchanged. OS-reserved Win/F12, unmodified/Shift-only keys, mouse buttons and universal editing shortcuts such as Ctrl+V/C/X/Z/A/Y or Ctrl+Shift+V are excluded. There is no keyboard hook, keystroke logging or continuous clipboard monitoring. Existing `Hotkey` DWORD values 0/1/2 retain their legacy meanings. An ordinary Windows notification confirms startup, with placement controlled by Windows.

In **Windows 1.2.0**, the selected appearance preference (automatic Windows theme, forced light, or forced dark) determines the appearance of all three native windows. Win32/GDI paints compact surfaces; DWM handles system captions and Windows 11 rounded corners; native RichEdit is retained for side-by-side review. System shcore.dll supports correct initial DPI sizing on the monitor under the cursor. The release includes a Settings language selector with **System default (Windows)**, **Русский**, and **English**. Settings, Preview, About, tray menus, notifications and warnings use the selected language. It adds optional REG_DWORD `Language` (0 auto, 1 Russian, 2 English), preserving existing profile/hotkey values. Other Windows display languages fall back to English. A separate theme selector offers **System default (Windows)**, **Light**, and **Dark**, with live preview and Cancel rollback. The user preference is stored as REG_DWORD `Theme` (0 auto, 1 light, 2 dark), while Windows high-contrast mode always takes precedence. No UI runtime or theme configuration files are shipped.

No spelling correction, AI, OCR, auto-paste, clipboard monitoring, telemetry or advertisements. Windows system DLLs are allowed; settings are stored exclusively under HKCU. Source and distribution terms: [MIT License](LICENSE), copyright (c) 2026 Stanley Lloyd.

## Building on Windows 10/11 x64

Requires **Visual Studio 2022 Build Tools** (MSVC v143 and Windows SDK) and CMake 3.21+. Build-time dependencies only, not end-user requirements.

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure
```

The Windows GUI is `build/Release/CopyFix.exe`. Release uses C++20 and static MSVC runtime `/MT`; PE imports are checked by CI. Windows 10/11 clean and working installations and the final About/version/link/monitor-centering changes were accepted by the owner; exact OS builds and installed runtime inventories are not independently documented. `tools/check-pe.ps1` checks imports and basic PE hardening and writes SHA-256 diagnostics.

## Portable configuration

Built-in defaults require no files. Settings are stored exclusively in the per-user Windows Registry (`HKEY_CURRENT_USER\Software\CopyFix`, current 1.2.0: DWORD values `Profile`, `Hotkey`, `Language`, `Theme`). No `copyfix.ini` is read or created. If the registry cannot be written, settings still apply for the current run. No administrator rights are required.

## Approved CopyFix icon

The owner-approved canonical raster sources are checked into Git byte-for-byte:

- `artwork/app-icon.png` — original detailed icon (1254×1254; white document, Aa, vivid green arrow on blue).
- `artwork/tray-16x16.png` — separately approved pixel-optimized 16×16 tray artwork (white document and green arrow, without tiny Aa).

`resources/copyfix.ico` embeds the approved 16×16 PNG **unchanged** and larger 24/32/48/64/128/256 px raster sizes derived from the main source. The native Win32 resource is embedded directly into `CopyFix.exe`; no icon or extra file is required beside the portable executable. `python tools/check-icon.py` validates canonical source checksums and the ICO; use `python tools/build-icon.py` (Pillow required only for explicit regeneration) after owner-approved artwork changes. The original sources must never be overwritten or silently re-encoded.

## Windows interface and test ZIP

The native Windows RichEdit preview (built-in `Msftedit.dll`, no bundled dependency) has side-by-side selectable text with pink highlights for deleted content, green highlights for added content and light neutral-tinted marks for whitespace-only changes. Toggle `↵ Показать настоящие переносы строк` to distinguish source hard line breaks from visual wrapping; markers are for viewing only and are not applied to the clipboard. Each pane retains its own approximate scroll position when markers are toggled. Actual clipboard serialization still uses CRLF; paragraph boundaries are preserved. The GitHub Actions test artifact ZIP contains **only `CopyFix.exe`**; PE audit results remain visible in CI logs. The official [v1.0.0 GitHub Release](https://github.com/StanleyLl0yd/copyfix/releases/tag/v1.0.0) distributes a single unsigned `CopyFix.exe`; CI test ZIPs remain separate, one-EXE artifacts.

## Security and limitations

- The Windows clipboard is not an atomic transaction. Before replacement CopyFix checks the sequence number **and** source text under the clipboard lock, preallocates output memory, and attempts restoration if an OS write unexpectedly fails. The clipboard APIs cannot provide absolute rollback guarantees after `EmptyClipboard()` succeeds.
- If the clipboard has formats other than plain text, a preview is required because accepting replaces the other representations with plain Unicode text. Undo does not recover rich representations.
- Clipboard LF newlines are serialized as CRLF for Windows consumers. Previews over 100,000 characters are refused without modifying the clipboard to prevent an uninspectable confirmation.
- A sequence change invalidates Undo, even if the visible text happens to match; Undo cannot overwrite newly copied content.
- Structured content detection is deliberately conservative, **not a complete format parser**; it may skip editable plain text. No fixed heuristics can safely infer every real paragraph boundary or preserve every compound word, so potentially destructive joins require explicit confirmation.
- Avoid copying passwords or secrets if you do not wish them to pass through the CopyFix process memory. CopyFix does not retain a clipboard history or log input/output text.
- Smart/Deep PDF reflow is heuristic and cannot determine every hyphenated compound without a dictionary or semantic context; do not accept ambiguous transformations without reviewing the highlighted result.
- The owner reports PASS on clean and working Windows 10/11 installations; exact OS builds and installed runtime inventory are not independently documented. The completed two-pass source/security audit and its remaining limitations are documented in [the exit-audit report](docs/WINDOWS-EXIT-AUDIT.md).

## Repository

- `src/text_engine.*`: platform-independent text transformations.
- `src/winmain.cpp`: native Win32 shell, clipboard handling and dialogs.
- `tests/core_tests.cpp`: standalone framework-free tests; 141 shared JSON golden cases, 25 built-in core cases and 900 property-like inputs.
- `tests/golden_cases.json`: UTF-8 shared golden corpus for future Android parity.
- `tests/perf_tests.cpp`: 10k/100k/1M-unit integrity and timing probe.
- `docs/QA-WINDOWS.md`: manual Windows 10/11 integration checklist.
- `docs/STATUS.md`: verified vs unverified milestone gates.
- `docs/WINDOWS-EXIT-AUDIT.md`: historical two-pass code/security audit at the earlier baseline.
- `docs/REPOSITORY-AUDIT-2026-11-03.md`: current repository-wide two-pass review, remediations, measurements, and remaining limitations.
- `.github/workflows/ci.yml`: MSVC x64 Release build, tests, PE audit and artifact upload (not GitHub Release).
- `tools/check-pe.ps1`: dependency, offline-import policy, PE security and hash diagnostics.
- `tools/check-history-bundle.py`: CI-only historical Git bundle integrity and limited credential-pattern screening.

Windows 1.0 acceptance is complete. Android (Kotlin + Compose) is **not started**; a separate owner decision and scope are required before Android development. No backend, shared heavyweight framework, or network permission is planned.
