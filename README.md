# Olympia G2

**G2** is the second-generation Olympia play-by-mail (PBM) strategy game engine
(~53K lines of C). It builds on G1, adding HTML reports and tunnels.

This repository is a standalone extraction of the G2 engine from the original
multi-engine Olympia monorepo. It builds on its own with CMake.

The code is legacy K&R-style C originally targeting 32-bit systems. A modernization
effort is underway to make it compile cleanly on 64-bit systems.

## Targets

- `olympia-g2` — the main game engine
- `mapgen-g2` — the map generator (inputs `Map`/`Land`/`Cities`/`Regions`,
  outputs `gate`/`loc`/`road`)

## Building

Requires CMake (>= 4.1), Ninja, and a Clang or GCC toolchain.

```bash
cmake --preset debug
cmake --build --preset debug
# Binaries: build/debug/olympia-g2, build/debug/mapgen-g2
```

Presets (see `CMakePresets.json`): `debug` (default), `release`, `asan-ubsan`
(AddressSanitizer + UndefinedBehaviorSanitizer for `olympia-g2`).

Without presets:

```bash
mkdir build && cd build && cmake .. && cmake --build .
```

## Running / golden tests

Build first (default `debug` preset), then:

```bash
# mapgen: generates gate/loc/road (inputs to the olympia run below)
./run/mapgen/mapgen.sh

# olympia: extracts fixtures, runs a turn, saves the database
./run/olympia-g2.sh

# compare the olympia run output against the golden snapshot
./tests/olympia/golden_check.sh           # YES = match
./tests/olympia/golden_check.sh --update   # refresh the snapshot
```

The scripts auto-detect the repo root and look for binaries at
`build/<preset>/<target>` (override the preset with `OLYMPIA_PRESET=release ...`).

## Layout

- `olympia/` — the G2 engine sources and headers
- `mapgen/` — the map generator
- `lib/` — shared support code (entity lists, tiles, roads, allocation, …)
- `tests/` — golden-test fixtures and golden files
- `run/` — run/test driver scripts and scratch run directories
- `doc/` — assorted G2 design/reference notes

## License

GNU Affero General Public License v3 — see [LICENSE](LICENSE). The original
Olympia sources are public domain.
