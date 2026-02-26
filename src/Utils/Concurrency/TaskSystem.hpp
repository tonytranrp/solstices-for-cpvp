#pragma once

#include <algorithm>
#include <cstddef>
#include <future>
#include <memory>
#include <mutex>
#include <thread>
#include <utility>

#include <taskflow/taskflow.hpp>
#include <taskflow/algorithm/for_each.hpp>

class TaskSystem {
public:
    static tf::Executor& executor()
    {
        std::scoped_lock lock(executorMutex());
        auto& instance = executorInstance();
        if (!instance)
        {
            instance = std::make_unique<tf::Executor>(resolveWorkerCount());
        }

        return *instance;
    }

    static std::size_t workerCount()
    {
        return static_cast<std::size_t>(executor().num_workers());
    }

    static void waitForAll()
    {
        std::scoped_lock lock(executorMutex());
        auto& instance = executorInstance();
        if (instance)
        {
            instance->wait_for_all();
        }
    }

    static void shutdown()
    {
        std::scoped_lock lock(executorMutex());
        auto& instance = executorInstance();
        if (instance)
        {
            instance->wait_for_all();
            instance.reset();
        }
    }

    template <typename Callable>
    static auto enqueue(Callable&& callable)
    {
        return executor().async(std::forward<Callable>(callable));
    }

    template <typename Index, typename Callable>
    static void parallelForIndex(Index begin, Index end, Callable&& callable)
    {
        if (begin >= end) {
            return;
        }

        tf::Taskflow taskflow("TaskSystem.parallelForIndex");
        taskflow.for_each_index(begin, end, static_cast<Index>(1), std::forward<Callable>(callable));
        executor().run(taskflow).wait();
    }

private:
    static std::mutex& executorMutex()
    {
        static std::mutex sMutex;
        return sMutex;
    }

    static std::unique_ptr<tf::Executor>& executorInstance()
    {
        static std::unique_ptr<tf::Executor> sExecutor = std::make_unique<tf::Executor>(resolveWorkerCount());
        return sExecutor;
    }

    static std::size_t resolveWorkerCount()
    {
        const auto concurrency = std::thread::hardware_concurrency();
        return std::max<std::size_t>(1, static_cast<std::size_t>(concurrency));
    }
};
