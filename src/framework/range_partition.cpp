#include "mathhunter/framework/range_partition.hpp"

#include <limits>

namespace mathhunter::framework {

std::vector<std::pair<std::uint64_t, std::uint64_t>> partition_u64_range(std::uint64_t begin,
                                                                          std::uint64_t end,
                                                                          std::uint64_t target_size) {
    std::vector<std::pair<std::uint64_t, std::uint64_t>> chunks;
    if (begin >= end) {
        return chunks;
    }
    if (target_size == 0) {
        chunks.emplace_back(begin, end);
        return chunks;
    }
    for (std::uint64_t cursor = begin; cursor < end;) {
        const std::uint64_t remaining = end - cursor;
        const std::uint64_t step = remaining < target_size ? remaining : target_size;
        chunks.emplace_back(cursor, cursor + step);
        cursor += step;
    }
    return chunks;
}

std::vector<std::pair<arithmetic::Integer, arithmetic::Integer>> partition_integer_range(
    const arithmetic::Integer& begin, const arithmetic::Integer& end,
    std::uint64_t target_size) {
    std::vector<std::pair<arithmetic::Integer, arithmetic::Integer>> chunks;
    if (begin >= end) {
        return chunks;
    }
    if (target_size == 0) {
        chunks.emplace_back(begin, end);
        return chunks;
    }
    const arithmetic::Integer step(target_size);
    arithmetic::Integer cursor = begin;
    while (cursor < end) {
        arithmetic::Integer next = cursor + step;
        if (next > end) {
            next = end;
        }
        chunks.emplace_back(cursor, next);
        cursor = next;
    }
    return chunks;
}

std::uint64_t saturating_range_size_u64(std::uint64_t begin, std::uint64_t end) {
    return end > begin ? end - begin : 0;
}

std::uint64_t saturating_range_size_u64(const arithmetic::Integer& begin,
                                         const arithmetic::Integer& end) {
    if (end <= begin) {
        return 0;
    }
    const arithmetic::Integer size = end - begin;
    const arithmetic::Integer cap(std::numeric_limits<std::uint64_t>::max());
    if (size > cap) {
        return std::numeric_limits<std::uint64_t>::max();
    }
    return size.get_ui();
}

}  // namespace mathhunter::framework
