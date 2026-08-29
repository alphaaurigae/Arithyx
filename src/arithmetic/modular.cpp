#include "mathhunter/arithmetic/modular.hpp"

#include <algorithm>
#include <array>

namespace mathhunter::arithmetic {

namespace {

constexpr std::array<unsigned long, 12> kMillerRabinWitnesses{2,  3,  5,  7,  11, 13,
                                                                17, 19, 23, 29, 31, 37};

bool trial_divide_small(const Integer& n) {
    if (n < 2) {
        return false;
    }
    for (unsigned long p : kMillerRabinWitnesses) {
        const Integer sp(p);
        if (n == sp) {
            return true;
        }
        if (n % sp == 0) {
            return false;
        }
    }
    return true;
}

bool miller_rabin_witness_composite(const Integer& n, const Integer& d, unsigned long r,
                                     unsigned long a) {
    Integer x = powmod(Integer(a), d, n);
    if (x == 1 || x == n - 1) {
        return false;
    }
    for (unsigned long i = 1; i < r; ++i) {
        x = mulmod(x, x, n);
        if (x == n - 1) {
            return false;
        }
    }
    return true;
}

}  // namespace

Integer deterministic_primality_bound() {
    static const Integer bound = [] {
        Integer b("3317044064679887385961981");
        return b;
    }();
    return bound;
}

Integer gcd(const Integer& a, const Integer& b) {
    Integer result;
    mpz_gcd(result.get_mpz_t(), a.get_mpz_t(), b.get_mpz_t());
    return result;
}

Integer mod(const Integer& a, const Integer& modulus) {
    if (modulus <= 0) {
        throw DomainError("mod: modulus must be positive");
    }
    Integer r;
    mpz_mod(r.get_mpz_t(), a.get_mpz_t(), modulus.get_mpz_t());
    return r;
}

Integer mulmod(const Integer& a, const Integer& b, const Integer& modulus) {
    if (modulus <= 0) {
        throw DomainError("mulmod: modulus must be positive");
    }
    Integer product = a * b;
    return mod(product, modulus);
}

Integer powmod(const Integer& base, const Integer& exponent, const Integer& modulus) {
    if (modulus <= 0) {
        throw DomainError("powmod: modulus must be positive");
    }
    if (exponent < 0) {
        throw DomainError("powmod: negative exponents are not supported");
    }
    Integer result;
    mpz_powm(result.get_mpz_t(), base.get_mpz_t(), exponent.get_mpz_t(), modulus.get_mpz_t());
    return result;
}

Integer factorial_mod(std::uint64_t n, const Integer& modulus) {
    if (modulus <= 0) {
        throw DomainError("factorial_mod: modulus must be positive");
    }
    Integer acc = mod(Integer(1), modulus);
    for (std::uint64_t k = 2; k <= n; ++k) {
        acc = mulmod(acc, Integer(k), modulus);
        if (acc == 0) {
            return acc;
        }
    }
    return acc;
}

Integer left_factorial_mod(std::uint64_t n, const Integer& modulus) {
    if (modulus <= 0) {
        throw DomainError("left_factorial_mod: modulus must be positive");
    }
    Integer term = mod(Integer(1), modulus);
    Integer sum = term;
    for (std::uint64_t k = 1; k < n; ++k) {
        term = mulmod(term, Integer(k), modulus);
        sum = mod(sum + term, modulus);
    }
    return sum;
}

bool is_prime_exact(const Integer& n) {
    if (n < 2) {
        return false;
    }
    if (n > deterministic_primality_bound()) {
        throw DomainError(
            "is_prime_exact: value exceeds the proven deterministic Miller-Rabin bound; "
            "refusing to fall back to probabilistic testing for a proof-critical check");
    }
    if (n <= 37) {
        return trial_divide_small(n);
    }
    if (std::ranges::any_of(kMillerRabinWitnesses,
                             [&n](unsigned long p) { return n % Integer(p) == 0; })) {
        return false;
    }

    Integer d = n - 1;
    unsigned long r = 0;
    while (d % 2 == 0) {
        d /= 2;
        ++r;
    }

    return std::ranges::none_of(kMillerRabinWitnesses, [&](unsigned long a) {
        return miller_rabin_witness_composite(n, d, r, a);
    });
}

bool is_odd(const Integer& n) { return (n % 2) != 0; }

std::optional<std::uint64_t> try_to_u64(const Integer& n) noexcept {
    if (n < 0) {
        return std::nullopt;
    }
    if (mpz_sizeinbase(n.get_mpz_t(), 2) > 64) {
        return std::nullopt;
    }
    static_assert(sizeof(unsigned long) == sizeof(std::uint64_t) ||
                      sizeof(unsigned long long) >= sizeof(std::uint64_t),
                  "platform integer width assumption for GMP export");
    std::uint64_t out = 0;
    std::size_t count = 0;
    mpz_export(&out, &count, 1, sizeof(std::uint64_t), 0, 0, n.get_mpz_t());
    return out;
}

}  // namespace mathhunter::arithmetic
