#pragma once

#include <cstdint>
#include <string_view>

namespace mathhunter::verifier {

// Module-owned domain rejection reason for KurepaVerifier (arithmetic-only:
// primality is meaningless outside this module's domain). See
// docs/GUIDANCE.md, "Phase 1 — ABI".
enum class KurepaDomainFailure : std::uint8_t {
    NotPrime,
};

[[nodiscard]] constexpr std::string_view to_string(KurepaDomainFailure f) noexcept {
    switch (f) {
        case KurepaDomainFailure::NotPrime:
            return "witness fails the independently recomputed primality check";
    }
    return "unknown kurepa domain failure";
}

}  // namespace mathhunter::verifier
