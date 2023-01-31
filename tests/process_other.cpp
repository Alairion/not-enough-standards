#include <iostream>
#include <mutex>
#include <thread>

#include <nes/process.hpp>
#include <nes/shared_memory.hpp>
#include <nes/named_mutex.hpp>
#include <nes/named_semaphore.hpp>

#include "common.hpp"

[[noreturn]] static void to_infinity_and_beyond()
{
    while(true)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds{10});
    }
}

static void named_pipe()
{
    nes::pipe_istream is{"nes_test_pipe"};
    CHECK(is, "Failed to open pipe.");

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

static void shared_memory()
{
    {
        nes::shared_memory memory{"nes_test_shared_memory", nes::shared_memory_options::constant};
        const auto value{*memory.map<const std::uint64_t>(0)};
        CHECK(value == 42, "Wrong value, expected 42 got " << value);
    }

    {
        nes::shared_memory new_memory{"nes_test_shared_memory"};
        *new_memory.map<std::uint64_t>(0) = 16777216;
    }
}

static void shared_memory_bad()
{
    nes::shared_memory memory{"nes_test_shared_memory", nes::shared_memory_options::constant};
    *memory.map<std::uint64_t>(0) = 12;
    //theorically unreachable
}

static void named_mutex()
{
    nes::named_mutex mutex{"nes_test_named_mutex"};
    std::lock_guard lock{mutex}; // will throw in case of error
}

static void timed_named_mutex()
{
    nes::timed_named_mutex mutex{"nes_test_timed_named_mutex"};
    std::unique_lock lock{mutex, std::defer_lock};

    while(!lock.try_lock_for(std::chrono::milliseconds{10}))
        ; // will throw in case of error
}

static void named_semaphore()
{
    nes::named_semaphore semaphore{"nes_test_named_semaphore"};

    for(std::size_t i{}; i < 8; ++i)
    {
        semaphore.acquire(); // will throw in case of error
    }
}

int main(int argc, char** argv)
{
    for(int i{}; i < argc; ++i)
    {
        try
        {
            using namespace std::string_view_literals;

            if(argv[i] == "process kill"sv)
            {
                to_infinity_and_beyond();
            }
            else if(argv[i] == "named pipe"sv)
            {
                named_pipe();
            }
            else if(argv[i] == "shared memory"sv)
            {
                shared_memory();
            }
            else if(argv[i] == "shared memory bad"sv)
            {
                shared_memory_bad();
            }
            else if(argv[i] == "named mutex"sv)
            {
                named_mutex();
            }
            else if(argv[i] == "timed named mutex"sv)
            {
                timed_named_mutex();
            }
            else if(argv[i] == "named semaphore"sv)
            {
                named_semaphore();
            }
        }
        catch(const std::exception& e)
        {
            std::cout << e.what() << std::endl;
            return 1;
        }
    }
}
