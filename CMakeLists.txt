cmake_minimum_required(VERSION 3.23) # required for file sets

project(NotEnoughStandards VERSION 1.1.0)

if(${CMAKE_SOURCE_DIR} STREQUAL ${CMAKE_CURRENT_SOURCE_DIR})
    option(BUILD_TESTING "Build Not Enough Standards' tests" OFF)
else() # don't use caller cache
    set(BUILD_TESTING OFF)
endif()

add_library(NotEnoughStandards INTERFACE)
target_compile_features(NotEnoughStandards INTERFACE cxx_std_20)
target_sources(NotEnoughStandards INTERFACE
    FILE_SET HEADERS
    BASE_DIRS include
    FILES
        include/nes/shared_library.hpp
        include/nes/shared_memory.hpp
        include/nes/named_mutex.hpp
        include/nes/semaphore.hpp
        include/nes/named_semaphore.hpp
        include/nes/pipe.hpp
        include/nes/process.hpp
        include/nes/thread_pool.hpp
    )

if(UNIX)
    target_link_libraries(NotEnoughStandards INTERFACE dl)
    if(NOT ${CMAKE_SYSTEM_NAME} STREQUAL ANDROID)
        target_link_libraries(NotEnoughStandards INTERFACE pthread rt)
    endif()
endif()

if(BUILD_TESTING OR NOT_ENOUGH_STANDARDS_BUILD_TESTS)
    include(CTest)
    get_property(multi_config GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)

    add_executable(NotEnoughStandardsTest tests/common.hpp tests/process.cpp)
    target_link_libraries(NotEnoughStandardsTest PRIVATE NotEnoughStandards)

    add_executable(NotEnoughStandardsTestOther tests/common.hpp tests/process_other.cpp)
    target_link_libraries(NotEnoughStandardsTestOther PRIVATE NotEnoughStandards)

    add_executable(NotEnoughStandardsPipeBufferTest tests/pipe_buffer_test.cpp)
    target_link_libraries(NotEnoughStandardsPipeBufferTest PRIVATE NotEnoughStandards)

    add_library(NotEnoughStandardsTestLib SHARED tests/common.hpp tests/library.cpp)
    set_target_properties(NotEnoughStandardsTestLib PROPERTIES PREFIX "")
    target_link_libraries(NotEnoughStandardsTestLib PRIVATE NotEnoughStandards)

    add_test(NAME NotEnoughStandardsTest COMMAND NotEnoughStandardsTest)
    add_test(NAME NotEnoughStandardsPipeBufferTest COMMAND NotEnoughStandardsPipeBufferTest)
    if(multi_config)
        set_tests_properties(NotEnoughStandardsTest PROPERTIES WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>)
    endif()

    add_executable(NotEnoughStandardsThreadPoolTest tests/common.hpp tests/thread_pool_test.cpp)
    target_link_libraries(NotEnoughStandardsThreadPoolTest PRIVATE NotEnoughStandards)
    add_test(NAME NotEnoughStandardsThreadPoolTest COMMAND NotEnoughStandardsThreadPoolTest)
endif()

include(CMakePackageConfigHelpers)

configure_package_config_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/NotEnoughStandards.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/NotEnoughStandardsConfig.cmake
    INSTALL_DESTINATION lib/cmake/NotEnoughStandards
)

write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/NotEnoughStandardsConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

install(TARGETS NotEnoughStandards
    EXPORT NotEnoughStandardsTargets
    FILE_SET HEADERS DESTINATION include
)

install(EXPORT NotEnoughStandardsTargets
    DESTINATION lib/cmake/NotEnoughStandards
    NAMESPACE NotEnoughStandards::
)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/NotEnoughStandardsConfigVersion.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/NotEnoughStandardsConfig.cmake
    DESTINATION lib/cmake/NotEnoughStandards
)

