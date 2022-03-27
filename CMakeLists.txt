cmake_minimum_required(VERSION 3.8)

project(NotEnoughStandards VERSION 1.0.0)

option(NES_BUILD_EXAMPLES "Build Not Enough Standards' examples" OFF)
option(NES_BUILD_TESTS "Build Not Enough Standards' tests" OFF)

add_library(NotEnoughStandards INTERFACE)

target_include_directories(NotEnoughStandards INTERFACE
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_sources(NotEnoughStandards INTERFACE
    "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>/include/nes/shared_library.hpp"
    "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>/include/nes/shared_memory.hpp"
    "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>/include/nes/named_mutex.hpp"
    "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>/include/nes/semaphore.hpp"
    "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>/include/nes/named_semaphore.hpp"
    "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>/include/nes/pipe.hpp"
    "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>/include/nes/process.hpp"
    "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>/include/nes/hash.hpp"
    "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>/include/nes/thread_pool.hpp"
    )

if(UNIX)
    target_link_libraries(NotEnoughStandards INTERFACE pthread rt dl)
endif()

if(NES_BUILD_EXAMPLES)
    add_executable("NotEnoughStandards_example" "main.cpp")
    target_link_libraries("NotEnoughStandards_example" NotEnoughStandards)

    if(UNIX)
        target_link_libraries("NotEnoughStandards_example" -rdynamic)
    endif()

    add_executable("NotEnoughStandards_example_other" "other.cpp")
    target_link_libraries("NotEnoughStandards_example_other" NotEnoughStandards)
endif()

if(NES_BUILD_TESTS)
    add_executable("NotEnoughStandards_test" "tests/process.cpp")
    target_link_libraries("NotEnoughStandards_test" NotEnoughStandards)

    add_executable("NotEnoughStandards_test_other" "tests/process_other.cpp")
    target_link_libraries("NotEnoughStandards_test_other" NotEnoughStandards)

    add_library("NotEnoughStandards_test_lib" SHARED "tests/library.cpp")
    set_target_properties("NotEnoughStandards_test_lib" PROPERTIES PREFIX "")
    target_link_libraries("NotEnoughStandards_test_lib" NotEnoughStandards)
endif()

include(CMakePackageConfigHelpers)

configure_package_config_file(
    "${PROJECT_SOURCE_DIR}/cmake/NotEnoughStandards.cmake.in"
    "${PROJECT_BINARY_DIR}/NotEnoughStandards.cmake"
    INSTALL_DESTINATION lib/cmake/NotEnoughStandards
)

write_basic_package_version_file(
    "${PROJECT_BINARY_DIR}/NotEnoughStandardsConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

install(TARGETS NotEnoughStandards
        EXPORT NotEnoughStandardsTargets
        PUBLIC_HEADER DESTINATION "include" COMPONENT Development
)

install(EXPORT NotEnoughStandardsTargets
        DESTINATION lib/cmake/NotEnoughStandards
)

install(FILES "${PROJECT_BINARY_DIR}/NotEnoughStandardsConfigVersion.cmake"
              "${PROJECT_BINARY_DIR}/NotEnoughStandards.cmake"
        DESTINATION "lib/cmake/NotEnoughStandards"
)

install(DIRECTORY ${PROJECT_SOURCE_DIR}/include/
        DESTINATION "include"
)

