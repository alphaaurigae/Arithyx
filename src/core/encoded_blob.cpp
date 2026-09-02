#include "mathhunter/core/encoded_blob.hpp"

#include <algorithm>

#include "mathhunter/core/byteio.hpp"

namespace mathhunter::core {

bool operator==(const EncodedBlob& a, const EncodedBlob& b) noexcept {
    return a.schema_version == b.schema_version && a.bytes == b.bytes;
}

std::vector<std::uint8_t> frame_encoded_blob(std::span<const std::uint8_t> header,
                                              const EncodedBlob& blob) {
    ByteWriter w;
    w.put_bytes(header);
    w.put_le<std::uint16_t>(blob.schema_version);
    w.put_length_prefixed(blob.bytes);

    const auto digest = sha256(std::as_bytes(std::span{w.bytes()}));
    w.put_bytes(std::span<const std::uint8_t>{digest});
    return std::move(w).take();
}

std::expected<UnframedBlob, EncodedBlobError> unframe_encoded_blob(
    std::span<const std::uint8_t> header, std::span<const std::uint8_t> framed,
    std::size_t max_bytes) {
    if (framed.size() < header.size() ||
        !std::equal(header.begin(), header.end(), framed.begin())) {
        return std::unexpected(EncodedBlobError::Truncated);
    }

    ByteReader r(framed);
    if (auto skipped = r.get_bytes(header.size()); !skipped) {
        return std::unexpected(EncodedBlobError::Truncated);
    }

    auto schema_version = r.get_le<std::uint16_t>();
    if (!schema_version) {
        return std::unexpected(EncodedBlobError::Truncated);
    }

    auto blob_bytes = r.get_length_prefixed(max_bytes);
    if (!blob_bytes) {
        return std::unexpected(blob_bytes.error() == ByteIoError::LengthLimitExceeded
                                    ? EncodedBlobError::PayloadTooLarge
                                    : EncodedBlobError::Truncated);
    }

    const std::size_t prefix_len = r.position();

    auto digest_bytes = r.get_bytes(Digest256{}.size());
    if (!digest_bytes) {
        return std::unexpected(EncodedBlobError::Truncated);
    }
    if (!r.expect_end()) {
        return std::unexpected(EncodedBlobError::TrailingGarbage);
    }

    Digest256 stored_digest{};
    std::copy(digest_bytes->begin(), digest_bytes->end(), stored_digest.begin());

    const auto recomputed = sha256(std::as_bytes(std::span{framed.data(), prefix_len}));
    if (!constant_time_equal(recomputed, stored_digest)) {
        return std::unexpected(EncodedBlobError::DigestMismatch);
    }

    UnframedBlob out;
    out.blob.schema_version = *schema_version;
    out.blob.bytes = std::move(*blob_bytes);
    out.digest = stored_digest;
    return out;
}

}  // namespace mathhunter::core
