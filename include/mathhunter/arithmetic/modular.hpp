#pragma once

#include <gmpxx.h>

#include <cstdint>
#include <optional>
#include <stdexcept>
#include <vector>

namespace mathhunter::arithmetic {

using Integer = mpz_class;

[[nodiscard]] Integer deterministic_primality_bound();

class DomainError final : public std::domain_error {
public:
    using std::domain_error::domain_error;
};

[[nodiscard]] Integer gcd(const Integer& a, const Integer& b);

[[nodiscard]] Integer mod(const Integer& a, const Integer& modulus);

[[nodiscard]] Integer mulmod(const Integer& a, const Integer& b, const Integer& modulus);

[[nodiscard]] Integer powmod(const Integer& base, const Integer& exponent, const Integer& modulus);

[[nodiscard]] Integer factorial_mod(std::uint64_t n, const Integer& modulus);

[[nodiscard]] Integer left_factorial_mod(const Integer& n, const Integer& modulus);

[[nodiscard]] bool is_prime_exact(const Integer& n);

[[nodiscard]] bool is_odd(const Integer& n);

[[nodiscard]] std::optional<std::uint64_t> try_to_u64(const Integer& n) noexcept;

}  // namespace mathhunter::arithmetic
