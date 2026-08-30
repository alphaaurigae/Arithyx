Original phase mockup:
"I would therefore use this order
Phase 1 — ABI

Define the minimum contract for a module:

Module
 ├── identity
 ├── metadata
 ├── discovery backend(s)
 ├── verification backend(s)
 ├── work-unit representation
 └── certificate representation

Do not optimize it yet.

The test should be:

Can I add a completely different conjecture without modifying framework code?

If the answer is no, the ABI isn't finished.

Phase 2 — migrate the three existing prototypes

Don't immediately delete anything.

Use the three prototypes as adversarial ABI tests:

Kurepa       → arithmetic/range-style
toy_mod7     → arithmetic/range-style
pigeonhole   → SAT/enumeration-style

The objective isn't to make them fast.

The objective is:

Can all three implement the same framework contract without contaminating that contract with assumptions from any one of them?

If pigeonhole forces you to add "SAT stuff" to the generic interface, that's a warning.

Likewise if Kurepa forces everything to have begin/end.

Phase 3 — generic WU

This is where I strongly agree with the audit.

Don't define:

struct WorkUnit {
    uint64_t begin;
    uint64_t end;
};

as the framework abstraction.

Instead conceptually:

WorkUnit
 ├── identity
 ├── serialized partition key
 └── module-defined semantics

The framework should be able to say:

give me WU X
run WU X
report result for WU X

without knowing whether X represents:

[1000000, 2000000)

or:

SAT assumptions: x1=false,x7=true,...

or:

Turing machine IDs 18200–18300

or:

a graph-search partition

That is the abstraction that will actually make later distributed execution straightforward.

Phase 4 — tracker

Only after the WU abstraction exists.

Then the tracker becomes genuinely generic:

PENDING
   ↓
LEASED
   ↓
RUNNING
   ↓
RESULT
   ↓
VERIFIED

with retry/timeout semantics around it.

The tracker shouldn't care what mathematics is being computed.

Phase 5 — distribution

Only here would I start thinking seriously about BOINC/MPI/etc.

At that point the distributed system becomes an implementation detail:

                 Framework
                    │
              WorkUnit API
                    │
       ┌────────────┼────────────┐
       │            │            │
    local        cluster       BOINC
   executor       executor     executor

That's much healthier than designing the framework around BOINC and subsequently discovering that a SAT cube or Turing-machine batch doesn't fit.

Phase 6 — experimental discovery backends

LLM/evolutionary search should be treated exactly as the audit suggests:

DiscoveryBackend
       │
       ├── brute force
       ├── heuristic
       ├── SAT
       ├── evolutionary
       └── LLM-guided

All of them have exactly the same authority:

none.

They generate candidates.

The verifier decides.

That is an excellent property for this kind of project because you can eventually throw increasingly insane discovery algorithms at the framework without having to expand the trust boundary."


------------------------------

Actual phase progress

Phase 1 — ABI
The two axes that must be kept separate

Re-reading core/result.hpp, certificate.hpp, and the three modules together surfaces something worth naming explicitly, because getting it right is what makes the rest of this tractable: there are two independent axes, and only one of them is currently modeled generically.

Claim shape — already correctly generic today: Observation<T> / CandidateCounterexample<T> / VerifiedCounterexample<T,E> (falsification search), BoundedSearchResult<T> / VerifiedBoundedResult<T> (bounded completeness), CandidateProof<T> / VerifiedProof<T> (proof search). These three shapes are domain-agnostic templates and should stay a small, closed, framework-owned set — don't let a 4th module invent a 4th shape ad hoc; if one genuinely needs a new shape, that's a deliberate framework-level decision, not something a module silently bolts on.
Claim domain — currently not modeled generically at all. It's implicit in each module's Witness/Evidence/free-function signatures. This is the axis Phase 1 actually has to build.

The ABI's job is: make domain fully opaque to the framework, while shape stays something the framework understands (because Verified<T>'s token gate, certificate structure, and the tracker's result states all key off shape, never domain).

