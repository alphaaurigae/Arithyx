#pragma once

#include <concepts>
#include <functional>
#include <future>
#include <memory>

#include "mathhunter/framework/work_unit.hpp"

namespace mathhunter::framework {

template <typename E>
concept WorkUnitExecutor =
    requires(E e, const WorkUnitKey& key, std::function<WorkUnitOutcome(const WorkUnitKey&)> run_fn) {
        { e.submit(key, run_fn) } -> std::same_as<std::future<WorkUnitOutcome>>;
    };

class LocalExecutor final {
public:
    explicit LocalExecutor(unsigned worker_count = 0);
    ~LocalExecutor();

    LocalExecutor(const LocalExecutor&) = delete;
    LocalExecutor& operator=(const LocalExecutor&) = delete;
    LocalExecutor(LocalExecutor&&) = delete;
    LocalExecutor& operator=(LocalExecutor&&) = delete;

    [[nodiscard]] std::future<WorkUnitOutcome> submit(
        const WorkUnitKey& key, std::function<WorkUnitOutcome(const WorkUnitKey&)> run_fn);

    [[nodiscard]] unsigned worker_count() const noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

static_assert(WorkUnitExecutor<LocalExecutor>);

}  // namespace mathhunter::framework
