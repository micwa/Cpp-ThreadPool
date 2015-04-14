#include <iostream>
#include <future>
#include <thread>
#include <vector>
#include <chrono>
#include <utility>
#include <cstdio>
#include <cassert>

#include "ThreadPool.h"

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
    ThreadPool<int(int), int> pool(2, 2);

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

void testPerformance()
{
    clock_t t = clock();
    future<int> fut[100];

    ThreadPool<int(int), int> pool(4, 4);

    for (int i = 0; i < 100; ++i)
        fut[i] = pool.submit(expensiveComputation, i);
    for (int i = 0; i < 100; ++i)
        printf("Case #%d: %d\n", i + 1, fut[i].get());

    pool.wait();
    t = clock() - t;
    printf("testPerformance() with ThreadPool took %ld cycles (%f seconds)\n", t, ((float)t) / CLOCKS_PER_SEC);
    pool.shutdown();
    assert(pool.isShutdown() && pool.isTerminated());

    // Async
    t = clock();

    for (int i = 0; i < 100; ++i)
        fut[i] = async(expensiveComputation, i);
    for (int i = 0; i < 100; ++i)
        printf("Case #%d: %d\n", i + 1, fut[i].get());

    t = clock() - t;
    printf("testPerformance() with async() took %ld cycles (%f seconds)\n", t, ((float)t) / CLOCKS_PER_SEC);

    // Regular
    t = clock();

    for (int i = 0; i < 100; ++i)
    {
        int res = expensiveComputation(i);
        printf("Case #%d: %d\n", i + 1, res);
    }

    t = clock() - t;
    printf("testPerformance() with async() took %ld cycles (%f seconds)\n", t, ((float)t) / CLOCKS_PER_SEC);
}

int main()
{
    testWorkerPull();
    testPerformance();

    cout << "----- All tests finished" << endl;
    
    return 0;
}