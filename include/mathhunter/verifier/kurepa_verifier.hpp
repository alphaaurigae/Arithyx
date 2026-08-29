#pragma once

#include <cstdint>
#include <expected>

#include "mathhunter/conjectures/kurepa.hpp"
#include "mathhunter/verifier/kurepa_domain_failure.hpp"
#include "mathhunter/verifier/verification_error.hpp"

namespace mathhunter::verifier {

class KurepaVerifier final {
public:
    using DomainFailure = KurepaDomainFailure;
    using Failure = VerificationOutcome<DomainFailure>;

    using VerifiedCounterexample =
        core::VerifiedCounterexample<conjectures::kurepa::Witness, conjectures::kurepa::Evidence>;
    using VerifiedBoundedResult = core::VerifiedBoundedResult<arithmetic::Integer>;

    static constexpr std::uint64_t kMaxBoundedRangeSize = 1'000'000;

    [[nodiscard]] std::expected<VerifiedCounterexample, Failure> verify_counterexample(
        const conjectures::kurepa::Witness& witness) const;

    [[nodiscard]] std::expected<VerifiedBoundedResult, Failure> verify_bounded_result(
        const core::BoundedSearchResult<arithmetic::Integer>& claimed) const;
};

}  // namespace mathhunter::verifier
