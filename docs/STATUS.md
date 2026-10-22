# CopyFix validation record - 2026-10-22

| Item | Actual status |
| --- | --- |
| Repository owner authenticated with GitHub connector | VERIFIED, `StanleyLl0yd` |
| `StanleyLl0yd/copyfix` remote repository | NOT FOUND (404) |
| Local source tree and Git history | PREPARED locally |
| Native Win32 GUI, tray, hotkey, clipboard, preview, Undo | CODE WRITTEN; WINDOWS BUILD NOT VERIFIED |
| C++ text engine | BUILT and TESTED on Linux x64 with Clang/GCC |
| Golden corpus | 118 JSON cases + 25 built-in cases |
| Determinism/idempotency checks | 900 property-like cases PASS locally |
| Strict Clang diagnostics | PASS, `-Werror -Wall -Wextra -Wpedantic -Wconversion -Wshadow` |
| ASan and UBSan | PASS for core test suite locally |
| 10k, 100k, 1M units | BASIC INTEGRITY PASS locally |
| MSVC v143 Windows executable | NOT VERIFIED (no Windows SDK in current environment) |
| `/MT` Release linker configuration | CONFIGURED, NOT VERIFIED in binary |
| PE imports, hardening, size, clean Win10/11 launch | NOT VERIFIED |
| GitHub Actions | WORKFLOW WRITTEN, NOT RUN |
| Live clipboard integration and tray behavior | NOT VERIFIED |
| Main branch protection, Dependabot and repo scanning | NOT CONFIGURED (remote missing) |
| GitHub Release / Windows 1.0 | NOT CREATED |
| Android | NOT STARTED (deferred until Windows 1.0) |

No license has been chosen. The current files are a Windows prototype, **not a certified portable release**.
