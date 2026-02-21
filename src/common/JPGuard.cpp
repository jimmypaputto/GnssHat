/*
 * Jimmy Paputto 2023
 */

#include "common/JPGuard.hpp"


JPGuard::JPGuard()
: locked_(false)
{
}

bool JPGuard::takeResource()
{
    std::unique_lock lock(mutex_);
    cv_.wait(lock, [this] { return !locked_; });
    locked_ = true;
    return true;
}

bool JPGuard::takeResource(const uint32_t timeoutMs)
{
    std::unique_lock lock(mutex_);
    const auto cvStatus = cv_.wait_for(
        lock,
        std::chrono::milliseconds(timeoutMs),
        [this] { return !locked_; }
    );
    if (cvStatus)
    {
        locked_ = true;
    }
    return cvStatus;
}

bool JPGuard::releaseResource()
{
    {
        std::lock_guard lock(mutex_);
        locked_ = false;
    }
    cv_.notify_one();
    return true;
}
