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
> prints the real date. Ported from `../olympia-g3`. (The separate `fact/100`
> `st -32` flicker — once an open Phase 5 item — was root-caused and fixed in
> issue #9; see [Phase 5](#phase-5--lock-down--done).)

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
- **No CI workflows.** This repo does not use CI (no `.github/workflows`, no
  GitHub Actions). Maintainer decision (2026-06-13, issue #9): do **not** add CI
  pipelines. Gates are run locally — the golden gate via `./tests/olympia/golden_check.sh`,
  and the sanitizer gate via `OLYMPIA_PRESET=asan-ubsan` against the same golden
  flow. Don't propose or wire CI even when an issue's text asks for it.
- **Golden output is the contract.** Behavior changes that alter engine output
  must be deliberate and the snapshot updated in the same change with a note on
  why. Modernization changes (prototypes, casts, dead-code removal) must produce
  byte-identical golden output.
- Build config lives in `CMakeLists.txt`. The per-target compile flags are
  inlined into each `add_executable` block (see lines ~221 and ~266), *not*
  applied via the `phase_N_build_flags()` functions — those are roadmap
  scaffolding (defined, not yet called). `LEGACY_C_FLAGS_STRICT` is likewise
  staged but unused.
- **C11 standard** is set both project-wide (`CMAKE_C_STANDARD 11` /
  `…_REQUIRED ON` / `CMAKE_C_EXTENSIONS OFF`, lines 4–6) *and* declared
  explicitly per target via `target_compile_features(<tgt> PRIVATE c_std_11)`
  (issue #4, merged in #7). The per-target call is documentation/intent only —
  inert because the global standard already forces `-std=c11`; it guards against
  divergence if new targets are added. Mirrors the same change in sibling
  `../olympia-g1`. Not part of the 64-bit modernization effort.

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
| 5 | `missing-declarations` + asan/ubsan verified against the golden flow | ✅ enforced (`-Werror`); flicker fixed |
| 6 | `shorten-64-to-32` (Clang-guarded) + `sizeof-pointer-memaccess` | ✅ enforced (`-Werror`); MD5 `bzero` bug fixed |
| 7 | `sign-conversion` | ✅ enforced (`-Werror`) |

Phases 1–7 are complete and locked in — the dangerous 32→64-bit pointer/int
hazards (bad casts, int/pointer conversions, and implicitly-declared functions
whose `int` return truncates a pointer) build clean as errors, the dead source
files are gone, every function now has a real prototype and declaration, the
LP64 width truncations (`long`/`size_t`/`ssize_t`-into-`int`) are explicit, the
signed/unsigned implicit conversions are explicit, and the `asan-ubsan` preset
builds + runs the golden flow clean. The remaining 64-bit work is the later
ladder (issues #12–#14: `return-type`, `implicit-int-conversion`, flag
consolidation).

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

Verified clean rebuild and `./tests/olympia/golden_check.sh` → `YES`. **Historical
caveat (now resolved):** G2 had a pre-existing build-to-build non-determinism — a
single `st -32` line in `fact/100` flickered across clean rebuilds of
*byte-identical* source. Phase 4 ruled out missing prototypes; **Phase 5
(issue #9) root-caused and fixed it** — an uninitialized-value read, not a
prototype bug. `fact/100` is now a normal manifest entry (`st 1`, deterministic);
the `golden_check.sh` special case is gone. See
[Phase 5](#phase-5--lock-down--done).

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

### Phase 5 — Lock down ✅ done

`-Wmissing-declarations -Werror=missing-declarations` is now inlined into both
targets' `target_compile_options` blocks. It measures **0** hits (Phase 4's
prototype work already covered the function-declaration class on clang; the flag
is a cross-compiler guard — on GCC it's a distinct check). Debug and asan-ubsan
build clean; golden gate `YES` on both.

**The `asan-ubsan` preset actually works now.** Its `OLYMPIA_SANITIZERS` cache
variable was declared with a malformed `set(... CACHE STRING "<doc>"
address,undefined)` — the stray trailing token broke CMake's `CACHE` keyword
recognition, so the whole thing parsed as a *normal* (directory-scope) `set` with
a list value that shadowed the preset's cache value and leaked `CACHE STRING …`
literals onto the compiler command line (every TU failed `no such file or
directory: 'CACHE'`). Fixed to the proper one-line form
`set(OLYMPIA_SANITIZERS "address,undefined" CACHE STRING "…")`. The preset now
builds and runs the full golden flow with **no** ASan/UBSan diagnostics.

**The `st -32` flicker is fixed (root cause found).** Running the golden flow
under the sanitizers surfaced a *different* garbage value (`st 80`) in the same
`fact/100` slot — the tell of an uninitialized read (ASan can't flag uninit
reads directly, but the build-to-build value variation is conclusive). Traced to
`olympia/use.c:i_use()`: when an interruptible use-skill command whose `use_tbl`
entry has **no** interrupt handler (e.g. `sk_make_catapult`, the "Scrying One"
NPC's `use 613`) is interrupted, `i_use` fell off the end **without a return**,
so `interrupt_order()`'s `c->status = (*interrupt)(c)` assigned an uninitialized
register value. That value (varying per build: `-32`, `80`, or absent because
`print_command` only emits `st` when `status != 0`) then persisted into the
requeued command (`oly_parse` doesn't reset `status`) and got saved. Fix: `i_use`
returns `TRUE` when there's no inner interrupt handler — matching G1's `i_use`
(which already had the `return TRUE`). `status` is now a deterministic `st 1`;
**5 clean rebuilds confirm stability.** `fact/100` is folded back into the hash
manifest and the `golden_check.sh` flicker special case (plus the
`fact-100.reference`) is deleted.

> **CI:** issue #9 also called for wiring the `asan-ubsan` flow into CI, but the
> maintainer decided against adding CI workflows to this repo (2026-06-13). The
> sanitizer gate is run locally via `OLYMPIA_PRESET=asan-ubsan` + the golden
> scripts (see [Test](#test--golden-snapshots-must-stay-green)).

### Phase 6 — `shorten-64-to-32` + `sizeof-pointer-memaccess` (issue #10) ✅ done

The **first real 64-bit phase** (the earlier phases guarded *pointer/int*
hazards; this one surfaces *width* truncation). `-Wshorten-64-to-32` isolates
exactly the `long`/`size_t`/`ssize_t`-into-`int`/`short` truncations that diverge
between ILP32 and LP64. Both flags are now `-Werror` on both targets — the
shorten flag Clang-guarded (`if (CMAKE_C_COMPILER_ID MATCHES "Clang")`, it's a
Clang-only diagnostic), `sizeof-pointer-memaccess` portable. They're inlined in
both `target_compile_options` blocks alongside the Phase 1–5 flags; the
`-Wno-sizeof-pointer-memaccess` suppression was removed from `LEGACY_C_FLAGS`.

**The `sizeof-pointer-memaccess` bug (the concrete defect, 2 sites).** In
`MD5Final` the defensive post-digest wipe was `bzero(ctx, sizeof(ctx))` where
`ctx` is `struct xMD5Context *` — `sizeof(ctx)` is the *pointer* size (8 on LP64,
was 4 on ILP32), so it zeroed only 8 bytes instead of `sizeof(*ctx)`. Fixed to
`sizeof(*ctx)` in **both** `olympia/rnd.c` and `mapgen/rnd.c` (G2's MD5 RNG, no
G1 counterpart). Golden-safe: the digest is `bcopy`'d out before the wipe, so the
produced MD5 — and the RNG built on it — is unchanged.

**15 `-Wshorten-64-to-32` sites** (10 olympia in 6 files + 5 mapgen), all fixed
representation-preservingly (the implicit conversion already truncated exactly
this way, so golden stays byte-identical):

- `z.c` + `mapgen/z.c` `readlin` path: `nread` retyped `int`→`ssize_t` (its
  source is `read()`), clearing both `nread = read(...)` sites in each;
  downstream indexing/compares are unaffected.
- `mapgen/z.c` `str_save`: `(unsigned)` cast on `strlen(s) + 1` feeding mapgen's
  `my_malloc(unsigned size)`. (**Differs from G1:** olympia's `my_malloc` takes
  `size_t` — checked_alloc-based — so its `str_save` was never flagged.)
- `olympia/rnd.c` `md5_int`: `return (int) buf[0]` — low 32 bits of the MD5 word
  (a 32-bit `unsigned long` on ILP32).
- `olympia/code.c` `letter_val`: `return (int)(p-let)` — index into a fixed short
  string.
- `strlen()`→`int` name/line/word lengths, provably `<2^31`: documented `(int)`
  casts in `z.c`/`mapgen/z.c` `fuzzy_strcmp`, `c2.c` `line_length_check`,
  `check.c` `check_loc_name_lengths`, `eat.c` `do_eat_command`, `report.c`
  `strip_leading_stupid_word`.

Probe (`-Wshorten-64-to-32`) now reports 0; both targets build clean with the new
`-Werror` flags; debug and asan-ubsan golden gates both `YES` (byte-identical)
and asan/ubsan clean.

### Phase 7 — `sign-conversion` (issue #11) ✅ done

`-Wsign-conversion -Werror=sign-conversion` is now inlined into both targets'
`target_compile_options` blocks (it's **not** Clang-only — no
`if (CMAKE_C_COMPILER_ID …)` guard, unlike Phase 6's shorten flag). This is the
signed/unsigned implicit-conversion class: arch-independent (it bites the same on
ILP32 and LP64), but a large population. There was no `-Wno-sign-conversion`
suppression in `LEGACY_C_FLAGS` to drop. **Mirrors `../olympia-g1#11` minus the
seed fix** — G1's Phase 7 canonicalised a `seed[3]` signed/unsigned `extern`
mismatch in its `drand48`/`erand48` RNG; G2 replaced that RNG with an MD5 RNG, so
there is **no `seed[3]`** and nothing to canonicalise. G2's distribution is
instead rnd.c-heavy (the MD5 RNG mixes `unsigned`/`int`), unlike G1's io.c-heavy
spread.

**43 sites** matched the warn-only debug sweep exactly (rnd.c 12, gm.c 7, perm.c
6, report.c 5, mapgen z.c 2, input.c 2, use.c 2, and singles in buy.c, check.c,
io.c, seed.c, sout.c, summary.c, swear.c). All fixed representation-preservingly
(the implicit conversion already did exactly this, so golden stays
byte-identical):

- **MD5 RNG (`olympia/rnd.c` + `mapgen/rnd.c`, 6 each).** `rnd()`:
  `range = (unsigned)(high-low)`, `r = (int)range`, `mask |= (unsigned)r`,
  `return (int)(num + (unsigned)low)` — all modulo-2³² identities.
  `xMD5Update()`: `t + (word32)len` (`len >= 0`). The digest is `bcopy`'d out
  before any wipe, so the produced MD5 — and the RNG on it — is unchanged.
- **qsort `nmemb` (bulk).** The `*_len()` count (`int`) feeding qsort's `size_t`
  `nmemb`, cast `(size_t)`: use.c, buy.c, seed.c, input.c, gm.c (×5), perm.c
  (×4), report.c (×3), swear.c, check.c.
- **The shared `loop_known` macro** (`olympia/loop.h`). One `(size_t)` on its
  embedded `qsort(ilist_len(kn))` clears **all 6** expansion sites at once
  (summary.c, io.c, gm.c ×2, report.c ×2) — those 6 are *not* separate edits.
- **`mapgen/z.c`** `*((int *)p) = (int)size` — the `my_malloc` size-header slot
  (mapgen's `my_malloc` takes `unsigned`). **The fixed `spaces` buffer**:
  `(size_t)spaces_len` in the `perm.c` subscript and
  `my_malloc((size_t)spaces_len+1)` in `sout.c`.

**asan-ubsan surfaces 4 more (sanitizer-only).** The `asan-ubsan` preset sanitizes
**only** `olympia-g2` (`olympia_enable_sanitizers()` isn't called for mapgen-g2);
under that instrumentation, four `bcopy`/`bzero` int-length args in
`olympia/rnd.c`'s MD5 core (lines ~98/118/138/144 — `bcopy(…, (size_t)len)`,
`bzero(p, (size_t)(count+8))`) emit sign-conversion that the `-Og` debug build
doesn't. They were fixed too (all provably non-negative) so the lockdown holds
under **both** presets, and mirrored into `mapgen/rnd.c`'s twin MD5 to keep the
two copies in sync. So the actual edit count is 43 (debug probe) + 4 olympia + 4
mirrored mapgen.

Probe (`-Wsign-conversion`) reports 0 on both presets; both targets build clean
with the new `-Werror` flags; debug and asan-ubsan golden gates both `YES`
(byte-identical) and asan/ubsan clean.
