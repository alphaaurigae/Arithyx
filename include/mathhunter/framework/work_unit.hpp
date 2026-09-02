#pragma once

#include <cstdint>
#include <expected>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "mathhunter/certificate/certificate.hpp"
#include "mathhunter/core/encoded_blob.hpp"
#include "mathhunter/core/hash.hpp"

namespace mathhunter::framework {

struct WorkUnitKey final {
    std::string module_id;
    core::EncodedBlob partition;

    [[nodiscard]] core::Digest256 identity_digest() const;
};

[[nodiscard]] bool operator==(const WorkUnitKey& a, const WorkUnitKey& b) noexcept;

enum class WorkUnitKeyError : std::uint8_t {
    Truncated,
    TrailingGarbage,
    DigestMismatch,
    PayloadTooLarge,
    EmptyModuleId,
};

[[nodiscard]] constexpr std::string_view to_string(WorkUnitKeyError e) noexcept {
    switch (e) {
        case WorkUnitKeyError::Truncated:
            return "work unit key is truncated";
        case WorkUnitKeyError::TrailingGarbage:
            return "work unit key has trailing garbage after digest";
        case WorkUnitKeyError::DigestMismatch:
            return "work unit key digest does not match contents (tampered or corrupted)";
        case WorkUnitKeyError::PayloadTooLarge:
            return "work unit key partition exceeds sanity limit";
        case WorkUnitKeyError::EmptyModuleId:
            return "work unit key is missing its owning module id";
    }
    return "unknown work unit key error";
}

[[nodiscard]] std::vector<std::uint8_t> encode_work_unit_key(const WorkUnitKey& key);
[[nodiscard]] std::expected<WorkUnitKey, WorkUnitKeyError> decode_work_unit_key(
    std::span<const std::uint8_t> bytes);

struct WorkUnitCost final {
    std::uint64_t units{0};
};

enum class WorkUnitOutcomeKind : std::uint8_t {
    BoundedCompletion,
    Hit,
    Anomaly,
};

[[nodiscard]] constexpr std::string_view to_string(WorkUnitOutcomeKind k) noexcept {
    switch (k) {
        case WorkUnitOutcomeKind::BoundedCompletion:
            return "bounded_completion";
        case WorkUnitOutcomeKind::Hit:
            return "hit";
        case WorkUnitOutcomeKind::Anomaly:
            return "anomaly";
    }
    return "unknown";
}

struct WorkUnitOutcome final {
    WorkUnitOutcomeKind kind{};
    std::optional<certificate::CertificateKind> certificate_kind;
    std::vector<std::uint8_t> payload;
    std::string diagnostic;
};

}  // namespace mathhunter::framework
