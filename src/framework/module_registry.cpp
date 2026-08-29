#include "mathhunter/framework/module_registry.hpp"

#include <algorithm>

namespace mathhunter::framework {

void ModuleRegistry::register_module(ModuleDescriptor descriptor) {
    modules_.push_back(std::move(descriptor));
}

bool ModuleRegistry::has_claim(core::ClaimId claim) const noexcept {
    return std::ranges::any_of(modules_,
                                [claim](const ModuleDescriptor& m) { return m.claim == claim; });
}

bool ModuleRegistry::has_kind(certificate::CertificateKind kind) const noexcept {
    return find_kind_ops(kind) != nullptr;
}

const ModuleDescriptor* ModuleRegistry::find_by_id(std::string_view id) const noexcept {
    for (const auto& m : modules_) {
        if (m.id == id) {
            return &m;
        }
    }
    return nullptr;
}

const CertificateKindOps* ModuleRegistry::find_kind_ops(
    certificate::CertificateKind kind) const noexcept {
    for (const auto& m : modules_) {
        for (const auto& k : m.kinds) {
            if (k.kind == kind) {
                return &k;
            }
        }
    }
    return nullptr;
}

ModuleRegistry& registry() noexcept {
    static ModuleRegistry instance;
    return instance;
}

}  // namespace mathhunter::framework
