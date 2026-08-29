#include "mathhunter/sat/dpll_solver.hpp"

#include <algorithm>

namespace mathhunter::sat {

namespace {

enum class Value : std::uint8_t { Unassigned = 0, True = 1, False = 2 };

using Assignment = std::vector<Value>;

[[nodiscard]] std::optional<bool> clause_status(const Clause& clause, const Assignment& a) {
    bool has_unassigned = false;
    for (Literal lit : clause) {
        const auto var = static_cast<std::size_t>(lit > 0 ? lit : -lit);
        const Value v = a[var];
        if (v == Value::Unassigned) {
            has_unassigned = true;
            continue;
        }
        const bool satisfied_by_this_literal =
            (lit > 0 && v == Value::True) || (lit < 0 && v == Value::False);
        if (satisfied_by_this_literal) {
            return true;
        }
    }
    if (has_unassigned) {
        return std::nullopt;
    }
    return false;
}

[[nodiscard]] bool unit_propagate(const Cnf& cnf, Assignment& a) {
    bool changed = true;
    while (changed) {
        changed = false;
        for (const auto& clause : cnf.clauses) {
            const auto status = clause_status(clause, a);
            if (status.has_value() && !*status) {
                return false;
            }
            if (status.has_value()) {
                continue;
            }
            Literal unit_lit = 0;
            int unassigned_count = 0;
            for (Literal lit : clause) {
                const auto var = static_cast<std::size_t>(lit > 0 ? lit : -lit);
                if (a[var] == Value::Unassigned) {
                    ++unassigned_count;
                    unit_lit = lit;
                }
            }
            if (unassigned_count == 1) {
                const auto var = static_cast<std::size_t>(unit_lit > 0 ? unit_lit : -unit_lit);
                a[var] = unit_lit > 0 ? Value::True : Value::False;
                changed = true;
            }
        }
    }
    return true;
}

[[nodiscard]] bool all_satisfied(const Cnf& cnf, const Assignment& a) {
    return std::ranges::all_of(cnf.clauses, [&a](const Clause& clause) {
        const auto status = clause_status(clause, a);
        return status.has_value() && *status;
    });
}

[[nodiscard]] std::optional<std::size_t> pick_unassigned(const Assignment& a) {
    for (std::size_t v = 1; v < a.size(); ++v) {
        if (a[v] == Value::Unassigned) {
            return v;
        }
    }
    return std::nullopt;
}

[[nodiscard]] bool dpll(const Cnf& cnf, Assignment a, Assignment& solution_out) {
    if (!unit_propagate(cnf, a)) {
        return false;
    }
    if (all_satisfied(cnf, a)) {
        solution_out = a;
        return true;
    }
    const auto branch_var = pick_unassigned(a);
    if (!branch_var.has_value()) {
        return false;
    }

    Assignment try_true = a;
    try_true[*branch_var] = Value::True;
    if (dpll(cnf, try_true, solution_out)) {
        return true;
    }

    Assignment try_false = a;
    try_false[*branch_var] = Value::False;
    return dpll(cnf, try_false, solution_out);
}

}  // namespace

DpllSolver::Result DpllSolver::solve(const Cnf& cnf) const {
    Assignment initial(static_cast<std::size_t>(cnf.num_vars) + 1, Value::Unassigned);
    Assignment solution;
    Result result;
    result.satisfiable = dpll(cnf, initial, solution);
    if (result.satisfiable) {
        result.assignment.resize(solution.size());
        for (std::size_t i = 0; i < solution.size(); ++i) {
            result.assignment[i] = solution[i] == Value::True;
        }
    }
    return result;
}

}  // namespace mathhunter::sat
