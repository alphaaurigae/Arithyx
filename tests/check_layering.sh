#!/bin/bash
#
# Phase 2 regression guard (docs/GUIDANCE.md, Phase 2 definition of done).
#
# The invariant this enforces is architectural, not textual: sat:: (the
# CNF/DPLL SAT backend) may be depended on only by its own implementation
# and by the one module that is genuinely SAT-shaped (pigeonhole) --
# including that module's own tests. It must never leak into the generic
# framework/registry/core/certificate/verification-interface layer that
# every module (arithmetic-range-shaped or SAT-shaped alike) shares. That
# generic layer staying free of sat:: is the actual thing Phase 1's ABI
# split was for; grepping for the literal token is just how we check it
# mechanically.
#
# Two complementary checks:
#
#   1. ALLOWLIST SCAN: every file under src/, include/, tests/ that
#      contains the literal token `sat::` must be one of the explicitly
#      allowed SAT-dependent surfaces below. Anything else finding a new
#      reason to say `sat::` -- anywhere in the tree -- fails the build.
#
#   2. GENERIC-BOUNDARY ASSERTION: a curated list of files that make up
#      the generic framework/core/certificate/verification-interface
#      boundary (core/, framework/, certificate/, the generic verifier
#      concepts, and -- the interesting one -- pigeonhole's own verifier
#      *header*, whose interface is generic even though its .cpp
#      implementation legitimately isn't) are each explicitly re-checked
#      and reported by name. This turns "no violation found elsewhere by
#      omission" into a positive, itemized assertion, and also fails if
#      one of these files goes missing (the architecture moved and this
#      guard's own knowledge of it is stale).
#
# include/mathhunter/verifier/pigeonhole_verifier.hpp deliberately does
# NOT appear in the SAT-dependent allowlist below and DOES appear in the
# generic-boundary list: it's the one header where a SAT-shaped module's
# public verifier interface stays exactly as generic as Kurepa's or
# toy_mod7's, with the sat:: dependency confined to the .cpp behind it.
# That split -- not the literal three-path count from the original
# checklist wording -- is what "closes out Phase 2 honestly" means.

set -euo pipefail

if [[ $# -ge 1 ]]; then
    REPO_ROOT="$1"
else
    REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
fi

cd "${REPO_ROOT}"

# --- Check 1: allowlist scan -----------------------------------------------

ALLOWED_PREFIXES=(
    "src/sat/"
    "include/mathhunter/sat/"
)

ALLOWED_FILES=(
    "src/conjectures/pigeonhole.cpp"
    "include/mathhunter/conjectures/pigeonhole.hpp"
    "src/verifier/pigeonhole_verifier.cpp"
    "tests/test_pigeonhole.cpp"
)

is_allowed() {
    local path="$1"
    local prefix
    for prefix in "${ALLOWED_PREFIXES[@]}"; do
        [[ "${path}" == "${prefix}"* ]] && return 0
    done
    local allowed
    for allowed in "${ALLOWED_FILES[@]}"; do
        [[ "${path}" == "${allowed}" ]] && return 0
    done
    return 1
}

mapfile -t MATCHES < <(
    grep -rIl \
        --include='*.hpp' --include='*.cpp' --include='*.h' --include='*.cc' \
        --exclude-dir=build --exclude-dir=.git \
        -e 'sat::' -- src include tests 2>/dev/null || true
)

violations=()
for path in "${MATCHES[@]}"; do
    rel="${path#./}"
    if ! is_allowed "${rel}"; then
        violations+=("${rel}")
    fi
done

if [[ ${#violations[@]} -gt 0 ]]; then
    echo "[FAIL] sat:: symbol referenced outside its allowed layering boundary:" >&2
    for v in "${violations[@]}"; do
        echo "  ${v}" >&2
        grep -n 'sat::' -- "${v}" | sed 's/^/    /' >&2
    done
    echo "" >&2
    echo "See docs/GUIDANCE.md, Phase 2 definition of done." >&2
    exit 1
fi

# --- Check 2: generic-boundary assertion -----------------------------------
#
# These files ARE the generic contract: every module (arithmetic-range or
# SAT-shaped) is expected to sit behind it identically. If any of them
# ever contains `sat::`, that is exactly the "pigeonhole forces SAT stuff
# into the generic interface" regression Phase 2 exists to catch.

GENERIC_BOUNDARY_FILES=(
    "include/mathhunter/core/result.hpp"
    "include/mathhunter/core/byteio.hpp"
    "include/mathhunter/core/hash.hpp"
    "include/mathhunter/core/encoded_blob.hpp"
    "include/mathhunter/framework/module_registry.hpp"
    "include/mathhunter/framework/backend_concepts.hpp"
    "include/mathhunter/framework/cli_args.hpp"
    "include/mathhunter/framework/work_unit.hpp"
    "include/mathhunter/framework/tracker.hpp"
    "include/mathhunter/framework/executor.hpp"
    "include/mathhunter/framework/range_partition.hpp"
    "include/mathhunter/certificate/certificate.hpp"
    "include/mathhunter/certificate/bigint_codec.hpp"
    "include/mathhunter/verifier/verification_error.hpp"
    "include/mathhunter/verifier/verification_backend.hpp"
    "include/mathhunter/verifier/pigeonhole_verifier.hpp"
    "src/core/encoded_blob.cpp"
    "src/framework/module_registry.cpp"
    "src/framework/work_unit.cpp"
    "src/framework/tracker.cpp"
    "src/framework/executor.cpp"
    "src/framework/range_partition.cpp"
    "src/certificate/certificate.cpp"
    "src/certificate/bigint_codec.cpp"
)

boundary_violations=()
missing_files=()
for f in "${GENERIC_BOUNDARY_FILES[@]}"; do
    if [[ ! -f "${f}" ]]; then
        missing_files+=("${f}")
        continue
    fi
    if grep -q 'sat::' -- "${f}"; then
        boundary_violations+=("${f}")
    fi
done

if [[ ${#missing_files[@]} -gt 0 ]]; then
    echo "[FAIL] generic-boundary file(s) expected by this guard no longer exist" \
         "(architecture moved; update GENERIC_BOUNDARY_FILES in tests/check_layering.sh):" >&2
    for f in "${missing_files[@]}"; do
        echo "  ${f}" >&2
    done
    exit 1
fi

if [[ ${#boundary_violations[@]} -gt 0 ]]; then
    echo "[FAIL] sat:: leaked into the generic framework/core/certificate/verification" \
         "boundary -- this is the exact contamination Phase 2 is meant to catch:" >&2
    for f in "${boundary_violations[@]}"; do
        echo "  ${f}" >&2
        grep -n 'sat::' -- "${f}" | sed 's/^/    /' >&2
    done
    echo "" >&2
    echo "See docs/GUIDANCE.md, Phase 2 definition of done." >&2
    exit 1
fi

echo "[OK] sat:: layering guard:"
echo "  allowlist scan:    no violations (${#MATCHES[@]} SAT-dependent file(s) confirmed legitimate)"
echo "  generic boundary:  ${#GENERIC_BOUNDARY_FILES[@]} file(s) confirmed free of sat::, incl. pigeonhole_verifier.hpp"
exit 0
