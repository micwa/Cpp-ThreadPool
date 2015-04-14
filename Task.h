#include <future>
#include <algorithm>
#include <utility>
#include <type_traits>

#include "seq.h"

template <class FunctionType, class... Args>
class Task
{
private:
    std::packaged_task<FunctionType> task_;
    std::tuple<Args...> args_;

    template<int... Nums>
    void executeActually(Sequence<Nums...> seq);
public:
    Task();

    template <class Fn, class... DeducedArgs>
    Task(Fn&& fn, DeducedArgs&&... args);

    Task(const Task& other) = delete;
    Task& operator=(Task& other) = delete;

    Task(Task&& other);
    Task& operator=(Task&& other);

    void execute();
    
    template <class Fn>
    std::future<typename std::result_of<Fn(Args...)>::type> getFuture(Fn&& fn);
};

template <class FunctionType, class... Args>
Task<FunctionType, Args...>::Task()
{}

template <class FunctionType, class... Args>
template <class Fn, class... DeducedArgs>
Task<FunctionType, Args...>::Task(Fn&& fn, DeducedArgs&&... args)
    : task_(std::forward<Fn>(fn))
    , args_(std::forward<Args>(args)...)
{}

template <class FunctionType, class... Args>
Task<FunctionType, Args...>::Task(Task&& other)
    : task_(std::move(other.task_))
    , args_(std::move(other.args_))
{}

template <class FunctionType, class... Args>
Task<FunctionType, Args...>& Task<FunctionType, Args...>::operator=(Task&& other)
{
    task_ = std::move(other.task_);
    args_ = std::move(other.args_);
    return *this;
}

template <class FunctionType, class... Args>
void Task<FunctionType, Args...>::execute()
{
    executeActually(typename IndexSequence<sizeof...(Args)>::seq());
}

template <class FunctionType, class... Args>
template <class Fn>
std::future<typename std::result_of<Fn(Args...)>::type>
Task<FunctionType, Args...>::getFuture(Fn&& fn)
{
    return std::move(task_.get_future());
}

template <class FunctionType, class... Args>
template <int... Nums>
void Task<FunctionType, Args...>::executeActually(Sequence<Nums...> seq)
{
     task_(std::get<Nums>(args_)...);
}