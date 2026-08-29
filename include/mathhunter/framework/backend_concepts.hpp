#pragma once

#include "mathhunter/core/result.hpp"

namespace mathhunter::framework {

// ABI Phase 1 — DiscoveryBackend concept family (docs/GUIDANCE.md).
//
// The framework currently exposes each module's discovery entry point as a
// plain free function (conjectures::kurepa::search_range,
// conjectures::pigeonhole::search_unsat_benchmark, ...) rather than the
// WorkUnitKey-driven backend object GUIDANCE sketches for Phase 3. These
// two concepts formalize today's actual calling convention for the two
// discovery shapes that exist right now — a [begin, end) range scanned for
// a falsifying witness, and a single opaque key searched for a proof —
// without inventing the opaque-bytes WorkUnitKey abstraction ahead of
// Phase 3. static_assert-ing every module's real search function against
// these turns "duck-typed convention" into a compile-checked contract.

template <typename Fn, typename Domain, typename Witness>
concept RangeSearchDiscovery = requires(Fn fn, const Domain& begin, const Domain& end) {
    { fn(begin, end) } -> core::FalsificationSearchOutcome<Domain, Witness>;
};

template <typename Fn, typename Key, typename ProofData, typename AnomalyData>
concept KeyedProofSearchDiscovery = requires(Fn fn, const Key& key) {
    { fn(key) } -> core::ProofSearchOutcome<ProofData, AnomalyData>;
};

}  // namespace mathhunter::framework
