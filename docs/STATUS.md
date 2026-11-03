# CopyFix Windows prototype 0.1.0 - verification record (2026-11-03)

This is evidence of tests performed, not a Windows 1.0 release declaration.

| Area | Current verified state |
| --- | --- |
| Repository | Public `StanleyLl0yd/copyfix`; main code baseline `35e66008f3d7aa9ecf32ffbeb802c6a0032213bd`, new repository-wide audit [PR #35](https://github.com/StanleyLl0yd/copyfix/pull/35) exact-head `271c2a4694ebfbcdbf452e7a79f328545dc4aff7` PASS; post-merge CI must be checked separately |
| Original local Git history | Original HEAD `53d27ada541b75e061de5bfbdf986fc1c9a25da6` preserved in `archive/CopyFix-bootstrap-history.bundle` |
| Native Windows program | C++20 / Win32, no third-party UI runtime; MSVC v143 Release x64 static CRT `/MT` PASS |
| Test corpus | 141 shared JSON golden fixtures plus 25 built-in core cases and additional targeted PDF/Unicode regressions |
| Determinism and idempotency | 900 property-like checks in core test, Windows CTest PASS |
| Windows CI | Eight suites: core, preview_diff, perf_integrity, preview_scroll, clipboard_contract, settings_registry, version_resources, icon_resources; **8/8 PASS**; Clang ASan/UBSan **3/3 PASS** and golden-fixture regeneration check PASS on exact audit head |
| PE safety and dependencies | ASLR, NX, CFG and runtime import policy PASS |
| Current reference audit EXE | 287,744 bytes; SHA-256 `C6B75E3592328F57908AC0582262C5C973C19688648EAA3A5B0C738007507054` from [exact-head #41623342037](https://github.com/StanleyLl0yd/copyfix/actions/runs/41623342037); predecessor exact-main 288,256 bytes |
| Artifact format | GitHub Actions ZIP containing exactly one `CopyFix.exe` (no installer, debug reports or DLL payload) |
| Icon integration | Approved original `artwork/app-icon.png` and 16×16 `artwork/tray-16x16.png` preserved with SHA-256; seven-size ICO embedded; light/dark and 16×16 readability owner-reported PASS (2026-11-03) |
| Local persistence | HKCU\Software\CopyFix Profile/Hotkey only; no INI or clipboard-content log |
| Interactive Windows evidence | Owner attested **15/15 functional/UX PASS** plus remaining desktop stress/visual cases PASS (2026-11-03), including clean and working Windows 10/11; OS builds and specific runtime inventory not supplied ([issue #7](https://github.com/StanleyLl0yd/copyfix/issues/7)) |
| Clean Windows 10 and Windows 11 | Both clean and working installations user-reported PASS; standalone VC runtime, OS build and elevation inventory not documented |
| End-to-end hotkey, tray, clipboard, race handling, Undo, reboot | Owner-reported PASS including 100K/1M characters, clipboard contention, reboot/relaunch, non-writable EXE directory, DPI scaling and light/dark icon legibility |
| Branch protection | Ruleset `25634329`: required PR + two strict GitHub Actions checks, resolved threads, linear history, squash-only, no deletion/force-push, zero bypass; independently verified |
| License / GitHub Release / Windows 1.0 | MIT owner-approved and committed; signing/certificates deferred; no 1.0 tag or GitHub Release authorized or created |
| Android | NOT STARTED; deferred until Windows 1.0 |

Current audit evidence: [exact-head Windows and sanitizer CI #41623342037](https://github.com/StanleyLl0yd/copyfix/actions/runs/41623342037) and EXE-only artifact `13224457634`. Prior exact-main [CI #41620567438](https://github.com/StanleyLl0yd/copyfix/actions/runs/41620567438) PASS. The clipboard test uses synthetic data on Windows Server 2022; manual Windows 10/11 acceptance is owner-attested for an earlier build, not the new About hyperlink.

Clang strict-warning and ASan/UBSan tests pass on exact code-main (3/3). Automated Windows CI runs on Windows Server 2022; owner separately reported desktop testing on clean and working Windows 10/11.

**Open before publication:** complete the explicit version bump to 1.0.0 through protected PR, manually inspect/click the new About link and keyboard/DPI behavior on Windows, optionally record exact OS build/runtime inventory, and obtain explicit owner authorization for GitHub Release `1.0.0`. Code signing is deferred and not a blocker. Earlier owner desktop checks are not re-certified on the newest build.

Historical exit-audit baseline: [WINDOWS-EXIT-AUDIT.md](WINDOWS-EXIT-AUDIT.md). Current repository-wide audit and remediation: [REPOSITORY-AUDIT-2026-11-03.md](REPOSITORY-AUDIT-2026-11-03.md).
