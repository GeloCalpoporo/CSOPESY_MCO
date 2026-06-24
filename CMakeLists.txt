# 1. Set the minimum version of CMake required
cmake_minimum_required(VERSION 3.15)

# 2. Name your project
project(CSOPESY_Emulator)

# 3. Enforce the C++20 Standard requested by your professor
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# 4. Target the x64 architecture
set(CMAKE_GENERATOR_PLATFORM x64)

# 5. Tell CMake to build an executable (.exe) using your specific source code
add_executable(CSOPESY_Emulator CSOPESY_Emulator/main.cpp)