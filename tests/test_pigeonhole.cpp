#include "mathhunter/conjectures/pigeonhole.hpp"
#include "mathhunter/sat/dpll_solver.hpp"
#include "mathhunter/verifier/pigeonhole_verifier.hpp"
#include "test_harness.hpp"

using namespace mathhunter;

int main() {
    for (std::uint32_t n : {1U, 2U, 3U, 4U, 5U}) {
        const auto holes = n - 1;
        const auto cnf = conjectures::pigeonhole::encode_php(n, holes);
        MH_CHECK(cnf.num_vars == conjectures::pigeonhole::expected_var_count(n, holes));
        MH_CHECK(cnf.clauses.size() == conjectures::pigeonhole::expected_clause_count(n, holes));
    }

    verifier::PigeonholeVerifier phv;

    for (std::uint32_t n : {1U, 2U, 3U, 4U, 5U}) {
        auto outcome = conjectures::pigeonhole::search_unsat_benchmark(n);
        MH_REQUIRE(std::holds_alternative<core::CandidateProof<conjectures::pigeonhole::UnsatProof>>(
            outcome));
        const auto candidate =
            std::get<core::CandidateProof<conjectures::pigeonhole::UnsatProof>>(outcome);
        MH_CHECK(candidate.proof.pigeons == n);

        auto verified = phv.verify_unsat(candidate.proof);
        MH_REQUIRE(verified.has_value());
    }

    {
        auto outcome = conjectures::pigeonhole::search_unsat_benchmark(4);
        auto candidate = std::get<core::CandidateProof<conjectures::pigeonhole::UnsatProof>>(outcome);
        candidate.proof.formula_hash[0] ^= 0xFF;
        auto result = phv.verify_unsat(candidate.proof);
        MH_CHECK(!result.has_value());
        if (!result.has_value()) {
            MH_CHECK(result.error() == verifier::VerificationFailure::HashMismatch);
        }
    }

    {
        conjectures::pigeonhole::UnsatProof zero_pigeons{};
        auto result = phv.verify_unsat(zero_pigeons);
        MH_CHECK(!result.has_value());
        if (!result.has_value()) {
            MH_CHECK(result.error() == verifier::VerificationFailure::WitnessOutOfDomain);
        }
    }

    {
        auto outcome = conjectures::pigeonhole::search_unsat_benchmark(7);
        auto candidate = std::get<core::CandidateProof<conjectures::pigeonhole::UnsatProof>>(outcome);
        auto result = phv.verify_unsat(candidate.proof);
        MH_CHECK(!result.has_value());
        if (!result.has_value()) {
            MH_CHECK(result.error() == verifier::VerificationFailure::DomainTooLargeForVerifier);
        }
    }

    {
        const auto cnf = conjectures::pigeonhole::encode_php(4, 4);
        sat::DpllSolver solver;
        const auto result = solver.solve(cnf);
        MH_REQUIRE(result.satisfiable);
        MH_CHECK(sat::evaluate(cnf, result.assignment));
    }

    MH_TEST_MAIN_EPILOGUE();
}
