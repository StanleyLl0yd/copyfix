# CopyFix validation record - 2026-10-22

| Item | Verified state |
| --- | --- |
| Public GitHub repository | VERIFIED, `StanleyLl0yd/copyfix` |
| Original source tree | VERIFIED: all 16 Git blobs and original tree `a2822e6c88f9b364795d3e0a44efed9e6ccf06dc` were reproduced exactly |
| Original local history | Original commit `53d27ada541b75e061de5bfbdf986fc1c9a25da6` archived as `archive/CopyFix-bootstrap-history.bundle`; GitHub API-created commit SHAs differ |
| Native Win32 GUI, tray, hotkey, clipboard, preview, Undo | CODE WRITTEN; interactive end-to-end validation NOT DONE |
| C++ text engine | Built and tested on Linux x64 with Clang/GCC |
| Golden corpus | 118 JSON cases + 25 built-in cases |
| Determinism/idempotency | 900 local property-like checks PASS |
| Strict Clang diagnostics | PASS, `-Werror -Wall -Wextra -Wpedantic -Wconversion -Wshadow` |
| ASan/UBSan | PASS for core tests on Linux |
| 10k, 100k, 1M units | Basic integrity/performance tests PASS on Linux |
| MSVC v143 Windows executable | PASS, MSVC 14.44.35207 / compiler 19.44.35229.0, x64 Release C++20 |
| Static CRT `/MT` and PE imports | PASS: CI rejects known dynamic VC++/UCRT/.NET runtime dependencies |
| PE hardening | PASS: ASLR, NX, Control Flow Guard header + CFG load configuration |
| Windows CI unit/performance tests | PASS: 2/2 CTest suites on Windows runner |
| Windows EXE size | 160,768 bytes (157.0 KiB), latest post-merge CI artifact |
| GitHub Actions | Exact PR HEAD and post-merge `main` PASS, workflow `39777098974` |
| Windows 10/11 clean-machine runtime | NOT VERIFIED |
| Live clipboard integration, tray, hotkey, preview and Undo | NOT VERIFIED interactively |
| Main branch integrity | Ruleset `25634329` active: no deletion or force-push; required PR gate NOT YET ENABLED |
| Dependabot, secret scanning, token policy | Configuration reported successful by owner-side script on 2026-10-22 |
| GitHub Release / Windows 1.0 | NOT CREATED |
| Android | NOT STARTED; deferred until Windows 1.0 |

Evidence: [MSVC first-success exact PR HEAD #39775639046](https://github.com/StanleyLl0yd/copyfix/actions/runs/39775639046), [Win32 OOM safety exact PR HEAD #39776938242](https://github.com/StanleyLl0yd/copyfix/actions/runs/39776938242), and [latest post-merge main #39777098974](https://github.com/StanleyLl0yd/copyfix/actions/runs/39777098974). Latest main PE summary: **160,768 bytes**, SHA-256 `5FCCD73F7C7DF74F60CC02E80F6637F16ECD56CE60129947321557C9B8B54E2C`.

The current files are a Windows prototype, **not a certified portable release**. No license has been selected. Real Windows UI/clipboard testing and Windows 10/11 clean-install validation remain mandatory before Windows 1.0.
