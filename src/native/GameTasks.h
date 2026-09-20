#pragma once

#include <chrono>
#include <functional>

namespace GameTasks {
void Add(std::function<void()> a_task, std::chrono::milliseconds a_delay = {});
void CancelPending();
}
