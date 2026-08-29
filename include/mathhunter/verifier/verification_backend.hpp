#pragma once

#include <expected>

#include "mathhunter/core/result.hpp"
#include "mathhunter/verifier/verification_error.hpp"

namespace mathhunter::verifier {

// ABI Phase 1 — VerificationBackend concept family (docs/GUIDANCE.md).
//
// One concept per closed claim shape. Deliberately does not require a
// verifier to name its error type: core::ExpectedOf accepts any error
// type, so a verifier that needs no domain-specific rejection reasons
// (ToyMod7Verifier, returning plain VerificationFailure) and one that does
// (KurepaVerifier/PigeonholeVerifier, returning
// VerificationOutcome<SomeDomainFailure>) both satisfy the same concept
// with zero special-casing.

template <typename V, typename Witness, typename Evidence>
concept CounterexampleVerifier = requires(const V verifier, const Witness& witness) {
    { verifier.verify_counterexample(witness) }
        -> core::ExpectedOf<core::VerifiedCounterexample<Witness, Evidence>>;
};

template <typename V, typename Domain>
concept BoundedResultVerifier =
    requires(const V verifier, const core::BoundedSearchResult<Domain>& claimed) {
        { verifier.verify_bounded_result(claimed) }
            -> core::ExpectedOf<core::VerifiedBoundedResult<Domain>>;
    };

template <typename V, typename ProofData>
concept UnsatProofVerifier = requires(const V verifier, const ProofData& claimed) {
    { verifier.verify_unsat(claimed) } -> core::ExpectedOf<core::VerifiedProof<ProofData>>;
};

}  // namespace mathhunter::verifier
