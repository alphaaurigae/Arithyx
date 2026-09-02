#pragma once

#include <cstdint>
#include <expected>
#include <span>
#include <string_view>
#include <vector>

#include "mathhunter/core/hash.hpp"

namespace mathhunter::core {

struct EncodedBlob final {
    std::uint16_t schema_version{1};
    std::vector<std::uint8_t> bytes;
};

[[nodiscard]] bool operator==(const EncodedBlob& a, const EncodedBlob& b) noexcept;

enum class EncodedBlobError : std::uint8_t {
    Truncated,
    TrailingGarbage,
    DigestMismatch,
    PayloadTooLarge,
};

[[nodiscard]] constexpr std::string_view to_string(EncodedBlobError e) noexcept {
    switch (e) {
        case EncodedBlobError::Truncated:
            return "encoded blob is truncated";
        case EncodedBlobError::TrailingGarbage:
            return "encoded blob has trailing garbage after digest";
        case EncodedBlobError::DigestMismatch:
            return "encoded blob digest does not match contents (tampered or corrupted)";
        case EncodedBlobError::PayloadTooLarge:
            return "encoded blob payload exceeds sanity limit";
    }
    return "unknown encoded blob error";
}

inline constexpr std::size_t kMaxEncodedBlobBytes = 16ULL * 1024 * 1024;

[[nodiscard]] std::vector<std::uint8_t> frame_encoded_blob(std::span<const std::uint8_t> header,
                                                             const EncodedBlob& blob);

struct UnframedBlob final {
    EncodedBlob blob;
    Digest256 digest{};
};

[[nodiscard]] std::expected<UnframedBlob, EncodedBlobError> unframe_encoded_blob(
    std::span<const std::uint8_t> header, std::span<const std::uint8_t> framed,
    std::size_t max_bytes = kMaxEncodedBlobBytes);

}  // namespace mathhunter::core
