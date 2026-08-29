#pragma once

#include <cstdint>
#include <expected>
#include <span>
#include <string>
#include <variant>
#include <vector>

#include "mathhunter/core/byteio.hpp"
#include "mathhunter/core/result.hpp"
#include "mathhunter/sat/cnf.hpp"

namespace mathhunter::conjectures::pigeonhole {

inline constexpr std::string_view kAlgorithmVersion = "pigeonhole-dpll-1";
inline constexpr std::string_view kStatement =
    "PHP(n): n pigeons cannot be placed into n-1 holes with no hole holding two pigeons. "
    "This is a KNOWN, PROVEN result (not an open conjecture); it is retained here as a "
    "known-result benchmark for validating the SAT encoding/solver/verifier pipeline, and "
    "as the classical example of a family requiring exponential resolution proof size.";

[[nodiscard]] sat::Cnf encode_php(std::uint32_t pigeons, std::uint32_t holes);

[[nodiscard]] std::uint64_t expected_var_count(std::uint32_t pigeons, std::uint32_t holes);
[[nodiscard]] std::uint64_t expected_clause_count(std::uint32_t pigeons, std::uint32_t holes);

struct UnsatProof final {
    std::uint32_t pigeons{0};
    core::Digest256 formula_hash{};
};

using SearchOutcome = std::variant<core::CandidateProof<UnsatProof>, core::Observation<std::uint32_t>>;

[[nodiscard]] SearchOutcome search_unsat_benchmark(std::uint32_t pigeons);

[[nodiscard]] std::vector<std::uint8_t> encode_unsat_proof_payload(
    const core::CandidateProof<UnsatProof>& candidate);

struct DecodedUnsatProof final {
    UnsatProof proof;
    std::string discovered_by;
};

[[nodiscard]] std::expected<DecodedUnsatProof, core::ByteIoError> decode_unsat_proof_payload(
    std::span<const std::uint8_t> payload);

}  // namespace mathhunter::conjectures::pigeonhole
