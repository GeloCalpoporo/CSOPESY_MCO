# 1. Set the correct minimum CMake version
cmake_minimum_required(VERSION 3.15)

project(HOMEWORK)

# 2. Enforce the C++20 Standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# 3. Enforce x64 architecture
set(CMAKE_GENERATOR_PLATFORM x64)

# 4. Build the executable
add_executable(HOMEWORK
        main.cpp)