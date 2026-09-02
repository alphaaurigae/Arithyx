#include "mathhunter/framework/work_unit.hpp"

#include "mathhunter/core/byteio.hpp"

namespace mathhunter::framework {

namespace {

[[nodiscard]] std::vector<std::uint8_t> module_id_header(std::string_view module_id) {
    core::ByteWriter w;
    w.put_length_prefixed_string(module_id);
    return w.bytes();
}

}  // namespace

bool operator==(const WorkUnitKey& a, const WorkUnitKey& b) noexcept {
    return a.module_id == b.module_id && a.partition == b.partition;
}

core::Digest256 WorkUnitKey::identity_digest() const {
    const auto encoded = encode_work_unit_key(*this);
    return core::sha256(std::as_bytes(std::span{encoded}));
}

std::vector<std::uint8_t> encode_work_unit_key(const WorkUnitKey& key) {
    return core::frame_encoded_blob(module_id_header(key.module_id), key.partition);
}

std::expected<WorkUnitKey, WorkUnitKeyError> decode_work_unit_key(
    std::span<const std::uint8_t> bytes) {
    core::ByteReader r(bytes);
    auto module_id = r.get_length_prefixed_string(256);
    if (!module_id) {
        return std::unexpected(WorkUnitKeyError::Truncated);
    }
    if (module_id->empty()) {
        return std::unexpected(WorkUnitKeyError::EmptyModuleId);
    }

    const auto header = module_id_header(*module_id);
    auto unframed = core::unframe_encoded_blob(header, bytes, core::kMaxEncodedBlobBytes);
    if (!unframed) {
        switch (unframed.error()) {
            case core::EncodedBlobError::Truncated:
                return std::unexpected(WorkUnitKeyError::Truncated);
            case core::EncodedBlobError::TrailingGarbage:
                return std::unexpected(WorkUnitKeyError::TrailingGarbage);
            case core::EncodedBlobError::DigestMismatch:
                return std::unexpected(WorkUnitKeyError::DigestMismatch);
            case core::EncodedBlobError::PayloadTooLarge:
                return std::unexpected(WorkUnitKeyError::PayloadTooLarge);
        }
        return std::unexpected(WorkUnitKeyError::Truncated);
    }

    WorkUnitKey key;
    key.module_id = *module_id;
    key.partition = std::move(unframed->blob);
    return key;
}

}  // namespace mathhunter::framework
