#pragma once

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "mathhunter/arithmetic/modular.hpp"

namespace mathhunter::framework::cli_args {

[[nodiscard]] inline std::optional<std::uint64_t> parse_u64_flag(
    const std::vector<std::string>& args, std::string_view flag) {
    for (std::size_t i = 0; i + 1 < args.size(); ++i) {
        if (args[i] == flag) {
            try {
                return std::stoull(args[i + 1]);
            } catch (...) {
                return std::nullopt;
            }
        }
    }
    return std::nullopt;
}

[[nodiscard]] inline std::optional<arithmetic::Integer> parse_integer_flag(
    const std::vector<std::string>& args, std::string_view flag) {
    for (std::size_t i = 0; i + 1 < args.size(); ++i) {
        if (args[i] == flag) {
            const std::string& value = args[i + 1];
            if (value.empty() ||
                !std::ranges::all_of(value, [](unsigned char c) { return std::isdigit(c) != 0; })) {
                return std::nullopt;
            }
            try {
                return arithmetic::Integer(value);
            } catch (const std::invalid_argument&) {
                return std::nullopt;
            }
        }
    }
    return std::nullopt;
}

[[nodiscard]] inline std::optional<std::string> parse_string_flag(
    const std::vector<std::string>& args, std::string_view flag) {
    for (std::size_t i = 0; i + 1 < args.size(); ++i) {
        if (args[i] == flag) {
            return args[i + 1];
        }
    }
    return std::nullopt;
}

}  // namespace mathhunter::framework::cli_args
