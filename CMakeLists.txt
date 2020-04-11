cmake_minimum_required(VERSION 3.8)

project(not_enough_standards)

option(NES_BUILD_EXAMPLES ON)

set(NES_INCLUDE_DIR "${PROJECT_SOURCE_DIR}/include")
set(NES_HEADERS "${NES_INCLUDE_DIR}/nes/shared_library.hpp"
                "${NES_INCLUDE_DIR}/nes/shared_memory.hpp"
                "${NES_INCLUDE_DIR}/nes/named_mutex.hpp"
                "${NES_INCLUDE_DIR}/nes/semaphore.hpp"
                "${NES_INCLUDE_DIR}/nes/named_semaphore.hpp"
                "${NES_INCLUDE_DIR}/nes/pipe.hpp"
                "${NES_INCLUDE_DIR}/nes/process.hpp"
                "${NES_INCLUDE_DIR}/nes/hash.hpp")

add_library(${PROJECT_NAME} INTERFACE)
target_include_directories(${PROJECT_NAME} INTERFACE NES_INCLUDE_DIR)
target_sources(${PROJECT_NAME} INTERFACE ${NES_HEADERS})

if(UNIX)
    target_link_libraries(${PROJECT_NAME} INTERFACE pthread rt dl)
endif()

if(NES_BUILD_EXAMPLES)
    add_executable("${PROJECT_NAME}_test" "main.cpp")
    target_link_libraries("${PROJECT_NAME}_test" ${PROJECT_NAME})
    target_include_directories("${PROJECT_NAME}_test" PRIVATE ${NES_INCLUDE_DIR})
    if(UNIX)
        target_link_libraries("${PROJECT_NAME}_test" -rdynamic)
    endif()

    add_executable("${PROJECT_NAME}_other" "other.cpp")
    target_link_libraries("${PROJECT_NAME}_other" ${PROJECT_NAME})
    target_include_directories("${PROJECT_NAME}_other" PRIVATE ${NES_INCLUDE_DIR})
endif()
