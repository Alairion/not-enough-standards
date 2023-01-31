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
#include <atomic>
#include <thread>
#include <future>
#include <condition_variable>
#include <mutex>
#include <functional>
#include <variant>
#include <cassert>
#include <span>
#include <utility>

namespace nes
{

namespace impl
{

class checkpoint_holder_base
{
public:
    checkpoint_holder_base(bool barrier)
    :m_barrier{barrier}
    {

    }

    virtual ~checkpoint_holder_base() = default;
    checkpoint_holder_base(const checkpoint_holder_base&) = delete;
    checkpoint_holder_base& operator=(const checkpoint_holder_base&) = delete;
    checkpoint_holder_base(checkpoint_holder_base&&) = delete;
    checkpoint_holder_base& operator=(checkpoint_holder_base&&) = delete;

    virtual void set_reset_value(std::size_t value) noexcept = 0;
    virtual void reset() = 0;
    virtual bool count_down() noexcept = 0;
    virtual bool check_barrier() const noexcept = 0;

    bool is_barrier() const noexcept
    {
        return m_barrier;
    }

private:
    bool m_barrier{};
};

template<typename T>
class checkpoint_holder final : public checkpoint_holder_base
{
public:
    explicit checkpoint_holder(bool barrier, std::future<T>*& user_reference)
    :checkpoint_holder_base{barrier}
    {
        user_reference = &m_future;
    }

    ~checkpoint_holder() = default;
    checkpoint_holder(const checkpoint_holder&) = delete;
    checkpoint_holder& operator=(const checkpoint_holder&) = delete;
    checkpoint_holder(checkpoint_holder&&) = delete;
    checkpoint_holder& operator=(checkpoint_holder&&) = delete;

    void set_reset_value(std::size_t value) noexcept override
    {
        m_reset_value = value;
    }

    void reset() override
    {
        m_promise = std::promise<void>{};
        m_future  = m_promise.get_future();

        m_counter.store(m_reset_value, std::memory_order_release);
    }

    bool count_down() noexcept override
    {
        const bool last{--(m_counter) == 0};

        if(last)
        {
            m_promise.set_value();
        }

        return last;
    }

    bool check_barrier() const noexcept override
    {
        return m_counter.load(std::memory_order_acquire) == 1;
    }

private:
    std::promise<T> m_promise{};
    std::future<T>  m_future{};
    std::atomic<std::size_t> m_counter{};
    std::size_t m_reset_value{};
};

class checkpoint
{
public:
    template<typename T>
    checkpoint(bool barrier, std::future<T>*& user_reference)
    :m_checkpoint{std::make_unique<checkpoint_holder<T>>(barrier, user_reference)}
    {

    }

    ~checkpoint() = default;
    checkpoint(const checkpoint&) = delete;
    checkpoint& operator=(const checkpoint&) = delete;
    checkpoint(checkpoint&&) = default;
    checkpoint& operator=(checkpoint&&) = default;

    void set_reset_value(std::size_t value) noexcept
    {
        m_checkpoint->set_reset_value(value);
    }

    void reset()
    {
        m_checkpoint->reset();
    }

    bool count_down() noexcept
    {
        return m_checkpoint->count_down();
    }

    bool check_barrier() const noexcept
    {
        return m_checkpoint->check_barrier();
    }

    bool is_barrier() const noexcept
    {
        return m_checkpoint->is_barrier();
    }

    checkpoint_holder_base* base() const noexcept
    {
        return m_checkpoint.get();
    }

private:
    std::unique_ptr<checkpoint_holder_base> m_checkpoint{};
};

using checkpoint_range = std::span<checkpoint_holder_base*>;

class task_holder_base
{
public:
    task_holder_base() = default;
    virtual ~task_holder_base() = default;
    task_holder_base(const task_holder_base&) = delete;
    task_holder_base& operator=(const task_holder_base&) = delete;
    task_holder_base(task_holder_base&&) = delete;
    task_holder_base& operator=(task_holder_base&&) = delete;

    virtual void reset() = 0;
    virtual void execute() = 0;

    void set_checkpoint_range(checkpoint_range checkpoints)
    {
        m_checkpoints = checkpoints;
    }

protected:
    void trigger_checkpoints()
    {
        for(auto& checkpoint : m_checkpoints)
        {
            checkpoint->count_down();
        }
    }

private:
    checkpoint_range m_checkpoints{};
};

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
        trigger_checkpoints();
    }

    void reset() override
    {

    }

private:
    Func m_func;
};

template<typename Func, typename Ret>
class return_task_holder final : public task_holder_base
{
public:
    return_task_holder(Func&& func, std::future<Ret>*& user_reference)
    :m_func{std::move(func)}
    {
        user_reference = &m_future;
    }

