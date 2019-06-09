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

[[noreturn]] void to_infinity_and_beyond()
{
    while(true)
    {
        std::cout << "Ha ha! I'm running indefinitely!" << std::endl;
        std::this_thread::sleep_for(std::chrono::milliseconds{500});
    }
}

void named_pipe_example()
{
    nes::pipe_istream is{"nes_example_pipe"};
    if(!is)
    {
        std::cerr << "Failed to open pipe." << std::endl;
        return;
    }

    while(is)
    {
        data_type type{};
        is.read(reinterpret_cast<char*>(&type), sizeof(data_type));
        if(type == data_type::uint32)
        {
            std::uint32_t value{};
            is.read(reinterpret_cast<char*>(&value), sizeof(std::uint32_t));

            std::cout << "Received an unsigned integer: " << value << std::endl;
        }
        else if(type == data_type::float64)
        {
            double value{};
            is.read(reinterpret_cast<char*>(&value), sizeof(double));

            std::cout << "Received a double: " << value << std::endl;
        }
        else if(type == data_type::string)
        {
            std::uint64_t size{};
            is.read(reinterpret_cast<char*>(&size), sizeof(std::uint64_t));
            std::string str{};
            str.resize(size);
            is.read(reinterpret_cast<char*>(std::data(str)), static_cast<std::streamsize>(size));

            std::cout << "Received a string: " << str << std::endl;
        }
    }
}

void shared_memory_example()
{
    {
        nes::shared_memory memory{"nes_example_shared_memory", nes::shared_memory_option::constant};
        std::cout << "Value in shared memory is: " << *memory.map<const std::uint64_t>(0) << std::endl;
    }

    {
        std::cout << "Modifying value in shared memory to 2^24..." << std::endl;
        nes::shared_memory new_memory{"nes_example_shared_memory"};
        *new_memory.map<std::uint64_t>(0) = 16777216;
    }
}

void named_mutex_example()
{
    auto tp1 = std::chrono::high_resolution_clock::now();

    nes::named_mutex mutex{"nes_example_named_mutex"};
    std::lock_guard lock{mutex};

    auto tp2 = std::chrono::high_resolution_clock::now();
    std::cout << "Gained ownership of the mutex after " << std::chrono::duration_cast<std::chrono::duration<double>>(tp2 - tp1).count() << "s." << std::endl;
}

void timed_named_mutex_example()
{
    nes::timed_named_mutex mutex{"nes_example_timed_named_mutex"};
    std::unique_lock lock{mutex, std::defer_lock};

    std::uint32_t tries{};
    while(!lock.try_lock_for(std::chrono::milliseconds{10}))
        ++tries;

    std::cout << "Gained ownership of the mutex after " << tries << " tries." << std::endl;
}

void named_semaphore_example()
{
    nes::named_semaphore semaphore{"nes_example_named_semaphore"};

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

            if(argv[i] == "process kill example"sv)
                to_infinity_and_beyond();
            if(argv[i] == "named pipe example"sv)
                named_pipe_example();
            if(argv[i] == "shared memory example"sv)
                shared_memory_example();
            if(argv[i] == "named mutex example"sv)
                named_mutex_example();
            if(argv[i] == "timed named mutex example"sv)
                timed_named_mutex_example();
            if(argv[i] == "named semaphore example"sv)
                named_semaphore_example();
        }
        catch(const std::exception& e)
        {
            std::cout << e.what() << std::endl;
        }
    }
}
