#pragma once

#include <expected>
#include <string>
#include <variant>
#include <vector>

#include "mathhunter/arithmetic/modular.hpp"
#include "mathhunter/core/byteio.hpp"
#include "mathhunter/core/result.hpp"

namespace mathhunter::conjectures::kurepa {

inline constexpr std::string_view kAlgorithmVersion = "kurepa-search-1";

struct Witness final {
    arithmetic::Integer p;
};

struct Evidence final {
    arithmetic::Integer residue;
};

using SearchOutcome = std::variant<core::BoundedSearchResult<arithmetic::Integer>,
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
    const core::BoundedSearchResult<arithmetic::Integer>& result);

[[nodiscard]] std::expected<core::BoundedSearchResult<arithmetic::Integer>, core::ByteIoError>
decode_bounded_result_payload(std::span<const std::uint8_t> payload);

}  // namespace mathhunter::conjectures::kurepa
