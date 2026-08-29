#pragma once

#include <filesystem>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <vector>

namespace mathhunter::cli {

[[nodiscard]] std::vector<std::filesystem::path> metadata_search_dirs();

[[nodiscard]] std::optional<nlohmann::json> load_conjecture_metadata(const std::string& identifier);

[[nodiscard]] std::vector<std::string> list_conjecture_identifiers();

}  // namespace mathhunter::cli
