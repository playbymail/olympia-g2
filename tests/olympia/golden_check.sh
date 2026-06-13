#!/usr/bin/env bash
set -euo pipefail
############################################################################
#
# golden_check.sh — byte-for-byte gate on the olympia-g2 engine output.
#
# The engine writes its post-turn database to run/olympia/lib (~3500 files,
# ~140 MB) — far too large to commit as a literal golden tree the way G1 does.
# Instead the golden is a **hash manifest**: one `<sha256>  <relpath>` line per
# run file, sorted. That captures every byte of every file in a small, diffable
# artifact. Any content change anywhere shows up as a manifest line that no
# longer matches.
#
# History: fact/100 used to be special-cased for a known build-to-build
# non-determinism — a lone `` st -32 `` line that flickered in and out across
# clean rebuilds of byte-identical source. Phase 5 (issue #9) found and fixed
# the root cause: `i_use()` (olympia/use.c) fell off the end without a return
# when a use-skill command with no interrupt handler (e.g. sk_make_catapult)
# was interrupted, so `command->status` was assigned an uninitialized register
# value (the saved `` st `` line, garbage that varied per build). With `i_use`
# returning TRUE the status is a deterministic `` st 1 ``, the flicker is gone,
# and fact/100 is now a normal manifest entry — no special case.
#
############################################################################
#
# Resolve the repo root from this script's location so the repo is relocatable.
OLYMPIA_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
OLYMPIA_ENGINE=g2
OLYMPIA_COMMAND=olympia
############################################################################
#
ROOT="${OLYMPIA_ROOT}"
RUN_LIB="${ROOT}/run/olympia/lib"
GOLDEN="${ROOT}/tests/olympia/golden"
MANIFEST="${GOLDEN}/manifest.sha256"

usage() {
  echo "usage: $0 [--update]"
  echo "  --update   replace the golden manifest from current run output"
  exit 2
}

UPDATE=0
if [[ $# -gt 1 ]]; then usage; fi
if [[ $# -eq 1 ]]; then
  [[ "$1" == "--update" ]] || usage
  UPDATE=1
fi

[[ -d "${RUN_LIB}" ]] || { echo "NO: missing run lib dir: ${RUN_LIB} (run ./run/olympia-${OLYMPIA_ENGINE}.sh first)"; exit 1; }

# Pick a sha256 tool (shasum on macOS, sha256sum on Linux; either works on both).
if command -v sha256sum >/dev/null 2>&1; then
  SHA_CMD="sha256sum"
elif command -v shasum >/dev/null 2>&1; then
  SHA_CMD="shasum -a 256"
else
  echo "NO: no sha256 tool (need sha256sum or shasum)"; exit 1
fi

# Files we never golden:
#   .DS_Store     — macOS noise
# Add more -o clauses here if a file proves nondeterministic on g3/tag.
# (Tree has only simple relative paths — no spaces/newlines — so newline-safe.)
gen_manifest() {
  ( cd "${RUN_LIB}" \
      && find . -type f -name .DS_Store -prune \
              -o -type f -print \
      | LC_ALL=C sort \
      | xargs ${SHA_CMD} \
      | LC_ALL=C sort )
}

############################################################################
# Update mode: (re)write the golden manifest.
if [[ "${UPDATE}" -eq 1 ]]; then
  mkdir -p "${GOLDEN}"
  # Clear any prior golden artifacts (including the legacy single 'master' file
  # and the retired fact/100 flicker reference).
  rm -f "${GOLDEN}/master" "${MANIFEST}" "${GOLDEN}/fact-100.reference"
  gen_manifest > "${MANIFEST}"
  echo "OK: updated golden manifest ($(wc -l < "${MANIFEST}" | tr -d ' ') files)"
  exit 0
fi

############################################################################
# Test mode: compare current run output to golden.
[[ -f "${MANIFEST}" ]] || { echo "NO: missing golden manifest: ${MANIFEST}. Run: $0 --update"; exit 1; }

# The manifest (every run file) must match byte-for-byte.
DIFF_OUT="$(diff "${MANIFEST}" <(gen_manifest) || true)"
if [[ -n "${DIFF_OUT}" ]]; then
  echo "NO: run output diverges from golden manifest:"
  echo "${DIFF_OUT}" | head -50
  exit 1
fi

echo "YES"
exit 0
