#include "mathhunter/arithmetic/primes.hpp"

#include <array>

namespace mathhunter::arithmetic {

namespace {

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#endif

std::uint64_t mulmod64(std::uint64_t a, std::uint64_t b, std::uint64_t m) noexcept {
    return static_cast<std::uint64_t>((static_cast<unsigned __int128>(a) * b) % m);
}

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic pop
#endif

std::uint64_t powmod64(std::uint64_t base, std::uint64_t exp, std::uint64_t m) noexcept {
    std::uint64_t result = 1 % m;
    base %= m;
    while (exp > 0) {
        if ((exp & 1U) != 0U) {
            result = mulmod64(result, base, m);
        }
        base = mulmod64(base, base, m);
        exp >>= 1U;
    }
    return result;
}

constexpr std::array<std::uint64_t, 12> kWitnesses{2,  3,  5,  7,  11, 13,
                                                     17, 19, 23, 29, 31, 37};

}  // namespace

bool is_prime_u64(std::uint64_t n) noexcept {
    if (n < 2) {
        return false;
    }
    for (std::uint64_t p : kWitnesses) {
        if (n == p) {
            return true;
        }
        if (n % p == 0) {
            return false;
        }
    }

    std::uint64_t d = n - 1;
    unsigned r = 0;
    while ((d & 1U) == 0U) {
        d >>= 1U;
        ++r;
    }

    for (std::uint64_t a : kWitnesses) {
        std::uint64_t x = powmod64(a, d, n);
        if (x == 1 || x == n - 1) {
            continue;
        }
        bool composite = true;
        for (unsigned i = 1; i < r; ++i) {
            x = mulmod64(x, x, n);
            if (x == n - 1) {
                composite = false;
                break;
            }
        }
        if (composite) {
            return false;
        }
    }
    return true;
}

std::vector<std::uint64_t> primes_in_range(std::uint64_t begin, std::uint64_t end) {
    std::vector<std::uint64_t> out;
    for_each_prime_in_range(begin, end, [&out](std::uint64_t p) {
        out.push_back(p);
        return true;
    });
    return out;
}

void for_each_prime_in_range(std::uint64_t begin, std::uint64_t end,
                              const std::function<bool(std::uint64_t)>& visitor) {
    if (begin < 2) {
        begin = 2;
    }
    for (std::uint64_t candidate = begin; candidate < end; ++candidate) {
        if (is_prime_u64(candidate)) {
            if (!visitor(candidate)) {
                return;
            }
        }
    }
}

void for_each_prime_in_range(const Integer& begin, const Integer& end,
                              const std::function<bool(const Integer&)>& visitor) {
    Integer candidate = begin < 2 ? Integer(2) : begin;
    for (; candidate < end; ++candidate) {
        if (is_prime_exact(candidate)) {
            if (!visitor(candidate)) {
                return;
            }
        }
    }
}


}  // namespace mathhunter::arithmetic
