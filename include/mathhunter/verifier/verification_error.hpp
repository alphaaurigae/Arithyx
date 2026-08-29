#pragma once

#include <cstdint>
#include <string_view>
#include <variant>

namespace mathhunter::verifier {

// Framework-owned, shape-level rejection reasons (docs/GUIDANCE.md, "Phase
// 1 — ABI", "VerificationFailure already fails the Phase-2 test today").
// Every value here has to make sense for *any* verifier regardless of
// domain: it is about the shape of the claim (a range wasn't fully
// covered, a hash didn't match, a witness didn't even satisfy the claim's
// structural domain) never about what the domain itself means. A module
// with its own domain-specific rejection reasons (KurepaDomainFailure,
// PigeonholeDomainFailure, ...) does not edit this enum; it defines its
// own and combines it with this one via VerificationOutcome below.
enum class VerificationFailure : std::uint8_t {
    WitnessOutOfDomain,
    ClaimDoesNotHold,
    RangeMismatch,
    RangeIncomplete,
    HashMismatch,
    MalformedCertificate,
    EncodingMismatch,
    DomainTooLargeForVerifier,
};

[[nodiscard]] constexpr std::string_view to_string(VerificationFailure f) noexcept {
    switch (f) {
        case VerificationFailure::WitnessOutOfDomain:
            return "witness is not in the domain required by the claim";
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
        case VerificationFailure::DomainTooLargeForVerifier:
            return "instance exceeds this prototype verifier's brute-force capacity; "
                   "a DRAT/LRAT-checking verifier is required beyond this scale";
    }
    return "unknown verification failure";
}

// A verifier's actual error type: either one of the framework-owned shape
// failures above, or one of its own module-owned ModuleFailure values.
// Modules with no domain-specific rejection reasons at all (ToyMod7Verifier
// today) simply don't use this wrapper and return VerificationFailure
// directly — shape-optionality, not every module needs a domain enum.
template <typename ModuleFailure>
class VerificationOutcome final {
public:
    constexpr VerificationOutcome(VerificationFailure f) noexcept : value_(f) {}
    constexpr VerificationOutcome(ModuleFailure f) noexcept : value_(f) {}

    [[nodiscard]] friend constexpr bool operator==(const VerificationOutcome& lhs,
                                                     const VerificationOutcome& rhs) noexcept {
        return lhs.value_ == rhs.value_;
    }
    [[nodiscard]] friend constexpr bool operator==(const VerificationOutcome& lhs,
                                                     VerificationFailure rhs) noexcept {
        return std::holds_alternative<VerificationFailure>(lhs.value_) &&
               std::get<VerificationFailure>(lhs.value_) == rhs;
    }
    [[nodiscard]] friend constexpr bool operator==(const VerificationOutcome& lhs,
                                                     ModuleFailure rhs) noexcept {
        return std::holds_alternative<ModuleFailure>(lhs.value_) &&
               std::get<ModuleFailure>(lhs.value_) == rhs;
    }

    [[nodiscard]] constexpr bool is_domain_failure() const noexcept {
        return std::holds_alternative<ModuleFailure>(value_);
    }
    [[nodiscard]] constexpr ModuleFailure domain_failure() const {
        return std::get<ModuleFailure>(value_);
    }
    [[nodiscard]] constexpr VerificationFailure shape_failure() const {
        return std::get<VerificationFailure>(value_);
    }

    [[nodiscard]] std::string_view message() const noexcept {
        return std::visit([](const auto& f) -> std::string_view { return to_string(f); }, value_);
    }

private:
    std::variant<VerificationFailure, ModuleFailure> value_;
};

template <typename ModuleFailure>
[[nodiscard]] std::string_view to_string(const VerificationOutcome<ModuleFailure>& outcome) noexcept {
    return outcome.message();
}

}  // namespace mathhunter::verifier
