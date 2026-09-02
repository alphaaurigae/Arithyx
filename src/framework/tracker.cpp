#include "mathhunter/framework/tracker.hpp"

#include <algorithm>

namespace mathhunter::framework {

WorkUnitTracker::WorkUnitTracker(TrackerConfig config) : config_(config) {}

WorkUnitId WorkUnitTracker::enqueue(WorkUnitKey key) {
    const auto id = key.identity_digest();
    std::lock_guard<std::mutex> lock(mutex_);
    if (records_.contains(id)) {
        return id;
    }
    WorkUnitRecord record;
    record.key = std::move(key);
    record.state = WorkUnitState::Pending;
    records_.emplace(id, std::move(record));
    pending_queue_.push_back(id);
    return id;
}

std::optional<std::pair<WorkUnitId, WorkUnitKey>> WorkUnitTracker::lease(const WorkerId& worker) {
    std::lock_guard<std::mutex> lock(mutex_);
    while (!pending_queue_.empty()) {
        const auto id = pending_queue_.front();
        pending_queue_.erase(pending_queue_.begin());
        auto it = records_.find(id);
        if (it == records_.end() || it->second.state != WorkUnitState::Pending) {
            continue;
        }
        it->second.state = WorkUnitState::Leased;
        it->second.leased_to = worker;
        it->second.lease_expiry = std::chrono::steady_clock::now() + config_.lease_duration;
        ++it->second.attempts;
        return std::make_pair(id, it->second.key);
    }
    return std::nullopt;
}

void WorkUnitTracker::mark_running(const WorkUnitId& id, const WorkerId& worker) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = records_.find(id);
    if (it == records_.end() || it->second.state != WorkUnitState::Leased ||
        it->second.leased_to != worker) {
        return;
    }
    it->second.state = WorkUnitState::Running;
}

void WorkUnitTracker::report_result(const WorkUnitId& id, const WorkerId& worker,
                                     WorkUnitOutcome outcome) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = records_.find(id);
    if (it == records_.end() || it->second.leased_to != worker ||
        (it->second.state != WorkUnitState::Leased && it->second.state != WorkUnitState::Running)) {
        return;
    }

    if (outcome.kind == WorkUnitOutcomeKind::Anomaly) {
        it->second.flag_reason = outcome.diagnostic;
        it->second.outcome = std::move(outcome);
        it->second.state = WorkUnitState::Flagged;
        return;
    }

    it->second.outcome = std::move(outcome);
    it->second.state = WorkUnitState::Result;
}

void WorkUnitTracker::report_verification(const WorkUnitId& id, bool agrees,
                                           std::string_view reason) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = records_.find(id);
    if (it == records_.end() || it->second.state != WorkUnitState::Result) {
        return;
    }

    if (!agrees) {
        it->second.flag_reason = std::string(reason);
        it->second.state = WorkUnitState::Flagged;
        return;
    }

    ++it->second.verification_agreements;
    if (it->second.verification_agreements >= config_.verification_quorum) {
        it->second.state = WorkUnitState::Verified;
    }
}

std::size_t WorkUnitTracker::reclaim_expired_leases() {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto now = std::chrono::steady_clock::now();
    std::size_t reclaimed = 0;
    for (auto& [id, record] : records_) {
        const bool in_flight =
            record.state == WorkUnitState::Leased || record.state == WorkUnitState::Running;
        if (!in_flight || record.lease_expiry > now) {
            continue;
        }
        ++reclaimed;
        if (record.attempts < config_.max_attempts) {
            record.state = WorkUnitState::Pending;
            record.leased_to.reset();
            pending_queue_.push_back(id);
        } else {
            record.state = WorkUnitState::Flagged;
            record.flag_reason = "lease retry budget exhausted";
        }
    }
    return reclaimed;
}

std::optional<WorkUnitRecord> WorkUnitTracker::find(const WorkUnitId& id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = records_.find(id);
    if (it == records_.end()) {
        return std::nullopt;
    }
    return it->second;
}

std::vector<WorkUnitRecord> WorkUnitTracker::snapshot() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<WorkUnitRecord> out;
    out.reserve(records_.size());
    for (const auto& [id, record] : records_) {
        out.push_back(record);
    }
    return out;
}

bool WorkUnitTracker::all_terminal() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return std::ranges::all_of(records_, [](const auto& kv) {
        return kv.second.state == WorkUnitState::Verified ||
               kv.second.state == WorkUnitState::Flagged;
    });
}

}  // namespace mathhunter::framework
