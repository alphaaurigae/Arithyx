#include "mathhunter/framework/module_registry.hpp"
#include "test_harness.hpp"

using namespace mathhunter;

namespace {

std::optional<framework::WorkUnitKey> make_sample_key(const framework::ModuleDescriptor& module) {
    if (!module.work_unit.make_from_cli) {
        return std::nullopt;
    }
    for (const std::vector<std::string>& candidate_args :
         {std::vector<std::string>{"--begin", "2", "--end", "5000"},
          std::vector<std::string>{"--pigeons", "5"}}) {
        auto key = module.work_unit.make_from_cli(candidate_args);
        if (key.has_value()) {
            return *key;
        }
    }
    return std::nullopt;
}

}  // namespace

int main() {
    std::size_t modules_checked = 0;

    for (const auto& module : framework::registry().modules()) {
        if (!module.work_unit.run) {
            continue;
        }
        const auto sample = make_sample_key(module);
        if (!sample.has_value()) {
            std::fprintf(stderr, "[SKIP] %s: no candidate CLI args produced a sample work unit\n",
                         module.id.c_str());
            continue;
        }
        ++modules_checked;

        const auto first = module.work_unit.run(*sample);
        const auto second = module.work_unit.run(*sample);

        MH_CHECK(first.kind == second.kind);
        MH_CHECK(first.certificate_kind == second.certificate_kind);
        MH_CHECK(first.payload == second.payload);
        MH_CHECK(first.diagnostic == second.diagnostic);

        if (module.work_unit.estimate_cost) {
            MH_CHECK(module.work_unit.estimate_cost(*sample).units ==
                     module.work_unit.estimate_cost(*sample).units);
        }
    }

    MH_CHECK(modules_checked >= 3);

    MH_TEST_MAIN_EPILOGUE();
}
