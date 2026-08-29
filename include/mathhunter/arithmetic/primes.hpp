#pragma once

#include <cstdint>
#include <functional>
#include <vector>

namespace mathhunter::arithmetic {

[[nodiscard]] bool is_prime_u64(std::uint64_t n) noexcept;

[[nodiscard]] std::vector<std::uint64_t> primes_in_range(std::uint64_t begin, std::uint64_t end);

void for_each_prime_in_range(std::uint64_t begin, std::uint64_t end,
                              const std::function<bool(std::uint64_t)>& visitor);

}  // namespace mathhunter::arithmetic
