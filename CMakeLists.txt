cmake_minimum_required(VERSION 3.10)

# Project name
project(thread_race_rng_test C)

# C11
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF) # Set to ON if you need GNU C11 extensions

# compiler warnings
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -Wpedantic)
elseif(CMAKE_C_COMPILER_ID STREQUAL "MSVC")
    add_compile_options(/W4)
endif()

find_package(Threads REQUIRED)

# test executable
add_executable(thread_race_rng_test test.c)

# link_pthread
target_link_libraries(thread_race_rng_test PRIVATE Threads::Threads)
