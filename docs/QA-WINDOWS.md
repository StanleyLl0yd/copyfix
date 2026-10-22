# CopyFix Windows QA checklist

**Important**: This is a checklist, not a record of tests already performed. Every item must be explicitly verified on Windows before calling the application ready for release.

## Release build (Windows 10 and Windows 11, x64)

- [ ] Build with Visual Studio 2022 MSVC v143 14.44, C++20, Release x64, `/MT`.
- [ ] Run tests with CTest and inspect analysis warnings.
- [ ] Inspect `dumpbin /dependents` for unexpected DLL imports and VC runtime dependencies.
- [ ] Inspect PE headers for ASLR, NX and CFG. Measure and log EXE size and import set.
- [ ] Run on clean Windows 10 x64 VM (without extra runtimes, admin rights or installed IDE).
- [ ] Run on clean Windows 11 x64 VM under the same conditions.
- [ ] Run EXE from a writable directory and a read-only directory, and after reboot.
- [ ] Check idle CPU, private working set and size/performance targets.
- [ ] Check taskbar explorer restart and relaunch of the tray icon.

## New UX and registry validation

- [ ] Download the latest GitHub Actions ZIP and confirm the only file is `CopyFix.exe`.
- [ ] Verify the two-column preview preserves blank lines, sentences, spaces, Unicode emoji and line endings (CRLF in Win32 EDIT).
- [ ] Resize preview; inspect controls at 100%, 125%, 150% and 200% display scaling. Verify keyboard Tab and Escape.
- [ ] Verify settings saved in `HKEY_CURRENT_USER\Software\CopyFix` (`Profile`, `Hotkey` DWORD). Restart and confirm persistence.
- [ ] Confirm no `copyfix.ini` is created or read; test running from a read-only directory.
- [ ] Check that cancelling preview leaves the clipboard unchanged, and that Undo remains functional.

## Safe clipboard integration (isolated VM/session only)

- [ ] Plain Russian copied from Notepad: trigger hotkey; paste cleaned text.
- [ ] Mixed Cyrillic/Latin text including Ё/ё and combining marks.
- [ ] Emoji sequences with variation selector, zero-width joiner and surrogate pair.
- [ ] PDF copy with CRLF, blank lines, hyphenated words; verify deep preview and explicit approval.
- [ ] Structured data (JSON, XML, CSV, Markdown, code, logs, URLs/IP): no silent changes.
- [ ] Rich source (HTML/RTF from Word/browser): preview appears before replacing formats.
- [ ] Pure image and files copied to clipboard: unchanged.
- [ ] Empty and unavailable clipboard: no crash, no destructive write.
- [ ] Clipboard occupied by another process: bounded retry, no overwrite.
- [ ] Clipboard changed between preview and approval: no overwrite.
- [ ] Ctrl+Alt+F9 occupied: settings can choose another shortcut, with clear warning.
- [ ] Undo immediately after cleaning restores original plain text.
- [ ] Undo after another app copies new content: does not overwrite it.
- [ ] Change profile and hotkey; verify settings persisted where directory is writable.
- [ ] Read-only directory: app continues with default/current settings without error.
- [ ] Large input (100K, 1M chars), long line, repeated operations: no crashes/leaks.
- [ ] Deep preview over limit: text unchanged with clear notification.
- [ ] No installer, backend, network requests, telemetry, history, log of clipboard data.
- [ ] Scroll both preview panes independently using scrollbar thumb, mouse wheel and Page Up/Down; no overprinted glyphs, preview closing, or process termination.
- [ ] Click Apply and Cancel after scrolling; scroll notifications must never be mistaken for button commands.
- [ ] Check UI dialogs when Windows text scale is 125%, 150% and 200%.
- [ ] Verify copied text pasted into common apps (browser form, Word, Notepad).

## Known design caveat

Win32 clipboard replacement is not transactional. When `EmptyClipboard` succeeds but `SetClipboardData` fails, the application makes a best-effort restoration of original **plain Unicode text**, not rich formats. Successful unit tests alone do not establish complete clipboard safety or portable runtime compatibility.
