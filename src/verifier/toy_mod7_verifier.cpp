#include "mathhunter/verifier/toy_mod7_verifier.hpp"

namespace mathhunter::verifier {

using conjectures::toy_mod7::Evidence;
using conjectures::toy_mod7::Witness;

std::expected<ToyMod7Verifier::VerifiedCounterexample, VerificationFailure>
ToyMod7Verifier::verify_counterexample(const Witness& witness) const {
    const std::uint64_t remainder = witness.n % 7;
    if (remainder != 0) {
        return std::unexpected(VerificationFailure::ClaimDoesNotHold);
    }
    core::VerifiedCounterexamplePayload<Witness, Evidence> payload;
    payload.claim = core::ClaimId::ToyMod7;
    payload.witness = witness;
    payload.evidence = Evidence{.remainder = remainder};
    return VerifiedCounterexample(payload, core::VerificationToken{});
}

std::expected<ToyMod7Verifier::VerifiedBoundedResult, VerificationFailure>
ToyMod7Verifier::verify_bounded_result(
    const core::BoundedSearchResult<std::uint64_t>& claimed) const {
    if (claimed.begin > claimed.end) {
        return std::unexpected(VerificationFailure::RangeMismatch);
    }
    std::uint64_t independently_tested = 0;
    for (std::uint64_t n = claimed.begin; n < claimed.end; ++n) {
        ++independently_tested;
        if (n % 7 == 0) {
            return std::unexpected(VerificationFailure::ClaimDoesNotHold);
        }
    }
    if (independently_tested != claimed.candidates_tested) {
        return std::unexpected(VerificationFailure::RangeIncomplete);
    }
    return VerifiedBoundedResult(claimed, core::VerificationToken{});
}

}  // namespace mathhunter::verifier
