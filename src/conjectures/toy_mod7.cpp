#include "mathhunter/conjectures/toy_mod7.hpp"

namespace mathhunter::conjectures::toy_mod7 {

SearchOutcome search_range(std::uint64_t begin, std::uint64_t end) {
    std::uint64_t candidates_tested = 0;
    for (std::uint64_t n = begin; n < end; ++n) {
        ++candidates_tested;
        if (n % 7 == 0) {
            core::CandidateCounterexample<Witness> candidate;
            candidate.claim = core::ClaimId::ToyMod7;
            candidate.witness.n = n;
            candidate.discovered_by = std::string(kAlgorithmVersion);
            candidate.search_seed = 0;
            return candidate;
        }
    }
    core::BoundedSearchResult<std::uint64_t> result;
    result.claim = core::ClaimId::ToyMod7;
    result.begin = begin;
    result.end = end;
    result.candidates_tested = candidates_tested;
    result.algorithm_version = std::string(kAlgorithmVersion);
    return result;
}

std::vector<std::uint8_t> encode_counterexample_payload(
    const core::CandidateCounterexample<Witness>& candidate) {
    core::ByteWriter w;
    w.put_le<std::uint64_t>(candidate.witness.n);
    w.put_length_prefixed_string(candidate.discovered_by);
    return w.bytes();
}

std::expected<DecodedCounterexample, core::ByteIoError> decode_counterexample_payload(
    std::span<const std::uint8_t> payload) {
    core::ByteReader r(payload);
    auto n = r.get_le<std::uint64_t>();
    if (!n) return std::unexpected(n.error());
    auto discovered_by = r.get_length_prefixed_string(256);
    if (!discovered_by) return std::unexpected(discovered_by.error());
    if (auto end = r.expect_end(); !end) return std::unexpected(end.error());

    DecodedCounterexample out;
    out.witness.n = *n;
    out.discovered_by = *discovered_by;
    return out;
}

std::vector<std::uint8_t> encode_bounded_result_payload(
    const core::BoundedSearchResult<std::uint64_t>& result) {
    core::ByteWriter w;
    w.put_le<std::uint64_t>(result.begin);
    w.put_le<std::uint64_t>(result.end);
    w.put_le<std::uint64_t>(result.candidates_tested);
    w.put_length_prefixed_string(result.algorithm_version);
    return w.bytes();
}

std::expected<core::BoundedSearchResult<std::uint64_t>, core::ByteIoError>
decode_bounded_result_payload(std::span<const std::uint8_t> payload) {
    core::ByteReader r(payload);
    auto begin = r.get_le<std::uint64_t>();
    if (!begin) return std::unexpected(begin.error());
    auto end_val = r.get_le<std::uint64_t>();
    if (!end_val) return std::unexpected(end_val.error());
    auto tested = r.get_le<std::uint64_t>();
    if (!tested) return std::unexpected(tested.error());
    auto algo = r.get_length_prefixed_string(256);
    if (!algo) return std::unexpected(algo.error());
    if (auto e = r.expect_end(); !e) return std::unexpected(e.error());

    core::BoundedSearchResult<std::uint64_t> result;
    result.claim = core::ClaimId::ToyMod7;
    result.begin = *begin;
    result.end = *end_val;
    result.candidates_tested = *tested;
    result.algorithm_version = *algo;
    return result;
}

}  // namespace mathhunter::conjectures::toy_mod7
