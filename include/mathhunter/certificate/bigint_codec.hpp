#pragma once

#include "mathhunter/arithmetic/modular.hpp"
#include "mathhunter/core/byteio.hpp"

namespace mathhunter::certificate {

constexpr std::size_t kMaxWitnessBytes = 4096;

void put_nonneg_integer(core::ByteWriter& writer, const arithmetic::Integer& value);

[[nodiscard]] std::expected<arithmetic::Integer, core::ByteIoError> get_nonneg_integer(
    core::ByteReader& reader, std::size_t max_bytes = kMaxWitnessBytes);

}  // namespace mathhunter::certificate
