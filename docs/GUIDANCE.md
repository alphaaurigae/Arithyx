# math-hunter — Guidance

This document is the map of the territory: what got built, why it was built
that way, what was deliberately left out, and what to do next. It is written
against the original task brief, which asked for a computational-mathematics
research framework organized around one rule: **discovery is not proof**.

This is a **prototype**, not the finished framework the brief ultimately
describes. It implements the brief's own "Development Order" items 1–9 in
full (core result model → arithmetic → certificates → verifier → search →
Kurepa → Kurepa certificate → Kurepa verification → tests), plus a working
SAT backend and a second worked example, and stops there deliberately. The
brief itself says: *"Do not generate enormous amounts of code merely to
appear comprehensive. Prefer a smaller correct system over a large
speculative framework."* Everything below was chosen with that sentence as
the tiebreaker.

Every claim about "this passes" or "this was tested" in this document is
something that was actually run in the environment this was built in —
`ctest` output, `clang-tidy` output, and sanitizer runs are described
accurately, not aspirationally.

---

## 1. The one rule, in code

The brief's central requirement — that a search program can never mark its
own result as proven — is enforced with the type system, not just with
convention:

```cpp
class VerificationToken final {
    VerificationToken() = default;         // private constructor
    friend class mathhunter::verifier::KurepaVerifier;
    friend class mathhunter::verifier::ToyMod7Verifier;
    friend class mathhunter::verifier::PigeonholeVerifier;
};

template <typename Payload>
class Verified final {
public:
    Verified(Payload payload, VerificationToken);   // needs a token to construct
    ...
};
```

`core::result.hpp` defines seven distinct result shapes (`Observation`,
`CandidateCounterexample`, `VerifiedCounterexample`, `BoundedSearchResult`,
`VerifiedBoundedResult`, `CandidateProof`, `VerifiedProof`). None of them is
a `bool success`. The only way to obtain a `Verified<T>` anywhere in the
codebase is to be one of the three friended verifier classes — search code
physically cannot construct one. This is checked by the compiler, not by
convention or code review.

