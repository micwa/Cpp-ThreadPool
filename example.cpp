#include <iostream>
#include <future>
#include <thread>
#include <vector>
#include <chrono>
#include <utility>
#include <string>

#include "ThreadPool.h"

using namespace std;

int gvar;

void printAndIncrementGvar(int secs)
{
    cout << "Sleeping for: " << secs << endl;
    this_thread::sleep_for(chrono::seconds(secs));
    cout << "gvar is: " << gvar << endl;
    gvar++;
}

int sleepyMultiplication(int x, int y)
{
    // Note that this is not synced
    static int times = 1;
    cout << "Multiplication " << times++ << " feels sleepy..." << endl;
    this_thread::sleep_for(chrono::seconds(2));
    return x * y;
}

void example()
{
    cout << "\n----- A basic example" << endl;
    ThreadPool<int(int, int), int, int> pool(2);
    future<int> fut[4];

    for (int i = 0; i < 4; ++i)
        fut[i] = pool.submit(sleepyMultiplication, i + 1, i + 2);

    // If wait() is not specified, the first two will be printed out immediately
    // (as opposed to when all 4 computations are finished)
    pool.wait();
    for (int i = 0; i < 4; ++i)
        cout << "The result of computation #" << i + 1 << " is " << fut[i].get() << endl;

    pool.execute([](int, int) {
        cout << "Did you say I have to take two arguments?" << endl;
        return 0;
    }, 0, 0);
}

void exampleWindows()
{
    cout << "\n----- Wrapping functions in lambdas for MSVC2013 (which does not support std::packaged_task<void()>)" << endl;
    ThreadPool<int(int), int> pool(1);
    gvar = 1;

    // Wrap printAndIncrementGvar(int) in a lambda
    for (int i = 0; i < 5; ++i)
    {
        pool.execute([](int x) -> int {
            printAndIncrementGvar(x);
            return 0;
        }, i);
    }
    pool.wait();
}

void exampleLambdaEverything()
{
#ifndef _WIN32
    cout << "\n----- A \"general use\" ThreadPool, where everything is wrapped in a lamdba" << endl;
    ThreadPool<void()> pool(1);
    gvar = 1;

    for (int i = 0; i < 2; ++i)
        pool.execute([=]() { printAndIncrementGvar(i); });

    pool.execute([]() {
        cout << "hello ";
    });
    pool.execute([]() {
        cout << "world" << endl;
    });
    pool.wait();
#endif
}

int main()
{
    example();
    exampleWindows();
    exampleLambdaEverything();

    return 0;
}
