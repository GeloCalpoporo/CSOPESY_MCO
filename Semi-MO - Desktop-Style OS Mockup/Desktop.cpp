#include "Desktop.h"
#include "Taskbar.h"
#include <chrono>
#include <ctime>

void RenderDesktop(GLFWwindow* window, GLuint bg_texture) {
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(ImVec2(
        viewport->WorkSize.x,
        viewport->WorkSize.y - TASKBAR_HEIGHT
    ));

    ImGuiWindowFlags desktop_flags =
        ImGuiWindowFlags_NoDecoration          |
        ImGuiWindowFlags_NoMove                |
        ImGuiWindowFlags_NoSavedSettings       |
        ImGuiWindowFlags_NoBringToFrontOnFocus |
        ImGuiWindowFlags_NoScrollbar           |
        ImGuiWindowFlags_NoScrollWithMouse;

    ImGui::Begin("Desktop Canvas", nullptr, desktop_flags);

    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    ImVec2 p_min = ImGui::GetCursorScreenPos();
    ImVec2 p_max = ImVec2(
        p_min.x + ImGui::GetContentRegionAvail().x,
        p_min.y + ImGui::GetContentRegionAvail().y
    );

    if (bg_texture != 0) {
        draw_list->AddImage((void*)(intptr_t)bg_texture, p_min, p_max);
    } else {
        draw_list->AddRectFilledMultiColor(
            p_min, p_max,
            IM_COL32(15, 30, 50, 255), IM_COL32(30, 60, 90, 255),
            IM_COL32(10, 15, 25, 255), IM_COL32(10, 15, 25, 255)
        );
    }

    // Real-Time Clock (top right)
    auto now = std::chrono::system_clock::now();
    std::time_t current_time = std::chrono::system_clock::to_time_t(now);
    struct tm time_info;
    localtime_s(&time_info, &current_time);

    char time_str[64];
    std::strftime(time_str, sizeof(time_str), "%A, %b %d, %Y | %I:%M %p", &time_info);

    ImVec2 text_size  = ImGui::CalcTextSize(time_str);
    float  padding_x  = 10.0f;
    float  padding_y  = 5.0f;

    ImVec2 text_pos(ImGui::GetWindowWidth() - text_size.x - padding_x - 20, 20 + padding_y);
    ImGui::SetCursorPos(text_pos);

    ImVec2 screen_pos = ImGui::GetCursorScreenPos();
    ImVec2 rect_min   = ImVec2(screen_pos.x - padding_x, screen_pos.y - padding_y);
    ImVec2 rect_max   = ImVec2(screen_pos.x + text_size.x + padding_x, screen_pos.y + text_size.y + padding_y);
    ImGui::GetWindowDrawList()->AddRectFilled(rect_min, rect_max, IM_COL32(0, 0, 0, 150), 5.0f);
    ImGui::Text("%s", time_str);

    // PWR button removed from here — it now lives in the Taskbar (Member 2)

    ImGui::End();
}