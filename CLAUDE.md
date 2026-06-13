# CLAUDE.md

Guidance for working in the **Olympia G2** repository.

## What this is

G2 is the second-generation Olympia play-by-mail (PBM) strategy game engine
(~53K lines of C), descended from G1 and the ancestor of the later G3/TAG
engines (G2 adds HTML reports and tunnels). This repo is a standalone extraction
of G2 from the original multi-engine monorepo; it builds on its own with CMake.

The code is legacy C originally targeting **32-bit** systems. An active
modernization effort is bringing it to clean **C11 on 64-bit**. See
[Modernization status](#modernization-status) and the full
[BUILD_HISTORY.md](BUILD_HISTORY.md) — read them before changing build flags or
touching prototypes/headers.

## Build

Requires CMake (>= 4.1), Ninja, and Clang or GCC.

```bash
cmake --preset debug
cmake --build --preset debug
# Binaries: build/debug/olympia-g2, build/debug/mapgen-g2
```

Presets (`CMakePresets.json`): `debug` (default), `release`, `asan-ubsan`.
The `asan-ubsan` preset sets `OLYMPIA_SANITIZE=ON` with address+undefined.

## Test — golden snapshots (must stay green)

Any change must keep the golden tests passing. The olympia check printing `YES`
is the gate.

```bash
./run/mapgen/mapgen.sh                     # generate gate/loc/road
./run/olympia-g2.sh                        # extract fixtures, run a turn, save DB
./tests/olympia/golden_check.sh            # YES = match (exit 0)
./tests/olympia/golden_check.sh --update   # refresh snapshot (only when output is intentionally changed)
```

Scripts auto-detect the repo root and look for binaries at
`build/<preset>/<target>` (override with `OLYMPIA_PRESET=release ...`). Run the
sanitizer gate with `OLYMPIA_PRESET=asan-ubsan` against the same golden flow.

> The golden gate is deterministic because the engine is run with
> `test-use-const-report-date`, which pins the Olympia Times masthead date so its
> sha256 doesn't drift day-to-day. It affects **only** the newsletter date — all
> other output is byte-identical. See
> [BUILD_HISTORY.md](BUILD_HISTORY.md#deterministic-newsletter-date-test-use-const-report-date-issue-2).

## Layout

- `olympia/` — G2 engine sources (54 `.c`) and headers
- `mapgen/` — map generator (`mapgen.c`, `z.c`, `rnd.c`)
- `lib/` — shared support code (entity lists, tiles, roads, allocation, …)
- `tests/` — golden fixtures + golden snapshots for olympia and mapgen
- `run/` — run/test driver scripts and scratch run dirs
- `doc/` — assorted G2 design/reference notes, plus the modernization playbook
  and `phase4-tools/`

## Conventions

- Legacy C style: tabs, ANSI prototypes for definitions, terse names. Match the
  surrounding file; don't reformat untouched code.
- **Golden output is the contract.** Behavior changes that alter engine output
  must be deliberate and the snapshot updated in the same change with a note on
  why. Modernization changes (prototypes, casts, dead-code removal) must produce
  byte-identical golden output.
- Build config lives in `CMakeLists.txt`. There is **one flag set for the whole
  project**: `olympia_compile_flags(tgt)` holds every warning flag (suppressions
  + the locked-in `-Werror` classes) and is applied identically to both targets;
  `olympia_enable_sanitizers(tgt)` adds the optional asan/ubsan instrumentation.
  Enforced classes are written as a deliberate `-Wfoo -Werror=foo` pair, one per
  line, as a record of completed modernization work — don't collapse the pairs.
  Optimization level is driven by `CMAKE_BUILD_TYPE` (via the presets), not
  hardcoded per target. Read the conventions block at the top of `CMakeLists.txt`
  and the [Warning policy](BUILD_HISTORY.md#warning-policy) before touching flags.
- **C11 standard** is set project-wide (`CMAKE_C_STANDARD 11` / `…_REQUIRED ON` /
  `CMAKE_C_EXTENSIONS OFF`) and also declared per target via
  `target_compile_features(<tgt> PRIVATE c_std_11)` (documentation/intent only —
  inert because the global standard already forces `-std=c11`).
- **No CI workflows.** This repo does not use CI (no `.github/workflows`).
  Maintainer decision (2026-06-13, issue #9): gates are run locally — the golden
  gate via `./tests/olympia/golden_check.sh`, and the sanitizer gate via
  `OLYMPIA_PRESET=asan-ubsan` against the same golden flow. Don't propose or wire
  CI even when an issue's text asks for it.

## Git workflow

- **Do not create git branches until explicitly asked.** Do the work as commits
  on the current branch (usually `main`). Branches and PRs are created only when
  the user says they are ready to deal with PRs — wait for that request.
- Likewise, do not push or open PRs unless explicitly asked.

## Modernization status

The C11/64-bit modernization ran as a phased warning ladder and is **complete**.
Every targeted warning class is now enforced as `-Werror` via
`olympia_compile_flags()` in `CMakeLists.txt` (applied to both targets), and the
golden flow runs clean under AddressSanitizer + UndefinedBehaviorSanitizer. The
per-phase scaffolding has been removed; the remaining work is the actual
32→64-bit refactoring the ladder cleared the way for.

Enforced classes (all `-Werror`):

| Phase | Scope |
|-------|-------|
| 1 | `int-to-pointer-cast`, `pointer-to-int-cast` |
| 2 | `incompatible-pointer-types` |
| 3 | `int-conversion` |
| 3.5 | removed dead/unused source files |
| 4 | `strict-prototypes`, `missing-prototypes`, `implicit-function-declaration` |
| 5 | `missing-declarations` + sanitizers verified against the golden flow |
| 6 | `shorten-64-to-32` (Clang-guarded) + `sizeof-pointer-memaccess` |
| 7 | `sign-conversion` |
| 8 | `return-type` + `return-mismatch` |
| 13 | `implicit-int-conversion` (Clang-guarded, code-quality — not a 64-bit hazard) |

Also locked in: `-Wformat`/`-Werror=format` vararg checking (project-wide), flag
consolidation into `olympia_compile_flags()` with the dead `SYSV` branch removed
(#14), and a deterministic newsletter date for reproducible goldens (#2).

**Before changing build flags, prototypes, or headers, read
[BUILD_HISTORY.md](BUILD_HISTORY.md)** — it has the full phase-by-phase record,
the traps each class hid, the rationale behind every locked-in flag, and the
[warning policy](BUILD_HISTORY.md#warning-policy) for triaging new warnings.
