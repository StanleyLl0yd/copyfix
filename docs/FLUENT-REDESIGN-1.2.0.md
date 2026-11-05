# CopyFix Windows 1.2.0 — Approved Fluent UI redesign

**Status:** Draft implementation, owner visual acceptance pending. The released Windows 1.1.0 is immutable.

## Design contract (approved 2026-11-05)

| Window | Approved concept | Implementation intent |
| --- | --- | --- |
| Settings | **C**, light and dark | Rounded cards; native Ctrl / Alt / Shift checkboxes; complete supported-key selector; **three segmented profile buttons, no radio buttons**; concise help and validation feedback |
| Preview | **A**, light and dark | Dual read-only native RichEdit surfaces; restrained rose/green difference highlighting; two cosmetic formatting toggles; Apply/Cancel remain explicit |
| About | **C**, light and dark | Centered native window, **unchanged canonical icon**, live version label, verified website/GitHub links, MIT and copyright |

## Runtime architecture

- **C++20/Win32 + GDI + DWM**. System **dwmapi.dll** and **shcore.dll** are already supplied with Windows 10/11; they are native operating-system libraries, not distributed dependencies.
- The app follows the Windows **AppsUseLightTheme** preference. It rereads the value when opening windows and upon `WM_SETTINGCHANGE`/`WM_THEMECHANGED`. If the value is unavailable, use the light palette. No theme preference is stored in CopyFix's Registry key.
- On Windows 11 DWM draws the standard system caption and rounded outer corners. On Windows 10 unsupported DWM styling attributes fail harmlessly; render the client area as opaque surfaces.
- Use **native Win32 BUTTON/COMBOBOX** controls, with scoped drawing/subclassing where necessary. Preserve focus, keyboard input and checkbox semantics; do not install keyboard hooks.
- Keep the existing **Msftedit.dll RichEdit** controls, Unicode/UTF-16 handling, scroll preservation, diff ranges, clipboard safety and confirm/undo contracts.
- All user preferences remain the existing `HKCU\Software\CopyFix` REG_DWORD values `Profile` and `Hotkey`.
- Window sizes and initial locations are scaled to the **effective DPI of the monitor under the cursor**. Every window is centered using measured dimensions.
- No third-party UI runtime, additional shipped files, network calls, telemetry or external assets. The approved artwork remains `artwork/app-icon.png` and `artwork/tray-16x16.png` with embedded `resources/copyfix.ico`.

## Automated gates

- Shared 141 JSON golden fixtures, core Unicode and data-structure invariants remain unchanged.
- Windows test suite: core, preview diff, performance/integrity, preview scroll, clipboard contract, About positioning/version/links, settings persistence/hotkey rollback/segment selection, VersionInfo, canonical icon and light/dark GDI surfaces.
- Clang AddressSanitizer/UndefinedBehaviorSanitizer, archive and fixture integrity checks.
- PE hardening (ASLR/NX/CFG), native DLL imports, MSVC `/MT`, single portable EXE on GitHub Actions.
- Protected PR exact-head checks and user acceptance **before** merging or publishing another GitHub Release.

## Pending manual acceptance

- [ ] Windows 11 light and dark, all **three** windows side-by-side against the approved mockups.
- [ ] Windows 10 opaque light/dark fallback; standard title bar and corners do not break.
- [ ] Change the Windows app theme with Settings/Preview/About open; client colors, text, diff and title bar update without restarting.
- [ ] 100%, 150%, 200% DPI, including opening on a monitor with a different effective DPI and resizing/dragging between monitors.
- [ ] Check dark dropdown visibility, standard checkboxes, segmented profile selection, tab order, focus cues, keyboard shortcuts and screen-reader labels.
- [ ] Repeat previously accepted occupied-hotkey warning, default Ctrl+Alt+F9, save/cancel, settings persistence and profile behavior.
- [ ] Long PDF text, Unicode, rich-format warning, highlighted differences, whitespace/break markers, both independent scroll positions, Apply/Cancel and Undo.
- [ ] About uses the canonical app icon, correct runtime version and valid official project/GitHub links.
- [ ] No new startup windows, no loss of tray functionality or one-EXE packaging.

The application is **not** released at 1.2.0 until the owner reviews the new build and explicitly approves release.
