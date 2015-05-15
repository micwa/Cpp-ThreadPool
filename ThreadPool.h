#ifndef THREAD_POOL_H_
#define THREAD_POOL_H_

#include <vector>
#include <queue>
#include <future>
#include <mutex>
#include <condition_variable>
#include <algorithm>
#include <utility>
#include <type_traits>

#include "Task.h"

/**
 * Copyright (c) 2015 by Michael Wang
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */
template <class FunctionType, class... Args>
class ThreadPool
{
private:
    static const int MAX_WAIT_TIME;         // Maximum time (in milliseconds) to sleep for in wait()

    std::vector<std::thread> threads_;
    std::queue<Task<FunctionType, Args...>> tasks_;
    std::mutex lock_;
    std::condition_variable taskAvailable_;
    int activeThreads_;
    bool isShutdown_, isForced_, waitOnDestroy_;
    
    void doWork(int id);
public:
    ThreadPool(int numThreads, bool waitOnDestroy = true);
    ~ThreadPool();
    
    int activeThreads() const;
    int threadCount() const;
    bool isShutdown() const;
    bool isTerminated() const;
    
    void shutdown(bool force = false);
    void wait();
    
    template <class Fn, class... DeducedArgs>
    void execute(Fn&& fn, DeducedArgs&&... args);
    
    template <class Fn, class... DeducedArgs>
    std::future<typename std::result_of<Fn(Args...)>::type>
    submit(Fn&& fn, DeducedArgs&&... args);
};

template <class FunctionType, class... Args>
const int ThreadPool<FunctionType, Args...>::MAX_WAIT_TIME = 50;

// Constructs a ThreadPool with the specified amount of threads (must be nonnegative).
// If waitOnDestroy is true, the ThreadPool will call wait() on destruction; otherwise, it will not.
template <class FunctionType, class... Args>
ThreadPool<FunctionType, Args...>::ThreadPool(int numThreads, bool waitOnDestroy)
    : threads_(numThreads)
    , activeThreads_(0)
    , isShutdown_(false)
    , isForced_(false)
    , waitOnDestroy_(waitOnDestroy)
{
    for (int i = 0; i != threads_.size(); ++i)
        threads_[i] = std::move(std::thread(&ThreadPool::doWork, this, i + 1));
}

// Join all uncompleted threads if this ThreadPool hasn't been forcefully shut down
template <class FunctionType, class... Args>
ThreadPool<FunctionType, Args...>::~ThreadPool()
{
    if (waitOnDestroy_)
        wait();
    if (!isShutdown_)
        shutdown(false);

    if (!isForced_)
        for (int i = 0; i != threads_.size(); ++i)
            threads_[i].join();
}

template <class FunctionType, class... Args>
inline
int ThreadPool<FunctionType, Args...>::activeThreads() const { return activeThreads_; }

template <class FunctionType, class... Args>
inline
int ThreadPool<FunctionType, Args...>::threadCount() const { return threads_.size(); }

template <class FunctionType, class... Args>
inline
bool ThreadPool<FunctionType, Args...>::isShutdown() const { return isShutdown_; }

template <class FunctionType, class... Args>
inline
bool ThreadPool<FunctionType, Args...>::isTerminated() const { return isShutdown_ && activeThreads_ == 0; }

template <class FunctionType, class... Args>
template <class Fn, class... DeducedArgs>
inline
void ThreadPool<FunctionType, Args...>::execute(Fn&& fn, DeducedArgs&&... args)
{
    submit(std::forward<Fn>(fn), std::forward<Args>(args)...);
}

// DeducedArgs must have the same (decayed) type as Args; its purpose is to force
// deduction of template arguments, because Args, if used, would be already specified.
template <class FunctionType, class... Args>
template <class Fn, class... DeducedArgs>
std::future<typename std::result_of<Fn(Args...)>::type>
ThreadPool<FunctionType, Args...>::submit(Fn&& fn, DeducedArgs&&... args)
{
    using retType = typename std::result_of<Fn(Args...)>::type;

    if (isShutdown_)
        return std::future<retType>();

    Task<FunctionType, Args...> task(std::forward<Fn>(fn), std::forward<Args>(args)...);
    std::future<retType> fut = std::move(task.getFuture());

    std::unique_lock<std::mutex> ul(lock_);
    tasks_.push(std::move(task));
    ul.unlock();

    taskAvailable_.notify_one();
    
    return std::move(fut);
}

// Signal to threads that they should finish what they're doing. If force == true,
// all threads in this ThreadPool will be detached (and can be safely destructed).
// (This does not block the calling thread)
template <class FunctionType, class... Args>
void ThreadPool<FunctionType, Args...>::shutdown(bool force)
{
    if (isShutdown_)
        return;

    isForced_ = force;
    isShutdown_ = true;

    taskAvailable_.notify_all();

    if (force)
        for (int i = 0; i != threads_.size(); ++i)
            threads_[i].detach();
}

// Wait for all tasks (enqueued and currently running) to complete. This function,
// as opposed to shutdown(), *does* block the calling thread by polling until the
// task queue is empty.
// That is, after wait() returns, activeThreads() == 0.
template <class FunctionType, class... Args>
void ThreadPool<FunctionType, Args...>::wait()
{
    const int multiplier = 2;
    int waitTime = 1;

    while (activeThreads_ != 0 || !tasks_.empty())
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(waitTime));
        waitTime = std::min(MAX_WAIT_TIME, multiplier * waitTime);
    }
}

template <class FunctionType, class... Args>
void ThreadPool<FunctionType, Args...>::doWork(int id)
{
    // Dequeue tasks until the thread pool is shutdown
    while (!isShutdown_)
    {
        Task<FunctionType, Args...> task;
        {
            std::unique_lock<std::mutex> ul(lock_);

            while (tasks_.empty() && !isShutdown_)
                taskAvailable_.wait(ul);
            if (isShutdown_)
                break;

            ++activeThreads_;
            task = std::move(tasks_.front());
            tasks_.pop();
        }   // So lock_ is unlocked after activeThreads_ increments/a task is *definitely* pulled
        
        task.execute();
        --activeThreads_;
    }
}

#endif /* THREAD_POOL_H */