    ~return_task_holder() = default;
    return_task_holder(const return_task_holder&) = delete;
    return_task_holder& operator=(const return_task_holder&) = delete;
    return_task_holder(return_task_holder&&) = delete;
    return_task_holder& operator=(return_task_holder&&) = delete;

    void execute() override
    {
        if constexpr(std::is_same_v<Ret, void>)
        {
            m_func();
            m_promise.set_value();
        }
        else
        {
            m_promise.set_value(m_func());
        }

        trigger_checkpoints();
    }

    void reset() override
    {
        m_promise = std::promise<Ret>{};
        m_future  = m_promise.get_future();
    }

private:
    Func m_func;
    std::promise<Ret> m_promise{};
    std::future<Ret>  m_future{};
};

class task
{
public:
    template<typename Func>
    task(Func&& func)
    :m_holder{std::make_unique<task_holder<Func>>(std::forward<Func>(func))}
    {

    }

    template<typename Func, typename Ret>
    task(Func&& func, std::future<Ret>*& user_reference)
    :m_holder{std::make_unique<return_task_holder<Func, Ret>>(std::forward<Func>(func), user_reference)}
    {

    }

    ~task() = default;
    task(const task&) = delete;
    task& operator=(const task&) = delete;
    task(task&&) = default;
    task& operator=(task&&) = default;

    void set_checkpoint_range(checkpoint_range checkpoints)
    {
        m_holder->set_checkpoint_range(checkpoints);
    }

    void execute()
    {
        m_holder->execute();
    }

    void reset()
    {
        m_holder->reset();
    }

    task_holder_base* holder() const noexcept
    {
        return m_holder.get();
    }

private:
    std::unique_ptr<task_holder_base> m_holder{};
};

class fence_holder
{
public:
    fence_holder() = default;
    ~fence_holder() = default;
    fence_holder(const fence_holder&) = delete;
    fence_holder& operator=(const fence_holder&) = delete;
    fence_holder(fence_holder&&) = delete;
    fence_holder& operator=(fence_holder&&) = delete;

    void set_condition(std::condition_variable& condition) noexcept
    {
        m_condition = &condition;
    }

    void reset() noexcept
    {
        m_signaled.store(false, std::memory_order_release);
    }

    void signal() noexcept
    {
        m_signaled.store(true, std::memory_order_release);
        m_condition->notify_one();
    }

    bool is_signaled() const noexcept
    {
        return m_signaled.load(std::memory_order_acquire);
    }

private:
    std::condition_variable* m_condition{};
    std::atomic<bool> m_signaled{};
};

class fence
{
public:
    fence()
    :m_holder{std::make_unique<fence_holder>()}
    {

    }

    ~fence() = default;
    fence(const fence&) = delete;
    fence& operator=(const fence&) = delete;
    fence(fence&&) = default;
    fence& operator=(fence&&) = default;

    void set_condition(std::condition_variable& condition) noexcept
    {
        m_holder->set_condition(condition);
    }

    void reset() noexcept
    {
        m_holder->reset();
    }

    void signal() noexcept
    {
        m_holder->signal();
    }

    bool is_signaled() const noexcept
    {
        return m_holder->is_signaled();
    }

    fence_holder* holder() const noexcept
    {
        return m_holder.get();
    }

private:
    std::unique_ptr<fence_holder> m_holder{};
};

using task_type = std::variant<checkpoint, task, fence>;

}

template<typename T>
class task_result
{
    friend class task_builder;

public:
    task_result() = default;
    ~task_result() = default;
    task_result(const task_result&) = delete;
    task_result& operator=(const task_result&) = delete;

    task_result(task_result&& other) noexcept
    :m_state{std::exchange(other.m_state, nullptr)}
    {

    }

    task_result& operator=(task_result&& other) noexcept
    {
        m_state = std::exchange(other.m_state, nullptr);

        return *this;
    }

    T get()
    {
        return m_state->get();
    }

    bool valid() const noexcept
    {
        return m_state->valid();
    }

    void wait() const
    {
        m_state->wait();
    }

    template<class Rep, class Period>
    bool wait_for(const std::chrono::duration<Rep, Period>& timeout) const
    {
        return m_state->wait_for(timeout) == std::future_status::ready;
    }

    template<class Rep, class Period>
    bool wait_until(const std::chrono::duration<Rep, Period>& timeout) const
    {
        return m_state->wait_until(timeout) == std::future_status::ready;
    }

private:
    std::future<T>* m_state{};
};

using task_checkpoint = task_result<void>;

