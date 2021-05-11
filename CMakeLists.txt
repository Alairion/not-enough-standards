cmake_minimum_required(VERSION 3.8)

project(not_enough_standards VERSION 0.1.4)

option(NES_BUILD_EXAMPLES OFF)
option(NES_BUILD_TESTS OFF)

add_library(${PROJECT_NAME} INTERFACE)

target_include_directories(${PROJECT_NAME} INTERFACE
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_sources(${PROJECT_NAME} INTERFACE
    ${PROJECT_SOURCE_DIR}/include/nes/shared_library.hpp
    ${PROJECT_SOURCE_DIR}/include/nes/shared_memory.hpp
    ${PROJECT_SOURCE_DIR}/include/nes/named_mutex.hpp
    ${PROJECT_SOURCE_DIR}/include/nes/semaphore.hpp
    ${PROJECT_SOURCE_DIR}/include/nes/named_semaphore.hpp
    ${PROJECT_SOURCE_DIR}/include/nes/pipe.hpp
    ${PROJECT_SOURCE_DIR}/include/nes/process.hpp
    ${PROJECT_SOURCE_DIR}/include/nes/hash.hpp
    ${PROJECT_SOURCE_DIR}/include/nes/thread_pool.hpp
    )

if(UNIX)
    target_link_libraries(${PROJECT_NAME} INTERFACE pthread rt dl)
endif()

if(NES_BUILD_EXAMPLES)
    add_executable("${PROJECT_NAME}_example" "main.cpp")
    target_link_libraries("${PROJECT_NAME}_example" ${PROJECT_NAME})

    if(UNIX)
        target_link_libraries("${PROJECT_NAME}_example" -rdynamic)
    endif()

    add_executable("${PROJECT_NAME}_example_other" "other.cpp")
    target_link_libraries("${PROJECT_NAME}_example_other" ${PROJECT_NAME})
endif()

if(NES_BUILD_TESTS)
    add_executable("${PROJECT_NAME}_test" "tests/process.cpp")
    target_link_libraries("${PROJECT_NAME}_test" ${PROJECT_NAME})

    add_executable("${PROJECT_NAME}_test_other" "tests/process_other.cpp")
    target_link_libraries("${PROJECT_NAME}_test_other" ${PROJECT_NAME})

    add_library("${PROJECT_NAME}_test_lib" SHARED "tests/library.cpp")
    set_target_properties("${PROJECT_NAME}_test_lib" PROPERTIES PREFIX "")
    target_link_libraries("${PROJECT_NAME}_test_lib" ${PROJECT_NAME})
endif()
