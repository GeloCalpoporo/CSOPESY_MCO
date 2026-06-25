# 1. Minimum version of CMake required
cmake_minimum_required(VERSION 3.15)

# 2. Project Name
project(CSOPESY_Emulator)

# 3. Enforce the C++20 Standard 
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# 4. x64 architecture
set(CMAKE_GENERATOR_PLATFORM x64)

# 5. build an executable (.exe) specific source code
add_executable(CSOPESY_Emulator 
	CSOPESY_Emulator/main.cpp 
	"CSOPESY_Emulator/Config.h" 
	"CSOPESY_Emulator/Process.h" 
	"CSOPESY_Emulator/Scheduler.h" 
	"CSOPESY_Emulator/ConsoleManager.h" 
	"CSOPESY_Emulator/ConsoleManager.cpp"
	# Member 2: add CSOPESY_Emulator/Process.cpp here when ready
    # Member 3: add CSOPESY_Emulator/Scheduler.cpp here when ready
	 "CSOPESY_Emulator/Process.cpp"	"CSOPESY_Emulator/Scheduler.cpp" # Only added this line to avoid compilation errors due to missing implementation
)

target_include_directories(CSOPESY_Emulator PRIVATE CSOPESY_Emulator)
