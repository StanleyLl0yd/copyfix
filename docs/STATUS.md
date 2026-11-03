# CopyFix Windows prototype 0.1.0 - verification record (2026-11-03)

This is evidence of tests performed, not a Windows 1.0 release declaration.

| Area | Current verified state |
| --- | --- |
| Repository | Public `StanleyLl0yd/copyfix`; last fully tested exact-main `775a80f197c7bd45bc3c9668b666083e55ca414b` (license/docs-only update pending post-merge verification) |
| Original local Git history | Original HEAD `53d27ada541b75e061de5bfbdf986fc1c9a25da6` preserved in `archive/CopyFix-bootstrap-history.bundle` |
| Native Windows program | C++20 / Win32, no third-party UI runtime; MSVC v143 Release x64 static CRT `/MT` PASS |
| Test corpus | 141 shared JSON golden fixtures plus 25 built-in core cases and additional targeted PDF/Unicode regressions |
| Determinism and idempotency | 900 property-like checks in core test, Windows CTest PASS |
| Windows CI | Seven suites: core, preview_diff, perf_integrity, preview_scroll, clipboard_contract (including contention/large text/malformed UTF-16), settings_registry, icon_resources; **7/7 PASS**; strict Clang ASan/UBSan **3/3 PASS** on the same code baseline |
| PE safety and dependencies | ASLR, NX, CFG and runtime import policy PASS |
| Current reference EXE | 285,184 bytes; SHA-256 `A69CB4B48C1D794736A85E7A37E0F80E479CEA006788682EC7231711E4D9BEFB` from exact-main run #41609035641 |
| Artifact format | GitHub Actions ZIP containing exactly one `CopyFix.exe` (no installer, debug reports or DLL payload) |
| Icon integration | Approved original `artwork/app-icon.png` and 16×16 `artwork/tray-16x16.png` preserved with SHA-256; seven-size ICO embedded; light/dark and 16×16 readability owner-reported PASS (2026-11-03) |
| Local persistence | HKCU\Software\CopyFix Profile/Hotkey only; no INI or clipboard-content log |
| Interactive Windows evidence | Owner attested **15/15 functional/UX PASS** plus remaining desktop stress/visual cases PASS (2026-11-03), including clean and working Windows 10/11; OS builds and specific runtime inventory not supplied ([issue #7](https://github.com/StanleyLl0yd/copyfix/issues/7)) |
| Clean Windows 10 and Windows 11 | Both clean and working installations user-reported PASS; standalone VC runtime, OS build and elevation inventory not documented |
| End-to-end hotkey, tray, clipboard, race handling, Undo, reboot | Owner-reported PASS including 100K/1M characters, clipboard contention, reboot/relaunch, non-writable EXE directory, DPI scaling and light/dark icon legibility |
| Branch protection | Ruleset `25634329`: required PR + two strict GitHub Actions checks, resolved threads, linear history, squash-only, no deletion/force-push, zero bypass; independently verified |
| License / GitHub Release / Windows 1.0 | MIT owner-approved 2026-11-03, [LICENSE](../LICENSE) added by PR; 1.0 release NOT authorized or created |
| Android | NOT STARTED; deferred until Windows 1.0 |

Current evidence: [exact-main Windows and sanitizer CI #41609035641](https://github.com/StanleyLl0yd/copyfix/actions/runs/41609035641) and its EXE-only artifact `13217817698`. The clipboard_contract test uses synthetic clipboard data on the Windows CI runner; manual Windows 10/11 acceptance is separately owner-attested, not independently recorded.

Clang strict-warning and ASan/UBSan tests pass on exact code-main (3/3). Automated Windows CI runs on Windows Server 2022; owner separately reported desktop testing on clean and working Windows 10/11.

**Open before publication:** record OS builds and runtime/elevation details if a reproducible compatibility matrix is required; obtain explicit owner authorization for GitHub Release `1.0.0`. All listed Windows desktop scenarios were user-reported PASS, not independently instrumented. MIT selected, release not yet authorized.

Two-pass source/security exit-audit coverage, exact code changes and remaining limitations: [WINDOWS-EXIT-AUDIT.md](WINDOWS-EXIT-AUDIT.md).
