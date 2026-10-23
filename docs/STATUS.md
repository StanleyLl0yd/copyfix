# CopyFix Windows prototype 0.1.0 - verification record (2026-10-23)

This is evidence of tests performed, not a Windows 1.0 release declaration.

| Area | Current verified state |
| --- | --- |
| Repository | Public `StanleyLl0yd/copyfix`, main `3e1ba3fff09eb01efd768c41dc119165efdcb703` before this documentation update |
| Original local Git history | Original HEAD `53d27ada541b75e061de5bfbdf986fc1c9a25da6` preserved in `archive/CopyFix-bootstrap-history.bundle` |
| Native Windows program | C++20 / Win32, no third-party UI runtime; MSVC v143 Release x64 static CRT `/MT` PASS |
| Test corpus | 141 shared JSON golden fixtures plus 25 built-in core cases and additional targeted PDF/Unicode regressions |
| Determinism and idempotency | 900 property-like checks in core test, Windows CTest PASS |
| Windows CI | Four suites: core, preview_diff, perf_integrity, preview_scroll; **4/4 PASS** |
| PE safety and dependencies | ASLR, NX, CFG and runtime import policy PASS |
| Current reference EXE | 174,080 bytes; SHA-256 `F82B42B7A689454899177A6CD8C9A462C84615A2823FB8B52E6F32FA8B73B7A7` from post-merge run |
| Artifact format | GitHub Actions ZIP containing exactly one `CopyFix.exe` (no installer, debug reports or DLL payload) |
| Local persistence | HKCU\Software\CopyFix Profile/Hotkey only; no INI or clipboard-content log |
| Interactive Windows evidence | User screenshots on 2026-10-22 confirmed native preview opened, all three profiles could be compared, and text reflow/highlights were rendered; this is **partial QA**, not full acceptance |
| Clean Windows 10 and Windows 11 | NOT VERIFIED separately on clean machines without development runtimes |
| End-to-end hotkey, tray, clipboard, race handling, Undo, reboot | NOT FULLY VERIFIED interactively |
| Branch protection | Ruleset `25634329` blocks deletion/force-push; required PR gate is not yet confirmed enabled |
| License / GitHub Release / Windows 1.0 | NOT CREATED; awaiting Windows runtime/clipboard acceptance |
| Android | NOT STARTED; deferred until Windows 1.0 |

Current evidence: [post-merge Windows CI #39922531574](https://github.com/StanleyLl0yd/copyfix/actions/runs/39922531574) and its EXE-only artifact `12487696949`.

Prior development checks (strict Clang, ASan/UBSan on Linux) passed in their respective earlier source revisions; do not treat these as a fresh sanitizer run on this exact main.

**Open acceptance gates**: Test on clean Windows 10/11 x64; verify hotkey conflict, tray restoration after Explorer restart, rich/plain clipboard data preservation decisions, Undo without overwriting newer data, profile persistence/restart, read-only EXE directory, high-DPI scaling, scrolling and performance. No license or release is authorized.
