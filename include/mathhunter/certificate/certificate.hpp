#pragma once

#include <cstdint>
#include <expected>
#include <filesystem>
#include <string>
#include <vector>

#include "mathhunter/core/hash.hpp"
#include "mathhunter/core/result.hpp"

namespace mathhunter::certificate {

enum class CertificateKind : std::uint8_t {
    KurepaCounterexample = 1,
    KurepaBoundedResult = 2,
    ToyMod7Counterexample = 3,
    ToyMod7BoundedResult = 4,
    PigeonholeUnsatResult = 5,
};

[[nodiscard]] constexpr std::string_view to_string(CertificateKind kind) noexcept {
    switch (kind) {
        case CertificateKind::KurepaCounterexample:
            return "kurepa_counterexample";
        case CertificateKind::KurepaBoundedResult:
            return "kurepa_bounded_result";
        case CertificateKind::ToyMod7Counterexample:
            return "toy_mod7_counterexample";
        case CertificateKind::ToyMod7BoundedResult:
            return "toy_mod7_bounded_result";
        case CertificateKind::PigeonholeUnsatResult:
            return "pigeonhole_unsat_result";
    }
    return "unknown";
}

enum class CertificateError : std::uint8_t {
    CannotOpenFile,
    BadMagic,
    UnsupportedVersion,
    UnknownClaim,
    UnknownKind,
    PayloadTooLarge,
    Truncated,
    TrailingGarbage,
    DigestMismatch,
};

[[nodiscard]] std::string_view to_string(CertificateError err) noexcept;

struct CertificateFile final {
    std::uint16_t format_version{1};
    core::ClaimId claim{core::ClaimId::Unknown};
    CertificateKind kind{};
    std::vector<std::uint8_t> payload;
    core::Digest256 digest{};
};

[[nodiscard]] std::expected<void, CertificateError> write_certificate_file(
    const std::filesystem::path& path, core::ClaimId claim, CertificateKind kind,
    const std::vector<std::uint8_t>& payload);

[[nodiscard]] std::expected<CertificateFile, CertificateError> read_certificate_file(
    const std::filesystem::path& path);

constexpr std::size_t kMaxCertificatePayloadBytes = 16ULL * 1024 * 1024;

}  // namespace mathhunter::certificate
