///////////////////////////////////////////////////////////
/// Copyright 2019 Alexy Pellegrini
///
/// Permission is hereby granted, free of charge,
/// to any person obtaining a copy of this software
/// and associated documentation files (the "Software"),
/// to deal in the Software without restriction,
/// including without limitation the rights to use,
/// copy, modify, merge, publish, distribute, sublicense,
/// and/or sell copies of the Software, and to permit
/// persons to whom the Software is furnished to do so,
/// subject to the following conditions:
///
/// The above copyright notice and this permission notice
/// shall be included in all copies or substantial portions
/// of the Software.
///
/// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF
/// ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED
/// TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A
/// PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT
/// SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR
/// ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
/// ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
/// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE
/// OR OTHER DEALINGS IN THE SOFTWARE.
///////////////////////////////////////////////////////////

#ifndef NOT_ENOUGH_STANDARDS_TESTS_COMMON
#define NOT_ENOUGH_STANDARDS_TESTS_COMMON

#include <cstdlib>
#include <cstdint>
#include <iostream>

#define CHECK(expr, message) \
    if(!(expr)) \
    { \
        std::cerr << "Error: Test failed at " << __FILE__ << "L." << __LINE__ << ":\n " << message << std::endl; \
        std::exit(1); \
    } static_cast<void>(0)


enum class data_type : std::uint32_t
{
    uint32 = 1,
    float64,
    string
};

inline std::string data_type_to_string(data_type type)
{
    switch(type)
    {
    case data_type::uint32: return "uint32";
    case data_type::float64: return "float64";
    case data_type::string: return "string";
    }

    return "Wrong type";
}

#endif
