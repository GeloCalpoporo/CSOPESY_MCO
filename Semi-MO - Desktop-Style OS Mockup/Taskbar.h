#pragma once
#include "imgui.h"
#include <GLFW/glfw3.h>

static const float TASKBAR_HEIGHT = 54.0f;

struct AppState {
    bool show_task_manager = false;
    bool show_app1         = false;
    bool show_app2         = false;
};

// PNG icon textures loaded in main.cpp and passed into RenderTaskbar
struct TaskbarIcons {
    GLuint files   = 0;   // icon_files.png   — File Explorer button
    GLuint init    = 0;   // icon_init.png    — INIT button
    GLuint taskmgr = 0;   // icon_taskmgr.png — Task Manager button
};

void RenderTaskbar(AppState& state, GLFWwindow* window, const TaskbarIcons& icons);
void RenderTaskManager(bool* p_open);