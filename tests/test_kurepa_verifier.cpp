#include "mathhunter/conjectures/kurepa.hpp"
#include "mathhunter/verifier/kurepa_verifier.hpp"
#include "test_harness.hpp"

using namespace mathhunter;

int main() {
    verifier::KurepaVerifier kurepa_verifier;

    auto outcome = conjectures::kurepa::search_range(3, 5000);
    MH_REQUIRE(std::holds_alternative<core::BoundedSearchResult<arithmetic::Integer>>(outcome));
    const auto bounded = std::get<core::BoundedSearchResult<arithmetic::Integer>>(outcome);

    auto verified = kurepa_verifier.verify_bounded_result(bounded);
    MH_REQUIRE(verified.has_value());
    MH_CHECK(verified->get().candidates_tested == bounded.candidates_tested);

    {
        auto tampered = bounded;
        tampered.candidates_tested += 1;
        auto result = kurepa_verifier.verify_bounded_result(tampered);
        MH_CHECK(!result.has_value());
        if (!result.has_value()) {
            MH_CHECK(result.error() == verifier::VerificationFailure::RangeIncomplete);
        }
    }

    {
        conjectures::kurepa::Witness fabricated;
        fabricated.p = arithmetic::Integer(97);
        auto result = kurepa_verifier.verify_counterexample(fabricated);
        MH_CHECK(!result.has_value());
        if (!result.has_value()) {
            MH_CHECK(result.error() == verifier::VerificationFailure::ClaimDoesNotHold);
        }
    }

    {
        conjectures::kurepa::Witness composite;
        composite.p = arithmetic::Integer(9);
        auto result = kurepa_verifier.verify_counterexample(composite);
        MH_CHECK(!result.has_value());
        if (!result.has_value()) {
            MH_CHECK(result.error() == verifier::KurepaDomainFailure::NotPrime);
        }
    }

    {
        conjectures::kurepa::Witness even_p;
        even_p.p = arithmetic::Integer(2);
        auto result = kurepa_verifier.verify_counterexample(even_p);
        MH_CHECK(!result.has_value());
        if (!result.has_value()) {
            MH_CHECK(result.error() == verifier::VerificationFailure::WitnessOutOfDomain);
        }
    }

    {
        auto payload_bytes = conjectures::kurepa::encode_bounded_result_payload(bounded);
        auto decoded = conjectures::kurepa::decode_bounded_result_payload(payload_bytes);
        MH_REQUIRE(decoded.has_value());
        auto reverified = kurepa_verifier.verify_bounded_result(*decoded);
        MH_CHECK(reverified.has_value());
    }

    MH_TEST_MAIN_EPILOGUE();
}
