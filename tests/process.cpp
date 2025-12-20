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
    data_type type{};

    is.read(reinterpret_cast<char*>(&type), sizeof(data_type));
    CHECK(type == data_type::uint32, "Wrong data type, expected uint32 got " << data_type_to_string(type));

    std::uint32_t uint_value{};
    is.read(reinterpret_cast<char*>(&uint_value), sizeof(std::uint32_t));
    CHECK(uint_value == 42, "Wrong value, expected 42 got " << uint_value);

    is.read(reinterpret_cast<char*>(&type), sizeof(data_type));
    CHECK(type == data_type::float64, "Wrong data type, expected float64 got " << data_type_to_string(type));

    double float_value{};
    is.read(reinterpret_cast<char*>(&float_value), sizeof(double));
    CHECK(float_value > 3.139 && float_value < 3.141, "Wrong value, expected 3.14 got " << float_value);

    is.read(reinterpret_cast<char*>(&type), sizeof(data_type));
    CHECK(type == data_type::string, "Wrong data type, expected string got " << data_type_to_string(type));

    std::uint64_t str_size{};
    is.read(reinterpret_cast<char*>(&str_size), sizeof(std::uint64_t));
    std::string str_value{};
    str_value.resize(str_size);
    is.read(std::data(str_value), static_cast<std::streamsize>(str_size));

    is.read(reinterpret_cast<char*>(&type), sizeof(data_type));
    CHECK(type == data_type::string, "Wrong data type, expected string got " << data_type_to_string(type));
    is.read(reinterpret_cast<char*>(&str_size), sizeof(std::uint64_t));
    str_value.clear();
    is >> str_value;
    CHECK(str_value == "Hello", "Wrong value, expected \"Hello\" got \"" << str_value << "\"");
    str_value.clear();
    str_value.resize(3);
    is.read(std::data(str_value), 3);
    CHECK(str_value == " wo", "Wrong value, expected \" wo\" got \"" << str_value << "\"");
    is >> str_value;
    CHECK(str_value == "rld!", "Wrong value, expected \"rld!\" got \"" << str_value << "\"");

}

static void pipe_test()
{
    auto [is, os] = nes::make_anonymous_pipe();

    std::thread thread{a_thread, std::ref(is)};

    const data_type uint_type{data_type::uint32};
    const std::uint32_t uint_value{42};

    os.write(reinterpret_cast<const char*>(&uint_type), sizeof(data_type));
    os.write(reinterpret_cast<const char*>(&uint_value), sizeof(std::uint32_t));

    const data_type float_type{data_type::float64};
    const double float_value{3.14};

    os.write(reinterpret_cast<const char*>(&float_type), sizeof(data_type));
    os.write(reinterpret_cast<const char*>(&float_value), sizeof(double));

    const data_type str_type{data_type::string};
    const std::string str_value{"Hello world!"};
    const std::uint64_t size_size{std::size(str_value)};

    os.write(reinterpret_cast<const char*>(&str_type), sizeof(data_type));
    os.write(reinterpret_cast<const char*>(&size_size), sizeof(std::uint64_t));
    os.write(std::data(str_value), static_cast<std::streamsize>(size_size));

    // send it twice to test formatted input too
    os.write(reinterpret_cast<const char*>(&str_type), sizeof(data_type));
    os.write(reinterpret_cast<const char*>(&size_size), sizeof(std::uint64_t));
    os.put(str_value[0]);
    os.write(str_value.data() + 1, str_value.size() - 1);

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

    const data_type uint_type{data_type::uint32};
    const std::uint32_t uint_value{42};

    os.write(reinterpret_cast<const char*>(&uint_type), sizeof(data_type));
    os.write(reinterpret_cast<const char*>(&uint_value), sizeof(std::uint32_t));

    const data_type float_type{data_type::float64};
    const double float_value{3.14};

    os.write(reinterpret_cast<const char*>(&float_type), sizeof(data_type));
    os.write(reinterpret_cast<const char*>(&float_value), sizeof(double));

    const data_type str_type{data_type::string};
    const std::string str_value{"Hello world!"};
    const std::uint64_t size_size{std::size(str_value)};

    os.write(reinterpret_cast<const char*>(&str_type), sizeof(data_type));
    os.write(reinterpret_cast<const char*>(&size_size), sizeof(std::uint64_t));
    os.write(std::data(str_value), static_cast<std::streamsize>(size_size));

    os.close();

    CHECK(other.joinable(), "Process is not joinable");
    other.join();
    CHECK(other.return_code() == 0, "Other process failed with code " << other.return_code() << ":\n"
                                                                      << other.stdout_stream().rdbuf());
}

static void process_test()
{
    nes::process other{
    other_path, {"Hey!", "\\\"12\"\"\\\\\\", "\\42\\", "It's \"me\"!"},
     nes::process_options::grab_stdout
    };
    std::cout << other.stdout_stream().rdbuf() << std::endl;

    CHECK(other.joinable(), "Process is not joinable");
    other.join();
    CHECK(other.return_code() == 0, "Other process failed with code " << other.return_code() << ":\n"
                                                                      << other.stdout_stream().rdbuf());
}

static void process_kill_test()
{
    nes::process other{other_path, std::vector<std::string>{"process kill"}, nes::process_options::grab_stdout};
    std::this_thread::sleep_for(std::chrono::milliseconds{200});
    CHECK(other.kill(), "Failed to kill other process");
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
    CHECK(other.return_code() == 0, "Other process failed with code " << other.return_code() << ":\n"
                                                                      << other.stdout_stream().rdbuf());

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
    CHECK(other.return_code() == 0, "Other process failed with code " << other.return_code() << ":\n"
                                                                      << other.stdout_stream().rdbuf());
}

static void timed_named_mutex_test()
{
    nes::timed_named_mutex mutex{"nes_test_timed_named_mutex"};
    std::unique_lock lock{mutex};

    nes::process other{other_path, std::vector<std::string>{"timed named mutex"}, nes::process_options::grab_stdout};

    lock.unlock();

    CHECK(other.joinable(), "Process is not joinable");
    other.join();
    CHECK(other.return_code() == 0, "Other process failed with code " << other.return_code() << ":\n"
                                                                      << other.stdout_stream().rdbuf());
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
    CHECK(other.return_code() == 0, "Other process failed with code " << other.return_code() << ":\n"
                                                                      << other.stdout_stream().rdbuf());
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
    }
    catch(const std::exception& e)
    {
        CHECK(false, e.what());
        return 1;
    }
}
