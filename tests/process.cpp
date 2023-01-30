#include <iostream>
#include <thread>
#include <mutex>
#include <shared_mutex>
#include <iterator>
#include <iomanip>
#include <array>
#include <cassert>
#include <cstdlib>
#include <random>

#include <nes/pipe.hpp>
#include <nes/shared_library.hpp>
#include <nes/process.hpp>
#include <nes/shared_memory.hpp>
#include <nes/named_mutex.hpp>
#include <nes/semaphore.hpp>
#include <nes/named_semaphore.hpp>
#include <nes/thread_pool.hpp>

#include "common.hpp"

#if defined(NES_WIN32_PROCESS)
    constexpr const char* other_path{"NotEnoughStandardsTestOther.exe"};
    constexpr const char* lib_path{"NotEnoughStandardsTestLib.dll"};
#elif defined(NES_POSIX_PROCESS)
    constexpr const char* other_path{"./NotEnoughStandardsTestOther"};
    constexpr const char* lib_path{"./NotEnoughStandardsTestLib.so"};
#endif

static void shared_library_test()
{
    nes::shared_library lib{lib_path};

    auto func = lib.load<std::int32_t()>("nes_lib_func");

    CHECK(func, "Can not load library \"" << lib_path << "\"");
    const auto value{func()};
    CHECK(value == 42, "Function returned wrong value " << value);
}

static void a_thread(nes::basic_pipe_istream<char>& is) noexcept
{
    data_type     type{};
    std::uint32_t uint_value{};
    double        float_value{};
    std::string   str_value{};
    std::uint64_t str_size{};

    is.read(reinterpret_cast<char*>(&type), sizeof(data_type));
    CHECK(type == data_type::uint32, "Wrong data type, expected uint32 got " << data_type_to_string(type));

    is.read(reinterpret_cast<char*>(&uint_value), sizeof(std::uint32_t));
    CHECK(uint_value == 42, "Wrong value, expected 42 got " << uint_value);

    is.read(reinterpret_cast<char*>(&type), sizeof(data_type));
    CHECK(type == data_type::float64, "Wrong data type, expected float64 got " << data_type_to_string(type));

    is.read(reinterpret_cast<char*>(&float_value), sizeof(double));
    CHECK(float_value > 3.139 && float_value < 3.141, "Wrong value, expected 3.14 got " << float_value);

    is.read(reinterpret_cast<char*>(&type), sizeof(data_type));
    CHECK(type == data_type::string, "Wrong data type, expected string got " << data_type_to_string(type));

    is.read(reinterpret_cast<char*>(&str_size), sizeof(std::uint64_t));
    str_value.resize(str_size);
    is.read(std::data(str_value), static_cast<std::streamsize>(str_size));

    CHECK(str_value == "Hello world!", "Wrong value, expected \"Hello world!\" got \"" << str_value << "\"");
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
    {
        thread.join();
    }
}

