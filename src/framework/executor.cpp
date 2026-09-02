#include "mathhunter/framework/executor.hpp"

#include <algorithm>
#include <condition_variable>
#include <mutex>
#include <queue>
#include <stop_token>
#include <thread>
#include <vector>

namespace mathhunter::framework {

struct LocalExecutor::Impl {
    std::mutex mutex;
    std::condition_variable cv;
    std::queue<std::packaged_task<WorkUnitOutcome()>> tasks;
    std::vector<std::jthread> workers;
    bool stopping{false};

    void worker_loop(const std::stop_token& stop_token) {
        while (true) {
            std::packaged_task<WorkUnitOutcome()> task;
            {
                std::unique_lock<std::mutex> lock(mutex);
                cv.wait(lock, [&] { return stopping || !tasks.empty() || stop_token.stop_requested(); });
                if (stop_token.stop_requested() && tasks.empty()) {
                    return;
                }
                if (stopping && tasks.empty()) {
                    return;
                }
                if (tasks.empty()) {
                    continue;
                }
                task = std::move(tasks.front());
                tasks.pop();
            }
            task();
        }
    }
};

LocalExecutor::LocalExecutor(unsigned worker_count) : impl_(std::make_unique<Impl>()) {
    const unsigned count =
        worker_count != 0 ? worker_count : std::max(1U, std::thread::hardware_concurrency());
    impl_->workers.reserve(count);
    for (unsigned i = 0; i < count; ++i) {
        impl_->workers.emplace_back([this](const std::stop_token& st) { impl_->worker_loop(st); });
    }
}

LocalExecutor::~LocalExecutor() {
    {
        std::lock_guard<std::mutex> lock(impl_->mutex);
        impl_->stopping = true;
    }
    impl_->cv.notify_all();
    for (auto& worker : impl_->workers) {
        worker.request_stop();
    }
}

std::future<WorkUnitOutcome> LocalExecutor::submit(
    const WorkUnitKey& key, std::function<WorkUnitOutcome(const WorkUnitKey&)> run_fn) {
    std::packaged_task<WorkUnitOutcome()> task([key, run_fn = std::move(run_fn)] {  // NOLINT(bugprone-exception-escape)
        return run_fn(key);
    });
    auto future = task.get_future();
    {
        std::lock_guard<std::mutex> lock(impl_->mutex);
        impl_->tasks.push(std::move(task));
    }
    impl_->cv.notify_one();
    return future;
}

unsigned LocalExecutor::worker_count() const noexcept {
    return static_cast<unsigned>(impl_->workers.size());
}

}  // namespace mathhunter::framework
