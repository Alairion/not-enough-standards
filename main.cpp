#include <iostream>
#include <thread>
#include <mutex>
#include <shared_mutex>
#include <iterator>
#include <iomanip>

#include <nes/pipe.hpp>
#include <nes/shared_library.hpp>
#include <nes/process.hpp>
#include <nes/shared_memory.hpp>
#include <nes/named_mutex.hpp>
#include <nes/semaphore.hpp>
#include <nes/named_semaphore.hpp>

#if defined(NES_WIN32_SHARED_LIBRARY)
    #define NES_EXAMPLE_EXPORT __declspec(dllexport)
#elif defined(NES_POSIX_SHARED_LIBRARY) && defined(__GNUC__)
    #define NES_EXAMPLE_EXPORT __attribute__((visibility("default")))
#else
    #define NES_EXAMPLE_EXPORT //The example may not work properly in this case.
#endif

#if defined(NES_WIN32_PROCESS)
    constexpr const char* other_path{"not_enough_standards_other.exe"};
#elif defined(NES_POSIX_PROCESS)
    constexpr const char* other_path{"not_enough_standards_other"};
#endif

extern "C" NES_EXAMPLE_EXPORT void foo(int i)
{
    std::cout << "Hello " << i << "!" << std::endl;
}

void shared_library_example()
{
    nes::shared_library lib{nes::load_current};

    auto foo_func = lib.load<void(int)>("foo");
    if(foo_func)
        foo_func(42);
}

enum class data_type : std::uint32_t
{
    uint32 = 1,
    float64,
    string
};

void a_thread(nes::basic_pipe_istream<char>& is) noexcept
{
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

void pipe_example()
{
    auto&&[is, os] = nes::make_anonymous_pipe();

    std::thread thread{a_thread, std::ref(is)};

    for(std::uint32_t i{1}; i < 20; ++i)
    {
        if((i % 3) == 0)
        {
            const data_type type{data_type::uint32};
            os.write(reinterpret_cast<const char*>(&type), sizeof(data_type));

            os.write(reinterpret_cast<const char*>(&i), sizeof(std::uint32_t));
        }
        else if((i % 3) == 1)
        {
            const data_type type{data_type::float64};
            os.write(reinterpret_cast<const char*>(&type), sizeof(data_type));

            const double value{1.0 / i};
            os.write(reinterpret_cast<const char*>(&value), sizeof(double));
        }
        else if((i % 3) == 2)
        {
            const data_type type{data_type::string};
            os.write(reinterpret_cast<const char*>(&type), sizeof(data_type));

            const std::string str{"Hello " + std::to_string(i) + "!"};
            const std::uint64_t size{std::size(str)};
            os.write(reinterpret_cast<const char*>(&size), sizeof(std::uint64_t));
            os.write(std::data(str), static_cast<std::streamsize>(size));
        }
    }

    os.close();

    if(thread.joinable())
        thread.join();
}

void another_thread(const std::array<std::uint64_t, 8>& data, nes::semaphore& semaphore)
{
    const auto tp1{std::chrono::high_resolution_clock::now()};
    for(std::size_t i{}; i < 8; ++i)
    {
        semaphore.acquire();

        const auto elapsed_time{std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - tp1)};
        std::cout << "Value " << i << " ready after " << elapsed_time.count() << "ms: " << data[i] << std::endl;
    }
}

void semaphore_example()
{
    std::array<std::uint64_t, 8> data{0, 1};
    nes::semaphore semaphore{2};
    std::thread thread{another_thread, std::cref(data), std::ref(semaphore)};

    for(std::size_t i{2}; i < 8; ++i)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds{250});
        data[i] = i * i;
        semaphore.release();
    }

    if(thread.joinable())
        thread.join();
}

