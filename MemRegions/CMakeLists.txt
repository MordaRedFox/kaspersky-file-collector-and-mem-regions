cmake_minimum_required(VERSION 3.16)
project(MemRegions LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if (NOT WIN32)
    message(FATAL_ERROR "MemRegions is a Windows-only project.")
endif()

set(MEMREGIONS_HEADERS
    include/memregions/common.h
    include/memregions/protection.h
    include/memregions/process_locator.h
    include/memregions/region_scanner.h
    include/memregions/reporter.h
    include/memregions/application.h
)

set(MEMREGIONS_SOURCES
    src/main.cpp
    src/application.cpp
    src/process_locator.cpp
    src/protection.cpp
    src/region_scanner.cpp
    src/reporter.cpp
)

add_executable(MemRegions ${MEMREGIONS_SOURCES} ${MEMREGIONS_HEADERS})

target_include_directories(MemRegions PRIVATE include)

if (MSVC)
    target_compile_options(MemRegions PRIVATE /W4 /permissive-)
else()
    target_compile_options(MemRegions PRIVATE -Wall -Wextra -Wpedantic)
endif()