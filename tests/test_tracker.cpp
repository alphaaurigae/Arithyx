#include <thread>

#include "mathhunter/framework/tracker.hpp"
#include "test_harness.hpp"

using namespace mathhunter;

namespace {

framework::WorkUnitKey sample_key(std::string_view tag) {
    return framework::WorkUnitKey{
        .module_id = "tracker_test",
        .partition = core::EncodedBlob{.schema_version = 1,
                                        .bytes = std::vector<std::uint8_t>(tag.begin(), tag.end())}};
}

framework::WorkUnitOutcome bounded_completion_outcome() {
    return framework::WorkUnitOutcome{.kind = framework::WorkUnitOutcomeKind::BoundedCompletion,
                                       .certificate_kind = std::nullopt,
                                       .payload = {1, 2, 3},
                                       .diagnostic = {}};
}

framework::WorkUnitOutcome anomaly_outcome() {
    return framework::WorkUnitOutcome{.kind = framework::WorkUnitOutcomeKind::Anomaly,
                                       .certificate_kind = std::nullopt,
                                       .payload = {},
                                       .diagnostic = "solver contradicted its own invariant"};
}

}  // namespace

int main() {
    {
        framework::WorkUnitTracker tracker;
        const auto id = tracker.enqueue(sample_key("a"));

        auto leased = tracker.lease("worker-1");
        MH_REQUIRE(leased.has_value());
        MH_CHECK(leased->first == id);

        auto record = tracker.find(id);
        MH_REQUIRE(record.has_value());
        MH_CHECK(record->state == framework::WorkUnitState::Leased);
        MH_CHECK(record->attempts == 1);

        MH_CHECK(!tracker.lease("worker-2").has_value());

        tracker.mark_running(id, "worker-1");
        record = tracker.find(id);
        MH_REQUIRE(record.has_value());
        MH_CHECK(record->state == framework::WorkUnitState::Running);

        tracker.report_result(id, "worker-1", bounded_completion_outcome());
        record = tracker.find(id);
        MH_REQUIRE(record.has_value());
        MH_CHECK(record->state == framework::WorkUnitState::Result);

        tracker.report_verification(id, true);
        record = tracker.find(id);
        MH_REQUIRE(record.has_value());
        MH_CHECK(record->state == framework::WorkUnitState::Verified);
        MH_CHECK(tracker.all_terminal());
    }

    {
        framework::WorkUnitTracker tracker;
        const auto id = tracker.enqueue(sample_key("b"));
        auto leased = tracker.lease("worker-1");
        MH_REQUIRE(leased.has_value());

        tracker.report_result(id, "worker-1", anomaly_outcome());
        auto record = tracker.find(id);
        MH_REQUIRE(record.has_value());
        MH_CHECK(record->state == framework::WorkUnitState::Flagged);
        MH_CHECK(!record->flag_reason.empty());
    }

    {
        framework::WorkUnitTracker tracker(framework::TrackerConfig{.verification_quorum = 2});
        const auto id = tracker.enqueue(sample_key("c"));
        auto leased = tracker.lease("worker-1");
        MH_REQUIRE(leased.has_value());
        tracker.report_result(id, "worker-1", bounded_completion_outcome());

        tracker.report_verification(id, true);
        auto record = tracker.find(id);
        MH_REQUIRE(record.has_value());
        MH_CHECK(record->state == framework::WorkUnitState::Result);

        tracker.report_verification(id, true);
        record = tracker.find(id);
        MH_REQUIRE(record.has_value());
        MH_CHECK(record->state == framework::WorkUnitState::Verified);
    }

    {
        framework::WorkUnitTracker tracker;
        const auto id = tracker.enqueue(sample_key("d"));
        auto leased = tracker.lease("worker-1");
        MH_REQUIRE(leased.has_value());
        tracker.report_result(id, "worker-1", bounded_completion_outcome());

        tracker.report_verification(id, false, "independent verifier disagreed");
        auto record = tracker.find(id);
        MH_REQUIRE(record.has_value());
        MH_CHECK(record->state == framework::WorkUnitState::Flagged);
        MH_CHECK(record->flag_reason == "independent verifier disagreed");
    }

    {
        framework::WorkUnitTracker tracker(framework::TrackerConfig{
            .lease_duration = std::chrono::seconds{0}, .max_attempts = 2});
        const auto id = tracker.enqueue(sample_key("e"));

        auto first_lease = tracker.lease("worker-1");
        MH_REQUIRE(first_lease.has_value());
        std::this_thread::sleep_for(std::chrono::milliseconds{5});
        MH_CHECK(tracker.reclaim_expired_leases() == 1);

        auto record = tracker.find(id);
        MH_REQUIRE(record.has_value());
        MH_CHECK(record->state == framework::WorkUnitState::Pending);

        auto second_lease = tracker.lease("worker-2");
        MH_REQUIRE(second_lease.has_value());
        std::this_thread::sleep_for(std::chrono::milliseconds{5});
        MH_CHECK(tracker.reclaim_expired_leases() == 1);

        record = tracker.find(id);
        MH_REQUIRE(record.has_value());
        MH_CHECK(record->state == framework::WorkUnitState::Flagged);
        MH_CHECK(tracker.all_terminal());
    }

    {
        framework::WorkUnitTracker tracker;
        const auto id = tracker.enqueue(sample_key("f"));
        const auto duplicate_id = tracker.enqueue(sample_key("f"));
        MH_CHECK(id == duplicate_id);
        MH_CHECK(tracker.snapshot().size() == 1);
    }

    MH_TEST_MAIN_EPILOGUE();
}