class task_fence
{
    friend class task_builder;

public:
    task_fence() = default;
    ~task_fence() = default;
    task_fence(const task_fence&) = delete;
    task_fence& operator=(const task_fence&) = delete;

    task_fence(task_fence&& other) noexcept
    :m_holder{std::exchange(other.m_holder, nullptr)}
    {

    }

    task_fence& operator=(task_fence&& other) noexcept
    {
        m_holder = std::exchange(other.m_holder, nullptr);

        return *this;
    }

    void signal() noexcept
    {
        m_holder->signal();
    }

private:
    impl::fence_holder* m_holder{};
};

class task_list
{
    friend class thread_pool;
    friend class task_builder;

public:
    constexpr task_list() = default;
    ~task_list() = default;
    task_list(const task_list&) = delete;
    task_list& operator=(const task_list&) = delete;
    task_list(task_list&&) = default;
    task_list& operator=(task_list&&) = default;

private:
    void reset(std::condition_variable& condition)
    {
        for(auto& task : m_tasks)
        {
            std::visit([&condition](auto&& task)
            {
                using alternative_type = std::decay_t<decltype(task)>;

                if constexpr(std::is_same_v<alternative_type, impl::fence>)
                {
                    task.set_condition(condition);
                }

                task.reset();

            }, task);
        }

        m_current = std::begin(m_tasks);
    }

    template<typename OutputIt>
    std::pair<bool, std::size_t> next(OutputIt output)
    {
        std::size_t count{};

        while(m_current != std::end(m_tasks))
        {
            if(std::holds_alternative<impl::checkpoint>(*m_current))
            {
                auto& checkpoint{std::get<impl::checkpoint>(*m_current)};

                if(checkpoint.is_barrier() && !checkpoint.check_barrier())
                {
                    return std::make_pair(false, count);
                }

                checkpoint.count_down();
            }
            else if(std::holds_alternative<impl::task>(*m_current))
            {
                auto& task{std::get<impl::task>(*m_current)};

                *output++ = task.holder();
                ++count;
            }
            else
            {
                auto& fence{std::get<impl::fence>(*m_current)};

                if(!fence.is_signaled())
                {
                    return std::make_pair(false, count);
                }
            }

            ++m_current;
        }

        return std::make_pair(true, count);
    }

private:
    std::vector<impl::task_type> m_tasks{};
    std::vector<impl::task_type>::iterator m_current{};
    std::vector<impl::checkpoint_holder_base*> m_checkpoints{};
};

class task_builder
{
public:
    explicit task_builder(std::uint32_t thread_count = std::thread::hardware_concurrency())
    :m_thread_count{thread_count != 0 ? thread_count : 8}
    {
        m_tasks.reserve(32);
    }

    ~task_builder() = default;
    task_builder(const task_builder&) = delete;
    task_builder& operator=(const task_builder&) = delete;
    task_builder(task_builder&&) = default;
    task_builder& operator=(task_builder&&) = default;

    template<typename Func, typename... Args>
    void execute(Func&& func, Args&&... args)
    {
        push_task([func = std::forward<Func>(func), ...args = std::forward<Args>(args)]() mutable
        {
            std::invoke(func, args...);
        });
    }

    template<typename Func, typename... Args>
    [[nodiscard]] auto invoke(Func&& func, Args&&... args)
    {
        using func_return_type = std::invoke_result_t<Func, Args...>;
        using result_type = task_result<func_return_type>;

        result_type output{};

        push_task([func = std::forward<Func>(func), ...args = std::forward<Args>(args)]() mutable
        {
            return std::invoke(func, args...);
        }, output.m_state);

        return output;
    }

