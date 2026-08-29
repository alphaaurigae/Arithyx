#include "mathhunter/verifier/kurepa_verifier.hpp"

namespace mathhunter::verifier {

using conjectures::kurepa::Evidence;
using conjectures::kurepa::Witness;

std::expected<KurepaVerifier::VerifiedCounterexample, VerificationFailure>
KurepaVerifier::verify_counterexample(const Witness& witness) const {
    if (witness.p <= 2) {
        return std::unexpected(VerificationFailure::WitnessOutOfDomain);
    }
    if (!arithmetic::is_odd(witness.p)) {
        return std::unexpected(VerificationFailure::WitnessOutOfDomain);
    }
    if (!arithmetic::is_prime_exact(witness.p)) {
        return std::unexpected(VerificationFailure::NotPrime);
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

std::expected<KurepaVerifier::VerifiedBoundedResult, VerificationFailure>
KurepaVerifier::verify_bounded_result(
    const core::BoundedSearchResult<arithmetic::Integer>& claimed) const {
    const auto begin_u64 = arithmetic::try_to_u64(claimed.begin);
    const auto end_u64 = arithmetic::try_to_u64(claimed.end);
    if (!begin_u64 || !end_u64) {
        return std::unexpected(VerificationFailure::RangeMismatch);
    }
    if (*begin_u64 > *end_u64) {
        return std::unexpected(VerificationFailure::RangeMismatch);
    }

    std::uint64_t independently_tested = 0;
    for (std::uint64_t candidate = std::max<std::uint64_t>(*begin_u64, 2); candidate < *end_u64;
         ++candidate) {
        if (candidate == 2) {
            continue;
        }
        const arithmetic::Integer p(candidate);
        if (!arithmetic::is_prime_exact(p)) {
            continue;
        }
        ++independently_tested;
        const arithmetic::Integer residue = arithmetic::left_factorial_mod(candidate, p);
        if (residue == 0) {
            return std::unexpected(VerificationFailure::ClaimDoesNotHold);
        }
    }

    if (independently_tested != claimed.candidates_tested) {
        return std::unexpected(VerificationFailure::RangeIncomplete);
    }

    return VerifiedBoundedResult(claimed, core::VerificationToken{});
}

}  // namespace mathhunter::verifier
