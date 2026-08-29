#pragma once

#include <concepts>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>

namespace mathhunter::verifier {
class KurepaVerifier;
class ToyMod7Verifier;
class PigeonholeVerifier;
}  // namespace mathhunter::verifier

namespace mathhunter::core {

enum class ClaimId : std::uint8_t {
    Unknown = 0,
    KurepaLeftFactorial = 1,
    ToyMod7 = 2,
    PigeonholeUnsat = 3,
};

[[nodiscard]] constexpr std::string_view claim_id_name(ClaimId id) noexcept {
    switch (id) {
        case ClaimId::KurepaLeftFactorial:
            return "kurepa";
        case ClaimId::ToyMod7:
            return "toy_mod7";
        case ClaimId::PigeonholeUnsat:
            return "pigeonhole";
        case ClaimId::Unknown:
        default:
            return "unknown";
    }
}

class VerificationToken final {
public:
    VerificationToken(const VerificationToken&) = delete;
    VerificationToken& operator=(const VerificationToken&) = delete;
    VerificationToken(VerificationToken&&) noexcept = default;
    VerificationToken& operator=(VerificationToken&&) = delete;

private:
    VerificationToken() = default;

    friend class mathhunter::verifier::KurepaVerifier;
    friend class mathhunter::verifier::ToyMod7Verifier;
    friend class mathhunter::verifier::PigeonholeVerifier;
};

template <typename Payload>
class Verified final {
public:
    Verified(Payload payload, VerificationToken /*proof_of_verification*/)
        : payload_(std::move(payload)) {}

    [[nodiscard]] const Payload& get() const& noexcept { return payload_; }
    [[nodiscard]] Payload get() && noexcept { return std::move(payload_); }

private:
    Payload payload_;
};

template <typename Witness>
struct Observation final {
    ClaimId claim{ClaimId::Unknown};
    Witness data{};
    std::string note;
};

template <typename Witness>
struct CandidateCounterexample final {
    ClaimId claim{ClaimId::Unknown};
    Witness witness{};
    std::string discovered_by;
    std::uint64_t search_seed{0};
};

template <typename Witness, typename Evidence>
struct VerifiedCounterexamplePayload final {
    ClaimId claim{ClaimId::Unknown};
    Witness witness{};
    Evidence evidence{};
};

template <typename Witness, typename Evidence>
using VerifiedCounterexample = Verified<VerifiedCounterexamplePayload<Witness, Evidence>>;

template <typename Domain>
struct BoundedSearchResult final {
    ClaimId claim{ClaimId::Unknown};
    Domain begin{};
    Domain end{};
    std::uint64_t candidates_tested{0};
    std::string algorithm_version;
};

template <typename Domain>
using VerifiedBoundedResult = Verified<BoundedSearchResult<Domain>>;

template <typename ProofData>
struct CandidateProof final {
    ClaimId claim{ClaimId::Unknown};
    ProofData proof{};
    std::string discovered_by;
};

template <typename ProofData>
using VerifiedProof = Verified<CandidateProof<ProofData>>;

}  // namespace mathhunter::core
