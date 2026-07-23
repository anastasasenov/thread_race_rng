cmake_minimum_required(VERSION 3.10)

# Test project name
set(TEST_PRJ thread_race_rng_test)
project(${TEST_PRJ} C)

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
find_library(MATH_LIBRARY m)

# test executable
add_executable(${TEST_PRJ} test.c)

# link_pthread
target_link_libraries(${TEST_PRJ} PRIVATE Threads::Threads)
target_link_libraries(${TEST_PRJ} PRIVATE ${MATH_LIBRARY})

