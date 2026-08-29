#pragma once

#include <expected>

#include "mathhunter/conjectures/pigeonhole.hpp"
#include "mathhunter/verifier/verification_error.hpp"

namespace mathhunter::verifier {

class PigeonholeVerifier final {
public:
    using VerifiedProof = core::VerifiedProof<conjectures::pigeonhole::UnsatProof>;

    static constexpr std::uint32_t kMaxBruteForcePigeons = 6;

    [[nodiscard]] std::expected<VerifiedProof, VerificationFailure> verify_unsat(
        const conjectures::pigeonhole::UnsatProof& claimed) const;
};

}  // namespace mathhunter::verifier
