#include <chrono>

#include "mathhunter/framework/executor.hpp"
#include "mathhunter/framework/module_registry.hpp"
#include "mathhunter/framework/tracker.hpp"
#include "test_harness.hpp"

using namespace mathhunter;

namespace {

struct SampleJob final {
    std::string_view module_id;
    std::vector<std::string> cli_args;
};

}  // namespace

int main() {
    framework::WorkUnitTracker tracker;
    framework::LocalExecutor executor(2);
    MH_CHECK(executor.worker_count() == 2);

    const std::vector<SampleJob> jobs{
        {"kurepa", {"--begin", "3", "--end", "5000"}},
        {"toy_mod7", {"--begin", "1", "--end", "100"}},
        {"pigeonhole", {"--pigeons", "5"}},
    };

    std::vector<framework::WorkUnitId> enqueued_ids;
    for (const auto& job : jobs) {
        const auto* module = framework::registry().find_by_id(job.module_id);
        MH_REQUIRE(module != nullptr);
        MH_REQUIRE(static_cast<bool>(module->work_unit.make_from_cli));
        auto key = module->work_unit.make_from_cli(job.cli_args);
        MH_REQUIRE(key.has_value());
        enqueued_ids.push_back(tracker.enqueue(*key));
    }
    MH_CHECK(enqueued_ids.size() == 3);

    std::vector<std::pair<framework::WorkUnitId, std::future<framework::WorkUnitOutcome>>>
        in_flight;
    const framework::WorkerId worker_id = "local-worker-0";

    for (std::size_t i = 0; i < jobs.size(); ++i) {
        auto leased = tracker.lease(worker_id);
        MH_REQUIRE(leased.has_value());
        const auto& [id, key] = *leased;
        tracker.mark_running(id, worker_id);

        const auto* module = framework::registry().find_by_id(key.module_id);
        MH_REQUIRE(module != nullptr);
        auto run_fn = module->work_unit.run;
        in_flight.emplace_back(id, executor.submit(key, run_fn));
    }

    for (auto& [id, future] : in_flight) {
        const auto status = future.wait_for(std::chrono::seconds{10});
        MH_REQUIRE(status == std::future_status::ready);
        auto outcome = future.get();
        tracker.report_result(id, worker_id, outcome);

        auto record = tracker.find(id);
        MH_REQUIRE(record.has_value());
        MH_REQUIRE(record->outcome.has_value());

        if (record->outcome->kind == framework::WorkUnitOutcomeKind::Anomaly) {
            continue;
        }
        MH_REQUIRE(record->outcome->certificate_kind.has_value());
        const auto* ops = framework::registry().find_kind_ops(*record->outcome->certificate_kind);
        MH_REQUIRE(ops != nullptr);
        MH_REQUIRE(static_cast<bool>(ops->verify_quiet));
        const auto verdict = ops->verify_quiet(record->outcome->payload);
        MH_CHECK(verdict == framework::SilentVerifyOutcome::Verified);
        tracker.report_verification(id, verdict == framework::SilentVerifyOutcome::Verified);
    }

    MH_CHECK(tracker.reclaim_expired_leases() == 0);
    MH_CHECK(tracker.all_terminal());

    for (const auto& id : enqueued_ids) {
        auto record = tracker.find(id);
        MH_REQUIRE(record.has_value());
        MH_CHECK(record->state == framework::WorkUnitState::Verified);
    }

    MH_TEST_MAIN_EPILOGUE();
}