    template<typename Func, typename... Args>
    void dispatch(std::uint32_t x, std::uint32_t y, std::uint32_t z, Func&& func, Args&&... args)
    {
        assert(x != 0 && "nes::task_builder::dispatch called with x == 0");
        assert(y != 0 && "nes::task_builder::dispatch called with y == 0");
        assert(z != 0 && "nes::task_builder::dispatch called with z == 0");

        const std::uint64_t total_calls{static_cast<std::uint64_t>(x) * y * z};

        if(total_calls < m_thread_count)
        {
            for(std::uint32_t current_z{}; current_z < z; ++current_z)
            {
                for(std::uint32_t current_y{}; current_y < y; ++current_y)
                {
                    for(std::uint32_t current_x{}; current_x < x; ++current_x)
                    {
                        push_task([current_x, current_y, current_z, func = std::forward<Func>(func), ...args = std::forward<Args>(args)]() mutable
                        {
                            std::invoke(func, current_x, current_y, current_z, args...);
                        });
                    }
                }
            }
        }
        else
        {
            const std::uint64_t calls_per_thread{total_calls / m_thread_count};

            std::uint64_t remainder{total_calls % m_thread_count};
            std::uint64_t calls{};

            while(calls < total_calls)
            {
                std::uint64_t count{calls_per_thread};

                if(remainder > 0)
                {
                    ++count;
                    --remainder;
                }

                push_task([calls, count, x, y, z_factor = (x * y), func = std::forward<Func>(func), ...args = std::forward<Args>(args)]() mutable
                {
                    for(std::uint64_t i{calls}; i < calls + count; ++i)
                    {
                        const auto current_x{static_cast<std::uint32_t>(i % x)};
                        const auto current_y{static_cast<std::uint32_t>((i / x) % y)};
                        const auto current_z{static_cast<std::uint32_t>(i / z_factor)};

                        std::invoke(func, current_x, current_y, current_z, args...);
                    }
                });

                calls += count;
            }
        }
    }

    task_checkpoint barrier()
    {
        task_result<void> output{};
        m_tasks.emplace_back(std::in_place_type<impl::checkpoint>, true, output.m_state);

        return output;
    }

    [[nodiscard]] task_checkpoint checkpoint()
    {
        task_result<void> output{};
        m_tasks.emplace_back(std::in_place_type<impl::checkpoint>, false, output.m_state);

        return output;
    }

    [[nodiscard]] task_fence fence()
    {
        task_fence output{};
        output.m_holder = std::get<impl::fence>(m_tasks.emplace_back(std::in_place_type<impl::fence>)).holder();

        return output;
    }

    task_list build()
    {
        barrier(); //this barrier is used by the pool to know when all the tasks are done and the list can be returned to user

        task_list output{};
        output.m_tasks.reserve(std::size(m_tasks));
        output.m_checkpoints.reserve(count_checkpoints());

        auto begin  {std::begin(m_tasks)};
        auto current{begin};

        std::size_t checkpoints_begin{};
        std::size_t checkpoints_size{};

        while(current != std::end(m_tasks))
        {
            if(std::holds_alternative<impl::checkpoint>(*current))
            {
                auto& checkpoint{std::get<impl::checkpoint>(*current)};

                output.m_checkpoints.emplace_back(checkpoint.base());
                ++checkpoints_size;

                if(checkpoint.is_barrier())
                {
                    const auto checkpoints_it{std::begin(output.m_checkpoints) + checkpoints_begin};

                    flush(output.m_tasks, checkpoints_it, checkpoints_it + checkpoints_size, begin, current + 1);

                    checkpoints_begin += checkpoints_size;
                    checkpoints_size = 0;

                    begin = current + 1;
                }
            }

            ++current;
        }

        m_tasks.clear();

        return output;
    }

private:
    template<typename... Args>
    void push_task(Args&&... args)
    {
        m_tasks.emplace_back(std::in_place_type<impl::task>, std::forward<Args>(args)...);
    }

    template<typename InputIt, typename CheckpointIt>
    void flush(std::vector<impl::task_type>& output, CheckpointIt checkpoints_begin, CheckpointIt checkpoints_end, InputIt begin, InputIt end)
    {
        std::size_t checkpoint_counter{};

        while(begin != end)
        {
            if(std::holds_alternative<impl::checkpoint>(*begin))
            {
                auto checkpoint{std::get<impl::checkpoint>(std::move(*begin))};

                checkpoint.set_reset_value(checkpoint_counter + 1); //+ 1 for the caller
                output.emplace_back(std::in_place_type<impl::checkpoint>, std::move(checkpoint));

                ++checkpoints_begin;
            }
            else if(std::holds_alternative<impl::task>(*begin))
            {
                auto task{std::get<impl::task>(std::move(*begin))};

                task.set_checkpoint_range(impl::checkpoint_range{checkpoints_begin, checkpoints_end});
                output.emplace_back(std::in_place_type<impl::task>, std::move(task));

                ++checkpoint_counter;
            }
            else
            {
                auto fence{std::get<impl::fence>(std::move(*begin))};

                output.emplace_back(std::in_place_type<impl::fence>, std::move(fence));
            }

            ++begin;
        }
    }

    std::size_t count_checkpoints() const noexcept
    {
        return std::count_if(std::begin(m_tasks), std::end(m_tasks), [](auto&& task)
        {
            return std::holds_alternative<impl::checkpoint>(task);
        });
    }

private:
    std::uint32_t m_thread_count{};
    std::vector<impl::task_type> m_tasks{};
};

