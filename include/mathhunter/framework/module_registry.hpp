#pragma once

#include <cstdint>
#include <expected>
#include <functional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "mathhunter/certificate/certificate.hpp"
#include "mathhunter/core/result.hpp"
#include "mathhunter/framework/work_unit.hpp"

namespace mathhunter::framework {

// The single type-erasure boundary in math-hunter (docs/GUIDANCE.md,
// "Phase 1 — ABI"): everything above this point (certificate.cpp's
// structural validation, the CLI's search/inspect/verify dispatch) talks
// only to ModuleRegistry, never to a specific conjecture module by name.
// Everything below it (src/conjectures/*_registration.cpp) is free to use
// whatever concrete Witness/Evidence/ProofData types a module needs.

enum class SilentVerifyOutcome : std::uint8_t {
    Verified,
    Rejected,
    Unverifiable,
};

struct CertificateKindOps final {
    certificate::CertificateKind kind{};
    std::function<void(std::span<const std::uint8_t> payload)> inspect;
    std::function<int(std::span<const std::uint8_t> payload)> verify;
    std::function<SilentVerifyOutcome(std::span<const std::uint8_t> payload)> verify_quiet;
};

struct WorkUnitOps final {
    std::function<std::expected<WorkUnitKey, std::string>(const std::vector<std::string>& args)>
        make_from_cli;
    std::function<WorkUnitCost(const WorkUnitKey&)> estimate_cost;
    std::function<std::vector<WorkUnitKey>(const WorkUnitKey&, std::uint64_t target_units)>
        partition;
    std::function<WorkUnitOutcome(const WorkUnitKey&)> run;
};

struct ModuleDescriptor final {
    std::string id;
    core::ClaimId claim{core::ClaimId::Unknown};
    std::string search_usage;
    std::function<int(const std::vector<std::string>& args)> run_search;
    std::function<int()> run_benchmark;
    std::vector<CertificateKindOps> kinds;
    WorkUnitOps work_unit;
};

class ModuleRegistry final {
public:
    void register_module(ModuleDescriptor descriptor);

    [[nodiscard]] bool has_claim(core::ClaimId claim) const noexcept;
    [[nodiscard]] bool has_kind(certificate::CertificateKind kind) const noexcept;

    [[nodiscard]] const ModuleDescriptor* find_by_id(std::string_view id) const noexcept;
    [[nodiscard]] const CertificateKindOps* find_kind_ops(
        certificate::CertificateKind kind) const noexcept;

    [[nodiscard]] std::span<const ModuleDescriptor> modules() const noexcept { return modules_; }

private:
    std::vector<ModuleDescriptor> modules_;
};

[[nodiscard]] ModuleRegistry& registry() noexcept;

}  // namespace mathhunter::framework
