#include <RE/Skyrim.h> // IWYU pragma: keep

#include "GameTasks.h"
#include <SKSE/SKSE.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <map>
#include <mutex>
#include <stop_token>
#include <thread>
#include <utility>

namespace {
class TaskQueue {
public:
    TaskQueue()
        : worker_([this](const std::stop_token& a_stop) { Run(a_stop); }) {}

    void Add(std::function<void()> a_task, std::chrono::milliseconds a_delay) {
        std::unique_lock lock(mutex_);
        PendingTask pending {.action = std::move(a_task), .generation = generation_.load()};
        if (a_delay.count() <= 0) {
            lock.unlock();
            Dispatch(std::move(pending));
            return;
        }
        tasks_.emplace(std::chrono::steady_clock::now() + a_delay, std::move(pending));
        changed_.notify_one();
    }

    void CancelPending() {
        std::scoped_lock const lock(mutex_);
        ++generation_;
        tasks_.clear();
        changed_.notify_one();
    }

private:
    struct PendingTask {
        std::function<void()> action;
        std::uint64_t generation;
    };

    void Dispatch(PendingTask a_task) {
        SKSE::GetTaskInterface()->AddTask([this, task = std::move(a_task)] {
            // Loading also invalidates tasks already handed to SKSE.
            if (task.generation == generation_.load()) {
                task.action();
            }
        });
    }

    void Run(const std::stop_token& a_stop) {
        while (!a_stop.stop_requested()) {
            std::unique_lock lock(mutex_);
            if (!changed_.wait(lock, a_stop, [this] { return !tasks_.empty(); })) {
                return;
            }
            const auto due = tasks_.begin()->first;
            if (changed_.wait_until(lock, a_stop, due, [this, due] {
                    return tasks_.empty() || tasks_.begin()->first != due;
                })) {
                continue;
            }
            if (a_stop.stop_requested()) {
                return;
            }
            auto task = std::move(tasks_.begin()->second);
            tasks_.erase(tasks_.begin());
            lock.unlock();
            Dispatch(std::move(task));
        }
    }

    std::mutex mutex_;
    std::condition_variable_any changed_;
    std::multimap<std::chrono::steady_clock::time_point, PendingTask> tasks_;
    std::atomic<std::uint64_t> generation_ {0};
    std::jthread worker_;
};

TaskQueue& GetQueue() {
    // Joining the worker during DLL teardown can deadlock. Keep the queue alive until process exit.
    static auto* const queue = new TaskQueue;
    return *queue;
}
}

void GameTasks::Add(std::function<void()> a_task, std::chrono::milliseconds a_delay) {
    GetQueue().Add(std::move(a_task), a_delay);
}

void GameTasks::CancelPending() {
    GetQueue().CancelPending();
}
