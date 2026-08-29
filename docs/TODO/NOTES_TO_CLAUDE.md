# mathhunter / Kurepa large-range search — continuation notes

Context: user wants to search Kurepa's left factorial conjecture over ranges
around `10^20` and beyond (see `hunter_kurepa.pl`, exponent range
`20.121111111111111111 -> 20.121111111111111112`). Thread closed before
implementation; this is the state of investigation to resume from, once full
repo access (not just the file subset uploaded) is available.

## Files reviewed so far (uploaded subset, read-only copies)
`primes.cpp/.hpp`, `modular.cpp/.hpp`, `bigint_codec.cpp/.hpp`,
`certificate.cpp/.hpp`, `metadata_loader.cpp/.hpp`, `verifier_main.cpp`,
`hunter_main.cpp`, `toy_mod7.cpp/.hpp`, `pigeonhole.cpp/.hpp`, `hash.cpp/.hpp`,
`dpll_solver.cpp/.hpp`, `cnf.cpp/.hpp`, `kurepa.cpp/.hpp`,
`kurepa_verifier.cpp/.hpp`, `pigeonhole_verifier.cpp/.hpp`,
`toy_mod7_verifier.cpp/.hpp`, `byteio.hpp`, `result.hpp`,
`verification_error.hpp`, `hunter_kurepa.pl`.

Not yet seen: build system (CMakeLists.txt), any test suite, CI config,
`conjectures/metadata/*.json`, actual repo directory layout (paths in
`#include "mathhunter/..."` imply a `mathhunter/` include root not yet
confirmed on disk).

## THE ACTUAL BLOCKER (do not skip past this again)

`arithmetic::left_factorial_mod` in `modular.cpp`:

```cpp
Integer left_factorial_mod(std::uint64_t n, const Integer& modulus) {
    Integer term = mod(Integer(1), modulus);
    Integer sum = term;
    for (std::uint64_t k = 1; k < n; ++k) {
        term = mulmod(term, Integer(k), modulus);
        sum = mod(sum + term, modulus);
    }
    return sum;
}
```

This is `O(n)` big-integer modular multiplications to evaluate `!n mod n` for
a single candidate prime `n`. For `n ≈ 1.3×10^20` (the `10^20.121` range the
user is targeting) this is ~1.3×10^20 mulmods for **one** candidate. Even at
an optimistic 10^10 mulmods/sec, that's ~4×10^2 years per candidate, and the
range contains many primes. Widening the type from `uint64_t` to `mpz_class`
does NOT fix this — it only changes what values can be *specified*, not
whether any single value is computable in finite time. This is an
**algorithmic complexity problem**, not a type-width problem.

Also note: the function signature itself (`std::uint64_t n`) already hard-
caps candidate primes at `2^64-1` regardless of anything else — `n` can't
even be constructed above that without a signature change first.

### Two independent problems, not one
1. **Type width**: `kurepa::search_range(std::uint64_t, std::uint64_t)`,
   `arithmetic::for_each_prime_in_range(std::uint64_t, std::uint64_t, ...)`,
   `KurepaVerifier::verify_bounded_result` (via `try_to_u64(claimed.begin/end)`
   in `kurepa_verifier.cpp`), and `hunter_main.cpp`'s `parse_u64_flag` for
   `--begin`/`--end` are all `uint64_t`-bounded. Mechanical fix: GMP
   (`mpz_class` / `arithmetic::Integer`) everywhere in this pipeline. Low
   risk, well understood, I have a full change list drafted (see below).
2. **Algorithmic complexity**: `left_factorial_mod` is O(n). Fixing #1 alone
   is engineering theater at `10^20` scale — it will compile, "search," and
   never finish a single checkpoint. This must be addressed first, or the
   user must scope the target magnitude down to something #1 alone can reach
   (see feasibility table below).

## Feasibility table (single candidate cost, O(n) mulmods, rough)

Assuming ~10^8–10^9 GMP mulmods/sec/core (Integer is `mpz_class`, moduli here
are the same size as `n`, so this is arguably optimistic for larger `n`):

| n magnitude | mulmods | wall time (single core, ~10^9/s) |
|---|---|---|
| 10^7  | 10^7  | ~0.01 s |
| 10^9  | 10^9  | ~1 s |
| 10^12 | 10^12 | ~17 min |
| 10^14 | 10^14 | ~1.2 days |
| 10^15 | 10^15 | ~12 days |
| 10^18 | 10^18 | ~32 years |
| 10^20 | 10^20 | ~3170 years |

These are per-candidate, and the range around `10^20.121...` to
`10^20.121...+1` step in the current Perl script corresponds to an interval
of ~13 integers-ish per checkpoint at that step size (needs re-verification
once `exponent_to_integer` semantics are re-read against real values), each
of which will contain 0 or a handful of primes, but even one prime at that
magnitude is already computationally out of reach. **Practical target
ceiling with the current algorithm is roughly 10^12–10^14**, not 10^20,
even after all type-width fixes.

## Path to actually reaching 10^20+ (if user wants it)

Sub-linear computation of `n! mod p` (equivalently the left factorial residue)
is a studied problem, related to:
- Wilson quotient computation techniques.
- "Smooth" factorial-mod-p via subproduct-tree polynomial evaluation:
  build the product `∏(x - k)` for `k` in blocks, evaluate via
  multipoint evaluation, achieving roughly `O(√n · polylog(n))` or
  `O(n^{2/3})`-class algorithms depending on the specific construction
  (this needs literature research before implementation — do NOT invent an
  algorithm from memory without verifying correctness against small known
  values first; get numeric agreement with the current O(n) reference
  implementation on say n up to 10^7 before trusting a new fast path).
