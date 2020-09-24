cmake_minimum_required(VERSION 3.8)

project(not_enough_standards)

option(NES_BUILD_EXAMPLES ON)

set(NES_HEADERS ${PROJECT_SOURCE_DIR}/include/nes/shared_library.hpp
                ${PROJECT_SOURCE_DIR}/include/nes/shared_memory.hpp
                ${PROJECT_SOURCE_DIR}/include/nes/named_mutex.hpp
                ${PROJECT_SOURCE_DIR}/include/nes/semaphore.hpp
                ${PROJECT_SOURCE_DIR}/include/nes/named_semaphore.hpp
                ${PROJECT_SOURCE_DIR}/include/nes/pipe.hpp
                ${PROJECT_SOURCE_DIR}/include/nes/process.hpp
                ${PROJECT_SOURCE_DIR}/include/nes/hash.hpp
                ${PROJECT_SOURCE_DIR}/include/nes/thread_pool.hpp
                ${PROJECT_SOURCE_DIR}/include/nes/math.hpp)

add_library(${PROJECT_NAME} INTERFACE)

target_include_directories(${PROJECT_NAME} INTERFACE
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

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
