#pragma once

#include <expected>

#include "mathhunter/conjectures/toy_mod7.hpp"
#include "mathhunter/verifier/verification_error.hpp"

namespace mathhunter::verifier {

class ToyMod7Verifier final {
public:
    using VerifiedCounterexample =
        core::VerifiedCounterexample<conjectures::toy_mod7::Witness, conjectures::toy_mod7::Evidence>;
    using VerifiedBoundedResult = core::VerifiedBoundedResult<std::uint64_t>;

    [[nodiscard]] std::expected<VerifiedCounterexample, VerificationFailure> verify_counterexample(
        const conjectures::toy_mod7::Witness& witness) const;

    [[nodiscard]] std::expected<VerifiedBoundedResult, VerificationFailure> verify_bounded_result(
        const core::BoundedSearchResult<std::uint64_t>& claimed) const;
};

}  // namespace mathhunter::verifier
