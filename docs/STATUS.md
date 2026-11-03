# CopyFix Windows 1.0.0 release-candidate source — verification record (2026-11-03)

This is evidence of tests performed, not a Windows 1.0 release declaration.

| Area | Current verified state |
| --- | --- |
| Repository | Public `StanleyLl0yd/copyfix`; audited protected `main=50dee4b08c5c5635dc0895942a5cda4dc976f375` with [PR #35](https://github.com/StanleyLl0yd/copyfix/pull/35) merged; 1.0.0 candidate prepared in separate PR, not yet accepted or published |
| Original local Git history | Original HEAD `53d27ada541b75e061de5bfbdf986fc1c9a25da6` preserved in `archive/CopyFix-bootstrap-history.bundle` |
| Native Windows program | C++20 / Win32, no third-party UI runtime; MSVC v143 Release x64 static CRT `/MT` PASS |
| Test corpus | 141 shared JSON golden fixtures plus 25 built-in core cases and additional targeted PDF/Unicode regressions |
| Determinism and idempotency | 900 property-like checks in core test, Windows CTest PASS |
| Windows CI | Audited `0.1.0` exact-main [run #41625261532](https://github.com/StanleyLl0yd/copyfix/actions/runs/41625261532): eight Windows suites **8/8 PASS**, Clang ASan/UBSan **3/3 PASS**, golden fixtures and historical Git-bundle validation PASS. Candidate `1.0.0` CI requires its own exact-head PASS. |
| PE safety and dependencies | ASLR, NX, CFG and runtime import policy PASS |
| Reference audited `0.1.0` EXE | 287,744 bytes; SHA-256 `3D2B85ACCE357038AE986F8D199CB2F79D7D4F95495767C33972D836A66945DF` from [exact-main #41625261532](https://github.com/StanleyLl0yd/copyfix/actions/runs/41625261532), artifact #13224943316; this hash must **not** be reused for the new `1.0.0` build |
| Artifact format | GitHub Actions ZIP containing exactly one `CopyFix.exe` (no installer, debug reports or DLL payload) |
| Icon integration | Approved original `artwork/app-icon.png` and 16×16 `artwork/tray-16x16.png` preserved with SHA-256; seven-size ICO embedded; light/dark and 16×16 readability owner-reported PASS (2026-11-03) |
| Local persistence | HKCU\Software\CopyFix Profile/Hotkey only; no INI or clipboard-content log |
| Interactive Windows evidence | Owner attested **15/15 functional/UX PASS** plus remaining desktop stress/visual cases PASS (2026-11-03), including clean and working Windows 10/11; OS builds and specific runtime inventory not supplied ([issue #7](https://github.com/StanleyLl0yd/copyfix/issues/7)) |
| Clean Windows 10 and Windows 11 | Both clean and working installations user-reported PASS; standalone VC runtime, OS build and elevation inventory not documented |
| End-to-end hotkey, tray, clipboard, race handling, Undo, reboot | Owner-reported PASS including 100K/1M characters, clipboard contention, reboot/relaunch, non-writable EXE directory, DPI scaling and light/dark icon legibility |
| Branch protection | Ruleset `25634329`: required PR + two strict GitHub Actions checks, resolved threads, linear history, squash-only, no deletion/force-push, zero bypass; independently verified |
| License / GitHub Release / Windows 1.0 | MIT owner-approved; source version prepared as 1.0.0 release candidate; signing deferred; **no 1.0 tag or GitHub Release authorized or created** |
| Android | NOT STARTED; deferred until Windows 1.0 |

Latest audited `0.1.0` baseline: [exact-main Windows and sanitizer CI #41625261532](https://github.com/StanleyLl0yd/copyfix/actions/runs/41625261532), EXE-only artifact `13224943316`. Archive bundle's 24 Git objects passed `fsck` and 16 blobs passed limited credential-pattern screening. The `1.0.0` release candidate must earn separate exact-head and post-merge exact-main checks. The clipboard test uses synthetic data on Windows Server 2022; manual Windows 10/11 acceptance is owner-attested for an earlier build, not the new About hyperlink.

Clang strict-warning and ASan/UBSan tests pass on exact code-main (3/3). Automated Windows CI runs on Windows Server 2022; owner separately reported desktop testing on clean and working Windows 10/11.

**Open before publication:** pass the release-candidate version/VersionInfo CI and protected PR gate, manually inspect the `CopyFix 1.0.0` About text, click and keyboard-open the official hyperlink, and check legibility/DPI on Windows 10/11. Record the candidate's own EXE size/hash and exact-main CI. Exact OS build/runtime inventory remains optional. Obtain **separate explicit owner authorization** before creating GitHub Release/tag `1.0.0`. Code signing is deferred and not a blocker.

Historical exit-audit baseline: [WINDOWS-EXIT-AUDIT.md](WINDOWS-EXIT-AUDIT.md). Current repository-wide audit and remediation: [REPOSITORY-AUDIT-2026-11-03.md](REPOSITORY-AUDIT-2026-11-03.md).
