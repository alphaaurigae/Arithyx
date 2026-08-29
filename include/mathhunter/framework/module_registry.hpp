#pragma once

#include <cstdint>
#include <functional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "mathhunter/certificate/certificate.hpp"
#include "mathhunter/core/result.hpp"

namespace mathhunter::framework {

// The single type-erasure boundary in math-hunter (docs/GUIDANCE.md,
// "Phase 1 — ABI"): everything above this point (certificate.cpp's
// structural validation, the CLI's search/inspect/verify dispatch) talks
// only to ModuleRegistry, never to a specific conjecture module by name.
// Everything below it (src/conjectures/*_registration.cpp) is free to use
// whatever concrete Witness/Evidence/ProofData types a module needs.

struct CertificateKindOps final {
    certificate::CertificateKind kind{};
    std::function<void(std::span<const std::uint8_t> payload)> inspect;
    std::function<int(std::span<const std::uint8_t> payload)> verify;
};

struct ModuleDescriptor final {
    std::string id;
    core::ClaimId claim{core::ClaimId::Unknown};
    std::string search_usage;
    std::function<int(const std::vector<std::string>& args)> run_search;
    std::function<int()> run_benchmark;
    std::vector<CertificateKindOps> kinds;
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
