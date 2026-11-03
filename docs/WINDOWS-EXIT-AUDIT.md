# CopyFix — Windows 1.0 exit audit (2026-11-03)

**Code baseline:** `1e334e3b467ba585375c6fd04c20272d0443c421`. **Source/security audit: two passes completed. Windows 1.0 acceptance: OPEN.** This report documents reviewed code and CI evidence, **not** clean Windows 10/11 runtime approval, owner release authorization, or an approved license.

## Scope and architecture

- **Pass 1 — entire repository inventory and review:** 33 tracked blobs at the code baseline. Reviewed all six production `src/*` files, seven C++ test files, 141-case JSON/generated-include fixture pair, `CMakeLists.txt`, resource IDs/RC, icon metadata and SHA checks, all four `tools/*` scripts, `.github/workflows/ci.yml`, Dependabot, `.gitignore`, `AGENTS.md`, README, both QA/status docs and artwork provenance. The historical `archive/CopyFix-bootstrap-history.bundle` was checked for presence/purpose but not parsed or executed; it is not shipped. Approved binary artwork was verified by CI hash/ICO/resource checks, **not** visually reapproved.
- **Architecture:** C++20 `copyfix_core` (deterministic text cleanup, protected-structure classifier and bounded diff); native Win32 `winmain.cpp` (tray, hotkey, RichEdit preview, guarded clipboard/Undo and HKCU registry); resource-backed ICO; seven native Windows test executables compiled only for CI and not included in the portable ZIP. No backend or third-party desktop framework.
- **Pass 2 — post-fix repository-wide re-review:** rechecked production C++ and every C++ test for exception boundaries, `noexcept` allocation, lengths, UTF-16, clipboard ownership/sequence, Undo, bounded diff, Win32 callbacks, indirect entry points, CI action pins, resource/provenance integrity, build/tooling, documentation and generated fixtures. Found and fixed an additional allocation-heavy clipboard path. No unproven deletions or speculative architectural rewrite.

## Remediations completed

1. [PR #29](https://github.com/StanleyLl0yd/copyfix/pull/29) — replaced two allocating `std::wstring::substr` calls inside `noexcept` prose/compound-word helpers with `std::wstring_view`. This removes potential `std::terminate` on allocation failure; added 100K-character classification/dehyphenation regression cases.
2. [PR #30](https://github.com/StanleyLl0yd/copyfix/pull/30) — replaced temporary allocation of normalized clipboard text during no-op comparison with a bounded, allocation-free CR/LF/CRLF equivalence check. Preserved the exact-string early return, commit/Undo flow and 1M-character cap; added native CR/LF/CRLF and 1M-character tests.
3. [PR #31](https://github.com/StanleyLl0yd/copyfix/pull/31) — added strict Clang C++20 `-Wall -Wextra -Wpedantic -Werror`, ASan/UBSan (including leak detection) on Ubuntu CI. The Windows MSVC v143 Release `/MT`, PE checks, icons and EXE-only output are unchanged.

**Removal/consolidation:** removed three unnecessary heap-backed temporary text operations (two classification substrings and the clipboard-comparison conversion); no code files, assets, public interfaces, persistence formats or user features removed. **Dependencies:** unchanged—Windows system DLLs and system RichEdit only at runtime; Pillow is optional for *explicit* icon regeneration, not for the binary build; Actions are SHA-pinned. **Legacy:** the original history bundle remains intentionally preserved, outside the release artifact.

**Intentionally unchanged:** conservative structural-detection heuristics; bounded preview/LCS limits; native Win32 UI and modeless callbacks; dedicated Win32 smoke tests that include the application implementation; approved canonical icon bytes; one Undo snapshot. Simplifying these without proof could change reviewed UX, data fidelity, native callback behavior or approved artwork.

## Verified on the exact code-main SHA

- [Post-merge CI #41607441538](https://github.com/StanleyLl0yd/copyfix/actions/runs/41607441538): **Windows 7/7 CTest PASS**, MSVC v143 x64 Release `/MT`, ASLR/NX/CFG and runtime import policy PASS; **Clang 18.1.3 3/3 CTest PASS under ASan/UBSan**, strict warnings-as-errors.
- Windows job checks approved PNG SHA-256, all seven embedded ICO sizes and the exact 16×16 tray payload. The test-artifact ZIP is configured to contain only `CopyFix.exe`.
- Exact-code-main Windows binary: **285,184 bytes**, SHA-256 `7FDD4284184C28B877AECDF32EED3FA379DCB8FCB8E89668EE7F48DDD3499F0F`. [Artifact #13216933533](https://github.com/StanleyLl0yd/copyfix/actions/runs/41607441538/artifacts/13216933533). Artifact ZIP compression size is not the EXE size.
- Test data: 141 unique shared golden fixtures, 900 property-like core checks, plus targeted PDF, Unicode, Win32 clipboard, preview, registry, hotkey and icon regressions.
- Comparable C++ source/test line count (13 tracked `.cpp/.h` files; exclude fixtures, scripts and docs): **2,310 → 2,347** lines; production **1,459 → 1,477**, tests **851 → 870**. Net additions are bounded no-allocation comparator and regressions; this is security and allocation simplification, not a line-count reduction. Code-baseline tracked blob count remains 33.

## Remaining release gates and limitations

- **Interactive clean Windows 10 x64 and Windows 11 x64:** NOT VERIFIED independently on machines lacking development tools/extra VC runtime; the Windows CI runner is Windows Server 2022, not either desktop OS.
- **Desktop scenarios:** independently attest icon readability on light/dark taskbars and large-size Explorer/taskbar use; 100/125/150/200% DPI, read-only EXE directory, reboot/relaunch, Explorer restart, busy clipboard, 100K/1M clipboard inputs, rich/plain preview, Undo/new-copy race and common paste targets. User reported **15/15 functional/UX PASS** on 2026-11-03, but did not identify the tested OS/build; that does not close these environment-specific checks.
- **GitHub PR gate:** [ruleset #25634329](https://github.com/StanleyLl0yd/copyfix/rules/25634329) is active but contains only `deletion` and `non_fast_forward`. It does **not** yet require PRs, checks or resolved threads. Preserve squash-only, linear history, no force-push/deletion and 0 mandatory approvals while configuring the required PR/status rules.
- **Clipboard limitations:** Windows `EmptyClipboard`/`SetClipboardData` is not transactional; if write fails after clearing, rollback is best-effort **plain Unicode**, not complete restoration of HTML/RTF. Native CI uses disposable synthetic clipboard content, not real Word/browser workflows.
- **Verification unavailable here:** interactive Windows 10/11 desktop/visual tests, machine-specific idle/memory observations, independent runtime installs without toolchains, actual failure injection for OOM and Windows GUI sanitizers. Do not label these PASS.
- **Owner gates:** no license selected; no GitHub Release `1.0.0` and no Android work are authorized.

[Issue #7](https://github.com/StanleyLl0yd/copyfix/issues/7) remains **OPEN** until the above evidence and owner decisions are supplied. No release should be created from this report alone.
