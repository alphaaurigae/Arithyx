#include "metadata_loader.hpp"

#include <Poco/JSON/Parser.h>

#include <algorithm>
#include <cstdlib>
#include <fstream>

namespace mathhunter::cli {

std::vector<std::filesystem::path> metadata_search_dirs() {
    std::vector<std::filesystem::path> dirs;
    if (const char* env = std::getenv("MATHHUNTER_METADATA_DIR"); env != nullptr) {
        dirs.emplace_back(env);
    }
    dirs.emplace_back("conjectures/metadata");
    dirs.emplace_back("../conjectures/metadata");
    dirs.emplace_back("../../conjectures/metadata");
    return dirs;
}

std::optional<Poco::JSON::Object::Ptr> load_conjecture_metadata(const std::string& identifier) {
    for (const auto& dir : metadata_search_dirs()) {
        const auto path = dir / (identifier + ".json");
        std::ifstream in(path);
        if (!in) {
            continue;
        }
        try {
            Poco::JSON::Parser parser;
            const auto result = parser.parse(in);
            return result.extract<Poco::JSON::Object::Ptr>();
        } catch (const Poco::Exception&) {
            return std::nullopt;
        }
    }
    return std::nullopt;
}

std::vector<std::string> list_conjecture_identifiers() {
    for (const auto& dir : metadata_search_dirs()) {
        if (!std::filesystem::exists(dir)) {
            continue;
        }
        std::vector<std::string> ids;
        for (const auto& entry : std::filesystem::directory_iterator(dir)) {
            if (entry.path().extension() == ".json") {
                ids.push_back(entry.path().stem().string());
            }
        }
        std::sort(ids.begin(), ids.end());
        if (!ids.empty()) {
            return ids;
        }
    }
    return {};
}

}  // namespace mathhunter::cli
