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
 Grep guard: no sat:: symbol outside src/sat/, src/conjectures/pigeonhole.cpp, src/verifier/pigeonhole_verifier.cpp
 No performance work done in this phase — explicitly out of scope, per your "core not feature" framing
Phase 3 — generic work unit
struct WorkUnitKey {
    ModuleId module;                    // registry lookup key
    std::vector<std::byte> partition;   // module-owned encoding, opaque to framework
};

// module supplies, framework calls generically:
CostEstimate estimate_cost(partition);   // opaque scheduling hint, not business logic
DiscoveryOutcome run(partition);         // must be a pure function of `partition` alone
std::string describe(partition);         // human-readable, debug/logging only

Framework verbs are exactly the three you named — lease, run, report — and nothing else. The framework never branches on what a WU means.

Determinism requirement, stated explicitly as part of the contract: run(partition) must be side-effect-free and a pure function of partition (this is already true of search_range today — good precedent, just needs to become a documented, checked contract rather than an accident of how it happened to be written). This is what makes retries safe later and is the same property checkpointing needs, so it's worth locking down here rather than rediscovering in Phase 4/5.

Serialization: don't invent a second format. partition needs a self-describing wrapper (module id, format version, bytes, digest) for the same reasons certificates need one. Reuse core::ByteWriter/ByteReader — that machinery already exists, is already tested (test_certificate_corruption.cpp's adversarial cases apply equally well here), and inventing a second serialization scheme for WU keys would be exactly the kind of self-inflicted duplication this whole exercise is trying to avoid.

Phase 3 definition of done
 WorkUnitKey is opaque bytes + module tag, verified against all three Phase-2 modules (range-pair for kurepa/toy_mod7, single integer for pigeonhole) with no framework-level struct field named begin/end/n anywhere
 Determinism contract documented and, ideally, exercised by a test that runs the same WU twice and diff-checks output
 WU key serialization reuses ByteWriter/ByteReader, not a new format
Phase 4 — tracker

State machine, with the failure edges made explicit (your diagram plus the branches that make it correct rather than just diagrammable):

PENDING → LEASED → RUNNING → RESULT → VERIFIED
            │                   │
            ▼                   ▼
         EXPIRED             REJECTED
      (TTL/no heartbeat)   (verifier said no)
            │                   │
            ▼                   ▼
        PENDING            PENDING (single reject, different worker)
        (requeue,               │
      attempt++)                ▼
                          FLAGGED (repeated reject
                          across independent workers —
                          treat as a poisoned WU,
                          not a bad client; stop requeueing,
                          needs human review)

That FLAGGED branch matters for correctness, not just robustness: without distinguishing "one bad worker" from "this WU is itself malformed/impossible," a poisoned WU silently burns compute forever via infinite requeue. This is a correctness property of the tracker, not an optimization, which is why it belongs in Phase 4 rather than being deferred as a "later" concern.

Schema stays fully opaque, per your framing that the tracker shouldn't know what math is running:

work_units(id, module_id, wu_key_bytes BLOB, state, attempt_count, cost_estimate, created_at)
leases(id, wu_id, worker_id, leased_at, ttl_expires_at, heartbeat_at)
results(id, wu_id, worker_id, certificate_bytes BLOB, submitted_at, verifier_state, verifier_detail,
        UNIQUE(wu_id, worker_id))

The UNIQUE(wu_id, worker_id) constraint is the idempotency requirement made concrete — a worker resubmitting the same result twice (retry after a dropped ack, say) must not double-count toward quorum.

Storage stays Poco::Data+SQLite as previously discussed — no new dependency, no change from the earlier audit, this part was already right.

Phase 4 definition of done
 State machine implemented with the FLAGGED branch, not just the happy path
 Schema columns are opaque BLOBs for wu_key_bytes/certificate_bytes — tracker code never deserializes either
 Idempotent result submission enforced at the schema level (UNIQUE constraint), not just by convention
 Explicitly still local-only at this point — no network transport required to test the state machine; a same-process/single-SQLite-file tracker is sufficient to validate Phase 4 in isolation
Phase 5 — distribution as a pluggable executor

Confirmed by current research: std::execution (P2300, the scheduler/sender/receiver model with pluggable execution resources — thread-pool, single thread, GPU, io_uring, "your own") was formally adopted into C++26 at the June 2024 WG21 plenary, and a header-only, dependency-free reference implementation (stdexec, requiring only C++20) already exists and is actively used to add GPU and other scheduler backends without touching the abstraction itself. This is the community-vetted shape for exactly the problem your Phase 5 diagram describes — pluggable execution resources behind one abstraction — and it's worth deliberately mirroring rather than inventing an unrelated shape, without taking on the dependency itself yet.

concept WorkUnitExecutor = requires(E e, const WorkUnitKey& wu) {
    { e.submit(wu) } -> std::same_as<ExecutionHandle>;
    { e.poll(handle) } -> std::same_as<ExecutionStatus>;
    { e.fetch_result(handle) } -> std::same_as<std::optional<CertificatePayload>>;
};

Three implementations, all satisfying the same concept:

LocalExecutor — direct in-process call, submit() just calls run() synchronously. This is what already exists today (math-hunter search kurepa ...) — it should remain the only executor exercised through Phases 1–4, deliberately, so the ABI/WU/tracker get validated with zero infrastructure risk before anything distributed is built.
ClusterExecutor — talks to the Phase 4 tracker's REST surface.
BoincExecutor — adapter mapping WorkUnitKey bytes to BOINC job templates, per the earlier audit's §3.6 mapping.

Recommendation: don't take a dependency on stdexec now (it's explicitly experimental, and its GPU/io_uring scheduler extensions are irrelevant here) — but write the WorkUnitExecutor concept's vocabulary (scheduler-like submit/poll/fetch split) so that if/when the project's standard target eventually moves past C++23 and std::execution ships in libstdc++/libc++, swapping in the real thing is additive, not a redesign. This costs nothing now and avoids a second painful ABI migration later — the same "don't reinvent, don't collide with where the field is already going" principle as the LRAT/Mallob finding from the earlier audit.

Phase 5 definition of done
 WorkUnitExecutor concept defined, LocalExecutor implemented and is the only one used through end of Phase 4
 ClusterExecutor/BoincExecutor remain unimplemented stubs until Phases 1–4 are independently validated
 No BOINC/MPI/network code written before this phase, full stop
Consolidated kickoff TODO (bottom-up order, matches your priority)
Split VerificationFailure into framework/shape-level vs module-owned domain reasons (Phase 1 finding, blocking)
Write DiscoveryBackend/VerificationBackend concepts formally
Make certificate.hpp dispatch by registry, not hand-edited switch statements
Make cli/hunter_main.cpp dispatch registry-driven, not if/else on hardcoded names
Re-migrate kurepa, toy_mod7, pigeonhole onto the new concepts, unchanged behavior, as the adversarial test
Add the sat::-symbol grep guard as a standing regression check
Define WorkUnitKey as opaque bytes + module tag; validate against all three modules' genuinely different partition shapes
Reuse ByteWriter/ByteReader for WU key serialization
Build the Phase 4 tracker state machine (including FLAGGED) against a local SQLite file, no network yet
Define WorkUnitExecutor concept; implement LocalExecutor only
Stop. Everything past this point (ClusterExecutor, BoincExecutor, any transport, any grid tooling) is explicitly Phase 5+ and out of scope until 1–10 are done and proven against all three prototypes with zero shared-code contamination.