Contract sketch (conceptual — not for implementation yet)
Module identity     : stable string id, matches conjectures/metadata/<id>.json
Module metadata      : loaded at runtime from JSON (already true — keep it true)
Discovery backend(s)  : concept-constrained type, run(WorkUnitKey) -> DiscoveryOutcome<Module>
Verification backend(s): concept-constrained type, verify(Witness|ProofData) -> expected<Verified<...>, ...>
Work-unit representation: module-owned opaque bytes + module-owned cost estimator
Certificate representation: module-owned encode/decode of its own payload bytes only
template <typename B, typename M>
concept DiscoveryBackend = requires(B b, const typename M::WorkUnitKey& wu) {
    { b.run(wu) } -> std::same_as<typename M::DiscoveryOutcome>;
};

template <typename B, typename M>
concept VerificationBackend = requires(B b, const typename M::ClaimInput& in) {
    { b.verify(in) } -> std::same_as<std::expected<core::Verified<typename M::VerifiedPayload>,
                                                     VerificationFailure>>;
};

Two deliberate C++23 design decisions, both aligned with your existing preferences:

Concepts, not virtual base classes, for the module contract itself. Discovery/verification are the hot path (this is exactly where "no virtual functions in hot paths, prefer CRTP/function objects" applies) — a module's DiscoveryBackend/VerificationBackend should be structurally checked at compile time, zero vtable indirection inside a search loop.
Type erasure only at the outer boundary, i.e. exactly one place: "which module did the CLI/tracker ask for" — a small self-registering factory/registry (module registers itself via static init, framework looks it up by string id) is fine there because it's called once per job dispatch, not per candidate. This is the correct split between "must be fast" (inside a module) and "correctness/genericity matters more than speed" (module selection).
Certificate payload becomes typed-per-family, not one schema
struct CertificatePayload {
    ClaimFamilyTag family;          // small closed enum the framework owns (arithmetic_residue | proof_object | opaque_external)
    std::vector<std::byte> bytes;   // module-owned; framework never interprets contents
};

