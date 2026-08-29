#include "mathhunter/conjectures/toy_mod7.hpp"
#include "mathhunter/verifier/toy_mod7_verifier.hpp"
#include "test_harness.hpp"

using namespace mathhunter;

int main() {
    verifier::ToyMod7Verifier toy_verifier;

    auto found = conjectures::toy_mod7::search_range(1, 100);
    MH_REQUIRE(std::holds_alternative<core::CandidateCounterexample<conjectures::toy_mod7::Witness>>(
        found));
    const auto candidate =
        std::get<core::CandidateCounterexample<conjectures::toy_mod7::Witness>>(found);
    MH_CHECK(candidate.witness.n == 7);

    auto verified = toy_verifier.verify_counterexample(candidate.witness);
    MH_REQUIRE(verified.has_value());
    MH_CHECK(verified->get().evidence.remainder == 0);

    {
        conjectures::toy_mod7::Witness fabricated;
        fabricated.n = 8;
        auto result = toy_verifier.verify_counterexample(fabricated);
        MH_CHECK(!result.has_value());
        if (!result.has_value()) {
            MH_CHECK(result.error() == verifier::VerificationFailure::ClaimDoesNotHold);
        }
    }

    auto safe_range = conjectures::toy_mod7::search_range(1, 7);
    MH_REQUIRE(
        std::holds_alternative<core::BoundedSearchResult<std::uint64_t>>(safe_range));
    const auto bounded = std::get<core::BoundedSearchResult<std::uint64_t>>(safe_range);
    MH_CHECK(bounded.candidates_tested == 6);

    auto verified_bounded = toy_verifier.verify_bounded_result(bounded);
    MH_REQUIRE(verified_bounded.has_value());

    {
        auto tampered = bounded;
        tampered.end = 8;
        auto result = toy_verifier.verify_bounded_result(tampered);
        MH_CHECK(!result.has_value());
        if (!result.has_value()) {
            MH_CHECK(result.error() == verifier::VerificationFailure::ClaimDoesNotHold ||
                     result.error() == verifier::VerificationFailure::RangeIncomplete);
        }
    }

    MH_TEST_MAIN_EPILOGUE();
}
