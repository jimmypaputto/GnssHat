/*
 * Jimmy Paputto 2025
 */

#ifndef JIMMY_PAPUTTO_NOTIFIER_HPP_
#define JIMMY_PAPUTTO_NOTIFIER_HPP_

#include <atomic>
#include <condition_variable>
#include <mutex>


namespace JimmyPaputto
{

class Notifier
{
public:
    void notify()
    {
        std::lock_guard<std::mutex> lock(mtx);
        cv.notify_one();
    }

    void wait()
    {
        std::unique_lock<std::mutex> lock(mtx);
        cv.wait(lock);
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
    std::condition_variable cv;
    std::atomic<bool> flag = false;
};

}  // JimmyPaputto

#endif  // JIMMY_PAPUTTO_NOTIFIER_HPP_