The outer envelope (certificate.hpp's magic/version/digest machinery) stays exactly as it is today — it's already correctly scoped to integrity/versioning, not content. The change is narrow: certificate.hpp should stop knowing about CertificateKind::KurepaCounterexample etc. by name (today it does, via the hand-edited is_known_kind switch) and instead dispatch decode/interpretation entirely into the module via the registry, keeping only the family tag generic.

Operationalizing your acceptance test

"Can I add a completely different conjecture without modifying framework code?" is the right test but needs a checklist to actually be checkable, because some touches are expected build wiring, not contamination:

Must NOT require edits (this is the actual test):

core/result.hpp — the three shape templates
certificate/certificate.hpp — envelope logic
verifier/verification_error.hpp — see finding below, this currently would need edits, which is a bug in the current design
cli/hunter_main.cpp's command dispatch — currently an if (conjecture == "kurepa") ... else if (conjecture == "toy_mod7") ... chain, which fails this test today; it needs to become registry-driven (iterate registered modules) before Phase 1 can be called done

Expected/acceptable to touch (build wiring, not framework contamination):

CMakeLists.txt glob already picks up new src/conjectures/*.cpp/src/verifier/*.cpp files automatically — fine
tests/CMakeLists.txt needs one new mathhunter_add_test(...) line — fine, that's test wiring, not shared logic
A new conjectures/metadata/<id>.json — expected, that's the whole point of metadata-driven identity
Concrete finding: VerificationFailure already fails the Phase-2 test today

Re-reading include/mathhunter/verifier/verification_error.hpp: the enum mixes genuinely generic/structural failures (HashMismatch, MalformedCertificate, RangeMismatch, RangeIncomplete) with domain-specific ones (NotPrime — arithmetic-only, UnsatClaimRefuted/SatClaimRefuted/DomainTooLargeForVerifier — SAT-only). A 4th module with a genuinely different domain (say, a decider/enumeration-style claim) would either have to shoehorn its rejection reasons into this list — editing a shared header, failing the test — or leave it unedited and lose typed rejection reasons. This needs splitting before Phase 2 can honestly validate anything:

Framework-owned, shape-level failures (stay in a small closed enum): digest/malformed/range-incomplete-style structural failures that apply regardless of domain.
Module-owned domain rejection reasons: not a shared enum at all — either a module-scoped enum type parameter, or a generic ClaimDoesNotHold{std::string detail} variant carrying a domain-specific message, decided in Phase 1, then stress-tested honestly in Phase 2 rather than discovered as a surprise there.
Phase 1 definition of done
 DiscoveryBackend/VerificationBackend concepts formally written (not just duck-typed convention)
 VerificationFailure split into framework/shape-level vs module-owned domain reasons
 CertificateKind-style hand-edited switches replaced by registry dispatch
 cli/hunter_main.cpp dispatch is data-driven off the registry, not an if/else chain of hardcoded names
 Module registry exists as the only type-erasure boundary in the codebase
Phase 1 — status: DONE (all five checklist items), implemented directly
against kurepa/toy_mod7/pigeonhole with zero behavioral change (verified by
the existing test suite, unmodified except one call site renamed, see
below, plus full ASan+UBSan+LeakSanitizer and -Werror runs, plus clang-tidy
against every new/changed translation unit).

Concrete decisions made, since the sketch above was explicitly conceptual:

- VerificationFailure was split exactly as this document proposed, but the
  "generic ClaimDoesNotHold{std::string detail} variant" idea was
  generalized one step further: a verifier's real error type is
  `std::variant<VerificationFailure, ModuleFailure>` (wrapped as
  `VerificationOutcome<ModuleFailure>`, include/mathhunter/verifier/
  verification_error.hpp), with heterogeneous `operator==` against either
  alternative so existing call sites comparing against a bare
  VerificationFailure value keep compiling unchanged. ModuleFailure is a
  plain module-owned enum (KurepaDomainFailure, PigeonholeDomainFailure),
  not a string, so `to_string` stays exhaustively switch-checked per
  module. ToyMod7Verifier needs no ModuleFailure at all and still returns
  bare VerificationFailure — shape-optionality confirmed one phase early.
- Correction to this document: DomainTooLargeForVerifier is not SAT-only.
  KurepaVerifier::verify_bounded_result already returns it today
  (kMaxBoundedRangeSize). It stays a framework/shape-level value.
- DiscoveryBackend/VerificationBackend became concept families, not single
  concepts, because the framework has three closed claim shapes today, not
  one: RangeSearchDiscovery/KeyedProofSearchDiscovery (include/mathhunter/
  framework/backend_concepts.hpp) and CounterexampleVerifier/
  BoundedResultVerifier/UnsatProofVerifier (include/mathhunter/verifier/
  verification_backend.hpp), all static_assert-checked against the real
  free functions/classes in each module's *_registration.cpp. This
  deliberately does not introduce Phase 3's WorkUnitKey opaque-bytes
  abstraction; RangeSearchDiscovery still models a [begin,end) pair and
  KeyedProofSearchDiscovery still models a single opaque key, matching
  what the modules actually do today.
- The module registry (include/mathhunter/framework/module_registry.hpp)
  is the sole type-erasure boundary: certificate.cpp's is_known_claim/
  is_known_kind, and both CLI binaries' search/benchmark/inspect/verify
  dispatch, now only call ModuleRegistry. Each module registers itself
  via a `[[gnu::constructor(N)]]`-attributed function in its own
  src/conjectures/<id>_registration.cpp — true self-registration, not a
  hand-maintained call list — which also means every executable linking
  mathhunter_core must use `--whole-archive`
  (`$<LINK_LIBRARY:WHOLE_ARCHIVE,mathhunter_core>` in CMakeLists.txt/
  tests/CMakeLists.txt): nothing else references symbols inside a
  *_registration.cpp translation unit, so a normal static-archive link
  would silently drop it and the module would never register. This is the
  one non-obvious trap in "module registers itself via static init" for a
  static-library target; worth keeping in mind for Phase 2/3 modules too.
  Explicit `[[gnu::constructor(1001/1002/1003)]]` priorities on
  kurepa/toy_mod7/pigeonhole keep CLI help-text ordering identical to
  before purely for cosmetic stability; nothing depends on this order.
- CertificateKind's on-disk enum values were deliberately left untouched
  (still literal per-module tags, e.g. KurepaCounterexample = 1). Making
  the certificate *kind* space itself module-opaque is a bigger,
  binary-format-affecting change than "replace the hand-edited switch
  that checks/dispatches on it," which is what the Phase 1 checklist
  actually asked for; revisit if/when Phase 3's opaque WorkUnitKey work
  also wants an opaque on-disk kind space.

Acceptance-test re-check: adding a 4th conjecture module today requires:
a new src/conjectures/<id>.cpp + <id>_registration.cpp (auto-globbed),
a new conjectures/metadata/<id>.json, and one new `mathhunter_add_test(...)`
line in tests/CMakeLists.txt if it gets a test. No edits to core/result.hpp,
certificate.hpp/.cpp, verification_error.hpp, or either CLI main. CLI help
text, claim/kind validation, and search/benchmark/inspect/verify dispatch
all pick it up automatically.

Phase 2 — migrate the three prototypes as adversarial ABI tests

Per-module pressure points, grounded in what's actually in the codebase (not hypothetical):

kurepa (arithmetic/range, falsification+bounded-completeness dual shape). WorkUnitKey = serialized [begin, end) of arithmetic::Integer (GMP-backed, arbitrary precision). Already-good precedent: BoundedSearchResult<Domain> is template-generic over Domain, not hardcoded to Integer — confirm this stays true after Phase 1's refactor.

toy_mod7 (same shape family, different witness width). This is the actual test for whether Phase 1's Witness/WorkUnitKey genericity is real: its witness is a plain uint64_t, not mpz_class. If any shared path silently assumes GMP types (it currently doesn't — bigint_codec.hpp is correctly kurepa-only, not referenced from generic certificate code — confirmed clean), that's contamination. Keep verifying this stays true as things move.

pigeonhole (SAT/proof-search, no witness at all). The real pressure test, on two fronts:

WorkUnitKey is a single n, not a range — validates that Phase 1's opaque-bytes design doesn't secretly assume "two endpoints."
Its claim shape is CandidateProof<UnsatProof> — there is no witness that disproves anything, only a proof object plus a formula-hash. This argues against forcing every module to define a Witness type: a module using only the proof-search shape should not need to define Witness/Evidence at all, only ProofData. Decide this explicitly in Phase 1 rather than let Phase 2 discover it needs a workaround.

Additional contamination check already done above: sat::Cnf/sat::Clause types are correctly confined to src/sat/ and src/conjectures/pigeonhole.cpp today — none of core/, certificate/, or the generic verifier machinery reference them. Good — Phase 2 should add a lint/grep-based check (not a heavy tool, literally grep -rL for sat:: outside those two directories) to keep it that way as a regression guard, since this is exactly the kind of thing that quietly regresses during refactors.

Phase 2 definition of done
 All three modules implement Phase 1's concepts with zero shared-code edits beyond the registry entry
 pigeonhole confirmed not to require Witness/Evidence — proves shape-optionality works
 toy_mod7 confirmed not to require GMP anywhere in shared code — proves witness-width genericity
 Grep guard: sat:: may appear only within the SAT backend and explicitly SAT-dependent module/test surfaces; it must never appear in generic framework, registry, work-unit, certificate, or generic verification interfaces
 No performance work done in this phase — explicitly out of scope, per your "core not feature" framing
Phase 2 — status: DONE (5/5). Closes the one gap STATUS audit 1 found
("Missing grep guard: no check_layering.sh-style script or corresponding
ctest entry appears anywhere in tests/CMakeLists.txt or elsewhere").

Revised criterion (post-review): the checklist line above originally read
"no sat:: symbol outside src/sat/, src/conjectures/pigeonhole.cpp,
src/verifier/pigeonhole_verifier.cpp" — three literal paths. On review
that wording was replaced with the semantically stronger version now
shown, because literal-path matching and the actual architectural goal
diverge the moment you also grep headers: `include/mathhunter/
conjectures/pigeonhole.hpp` legitimately spells `sat::Cnf` as
`encode_php()`'s return type, and contorting that header into an opaque
handle purely to make a literal-path grep pass would be architecture
driven by the guard rather than by the real module/backend boundary. The
thing actually worth protecting is that no *SAT assumption* leaks into
the generic layer every module sits behind — core/, framework/,
certificate/, and the generic verifier concepts — not that the string
`sat::` is confined to an exact file list picked before the headers were
checked against it.

Concrete decisions made:

- tests/check_layering.sh runs two complementary checks, both wired into
  `tests/CMakeLists.txt` as the `check_layering` ctest case so `ctest
  --test-dir build` exercises them on every run:
    1. **Allowlist scan** — every `.hpp`/`.cpp`/`.h`/`.cc` under `src/`,
       `include/`, `tests/` (excluding `build/`, `.git/`) containing the
       literal token `sat::` must be one of the explicitly allowed
       SAT-dependent surfaces: `src/sat/`, `include/mathhunter/sat/`,
       `src/conjectures/pigeonhole.cpp`, `include/mathhunter/conjectures/
       pigeonhole.hpp`, `src/verifier/pigeonhole_verifier.cpp`, and
       `tests/test_pigeonhole.cpp` (the last two deliberately exercise
       `sat::` directly as part of testing the SAT-shaped module). Any
       new file anywhere in the tree that starts saying `sat::` for any
       other reason fails the build immediately.
    2. **Generic-boundary assertion** — a curated, named list of the
       files that *are* the generic contract (`core/result.hpp`,
       `core/byteio.hpp`, `core/hash.hpp`, `framework/module_registry.*`,
       `framework/backend_concepts.hpp`, `framework/cli_args.hpp`,
       `certificate/certificate.*`, `certificate/bigint_codec.*`,
       `verifier/verification_error.hpp`, `verifier/
       verification_backend.hpp`, and — the interesting one —
       `verifier/pigeonhole_verifier.hpp`) is individually re-checked and
       reported by name, rather than left as "everything not on the
       allowlist, by omission." This turns "no violation found" into a
       positive, itemized assertion of the actual invariant, and also
       fails loudly if one of these files is ever renamed/removed (the
       architecture moved and the guard's own knowledge of it is stale).
- `include/mathhunter/verifier/pigeonhole_verifier.hpp` is the deliberate
  centerpiece of check 2, not an incidental omission from check 1: it is
  the one header where a SAT-shaped module's *public verifier interface*
  is exactly as generic as Kurepa's or toy_mod7's (it names only
  `conjectures::pigeonhole::UnsatProof`), with the `sat::` dependency
  confined entirely to the `.cpp` behind it. That split — interface
  generic, implementation domain-specific — is the actual result Phase 2
  set out to prove, more so than any literal path count.
- Regression-tested both checks with positive controls, each reverted
  after confirming failure: (a) a stray `sat::` reference injected into
  `src/arithmetic/modular.cpp` (an unrelated, non-allowlisted file) is
  caught by check 1 with file+line; (b) a stray `sat::` reference
  injected into `include/mathhunter/verifier/pigeonhole_verifier.hpp`
  (the generic-boundary centerpiece) is caught by check 1 first, since
  it also isn't allowlisted, with the same clear file+line output; (c)
  temporarily removing `include/mathhunter/core/hash.hpp` is caught by
  check 2's missing-file assertion rather than silently passing. All
  three fail loudly and specifically; the working tree was confirmed
  byte-identical to before each control afterward.

Phases 3–5 — not started: no WorkUnitKey, tracker schema, or WorkUnitExecutor/LocalExecutor code exists anywhere in the provided sources. Correct.

Spot-checked specific technical citations in the audit, all verified correct against code:

[[gnu::constructor(1001/1002/1003)]] priorities on kurepa/toy_mod7/pigeonhole registration — exact match.
CertificateKind values still literal (KurepaCounterexample = 1, etc.) — exact match.
KurepaVerifier::verify_bounded_result returning DomainTooLargeForVerifier via kMaxBoundedRangeSize — exact match, supports the "not SAT-only" correction.
PigeonholeVerifier::kMaxBruteForcePigeons = 6 and the v >= 63 brute-force ceiling — exact match.
The Kurepa CLI's "re-run with an independently authored verifier" print statement, cited as evidence for Decision A — exact match in kurepa_registration.cpp.
sat:: usage confined to src/sat/, src/conjectures/pigeonhole.cpp, src/verifier/pigeonhole_verifier.cpp — consistent with what's shown (pigeonhole_registration.cpp doesn't reference sat:: directly, so the audit's suggestion to add it to the allowlist is a forward-looking recommendation, not a claim about current violation).

One assumption I have to flag: the "grep guard is missing" conclusion (and thus the whole "4/5" Phase 2 score) is only as good as the assumption that the file set given to me is the complete repository. If there's a tests/check_layering.sh or similar CI script that simply wasn't included in this document bundle, that specific line item would already be satisfied. Everything else in the audit doesn't depend on that assumption.

----

STATUS AUDIT 2

The whole-archive/[[gnu::constructor]] self-registration pattern is a real, sharp trap for a static-library architecture, and Phase 3's WorkUnitKey work and Phase 5's pluggable executors are both going to want modules to register more than just a ModuleDescriptor (cost estimators, executors, eventually maybe out-of-process backends). Worth writing this down explicitly as a constraint on Phase 3+ rather than rediscovering it: any new thing a module self-registers must go through the same registry, not a second ad hoc static-init mechanism, or the whole-archive linking requirement silently stops being sufficient and things register inconsistently depending on which .o happened to get pulled in by something else.
 
2. Phase 3 — generic work unit: refined plan
 
The original sketch is sound. Two refinements based on Phase 1 experience:
 
3.1 — Don't let partition bytes become a second certificate format by accident. The doc already says "reuse ByteWriter/ByteReader, don't invent a second format" — the concrete risk is partition's bytes and a certificate payload's bytes drifting into being structurally the same thing (module id + versioned encode/decode) without being literally the same code path. Recommendation: give certificate.hpp and the new work_unit.hpp a shared small EncodedBlob { std::uint16_t schema_version; std::vector<std::uint8_t> bytes; } primitive with one digest-and-frame function, so "versioned opaque bytes with a digest" is implemented exactly once and both certificates and work units are call sites, not two parallel implementations that will inevitably diverge.
 
3.2 — Determinism needs a test, not just a contract. The doc already flags this as "ideally exercised by a test." Make it non-optional: any module registering a RangeSearchDiscovery/KeyedProofSearchDiscovery should get one shared, framework-provided determinism test that just calls run(partition) twice and diff-checks — this belongs in the registry's static_assert-checked contract from Phase 1 (add a requires clause isn't possible for "is pure," but a runtime ctest fixture that iterates framework::registry().modules() and re-runs each one's last-known WU twice is cheap and catches an entire class of "silently became stateful" bugs Phase 4's retry logic will depend on being absent).
 
3. Phase 4 — tracker: refined plan + decisions needed
 
The state machine and schema in the doc are good and I wouldn't change them structurally. Two things need a decision from you before Phase 4 can start, because they change the schema:
 
Decision A — what "VERIFIED" is allowed to mean across a distributed swarm. Today, single-process, Verified<T>'s token can only be constructed by the one verifier class that ships in the binary. Once results come from untrusted or merely unaudited workers, "the tracker's own single verifier said yes" is a much weaker claim than what the project's core principle ("discovery is not proof") implies. Concretely: does results.verifier_state = VERIFIED in the Phase 4 schema mean (a) one verifier run, trusted because it's the tracker's own code, or (b) N independently-invoked verifier runs (possibly by different worker processes, or by the tracker itself re-running the verifier out of an abundance of caution) that agree? (b) is more expensive and slower to reach FLAGGED/VERIFIED but is the honest extension of what the CLI already does today (see the Kurepa search output's own text: "re-run with an independently authored verifier before announcing anything" — right now that's a human instruction in a print statement, not an enforced property). I'd lean towards (b) with a configurable quorum (default 1, so today's behavior is the default), specifically because "not enforcing it" is a decision that's very hard to retrofit later without a schema migration, whereas "support quorum=1 today, raise it later" costs nothing now. Flagging this as a real design fork, not a formality — I don't think there's a clearly-correct default here, it depends on how much you trust your own worker fleet.
 
Decision B — persistence choice. The doc says Poco::Data+SQLite, matching your library-priority ordering (poco > nlohmann > flint > gmp > ...). That's fine for Phase 4's explicitly local-only, single-file validation goal. It will not survive contact with concurrent writers once Phase 5 adds a real ClusterExecutor — SQLite's single-writer model means the tracker process becomes a hard bottleneck and single point of failure the moment more than one tracker instance needs to exist (e.g. for HA). That's fine to defer, but worth deciding now whether Phase 4's schema is meant to be the final schema (in which case, design it against Postgres semantics even if SQLite is what runs first) or explicitly a throwaway prototype schema you expect to redo in Phase 5. I'd design Phase 4's schema to be Postgres-compatible from day one — same DDL runs on both, UNIQUE(wu_id, worker_id) and the state-machine columns are portable — and only decide the actual production database when Phase 5's ClusterExecutor is real. This costs nothing extra now and avoids a second schema migration later.
 
4. Phase 5 — pluggable executor: refined plan + decisions needed
 
The WorkUnitExecutor concept and the three-implementation plan (Local/Cluster/Boinc) are good. One addition worth deciding before writing ClusterExecutor:
 
Decision C — trust model: internal cluster or public volunteer compute? BoincExecutor in the original doc implies the latter is at least on the table eventually. This is a fork in the road that changes almost everything downstream of it: an internal cluster can trust workers (no sandboxing needed beyond normal process isolation, results can be merely audited); public/volunteer compute (BOINC-style, or a public-bounty model) means a worker can be actively adversarial, not just buggy, which pushes Decision A firmly towards quorum > 1 as non-optional, and adds a whole track of work this document's phases don't currently cover: hermetic sandboxing per work unit (the WU's partition bytes are the entire input, and run() is documented as pure — that purity contract is exactly what makes sandboxing tractable: no network, no filesystem, bounded memory/CPU, seccomp or a container per WU), and almost certainly cryptographic worker identity/reputation rather than a bare worker_id string. I would not build sandboxing speculatively before this is decided — it's real engineering cost with no payoff for an internal-cluster-only deployment — but the WorkUnitKey/pure-function contract Phase 3 is about to lock in is precisely the thing that makes sandboxing possible later without another ABI break, so Phase 3 is the last cheap point to confirm "yes, run() really must stay pure, with no exceptions for convenience" before it's load-bearing for security rather than just correctness.
 
5. Cross-cutting production tracks (not owned by any single phase)
 
These aren't in the original phase plan at all — they're the gap between "the ABI/tracker/executor design is sound" and "this is something you'd actually run." None of them block Phase 3; all of them block calling this "production."
 
Observability. Nothing currently logs anything structured — CLI output is for a human at a terminal. A tracker with retrying workers and a FLAGGED state needs structured, queryable logs/metrics (WU lease/expire/reject counts per module, verifier disagreement rate) or FLAGGED WUs become invisible until someone thinks to look.
Configuration. Today everything is CLI flags and env vars (MATHHUNTER_METADATA_DIR). A tracker process needs a real config story (DB connection, TTLs, quorum, per-module resource limits) separate from one-shot CLI ergonomics.
On-disk format versioning discipline. CertificateFile::format_version exists but nothing exercises "read a v1 certificate with a binary that knows about v2" — worth a test the moment there's any reason to bump the version, so the version field is a tested contract, not a field that's there in case.
Threat model, written down explicitly. Even for an internal-cluster deployment: what happens if a worker process is compromised, not just buggy? Decision A/C above are the two biggest levers here; the answer should be a short document, not implicit in code.
Packaging/build reproducibility. .clang-tidy, sanitizer builds, and -Werror are already good CI-grade signal; formalize them as an actual CI pipeline (GitHub Actions or equivalent) running the full matrix (regular + sanitized + clang-tidy) on every change, since right now that's a set of commands in the README a human has to remember to run.
6. The "novel attempt" — what would actually differentiate this
 
The project's stated non-negotiable — "discovery is not proof," enforced by Verified<T>'s constructor-private token — is a genuinely uncommon design choice. Most prior art in this space (GIMPS, PolyMath-style distributed collaboration, general BOINC projects, SAT-competition infrastructure) treats "a worker reported a result" and "the result is trustworthy" as the same event, or separates them only by convention/manual review, not by the type system. That's the real differentiator here, and it's worth deliberately sharpening rather than diluting as the system grows. Concrete proposals, roughly in order of cost:
 
Make quorum-based independent re-verification (Decision A) the headline feature of Phase 4, not an afterthought. The FLAGGED state in the tracker diagram already gestures at "distinguish a bad worker from a poisoned WU" — extend that same instinct to verification itself: a claim isn't VERIFIED because one process said so, it's VERIFIED because N structurally-independent verifier runs agree, where "structurally independent" can start as "same code, different worker/process" and later mean "different, independently-authored verifier implementation" (the Kurepa CLI text already asks a human to do this manually today).
Replace pigeonhole's brute-force-only UNSAT verification with a real DRAT/LRAT proof pipeline (a modern CDCL solver emitting a DRAT certificate, checked by an independently-authored external checker like drat-trim or cake_lpr) for instances beyond kMaxBruteForcePigeons. This directly removes the current verifier's sharpest limitation (DomainTooLargeForVerifier at 6 pigeons) and is a natural fit for the module-owned-domain-failure ABI Phase 1 just built: pigeonhole gets a second verification backend, framework code doesn't change.
Certificate transparency log. The SHA-256 digest machinery already in certificate.hpp is one step from a Merkle-tree append-only log of every VERIFIED result across the whole tracker — tamper-evident, independently auditable by anyone without trusting the tracker operator, in the same spirit as Certificate Transparency for TLS certs. This is a genuinely novel angle for a math-conjecture-hunting framework specifically (I'm not aware of prior art combining this with distributed conjecture search) and composes naturally with Phase 4's existing results table — it's an additional derived artifact, not a schema change.
Formal-proof-assistant export for verified number-theoretic claims (Kurepa-shaped modules specifically): once a claim is Verified<T>, optionally emit a Lean 4 / mathlib-checkable artifact for the underlying arithmetic fact, so "verified" can eventually mean "independently checked by a widely-trusted external kernel," not just "this framework's own second recomputation agreed with its first." This is the most expensive item on this list and I would not start it before 1–3 are real; flagging it because it's the natural ceiling of "discovery is not proof" taken all the way, and it's worth having in view even if it's a Phase 6+ concern.
 
None of 1–4 require reopening Phase 1's ABI — that's the actual payoff of having done Phase 1 correctly: the module-owned-domain-failure split and the registry's kind-ops dispatch already have room for "a module offers more than one verifier" and "a module's certificate kind carries an optional extra proof artifact" without another framework-wide refactor.
 
7. Recommended immediate next steps, in order
Add the Phase 2 grep-guard script (§1.1) — small, closes out Phase 2 honestly.
Get an explicit answer on Decision A (quorum) and Decision B (Postgres-shaped schema now vs. later) — both are cheap to decide now and expensive to retrofit.
Start Phase 3 (WorkUnitKey, §2), with the shared EncodedBlob primitive and the determinism ctest fixture built in from the start rather than added after.
Only after Phase 3 is solid: Phase 4's tracker, schema written against whichever answer Decision B got.
Defer Decision C (public/volunteer trust model) and any sandboxing work until there's an actual answer to "who runs the workers" — don't build it speculatively.
Treat §6 item 2 (DRAT/LRAT for pigeonhole) as a good candidate for "next thing to implement" independent of the phase order above, since it's self-contained, doesn't touch the framework, and directly improves the one verifier with the sharpest known capacity limit today.

----

Editorial note (post-audit-2): recommended immediate next step #1 above
("Add the Phase 2 grep-guard script") is now done — see "Phase 2 — status:
DONE" above. Decisions A/B/C (quorum semantics, Postgres-shaped Phase 4
schema, public-vs-internal trust model) remain open and are unchanged by
this update; they still need an explicit answer from the project owner
before Phase 4 work starts, per Phase 5's "no BOINC/MPI/network code
before Phases 1-4 are proven" rule.
