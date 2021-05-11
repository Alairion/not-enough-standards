#include <iostream>
#include <mutex>
#include <thread>

#include <nes/process.hpp>
#include <nes/shared_memory.hpp>
#include <nes/named_mutex.hpp>
#include <nes/named_semaphore.hpp>

enum class data_type : std::uint32_t
{
    uint32 = 1,
    float64,
    string
};

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
    if(!is)
    {
        throw std::runtime_error{"Failed to open pipe."};
    }

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

static void shared_memory()
{
    {
        nes::shared_memory memory{"nes_test_shared_memory", nes::shared_memory_options::constant};
        assert(*memory.map<const std::uint64_t>(0) == 42);
    }

    {
        nes::shared_memory new_memory{"nes_test_shared_memory"};
        *new_memory.map<std::uint64_t>(0) = 16777216;
    }
}

static void named_mutex()
{
    auto tp1 = std::chrono::high_resolution_clock::now();

    nes::named_mutex mutex{"nes_test_named_mutex"};
    std::lock_guard lock{mutex};

    auto tp2 = std::chrono::high_resolution_clock::now();
    std::cout << "Gained ownership of the mutex after " << std::chrono::duration_cast<std::chrono::duration<double>>(tp2 - tp1).count() << "s." << std::endl;
}

static void timed_named_mutex()
{
    nes::timed_named_mutex mutex{"nes_test_timed_named_mutex"};
    std::unique_lock lock{mutex, std::defer_lock};

    std::uint32_t tries{};
    while(!lock.try_lock_for(std::chrono::milliseconds{10}))
        ++tries;

    std::cout << "Gained ownership of the mutex after " << tries << " tries." << std::endl;
}

static void named_semaphore()
{
    nes::named_semaphore semaphore{"nes_test_named_semaphore"};

    const auto tp1{std::chrono::high_resolution_clock::now()};
    for(std::size_t i{}; i < 8; ++i)
    {
        semaphore.acquire();

        const auto elapsed_time{std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - tp1)};
        std::cout << "Acquired after " << elapsed_time.count() << "ms." << std::endl;
    }
}

int main(int argc, char** argv)
{
    std::cout << "Hello world! I'm Other!\n";
    std::cout << "You gaved me " << argc << " arguments:";
    for(int i{}; i < argc; ++i)
        std::cout << "[" << argv[i] << "] ";
    std::cout << '\n';
    std::cout << "My working directory is \"" + nes::this_process::working_directory() << "\"." << std::endl;

    for(int i{}; i < argc; ++i)
    {
        try
        {
            using namespace std::string_view_literals;

            if(argv[i] == "process kill"sv)
            {
                to_infinity_and_beyond();
            }
            if(argv[i] == "named pipe"sv)
            {
                named_pipe();
            }
            if(argv[i] == "shared memory"sv)
            {
                shared_memory();
            }
            if(argv[i] == "named mutex"sv)
            {
                named_mutex();
            }
            if(argv[i] == "timed named mutex"sv)
            {
                timed_named_mutex();
            }
            if(argv[i] == "named semaphore"sv)
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
