cmake_minimum_required(VERSION 3.16)
project(file_collector LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Threads REQUIRED)

add_library(file_collector STATIC src/file_collector.cpp)
target_include_directories(file_collector PUBLIC include)
target_compile_features(file_collector PUBLIC cxx_std_20)
target_link_libraries(file_collector PUBLIC Threads::Threads)

option(FILE_COLLECTOR_BUILD_TESTS "Build unit tests" ON)
if(FILE_COLLECTOR_BUILD_TESTS)
    enable_testing()
    include(FetchContent)

    find_package(GTest QUIET)
    if(NOT GTest_FOUND)
        FetchContent_Declare(
            googletest
            GIT_REPOSITORY https://github.com/google/googletest.git
            GIT_TAG        v1.14.0
            GIT_SHALLOW    TRUE
        )
        set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
        FetchContent_MakeAvailable(googletest)
    endif()

    add_executable(file_collector_tests tests/file_collector_test.cpp)
    target_link_libraries(file_collector_tests PRIVATE file_collector GTest::gtest_main)

    include(GoogleTest)
    gtest_discover_tests(file_collector_tests)
endif()