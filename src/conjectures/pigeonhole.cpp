#include "mathhunter/conjectures/pigeonhole.hpp"

#include "mathhunter/sat/dpll_solver.hpp"

namespace mathhunter::conjectures::pigeonhole {

namespace {

[[nodiscard]] std::int32_t var_index(std::uint32_t pigeon, std::uint32_t hole,
                                      std::uint32_t holes) {
    return static_cast<std::int32_t>((pigeon - 1) * holes + hole);
}

}  // namespace

sat::Cnf encode_php(std::uint32_t pigeons, std::uint32_t holes) {
    sat::Cnf cnf;
    cnf.num_vars = pigeons * holes;

    for (std::uint32_t i = 1; i <= pigeons; ++i) {
        sat::Clause clause;
        clause.reserve(holes);
        for (std::uint32_t j = 1; j <= holes; ++j) {
            clause.push_back(var_index(i, j, holes));
        }
        cnf.clauses.push_back(std::move(clause));
    }

    for (std::uint32_t j = 1; j <= holes; ++j) {
        for (std::uint32_t i1 = 1; i1 <= pigeons; ++i1) {
            for (std::uint32_t i2 = i1 + 1; i2 <= pigeons; ++i2) {
                cnf.clauses.push_back(sat::Clause{-var_index(i1, j, holes), -var_index(i2, j, holes)});
            }
        }
    }

    return cnf;
}

std::uint64_t expected_var_count(std::uint32_t pigeons, std::uint32_t holes) {
    return static_cast<std::uint64_t>(pigeons) * holes;
}

std::uint64_t expected_clause_count(std::uint32_t pigeons, std::uint32_t holes) {
    const std::uint64_t p = pigeons;
    const std::uint64_t pairs_per_hole = (p * (p - 1)) / 2;
    return p + static_cast<std::uint64_t>(holes) * pairs_per_hole;
}

SearchOutcome search_unsat_benchmark(std::uint32_t pigeons) {
    const std::uint32_t holes = pigeons > 0 ? pigeons - 1 : 0;
    const auto cnf = encode_php(pigeons, holes);

    sat::DpllSolver solver;
    const auto result = solver.solve(cnf);

    if (!result.satisfiable) {
        UnsatProof proof;
        proof.pigeons = pigeons;
        proof.formula_hash = sat::hash_cnf(cnf);
        core::CandidateProof<UnsatProof> candidate;
        candidate.claim = core::ClaimId::PigeonholeUnsat;
        candidate.proof = proof;
        candidate.discovered_by = std::string(kAlgorithmVersion);
        return candidate;
    }

    core::Observation<std::uint32_t> anomaly;
    anomaly.claim = core::ClaimId::PigeonholeUnsat;
    anomaly.data = pigeons;
    anomaly.note =
        "anomaly: solver reported SAT for PHP(n), contradicting the known theorem; "
        "this indicates an encoder or solver bug, not a mathematical disproof";
    return anomaly;
}

std::vector<std::uint8_t> encode_unsat_proof_payload(
    const core::CandidateProof<UnsatProof>& candidate) {
    core::ByteWriter w;
    w.put_le<std::uint32_t>(candidate.proof.pigeons);
    w.put_bytes(std::span<const std::uint8_t>{candidate.proof.formula_hash});
    w.put_length_prefixed_string(candidate.discovered_by);
    return w.bytes();
}

std::expected<DecodedUnsatProof, core::ByteIoError> decode_unsat_proof_payload(
    std::span<const std::uint8_t> payload) {
    core::ByteReader r(payload);
    auto pigeons = r.get_le<std::uint32_t>();
    if (!pigeons) return std::unexpected(pigeons.error());
    auto hash_bytes = r.get_bytes(32);
    if (!hash_bytes) return std::unexpected(hash_bytes.error());
    auto discovered_by = r.get_length_prefixed_string(256);
    if (!discovered_by) return std::unexpected(discovered_by.error());
    if (auto e = r.expect_end(); !e) return std::unexpected(e.error());

    DecodedUnsatProof out;
    out.proof.pigeons = *pigeons;
    std::copy(hash_bytes->begin(), hash_bytes->end(), out.proof.formula_hash.begin());
    out.discovered_by = *discovered_by;
    return out;
}

}  // namespace mathhunter::conjectures::pigeonhole