void named_pipe_example()
{
    nes::process other{other_path, std::vector<std::string>{"named pipe example"}, nes::process_options::grab_stdout};

    nes::pipe_ostream os{"nes_example_pipe"};
    if(!os)
        throw std::runtime_error{"Can not open pipe."};

    for(std::uint32_t i{1}; i < 20; ++i)
    {
        if((i % 3) == 0)
        {
            const data_type type{data_type::uint32};
            os.write(reinterpret_cast<const char*>(&type), sizeof(data_type));

            os.write(reinterpret_cast<const char*>(&i), sizeof(std::uint32_t));
        }
        else if((i % 3) == 1)
        {
            const data_type type{data_type::float64};
            os.write(reinterpret_cast<const char*>(&type), sizeof(data_type));

            const double value{1.0 / i};
            os.write(reinterpret_cast<const char*>(&value), sizeof(double));
        }
        else if((i % 3) == 2)
        {
            const data_type type{data_type::string};
            os.write(reinterpret_cast<const char*>(&type), sizeof(data_type));

            const std::string str{"Hello " + std::to_string(i) + "!"};
            const std::uint64_t size{std::size(str)};
            os.write(reinterpret_cast<const char*>(&size), sizeof(std::uint64_t));
            os.write(std::data(str), static_cast<std::streamsize>(size));
        }
    }
    os.close();

    std::cout << other.stdout_stream().rdbuf() << std::endl;

    if(other.joinable())
        other.join();

    std::cout << "Other process ended with code: " << other.return_code() << std::endl;
}

void process_example()
{
    std::cout << "Current process has id " << nes::this_process::get_id() << " and its current directory is \"" << nes::this_process::working_directory() << "\"" << std::endl;

    nes::process other{other_path, {"Hey!", "\\\"12\"\"\\\\\\", "\\42\\", "It's \"me\"!"}, nes::process_options::grab_stdout};
    std::cout << other.stdout_stream().rdbuf() << std::endl;

    if(other.joinable())
        other.join();

    std::cout << "Other process ended with code: " << other.return_code() << std::endl;
}

void process_kill_example()
{
    nes::process other{other_path, std::vector<std::string>{"process kill example"}, nes::process_options::grab_stdout};
    std::this_thread::sleep_for(std::chrono::seconds{3});
    other.kill();

    std::cout << other.stdout_stream().rdbuf() << std::endl;
    std::cout << "Shut up." << std::endl;
    std::cout << "Other process ended with code: " << other.return_code() << std::endl;
}

void shared_memory_example()
{
    nes::shared_memory memory{"nes_example_shared_memory", sizeof(std::uint64_t)};
    auto value{memory.map<std::uint64_t>(0)};
    *value = 42;

    nes::process other{other_path, std::vector<std::string>{"shared memory example"}, nes::process_options::grab_stdout};
    std::cout << other.stdout_stream().rdbuf() << std::endl;

    if(other.joinable())
        other.join();

    std::cout << "Other process ended with code: " << other.return_code() << std::endl;
    std::cout << "The value in shared memory is: " << *value << std::endl;
}

void named_mutex_example()
{
    nes::named_mutex mutex{"nes_example_named_mutex"};
    std::unique_lock lock{mutex};

    nes::process other{other_path, std::vector<std::string>{"named mutex example"}, nes::process_options::grab_stdout};

    std::this_thread::sleep_for(std::chrono::milliseconds{500});
    lock.unlock();

    std::cout << other.stdout_stream().rdbuf() << std::endl;

    if(other.joinable())
        other.join();
}

void timed_named_mutex_example()
{
    nes::timed_named_mutex mutex{"nes_example_timed_named_mutex"};
    std::unique_lock lock{mutex};

    nes::process other{other_path, std::vector<std::string>{"timed named mutex example"}, nes::process_options::grab_stdout};

    std::this_thread::sleep_for(std::chrono::milliseconds{1000});
    lock.unlock();

    std::cout << other.stdout_stream().rdbuf() << std::endl;

    if(other.joinable())
        other.join();
}

void named_semaphore_example()
{
    nes::named_semaphore semaphore{"nes_example_named_semaphore"};

    nes::process other{other_path, std::vector<std::string>{"named semaphore example"}, nes::process_options::grab_stdout};

    for(std::size_t i{0}; i < 8; ++i)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds{100});
        semaphore.release();
    }

    std::cout << other.stdout_stream().rdbuf() << std::endl;

    if(other.joinable())
        other.join();
}

int main()
{
    try
    {
        shared_library_example();
        //pipe_example();
        //semaphore_example();
        //process_example();
        //process_kill_example();
        //named_pipe_example();
        //shared_memory_example();
        //named_mutex_example();
        //timed_named_mutex_example();
        //named_semaphore_example();
    }
    catch(const std::exception& e)
    {
        std::cout << e.what() << std::endl;
    }
}
