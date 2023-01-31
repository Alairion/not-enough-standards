#include <cstdint>

#ifdef _WIN32
    #define NES_EXPORT __declspec(dllexport)
#else
    #define NES_EXPORT
#endif

extern "C" NES_EXPORT std::int32_t nes_lib_func();
extern "C" NES_EXPORT std::int32_t nes_lib_func()
{
    return 42;
}
