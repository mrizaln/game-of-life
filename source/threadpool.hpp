#ifndef THREADPOOL_HPP_YWONTBSQ
#define THREADPOOL_HPP_YWONTBSQ

#include <spdlog/spdlog.h>

#include <functional>
#include <future>
#include <thread>
#include <type_traits>
#include <vector>
#include <deque>
#include <mutex>
#include <condition_variable>

class ThreadPool
{
public:
    // NOTE: until C++23's std::move_only_function is available, use this
    // TODO: replace with std::move_only_function if it has come mainstream (at least exist in gcc main release)
    using Task_type = std::function<void()>;

private:
    std::vector<std::jthread> m_threads;
    std::deque<Task_type>     m_tasks;
    mutable std::mutex        m_mutex;
    std::condition_variable   m_condition;
    bool                      m_stop = false;

public:
    ThreadPool(size_t numThreads)
    {
        for (size_t i = 0; i < numThreads; ++i) {
            m_threads.emplace_back([this] { while (true) {
                Task_type task;
                {
                    std::unique_lock lock{ m_mutex };

                    m_condition.wait(lock, [this] {
                        auto condition = !m_tasks.empty() || m_stop;
                        return condition;
                    });

                    if (m_stop && m_tasks.empty()) {
                        return;
                    }

                    task = std::move(m_tasks.front());
                    m_tasks.pop_front();
                }
                task();
            } });
        }
        spdlog::info("(ThreadPool) Pool created with [{}] numbers of thread", numThreads);
    }

    ~ThreadPool()
    {
        spdlog::info("(ThreadPool) Destructor called, there are [{}] tasks left", m_tasks.size());
        stopPool();
    }

    template <typename... Args, std::invocable<Args...> Func>
    [[nodiscard]] auto enqueue(Func&& func, Args&&... args) -> std::future<std::invoke_result_t<Func, Args...>>
    {
        using Task = std::packaged_task<std::invoke_result_t<Func, Args...>()>;

        // because of std::function CopyConstructible requirement, this std::packaged_task can't be inside them.
        // a workaround is to just leave it live in the heap as std::shared_ptr
        auto packagedTask = std::make_shared<Task>(
            [func = std::forward<Func>(func), ... args = std::forward<Args>(args)]() mutable {
                return func(std::forward<Args>(args)...);
            }
        );
        auto res = packagedTask->get_future();
        {
            std::unique_lock lock{ m_mutex };
            m_tasks.emplace_back([task = std::move(packagedTask)]() mutable { (*task)(); });
        }
        m_condition.notify_one();

        return res;
    }

    template <typename... Args, std::invocable<Args...> Func>
        requires std::same_as<std::invoke_result_t<Func, Args...>, void>
    void enqueueDetached(Func&& func, Args&&... args)
    {
        {
            std::unique_lock lock{ m_mutex };
            m_tasks.emplace_back([func = std::forward<Func>(func), ... args = std::forward<Args>(args)]() mutable {
                func(std::forward<Args>(args)...);
            });
        }
        m_condition.notify_one();
    }

    std::size_t queuedTasks() const
    {
        std::unique_lock lock{ m_mutex };
        return m_tasks.size();
    }

    // after this call, the instance will effectively become unusable.
    // create a new instance if you want to use ThreadPool again.
    void stopPool(bool ignoreQueuedTasks = false)
    {
        {
            std::unique_lock lock{ m_mutex };
            if (ignoreQueuedTasks) {
                m_tasks.clear();
            }
            m_stop = true;
        }
        m_condition.notify_all();

        // for some reason, this prevents stray func destructor to be ran
        for (auto& thread : m_threads) {
            if (thread.joinable()) {
                thread.join();
            }
        }
    }

    std::size_t size() const
    {
        // std::unique_lock lock{ m_mutex };    // I guess lock is not actually needed here
        return m_threads.size();
    }
};

#endif /* end of include guard: THREADPOOL_HPP_YWONTBSQ */
