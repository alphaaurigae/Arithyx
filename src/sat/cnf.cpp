#include "mathhunter/sat/cnf.hpp"

#include "mathhunter/core/byteio.hpp"

namespace mathhunter::sat {

std::vector<std::uint8_t> canonical_bytes(const Cnf& cnf) {
    core::ByteWriter w;
    w.put_le<std::uint32_t>(cnf.num_vars);
    w.put_le<std::uint32_t>(static_cast<std::uint32_t>(cnf.clauses.size()));
    for (const auto& clause : cnf.clauses) {
        w.put_le<std::uint32_t>(static_cast<std::uint32_t>(clause.size()));
        for (Literal lit : clause) {
            w.put_le<std::uint32_t>(static_cast<std::uint32_t>(lit));
        }
    }
    return w.bytes();
}

core::Digest256 hash_cnf(const Cnf& cnf) {
    const auto bytes = canonical_bytes(cnf);
    return core::sha256(std::as_bytes(std::span{bytes}));
}

bool evaluate(const Cnf& cnf, const std::vector<bool>& assignment_one_indexed) {
    for (const auto& clause : cnf.clauses) {
        bool satisfied = false;
        for (Literal lit : clause) {
            const auto var = static_cast<std::uint32_t>(lit > 0 ? lit : -lit);
            if (var == 0 || var >= assignment_one_indexed.size()) {
                continue;
            }
            const bool value = assignment_one_indexed[var];
            if ((lit > 0 && value) || (lit < 0 && !value)) {
                satisfied = true;
                break;
            }
        }
        if (!satisfied) {
            return false;
        }
    }
    return true;
}

}  // namespace mathhunter::sat
