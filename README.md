## ThreadPool

A thread pool implementation that uses `std::thread`s to perform multiple
actions asynchronously (C++11 or newer is required). Simply include the header
"ThreadPool.h" and declare a ThreadPool with `ThreadPool<fnType, args...>` to
get started (see Examples).  You can download all three files separately
(ThreadPool.h, Task.h, seq.h) or use the ThreadPool.h in the "single-header"
directory.

Tasks can be enqueued using either `ThreadPool::execute()` or `ThreadPool::submit()`.
`ThreadPool::execute()` adds a task (i.e., a function pointer along with arguments)
to the thread pool's internal queue, and executes it. The return value (if any)
of the executed function is discarded. `ThreadPool::submit()` does the same,
except it returns an `std::future<returnType>` that represents the result of
the enqueued task. Calling `get()` on the `std::future` object will block until
the corresponding task completes (as usual).

ThreadPools also can be waited on and shutdown. Calling `wait()` blocks the
current thread until all tasks (running and in the queue) are completed.
ThreadPool's constructor takes a bool (default = true) specifying whether it
should `wait()` when its destructor is called. Otherwise, its destructor will
only call `shutdown()`. Shutting down a ThreadPool causes it to ignore further
calls to `execute()` and `submit()`, and complete only currently running tasks.
If the force option to `shutdown()` is set, it also detach()es all its threads.

Known issues:
* on MSVC2012/2013, `std::packaged_task<void(Args...)>` causes a compilation
  error so you can not declare ThreadPools with a function returning void,
  e.g. `ThreadPool<void(int), int>` is invalid

### Examples

A ThreadPool with function type/argument(s) as template parameters:

    int print(short s) { std::cout << s << std::endl; return 0; }
    ...
    ThreadPool<int(short), short> pool(2, false);
    std::future<int> fut;
    
    pool.execute(print, 42);
    fut = pool.submit(print, 57);
    pool.wait();                    // Since waitOnDestroy == false
    std::cout << fut.get() << std::endl;

You can use lambdas to prevent long ThreadPool declarations (and also execute
any kind of function):

    ThreadPool<void()> pool(2);     // Warning: doesn't work on MSVC2012/2013
    pool.execute([](std::string s) {
        cout << "hello " << s;
    }, "bob");
    pool.execute([]() {
        cout << "world" << endl;
        return 0;
    });
    // Automatically calls wait() when pool is destructed

### License

Copyright (c) 2015 by Michael Wang

This software is released under the MIT license.
