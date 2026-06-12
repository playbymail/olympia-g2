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
# One file is special-cased: fact/100. G2 has a known, pre-existing
# non-determinism — a single `` st -32 `` line in fact/100 flickers in and out
# across *clean rebuilds of byte-identical source* (a given binary is
# deterministic across reruns; two clean builds of the same source can disagree
# on that one line). We believe this is a bug from compiling with missing
# prototypes in 64-bit mode and expect Phase 4 (prototypes & declarations) to
# resolve it. Until then fact/100 is held out of the manifest and compared
# line-by-line against a committed reference: identical -> pass; differs by only
# the lone `` st -32 `` flicker -> pass, but loudly reported so the bug stays
# tracked; any other difference -> fail. When Phase 4 lands and the flicker is
# gone, fold fact/100 back into the manifest and delete the special case.
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
# The one known-flaky file, held out of the manifest and checked by hand.
FLAKY_REL="fact/100"
FLAKY_REF="${GOLDEN}/fact-100.reference"

usage() {
  echo "usage: $0 [--update]"
  echo "  --update   replace the golden manifest + reference from current run output"
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

# Files we never golden (volatile, or handled specially below):
#   .DS_Store     — macOS noise
#   ${FLAKY_REL}  — known st -32 flicker, checked line-by-line instead
# Add more -o clauses here if a file proves nondeterministic on g3/tag.
# (Tree has only simple relative paths — no spaces/newlines — so newline-safe.)
gen_manifest() {
  ( cd "${RUN_LIB}" \
      && find . -type f \( -name .DS_Store -o -path "./${FLAKY_REL}" \) -prune \
              -o -type f -print \
      | LC_ALL=C sort \
      | xargs ${SHA_CMD} \
      | LC_ALL=C sort )
}

############################################################################
# Update mode: (re)write the golden manifest + flaky-file reference.
if [[ "${UPDATE}" -eq 1 ]]; then
  mkdir -p "${GOLDEN}"
  # Clear any prior golden artifacts (including the legacy single 'master' file).
  rm -f "${GOLDEN}/master" "${MANIFEST}" "${FLAKY_REF}"
  gen_manifest > "${MANIFEST}"
  [[ -f "${RUN_LIB}/${FLAKY_REL}" ]] && cp "${RUN_LIB}/${FLAKY_REL}" "${FLAKY_REF}"
  echo "OK: updated golden manifest ($(wc -l < "${MANIFEST}" | tr -d ' ') files) + ${FLAKY_REL} reference"
  exit 0
fi

############################################################################
# Test mode: compare current run output to golden.
[[ -f "${MANIFEST}" ]] || { echo "NO: missing golden manifest: ${MANIFEST}. Run: $0 --update"; exit 1; }

FAIL=0

# 1) Manifest (everything except the flaky file) must match byte-for-byte.
DIFF_OUT="$(diff "${MANIFEST}" <(gen_manifest) || true)"
if [[ -n "${DIFF_OUT}" ]]; then
  echo "NO: run output diverges from golden manifest:"
  echo "${DIFF_OUT}" | head -50
  FAIL=1
fi

# 2) The flaky file: identical, or differs only by the lone `st -32` flicker.
FLICKER=0
if [[ -f "${FLAKY_REF}" ]]; then
  if [[ ! -f "${RUN_LIB}/${FLAKY_REL}" ]]; then
    echo "NO: missing run file ${FLAKY_REL}"
    FAIL=1
  else
    FDIFF="$(diff "${FLAKY_REF}" "${RUN_LIB}/${FLAKY_REL}" || true)"
    if [[ -n "${FDIFF}" ]]; then
      # Tolerate ONLY added/removed lines that are exactly ` st -32`.
      RESIDUAL="$(printf '%s\n' "${FDIFF}" | grep -E '^[<>]' | grep -vE '^[<>][[:space:]]+st -32$' || true)"
      if [[ -z "${RESIDUAL}" ]]; then
        FLICKER=1
      else
        echo "NO: ${FLAKY_REL} diverges beyond the known st -32 flicker:"
        echo "${FDIFF}" | head -50
        FAIL=1
      fi
    fi
  fi
fi

if [[ "${FAIL}" -ne 0 ]]; then
  exit 1
fi

if [[ "${FLICKER}" -eq 1 ]]; then
  echo "YES"
  echo "  note: known 'st -32' flicker present in ${FLAKY_REL} (tracked 64-bit/missing-prototype bug)"
else
  echo "YES"
fi
exit 0
