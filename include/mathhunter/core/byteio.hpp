#pragma once

#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <expected>
#include <span>
#include <string>
#include <vector>

namespace mathhunter::core {

enum class ByteIoError : std::uint8_t {
    UnexpectedEof,
    LengthLimitExceeded,
    TrailingGarbage,
};

[[nodiscard]] constexpr std::string_view to_string(ByteIoError e) noexcept {
    switch (e) {
        case ByteIoError::UnexpectedEof:
            return "unexpected end of input";
        case ByteIoError::LengthLimitExceeded:
            return "declared length exceeds sanity limit";
        case ByteIoError::TrailingGarbage:
            return "trailing garbage after expected end of input";
    }
    return "unknown byteio error";
}

class ByteWriter final {
public:
    void put_u8(std::uint8_t v) { buf_.push_back(v); }

    void put_bool(bool v) { put_u8(v ? 1U : 0U); }

    template <std::unsigned_integral T>
    void put_le(T v) {
        const T le = (std::endian::native == std::endian::little) ? v : std::byteswap(v);
        const auto* p = reinterpret_cast<const std::uint8_t*>(&le);
        buf_.insert(buf_.end(), p, p + sizeof(T));
    }

    void put_bytes(std::span<const std::uint8_t> data) {
        buf_.insert(buf_.end(), data.begin(), data.end());
    }

    void put_length_prefixed(std::span<const std::uint8_t> data) {
        put_le<std::uint32_t>(static_cast<std::uint32_t>(data.size()));
        put_bytes(data);
    }

    void put_length_prefixed_string(std::string_view s) {
        put_length_prefixed(std::span{reinterpret_cast<const std::uint8_t*>(s.data()), s.size()});
    }

    [[nodiscard]] const std::vector<std::uint8_t>& bytes() const noexcept { return buf_; }
    [[nodiscard]] std::vector<std::uint8_t> take() && noexcept { return std::move(buf_); }

private:
    std::vector<std::uint8_t> buf_;
};

class ByteReader final {
public:
    explicit ByteReader(std::span<const std::uint8_t> data) : data_(data) {}

    [[nodiscard]] std::expected<std::uint8_t, ByteIoError> get_u8() {
        if (pos_ + 1 > data_.size()) {
            return std::unexpected(ByteIoError::UnexpectedEof);
        }
        return data_[pos_++];
    }

    [[nodiscard]] std::expected<bool, ByteIoError> get_bool() {
        auto v = get_u8();
        if (!v) return std::unexpected(v.error());
        return *v != 0;
    }

    template <std::unsigned_integral T>
    [[nodiscard]] std::expected<T, ByteIoError> get_le() {
        if (pos_ + sizeof(T) > data_.size()) {
            return std::unexpected(ByteIoError::UnexpectedEof);
        }
        T le{};
        std::memcpy(&le, data_.data() + pos_, sizeof(T));
        pos_ += sizeof(T);
        return (std::endian::native == std::endian::little) ? le : std::byteswap(le);
    }

    [[nodiscard]] std::expected<std::vector<std::uint8_t>, ByteIoError> get_bytes(std::size_t n) {
        if (pos_ + n > data_.size()) {
            return std::unexpected(ByteIoError::UnexpectedEof);
        }
        std::vector<std::uint8_t> out(data_.begin() + static_cast<std::ptrdiff_t>(pos_),
                                       data_.begin() + static_cast<std::ptrdiff_t>(pos_ + n));
        pos_ += n;
        return out;
    }

    [[nodiscard]] std::expected<std::vector<std::uint8_t>, ByteIoError> get_length_prefixed(
        std::size_t max_len) {
        auto len = get_le<std::uint32_t>();
        if (!len) return std::unexpected(len.error());
        if (*len > max_len) {
            return std::unexpected(ByteIoError::LengthLimitExceeded);
        }
        return get_bytes(*len);
    }

    [[nodiscard]] std::expected<std::string, ByteIoError> get_length_prefixed_string(
        std::size_t max_len) {
        auto bytes = get_length_prefixed(max_len);
        if (!bytes) return std::unexpected(bytes.error());
        return std::string(bytes->begin(), bytes->end());
    }

    [[nodiscard]] std::expected<void, ByteIoError> expect_end() const {
        if (pos_ != data_.size()) {
            return std::unexpected(ByteIoError::TrailingGarbage);
        }
        return {};
    }

    [[nodiscard]] std::size_t position() const noexcept { return pos_; }
    [[nodiscard]] std::size_t remaining() const noexcept { return data_.size() - pos_; }

private:
    std::span<const std::uint8_t> data_;
    std::size_t pos_{0};
};

}  // namespace mathhunter::core
