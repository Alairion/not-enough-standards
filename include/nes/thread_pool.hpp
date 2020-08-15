///////////////////////////////////////////////////////////
/// Copyright 2020 Alexy Pellegrini
///
/// Permission is hereby granted, free of charge,
/// to any person obtaining a copy of this software
/// and associated documentation files (the "Software"),
/// to deal in the Software without restriction,
/// including without limitation the rights to use,
/// copy, modify, merge, publish, distribute, sublicense,
/// and/or sell copies of the Software, and to permit
/// persons to whom the Software is furnished to do so,
/// subject to the following conditions:
///
/// The above copyright notice and this permission notice
/// shall be included in all copies or substantial portions
/// of the Software.
///
/// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF
/// ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED
/// TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A
/// PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT
/// SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR
/// ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
/// ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
/// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE
/// OR OTHER DEALINGS IN THE SOFTWARE.
///////////////////////////////////////////////////////////

#ifndef NOT_ENOUGH_STANDARDS_THREAD_POOL
#define NOT_ENOUGH_STANDARDS_THREAD_POOL

#include <vector>
#include <thread>
#include <future>
#include <condition_variable>
#include <mutex>
#include <functional>
#include <iostream>

/*
C++17 workaround:

auto task = [promise = std::move(promise), func = std::forward<Func>(func), args = std::make_tuple(std::forward<Args>(args)...)]() mutable
{
    promise.set_value(std::apply(std::forward<Func>(func), std::move(args)));
};
*/

namespace nes
{

enum class thread_pool_options : std::uint32_t
{
    none = 0x00,
    fifo = 0x01
};

constexpr thread_pool_options operator&(thread_pool_options left, thread_pool_options right) noexcept
{
    return static_cast<thread_pool_options>(static_cast<std::uint32_t>(left) & static_cast<std::uint32_t>(right));
}

constexpr thread_pool_options& operator&=(thread_pool_options& left, thread_pool_options right) noexcept
{
    left = left & right;
    return left;
}

constexpr thread_pool_options operator|(thread_pool_options left, thread_pool_options right) noexcept
{
    return static_cast<thread_pool_options>(static_cast<std::uint32_t>(left) | static_cast<std::uint32_t>(right));
}

constexpr thread_pool_options& operator|=(thread_pool_options& left, thread_pool_options right) noexcept
{
    left = left | right;
    return left;
}

constexpr thread_pool_options operator^(thread_pool_options left, thread_pool_options right) noexcept
{
    return static_cast<thread_pool_options>(static_cast<std::uint32_t>(left) ^ static_cast<std::uint32_t>(right));
}

constexpr thread_pool_options& operator^=(thread_pool_options& left, thread_pool_options right) noexcept
{
    left = left ^ right;
    return left;
}

constexpr thread_pool_options operator~(thread_pool_options value) noexcept
{
    return static_cast<thread_pool_options>(~static_cast<std::uint32_t>(value));
}

class thread_pool
{
    //Base class for all task types
    struct task_holder_base
    {
        task_holder_base() = default;
        virtual ~task_holder_base() = default;
        task_holder_base(const task_holder_base&) = delete;
        task_holder_base& operator=(const task_holder_base&) = delete;
        task_holder_base(task_holder_base&&) = delete;
        task_holder_base& operator=(task_holder_base&&) = delete;

        virtual void execute() = 0;
    };

    //One shot task
    template<typename Func>
    class task_holder final : public task_holder_base
    {
    public:
        task_holder(Func&& func)
        :m_func{std::move(func)}
        {

        }

        ~task_holder() = default;
        task_holder(const task_holder&) = delete;
        task_holder& operator=(const task_holder&) = delete;
        task_holder(task_holder&&) = delete;
        task_holder& operator=(task_holder&&) = delete;

        void execute() override
        {
            m_func();
        }

    private:
        Func m_func;
    };

    //Chained task
    struct chained_task_node : public task_holder_base
    {
        chained_task_node() = default;
        virtual ~chained_task_node() = default;
        chained_task_node(const chained_task_node&) = delete;
        chained_task_node& operator=(const chained_task_node&) = delete;
        chained_task_node(chained_task_node&&) = delete;
        chained_task_node& operator=(chained_task_node&&) = delete;

        std::unique_ptr<chained_task_node> next{};
    };

    template<typename Func>
    class chained_task_holder final : public chained_task_node
    {
    public:
        chained_task_holder(Func&& func)
        :m_func{std::move(func)}
        {

        }

        ~chained_task_holder() = default;
        chained_task_holder(const chained_task_holder&) = delete;
        chained_task_holder& operator=(const chained_task_holder&) = delete;
        chained_task_holder(chained_task_holder&&) = delete;
        chained_task_holder& operator=(chained_task_holder&&) = delete;

        void execute() override
        {
            m_func();

            if(next)
            {
                next->execute();
            }
        }

    private:
        Func m_func;
    };

    //Only type stored in pool
    class task
    {
        friend class chain_builder;

    public:
        template<typename Func>
        task(Func&& func)
        :m_holder{std::make_unique<task_holder<Func>>(std::forward<Func>(func))}
        {

        }

        task(std::unique_ptr<task_holder_base> holder)
        :m_holder{std::move(holder)}
        {

        }

        ~task() = default;
        task(const task&) = delete;
        task& operator=(const task&) = delete;
        task(task&&) = default;
        task& operator=(task&&) = default;

        void execute()
        {
            m_holder->execute();
        }

