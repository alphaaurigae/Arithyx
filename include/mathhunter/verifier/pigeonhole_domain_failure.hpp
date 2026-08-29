#pragma once

#include <cstdint>
#include <string_view>

namespace mathhunter::verifier {

// Module-owned domain rejection reasons for PigeonholeVerifier (SAT/proof
// domain: satisfiability refutation is meaningless outside this module's
// domain). See docs/GUIDANCE.md, "Phase 1 — ABI".
enum class PigeonholeDomainFailure : std::uint8_t {
    UnsatClaimRefuted,
    SatClaimRefuted,
};

[[nodiscard]] constexpr std::string_view to_string(PigeonholeDomainFailure f) noexcept {
    switch (f) {
        case PigeonholeDomainFailure::UnsatClaimRefuted:
            return "a satisfying assignment was found; UNSAT claim is false";
        case PigeonholeDomainFailure::SatClaimRefuted:
            return "claimed satisfying assignment does not satisfy the formula";
    }
    return "unknown pigeonhole domain failure";
}

}  // namespace mathhunter::verifier
