# 1. Set here the minimum CMake version and Project Name
cmake_minimum_required(VERSION 3.15)
project(CSOPESY_OS_Mockup)

# 2. Force C++ 20 standard for everyone (CLion, VS, VS Code)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 3. Tell CMake to find the GLFW library downloaded via vcpkg
find_package(glfw3 CONFIG REQUIRED)

# 4. Tell CMake to find the built-in OpenGL library on the user's computer
find_package(OpenGL REQUIRED)

# 5. Tell CMake where to look for ImGui header files
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/imgui)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/imgui) # (Backend headers are in the same folder now)

# 6. Gather all the ImGui source files
set(IMGUI_SOURCES
    imgui/imgui.cpp
    imgui/imgui_demo.cpp
    imgui/imgui_draw.cpp
    imgui/imgui_tables.cpp
    imgui/imgui_widgets.cpp
    imgui/imgui_impl_glfw.cpp
    imgui/imgui_impl_opengl3.cpp
)

# 7. Create the actual executable application out of main.cpp and ImGui
add_executable(OS_Mockup main.cpp ${IMGUI_SOURCES})

# 8. Link GLFW and OpenGL to the executable
target_link_libraries(OS_Mockup PRIVATE glfw OpenGL::GL)
