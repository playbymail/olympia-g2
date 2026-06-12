# CLAUDE.md

Guidance for working in the **Olympia G2** repository.

## What this is

G2 is the second-generation Olympia play-by-mail (PBM) strategy game engine
(~60K lines of C), the ancestor of the later G3/TAG engines. This repo is a
standalone extraction of G2 from the original multi-engine monorepo; it builds
on its own with CMake.

The code is legacy C originally targeting **32-bit** systems. An active
modernization effort is bringing it to clean **C11 on 64-bit**. See
[Modernization status](#modernization-status) — read it before changing build
flags or touching prototypes/headers.

## Build

Requires CMake (>= 4.1), Ninja, and Clang or GCC.

```bash
cmake --preset debug
cmake --build --preset debug
# Binaries: build/debug/olympia-g2, build/debug/mapgen-g2
```

Presets (`CMakePresets.json`): `debug` (default), `release`, `asan-ubsan`.
The `asan-ubsan` preset sets `OLYMPIA_SANITIZE=ON` with address+undefined.

### 32-bit build (Linux only — for regenerating golden files)

```bash
mkdir build32 && cd build32
cmake -DBUILD_32BIT=ON ..   # requires gcc-multilib
cmake --build .
```

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
`build/<preset>/<target>` (override with `OLYMPIA_PRESET=release ...`).

## Layout

- `olympia/` — G2 engine sources (54 `.c`) and headers
- `mapgen/` — map generator (`mapgen.c`, `z.c`, `rnd.c`)
- `lib/` — shared support code (entity lists, tiles, roads, allocation, …)
- `tests/` — golden fixtures + golden snapshots for olympia and mapgen
- `run/` — run/test driver scripts and scratch run dirs
- `doc/` — assorted G2 design/reference notes, plus the modernization playbook
  and `phase4-tools/` (see below)

## Conventions

- Legacy C style: tabs, ANSI prototypes for definitions, terse names. Match the
  surrounding file; don't reformat untouched code.
- **Golden output is the contract.** Behavior changes that alter engine output
  must be deliberate and the snapshot updated in the same change with a note on
  why. Modernization changes (prototypes, casts, dead-code removal) must produce
  byte-identical golden output.
- Build config lives in `CMakeLists.txt`. The per-target compile flags are
  inlined into each `add_executable` block (see lines ~221 and ~266), *not*
  applied via the `phase_N_build_flags()` functions — those are roadmap
  scaffolding (defined, not yet called). `LEGACY_C_FLAGS_STRICT` is likewise
  staged but unused.

## Modernization status

A 5-phase ladder is documented as `phase_N_build_flags()` functions in
`CMakeLists.txt`. What is actually *enforced* is inlined into both targets as
`-Werror`. Current state:

| Phase | Scope | State |
|-------|-------|-------|
| 1 | `int-to-pointer-cast`, `pointer-to-int-cast` | ✅ enforced (`-Werror`) |
| 2 | `incompatible-pointer-types` | ✅ enforced |
| 3 | `int-conversion` | ✅ enforced |
| 3.5 | **Remove dead/unused source files** | ✅ done |
| 4 | `strict-prototypes`, `missing-prototypes`, `implicit-function-declaration` | 🔜 next |
| 5 | `missing-declarations` + sanitizers in CI | ⬜ wired (asan preset), not enforced |

Phases 1–3.5 are complete and locked in — the dangerous 32→64-bit pointer/int
hazards (bad casts and int/pointer conversions) build clean as errors, and the
dead source files are gone. The remaining work is Phase 4 (prototypes &
declarations), then Phase 5 (lock down).

> **Note:** the sister **G1** repo (`../olympia-g1`) has already completed
> Phase 3.5 and Phase 4. Its `CLAUDE.md` records exactly what those changes
> were, and its method is captured in `doc/modernization-prototypes-playbook.md`
> (copied into this repo) and the helper scripts in `doc/phase4-tools/`. Use
> them as the template for doing the same work here.

> **Caution on the prototype probe:** do **not** use `-Wold-style-definition` to
> find K&R *definitions* — clang reports those under
> `-Wdeprecated-non-prototype`. In G1 the wrong probe hid **95** K&R definitions
> (54 of them in the map generator); expect a comparable population here. See
> `doc/modernization-prototypes-playbook.md`.

### Phase 3.5 — Remove dead/unused source files ✅ done

Deleted three `lib/*.c`/`.h` modules that are in **no** `target_sources` block
(never compiled or linked into either target — verified by grepping
`CMakeLists.txt` for each basename):

- `lib/effects.c`, `lib/entity_builds.c` — list modules referenced only by
  declarations in `lib/lists.h`; their list types (`effects_list`,
  `entity_builds_list`, `struct effect`, `struct entity_build`) had no use
  anywhere in the compiled tree. The matching declaration blocks were pruned
  from `lib/lists.h`.
- `lib/ring_buffer.c` (+`.h`) — self-contained; its only export `ring_printf`
  is called nowhere. (The `ring_buffer` symbols in `olympia/sout.c` are an
  unrelated local static array.)

**Differs from G1:** G1 also removed `accept_ents.c` and `checked_alloc.c`, but
in **G2 both are live** — they appear in `olympia-g2`'s `target_sources` (and
`checked_alloc.h` in its header set), so they were kept.

