#pragma once

#include <optional>
#include <vector>

#include "mathhunter/sat/cnf.hpp"

namespace mathhunter::sat {

class DpllSolver final {
public:
    struct Result final {
        bool satisfiable{false};
        std::vector<bool> assignment;
    };

    [[nodiscard]] Result solve(const Cnf& cnf) const;
};

}  // namespace mathhunter::sat
