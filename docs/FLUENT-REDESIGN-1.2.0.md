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
- Preferences remain in `HKCU\Software\CopyFix`. Existing REG_DWORD `Profile` and `Hotkey` retain compatibility; 1.2.0 adds optional `Language` (0 automatic Windows UI language, 1 Russian, 2 English). Missing or invalid values default to automatic. Cancel never commits a draft language.
- Window sizes and initial locations are scaled to the **effective DPI of the monitor under the cursor**. Every window is centered using measured dimensions.
- No third-party UI runtime, additional shipped files, network calls, telemetry or external assets. The approved artwork remains `artwork/app-icon.png` and `artwork/tray-16x16.png` with embedded `resources/copyfix.ico`.

## Automated gates

- Shared 141 JSON golden fixtures, core Unicode and data-structure invariants remain unchanged.
- Windows test suite: core, preview diff, performance/integrity, preview scroll, clipboard contract, About positioning/version/links, settings persistence/hotkey rollback/segment selection, VersionInfo, canonical icon and light/dark GDI surfaces.
- Clang AddressSanitizer/UndefinedBehaviorSanitizer, archive and fixture integrity checks.
- PE hardening (ASLR/NX/CFG), native DLL imports, MSVC `/MT`, single portable EXE on GitHub Actions.
- Protected PR exact-head checks and user acceptance **before** merging or publishing another GitHub Release.


## Owner visual findings and corrective pass (2026-11-05)

**Observed environment:** Windows 11 Pro 25H2, OS build 26200.9168. The initial draft failed visual acceptance. This pass remains **Draft** and is not a release approval.

- Hotkey dropdown displayed corrupted symbols because `CBS_OWNERDRAWFIXED` lacked `CBS_HASSTRINGS`; fixed string retention and validated every visible list label with `CB_GETLBTEXT`. The dropdown uses compact rows with a bounded list height.
- Settings card layout clipped profile explanatory text and collided with the footer; rebalanced card dimensions, gaps and buttons. Added explicit Win32 child-rectangle assertions.
- Reduced oversized fonts to Segoe UI 10 pt body / 17 pt headings / 9 pt labels and captions, with per-monitor DPI font generation retained.
- Compressed Settings, Preview and About, preserving approved Settings C / Preview A / About C, card boundaries, two RichEdit panes, and the canonical icon. Added preview resizing and About non-overlap regressions.
- Replaced aliased GDI-line check glyph with a Windows Segoe UI Symbol checkmark drawn by the system font rasterizer. No external UI dependency or artwork change.
- Existing engine, clipboard, warning/rollback behavior, profiles, Registry storage and portable packaging are unchanged.

**Still required before merge/release:** exact-head Windows and Clang CI, visual repeat on the owner's Windows 11 for all three windows and both themes (especially 100%/150%/200% DPI, the full keyboard list, labels and resize behavior). Automated tests cannot independently certify visual fidelity. Do not merge or publish without explicit owner acceptance.

## Native language selection (further owner feedback, 2026-11-05)

The initial draft did not follow the Windows UI language: all user-facing strings were hardcoded in Russian. Settings now has a third native card with `System default (Windows)`, `Русский`, and `English`. The RU/EN translation catalog covers all three windows, tray menu, notifications and shortcut validation. Automatic mode uses the current Windows UI language (Russian if the primary UI language is Russian, otherwise English); it can be overridden independently of the OS. Settings previews the chosen language without committing it until Save. Saving updates already-open Preview/About windows and the tray tooltip.

The Settings window is taller to accommodate the language card. The shortcut popup shows at most six visible entries to reduce overlap with footer actions. No extra shipped DLLs/assets/runtime. Owner visual acceptance is still required for both languages and both themes, keyboard navigation, DPI, high contrast and native ComboBox popup rendering.

## Final native UI polish (2026-11-05)

Following the owner's light-theme screenshots, the current layout is considered acceptable in principle, **not yet released or fully accepted**. This final small pass avoids any architecture change:

- Preview: raise the status text contrast by blending the theme's muted and ink colors, and tighten the vertical gap to the footer toggles while keeping the action buttons aligned.
- About: increase the canonical icon's displayed size from 48 to 54 DIPs; load the corresponding native icon size at the window DPI and refresh it when moving between monitors.
- Settings: retain the bounded six-row native ComboBox popup. Windows scrolls the popup to the currently selected F9, so F1–F8 are above the visible region, **not missing**. The popup normally overlays lower controls; forcing it to open upward would require fragile native list-window repositioning, so it is intentionally not overridden.
- Added precise Win32 smoke assertions for all supported F1–F24 function-key labels except Windows-reserved F12, popup bounds, footer geometry/status contrast and About icon geometry.

No text transformation, clipboard, Undo, registration, persistence, language selection, theme preference or packaged canonical icon source is modified by this pass. Manual **dark-theme** and mixed-DPI screenshots are still required before considering merge/release.

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