**Retained: `lib/plist.c`** (pointer list) and its `lib/lists.h` declarations.
Unlike `ilist` (which stores `int`), `plist` stores `void *`, so it is wanted by
the 64-bit refactoring wherever pointers are kept in a list. It is not wired
into any build target yet; add it to the appropriate `target_sources` in
`CMakeLists.txt` when the first caller appears. Recover any deleted file from
git history if ever needed.

Verified clean rebuild and `./tests/olympia/golden_check.sh` → `YES`. **Caveat:**
G2 has a pre-existing build-to-build non-determinism — a single `st -32` line in
`fact/100` flickers across clean rebuilds of *byte-identical* source (proven by
rebuilding the unmodified tree twice and observing the same flicker), so it is
not caused by this change. Working hypothesis: a missing-prototype / 64-bit bug
(implicit declaration truncating a pointer through `int`), so **Phase 4 may make
it deterministic** — re-check then. `golden_check.sh` holds `fact/100` out of
its hash manifest and tolerates only the lone `st -32` flicker (reporting it),
failing on any other diff; see the playbook's "Verification gate" note.

### Phase 4 — Prototypes & declarations (next)

Goal: make `strict-prototypes`, `missing-prototypes`, and
`implicit-function-declaration` build clean as `-Werror` on both targets, then
remove the dead `-Wno-implicit-function-declaration` /
`-Wno-deprecated-non-prototype` suppressions from `LEGACY_C_FLAGS`. Follow the
full method and trap list in `doc/modernization-prototypes-playbook.md`; the
helper scripts in `doc/phase4-tools/` automate the mechanical edits:

- `kr2ansi.py` — convert K&R definitions to ANSI prototypes.
- `fix_void_defs.py` — convert empty-paren `name()` definitions to `name(void)`.
- `gen_proto3.py` — generate a prototype header per target.
- `fix_comp.py` — canonicalize `qsort` comparators to
  `(const void *, const void *)`.

Expected shape of the work (from G1, adapt to G2):

- Convert all K&R definitions to ANSI and empty-paren definitions to `(void)`
  (probe with `-Wdeprecated-non-prototype`, *not* `-Wold-style-definition`).
- Generate a prototype header per target — `olympia/proto.h` (include at the
  end of `oly.h`, after the type defs and a `#include <stdio.h>` for `FILE`)
  and `mapgen/proto.h`. `z.c` doesn't include `oly.h`, so its functions are
  declared in `z.h`. This single move clears nearly all of `missing-prototypes`
  and the internal `implicit-function-declaration` calls.
- Delete redundant empty-paren forward declarations and give surviving header
  decls real prototypes.
- Add the real libc headers (`string.h`, `stdlib.h`, `unistd.h`, `time.h`,
  `fcntl.h`, `sys/stat.h`) at the **top of `z.h` / `mapgen/z.h`**, before the
  engine's `bzero`/`bcopy`/`abs` shadow macros (and ahead of `oly.h`'s `wait`
  macro, which collides with `sys/wait.h` on macOS). `z.h` is the chokepoint
  every engine TU includes first.
- Expect the new prototypes to expose latent bugs (arg-count mismatches,
  wrong return types, accidental non-variadic decls, orphan decls). Fix them as
  real bugs and note them in the change.

The golden output must stay byte-identical through all of this.

### Phase 5 — Lock down (later)

Enable `-Werror=missing-declarations` and wire the `asan-ubsan` preset into CI
so sanitizers run against the golden flow.
