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

# 5. Copy config.txt next to the built executable every build
add_custom_command(TARGET HOMEWORK POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                ${CMAKE_SOURCE_DIR}/config.txt
                $<TARGET_FILE_DIR:HOMEWORK>)