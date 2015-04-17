#include <iostream>
#include <future>
#include <thread>
#include <vector>
#include <chrono>
#include <utility>
#include <cstdio>
#include <cassert>
#include <string>

#include "../ThreadPool.h"

using namespace std;

int printSleepPrint(int secs)
{
    cout << "Sleeping for: " << secs << endl;
    this_thread::sleep_for(chrono::seconds(secs));
    cout << "Woke up after: " << secs << endl;
    return 5;
}

void testWorkerPull()
{
    ThreadPool<int(int), int> pool(2);

    pool.execute(printSleepPrint, 7);       // Should complete after the others
    pool.execute(printSleepPrint, 2);
    pool.execute(printSleepPrint, 1);
    pool.execute(printSleepPrint, 3);

    pool.wait();
    assert(pool.activeThreads() == 0);
}

int expensiveComputation(unsigned x)
{
    int pp[100];
    for (int i = 0; i < 5000; ++i)
        for (int j = 0; j < 7000; ++j)
        {
        if (i * j % 3 == 0)
            pp[i % 100] += i * x;
        else
            pp[i % 100] *= j * x;
        }
    for (int i = 0; i < 100; ++i)
        x += pp[i];

    return x;
}

int expensiveTask(int x)
{
    long long res = expensiveComputation(x);
    string s("expensive");
    for (int i = 0; i < res % 3829381; ++i)
        s[(i * 7) % s.length()] = 'a' + (i * res % 26);
    cout << "Here is your result: " << s << endl;

    return 0;
}

void testPerformance(int n)
{
    using namespace std::chrono;
    future<int> fut[1000];

    clock_t t = clock();
    auto t1 = chrono::high_resolution_clock::now();
    duration<double> dur;

    ThreadPool<int(int), int> pool(4);

    for (int i = 0; i < n; ++i)
        fut[i] = pool.submit(expensiveComputation, i);
    for (int i = 0; i < n; ++i)
        printf("Case #%d: %d\n", i + 1, fut[i].get());

    // pool.wait();
    t = clock() - t;
    auto t2 = chrono::high_resolution_clock::now();
    dur = duration_cast<duration<double>>(t2 - t1);
    printf("testPerformance() with ThreadPool took %ld cycles (%f seconds)\n", t, dur.count());

    pool.shutdown();
    assert(pool.isShutdown() && pool.isTerminated());

    // Async
    t = clock();
    t1 = chrono::high_resolution_clock::now();

    for (int i = 0; i < n; ++i)
        fut[i] = async(launch::async, expensiveComputation, i);
    for (int i = 0; i < n; ++i)
        printf("Case #%d: %d\n", i + 1, fut[i].get());

    t = clock() - t;
    t2 = chrono::high_resolution_clock::now();
    dur = duration_cast<duration<double>>(t2 - t1);
    printf("testPerformance() with async() took %ld cycles (%f seconds)\n", t, dur.count());

    // Regular
    /*
    t = clock();
    t1 = chrono::high_resolution_clock::now();

    for (int i = 0; i < n; ++i)
    {
        int res = expensiveComputation(i);
        printf("Case #%d: %d\n", i + 1, res);
    }

    t = clock() - t;
    t2 = chrono::high_resolution_clock::now();
    dur = duration_cast<duration<double>>(t2 - t1);
    printf("testPerformance() with a single thread took %ld cycles (%f seconds)\n", t, dur.count());
    */
}

void testExecute(int n)
{
    using namespace std::chrono;
    future<int> fut[1000];

    clock_t t = clock();
    auto t1 = chrono::high_resolution_clock::now();
    duration<double> dur;

    ThreadPool<int(int), int> pool(4);

    for (int i = 0; i < n; ++i)
        pool.execute(expensiveTask, i);

    pool.wait();
    t = clock() - t;
    auto t2 = chrono::high_resolution_clock::now();
    dur = duration_cast<duration<double>>(t2 - t1);
    printf("testExecute() with ThreadPool took %ld cycles (%f seconds)\n", t, dur.count());

    pool.shutdown();
    assert(pool.isShutdown() && pool.isTerminated());

    // Async
    t = clock();
    t1 = chrono::high_resolution_clock::now();

    for (int i = 0; i < n; ++i)
        fut[i] = async(launch::async, expensiveTask, i);
    for (int i = 0; i < n; ++i)
        fut[i].get();

    t = clock() - t;
    t2 = chrono::high_resolution_clock::now();
    dur = duration_cast<duration<double>>(t2 - t1);
    printf("testExecute() with async() took %ld cycles (%f seconds)\n", t, dur.count());
}

void thisDoesntCompileOnWindows()
{
#ifndef _WIN32
    ThreadPool<void()> pool(2);
    pool.execute([]() {
        cout << "hello" << endl;
    });
    pool.execute([]() {
        cout << "world" << endl;
    });
    pool.wait();
#endif
}

int main()
{
    // testWorkerPull();
    testPerformance(100);
    // testPerformance(500);
    testExecute(100);
    thisDoesntCompileOnWindows();

    cout << "----- All tests finished" << endl;
    
    return 0;
}
