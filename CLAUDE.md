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
- Build config lives in `CMakeLists.txt`. The shared warning set — including
  `-Wformat -Werror=format`, enforced project-wide — is applied to both targets
  via the `olympia_compile_flags()` helper (issue #14). Optimization is owned by
  the build type / preset (not pinned per target), and sanitizers come via
  `olympia_enable_sanitizers()`. The old `phase_N_build_flags()` /
  `legacy_build_flags()` scaffolding and the `LEGACY_C_FLAGS` / `…_STRICT`
  variables were retired in #14 (deleted outright — recover from git history if
  ever needed). Read the conventions block at the top of `CMakeLists.txt` and
  the [Warning policy](#warning-policy) before touching flags.
- **C11 standard** is set both project-wide (`CMAKE_C_STANDARD 11` /
  `…_REQUIRED ON` / `CMAKE_C_EXTENSIONS OFF`, lines 4–6) *and* declared
  explicitly per target via `target_compile_features(<tgt> PRIVATE c_std_11)`
  (issue #4, merged in #7). The per-target call is documentation/intent only —
  inert because the global standard already forces `-std=c11`; it guards against
  divergence if new targets are added. Mirrors the same change in sibling
  `../olympia-g1`. Not part of the 64-bit modernization effort.

## Modernization status

A modernization ladder (Phases 1–8) drove each 32→64-bit warning class to zero
and locked it as `-Werror`. The enforced set is now applied to both targets via
the single `olympia_compile_flags()` helper in `CMakeLists.txt` (consolidated in
issue #14; it was previously inlined per target — the per-phase sections below
still describe it as "inlined", which is the historical state). Current state:

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
| 8 | `return-type` + `return-mismatch` | ✅ enforced (`-Werror`); register-garbage class |
| 13 | `implicit-int-conversion` (Clang-guarded) — **separate code-quality track, NOT a 64-bit hazard** | ✅ enforced (`-Werror`) |
| 14 | Housekeeping: consolidate flags into `olympia_compile_flags()`, remove dead `SYSV` branch, set warning policy | ✅ done (not a warning class) |

Phases 1–8 are complete and locked in — the dangerous 32→64-bit pointer/int
hazards (bad casts, int/pointer conversions, and implicitly-declared functions
whose `int` return truncates a pointer) build clean as errors, the dead source
files are gone, every function now has a real prototype and declaration, the
LP64 width truncations (`long`/`size_t`/`ssize_t`-into-`int`) are explicit, the
signed/unsigned implicit conversions are explicit, every non-void function now
returns a value on all paths (or was retyped to `void`) so no caller reads a
garbage register, and the `asan-ubsan` preset builds + runs the golden flow
clean. Issue #13 (`implicit-int-conversion`) is also done, but as a **separate
code-quality track, not a 64-bit phase** — see below. The ladder is now **fully
complete**: issue #14 (the final item) consolidated the per-phase flags into one
`olympia_compile_flags()` helper, removed the dead `SYSV` branch, and set the
post-64-bit [warning policy](#warning-policy) for triaging new warnings.

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

### Phase 8 — `return-type` + `return-mismatch` (issue #12) ✅ done

`-Wreturn-type -Werror=return-type` and `-Wreturn-mismatch -Werror=return-mismatch`
are now inlined into both targets' `target_compile_options` blocks (**not**
Clang-only — no `if (CMAKE_C_COMPILER_ID …)` guard, unlike Phase 6's shorten
flag). The `-Wno-return-mismatch` / `-Wno-return-type` pair was dropped from
`LEGACY_C_FLAGS`. This is the **register-garbage** class: a non-void function
that falls off the end (or hits a bare `return;`) leaves the caller reading an
uninitialized register — 8 bytes on LP64 vs 4 on ILP32, so genuinely worse on
64-bit. It is the exact class that caused the `fact/100` `st -32` golden flicker
(issue #9): `i_use()` fell off the end and its garbage return register became
`command->status`. Mirrors `../olympia-g1#13`; G2's flagged set differs (own
mapgen, plus `setup_html_*`/`copy_public_turns`/`add_chamber`/`choose_quest_monster`
that G1 lacks) but the two fix shapes are identical.

> **Inventory trap (the G1 `-ferror-limit` gotcha, hit here too).** Clang's
> default `-ferror-limit=19` truncated the warn-only sweep: `-Wreturn-mismatch`
> is **error-by-default** in this clang (Apple clang 21), so once the early
> `return-mismatch` sites in `mapgen.c` accumulated the build cut off the rest
> of the file's diagnostics. The first sweep reported only **61** sites (27
> mapgen + 34 olympia); a re-sweep with `-ferror-limit=0` (and the `-Werror`
> lockdown build itself) revealed the true total of **91** sites in this phase:
> **51** in `mapgen.c`, **14** in `main.c`, and **26** olympia singles (plus the
> 3 fixed early, below). Reconcile against an unlimited error limit, **not** the
> truncated first pass — the issue body's "~22 mapgen + ~14 main" undercounted
> for this reason; the maintainer comment's "~56 mapgen + ~21 main" raw estimate
> was closer.

**Three sites were fixed early** (ahead of this phase, while chasing the flicker —
verified still in place): `i_use()` (`return TRUE` when a use-skill has no
interrupt handler — the flicker root cause, 211346b), `exp_s()` (`return ""`
after the switch default), and `learn_skill()` (retyped `void` + `use.h` proto)
— all in `olympia/use.c`, commits 211346b + f964959.

Two fix shapes, both representation-preserving (golden byte-identical):

- **Retype to `void`** the legacy default-`int` procedures whose callers all
  ignore the return (definition + every `proto.h`/`*.h`/`stack.h` declaration in
  lockstep, verified no caller consumes the value). **49 mapgen.c** pipeline
  helpers (`set_regions`, `random_province`, `make_*`, `gate_*`, `count_*`,
  `print_*`, `bridge_*`, `dump_*`, `clear_*`, `read_map`, `open_fps`, …) and the
  olympia void-semantic helpers (`check_db`, `init_spaces`, `move_prisoner`,
  `queue`, static `gm_count_stuff`/`add_chamber`, and the **14 `main.c`**
  report/init writers `call_init_routines`, `write_totimes`/`_email`/
  `_player_list`/`_forwards`/`_factions`/`_forward_sup`/`_faction_sup`,
  `mail_reports`, `setup_html_dir`/`setup_html_all`, `set_html_pass`,
  `output_html_rep`, `copy_public_turns`).
- **Add the missing return value** where the function genuinely returns one.
  Most fall-off paths sit after a **live** `assert(FALSE)` (asserts are on in the
  `-Og` debug and `asan-ubsan` builds, so the added return is unreachable in the
  golden run → golden-neutral), matching the value G1 chose: `return 0`
  (`hinder_med_chance`, `reduce_qty`, `hidden_count_to_index`, `loc_depth`,
  `fort_covers`, `lead_char_pos`, `choose_quest_monster`), `return ""`
  (`liner_desc`, `rank_s`, `mage_menial_how`, `fog_excuse`, mapgen `name_guild`),
  `return FALSE` (`v_decree`, `here_precedes`, `promote_after`), `return NULL`
  (`find_attacker`, `find_defender`); and a real value on the reachable fall-off
  path: `return TRUE` (`v_unseal_gate`, `d_rally`, `i_repair` — which **stays
  `int`**, it's the `repair` interrupt handler in `use_tbl`), `return new`
  (`new_storm`).

**`order.c queue()`** (the falls-off-end noted in the #6 aside) is now `void`:
all callers ignore it and it never produced a value. Phase 4 had already made it
variadic with a `proto.h` prototype, so no register-ABI hazard remained — this
just makes the type honest.

Probe (`-Wreturn-type -Wreturn-mismatch` at `-ferror-limit=0`) reports 0 on both
presets; both targets build clean with the new `-Werror` flags; debug and
asan-ubsan golden gates both `YES` (byte-identical) and asan/ubsan clean.

### Issue #13 — `implicit-int-conversion` (code-quality track) ✅ done

**This is a separate code-quality / tech-debt track, NOT a 64-bit phase.** The
hits are `int`→`short`/`schar`/`char`/`uchar` narrowing conversions into entity
struct fields (the fields are narrow types). That narrowing is
**architecture-independent** — it truncates identically on ILP32 and LP64 — so
it is **not** a 32→64-bit porting hazard and was deliberately kept off the
64-bit critical path. Mirrors `../olympia-g1#14` (which also chose option (b):
drive the class to zero, then lock). Done after Phases 6–8 landed.

`-Wimplicit-int-conversion -Werror=implicit-int-conversion` is now inlined into
both targets' **Clang-guarded** block (`if (CMAKE_C_COMPILER_ID MATCHES
"Clang")`), alongside Phase 6's `-Wshorten-64-to-32` — because the flag is
**Clang-only**: GCC folds this class into the broader `-Wconversion`. (This is
the same guard shape as Phase 6, and **unlike** Phases 7/8, which are not
Clang-gated.) Unlike Phases 6–8 there was **no** `-Wno-implicit-int-conversion`
suppression to remove — this phase only *adds* the flag.

**162 sites, all fixed representation-preservingly** (explicit
`(short)`/`(schar)`/`(char)`/`(uchar)` casts matching the destination field; the
implicit narrowing already truncated exactly this way → golden byte-identical):

- **Long tail — 69 sites across 23 olympia TUs + `mapgen/z.c`** (commit
  `73152e4`). `quest.c` 9, `code.c` 8, `art.c` 7, `cloud.c` 4, `necro.c`/
  `immed.c` 3 each, and 1–2 across `buy.c`, `c1.c`, `c2.c`, `combat.c`, `day.c`,
  `eat.c`, `faery.c`, `garr.c`, `gate.c`, `npc.c`, `seed.c`, `storm.c`,
  `swear.c`, `tunnel.c`, `u.c`, `use.c`, `olympia/z.c`, `mapgen/z.c`. The
  `code.c` `letter_val()` and `bx[n]->kind/skind` pairs were `replace_all`
  (identical text, all narrowing).
- **`io.c` — 92 sites, its own commit** (`8274ba4`), done last (mirrors G1,
  where io.c was likewise the bulk). It is the entity *deserialization reader*: a
  uniform `case 'xx': p->field = atoi(t); break;` per narrow field (66 `schar`,
  24 `short`, 1 `char`, 1 `uchar`), plus three direct `p->f = var;` sites
  (`know`/`experience`/`consume`). Applied by line number with a guarded script.
- **`scry.c:630` — the 162nd site** (folded into the lockdown commit `21adc6b`).
  `p->barrier = -(c->who)` (`short` field) is reported under the sub-category
  **`-Wimplicit-int-conversion-on-negation`**, whose tag (`…-on-negation]`)
  slips past a `\[-Wimplicit-int-conversion\]` bracket-grep — so the warn-only
  probe's filtered count read **161** while the raw count read **162**. Enabling
  the `-Werror` flag surfaced it. (Lesson: count the class with a *prefix* match,
  not an exact-bracket match — the on-negation/on-overflow sub-categories share
  the `-Wimplicit-int-conversion` root but have distinct closing tags.)

Probe (`-Wimplicit-int-conversion` at `-ferror-limit=0`, prefix-matched) reports
0 on both presets; both targets build clean with the new `-Werror` flag; debug
and asan-ubsan golden gates both `YES` (byte-identical) and asan/ubsan clean.

### Issue #14 — flag consolidation + dead SYSV removal + warning policy ✅ done

The **final item on the modernization ladder** — pure end-of-ladder housekeeping,
no warning class. Mirrors `../olympia-g1#18` (+ `#17` + `#15`). Three sub-tasks,
each landed as its own golden-verified commit (structural CMake change kept
strictly separate from the source change):

- **Consolidated the flags.** The Phase 1–8 + Clang-guarded `-W.../-Werror=...`
  pairs were previously **inlined** into both `target_compile_options` blocks.
  They now live in a single `olympia_compile_flags(tgt)` helper applied
  identically to both targets (mirrors G1's `olympia_compile_flags`).
  Behavior-neutral: the only effective flag-stream delta (verified against
  `compile_commands.json` per target) was dropping four **dead** `-Wno-` entries
  — `incompatible-pointer-types`, `int-conversion`, `int-to-pointer-cast`,
  `pointer-to-int-cast` — each already overridden by the later `-Werror=` for the
  same class (later-flag-wins), so those classes stay enforced.
- **Retired the dead scaffolding (deleted outright).** `legacy_build_flags()`,
  `phase_1..5_build_flags()`, and the `LEGACY_C_FLAGS` / `LEGACY_C_FLAGS_STRICT`
  variables were all unused once the flags were consolidated. Deleted (not kept
  as commented history) — the per-phase story lives here in CLAUDE.md and in git
  history. Maintainer decision (2026-06-13).
- **Removed the dead `#ifdef SYSV` branch.** `olympia/z.h` carried a live-but-dead
  `#ifdef SYSV` block shadowing `bzero`/`bcopy` with `memset`/`memcpy`; `SYSV` is
  never defined anywhere in the tree, so the macros never expanded. `mapgen/z.h`
  already had its copy commented out. Both replaced with a note. Output-neutral
  by construction, and it makes the Phase-4 invariant explicit: `rnd.c` includes
  `z.h` precisely *because* those macros were `#ifdef SYSV` (off on macOS) so the
  golden-critical MD5 stayed on real libc `bzero`/`bcopy` (from `<strings.h>`).
  With the dead branch gone, the macros provably never apply anywhere. Mirrors
  G1 `#15` (G2 has **no `USE_OUR_RND`**, so only the `SYSV` half applied).
- **Optimization owned by the build type (not pinned per target).** The targets
  pinned `-O1 -g` (mapgen) / `-Og -g` (olympia) in `target_compile_options`.
  Because target options are appended *after* the build-type flags
  (`CMAKE_C_FLAGS_<CONFIG>`), the pinned `-O` clobbered the preset's choice —
  `release` emitted `-O3 -DNDEBUG` and was then silently overridden back to
  `-Og`/`-O1`, so it never actually built optimized (verified via
  `compile_commands.json`). Removed the pinned `-O`/`-g`; the build type now owns
  it: debug → `-O0 -g`, release → `-O3 -DNDEBUG`, asan-ubsan → `-O1 -g …` (its
  preset override). Mirrors `../olympia-g1@b149508`. Golden-neutral — the engine
  output is deterministic across `-O` levels.
- **Format checking unified project-wide.** `-Wformat -Werror=format` was
  olympia-only; mapgen builds clean under it, so it moved into the shared
  `olympia_compile_flags()` helper and now applies to **both** targets. The full
  class (incl. format-security / format-nonliteral) is an error with no
  sub-suppression (the non-literal sites were fixed in #6). No per-target warning
  difference remains.

Every #14 commit: golden gate `YES` on debug and asan-ubsan, byte-identical,
asan/ubsan clean.

## Warning policy

The post-64-bit policy for what is an error, what is suppressed, and how a new
warning gets triaged. (This is the still-open G1 `#17` question, answered here
for G2.) The authoritative list is the conventions block + `olympia_compile_flags()`
in `CMakeLists.txt`; this section is the rationale.

**Three tiers:**

1. **Enforced (`-Werror`).** Every class on the Phase 1–8 ladder, plus the
   project-wide format class (#5/#6) and the `implicit-int-conversion`
   code-quality class (#13). Each was driven to **zero** across the tree and
   locked. These are written as an explicit `-Wfoo -Werror=foo` pair, one class
   per line — the bare `-Wfoo` is redundant (the `-Werror=` already enables it)
   but is kept deliberately as a record that the class is at zero and locked.
   **Do not** collapse the pairs or relax any to a non-error.

2. **Suppressed (`-Wno-foo`).** Legacy idioms this ~60K-line codebase still leans
   on and that are **not** being chased right now — `-Wno-parentheses`,
   `-Wno-comment`, `-Wno-dangling-else`, `-Wno-macro-redefined`, `-Wno-multichar`,
   `-Wno-logical-not-parentheses`, `-Wno-compare-distinct-pointer-types`,
   `-Wno-non-literal-null-conversion`, `-Wno-deprecated-declarations`,
   `-Wno-extra-tokens`, `-Wno-incompatible-library-redeclaration`,
   `-Wno-tautological-constant-out-of-range-compare`. These are **stylistic /
   low-risk**, not 64-bit hazards. They stay suppressed until someone opens a
   dedicated cleanup track (the same option-(b) path #13 took for
   `implicit-int-conversion`: drive the whole class to zero in one focused pass,
   then promote to `-Werror` — never partially).

3. **Clang-only spellings.** `-Wshorten-64-to-32` (#10) and
   `-Wimplicit-int-conversion` (#13) are Clang-only diagnostics (GCC folds them
   into the broader `-Wconversion`); they sit behind
   `if (CMAKE_C_COMPILER_ID MATCHES "Clang")`. `-Wreturn-mismatch` is **not**
   Clang-gated in G2 (it sits in the common set) — keep it there.

**Triaging a new warning** (e.g. a compiler upgrade surfaces a new class, or a
new file trips something):

- If it is a **64-bit correctness hazard** (truncation, pointer/int confusion,
  width, sign, missing return → garbage register), treat it like a ladder phase:
  drive the class to **zero**, then add the `-Wfoo -Werror=foo` pair to
  `olympia_compile_flags()`. Never lock a class with live hits remaining.
- If it is **stylistic / low-risk**, either fix the few sites or add a
  documented `-Wno-foo` to the suppression list with a one-line reason. Prefer
  fixing if the count is small.
- **Every** flag change must keep the golden gate `YES` on **both** presets
  (debug and asan-ubsan), byte-identical, asan/ubsan clean — the same invariant
  every ladder phase held. A pure flag flip (enabling/relaxing enforcement)
  must be its **own commit**, separate from any structural CMake refactor and
  from any source change.
- **No CI.** Gates are run locally (maintainer decision, see *Conventions*);
  do not wire warning enforcement into CI.
