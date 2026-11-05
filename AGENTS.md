# CopyFix — Repository Agent Instructions

These rules govern automated development, reviews, audits, refactors, and release work for CopyFix.

## Provenance

- The mandatory two-pass repository-wide audit/refactoring protocol is adapted from [WindowTidy AGENTS.md](https://github.com/StanleyLl0yd/WindowTidy/blob/main/AGENTS.md) (Git blob `5c54bba3bcea3797ff16d9059e0827274b382206`). The same protocol is used by Password Generator, Everon, and FootClubSim.
- Approved raster icon preservation is adapted from [Password Generator AGENTS.md](https://github.com/StanleyLl0yd/password-generator/blob/main/AGENTS.md) (Git blob `060a30dd8e09b698c0aa0c7b6845d3003811ae7e`) and [SeaBattle AGENTS.md](https://github.com/StanleyLl0yd/SeaBattle/blob/main/AGENTS.md) (Git blob `f3510d0f07e55c8b90bf6b29cdac428cd93b6879`).

## CopyFix invariants

- CopyFix is an offline portable **C++20/Win32** utility targeting Windows 10/11 x64. Preserve native Win32 UI, system RichEdit, small dependency surface, and the reviewed functionality. No Electron, Qt, .NET desktop UI, cross-platform runtime, network dependency, telemetry, tracking, advertisements, OCR or AI unless explicitly approved.
- Release builds use MSVC v143, **Release /MT static CRT**. CI's user-facing ZIP contains **exactly one portable CopyFix.exe**. Do not ship installers, external runtime DLLs, settings files, logs or reports. System Windows DLL imports are permitted. Preserve PE ASLR, NX, CFG and dependency checks.
- Persist preferences **only in `HKEY_CURRENT_USER\Software\CopyFix`** (REG_DWORD `Profile`, `Hotkey`, and, starting with the unshipped Windows 1.2.0 draft, `Language` and `Theme`). Do not use INI or other sidecar settings files, HKLM, administrator elevation, or writable EXE-directory assumptions. Deep is the first-run profile default; language choice defaults to automatic Windows UI language (Russian or English fallback), with explicit Russian and English overrides. Theme defaults to Windows automatic and supports explicit Light/Dark overrides, always respecting system high-contrast mode. Preserve saved choices and legacy Profile/Hotkey values.
- Preserve Gentle, Smart and Deep behavior, source text fidelity, Unicode/UTF-16 validation, CRLF handling, paragraphs, meaningful hyphens, structured-data protection, input limits, explicit previews for ambiguous changes and bounded diff calculations.
- Clipboard access must be bounded and safe: sequence and content checks, stale-preview rejection, no replacing newer clipboard content on Undo, explicit risk handling for rich formats, and no logging or retention of clipboard contents. Acknowledged Win32 clipboard rollback limitations must not be concealed.
- Preserve tray icon, configurable global hotkeys, Explorer recovery, DPI/readable UI, keyboard handling and scroll stability. Native CI smoke tests do **not** substitute for real Windows desktop or clean-system acceptance.
- Use focused branches, exact-head CI, protected squash-only main, and post-merge verification. Do not bypass branch deletion/force-push protection. Ensure a required PR rule is enabled when available; do not claim it exists until verified.
- Every external GitHub Action must be pinned by immutable 40-character SHA. Keep default workflow tokens read-only/least-privileged. No unsafe `pull_request_target` execution, secrets, signing keys or sensitive user content in source or CI artifacts.
- Keep source-code comments minimal and **English only**; retain essential invariant explanations and remove stale comments.
- Windows **v1.0.0 was published and accepted on 2026-11-03** ([release](https://github.com/StanleyLl0yd/copyfix/releases/tag/v1.0.0), [issue #7](https://github.com/StanleyLl0yd/copyfix/issues/7)). Never claim specific OS versions, signatures, UI/manual behavior or CI checks PASS unless verified. New tags and future GitHub Releases still require explicit owner authorization and the relevant acceptance. Android development is not started; although the Windows 1.0 prerequisite is satisfied, do not start Android work without separate owner authorization.
- Continue from the verified current state, issues, CI and milestone rather than restarting completed work without reason. Do not rewrite working code solely for style; never invent statistics.

## Mandatory repository-wide audit and deep-refactoring protocol

When a task explicitly requests a full audit, security audit, repository-wide code review, cleanup, optimization, simplification, or deep refactoring, the rules in this section are mandatory. Full means the entire repository, not just changes, recently edited files, or known hot spots.

A repository-wide audit is an implementation task, not merely a request for recommendations.

### Objective

Audit the entire repository and reduce the codebase to the minimum necessary complexity while preserving 100% of its current functionality and externally observable behavior.

The refactored project must preserve, unless an explicitly requested bug fix requires otherwise:

- user-facing functionality;
- application behavior;
- UI/UX;
- business or game logic;
- public APIs and contracts;
- persisted and exchanged data formats;
- platform-specific behavior;
- documented capabilities;
- edge-case semantics.

The goal is minimum **necessary complexity**, not minimum line count.

Do not perform code golf.

A change is justified only when it objectively reduces one or more of:

- code volume;
- duplication;
- conceptual complexity;
- branching or state;
- coupling;
- maintenance burden;
- dependency surface;
- runtime cost;
- regression risk.

If a change merely makes the implementation different without making it demonstrably smaller, simpler, safer, clearer, or more efficient, leave the code unchanged.

Default decision rule:

- if code is proven unnecessary, delete it;
- if code can be objectively simplified, simplify it;
- if duplicate responsibilities can be safely consolidated, consolidate them;
- if an abstraction no longer provides value, remove it;
- if the benefit is uncertain, preserve the existing working behavior.

### Required audit scope

Before making repository-wide refactoring changes, inspect the complete repository rather than only recently changed files or obvious hotspots.

Review all relevant areas, including:

- production source code;
- tests and test utilities;
- resources and assets;
- configuration files;
- build scripts and build configuration;
- CI/CD workflows;
- release tooling;
- dependencies and development dependencies;
- static-analysis and security configuration;
- documentation;
- platform-specific integration;
- generated-code integration points;
- directory and module structure.

First establish:

1. the actual architecture;
2. authoritative sources of state and business logic;
3. all current user-visible functionality;
4. important internal behavior and contracts;
5. persistence and compatibility requirements;
6. framework-, build-, platform-, or convention-driven entry points.

Only then begin removing or consolidating code.

### Required removal candidates

Actively search for and remove, when proven safe:

- dead code;
- unreachable code;
- unused functions;
- unused classes;
- unused methods;
- unused variables;
- unused constants;
- unused types;
- unused interfaces;
- unused imports and exports;
- unused files;
- unused resources and assets;
- obsolete legacy code that no longer participates in current behavior;
- temporary workarounds whose original constraint no longer exists;
- duplicate or near-duplicate implementations;
- unnecessary abstraction layers;
- unnecessary wrappers;
- unnecessary helper functions;
- adapter chains that add no meaningful semantics;
- redundant data conversions;
- redundant intermediate objects;
- redundant intermediate state;
- redundant copies and transformations;
- defensive checks for states already guaranteed by types, architecture, invariants, or earlier validation;
- repeated validation of already validated data;
- repeated checks of the same condition;
- redundant fallbacks;
- obsolete compatibility branches;
- unused feature flags;
- obsolete configuration options;
- unused dependencies;
- unused development dependencies;
- duplicate-purpose dependencies;
- commented-out historical code;
- stale TODO/FIXME items;
- leftovers from previous migrations or refactors.

Pay particular attention to patterns such as:

- repeated null/state/bounds checks;
- nested checks of the same condition;
- repeated validation across adjacent layers;
- unnecessary `try/catch` blocks;
- wrapper -> wrapper -> wrapper call chains;
- DTO/model conversion chains;
- multiple representations of the same authoritative state;
- temporary states that duplicate existing state;
- compatibility paths left behind after migrations;
- helpers with only one trivial caller;
- abstractions created for future scenarios that never materialized.

### Proving code is unused

Do not classify code as unused solely because a textual search finds no direct call.

Before removing anything, check for indirect or convention-driven use through:

- callbacks;
- events;
- observers;
- dependency injection;
- reflection;
- dynamic loading;
- serialization/deserialization;
- framework conventions;
- lifecycle hooks;
- manifests;
- resources;
- routing;
- configuration;
- generated code;
- build scripts;
- CI/CD;
- release tooling;
- plugins;
- native/platform integration;
- test or debug tooling;
- external/public contracts.

When uncertainty remains, preserve the code until its lack of use can be demonstrated.

### Required simplification review

Look for opportunities to safely:

- shorten code without reducing readability;
- simplify control flow;
- reduce nesting;
- reduce mutable state;
- reduce the number of branches;
- merge equivalent paths;
- merge components with the same responsibility;
- remove unnecessary temporary values;
- remove unnecessary objects and allocations;
- remove redundant copies;
- remove repeated transformations;
- remove redundant loops or passes over the same data;
- compute invariant values once instead of repeatedly;
- replace custom code with standard language, platform, framework, or library facilities when clearly simpler;
- eliminate repeated business logic;
- centralize genuinely shared logic when doing so reduces total code and conceptual complexity;
- reduce coupling;
- remove premature generalization;
- remove architecture that exists only for hypothetical future requirements.

Do not introduce an abstraction merely to satisfy DRY.

Similar-looking code should only be unified when the resulting abstraction reduces real duplication and overall complexity.

### Architecture review

Explicitly determine whether:

- historical layers are still necessary;
- historical components are still necessary;
- abstractions are disproportionate to the actual problem;
- interfaces exist with only one implementation and no meaningful boundary benefit;
- classes or modules can be safely merged;
- classes or modules can be safely deleted;
- responsibilities are unnecessarily fragmented;
- there is premature generalization;
- there is speculative extensibility;
- infrastructure exists only for hypothetical future features;
- current architecture reflects requirements that no longer exist;
- duplicated state or business logic creates unnecessary coupling;
- implementation complexity is proportional to actual product complexity.

Prefer the simplest architecture that fully supports the current product.

Do not perform a large rewrite merely because another architecture appears cleaner, newer, or more fashionable.

### Performance review

Review performance only where there is practical value.

Look for:

- repeated calculations;
- repeated queries;
- repeated reads;
- unnecessary parsing;
- repeated transformations of unchanged data;
- avoidable allocations;
- unnecessary copies;
- redundant loops;
- multiple passes that can safely become one;
- unnecessary render/re-render/rebuild/recompute work;
- inappropriate data structures for actual access patterns;
- invariant operations performed repeatedly instead of once;
- unnecessary work in frequently executed paths.

Do not introduce micro-optimizations that reduce readability or maintainability without an obvious or measurable benefit.

### Dependency review

Review every production and development dependency.

For each dependency:

- verify that it is actually used;
- verify that its purpose is still required;
- remove unused dependencies;
- remove obsolete dependencies;
- identify multiple libraries serving the same purpose;
- consolidate overlapping dependencies where safe;
- prefer standard language/platform/framework functionality when it clearly replaces a dependency with less total complexity.

A dependency may be replaced by a small local implementation only when doing so clearly reduces overall complexity, risk, maintenance cost, or artifact size.

Do not replace a mature, well-maintained library with custom code merely to reduce the dependency count.

### Non-negotiable behavior-preservation rules

Unless explicitly requested, a repository-wide cleanup or refactor must not intentionally:

- remove user-facing functionality;
- change existing UX;
- change visual behavior;
- change business logic;
- change game rules;
- change public APIs;
- change public contracts;
- change persisted data formats;
- change exchanged data formats;
- change edge-case behavior;
- change platform behavior;
- weaken compatibility;
- weaken accessibility;
- weaken security or privacy controls;
- reduce functionality merely to reduce code size;
- add unrelated functionality;
- introduce new architectural layers only to satisfy generic best practices;
- perform a broad rewrite without a demonstrated need.

Repository-specific invariants defined elsewhere in this file remain mandatory throughout the refactor.

### Required execution workflow

For a repository-wide audit or deep refactor:

1. Inspect the entire repository.

2. Establish the current:
   - architecture;
   - feature set;
   - user-visible behavior;
   - internal contracts;
   - authoritative state;
   - persistence behavior;
   - platform constraints.

3. Collect reliable baseline statistics where practical.

4. Build an internal candidate list grouped into:
   - deletion;
   - consolidation;
   - simplification;
   - architecture reduction;
   - dependency cleanup;
   - practical performance optimization.

5. Validate every removal against both direct and indirect usage.

6. Rank candidates by:
   - confidence;
   - regression risk;
   - complexity reduction;
   - maintenance benefit.

7. Apply changes in small, logically coherent groups rather than as one uncontrolled rewrite.

8. After every meaningful group, run the relevant available checks, such as:
   - unit tests;
   - integration tests;
   - regression tests;
   - lint;
   - formatting checks;
   - type checking;
   - compilation;
   - build;
   - static analysis;
   - dependency/security checks;
   - project-specific validation.

9. If critical behavior lacks sufficient test coverage for a contemplated risky change, first add the minimum regression test required to capture the existing behavior.

10. Keep behavior-preserving refactoring separate from unrelated feature development.

11. When a candidate cannot be proven safe, leave it unchanged and record the reason.

12. Complete the first refactoring pass.

13. Perform a mandatory second full pass over the already-refactored repository.

14. During the second pass, look again for:
   - newly exposed simplification opportunities;
   - remaining dead code;
   - residual duplication;
   - unnecessary abstractions;
   - unnecessary wrappers;
   - redundant checks;
   - unused dependencies;
   - residual legacy;
   - temporary structures made obsolete by the first pass.

15. Finish with the complete available project verification suite.

### Validation requirements

The final verification should include every applicable repository check.

Examples include:

- clean build;
- complete unit-test suite;
- integration tests;
- regression tests;
- lint;
- formatting verification;
- type checking;
- static analysis;
- dependency audit;
- security checks;
- platform-specific builds;
- production build;
- project-specific verification scripts.

Do not claim a check passed unless it was actually executed successfully.

If a check cannot be run because of environment, credentials, hardware, platform, unavailable tooling, or another external limitation, state that explicitly.

### Comments during refactoring

Preserve the repository's comment policy.

As a default:

- keep comments to the minimum necessary;
- keep source-code comments in English;
- remove stale comments;
- remove misleading comments;
- remove redundant comments;
- do not add comments that merely narrate obvious code;
- retain comments that explain a non-obvious reason, constraint, workaround, invariant, compatibility requirement, or important contract.

Do not replace clear code with explanatory comments when the code itself can be made self-explanatory.

### Completion standard

A repository-wide audit/refactoring task is not complete merely because:

- code was formatted;
- static analysis was run;
- recommendations were listed;
- potential improvements were described;
- only recently modified files were reviewed.

Safe and justified improvements must be implemented directly.

The final codebase should be objectively smaller, simpler, less duplicated, less coupled, easier to reason about, or easier to maintain while preserving behavior.

### Required final report

At completion, report:

1. **Removed**
   - dead code;
   - unused files/resources;
   - obsolete compatibility code;
   - unnecessary abstractions;
   - other removed components.

2. **Consolidated**
   - duplicated logic;
   - equivalent components;
   - repeated validation;
   - shared responsibilities.

3. **Simplified**
   - control flow;
   - state;
   - architecture;
   - transformations;
   - hot paths.

4. **Dependencies**
   - dependencies removed;
   - duplicate-purpose dependencies consolidated;
   - dependency changes intentionally not made and why.

5. **Legacy**
   - legacy components found;
   - which were removed;
   - which remain and why.

6. **Intentionally unchanged**
   - candidates reviewed but preserved;
   - reason removal or simplification could not be proven safe.

7. **Verification**
   - tests run;
   - builds run;
   - lint/typecheck/static-analysis/security checks run;
   - their results.

8. **Limitations**
   - areas that could not be safely optimized;
   - missing coverage;
   - unavailable environment/tooling;
   - external constraints.

9. **Before/after statistics**, when they can be measured reliably:
   - file count;
   - source line count;
   - dependency count;
   - test count;
   - production/build artifact size;
   - other project-relevant metrics.

Never invent statistics.

Use the same counting method for before and after values.

### Final principle

When choosing between:

- preserving working code whose necessity is uncertain;
- deleting code because it appears unnecessary;

preserve the working behavior until the code is proven unnecessary.

The final goal is a codebase containing only the complexity required to implement the project's current functionality: as small, clean, understandable, maintainable, and efficient as reasonably possible, without functional regressions.

## Mandatory Windows 1.0 exit audit

- Before declaring Windows 1.0 accepted, closing [issue #7](https://github.com/StanleyLl0yd/copyfix/issues/7), or proceeding to another platform, perform the complete **repository-wide security audit, code review and safe refactoring**, including both full passes described above. This is an implementation and remediation task, not a list of advice.
- Inspect all source, tests, assets, Win32 integration, registry, clipboard, hotkeys, resources/icons, CMake, /MT, scripts, workflows, dependencies, documentation and binary packaging. Validate indirect usage before removals; preserve current behavior unless fixing a documented bug.
- Verify exact-head MSVC v143 x64 Release /MT, full native CTest suite, PE imports/ASLR/NX/CFG, one-EXE artifact, actual Windows 10/11 runtime, clipboard/Undo/large text, hotkey conflicts, HKCU persistence, DPI/scroll and no-extra-runtime operation. Use strict compilation and sanitizers where supported; explicitly disclose checks unavailable on the current host.
- Record first and second pass coverage, findings, implemented fixes, untouched candidates with reasons, reliable measurements, final-main CI and remaining blockers in the exit issue or linked report. Only declare acceptance after the required evidence is present. **A release and license require separate explicit owner decisions.** Apply equivalent exit gates to later milestones, never replacing a full audit with a diff-only review.

## Canonical CopyFix app icon

- The owner-selected current design direction is a **bright-blue rounded-square background, white document and vivid green sweeping arrow**; the large artwork also contains “Aa”. The owner separately requested an **optically optimized 16×16** version for the Windows tray. Do not substitute a different concept without approval.
- The owner-approved original full-resolution PNG is committed at **`artwork/app-icon.png`** (1254×1254; SHA-256 `24be921d182bc7030fea1290167043fc597cd266a0da167cc963eb3cf73c25a1`). Preserve the **exact original raster bytes** and source provenance: the selected CopyFix icon from the preceding user-approved design. Never replace it with a regenerated approximation.
- The owner-approved pixel-optimized 16×16 PNG is committed at **`artwork/tray-16x16.png`** (SHA-256 `d0d26a34c55bec5ffd258cc4f7ad9191b934ad433a71f6feeb5cde96ca2f6e64`). This is a **separately approved, size-specific canonical artwork**, not an automatic downscale of the master. Preserve its bytes and use it directly for the 16×16 ICO frame and system tray.
- Never trace, redraw, regenerate, restyle, vectorize, convert to SVG/vector PDF, recolor, crop, pad, sharpen, remove details, recompress, optimize in place, or overwrite either approved canonical PNG without a new explicit owner request. Never change these assets during routine code cleanup or full refactoring.
- Keep original source PNGs and their hashes in Git. Derive **separately named raster platform resources** (e.g. PNG dimensions and ICO) without changing the approved visible design. Use the separately approved 16×16 PNG for small tray/ICO entries; larger ICO entries may be faithfully resampled from the canonical full-size PNG. Do not infer that raw downsampling is readable at 16×16.
- Keep the icon pipeline reproducible and local. For Windows ICO verify 16, 24, 32, 48, 64, 128 and 256 px where source quality permits; ensure correct resources are embedded in **the single EXE**, not loaded from runtime sidecars. Preserve Windows taskbar, tray, Explorer and high-DPI resolution behavior.
- Visually validate native 16×16 on both light and dark Windows taskbars and inspect 20× nearest-neighbor enlargement; verify larger sizes separately. CI binary/resource checks do not replace owner visual approval. No mockup, comparison image or generated collage can be substituted for the actual raster icon.
- When an explicit owner-approved replacement arrives, update canonical source, documented hashes, derivatives, embedded resources and references together in a focused PR. Keep previous approved versions in Git history.

## Normal change workflow

1. Inspect current main, open PRs/issues, relevant dependencies, tests and actual CI; reuse verified context.
2. Make minimal coherent changes on a dedicated branch. Add regression tests for important behavior, clipboard safety or security changes.
3. Run targeted checks and exact-head Windows CI, including static CRT/PE checks and the applicable security validation. Do not weaken tests merely to obtain green status.
4. Verify changed files, required checks, mergeability and branch ancestry; merge via squash, then verify exact-main CI. Do not claim CI, build, visual approval or release status without evidence.
5. Keep README, QA status, version and release notes consistent. No license or GitHub Release without authorization.
6. For any requested full audit/security audit/review/refactor, follow **every** step of the full two-pass protocol above; for a small change, do not create unrelated churn.
