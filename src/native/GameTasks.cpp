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
        : _worker([this](const std::stop_token& a_stop) { Run(a_stop); }) {}

    void Add(std::function<void()> a_task, std::chrono::milliseconds a_delay) {
        std::unique_lock lock(_mutex);
        PendingTask pending {.action = std::move(a_task), .generation = _generation.load()};
        if (a_delay.count() <= 0) {
            lock.unlock();
            Dispatch(std::move(pending));
            return;
        }
        _tasks.emplace(std::chrono::steady_clock::now() + a_delay, std::move(pending));
        _changed.notify_one();
    }

    void CancelPending() {
        std::scoped_lock const lock(_mutex);
        ++_generation;
        _tasks.clear();
        _changed.notify_one();
    }

private:
    struct PendingTask {
        std::function<void()> action;
        std::uint64_t generation;
    };

    void Dispatch(PendingTask a_task) {
        SKSE::GetTaskInterface()->AddTask([this, task = std::move(a_task)] {
            // Loading also invalidates tasks already handed to SKSE.
            if (task.generation == _generation.load()) {
                task.action();
            }
        });
    }

    void Run(const std::stop_token& a_stop) {
        while (!a_stop.stop_requested()) {
            std::unique_lock lock(_mutex);
            if (!_changed.wait(lock, a_stop, [this] { return !_tasks.empty(); })) {
                return;
            }
            const auto due = _tasks.begin()->first;
            if (_changed.wait_until(lock, a_stop, due, [this, due] {
                    return _tasks.empty() || _tasks.begin()->first != due;
                })) {
                continue;
            }
            if (a_stop.stop_requested()) {
                return;
            }
            auto task = std::move(_tasks.begin()->second);
            _tasks.erase(_tasks.begin());
            lock.unlock();
            Dispatch(std::move(task));
        }
    }

    std::mutex _mutex;
    std::condition_variable_any _changed;
    std::multimap<std::chrono::steady_clock::time_point, PendingTask> _tasks;
    std::atomic<std::uint64_t> _generation {0};
    std::jthread _worker;
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
