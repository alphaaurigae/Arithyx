#pragma once

#include <cstdint>
#include <utility>
#include <vector>

#include "mathhunter/arithmetic/modular.hpp"

namespace mathhunter::framework {

[[nodiscard]] std::vector<std::pair<std::uint64_t, std::uint64_t>> partition_u64_range(
    std::uint64_t begin, std::uint64_t end, std::uint64_t target_size);

[[nodiscard]] std::vector<std::pair<arithmetic::Integer, arithmetic::Integer>>
partition_integer_range(const arithmetic::Integer& begin, const arithmetic::Integer& end,
                         std::uint64_t target_size);

[[nodiscard]] std::uint64_t saturating_range_size_u64(std::uint64_t begin, std::uint64_t end);
[[nodiscard]] std::uint64_t saturating_range_size_u64(const arithmetic::Integer& begin,
                                                       const arithmetic::Integer& end);

}  // namespace mathhunter::framework