class thread_pool
{
public:
    explicit thread_pool(std::size_t thread_count = std::thread::hardware_concurrency())
    {
        const auto worker_base = [this]()
        {
            while(true)
            {
                std::unique_lock lock{m_mutex};
                m_worker_condition.wait(lock, [this]
                {
                    if(std::empty(m_tasks) && !std::empty(m_task_lists))
                    {
                        const auto notify_count{update_task_lists()};

                        for(std::size_t i{}; i < std::min(notify_count, std::size(m_threads)); ++i)
                        {
                            m_worker_condition.notify_one();
                        }
                    }

                    if(std::empty(m_tasks) && std::empty(m_task_lists))
                    {
                        m_wait_condition.notify_all();
                    }

                    return !m_running || !std::empty(m_tasks);
                });

                if(!m_running)
                {
                    break;
                }

                auto task{std::move(m_tasks.front())};
                m_tasks.erase(std::begin(m_tasks));

                lock.unlock();

                if(std::holds_alternative<impl::task>(task))
                {
                    std::get<impl::task>(task).execute();
                }
                else
                {
                    std::get<impl::task_holder_base*>(task)->execute();
                }
            }
        };

        thread_count = thread_count != 0 ? thread_count : 8;

        m_threads.reserve(thread_count);
        for(std::size_t i{}; i < thread_count; ++i)
        {
            m_threads.emplace_back(worker_base);
        }

        m_tasks.reserve(4 * thread_count);
        m_task_lists.reserve(4 * thread_count);
    }

    ~thread_pool()
    {
        std::unique_lock lock{m_mutex};
        m_wait_condition.wait(lock, [this]
        {
            return std::empty(m_tasks) && std::empty(m_task_lists);
        });

        m_running = false;

        lock.unlock();

        m_worker_condition.notify_all();

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
    void execute(Func&& func, Args&&... args)
    {
        push_impl([func = std::forward<Func>(func), ...args = std::forward<Args>(args)]() mutable
        {
            std::invoke(std::forward<Func>(func), std::forward<Args>(args)...);
        });

        m_worker_condition.notify_one();
    }

    template<typename Func, typename... Args>
    auto invoke(Func&& func, Args&&... args)
    {
        using return_type  = std::invoke_result_t<Func, Args...>;
        using promise_type = std::promise<return_type>;
        using future_type  = std::future<return_type>;

        promise_type promise{};
        future_type  future {promise.get_future()};

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

        m_worker_condition.notify_one();

        return future;
    }

    std::future<task_list> push(task_list list)
    {
        list.reset(m_worker_condition);

        std::unique_lock lock{m_mutex};

        auto& data{m_task_lists.emplace_back()};
        data.list = std::move(list);

        auto future{data.promise.get_future()};

        const auto notify_count{update_task_lists()};

        lock.unlock();

        for(std::size_t i{}; i < std::min(notify_count, std::size(m_threads)); ++i)
        {
            m_worker_condition.notify_one();
        }

        return future;
    }

    void wait_idle()
    {
        std::unique_lock lock{m_mutex};
        m_wait_condition.wait(lock, [this]
        {
            return std::empty(m_tasks) && std::empty(m_task_lists);
        });
    }

    std::size_t thread_count() const noexcept
    {
        return std::size(m_threads);
    }

private:
    template<typename Func>
    void push_impl(Func&& func)
    {
        std::lock_guard lock{m_mutex};

        m_tasks.emplace_back(std::in_place_type<impl::task>, std::forward<Func>(func));
    }

    std::size_t update_task_lists()
    {
        std::size_t notify_count{};
        bool need_free{};

        for(auto& list : m_task_lists)
        {
            const auto [end, count] = list.list.next(std::back_inserter(m_tasks));

            if(end && count == 0)
            {
                list.promise.set_value(std::move(list.list));
                list.need_free = true;

                need_free = true;
            }

            notify_count += count;
        }

        if(need_free)
        {
            const auto predicate = [](const task_list_data& list)
            {
                return list.need_free;
            };

            m_task_lists.erase(std::remove_if(std::begin(m_task_lists), std::end(m_task_lists), predicate), std::end(m_task_lists));
        }

        return notify_count;
    }

private:
    struct task_list_data
    {
        task_list list{};
        std::promise<task_list> promise{};
        bool need_free{};
    };

private:
    std::vector<std::thread> m_threads{};

    std::vector<std::variant<impl::task, impl::task_holder_base*>> m_tasks{};
    std::vector<task_list_data> m_task_lists{};

    std::condition_variable m_worker_condition{};
    std::condition_variable m_wait_condition{};
    std::mutex m_mutex{};

    bool m_running{true};
};

}

#endif
