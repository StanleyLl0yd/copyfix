# CopyFix Windows prototype 0.1.0 - verification record (2026-11-03)

This is evidence of tests performed, not a Windows 1.0 release declaration.

| Area | Current verified state |
| --- | --- |
| Repository | Public `StanleyLl0yd/copyfix`, tested code baseline `1e334e3b467ba585375c6fd04c20272d0443c421` (documentation-only commits may follow) |
| Original local Git history | Original HEAD `53d27ada541b75e061de5bfbdf986fc1c9a25da6` preserved in `archive/CopyFix-bootstrap-history.bundle` |
| Native Windows program | C++20 / Win32, no third-party UI runtime; MSVC v143 Release x64 static CRT `/MT` PASS |
| Test corpus | 141 shared JSON golden fixtures plus 25 built-in core cases and additional targeted PDF/Unicode regressions |
| Determinism and idempotency | 900 property-like checks in core test, Windows CTest PASS |
| Windows CI | Seven suites: core, preview_diff, perf_integrity, preview_scroll, clipboard_contract (including contention/large text/malformed UTF-16), settings_registry, icon_resources; **7/7 PASS**; strict Clang ASan/UBSan **3/3 PASS** on the same code baseline |
| PE safety and dependencies | ASLR, NX, CFG and runtime import policy PASS |
| Current reference EXE | 285,184 bytes; SHA-256 `7FDD4284184C28B877AECDF32EED3FA379DCB8FCB8E89668EE7F48DDD3499F0F` from post-merge run |
| Artifact format | GitHub Actions ZIP containing exactly one `CopyFix.exe` (no installer, debug reports or DLL payload) |
| Icon integration | Approved original `artwork/app-icon.png` and 16×16 `artwork/tray-16x16.png` preserved with SHA-256; seven-size ICO embedded through native Win32 resource, binary/visual acceptance tracked separately |
| Local persistence | HKCU\Software\CopyFix Profile/Hotkey only; no INI or clipboard-content log |
| Interactive Windows evidence | User attested **15/15 functional and UX scenarios PASS on 2026-11-03** ([issue #7](https://github.com/StanleyLl0yd/copyfix/issues/7)); Windows version and clean-runtime environment not specified |
| Clean Windows 10 and Windows 11 | NOT VERIFIED separately on clean machines without development runtimes |
| End-to-end hotkey, tray, clipboard, race handling, Undo, reboot | Core hotkey/tray/clipboard/Undo/Explorer-restart scenarios PASS per user's 15-item QA; separate reboot/occupied clipboard/1M desktop edge cases not individually attested |
| Branch protection | Ruleset `25634329` blocks deletion/force-push; required PR gate is not yet confirmed enabled |
| License / GitHub Release / Windows 1.0 | NOT CREATED; awaiting Windows runtime/clipboard acceptance |
| Android | NOT STARTED; deferred until Windows 1.0 |

Current evidence: [post-merge Windows and sanitizer CI #41607441538](https://github.com/StanleyLl0yd/copyfix/actions/runs/41607441538) and its EXE-only artifact `13216933533`. The clipboard_contract test uses synthetic clipboard data on the Windows CI runner; it does not replace interactive clean-machine QA.

Clang 18.1.3 strict-warning build and ASan/UBSan tests now pass on this exact code-main SHA (3/3); Windows GUI and clipboard are still only exercised on the Windows Server 2022 CI runner, not on clean desktop Windows 10/11.

**Open acceptance gates**: Test on clean Windows 10/11 x64; verify hotkey conflict, tray restoration after Explorer restart, rich/plain clipboard data preservation decisions, Undo without overwriting newer data, profile persistence/restart, read-only EXE directory, high-DPI scaling, scrolling and performance. No license or release is authorized.

Two-pass source/security exit-audit coverage, exact code changes and remaining limitations: [WINDOWS-EXIT-AUDIT.md](WINDOWS-EXIT-AUDIT.md).
