# CopyFix Windows prototype 0.1.0 - verification record (2026-11-03)

This is evidence of tests performed, not a Windows 1.0 release declaration.

| Area | Current verified state |
| --- | --- |
| Repository | Public `StanleyLl0yd/copyfix`, main `36c9a702b1b98ea5bda74dfb173b00ab82cb9840` before this documentation update |
| Original local Git history | Original HEAD `53d27ada541b75e061de5bfbdf986fc1c9a25da6` preserved in `archive/CopyFix-bootstrap-history.bundle` |
| Native Windows program | C++20 / Win32, no third-party UI runtime; MSVC v143 Release x64 static CRT `/MT` PASS |
| Test corpus | 141 shared JSON golden fixtures plus 25 built-in core cases and additional targeted PDF/Unicode regressions |
| Determinism and idempotency | 900 property-like checks in core test, Windows CTest PASS |
| Windows CI | Six suites: core, preview_diff, perf_integrity, preview_scroll, clipboard_contract (including contention/large text/malformed UTF-16), settings_registry; **6/6 PASS** |
| PE safety and dependencies | ASLR, NX, CFG and runtime import policy PASS |
| Current reference EXE | 174,080 bytes; SHA-256 `BFE67B8C2D39332E17A1B12AD8D7D025828A0ED2FC63417C003E5DAAF9AF929F` from post-merge run |
| Artifact format | GitHub Actions ZIP containing exactly one `CopyFix.exe` (no installer, debug reports or DLL payload) |
| Icon integration | Approved original `artwork/app-icon.png` and 16×16 `artwork/tray-16x16.png` preserved with SHA-256; seven-size ICO embedded through native Win32 resource, binary/visual acceptance tracked separately |
| Local persistence | HKCU\Software\CopyFix Profile/Hotkey only; no INI or clipboard-content log |
| Interactive Windows evidence | User attested **15/15 functional and UX scenarios PASS on 2026-11-03** ([issue #7](https://github.com/StanleyLl0yd/copyfix/issues/7)); Windows version and clean-runtime environment not specified |
| Clean Windows 10 and Windows 11 | NOT VERIFIED separately on clean machines without development runtimes |
| End-to-end hotkey, tray, clipboard, race handling, Undo, reboot | Core hotkey/tray/clipboard/Undo/Explorer-restart scenarios PASS per user's 15-item QA; separate reboot/occupied clipboard/1M desktop edge cases not individually attested |
| Branch protection | Ruleset `25634329` blocks deletion/force-push; required PR gate is not yet confirmed enabled |
| License / GitHub Release / Windows 1.0 | NOT CREATED; awaiting Windows runtime/clipboard acceptance |
| Android | NOT STARTED; deferred until Windows 1.0 |

Current evidence: [post-merge Windows CI #41595069562](https://github.com/StanleyLl0yd/copyfix/actions/runs/41595069562) and its EXE-only artifact `13211284913`. The clipboard_contract test uses synthetic clipboard data on the Windows CI runner; it does not replace interactive clean-machine QA.

Prior development checks (strict Clang, ASan/UBSan on Linux) passed in their respective earlier source revisions; do not treat these as a fresh sanitizer run on this exact main.

**Open acceptance gates**: Test on clean Windows 10/11 x64; verify hotkey conflict, tray restoration after Explorer restart, rich/plain clipboard data preservation decisions, Undo without overwriting newer data, profile persistence/restart, read-only EXE directory, high-DPI scaling, scrolling and performance. No license or release is authorized.