(One nuance: `math-hunter` *does* link the verifier code and calls it
automatically after a search, printing `VERIFIED` / `VERIFICATION FAILED`
immediately. This is a deliberate reading of the brief, which asks for both
"the verifier must be a separate executable" *and* "automatically run the
independent verifier" on discovery — see §4.)

---

## 2. What was implemented

### 2.1 Core (`src/core/`, `include/mathhunter/core/`)
- `result.hpp` — the seven result types and the `VerificationToken` gate
  described above, plus a `ClaimId` enum (never a raw string) identifying
  which mathematical claim a result belongs to.
- `hash.hpp` / `hash.cpp` — SHA-256 via OpenSSL EVP, both one-shot
  (`sha256(...)`) and streaming (`Sha256Stream`) forms, plus
  `constant_time_equal` for digest comparison and `to_hex` for display. Used
  for certificate integrity and CNF identity — explicitly documented (both
  in code-adjacent naming and in this doc) as **integrity, not proof**: a
  hash establishes "this is the same data", never "this data is true".
- `byteio.hpp` — bounds-checked `ByteReader`/`ByteWriter` over
  `std::expected<T, ByteIoError>`. Every read either succeeds or returns a
  typed error (`UnexpectedEof`, `LengthLimitExceeded`, `TrailingGarbage`);
  nothing throws on malformed input, and nothing reads past the buffer.
  Endianness is normalized explicitly with `std::endian`/`std::byteswap`
  rather than assumed.

### 2.2 Arithmetic (`src/arithmetic/`)
- `modular.hpp/.cpp` — exact arithmetic on `mpz_class` (GMP): `gcd`, `mod`,
  `mulmod`, `powmod`, `factorial_mod`, `left_factorial_mod`, and
  `is_prime_exact`. Primality is **deterministic**, not probabilistic:
  `is_prime_exact` runs Miller–Rabin against the fixed 12-witness set
  `{2,3,5,7,11,13,17,19,23,29,31,37}`, which is a *proven* deterministic
  test for all `n < 3,317,044,064,679,887,385,961,981` (~2^71.7). Past that
  bound, the function throws `DomainError` rather than silently falling
  back to a probabilistic test — the brief explicitly forbids probabilistic
  evidence standing in for proof, and primality of a Kurepa witness is
  proof-critical, so "I don't know, and I won't guess" was the only
  acceptable failure mode.
- `primes.hpp/.cpp` — a **second, independent** deterministic primality
  test, written against native `uint64_t` and `unsigned __int128` instead
  of GMP, used only by the *search* side for speed. This means the
  search-side primality test and the verifier-side primality test
  (`is_prime_exact`) are two different implementations of the same
  mathematical fact, which is exactly the brief's "compare against an
  independent implementation" requirement, built into the architecture
  rather than bolted on as an extra check.

### 2.3 Certificates (`src/certificate/`)
- `certificate.hpp/.cpp` — the on-disk envelope: 8-byte magic, format
  version, claim id, certificate kind, a length-prefixed payload (capped at
  16 MiB), and a trailing SHA-256 digest over everything before it. Parsing
  is fail-closed at every step: bad magic, unsupported version, unknown
  claim/kind enum value, oversized length prefix, truncation, trailing
  garbage after the digest, and digest mismatch are all distinct, named
  `CertificateError` values, never exceptions, never undefined behavior.
- `bigint_codec.hpp/.cpp` — canonical big-endian encoding of non-negative
  `mpz_class` values via `mpz_export`/`mpz_import`, so certificates remain
  meaningful for witnesses larger than 64 bits even though the current
  conjecture modules don't need that range in practice.

### 2.4 SAT backend (`src/sat/`)
- `cnf.hpp/.cpp` — a CNF representation with a canonical byte serialization
  (used for hashing) and a direct `evaluate()` function for checking an
  assignment against a formula.
- `dpll_solver.hpp/.cpp` — a from-scratch DPLL solver with unit
  propagation. This is the **search-side, untrusted, optimized** component.
  Nothing downstream trusts its output without independent re-checking.

### 2.5 Conjecture modules (`src/conjectures/`, one verifier each in `src/verifier/`)

| Module | Statement | Status | Search backend | Verifier backend |
|---|---|---|---|---|
| `kurepa` | For every odd prime `p`, `p` does not divide `K(p) = sum_{i=0}^{p-1} i!` | **Open** (real conjecture; see metadata for a note on a disputed 2022 proof claim) | sequential odd-prime search, fast native primality, GMP left-factorial-mod | independent GMP-based deterministic primality + from-scratch left-factorial-mod recomputation |
| `toy_mod7` | For all `n` in range, `7` does not divide `n` | **Deliberately false**, a test fixture | linear scan | independent `n % 7` recomputation |
| `pigeonhole` | `PHP(n)`: `n` pigeons can't fit in `n-1` holes | **Solved** (labeled a known-result benchmark, per the brief's explicit instruction to label solved problems as benchmarks) | DPLL solver | CNF rebuilt from `n` alone (never trusts the certificate's CNF), checked against closed-form variable/clause-count invariants, hash-compared, then confirmed UNSAT by brute-force enumeration of all `2^V` assignments |

Each module's search side returns a `variant` of "found a counterexample" or
"no counterexample in this range" (or, for pigeonhole, "found a proof" vs.
"anomaly" — see below) — never a bare boolean, and never anything the
verifier is obligated to trust.

**On the pigeonhole "anomaly" path:** since `PHP(n)` is a *proven* theorem,
if the DPLL solver ever reports SAT, that is not treated as a mathematical
discovery — it is reported as `core::Observation` with a note that this
indicates an encoder or solver bug, exactly matching the brief's
distinction between "observation" and every stronger result category. No
code path allows a solver anomaly to be written to a proof certificate.

**Why metadata is JSON, not C++:** `conjectures/metadata/*.json` holds the
statement, domain, status, references, and known bounds for each claim.
`math-hunter list`/`info` read this at runtime (`nlohmann::json`, located
via `MATHHUNTER_METADATA_DIR` or a few conventional relative paths). This
was a specific, explicit brief requirement ("Do not hardcode all research
metadata into C++") and was followed literally rather than approximated
with a `constexpr` string table.

**On the Kurepa metadata's accuracy:** the statement, the 2016 verified
bound (2^34, Andrejic & Tatarevic), and the note about the disputed 2022
proof claim (and the earlier 2004 Barsky-Benzaghou claim that was retracted
in 2011) were checked against current sources before being written down,
specifically because the brief warns against ever inventing or overstating
a mathematical record. If you update this metadata, keep it that way.

### 2.6 CLI (`src/cli/`)
Two separate binaries, `math-hunter` and `math-verifier`, sharing the same
static library but never calling into each other's process:
- `math-hunter list|info|search|benchmark` — discovery only.
- `math-verifier verify|inspect` — independent verification only.
  `inspect` deliberately does *no* verification and says so in its output,
  so it can't be mistaken for a validity check.

### 2.7 Tests (`tests/`)
Six suites, all passing under both a normal build and a full
`-fsanitize=address,undefined,leak` build (GCC 13.3, C++23):

- `test_modular` — arithmetic primitives, known left-factorial values
  (`!1..!5`), primality on primes/composites, the deterministic-bound
  overflow throwing `DomainError`, cross-checks between the two independent
  primality implementations.
- `test_certificate_roundtrip` — write/read fidelity.
- `test_certificate_corruption` — **adversarial**: single-bit flip, magic
  corruption, truncation, trailing garbage, unknown claim id, empty file,
  and an oversized length-prefix field. Every one is rejected with the
  correct typed error.
- `test_kurepa_verifier` — verifies a real bounded search result end to
  end, then specifically checks that the verifier *rejects*: a tampered
  candidate count, a fabricated non-counterexample (`p = 97`), a composite
  passed as a witness, and `p = 2` as out-of-domain.
- `test_toy_verifier` — same shape, cheap conjecture, exercises the
  counterexample path (finds and verifies `n = 7`) as well as the
  bounded-no-counterexample path.
- `test_pigeonhole` — encoding invariants for `n = 1..5`, full search+verify
  round trip, a hash-tamper rejection, a zero-pigeons domain rejection, a
  capacity-exceeded case (`n = 7`, beyond the brute-force verifier's
  `kMaxBruteForcePigeons = 6`), and a demonstration of the SAT-witness path
  (`PHP(n, n)`, which is satisfiable, solved and independently evaluated).

Beyond the automated suite, the full CLI pipeline was run manually end to
end, including a certificate **hand-crafted with a Python script that never
touched any math-hunter code**, claiming `p = 97` is a Kurepa
counterexample — `math-verifier` rejected it with `ClaimDoesNotHold`,
because it recomputes `K(97) mod 97` itself and gets a nonzero residue. That
is the property the whole architecture exists to guarantee: a certificate
is checked against the mathematics, not against its own provenance.

Static analysis: `clang-tidy` (`modernize-*`, `performance-*`, `bugprone-*`,
`readability-*`, `cert-*`) runs clean (zero diagnostics) over every file in
`src/`, with a small number of deliberate, documented suppressions in
`.clang-tidy` (single-statement guard-clause braces, commutative-argument
"swappable parameters" warnings, and verifier classes kept non-static as an
extension point). Note: this sandbox's `clang-tidy` needed
`-stdlib=libc++` to see `<expected>` at all — the installed clang's
`__cpp_concepts` value (`201907L`) is below what libstdc++'s `<expected>`
header guards on (`>= 202002L`); the actual GCC 13.3 build used for the real
binaries is unaffected. This is a documented toolchain quirk of this
sandbox, not a project defect — see the README for the exact commands.

---

## 3. Final Quality Gate (from the brief), answered honestly

| Question | Answer |
|---|---|
| Can the system discover a known counterexample? | Yes — `toy_mod7` finds `n=7` on demand; this is the intended stand-in since no real Kurepa counterexample exists to discover. |
| Can it independently verify that counterexample? | Yes, by a verifier class that never touches search code (`ToyMod7Verifier`, `KurepaVerifier`). |
| Does modifying one bit of the certificate cause verification failure? | Yes — `test_certificate_corruption`, single-bit case, rejected via digest mismatch. |
| Can it reproduce a known bounded mathematical result? | Yes — Kurepa bounded search over any range with no counterexample, independently re-derived by the verifier from scratch. |
| Does the verifier reject an incomplete search? | Yes — a tampered `candidates_tested` count is rejected with `RangeIncomplete`. |
| Does the verifier distinguish bounded verification from an unrestricted theorem? | Yes at the type level (`BoundedSearchResult<T>` is a distinct type — there is no "theorem" type at all, deliberately, because nothing in this prototype proves an unrestricted universal claim) and at the UX level (CLI output explicitly says "This is a BOUNDED result. It says nothing about primes outside this interval."). |
| Can two independent implementations agree on the certificate? | Yes for primality (native `__int128` MR vs. GMP MR) and yes for CNF (rebuilt from `n` alone, hash-compared to the certificate's recorded hash rather than trusted). |
| Can a SAT UNSAT result be independently checked? | Yes, by brute-force enumeration, capacity-limited and explicit about that limit (see §4). |
| Can the SAT encoding itself be sanity-checked? | Yes — closed-form variable/clause-count invariants are checked against the encoder's actual output before any UNSAT claim is trusted. |
| Can a search be divided into independent ranges without changing the result? | Architecturally yes (`begin`/`end` are explicit parameters everywhere, no shared mutable state); not exercised by an actual parallel/distributed run in this prototype — see §5. |
| Can an interrupted search resume safely? | **No.** Checkpointing was explicitly deferred (brief's Development Order item 14, after the initial modules). See §5. |
| Are all proof-critical calculations exact? | Yes — no floating point anywhere in arithmetic, certificate, or verifier code. |
| Can malformed untrusted input crash the verifier? | Not that testing found — every parse path returns a typed error, confirmed under ASan/UBSan/LeakSan across the full test suite and a manual pass (truncation, huge length prefixes, corrupted enum values). No exhaustive fuzz harness exists yet (see §5). |
| Can an incorrect certificate ever be accepted? | Not found in testing — this is exactly what `test_certificate_corruption`, `test_kurepa_verifier`'s rejection cases, and the hand-crafted-fabrication smoke test are aimed at. |
| Is every mathematical claim backed by a clearly defined verification procedure? | Yes for the three implemented modules; there is no code path that reports a result without a corresponding verifier for its claim id. |

---

## 4. Deliberate deviations from the literal brief, and why

**`math-hunter` also runs verification automatically.** The brief says both
"the verifier must be a separate executable" (hard requirement, satisfied:
`math-verifier` is a real, independent, minimal-dependency binary that
never invokes `math-hunter`) and "[on finding a candidate] automatically
run the independent verifier" (a workflow requirement). Both are satisfied
by having `math-hunter` link the same verifier *library code* and call it
immediately after a search, while always writing a standalone certificate
file that `math-verifier` can re-check later, from a different machine,
with the original search program deleted. The certificate's validity never
depends on `math-hunter` having run the check correctly — that's exactly
what the hand-crafted-fabrication test above demonstrates.

**The SAT UNSAT verifier is brute-force, not DRAT/LRAT.** The brief allows
this explicitly ("Investigate and support established SAT proof formats
such as DRAT/LRAT/FRAT **where practical**"). A from-scratch DRAT checker
is a substantial project on its own; brute-force enumeration is *simpler,
more auditable, and requires trusting strictly less* than a proof checker
would (a DRAT checker has to be trusted to be correct; "try every
assignment" only has to be trusted to loop correctly). The tradeoff is
capacity: `PigeonholeVerifier::kMaxBruteForcePigeons = 6` (30 boolean
variables, ~10^9 assignments) is the practical ceiling on a workstation.
Beyond that, `math-verifier` reports `DomainTooLargeForVerifier` — a
distinct outcome from both "verified" and "rejected" — rather than silently
trusting the solver. This is the single biggest scaling gap in the
prototype; see §5.

**Kurepa's `left_factorial_mod` takes a `uint64_t` loop bound, not an
arbitrary-precision one.** The deterministic primality bound is ~2^71, but
computing `K(p)` for `p` anywhere near that is computationally infeasible
regardless of integer width — the real constraint is CPU time, not
representable range. `try_to_u64` politely rejects (as
`WitnessOutOfDomain`) any witness that doesn't fit, rather than silently
truncating.

**`ClaimId`'s underlying type is `uint8_t`, encoded as `uint32_t` in the
file format.** The in-memory enum was narrowed after a `clang-tidy
performance-enum-size` finding; the on-disk format still reserves 4 bytes
for the claim id so the file format has room to grow past 256 claim kinds
without a version bump, while the in-memory type stays minimal.

---

## 5. What's explicitly out of scope here, and the priority order to add it

This list is the direct continuation of the brief's "Development Order"
(items 10 onward) plus a few things the quality-gate table above flags as
gaps. Roughly in the order they'd pay for themselves:

1. **Checkpointing.** No search in this prototype can resume. For Kurepa
   this is low-risk to add (the search is already a pure function of
   `[begin, end)`; a checkpoint is just "the last prime fully processed,"
   integrity-checked the same way certificates are). Do this before
   attempting any search range large enough to actually threaten the
   current computational-verification record.
2. **DRAT/LRAT proof checking for the SAT backend.** This is what actually
   removes the `kMaxBruteForcePigeons = 6` ceiling and is the prerequisite
   for the SAT backend being useful on anything but toy instances. A
   minimal LRAT checker (verify each clause addition is implied by unit
   propagation over prior clauses) is a well-scoped, well-documented
   algorithm and is the natural next SAT-backend milestone.
3. **A genuinely independent second CNF encoder for pigeonhole** (or for
   whatever the next SAT-natural conjecture is), rather than the current
   "rebuild from `n` with the same function + structural invariant check."
   The current approach is honest about being a partial mitigation (see
   `pigeonhole.json`'s `verification_backend` field); a hand-written
   second encoder closes the remaining gap the brief calls out explicitly:
   *"a formally valid UNSAT proof of the wrong CNF proves nothing."*
4. **FLINT integration**, specifically for anything that becomes a
   factorial-arithmetic bottleneck at scale. Not needed yet:
   `left_factorial_mod` is not currently the bottleneck at prototype scale,
   and the brief's own instruction is "benchmark, then optimize," not the
   reverse.
5. **A real parallel/distributed range-decomposition run**, even
   single-machine, to validate the "workers are independently reproducible
   and re-combinable" property in practice rather than just by
   architectural argument (every search function already takes an explicit
   `[begin, end)` and touches no shared mutable state, which is the
   precondition — nothing about the search functions needs to change to
   parallelize them, only the CLI/orchestration layer needs to exist).
6. **More conjecture modules**, per the brief's "at least X" minimums that
   were satisfied at the floor (one real open conjecture, one SAT-natural
   benchmark, one toy fixture). Good next candidates that fit the existing
   architecture with no new backend: other left-factorial-adjacent
   congruence problems (the brief explicitly asks for "several modular
   factorial conjecture modules" after Kurepa), and Ramsey-number-style
   SAT-natural problems for the SAT backend once DRAT/LRAT exists (so the
   new module isn't stuck at the same brute-force ceiling).
7. **A fuzz harness** (libFuzzer/AFL over `read_certificate_file` and each
   `decode_*_payload` function) to replace "manual adversarial test cases +
   sanitizers" with continuous, generated adversarial input. The manual
   corruption tests cover the failure modes we thought of; a fuzzer covers
   the ones we didn't.
8. **Property-based and differential tests** for the arithmetic layer
   (e.g., `left_factorial_mod(n, m)` against a slow reference computed with
   unbounded-precision exact factorials for small `n`, or metamorphic tests
   like `left_factorial_mod(n, m1*m2)` consistent with the CRT combination
   of `mod m1` and `mod m2`). The current tests are example-based
   (known values, known primes); property tests would catch classes of bug
   the examples happen not to exercise.
9. **Symbolic/algebraic backend interface.** Genuinely not started. The
   brief allows this ("optional symbolic/algebraic backend interface...
   only implement concrete functionality where it is actually useful") and
   nothing in the three implemented modules needs one, so it was left
   alone rather than built as an unused abstraction.
10. **Benchmarking suite + PGO/LTO.** `math-hunter benchmark <id>` exists
    as a minimal timing harness for each module; there is no systematic
    benchmark suite, flamegraph/profiling workflow, or PGO/LTO build yet.
    Per the brief's own ordering ("profile, then optimize"), this should
    follow real usage at a scale where it matters, not precede it.
11. **Cluster orchestration.** Explicitly out of scope per the brief itself
    ("Do not implement cluster infrastructure now"). The interfaces are
    shaped for it (pure functions over explicit ranges, no shared state,
    deterministic given `[begin, end)` and a seed) but no coordinator,
    RPC, or job queue exists, intentionally.

---

## 6. Extending the framework

**Adding a conjecture module** (mirrors `toy_mod7`, the simplest example):
1. Add a `ClaimId` value in `core/result.hpp`.
2. Add `CertificateKind` value(s) in `certificate/certificate.hpp` (and its
   `is_known_kind`/`to_string` in the `.cpp`).
3. Write `conjectures/<name>.hpp/.cpp`: a `Witness` type, an `Evidence`
   type, a `search_range`-style function returning a `std::variant` of
   "found" vs. "not found" result types, and `encode_*`/`decode_*` payload
   functions built on `core::ByteWriter`/`ByteReader`.
4. Write `verifier/<name>_verifier.hpp/.cpp`: friend it in
   `core::VerificationToken`, implement `verify_*` methods that **recompute
   independently** rather than trust the witness's attached evidence, and
   return `std::expected<Verified<...>, verifier::VerificationFailure>`.
5. Add a JSON file under `conjectures/metadata/` — statement, domain,
   status, references, known bounds, sourced and checked, not invented.
6. Wire it into `cli/hunter_main.cpp` and `cli/verifier_main.cpp`'s
   dispatch, and add a `tests/test_<name>.cpp` with at least: a correct
   round trip, and one deliberately-wrong witness the verifier must reject.

**Adding a certificate type / backend / SAT encoding** follows the same
shape: new `CertificateKind`, new payload codec next to the thing being
certified, new verifier method that never calls into the search-side code
for its answer.

---

## 7. On the legacy prototype

The task referenced a legacy project (`generator.cpp`/`verifier.cpp`,
`irreducibility_N_dev`) as "design inspiration only." It was read, but
almost none of its structure survived: it brute-forces 2^27 assignments
looking for random-3-SAT instances that happen to be UNSAT, wraps the
search in classes named `KolmogorovTester` and `CircuitEnumerator` that
don't implement Kolmogorov complexity or circuit enumeration (the first is
just Shannon entropy of literal frequency; the second is just a
clause/variable ratio threshold), and calls the result a "proof artifact."
Nothing in it distinguishes discovery from proof, verifies anything
independent of the same generator seed, or corresponds to a real
mathematical claim — which is precisely the failure mode this brief is
structured to prevent. It was treated as a cautionary example of what
"looks rigorous" can hide, not as a starting point to refactor.
