#include <iostream>
#include <thread>
#include <mutex>
#include <shared_mutex>
#include <iterator>
#include <iomanip>
#include <array>
#include <cassert>
#include <random>

#include <nes/pipe.hpp>
#include <nes/shared_library.hpp>
#include <nes/process.hpp>
#include <nes/shared_memory.hpp>
#include <nes/named_mutex.hpp>
#include <nes/semaphore.hpp>
#include <nes/named_semaphore.hpp>
#include <nes/thread_pool.hpp>

#if defined(NES_WIN32_PROCESS)
    constexpr const char* other_path{"not_enough_standards_test_other.exe"};
    constexpr const char* lib_path{"not_enough_standards_test_lib.dll"};
#elif defined(NES_POSIX_PROCESS)
    constexpr const char* other_path{"not_enough_standards_test_other"};
    constexpr const char* lib_path{"not_enough_standards_test_lib.so"};
#endif

std::int32_t pow(std::int32_t value, std::uint32_t exponent);

static void shared_library_test()
{
    nes::shared_library lib{lib_path};

    auto func = lib.load<std::int32_t()>("nes_lib_func");

    assert(func);
    assert(func() == 42);
}

enum class data_type : std::uint32_t
{
    uint32 = 1,
    float64,
    string
};

static void a_thread(nes::basic_pipe_istream<char>& is) noexcept
{
    data_type     type{};
    std::uint32_t uint_value{};
    double        float_value{};
    std::string   str_value{};
    std::uint64_t str_size{};

    is.read(reinterpret_cast<char*>(&type), sizeof(data_type));
    assert(type == data_type::uint32);

    is.read(reinterpret_cast<char*>(&uint_value), sizeof(std::uint32_t));
    assert(uint_value == 42);

    is.read(reinterpret_cast<char*>(&type), sizeof(data_type));
    assert(type == data_type::float64);

    is.read(reinterpret_cast<char*>(&float_value), sizeof(double));
    assert(float_value > 3.13 && float_value < 3.15);

    is.read(reinterpret_cast<char*>(&type), sizeof(data_type));
    assert(type == data_type::string);

    is.read(reinterpret_cast<char*>(&str_size), sizeof(std::uint64_t));
    str_value.resize(str_size);
    is.read(std::data(str_value), static_cast<std::streamsize>(str_size));

    assert(str_value == "Hello world!");
}

static void pipe_test()
{
    auto [is, os] = nes::make_anonymous_pipe();

    std::thread thread{a_thread, std::ref(is)};

    const data_type     uint_type {data_type::uint32};
    const std::uint32_t uint_value{42};

    os.write(reinterpret_cast<const char*>(&uint_type), sizeof(data_type));
    os.write(reinterpret_cast<const char*>(&uint_value), sizeof(std::uint32_t));

    const data_type float_type {data_type::float64};
    const double    float_value{3.14};

    os.write(reinterpret_cast<const char*>(&float_type),  sizeof(data_type));
    os.write(reinterpret_cast<const char*>(&float_value), sizeof(double));

    const data_type     str_type {data_type::string};
    const std::string   str_value{"Hello world!"};
    const std::uint64_t size_size{std::size(str_value)};

    os.write(reinterpret_cast<const char*>(&str_type), sizeof(data_type));
    os.write(reinterpret_cast<const char*>(&size_size), sizeof(std::uint64_t));
    os.write(std::data(str_value), static_cast<std::streamsize>(size_size));

    os.close();

    if(thread.joinable())
        thread.join();
}

static void another_thread(const std::array<std::uint32_t, 8>& data, nes::semaphore& semaphore)
{
    for(std::uint32_t i{}; i < 8; ++i)
    {
        semaphore.acquire();

        assert(data[i] == i);
    }
}

static void semaphore_test()
{
    std::array<std::uint32_t, 8> data{0, 1};
    nes::semaphore semaphore{2};
    std::thread thread{another_thread, std::cref(data), std::ref(semaphore)};

    for(std::uint32_t i{2}; i < 8; ++i)
    {
        data[i] = i;

        semaphore.release();
    }

    if(thread.joinable())
        thread.join();
}

static void named_pipe_test()
{
    nes::process other{other_path, std::vector<std::string>{"named pipe"}, nes::process_options::grab_stdout};

    nes::pipe_ostream os{"nes_test_pipe"};
    if(!os)
        throw std::runtime_error{"Failed to open pipe."};

    const data_type     uint_type {data_type::uint32};
    const std::uint32_t uint_value{42};

    os.write(reinterpret_cast<const char*>(&uint_type), sizeof(data_type));
    os.write(reinterpret_cast<const char*>(&uint_value), sizeof(std::uint32_t));

    const data_type float_type {data_type::float64};
    const double    float_value{3.14};

    os.write(reinterpret_cast<const char*>(&float_type),  sizeof(data_type));
    os.write(reinterpret_cast<const char*>(&float_value), sizeof(double));

    const data_type     str_type {data_type::string};
    const std::string   str_value{"Hello world!"};
    const std::uint64_t size_size{std::size(str_value)};

    os.write(reinterpret_cast<const char*>(&str_type), sizeof(data_type));
    os.write(reinterpret_cast<const char*>(&size_size), sizeof(std::uint64_t));
    os.write(std::data(str_value), static_cast<std::streamsize>(size_size));

    os.close();

    if(other.joinable())
        other.join();

    assert(other.return_code() == 0);
}

