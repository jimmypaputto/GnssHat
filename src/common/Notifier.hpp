/*
 * Jimmy Paputto 2025
 */

#ifndef JIMMY_PAPUTTO_NOTIFIER_HPP_
#define JIMMY_PAPUTTO_NOTIFIER_HPP_

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <stop_token>


namespace JimmyPaputto
{

class Notifier
{
public:
    void notify()
    {
        std::lock_guard lock(mtx);
        ready = true;
        cv.notify_one();
    }

    void wait()
    {
        std::unique_lock lock(mtx);
        cv.wait(lock, [this]{ return ready; });
        ready = false;
    }

    bool wait(std::stop_token stoken)
    {
        std::unique_lock lock(mtx);
        if (!cv.wait(lock, stoken, [this]{ return ready; }))
            return false;
        ready = false;
        return true;
    }

    void setFlag(const bool value)
    {
        flag = value;
    }

    bool getFlag() const
    {
        return flag;
    }

private:
    std::mutex mtx;
    std::condition_variable_any cv;
    bool ready = false;
    std::atomic<bool> flag = false;
};

}  // JimmyPaputto

#endif  // JIMMY_PAPUTTO_NOTIFIER_HPP_
