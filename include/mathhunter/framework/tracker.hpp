#pragma once

#include <chrono>
#include <cstdint>
#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

#include "mathhunter/framework/work_unit.hpp"

namespace mathhunter::framework {

enum class WorkUnitState : std::uint8_t {
    Pending,
    Leased,
    Running,
    Result,
    Verified,
    Flagged,
};

[[nodiscard]] constexpr std::string_view to_string(WorkUnitState s) noexcept {
    switch (s) {
        case WorkUnitState::Pending:
            return "PENDING";
        case WorkUnitState::Leased:
            return "LEASED";
        case WorkUnitState::Running:
            return "RUNNING";
        case WorkUnitState::Result:
            return "RESULT";
        case WorkUnitState::Verified:
            return "VERIFIED";
        case WorkUnitState::Flagged:
            return "FLAGGED";
    }
    return "UNKNOWN";
}

using WorkUnitId = core::Digest256;
using WorkerId = std::string;

struct WorkUnitRecord final {
    WorkUnitKey key;
    WorkUnitState state{WorkUnitState::Pending};
    std::optional<WorkerId> leased_to;
    std::chrono::steady_clock::time_point lease_expiry;
    std::uint32_t attempts{0};
    std::optional<WorkUnitOutcome> outcome;
    std::uint32_t verification_agreements{0};
    std::string flag_reason;
};

struct TrackerConfig final {
    std::chrono::seconds lease_duration{300};
    std::uint32_t max_attempts{3};
    std::uint32_t verification_quorum{1};
};

class WorkUnitTracker final {
public:
    explicit WorkUnitTracker(TrackerConfig config = {});

    WorkUnitId enqueue(WorkUnitKey key);

    [[nodiscard]] std::optional<std::pair<WorkUnitId, WorkUnitKey>> lease(const WorkerId& worker);
    void mark_running(const WorkUnitId& id, const WorkerId& worker);
    void report_result(const WorkUnitId& id, const WorkerId& worker, WorkUnitOutcome outcome);
    void report_verification(const WorkUnitId& id, bool agrees, std::string_view reason = {});

    std::size_t reclaim_expired_leases();

    [[nodiscard]] std::optional<WorkUnitRecord> find(const WorkUnitId& id) const;
    [[nodiscard]] std::vector<WorkUnitRecord> snapshot() const;
    [[nodiscard]] bool all_terminal() const;

    [[nodiscard]] const TrackerConfig& config() const noexcept { return config_; }

private:
    mutable std::mutex mutex_;
    TrackerConfig config_;
    std::map<WorkUnitId, WorkUnitRecord> records_;
    std::vector<WorkUnitId> pending_queue_;
};

}  // namespace mathhunter::framework
