#include <array>

#include <nes/thread_pool.hpp>

#include "common.hpp"

static void smoke_test()
{
    static constexpr std::size_t buffer_size{8};

    // Some buffers
    std::array<std::uint32_t, buffer_size> input{32, 543, 4329, 12, 542, 656, 523, 98473};
    std::array<std::uint32_t, buffer_size> temp{};
    std::array<std::uint32_t, buffer_size> output{};

    // The task builder
    nes::task_builder builder{};

    builder.dispatch(buffer_size, 1, 1, [&input, &temp](std::uint32_t x, std::uint32_t y [[maybe_unused]], std::uint32_t z [[maybe_unused]] )
    {
        temp[x] = input[x] * 2u;
    });

    nes::task_checkpoint checkpoint{builder.checkpoint()};
    nes::task_fence fence{builder.fence()};

    builder.dispatch(buffer_size, 1, 1, [&input, &temp, &output](std::uint32_t x, std::uint32_t y [[maybe_unused]], std::uint32_t z [[maybe_unused]] )
    {
        for(auto value : temp)
        {
            output[x] += (value + input[x]);
        }
    });

    // Create a thread pool to run our task list.
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
        smoke_test();
    }
    catch(const std::exception& e)
    {
        CHECK(false, e.what());
        return 1;
    }
}