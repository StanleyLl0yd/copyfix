# CopyFix — Current project status (2026-11-05)

## Windows 1.1.0 — RELEASED / ACCEPTED

- Protected main `28e8bf2040370f90bcf87cbbc768468826de7e7a`, [PR #38](https://github.com/StanleyLl0yd/copyfix/pull/38) MERGED.
- [GitHub Release v1.1.0](https://github.com/StanleyLl0yd/copyfix/releases/tag/v1.1.0) published, one unsigned portable Windows x64 EXE, 324096 bytes, SHA-256 `9b0ca168c74b00502a3dfabda8b9bb21d965bee9ee68b93273d805ff5f08520d`.
- User acceptance **16/16 PASS**, plus Windows startup notification accepted. Exact-main [CI #42000538398](https://github.com/StanleyLl0yd/copyfix/actions/runs/42000538398): Windows 9/9 PASS, Clang 3/3 PASS.

## Windows 1.2.0 — VISUAL REDESIGN IN PROGRESS

- [PR #40](https://github.com/StanleyLl0yd/copyfix/pull/40), **Draft**. Not merged, not released.
- Approved visual directions: Settings **C**, Preview **A**, About **C**, all in light/dark Windows themes, without radio buttons in Settings.
- Native Win32/GDI/DWM/shcore, one portable EXE, existing RichEdit/text engine/persistence and canonical icon unchanged.
- Design and acceptance: [FLUENT-REDESIGN-1.2.0.md](FLUENT-REDESIGN-1.2.0.md). Owner has not yet accepted the new implementation; do not create Release.

## Android

- Roadmap agreed; implementation still **not started** and requires separate permission.

---

## Historical baseline — Windows 1.0.0 (2026-11-03)

This is the verified record for the published Windows 1.0.0 release. The owner accepted the final GUI checks on Windows 10/11; exact OS build/runtime inventories remain unrecorded.

| Area | Current verified state |
| --- | --- |
| Repository | Public `StanleyLl0yd/copyfix`; release source is protected squash commit `5453723534f69a4463ede8e170dd302f05495857` ([PR #36](https://github.com/StanleyLl0yd/copyfix/pull/36)), tagged `v1.0.0` |
| Original local Git history | Original HEAD `53d27ada541b75e061de5bfbdf986fc1c9a25da6` preserved in `archive/CopyFix-bootstrap-history.bundle` |
| Native Windows program | C++20 / Win32, no third-party UI runtime; MSVC v143 Release x64 static CRT `/MT` PASS |
| Test corpus | 141 shared JSON golden fixtures plus 25 built-in core cases and additional targeted PDF/Unicode regressions |
| Determinism and idempotency | 900 property-like checks in core test, Windows CTest PASS |
| Windows CI | [Exact-main #41630516021](https://github.com/StanleyLl0yd/copyfix/actions/runs/41630516021): **9/9 Windows CTest PASS** including new About centering regression; **3/3 Clang ASan/UBSan PASS**; fixture regeneration and archived bundle verification PASS |
| PE safety and dependencies | ASLR, NX, CFG and runtime import policy PASS |
| Official release `1.0.0` EXE | **288,256 bytes**, SHA-256 `814A0949CB8F5D75F25A9A97C08AD34E5F5912DBD45655824C0CB72B82CB6D26`. One unsigned portable EXE, [GitHub Release v1.0.0](https://github.com/StanleyLl0yd/copyfix/releases/tag/v1.0.0) |
| Artifact format | GitHub Actions ZIP containing exactly one `CopyFix.exe` (no installer, debug reports or DLL payload) |
| Icon integration | Approved original `artwork/app-icon.png` and 16×16 `artwork/tray-16x16.png` preserved with SHA-256; seven-size ICO embedded; light/dark and 16×16 readability owner-reported PASS (2026-11-03) |
| Local persistence | HKCU\Software\CopyFix Profile/Hotkey only; no INI or clipboard-content log |
| Interactive Windows evidence | Owner attested **15/15 functional/UX PASS** plus remaining desktop stress/visual cases PASS (2026-11-03), including clean and working Windows 10/11; OS builds and specific runtime inventory not supplied ([issue #7](https://github.com/StanleyLl0yd/copyfix/issues/7)) |
| Clean Windows 10 and Windows 11 | Both clean and working installations user-reported PASS; standalone VC runtime, OS build and elevation inventory not documented |
| End-to-end hotkey, tray, clipboard, race handling, Undo, reboot | Owner-reported PASS including 100K/1M characters, clipboard contention, reboot/relaunch, non-writable EXE directory, DPI scaling and light/dark icon legibility |
| Branch protection | Ruleset `25634329`: required PR + two strict GitHub Actions checks, resolved threads, linear history, squash-only, no deletion/force-push, zero bypass; independently verified |
| License / GitHub Release / Windows 1.0 | MIT; `v1.0.0` published 2026-11-03, tag points to release commit. [Release workflow #41630664582](https://github.com/StanleyLl0yd/copyfix/actions/runs/41630664582) PASS. Unsigned by owner decision; signing not required |
| Android | NOT STARTED. Windows 1.0 acceptance prerequisite fulfilled; any Android work requires a separate owner decision |

## Publication and acceptance evidence

- [PR #36](https://github.com/StanleyLl0yd/copyfix/pull/36) passed required CI and protected squash; release SHA `5453723534f69a4463ede8e170dd302f05495857`, tag `v1.0.0`.
- [Exact-main Windows CI #41630516021](https://github.com/StanleyLl0yd/copyfix/actions/runs/41630516021): 9 Windows and 3 Clang sanitizer suites PASS; version resources, canonical icon sources, PE hardening/runtime imports, golden fixtures and historical bundle integrity PASS.
- [Publishing workflow #41630664582](https://github.com/StanleyLl0yd/copyfix/actions/runs/41630664582): successful GitHub Release creation after exact-main checks; release has one EXE, SHA-256 recorded above.
- Owner reported functional/stress/visual acceptance on clean and working Windows 10/11 (including the final About hyperlink, version, keyboard/DPI and monitor-centered dialog). [Issue #7](https://github.com/StanleyLl0yd/copyfix/issues/7) closed **ACCEPTED**. Automated Windows runner is Windows Server 2022; desktop acceptance remains owner-reported and lacks exact OS build inventories.
- Known clipboard rollback, rich-text Undo and heuristic limitations remain as documented in [WINDOWS-EXIT-AUDIT.md](WINDOWS-EXIT-AUDIT.md) and [REPOSITORY-AUDIT-2026-11-03.md](REPOSITORY-AUDIT-2026-11-03.md).

The one-shot release publisher used for v1.0.0 has served its purpose and is retired after verified publication. No source behavior, canonical artwork, released tag or asset is changed by this documentation update.
