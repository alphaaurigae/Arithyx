#pragma once

#include <Poco/JSON/Object.h>

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace mathhunter::cli {

[[nodiscard]] std::vector<std::filesystem::path> metadata_search_dirs();

[[nodiscard]] std::optional<Poco::JSON::Object::Ptr> load_conjecture_metadata(
    const std::string& identifier);

[[nodiscard]] std::vector<std::string> list_conjecture_identifiers();

}  // namespace mathhunter::cli
