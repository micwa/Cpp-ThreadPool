## ThreadPool

A thread pool implementation that uses `std::thread`s to perform multiple
actions asynchronously. `ThreadPool`s are constructed with a fixed number of
threads and a function type (see Examples).

Tasks can be enqueued using either `ThreadPool::execute()` or `ThreadPool::submit()`.
`ThreadPool::execute()` adds a task (i.e., a function pointer along with arguments)
to the thread pool's internal queue, and executes it. The return value (if any)
of the executed function is discarded. `ThreadPool::submit()` does the same,
except it returns an `std::future<retType>` that represents the result of the
enqueued task. As one would expect, calling `get()` on the `std::future` object
will block until the corresponding task completes.

ThreadPools also can be waited on and shutdown. Calling `wait()` blocks the
current thread until all tasks (running and in the queue) are completed.
ThreadPool's constructor takes a bool (default = true) specifying whether it
should `wait()` when its destructor is called. Otherwise, its destructor will
only call `shutdown()`. Shutting down a ThreadPool causes it to ignore further
calls to `execute()` and `submit()`, and complete only currently running tasks.
If the force option to `shutdown()` is set, it also detach()es all its threads.

Known issues:
* on MSVC2012/2013, `std::packaged_task<void(Args...)>` causes a compile error
  so you can not declare ThreadPools with a function returning void, e.g.
  `ThreadPool<void(int), int>` is invalid

### Examples

### License

Copyright (c) 2015 by Michael Wang

This software is released under the MIT license.