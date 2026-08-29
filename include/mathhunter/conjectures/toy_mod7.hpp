#pragma once

#include <expected>
#include <span>
#include <string>
#include <variant>
#include <vector>

#include "mathhunter/core/byteio.hpp"
#include "mathhunter/core/result.hpp"

namespace mathhunter::conjectures::toy_mod7 {

inline constexpr std::string_view kAlgorithmVersion = "toy-mod7-search-1";
inline constexpr std::string_view kStatement =
    "For all n in the domain, n is not divisible by 7. "
    "This is deliberately FALSE (n=7 is a trivial counterexample); "
    "it exists solely to exercise the certificate and verifier machinery "
    "against a claim whose truth value is known with certainty.";

struct Witness final {
    std::uint64_t n{0};
};

struct Evidence final {
    std::uint64_t remainder{0};
};

using SearchOutcome = std::variant<core::BoundedSearchResult<std::uint64_t>,
                                    core::CandidateCounterexample<Witness>>;

[[nodiscard]] SearchOutcome search_range(std::uint64_t begin, std::uint64_t end);

[[nodiscard]] std::vector<std::uint8_t> encode_counterexample_payload(
    const core::CandidateCounterexample<Witness>& candidate);

struct DecodedCounterexample final {
    Witness witness;
    std::string discovered_by;
};

[[nodiscard]] std::expected<DecodedCounterexample, core::ByteIoError> decode_counterexample_payload(
    std::span<const std::uint8_t> payload);

[[nodiscard]] std::vector<std::uint8_t> encode_bounded_result_payload(
    const core::BoundedSearchResult<std::uint64_t>& result);

[[nodiscard]] std::expected<core::BoundedSearchResult<std::uint64_t>, core::ByteIoError>
decode_bounded_result_payload(std::span<const std::uint8_t> payload);

}  // namespace mathhunter::conjectures::toy_mod7
