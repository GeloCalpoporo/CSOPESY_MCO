# 1. Minimum version of CMake required
cmake_minimum_required(VERSION 3.15)

# 2. Project Name
project(CSOPESY_Emulator)

# 3. Enforce the C++20 Standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# 4. x64 architecture
set(CMAKE_GENERATOR_PLATFORM x64)

# 5. build an executable (.exe) from the source code
add_executable(CSOPESY_Emulator
		CSOPESY_Emulator/main.cpp
		"CSOPESY_Emulator/Config.h"
		"CSOPESY_Emulator/Process.h"
		"CSOPESY_Emulator/Scheduler.h"
		"CSOPESY_Emulator/ConsoleManager.h"
		"CSOPESY_Emulator/ConsoleManager.cpp"
		"CSOPESY_Emulator/Process.cpp"
		"CSOPESY_Emulator/Scheduler.cpp"
)

# NOTE: target_include_directories(... CSOPESY_Emulator) was REMOVED on purpose.
# On MinGW + Windows (case-insensitive filesystem) it put the project folder on the
# angle-bracket include path, so MinGW's <process.h> (pulled in by pthread.h) resolved
# to our local "Process.h" and broke <mutex>. All our includes are quoted
# (#include "Process.h"), which already resolve relative to each file's own folder,
# so no include directory is needed.


