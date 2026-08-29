#include "mathhunter/arithmetic/modular.hpp"
#include "mathhunter/arithmetic/primes.hpp"
#include "test_harness.hpp"

using mathhunter::arithmetic::Integer;

int main() {
    MH_CHECK(mathhunter::arithmetic::gcd(Integer(12), Integer(18)) == Integer(6));
    MH_CHECK(mathhunter::arithmetic::gcd(Integer(17), Integer(5)) == Integer(1));

    MH_CHECK(mathhunter::arithmetic::mulmod(Integer(7), Integer(8), Integer(10)) == Integer(6));
    MH_CHECK(mathhunter::arithmetic::powmod(Integer(2), Integer(10), Integer(1000)) ==
              Integer(24));

    MH_CHECK(mathhunter::arithmetic::factorial_mod(5, Integer(1000)) == Integer(120));
    MH_CHECK(mathhunter::arithmetic::factorial_mod(0, Integer(1000)) == Integer(1));
    MH_CHECK(mathhunter::arithmetic::factorial_mod(1, Integer(1000)) == Integer(1));

    MH_CHECK(mathhunter::arithmetic::left_factorial_mod(1, Integer(1000)) == Integer(1));
    MH_CHECK(mathhunter::arithmetic::left_factorial_mod(2, Integer(1000)) == Integer(2));
    MH_CHECK(mathhunter::arithmetic::left_factorial_mod(3, Integer(1000)) == Integer(4));
    MH_CHECK(mathhunter::arithmetic::left_factorial_mod(4, Integer(1000)) == Integer(10));
    MH_CHECK(mathhunter::arithmetic::left_factorial_mod(5, Integer(1000)) == Integer(34));

    for (int p : {2, 3, 5, 7, 11, 13, 97, 7919}) {
        MH_CHECK(mathhunter::arithmetic::is_prime_exact(Integer(p)));
        MH_CHECK(mathhunter::arithmetic::is_prime_u64(static_cast<std::uint64_t>(p)));
    }
    for (int c : {0, 1, 4, 6, 9, 100, 7920}) {
        MH_CHECK(!mathhunter::arithmetic::is_prime_exact(Integer(c)));
        MH_CHECK(!mathhunter::arithmetic::is_prime_u64(static_cast<std::uint64_t>(c)));
    }

    MH_CHECK(!mathhunter::arithmetic::is_odd(Integer(4)));
    MH_CHECK(mathhunter::arithmetic::is_odd(Integer(7)));

    auto small = mathhunter::arithmetic::try_to_u64(Integer(42));
    MH_REQUIRE(small.has_value());
    MH_CHECK(*small == 42ULL);

    Integer huge("123456789012345678901234567890");
    MH_CHECK(!mathhunter::arithmetic::try_to_u64(huge).has_value());

    bool threw = false;
    try {
        Integer past_bound = mathhunter::arithmetic::deterministic_primality_bound() + 1;
        [[maybe_unused]] auto ignored = mathhunter::arithmetic::is_prime_exact(past_bound);
    } catch (const mathhunter::arithmetic::DomainError&) {
        threw = true;
    }
    MH_CHECK(threw);

    const auto primes_10_30 = mathhunter::arithmetic::primes_in_range(10, 30);
    const std::vector<std::uint64_t> expected{11, 13, 17, 19, 23, 29};
    MH_CHECK(primes_10_30 == expected);

    MH_TEST_MAIN_EPILOGUE();
}
