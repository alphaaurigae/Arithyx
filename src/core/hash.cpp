#include "mathhunter/core/hash.hpp"

#include <openssl/evp.h>

#include <bit>
#include <stdexcept>

namespace mathhunter::core {

struct Sha256Stream::Impl {
    EVP_MD_CTX* ctx{EVP_MD_CTX_new()};
};

Sha256Stream::Sha256Stream() : impl_(new Impl()) {
    if (impl_->ctx == nullptr) {
        delete impl_;
        throw std::runtime_error("EVP_MD_CTX_new failed");
    }
    if (EVP_DigestInit_ex(impl_->ctx, EVP_sha256(), nullptr) != 1) {
        EVP_MD_CTX_free(impl_->ctx);
        delete impl_;
        throw std::runtime_error("EVP_DigestInit_ex failed");
    }
}

Sha256Stream::~Sha256Stream() {
    if (impl_ != nullptr) {
        EVP_MD_CTX_free(impl_->ctx);
        delete impl_;
    }
}

void Sha256Stream::update(std::span<const std::byte> data) {
    if (!data.empty()) {
        if (EVP_DigestUpdate(impl_->ctx, data.data(), data.size()) != 1) {
            throw std::runtime_error("EVP_DigestUpdate failed");
        }
    }
}

void Sha256Stream::update(std::string_view text) {
    update(std::as_bytes(std::span{text.data(), text.size()}));
}

void Sha256Stream::update_u64_le(std::uint64_t value) {
    std::array<std::byte, sizeof(std::uint64_t)> buf{};
    const auto le_value = std::endian::native == std::endian::little
                               ? value
                               : std::byteswap(value);
    __builtin_memcpy(buf.data(), &le_value, sizeof(le_value));
    update(std::span<const std::byte>{buf});
}

Digest256 Sha256Stream::finalize() {
    Digest256 out{};
    unsigned int len = 0;
    if (EVP_DigestFinal_ex(impl_->ctx, out.data(), &len) != 1 || len != out.size()) {
        throw std::runtime_error("EVP_DigestFinal_ex failed");
    }
    return out;
}

Digest256 sha256(std::span<const std::byte> data) {
    Sha256Stream s;
    s.update(data);
    return s.finalize();
}

Digest256 sha256(std::string_view text) {
    Sha256Stream s;
    s.update(text);
    return s.finalize();
}

std::string to_hex(const Digest256& digest) {
    constexpr std::string_view kHexDigits = "0123456789abcdef";
    std::string out;
    out.resize(digest.size() * 2);
    for (std::size_t i = 0; i < digest.size(); ++i) {
        out[2 * i] = kHexDigits[digest[i] >> 4];
        out[2 * i + 1] = kHexDigits[digest[i] & 0x0F];
    }
    return out;
}

bool constant_time_equal(const Digest256& a, const Digest256& b) noexcept {
    std::uint8_t diff = 0;
    for (std::size_t i = 0; i < a.size(); ++i) {
        diff = static_cast<std::uint8_t>(diff | (a[i] ^ b[i]));
    }
    return diff == 0;
}

}  // namespace mathhunter::core
