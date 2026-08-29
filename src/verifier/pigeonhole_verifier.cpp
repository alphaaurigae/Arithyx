#include "mathhunter/verifier/pigeonhole_verifier.hpp"

namespace mathhunter::verifier {

using conjectures::pigeonhole::UnsatProof;

std::expected<PigeonholeVerifier::VerifiedProof, PigeonholeVerifier::Failure>
PigeonholeVerifier::verify_unsat(const UnsatProof& claimed) const {
    if (claimed.pigeons == 0) {
        return std::unexpected(VerificationFailure::WitnessOutOfDomain);
    }
    if (claimed.pigeons > kMaxBruteForcePigeons) {
        return std::unexpected(VerificationFailure::DomainTooLargeForVerifier);
    }

    const std::uint32_t holes = claimed.pigeons - 1;
    const auto cnf = conjectures::pigeonhole::encode_php(claimed.pigeons, holes);

    if (cnf.num_vars != conjectures::pigeonhole::expected_var_count(claimed.pigeons, holes)) {
        return std::unexpected(VerificationFailure::EncodingMismatch);
    }
    if (cnf.clauses.size() !=
        conjectures::pigeonhole::expected_clause_count(claimed.pigeons, holes)) {
        return std::unexpected(VerificationFailure::EncodingMismatch);
    }

    const auto rebuilt_hash = sat::hash_cnf(cnf);
    if (!core::constant_time_equal(rebuilt_hash, claimed.formula_hash)) {
        return std::unexpected(VerificationFailure::HashMismatch);
    }

    const std::uint32_t v = cnf.num_vars;
    if (v >= 63) {
        return std::unexpected(VerificationFailure::DomainTooLargeForVerifier);
    }

    const std::uint64_t total_assignments = (v == 0) ? 1ULL : (1ULL << v);
    std::vector<bool> assignment(static_cast<std::size_t>(v) + 1, false);
    for (std::uint64_t mask = 0; mask < total_assignments; ++mask) {
        for (std::uint32_t var = 1; var <= v; ++var) {
            assignment[var] = ((mask >> (var - 1)) & 1ULL) != 0;
        }
        if (sat::evaluate(cnf, assignment)) {
            return std::unexpected(PigeonholeDomainFailure::UnsatClaimRefuted);
        }
    }

    return VerifiedProof(core::CandidateProof<UnsatProof>{.claim = core::ClaimId::PigeonholeUnsat,
                                                            .proof = claimed,
                                                            .discovered_by = "verified"},
                          core::VerificationToken{});
}

}  // namespace mathhunter::verifier
