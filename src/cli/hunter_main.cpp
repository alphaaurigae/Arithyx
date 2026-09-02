#include <Poco/JSON/Stringifier.h>

#include <iostream>
#include <string>
#include <vector>

#include "mathhunter/framework/module_registry.hpp"
#include "metadata_loader.hpp"

namespace {

using namespace mathhunter;

void print_usage() {
    std::cout << "math-hunter -- computational conjecture search (discovery only; never trust "
                 "this program's opinion of its own results -- use math-verifier)\n\n"
                 "Usage:\n"
                 "  math-hunter list\n"
                 "  math-hunter info <conjecture>\n";
    for (const auto& module : framework::registry().modules()) {
        std::cout << "  math-hunter " << module.search_usage << "\n";
    }
    std::cout << "  math-hunter benchmark <conjecture>\n";
}

int cmd_list() {
    const auto ids = cli::list_conjecture_identifiers();
    if (ids.empty()) {
        std::cerr << "No conjecture metadata found. Set MATHHUNTER_METADATA_DIR or run from "
                     "the repository root.\n";
        return 1;
    }
    for (const auto& id : ids) {
        auto meta = cli::load_conjecture_metadata(id);
        if (!meta) continue;
        std::cout << id << "\t" << (*meta)->optValue<std::string>("name", "?") << "\t["
                   << (*meta)->optValue<std::string>("status", "?") << "]\n";
    }
    return 0;
}

int cmd_info(const std::string& id) {
    auto meta = cli::load_conjecture_metadata(id);
    if (!meta) {
        std::cerr << "Unknown conjecture or metadata not found: " << id << "\n";
        return 1;
    }
    Poco::JSON::Stringifier::stringify(*meta, std::cout, 2);
    std::cout << "\n";
    return 0;
}

int cmd_benchmark(const std::string& id) {
    const auto* module = framework::registry().find_by_id(id);
    if (module == nullptr || !module->run_benchmark) {
        std::cerr << "No benchmark defined for: " << id << "\n";
        return 1;
    }
    return module->run_benchmark();
}

}  // namespace

int main(int argc, char** argv) {
    std::vector<std::string> args(argv + 1, argv + argc);
    if (args.empty()) {
        print_usage();
        return 1;
    }

    const std::string& command = args[0];
    std::vector<std::string> rest(args.begin() + 1, args.end());

    if (command == "list") return cmd_list();
    if (command == "info" && !rest.empty()) return cmd_info(rest[0]);
    if (command == "benchmark" && !rest.empty()) return cmd_benchmark(rest[0]);
    if (command == "search" && !rest.empty()) {
        const std::string& conjecture = rest[0];
        std::vector<std::string> flags(rest.begin() + 1, rest.end());
        const auto* module = framework::registry().find_by_id(conjecture);
        if (module == nullptr || !module->run_search) {
            std::cerr << "Unknown conjecture: " << conjecture << "\n";
            return 1;
        }
        return module->run_search(flags);
    }

    print_usage();
    return 1;
}
