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

> **`test-use-const-report-date`** (issue #2) makes the golden gate
> deterministic. The Olympia Times masthead (`times_masthead()` in
> `olympia/c2.c`) otherwise embeds the wall-clock date into
> `run/olympia/lib/times_0`, so its sha256 diverged from the manifest on any
> day but the capture day. Passing `test-use-const-report-date` on the engine
> command line sets the `test_use_const_report_date` global (`olympia/main.c`,
> declared `extern` in `oly.h`); `times_masthead()` then `strcpy`s the fixed
> date `"January 1, 2000"` instead of calling `time()/localtime()`. The flag is
> a long-form token pulled out of `argv` (and `argv` compacted) before
> `getopt()` runs, since getopt only handles single-char options. `run/olympia-g2.sh`
> passes it on the turn run. It affects **only** the newsletter date — all other
> output is byte-identical with or without it — and normal play (no flag) still
> prints the real date. Ported from `../olympia-g3`. The separate `fact/100`
> `st -32` flicker is unrelated and remains an open Phase 5 item.

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
| 4 | `strict-prototypes`, `missing-prototypes`, `implicit-function-declaration` | ✅ enforced (`-Werror`) |
| 5 | `missing-declarations` + sanitizers in CI | ⬜ wired (asan preset), not enforced (next) |

Phases 1–4 are complete and locked in — the dangerous 32→64-bit pointer/int
hazards (bad casts, int/pointer conversions, and implicitly-declared functions
whose `int` return truncates a pointer) build clean as errors, the dead source
files are gone, and every function now has a real prototype. The remaining work
is Phase 5 (`missing-declarations` + sanitizers in CI).

> **Note:** the sister **G1** repo (`../olympia-g1`) also completed Phase 3.5
> and Phase 4. Its `CLAUDE.md` records those changes, and the shared method is
> in `doc/modernization-prototypes-playbook.md` (with G2-specific notes) plus
> the helper scripts in `doc/phase4-tools/`. G3/TAG can use the same template.

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

**Retired: `lib/plist.c`** (issue #1, done 2026-06-12). The generic untyped
`plist` (`typedef void **plist;`) had **zero** call sites — g2's pointer
collections are all element-typed (`item_ents_list`, `trades_list`, etc. in
`lib/lists.h`) and `plist.c` was never in any `target_sources`. Deleted the file
and its typedef + 15 externs from `lib/lists.h`; nothing to change in
`CMakeLists.txt`. Removing an uncompiled file + an unused typedef is
output-neutral — binaries byte-identical, golden gate still `YES`. Identical to
the retirements in sibling `../olympia-g1` (commit dee82d8) and `../olympia-g3`
(issue 2). Recover from git history if a future caller ever wants it. (`ilist`,
which stores `int`, correctly stays.)

Verified clean rebuild and `./tests/olympia/golden_check.sh` → `YES`. **Caveat:**
G2 has a pre-existing build-to-build non-determinism — a single `st -32` line in
`fact/100` flickers across clean rebuilds of *byte-identical* source (proven by
rebuilding the unmodified tree twice and observing the same flicker), so it is
not caused by this change. **Update (Phase 4 done): the flicker is NOT a
missing-prototype bug.** Re-checked after Phase 4 with all three warning classes
at 0 and `-Werror` enforced — 6 clean rebuilds, 5 with no `st -32` line and 1
with it — so prototypes were not the cause. `golden_check.sh` still holds
`fact/100` out of its hash manifest and tolerates only the lone `st -32` flicker
(reporting it), failing on any other diff. Next probe: the `asan-ubsan` preset
(uninitialized read / UB is the new leading suspect) — see Phase 5.

### Phase 4 — Prototypes & declarations ✅ done

`strict-prototypes`, `missing-prototypes`, and `implicit-function-declaration`
are now `-Werror` on both targets (inlined into each `target_compile_options`
block) and the dead `-Wno-implicit-function-declaration` /
`-Wno-deprecated-non-prototype` suppressions are removed from `LEGACY_C_FLAGS`
(and the mirrored `legacy_build_flags()` scaffolding). All three classes measure
**0** across the tree; debug and release build clean; golden output unchanged.

What it took (full method + traps in `doc/modernization-prototypes-playbook.md`,
helper scripts in `doc/phase4-tools/`):

- Converted **65** K&R definitions to ANSI (38 in mapgen, 27 in olympia) with
  `kr2ansi.py`, and the ~270 empty-paren `name()` definitions to `name(void)`
  with `fix_void_defs.py` (probe with `-Wdeprecated-non-prototype`, *not*
  `-Wold-style-definition`).
- Canonicalised the **15** `qsort` comparators to
  `(const void *, const void *)` with local casts (G1's 14 + `rank_comp`).
- Generated a prototype header per target — **`olympia/proto.h`** (included at
  the end of `oly.h`, after the type defs and a `#include <stdio.h>` for `FILE`;
  forward-declares the private structs `build_ent`/`fight`/`harvest`/`make`/
  `wield`) and **`mapgen/proto.h`** (included in `mapgen.c` after the private
  `struct tile`). This cleared all `missing-prototypes`.
- Deleted the `use.c`/`glob.c` command-handler decl blocks and scattered local
  `extern T foo();` decls; gave the surviving header empty-paren decls real
  prototypes; fixed the `loop.h` `loop_known` macro's embedded `int_comp` decl.
- Added the real libc headers (`string.h`, `strings.h`, `stdlib.h`, `unistd.h`,
  `time.h`, `fcntl.h`, `sys/stat.h`) at the **top of `z.h` / `mapgen/z.h`**,
  before the engine's `bzero`/`bcopy`/`abs` shadow macros (and ahead of
  `oly.h`'s `wait` macro, which collides with `sys/wait.h` via `stdlib.h`).
  `z.h` is the chokepoint every engine TU includes first.
- Latent bugs the prototypes exposed, fixed as real bugs:
  `make_appropriate_subloc(row, col, 0)` called with a dead 3rd arg (×3 in
  `mapgen.c`); `queue()` poor-man's-varargs made genuinely variadic
  (`vsprintf`); a wrong-return-type `extern char *clear_wait_parse()` decl in
  `eat.c` (the function returns `void`); orphan decls `fetch_inside_name`
  (mapgen) and `wrap_done`/olympia `dir_assert` deleted.

**G2-specific notes (differs from G1):**

- **`queue()` + arm64 ABI.** The poor-man's-varargs `queue()` must become
  variadic *and* gain its `proto.h` prototype in the **same step**. On Apple
  arm64 a variadic call without a visible prototype uses the fixed-arity
  register convention while the callee reads the stack → **segfault** (it crashed
  `queue_npc_orders`). Converting `queue` to variadic before `proto.h` was wired
  in broke the run; doing both together fixed it.
- **MD5 RNG in `rnd.c`** (G2 replaced G1's `drand48`/z.c RNG). `olympia/rnd.c`
  and `mapgen/rnd.c` include neither `z.h` nor `oly.h`; made them `#include
  "z.h"` (safe — the `bzero`/`bcopy` macros are `#ifdef SYSV`, off on macOS, so
  the golden-critical MD5 is untouched) and declared their cross-file funcs
  (`MD5`, `save_seed`, `md5_int`) in `z.h` alongside `rnd`. Made the purely
  internal helpers (`byteSwap`, and mapgen's `MD5`) `static`.
- **`tunnel.c`** (G2-only). `create_tunnels` is cross-file (→ `proto.h`); the
  other four funcs are file-local — made `random_subworld_loc`/`fill_dir_exits`/
  `create_tunnel_set` `static`, and gave the dead `print_map` (whose signature
  uses file-private `SZ`/`MAX_LEVELS` macros, so it can't go in `proto.h`) a
  local prototype, kept non-static to avoid an unused-function warning.

### Phase 5 — Lock down (next)

Enable `-Werror=missing-declarations` and wire the `asan-ubsan` preset into CI
so sanitizers run against the golden flow. **Also use the sanitizer run to chase
the `st -32` flicker** — Phase 4 ruled out missing prototypes, and an
uninitialized read / UB is the new leading suspect (see the Phase 3.5 caveat
above and the `st-32` memory note).
