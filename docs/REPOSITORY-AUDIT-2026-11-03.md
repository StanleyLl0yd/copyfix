# CopyFix — complete repository-wide source/security review and safe refactoring (2026-11-03)

**Baseline:** protected `main=35e66008f3d7aa9ecf32ffbeb802c6a0032213bd`, Windows prototype **0.1.0**. **Implementation:** [PR #35](https://github.com/StanleyLl0yd/copyfix/pull/35). **Release gate:** [issue #7](https://github.com/StanleyLl0yd/copyfix/issues/7), OPEN; no 1.0.0 publication, tag, signing or Android work is authorized. This is a new review of the entire current repository, not a re-labeling of the historical [Windows exit audit](WINDOWS-EXIT-AUDIT.md).

## Complete inventory and architecture

At baseline: **37 tracked blobs** (33 text/code/config files and four binary assets). The first pass inventoried every tracked path and reviewed all accessible text files, including:

- All six `src/*` C++ files: deterministic UTF-16 text cleanup, bounded token/LCS preview diff, the single Win32 entry point and resource IDs; Win32 callbacks and indirect entry points were followed, not judged unused by grep alone.
- Eight `tests/*.cpp` sources (core, diff, performance, Win32 preview, clipboard, HKCU/hotkeys, embedded ICO and VersionInfo/manifest), `tests/golden_cases.json` (141 cases in nine categories), generated `tests/golden_cases.inc`, and its generator.
- `CMakeLists.txt`, `resources/copyfix.rc`, `resources/version.h.in`, `resources/copyfix.ico`, `tools/check-pe.ps1`, `tools/check-icon.py`, `tools/build-icon.py`, `tools/generate-golden-header.py`; added `tools/check-history-bundle.py` for CI-only validation.
- `.github/workflows/ci.yml`, Dependabot configuration, `.gitignore`, `LICENSE`, `AGENTS.md`, README, both existing QA/audit docs and status, `artwork/README.md` and both canonical icon sources.
- Historical `archive/CopyFix-bootstrap-history.bundle`: tracked binary, deliberately excluded from the EXE. Although direct connector binary reads were unavailable, the CI job now clones the local bundle **without checking out historical code**, executes `git fsck --full --strict --no-reflogs`, enumerates reachable Git objects, and inspects historical blobs against explicit high-risk credential patterns. Exact-head CI confirms **24 Git objects / 16 blobs screened, PASS**. This limited credential-pattern check is not a comprehensive secret scanner.

State/ownership: `copyfix_core` is pure deterministic transformation and diff; `src/winmain.cpp` owns one process-local tray/UI loop, buffered clipboard snapshot + sequence fencing, single-step plain-Unicode Undo, HKCU-only settings, RichEdit preview, configurable hotkeys and system TaskDialog About. CMake's `project(... VERSION ...)` generates About and Windows Explorer version resources; the checked-in multi-size ICO is embedded in one portable EXE. No application backend, telemetry, external UI framework or direct internet client.

## Pass 1 — findings, ranked by impact

| Impact | Finding | Implemented remediation |
| --- | --- | --- |
| High, rare resource failure | `PreviewProc(WM_CREATE)` allocated `std::wstring` for status outside a C++ exception boundary; allocation failure could escape a Win32 callback. | Build identical numeric/profile label in a bounded stack array, with no allocation or heavy formatting CRT. Regression checks the actual RichEdit window label. |
| Medium, fail-closed | Clipboard snapshot accepted sequence number zero; Microsoft documents this as the result when clipboard window-station access is unavailable. Sequence fencing must not trust an unavailable value. | Reject zero sequence in `ReadUnderLock` before writing; assert nonzero valid snapshots in native clipboard tests. |
| Low, unnecessary work | Structured-text classifier used five line traversals, repeated comma counts, and an allocating two-character `substr`; `OneLine` could classify the same input twice. | Consolidate classification to one pass, use indexed trailing-space comparison, calculate structured status once. Preserve CSV, repeated prose-like rows, YAML, Markdown and malformed-UTF flags. |
| Low, build-integrity gap | Generated C++ golden fixture had no CI drift detection; archived Git bundle had no CI integrity/credential-pattern inspection. | Regenerate and diff-check all 141 fixtures, plus verify bundle integrity and scan 16 reachable historical blobs for high-risk patterns on CI. |
| Low, architecture/attack surface | Unused single-call clipboard EOL wrapper; PE policy did not deny accidental direct WinHTTP/WinInet/Winsock/URLMON imports. | Call core EOL conversion directly; prohibit those direct network DLL imports in `check-pe.ps1`. This checks imports, not every possible dynamic network operation. |

All changes are behavior-preserving for successful operations, except the intentional **fail-closed refusal** when Windows cannot provide a trustworthy clipboard sequence number. No public interface, stored values, clipboard format, clipboard history, user action, default Deep profile or core transformation rule was removed.

## Pass 2 — full post-remediation re-review

Revisited all 37 baseline paths and changed code after the first group, not only the diff. Checked source classifier/UTF-16/CRLF semantics and golden corpus; diff bounds and fallback; clipboard lock ownership, allocations, Undo/new-copy race, rich-format confirmation and system error handling; callback exception boundaries, Win32 modeless notification routing, UI lifetime, hotkeys, settings, resource IDs, TaskDialog URL allowlist and VersionInfo; compiled test entry points; CMake/CRT/manifest; CI least-privilege/action pins; icon provenance/scripts; docs, license and packaging. Text-file high-risk credential-pattern and direct network-call scans found **no matching patterns**. An additional CI check passed `git fsck` for the archived bundle and screened 16 reachable blobs for known key/token patterns; full historical secret discovery beyond those signatures is not claimed.

No additional removable code or dependency was proven safe after these fixes. The local branch is intentionally not a rewrite of native Win32 architecture.

## Removed / consolidated / simplified

- **Removed:** one unneeded `ToClipboardLineEndings` forwarding function; allocation-causing `substr` for Markdown suffix; allocating preview-label formatting; repeated `IsStructured` call.
- **Consolidated:** five structure-classification line passes into one, with short-circuit protection preserved. Existing common diff, clipboard and registry helpers retained where they carry behavior.
- **Simplified:** fixed bounded status formatting, single structure evaluation, explicit zero-sequence rejection, reproducible fixture CI, archive-integrity/pattern CI, and PE direct-network import guard.
- **Dependencies:** zero third-party runtime libraries added or removed. Continue to use Windows system DLLs and system RichEdit, MSVC `/MT`; Pillow remains optional only for explicitly approved icon regeneration. The two GitHub Actions remain SHA-pinned and workflow tokens read-only.
- **Legacy:** retain the bootstrap history bundle as a deliberate non-shipped archival artifact; its 24 Git objects passed integrity checks and 16 reachable blobs passed the limited credential-pattern scan. Retain golden JSON + generated C++ fixture and the old dated audit as historical evidence. Neither canonical PNG nor generated ICO was modified.

## Deliberately unchanged and limitations

- Windows `EmptyClipboard` + `SetClipboardData` is **not transactional**. If writing fails after clearing, rollback remains best-effort **plain Unicode**, not full HTML/RTF restoration. Changing this requires a separate compatibility design with real Office/browser test coverage; do not overpromise.
- Conservative structured-data detection and PDF reflow heuristics, capped 1M-character clipboard input / 100K-character preview, bounded preview LCS, one-step Undo and global hotkeys remain as reviewed.
- Win32 modeless/UI ownership and tests that directly include `src/winmain.cpp` remain because removing the existing callback boundaries cannot be proven behavior-preserving. Per-monitor DPI transitions and actual About hyperlink click/keyboard accessibility on Windows 10/11 after the recent UI update still need independent interactive observation.
- Source-level zero-network-call/static import checks do not prove OS/browser behavior after the user explicitly clicks the official About link.
- The prior owner-reported Windows 10/11 clean/workstation functional and edge-case acceptance predates these small changes. CI uses Windows Server 2022; no new hands-on Windows 10/11 results, OOM fault injection, exhaustive historical secret scan, or Windows GUI sanitizers are claimed.
- Signing/certificates deferred by owner; MIT approved; **no license change, Android work, version bump or GitHub Release** in this PR.

## Measured before/after

Count method: tracked `.cpp/.h` physical newline counts, 14 matching source/test files (6 `src/*`, 8 test C++ files); binary sizes from actual CI PE checks. JSON fixtures remain **141** and full CTest suites **8 Windows + 3 Clang**.

| Measure | Baseline main | Audited exact PR head |
| --- | ---: | ---: |
| Tracked blobs | 37 | 39 (new audit report and Git-bundle checker) |
| Production C++ lines | 1,522 | 1,522 |
| C++ test lines | 959 | 989 |
| Total C++ lines | 2,481 | 2,511 |
| Third-party runtime libraries | 0 | 0 |
| Portable EXE bytes | 288,256 | 287,744 |
| Windows CTest suites | 8 | 8 |
| Clang ASan/UBSan suites | 3 | 3 |

**Exact-head validation:** [CI #41624443571](https://github.com/StanleyLl0yd/copyfix/actions/runs/41624443571), commit `ea24f7f391ce51f21542c3204b26033587951738`: Windows MSVC v143 x64 Release `/MT` **8/8 PASS**, PE ASLR/NX/CFG/import policy PASS, icons/VersionInfo/manifest PASS; Clang strict-warning C++20 + ASan/UBSan **3/3 PASS**, 141 golden-fixture drift check PASS, historical Git bundle integrity/limited credential-pattern checks **PASS (24 objects, 16 blobs)**. Audited EXE size **287,744** bytes, SHA-256 `C9BBCA97EEA513AD566A8FB3D88C7646D3B9BEE084AA921205C2BE7BEEFEA0C3`, one-EXE [artifact #13224394333](https://github.com/StanleyLl0yd/copyfix/actions/runs/41624443571/artifacts/13224394333). No known toolchain diagnostics failed.

The evidence above is exact-head of implementation before the final documentation-only commit. Identical production source produced different SHA-256 values in two Windows CI runs while retaining the same 287,744-byte size; **bit-for-bit reproducibility has not been established**. Preserve a per-build/artifact hash rather than treating either as permanent. **Final post-squash exact-main verification must be recorded separately in issue #7**; do not infer an unexecuted post-merge PASS. No dedicated linter/style checker or Windows GUI sanitizer exists beyond the enforced compilation/static analysis and test pipeline.

**Disposition:** audit implemented and reviewed; [issue #7](https://github.com/StanleyLl0yd/copyfix/issues/7) must remain OPEN until the separately authorized 1.0.0 version/release process and outstanding new About manual checks are completed.