static void another_thread(const std::array<std::uint32_t, 8>& data, nes::semaphore& semaphore)
{
    for(std::uint32_t i{}; i < 8; ++i)
    {
        semaphore.acquire();

        CHECK(data[i] == i, "Wrong value expected " << i << " got " << data[i]);
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
    {
        thread.join();
    }
}

static void named_pipe_test()
{
    nes::process other{other_path, std::vector<std::string>{"named pipe"}, nes::process_options::grab_stdout};

    nes::pipe_ostream os{"nes_test_pipe"};
    CHECK(os, "Failed to open pipe.");

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

    CHECK(other.joinable(), "Process is not joinable");
    other.join();
    CHECK(other.return_code() == 0, "Other process failed with code " << other.return_code() << ":\n" << other.stdout_stream().rdbuf());
}

static void process_test()
{
    nes::process other{other_path, {"Hey!", "\\\"12\"\"\\\\\\", "\\42\\", "It's \"me\"!"}, nes::process_options::grab_stdout};
    std::cout << other.stdout_stream().rdbuf() << std::endl;

    CHECK(other.joinable(), "Process is not joinable");
    other.join();
    CHECK(other.return_code() == 0, "Other process failed with code " << other.return_code() << ":\n" << other.stdout_stream().rdbuf());
}

static void process_kill_test()
{
    nes::process other{other_path, std::vector<std::string>{"process kill"}, nes::process_options::grab_stdout};
    std::this_thread::sleep_for(std::chrono::milliseconds{200});
    CHECK(other.kill(), "Failed to kill other process");
    CHECK(other.return_code() != 0, "Other returned 0");
    CHECK(!other.joinable(), "Other is still joinable");
}

static void shared_memory_test()
{
    nes::shared_memory memory{"nes_test_shared_memory", sizeof(std::uint64_t)};
    auto value{memory.map<std::uint64_t>(0)};
    CHECK(value, "Failed to map shared memory");

    *value = 42;

    CHECK(*value == 42, "Failed to write shared memory");

    nes::process other{other_path, std::vector<std::string>{"shared memory"}, nes::process_options::grab_stdout};

    CHECK(other.joinable(), "Process is not joinable");
    other.join();
    CHECK(other.return_code() == 0, "Other process failed with code " << other.return_code() << ":\n" << other.stdout_stream().rdbuf());

    CHECK(*value == 16777216, "Wrong value in shared memory, expected 16777216 got " << *value);

    other = nes::process{other_path, std::vector<std::string>{"shared memory bad"}, nes::process_options::grab_stdout};

    CHECK(other.joinable(), "Process is not joinable");
    other.join();
    CHECK(other.return_code() != 0, "Other process must return an error");
    CHECK(*value == 16777216, "Wrong value in shared memory, expected 16777216 got " << *value);
}

static void named_mutex_test()
{
    nes::named_mutex mutex{"nes_test_named_mutex"};
    std::unique_lock lock{mutex};

    nes::process other{other_path, std::vector<std::string>{"named mutex"}, nes::process_options::grab_stdout};

    lock.unlock();

    CHECK(other.joinable(), "Process is not joinable");
    other.join();
    CHECK(other.return_code() == 0, "Other process failed with code " << other.return_code() << ":\n" << other.stdout_stream().rdbuf());
}

static void timed_named_mutex_test()
{
    nes::timed_named_mutex mutex{"nes_test_timed_named_mutex"};
    std::unique_lock lock{mutex};

    nes::process other{other_path, std::vector<std::string>{"timed named mutex"}, nes::process_options::grab_stdout};

    lock.unlock();

    CHECK(other.joinable(), "Process is not joinable");
    other.join();
    CHECK(other.return_code() == 0, "Other process failed with code " << other.return_code() << ":\n" << other.stdout_stream().rdbuf());
}

static void named_semaphore_test()
{
    nes::named_semaphore semaphore{"nes_test_named_semaphore"};

    nes::process other{other_path, std::vector<std::string>{"named semaphore"}, nes::process_options::grab_stdout};

    for(std::size_t i{}; i < 8; ++i)
    {
        semaphore.release();
    }

    CHECK(other.joinable(), "Process is not joinable");
    other.join();
    CHECK(other.return_code() == 0, "Other process failed with code " << other.return_code() << ":\n" << other.stdout_stream().rdbuf());
}

static void thread_pool_test()
{
    static constexpr std::size_t buffer_size{8};

    //Some buffers
    std::array<std::uint32_t, buffer_size> input{32, 543, 4329, 12, 542, 656, 523, 98473};
    std::array<std::uint32_t, buffer_size> temp{};
    std::array<std::uint32_t, buffer_size> output{};

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

    std::future<nes::task_list> future{thread_pool.push(builder.build())};
    checkpoint.wait();

    constexpr std::array<std::uint32_t, buffer_size> temp_expected{64, 1086, 8658, 24, 1084, 1312, 1046, 196946};
    CHECK(temp == temp_expected, "Wrong array values");

    fence.signal();
    future.wait();

    constexpr std::array<std::uint32_t, buffer_size> output_expected{210476, 214564, 244852, 210316, 214556, 215468, 214404, 998004};
    CHECK(output == output_expected, "Wrong array values");
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

        std::cout << "All tests passed!" << std::endl;
    }
    catch(const std::exception& e)
    {
        CHECK(false, e.what());
        return 1;
    }
}
