#include "mathhunter/certificate/bigint_codec.hpp"

#include <stdexcept>
#include <vector>

namespace mathhunter::certificate {

void put_nonneg_integer(core::ByteWriter& writer, const arithmetic::Integer& value) {
    if (value < 0) {
        throw std::domain_error("put_nonneg_integer: certificate integers must be non-negative");
    }
    if (value == 0) {
        writer.put_length_prefixed(std::span<const std::uint8_t>{});
        return;
    }
    std::size_t count = 0;
    const std::size_t bits = mpz_sizeinbase(value.get_mpz_t(), 2);
    const std::size_t byte_capacity = (bits + 7) / 8;
    std::vector<std::uint8_t> buf(byte_capacity);
    mpz_export(buf.data(), &count, 1, 1, 1, 0, value.get_mpz_t());
    buf.resize(count);
    writer.put_length_prefixed(buf);
}

std::expected<arithmetic::Integer, core::ByteIoError> get_nonneg_integer(core::ByteReader& reader,
                                                                          std::size_t max_bytes) {
    auto bytes = reader.get_length_prefixed(max_bytes);
    if (!bytes) {
        return std::unexpected(bytes.error());
    }
    arithmetic::Integer value{0};
    if (!bytes->empty()) {
        mpz_import(value.get_mpz_t(), bytes->size(), 1, 1, 1, 0, bytes->data());
    }
    return value;
}

}  // namespace mathhunter::certificate
