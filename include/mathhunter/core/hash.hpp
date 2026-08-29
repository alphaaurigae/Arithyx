#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>

namespace mathhunter::core {

using Digest256 = std::array<std::uint8_t, 32>;

class Sha256Stream final {
public:
    Sha256Stream();
    Sha256Stream(const Sha256Stream&) = delete;
    Sha256Stream& operator=(const Sha256Stream&) = delete;
    Sha256Stream(Sha256Stream&&) = delete;
    Sha256Stream& operator=(Sha256Stream&&) = delete;
    ~Sha256Stream();

    void update(std::span<const std::byte> data);
    void update(std::string_view text);
    void update_u64_le(std::uint64_t value);

    [[nodiscard]] Digest256 finalize();

private:
    struct Impl;
    Impl* impl_;
};

[[nodiscard]] Digest256 sha256(std::span<const std::byte> data);
[[nodiscard]] Digest256 sha256(std::string_view text);

[[nodiscard]] std::string to_hex(const Digest256& digest);
[[nodiscard]] bool constant_time_equal(const Digest256& a, const Digest256& b) noexcept;

}  // namespace mathhunter::core
