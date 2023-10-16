cmake_minimum_required(VERSION 3.12) # required for C++20

project(NotEnoughStandards VERSION 1.0.5)

if(${CMAKE_SOURCE_DIR} STREQUAL ${CMAKE_CURRENT_SOURCE_DIR})
    option(BUILD_EXAMPLES "Build Not Enough Standards' examples" OFF)
    option(BUILD_TESTING "Build Not Enough Standards' tests" OFF)
else() # don't use caller cache
    set(BUILD_EXAMPLES OFF)
    set(BUILD_TESTING OFF)
endif()

add_library(NotEnoughStandards INTERFACE)
target_compile_features(NotEnoughStandards INTERFACE cxx_std_20)

target_include_directories(NotEnoughStandards INTERFACE
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_sources(NotEnoughStandards INTERFACE
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include/nes/shared_library.hpp>
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include/nes/shared_memory.hpp>
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include/nes/named_mutex.hpp>
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include/nes/semaphore.hpp>
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include/nes/named_semaphore.hpp>
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include/nes/pipe.hpp>
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include/nes/process.hpp>
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include/nes/thread_pool.hpp>
    )

if(UNIX)
    target_link_libraries(NotEnoughStandards INTERFACE dl)
    if(NOT ${CMAKE_SYSTEM_NAME} STREQUAL ANDROID)
        target_link_libraries(NotEnoughStandards INTERFACE pthread rt)
    endif()
endif()

if(BUILD_EXAMPLES OR NOT_ENOUGH_STANDARDS_BUILD_EXAMPLE)
    add_executable(NotEnoughStandardsExample main.cpp)
    target_link_libraries(NotEnoughStandardsExample NotEnoughStandards)

    if(UNIX)
        target_link_libraries(NotEnoughStandardsExample -rdynamic)
    endif()

    add_executable(NotEnoughStandardsExampleOther other.cpp)
    target_link_libraries(NotEnoughStandardsExampleOther NotEnoughStandards)
endif()

if(BUILD_TESTING OR NOT_ENOUGH_STANDARDS_BUILD_EXAMPLE)
    add_executable(NotEnoughStandardsTest tests/common.hpp tests/process.cpp)
    target_link_libraries(NotEnoughStandardsTest NotEnoughStandards)

    add_executable(NotEnoughStandardsTestOther tests/common.hpp tests/process_other.cpp)
    target_link_libraries(NotEnoughStandardsTestOther NotEnoughStandards)

    add_library(NotEnoughStandardsTestLib SHARED tests/common.hpp tests/library.cpp)
    set_target_properties(NotEnoughStandardsTestLib PROPERTIES PREFIX "")
    target_link_libraries(NotEnoughStandardsTestLib NotEnoughStandards)

    include(CTest)
    add_test(NAME NotEnoughStandardsTest COMMAND NotEnoughStandardsTest)
    get_property(multi_config GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
    if(multi_config)
        set_tests_properties(NotEnoughStandardsTest PROPERTIES WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>)
    endif()
endif()

include(CMakePackageConfigHelpers)

configure_package_config_file(
    ${PROJECT_SOURCE_DIR}/cmake/NotEnoughStandards.cmake.in
    ${PROJECT_BINARY_DIR}/NotEnoughStandardsConfig.cmake
    INSTALL_DESTINATION lib/cmake/NotEnoughStandards
)

write_basic_package_version_file(
    ${PROJECT_BINARY_DIR}/NotEnoughStandardsConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

install(TARGETS NotEnoughStandards
        EXPORT NotEnoughStandardsTargets
)

install(EXPORT NotEnoughStandardsTargets
        DESTINATION lib/cmake/NotEnoughStandards
        NAMESPACE NotEnoughStandards::
)

install(FILES ${PROJECT_BINARY_DIR}/NotEnoughStandardsConfigVersion.cmake
              ${PROJECT_BINARY_DIR}/NotEnoughStandardsConfig.cmake
        DESTINATION lib/cmake/NotEnoughStandards
)

install(DIRECTORY ${PROJECT_SOURCE_DIR}/include/
        DESTINATION include
)
