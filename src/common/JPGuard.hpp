/*
 * Jimmy Paputto 2023
 */

#ifndef JP_GUARD_HPP_
#define JP_GUARD_HPP_

#include <condition_variable>
#include <cstdint>
#include <mutex>


class JPGuard final
{
public:
    explicit JPGuard();

    bool takeResource();
    bool takeResource(const uint32_t timeoutMs);
    bool releaseResource();

private:
    std::mutex mutex_;
    std::condition_variable cv_;
    bool locked_;
};

#endif  // JP_GUARD_HPP_
