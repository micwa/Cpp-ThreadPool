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

/*
 * A thread pool class that uses std::threads to perform multiple tasks asynchronously
 * (functions must have the same type and arguments, however; lambdas do help).
 * Tasks are enqueued using either ThreadPool::execute() or ThreadPool()::submit().
 * ThreadPool::execute() enqueues tasks and discards their return value (if any).
 * ThreadPool::submit() enqueues tasks and returns a std::future that represents
 * the result of the task, which becomes available after the task executes.
 *
 * ThreadPools can be constructed with a maximum number of threads, and can be
 * shutdown. Shutting down a ThreadPool causes it to ignore further calls to
 * execute() and submit(), but it will continue executing tasks in its queue.
 * Shutting down a ThreadPool forcefully (force = True) causes all it to detach
 * all its threads and no longer execute any tasks.
 *
 * Known issues:
 *  - on MSVC2012/2013, packaged_task<void(Args...)> causes a compile error so you
 *    can not declare ThreadPools with a function returning void, e.g.
 *    ThreadPool<void(int), int> is invalid
 */
template <class FunctionType, class... Args>
class ThreadPool
{
private:
    static const int WAIT_TIME;             // Milliseconds between sleeping for wait()

    std::vector<std::thread> threads_;
    std::queue<Task<FunctionType, Args...>> tasks_;
    std::mutex lock_;
    std::condition_variable taskAvailable_;
    int activeThreads_;
    bool isShutdown_, isForced_;
    
    void doWork(int id);
    void notifyNewTask();
public:
    ThreadPool(int numThreads);
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
const int ThreadPool<FunctionType, Args...>::WAIT_TIME = 25;

// Constructs a ThreadPool with the specified amount of initialThreads (must be nonnegative).
// If initialThreads > REAL_MAX_THREADS, then REAL_MAX_THREADS are created.
template <class FunctionType, class... Args>
ThreadPool<FunctionType, Args...>::ThreadPool(int numThreads)
    : threads_(numThreads)
    , activeThreads_(0)
    , isShutdown_(false)
    , isForced_(false)
{
    for (int i = 0; i != threads_.size(); ++i)
        threads_[i] = std::move(std::thread(&ThreadPool::doWork, this, i + 1));
}

// Join all uncompleted threads if this ThreadPool hasn't been forcefully shut down
template <class FunctionType, class... Args>
ThreadPool<FunctionType, Args...>::~ThreadPool()
{
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

    std::lock_guard<std::mutex> lg(lock_);
    Task<FunctionType, Args...> task(std::forward<Fn>(fn), std::forward<Args>(args)...);
    std::future<retType> fut = std::move(task.getFuture());
    tasks_.push(std::move(task));

    notifyNewTask();
    
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
    while (activeThreads_ != 0 || !tasks_.empty())
        std::this_thread::sleep_for(std::chrono::milliseconds(WAIT_TIME));
}

// If there are thread(s) that are not busy, notifies one of them that a new task has
// been enqueued.
template <class FunctionType, class... Args>
inline
void ThreadPool<FunctionType, Args...>::notifyNewTask()
{
    if (threads_.size() != activeThreads_)
        taskAvailable_.notify_one();
}

template <class FunctionType, class... Args>
void ThreadPool<FunctionType, Args...>::doWork(int id)
{
    // Dequeue tasks until the thread pool is shutdown
    bool alreadyExecuted = false;
    while (!isShutdown_)
    {
        Task<FunctionType, Args...> task;
        {
            std::unique_lock<std::mutex> ul(lock_);
            
            // In case somehow this thread pauses during the loop check/construction of task (and gets pre-notified)
            if (alreadyExecuted)
                --activeThreads_;

            while (tasks_.empty() && !isShutdown_)
            {
                taskAvailable_.wait(ul);
            }
            if (isShutdown_)
                break;
            task = std::move(tasks_.front());
            tasks_.pop();

            ++activeThreads_;
        }   // So lock_ is unlocked after activeThreads_ increments/a task is *definitely* pulled
        
        task.execute();
        alreadyExecuted = true;
    }
}

#endif /* THREAD_POOL_H */