static void process_test()
{
    std::cout << "Current process has id " << nes::this_process::get_id() << " and its current directory is \"" << nes::this_process::working_directory() << "\"" << std::endl;

    nes::process other{other_path, {"Hey!", "\\\"12\"\"\\\\\\", "\\42\\", "It's \"me\"!"}, nes::process_options::grab_stdout};
    std::cout << other.stdout_stream().rdbuf() << std::endl;

    if(other.joinable())
        other.join();

    assert(other.return_code() == 0);
}

static void process_kill_test()
{
    nes::process other{other_path, std::vector<std::string>{"process kill"}, nes::process_options::grab_stdout};
    std::this_thread::sleep_for(std::chrono::seconds{3});
    other.kill();

    std::cout << other.stdout_stream().rdbuf() << std::endl;

    assert(other.return_code() != 0);
}

static void shared_memory_test()
{
    nes::shared_memory memory{"nes_test_shared_memory", sizeof(std::uint64_t)};
    auto value{memory.map<std::uint64_t>(0)};

    assert(value);

    *value = 42;

    assert(*value == 42);

    nes::process other{other_path, std::vector<std::string>{"shared memory"}, nes::process_options::grab_stdout};
    std::cout << other.stdout_stream().rdbuf() << std::endl;

    if(other.joinable())
        other.join();

    assert(other.return_code() == 0);
    assert(*value == 16777216);
}

static void named_mutex_test()
{
    nes::named_mutex mutex{"nes_test_named_mutex"};
    std::unique_lock lock{mutex};

    nes::process other{other_path, std::vector<std::string>{"named mutex"}, nes::process_options::grab_stdout};

    std::this_thread::sleep_for(std::chrono::milliseconds{500});
    lock.unlock();

    std::cout << other.stdout_stream().rdbuf() << std::endl;

    if(other.joinable())
        other.join();
}

static void timed_named_mutex_test()
{
    nes::timed_named_mutex mutex{"nes_test_timed_named_mutex"};
    std::unique_lock lock{mutex};

    nes::process other{other_path, std::vector<std::string>{"timed named mutex"}, nes::process_options::grab_stdout};

    std::this_thread::sleep_for(std::chrono::milliseconds{1000});
    lock.unlock();

    std::cout << other.stdout_stream().rdbuf() << std::endl;

    if(other.joinable())
        other.join();
}

static void named_semaphore_test()
{
    nes::named_semaphore semaphore{"nes_test_named_semaphore"};

    nes::process other{other_path, std::vector<std::string>{"named semaphore"}, nes::process_options::grab_stdout};

    for(std::size_t i{}; i < 8; ++i)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds{100});
        semaphore.release();
    }

    std::cout << other.stdout_stream().rdbuf() << std::endl;

    if(other.joinable())
        other.join();
}

static void thread_pool_test()
{
    static constexpr std::size_t buffer_size{32};

    //Some buffers
    std::array<std::uint32_t, buffer_size> input{};
    std::array<std::uint32_t, buffer_size> temp{};
    std::array<std::uint32_t, buffer_size> output{};

    const auto print_buffers = [&input, &temp, &output]()
    {
        const auto print_buffer = [](const std::array<std::uint32_t, buffer_size>& buffer)
        {
            for(auto value : buffer)
            {
                std::cout << value << ",";
            }
        };

        std::cout << "input:  ";
        print_buffer(input);
        std::cout << "\ntemp:   ";
        print_buffer(temp);
        std::cout << "\noutput: ";
        print_buffer(output);

        std::cout << std::endl;
    };

    //Fill the buffer with random values
    std::mt19937 rng{std::random_device{}()};
    std::uniform_int_distribution<std::uint32_t> dist{1, 9};

    for(auto& input_value : input)
    {
        input_value = dist(rng);
    }

    //The task builder
    nes::task_builder builder{};

    builder.dispatch(buffer_size, 1, 1, [&input, &temp](std::uint32_t x, std::uint32_t y [[maybe_unused]], std::uint32_t z [[maybe_unused]])
    {
        temp[x] = input[x] * 2u;
    });

    nes::task_checkpoint checkpoint{builder.checkpoint()};

    nes::task_fence fence{builder.fence()};

    builder.dispatch(buffer_size, 1, 1, [&input, &temp, &output](std::uint32_t x, std::uint32_t y [[maybe_unused]], std::uint32_t z [[maybe_unused]])
    {
        for(auto value : temp)
        {
            output[x] += (value + input[x]);
        }
    });

    //Create a thread pool to run our task list.
    nes::thread_pool thread_pool{};

    std::cout << "Initial state:" << std::endl;
    print_buffers();
    std::cout << "Launching first the work..." << std::endl;

    std::future<nes::task_list> future{thread_pool.push(builder.build())};

    std::cout << "Work started..." << std::endl;

    checkpoint.wait();

    std::cout << "First dispatch done:" << std::endl;
    print_buffers();
    std::cout << "Launching second dispatch..." << std::endl;

    fence.signal();

    std::cout << "Second dispatch started..." << std::endl;

    future.wait();

    std::cout << "Second dispatch done:" << std::endl;
    print_buffers();
}

int main()
{
    try
    {
        shared_library_test();
        pipe_test();
        semaphore_test();
        process_test();
        process_kill_test();
        named_pipe_test();
        shared_memory_test();
        named_mutex_test();
        timed_named_mutex_test();
        named_semaphore_test();
        thread_pool_test();

        std::cout << "Tests passed succesfully." << std::endl;
    }
    catch(const std::exception& e)
    {
        std::cout << e.what() << std::endl;
    }
}
