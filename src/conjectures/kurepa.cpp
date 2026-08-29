#include "mathhunter/conjectures/kurepa.hpp"

#include "mathhunter/arithmetic/primes.hpp"
#include "mathhunter/certificate/bigint_codec.hpp"

namespace mathhunter::conjectures::kurepa {

SearchOutcome search_range(std::uint64_t begin, std::uint64_t end) {
    std::uint64_t candidates_tested = 0;
    SearchOutcome found;
    bool has_found = false;

    arithmetic::for_each_prime_in_range(begin, end, [&](std::uint64_t p) {
        if (p == 2) {
            return true;
        }
        ++candidates_tested;
        const arithmetic::Integer modulus(p);
        const auto residue = arithmetic::left_factorial_mod(p, modulus);
        if (residue == 0) {
            core::CandidateCounterexample<Witness> candidate;
            candidate.claim = core::ClaimId::KurepaLeftFactorial;
            candidate.witness.p = arithmetic::Integer(p);
            candidate.discovered_by = std::string(kAlgorithmVersion);
            candidate.search_seed = 0;
            found = candidate;
            has_found = true;
            return false;
        }
        return true;
    });

    if (has_found) {
        return found;
    }

    core::BoundedSearchResult<arithmetic::Integer> result;
    result.claim = core::ClaimId::KurepaLeftFactorial;
    result.begin = arithmetic::Integer(begin);
    result.end = arithmetic::Integer(end);
    result.candidates_tested = candidates_tested;
    result.algorithm_version = std::string(kAlgorithmVersion);
    return result;
}

std::vector<std::uint8_t> encode_counterexample_payload(
    const core::CandidateCounterexample<Witness>& candidate) {
    core::ByteWriter w;
    certificate::put_nonneg_integer(w, candidate.witness.p);
    w.put_length_prefixed_string(candidate.discovered_by);
    return w.bytes();
}

std::expected<DecodedCounterexample, core::ByteIoError> decode_counterexample_payload(
    std::span<const std::uint8_t> payload) {
    core::ByteReader r(payload);
    auto p = certificate::get_nonneg_integer(r);
    if (!p) return std::unexpected(p.error());
    auto discovered_by = r.get_length_prefixed_string(256);
    if (!discovered_by) return std::unexpected(discovered_by.error());
    if (auto end = r.expect_end(); !end) return std::unexpected(end.error());

    DecodedCounterexample out;
    out.witness.p = *p;
    out.discovered_by = *discovered_by;
    return out;
}

std::vector<std::uint8_t> encode_bounded_result_payload(
    const core::BoundedSearchResult<arithmetic::Integer>& result) {
    core::ByteWriter w;
    certificate::put_nonneg_integer(w, result.begin);
    certificate::put_nonneg_integer(w, result.end);
    w.put_le<std::uint64_t>(result.candidates_tested);
    w.put_length_prefixed_string(result.algorithm_version);
    return w.bytes();
}

std::expected<core::BoundedSearchResult<arithmetic::Integer>, core::ByteIoError>
decode_bounded_result_payload(std::span<const std::uint8_t> payload) {
    core::ByteReader r(payload);
    auto begin = certificate::get_nonneg_integer(r);
    if (!begin) return std::unexpected(begin.error());
    auto end_val = certificate::get_nonneg_integer(r);
    if (!end_val) return std::unexpected(end_val.error());
    auto tested = r.get_le<std::uint64_t>();
    if (!tested) return std::unexpected(tested.error());
    auto algo = r.get_length_prefixed_string(256);
    if (!algo) return std::unexpected(algo.error());
    if (auto e = r.expect_end(); !e) return std::unexpected(e.error());

    core::BoundedSearchResult<arithmetic::Integer> result;
    result.claim = core::ClaimId::KurepaLeftFactorial;
    result.begin = *begin;
    result.end = *end_val;
    result.candidates_tested = *tested;
    result.algorithm_version = *algo;
    return result;
}

}  // namespace mathhunter::conjectures::kurepa
