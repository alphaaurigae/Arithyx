#include "mathhunter/certificate/certificate.hpp"

#include <array>
#include <fstream>

#include "mathhunter/core/byteio.hpp"

namespace mathhunter::certificate {

namespace {

constexpr std::array<std::uint8_t, 8> kMagic{'M', 'H', 'C', 'E', 'R', 'T', 0x00, 0x01};
constexpr std::uint16_t kFormatVersion = 1;

[[nodiscard]] bool is_known_claim(core::ClaimId claim) noexcept {
    switch (claim) {
        case core::ClaimId::KurepaLeftFactorial:
        case core::ClaimId::ToyMod7:
        case core::ClaimId::PigeonholeUnsat:
            return true;
        case core::ClaimId::Unknown:
        default:
            return false;
    }
}

[[nodiscard]] bool is_known_kind(CertificateKind kind) noexcept {
    switch (kind) {
        case CertificateKind::KurepaCounterexample:
        case CertificateKind::KurepaBoundedResult:
        case CertificateKind::ToyMod7Counterexample:
        case CertificateKind::ToyMod7BoundedResult:
        case CertificateKind::PigeonholeUnsatResult:
            return true;
    }
    return false;
}

}  // namespace

std::string_view to_string(CertificateError err) noexcept {
    switch (err) {
        case CertificateError::CannotOpenFile:
            return "cannot open certificate file";
        case CertificateError::BadMagic:
            return "bad magic bytes: not a math-hunter certificate";
        case CertificateError::UnsupportedVersion:
            return "unsupported certificate format version";
        case CertificateError::UnknownClaim:
            return "unknown or unset claim identifier";
        case CertificateError::UnknownKind:
            return "unknown certificate kind";
        case CertificateError::PayloadTooLarge:
            return "certificate payload exceeds sanity limit";
        case CertificateError::Truncated:
            return "certificate file is truncated";
        case CertificateError::TrailingGarbage:
            return "certificate file has trailing garbage after digest";
        case CertificateError::DigestMismatch:
            return "certificate digest does not match contents (tampered or corrupted)";
    }
    return "unknown certificate error";
}

std::expected<void, CertificateError> write_certificate_file(
    const std::filesystem::path& path, core::ClaimId claim, CertificateKind kind,
    const std::vector<std::uint8_t>& payload) {
    if (payload.size() > kMaxCertificatePayloadBytes) {
        return std::unexpected(CertificateError::PayloadTooLarge);
    }

    core::ByteWriter w;
    w.put_bytes(std::span<const std::uint8_t>{kMagic});
    w.put_le<std::uint16_t>(kFormatVersion);
    w.put_le<std::uint32_t>(static_cast<std::uint32_t>(claim));
    w.put_u8(static_cast<std::uint8_t>(kind));
    w.put_length_prefixed(payload);

    const auto prefix_digest = core::sha256(std::as_bytes(std::span{w.bytes()}));
    w.put_bytes(std::span<const std::uint8_t>{prefix_digest});

    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out) {
        return std::unexpected(CertificateError::CannotOpenFile);
    }
    const auto bytes = std::move(w).take();
    out.write(reinterpret_cast<const char*>(bytes.data()),
              static_cast<std::streamsize>(bytes.size()));
    if (!out.good()) {
        return std::unexpected(CertificateError::CannotOpenFile);
    }
    return {};
}

std::expected<CertificateFile, CertificateError> read_certificate_file(
    const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        return std::unexpected(CertificateError::CannotOpenFile);
    }
    std::vector<std::uint8_t> raw((std::istreambuf_iterator<char>(in)),
                                   std::istreambuf_iterator<char>());
    if (!in.good() && !in.eof()) {
        return std::unexpected(CertificateError::CannotOpenFile);
    }

    core::ByteReader r(raw);

    auto magic = r.get_bytes(kMagic.size());
    if (!magic) return std::unexpected(CertificateError::Truncated);
    if (!std::equal(magic->begin(), magic->end(), kMagic.begin())) {
        return std::unexpected(CertificateError::BadMagic);
    }

    auto version = r.get_le<std::uint16_t>();
    if (!version) return std::unexpected(CertificateError::Truncated);
    if (*version != kFormatVersion) {
        return std::unexpected(CertificateError::UnsupportedVersion);
    }

    auto claim_raw = r.get_le<std::uint32_t>();
    if (!claim_raw) return std::unexpected(CertificateError::Truncated);
    const auto claim = static_cast<core::ClaimId>(*claim_raw);
    if (!is_known_claim(claim)) {
        return std::unexpected(CertificateError::UnknownClaim);
    }

    auto kind_raw = r.get_u8();
    if (!kind_raw) return std::unexpected(CertificateError::Truncated);
    const auto kind = static_cast<CertificateKind>(*kind_raw);
    if (!is_known_kind(kind)) {
        return std::unexpected(CertificateError::UnknownKind);
    }

    auto payload = r.get_length_prefixed(kMaxCertificatePayloadBytes);
    if (!payload) {
        return std::unexpected(payload.error() == core::ByteIoError::LengthLimitExceeded
                                    ? CertificateError::PayloadTooLarge
                                    : CertificateError::Truncated);
    }

    const std::size_t prefix_len = r.position();

    auto digest_bytes = r.get_bytes(core::Digest256{}.size());
    if (!digest_bytes) return std::unexpected(CertificateError::Truncated);

    if (!r.expect_end()) {
        return std::unexpected(CertificateError::TrailingGarbage);
    }

    core::Digest256 stored_digest{};
    std::copy(digest_bytes->begin(), digest_bytes->end(), stored_digest.begin());

    const auto recomputed =
        core::sha256(std::as_bytes(std::span{raw.data(), prefix_len}));
    if (!core::constant_time_equal(recomputed, stored_digest)) {
        return std::unexpected(CertificateError::DigestMismatch);
    }

    CertificateFile file;
    file.format_version = *version;
    file.claim = claim;
    file.kind = kind;
    file.payload = std::move(*payload);
    file.digest = stored_digest;
    return file;
}

}  // namespace mathhunter::certificate
