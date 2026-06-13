# tests/mapgen/golden — historical 32-bit baseline (UNUSED)

These files are a **stale 32-bit (ILP32) mapgen snapshot** captured from the
original legacy code before the 64-bit modernization. They are kept only as a
historical artifact.

- **Nothing consumes them.** Neither `run/mapgen/mapgen.sh` nor the olympia
  golden gate (`tests/olympia/golden_check.sh`) reads this directory.
- They **diverge from current 64-bit (LP64) output even on a clean tree**, so
  comparing live mapgen output against them is meaningless.
- The 32-bit build support (`BUILD_32BIT`) that produced them was removed in
  issue #8; the LP64 golden under `tests/olympia/` is the sole contract.

Do not treat any diff against these files as a regression.