- This is a nontrivial algorithm-engineering project (weeks, not a patch),
  and changes the shape of `left_factorial_mod`'s signature/semantics
  enough that certificate format compatibility needs re-examination (does
  the certificate encode enough to let an independent verifier re-derive
  the residue via a *different* algorithm and still match? Currently yes —
  verifier calls `left_factorial_mod` independently; that stays true no
  matter which algorithm computes it, as long as both hunter and verifier
  use the same — or any correct — algorithm.)
- Do not start this without explicit user go-ahead; scope is large and the
  user has not chosen this option yet (see DECISIONS_NEEDED_FROM_USER.md).

## Drafted (not yet written) type-width fix, if/when authorized

Scope, in order, assuming repo access confirms include paths:

1. `primes.hpp`: add `#include "mathhunter/arithmetic/modular.hpp"` and a new
   overload:
   ```cpp
   void for_each_prime_in_range(const Integer& begin, const Integer& end,
                                 const std::function<bool(const Integer&)>& visitor);
   ```
   Keep the existing `uint64_t` overload — `toy_mod7` and `cmd_benchmark`
   still use the u64 path and must not regress.

2. `primes.cpp`: implement the new overload using `is_prime_exact` (already
   arbitrary-precision, already GMP-backed, already throws `DomainError`
   above `deterministic_primality_bound()` ~3.3×10^24 — reuse that guard,
   don't reinvent it).

3. `modular.hpp` / `modular.cpp`: **must** change
   `left_factorial_mod(std::uint64_t n, const Integer& modulus)` to
   `left_factorial_mod(const Integer& n, const Integer& modulus)` — otherwise
   candidates above `2^64-1` cannot even be passed in. This is required for
   step 1 (type width) regardless of whether step "algorithmic complexity"
   is ever tackled — it's the honest minimum to not silently truncate/UB on
   overflow. The loop body `for (std::uint64_t k = 1; k < n; ++k)` also needs
   to become an `Integer` loop (`for (Integer k = 1; k < n; ++k)`), which is
   yet another reason this is O(n) — the loop variable itself would be a
   bigint increment/compare each iteration, adding constant-factor overhead
   on top of the multiplications. Do this only after the complexity
   direction is decided — no point widening a loop we may replace entirely.

4. `kurepa.hpp` / `kurepa.cpp`: `search_range(std::uint64_t, std::uint64_t)`
   → `search_range(const Integer&, const Integer&)`. Note `p == 2` special
   case becomes an `Integer` comparison; `arithmetic::Integer(p)` construction
   calls become redundant (p already Integer) — clean those up, don't leave
   dead conversions.

5. `kurepa_verifier.cpp`: `verify_bounded_result` currently does
   `try_to_u64(claimed.begin)` / `try_to_u64(claimed.end)` then loops with a
   `uint64_t candidate` — this silently caps verifiable ranges at `2^64-1`
   even if the certificate format and hunter side are fixed. Must become an
   `Integer`-native loop (`Integer candidate = ...; ++candidate`). Also:
   add an explicit sanity cap on `(end - begin)` distinct from the
   `Integer` type's own capacity — nothing currently stops a certificate
   from *claiming* a bounded result over an absurdly large range and forcing
   the verifier into an unbounded loop; this is a DoS-shaped gap worth
   flagging to the user separately (verifier should refuse to brute-force
   verify ranges beyond some configured candidate count, returning
   `VerificationFailure::DomainTooLargeForVerifier`-equivalent, mirroring
   what `PigeonholeVerifier` already does with `kMaxBruteForcePigeons`).

6. `hunter_main.cpp`: `parse_u64_flag` used for kurepa's `--begin`/`--end`
   needs a decimal-string → `Integer` parser instead (GMP's
   `mpz_class(std::string)` constructor, but validate digits-only first —
   GMP will throw `std::invalid_argument` on garbage, which is acceptable
   but should be caught and turned into the existing CLI error-message
   pattern rather than crashing). `toy_mod7`/`pigeonhole` subcommands stay
   on `parse_u64_flag` — do not touch those paths.

7. Perl runner (`hunter_kurepa.pl`): replace fixed `Δexponent` checkpointing
   (`ΔN ≈ ln(10)·N·Δexponent`, grows proportional to N — a fixed tiny
   fractional exponent step becomes a gigantic integer range at high N) with
   fixed integer `RANGE_SIZE` checkpointing. This is correct and low-risk
   regardless of the complexity-algorithm decision, but only useful once (1)
   is done and pointless to build against a target magnitude the algorithm
   can't reach — confirm feasible ceiling with user first.

## Style/process reminders for continuation (user's stated prefs)
- C++23, GMP preferred over Boost (Boost forbidden unless explicit ask).
- No code comments unless explicitly tasked; no prose in code.
- Diff-format for changes to existing files unless >80% of file changes, in
  which case full file. Name target files.
- Must pass full sanitizer suite + clang-tidy modernize/performance/bugprone/
  readability/cert checks; strict const-correctness; RAII; no raw owning
  pointers.
- If removing any functionality, must call it out explicitly, one line.
- Confirm build system / CMake before assuming target names for
  `math-hunter` / `math-verifier` binaries referenced by `hunter_kurepa.pl`.

## Open technical question not yet resolved
`exponent_to_integer` in `hunter_kurepa.pl` uses `Math::BigFloat` with
`accuracy(100)` and `bpow` then `bfloor` — for `10^20.121111...111112` (18
nines... digits of precision requested at the fractional exponent level),
need to verify `Math::BigFloat`'s accuracy setting actually delivers ~100
significant digits of correctness at `10^20` scale before trusting the
integer endpoints it produces are exact — if this runner is kept at all
(even at a reduced target magnitude), that should be checked with a couple
of known test vectors before relying on it for checkpoint boundaries.
