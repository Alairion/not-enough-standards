#include <nes/pipe.hpp>

#include <algorithm>
#include <cstddef>
#include <thread>
#include <vector>

int main()
{
    constexpr std::size_t flushed_prefix_size = 4;
    constexpr std::size_t payload_size = nes::basic_pipe_streambuf<char>::buf_size + 1;
    constexpr std::size_t count = flushed_prefix_size + payload_size;

    auto [input, output] = nes::make_anonymous_pipe();
    std::vector<char> received(count);
    std::size_t received_count{};
    std::thread reader([&]
    {
        while(received_count < received.size() && input.get(received[received_count]))
        {
            ++received_count;
        }
    });

    for(std::size_t i{}; i < flushed_prefix_size; ++i)
    {
        output.put('p');
    }
    output.flush();
    const bool flush_succeeded = output.good();

    for(std::size_t i{}; i < payload_size; ++i)
    {
        output.put('x');
    }
    output.close();
    reader.join();

    const bool prefix_matches = std::all_of(received.begin(), received.begin() + flushed_prefix_size, [](char value) { return value == 'p'; });
    const bool payload_matches = std::all_of(received.begin() + flushed_prefix_size, received.end(), [](char value) { return value == 'x'; });

    return flush_succeeded && received_count == count && prefix_matches && payload_matches ? 0 : 1;
}
