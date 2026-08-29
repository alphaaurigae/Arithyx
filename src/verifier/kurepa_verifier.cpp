#include "mathhunter/verifier/kurepa_verifier.hpp"

namespace mathhunter::verifier {

using conjectures::kurepa::Evidence;
using conjectures::kurepa::Witness;

std::expected<KurepaVerifier::VerifiedCounterexample, KurepaVerifier::Failure>
KurepaVerifier::verify_counterexample(const Witness& witness) const {
    if (witness.p <= 2) {
        return std::unexpected(VerificationFailure::WitnessOutOfDomain);
    }
    if (!arithmetic::is_odd(witness.p)) {
        return std::unexpected(VerificationFailure::WitnessOutOfDomain);
    }
    if (!arithmetic::is_prime_exact(witness.p)) {
        return std::unexpected(KurepaDomainFailure::NotPrime);
    }

    const auto p_u64 = arithmetic::try_to_u64(witness.p);
    if (!p_u64) {
        return std::unexpected(VerificationFailure::WitnessOutOfDomain);
    }

    const arithmetic::Integer residue = arithmetic::left_factorial_mod(*p_u64, witness.p);
    if (residue != 0) {
        return std::unexpected(VerificationFailure::ClaimDoesNotHold);
    }

    core::VerifiedCounterexamplePayload<Witness, Evidence> payload;
    payload.claim = core::ClaimId::KurepaLeftFactorial;
    payload.witness = witness;
    payload.evidence = Evidence{.residue = residue};
    return VerifiedCounterexample(std::move(payload), core::VerificationToken{});
}

std::expected<KurepaVerifier::VerifiedBoundedResult, KurepaVerifier::Failure>
KurepaVerifier::verify_bounded_result(
    const core::BoundedSearchResult<arithmetic::Integer>& claimed) const {
    if (claimed.begin > claimed.end) {
        return std::unexpected(VerificationFailure::RangeMismatch);
    }
    if (claimed.end - claimed.begin > arithmetic::Integer(kMaxBoundedRangeSize)) {
        return std::unexpected(VerificationFailure::DomainTooLargeForVerifier);
    }

    std::uint64_t independently_tested = 0;
    try {
        arithmetic::Integer candidate = claimed.begin < 2 ? arithmetic::Integer(2) : claimed.begin;
        for (; candidate < claimed.end; ++candidate) {
            if (candidate == 2) {
                continue;
            }
            if (!arithmetic::is_prime_exact(candidate)) {
                continue;
            }
            ++independently_tested;
            const arithmetic::Integer residue = arithmetic::left_factorial_mod(candidate, candidate);
            if (residue == 0) {
                return std::unexpected(VerificationFailure::ClaimDoesNotHold);
            }
         }
    } catch (const arithmetic::DomainError&) {
        return std::unexpected(VerificationFailure::RangeMismatch);
     }
 
     if (independently_tested != claimed.candidates_tested) {
        return std::unexpected(VerificationFailure::RangeIncomplete);
    }

    return VerifiedBoundedResult(claimed, core::VerificationToken{});
}

}  // namespace mathhunter::verifier