    private:
        std::unique_ptr<task_holder_base> m_holder{};
    };

public:
    explicit thread_pool(std::size_t thread_count = std::thread::hardware_concurrency(), thread_pool_options options = thread_pool_options::none)
    :m_options{options}
    {
        const auto worker_base = [this]()
        {
            while(true)
            {
                std::unique_lock lock{m_mutex};
                m_condition.wait(lock, [this]
                {
                    return !m_running || !std::empty(m_queue);
                });

                if(!m_running && std::empty(m_queue))
                {
                    break;
                }

                task task{std::move(m_queue.back())};
                m_queue.pop_back();

                lock.unlock();
                task.execute();
            }
        };

        m_threads.reserve(thread_count);
        for(std::size_t i{}; i < thread_count; ++i)
        {
            m_threads.emplace_back(worker_base);
        }
    }

    ~thread_pool()
    {
        std::unique_lock lock{m_mutex};
        m_running = false;
        lock.unlock();

        m_condition.notify_all();

        for(auto&& thread : m_threads)
        {
            if(thread.joinable())
            {
                thread.join();
            }
        }
    }

    thread_pool(const thread_pool&) = delete;
    thread_pool& operator=(const thread_pool&) = delete;
    thread_pool(thread_pool&&) = delete;
    thread_pool& operator=(thread_pool&&) = delete;

    template<typename Func, typename... Args>
    [[nodiscard]] auto push(Func&& func, Args&&... args)
    {
        using return_type = std::invoke_result_t<Func, Args...>;
        using promise_type = std::promise<return_type>;
        using future_type = std::future<return_type>;

        promise_type promise{};
        future_type future{promise.get_future()};

        #if __cplusplus > 201703L
        if constexpr(std::is_same_v<return_type, void>)
        {
            push_impl([promise = std::move(promise), func = std::forward<Func>(func), ...args = std::forward<Args>(args)]() mutable
            {
                std::invoke(std::forward<Func>(func), std::forward<Args>(args)...);
                promise.set_value();
            });
        }
        else
        {
            push_impl([promise = std::move(promise), func = std::forward<Func>(func), ...args = std::forward<Args>(args)]() mutable
            {
                promise.set_value(std::invoke(std::forward<Func>(func), std::forward<Args>(args)...));
            });
        }
        #else
        if constexpr(std::is_same_v<return_type, void>)
        {
            push_impl([promise = std::move(promise), func = std::forward<Func>(func), args = std::make_tuple(std::forward<Args>(args)...)]() mutable
            {
                std::apply(std::forward<Func>(func), std::move(args));
                promise.set_value();
            });
        }
        else
        {
            push_impl([promise = std::move(promise), func = std::forward<Func>(func), args = std::make_tuple(std::forward<Args>(args)...)]() mutable
            {
                promise.set_value(std::apply(std::forward<Func>(func), std::move(args)));
            });
        }
        #endif

        m_condition.notify_one();

        return future;
    }

public:
    class chain_builder
    {
    public:
        template<typename Func>
        explicit chain_builder(thread_pool& pool, Func&& func)
        :m_pool{&pool}
        {
            m_head = std::make_unique<chained_task_holder<Func>>(std::forward<Func>(func));
            m_current = m_head.get();
        }

        ~chain_builder() = default;
        chain_builder(const chain_builder&) = delete;
        chain_builder& operator=(const chain_builder&) = delete;
        chain_builder(chain_builder&&) = default;
        chain_builder& operator=(chain_builder&&) = default;

        template<typename Func, typename... Args>
        [[nodiscard]] chain_builder& then(Func&& func, Args&&... args)
        {
            #if __cplusplus > 201703L
            auto task = [func = std::forward<Func>(func), ...args = std::forward<Args>(args)]() mutable
            {
                std::invoke(std::forward<Func>(func), std::forward<Args>(args)...);
            };
            #else
            auto task = [func = std::forward<Func>(func), args = std::make_tuple(std::forward<Args>(args)...)] mutable
            {
                std::apply(std::forward<Func>(func), std::move(args));
            };
            #endif

            m_current->next = std::make_unique<chained_task_holder<decltype(task)>>(std::move(task));
            m_current = m_current->next.get();

            return *this;
        }

        [[nodiscard]] std::future<void> push()
        {
            return m_pool->push([task = std::move(m_head)]
            {
                task->execute();
            });
        }

    private:
        thread_pool* m_pool{};
        std::unique_ptr<chained_task_node> m_head{};
        chained_task_node* m_current{};
    };

    template<typename Func, typename... Args>
    [[nodiscard]] auto chain(Func&& func, Args&&... args)
    {
        auto head = [func = std::forward<Func>(func), ...args = std::forward<Args>(args)]() mutable
        {
            std::invoke(std::forward<Func>(func), std::forward<Args>(args)...);
        };

        return chain_builder{*this, std::move(head)};
    }

public:
    void reserve(std::size_t task_count)
    {
        std::lock_guard lock{m_mutex};
        m_queue.reserve(task_count);
    }

    std::size_t thread_count() const noexcept
    {
        return std::size(m_threads);
    }

private:
    void push_impl(task task)
    {
        std::lock_guard lock{m_mutex};

        if(static_cast<bool>(m_options & thread_pool_options::fifo))
        {
            m_queue.insert(std::begin(m_queue), std::move(task));
        }
        else
        {
            m_queue.emplace_back(std::move(task));
        }
    }

private:
    thread_pool_options m_options{};
    std::vector<std::thread> m_threads{};
    std::vector<task> m_queue{};
    std::condition_variable m_condition{};
    std::mutex m_mutex{};
    bool m_running{true};
};

}

#endif
