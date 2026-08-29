#pragma once

#include <cstdint>
#include <string_view>

namespace mathhunter::verifier {

enum class VerificationFailure : std::uint8_t {
    WitnessOutOfDomain,
    NotPrime,
    ClaimDoesNotHold,
    RangeMismatch,
    RangeIncomplete,
    HashMismatch,
    MalformedCertificate,
    EncodingMismatch,
    UnsatClaimRefuted,
    SatClaimRefuted,
    DomainTooLargeForVerifier,
};

[[nodiscard]] constexpr std::string_view to_string(VerificationFailure f) noexcept {
    switch (f) {
        case VerificationFailure::WitnessOutOfDomain:
            return "witness is not in the domain required by the claim";
        case VerificationFailure::NotPrime:
            return "witness fails the independently recomputed primality check";
        case VerificationFailure::ClaimDoesNotHold:
            return "recomputed value does not establish the claimed mathematical relation";
        case VerificationFailure::RangeMismatch:
            return "certificate range does not match requested/expected range";
        case VerificationFailure::RangeIncomplete:
            return "search range was not exhaustively covered";
        case VerificationFailure::HashMismatch:
            return "recomputed hash does not match certificate hash";
        case VerificationFailure::MalformedCertificate:
            return "certificate payload is malformed or fails bounds checks";
        case VerificationFailure::EncodingMismatch:
            return "independently rebuilt encoding does not match certificate's recorded encoding";
        case VerificationFailure::UnsatClaimRefuted:
            return "a satisfying assignment was found; UNSAT claim is false";
        case VerificationFailure::SatClaimRefuted:
            return "claimed satisfying assignment does not satisfy the formula";
        case VerificationFailure::DomainTooLargeForVerifier:
            return "instance exceeds this prototype verifier's brute-force capacity; "
                   "a DRAT/LRAT-checking verifier is required beyond this scale";
    }
    return "unknown verification failure";
}

}  // namespace mathhunter::verifier
