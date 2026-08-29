#pragma once

#include <cstdint>
#include <span>
#include <vector>

#include "mathhunter/core/hash.hpp"

namespace mathhunter::sat {

using Literal = std::int32_t;

using Clause = std::vector<Literal>;

struct Cnf final {
    std::uint32_t num_vars{0};
    std::vector<Clause> clauses;
};

[[nodiscard]] std::vector<std::uint8_t> canonical_bytes(const Cnf& cnf);

[[nodiscard]] core::Digest256 hash_cnf(const Cnf& cnf);

[[nodiscard]] bool evaluate(const Cnf& cnf, const std::vector<bool>& assignment_one_indexed);

}  // namespace mathhunter::sat
