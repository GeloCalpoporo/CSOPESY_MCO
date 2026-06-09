#include "Desktop.h"
#include <chrono>
#include <ctime>

void RenderDesktop(GLFWwindow* window, GLuint bg_texture) {
    // Force the next window to match the exact size and position of the main viewport
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);

    // Set flags to make this window act like a static background wallpaper
    ImGuiWindowFlags desktop_flags =
        ImGuiWindowFlags_NoDecoration |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoBringToFrontOnFocus;

    // Begin the Desktop window
    ImGui::Begin("Desktop Canvas", nullptr, desktop_flags);

    // Draw the custom wallpaper
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    ImVec2 p_min = ImGui::GetCursorScreenPos(); // Top left corner of the window
    ImVec2 p_max = ImVec2(p_min.x + ImGui::GetContentRegionAvail().x, p_min.y + ImGui::GetContentRegionAvail().y);

    // Draw the Image Texture stretched over the Desktop Canvas
    if (bg_texture != 0) {
        draw_list->AddImage((void*)(intptr_t)bg_texture, p_min, p_max);
    }
    else {
        // Fallback gradient just in case the image fails to load
        draw_list->AddRectFilledMultiColor(p_min, p_max, IM_COL32(15, 30, 50, 255), IM_COL32(30, 60, 90, 255), IM_COL32(10, 15, 25, 255), IM_COL32(10, 15, 25, 255));
    }

    // Real-Time Clock (Fixed Top Right Corner)
    auto now = std::chrono::system_clock::now();
    std::time_t current_time = std::chrono::system_clock::to_time_t(now);
    struct tm time_info;
    localtime_s(&time_info, &current_time); // localtime for Windows

    // FORMAT: "Thursday, Apr 30, 2026  05:00 PM"
    // %A = Full Weekday, %b = Abbreviated Month, %d = Day, %Y = Year, %I = 12-Hour, %M = Minute, %p = AM/PM
    char time_str[64];
    std::strftime(time_str, sizeof(time_str), "%A, %b %d, %Y | %I:%M %p", &time_info);

    // Calculate text width to right align it
    ImVec2 text_size = ImGui::CalcTextSize(time_str);

    // Padding for the background highlight box
    float padding_x = 10.0f;
    float padding_y = 5.0f;

    // Position where the text itself will start 
    ImVec2 text_pos(ImGui::GetWindowWidth() - text_size.x - padding_x - 20, 20 + padding_y);
    ImGui::SetCursorPos(text_pos);

    // Visual Styling: Background Highlight Box
    // Get the absolute screen coordinates where the text is about to be drawn
    ImVec2 screen_pos = ImGui::GetCursorScreenPos();

    // Calculate the bounding box for the background (text size + padding)
    ImVec2 rect_min = ImVec2(screen_pos.x - padding_x, screen_pos.y - padding_y);
    ImVec2 rect_max = ImVec2(screen_pos.x + text_size.x + padding_x, screen_pos.y + text_size.y + padding_y);

    // Draw a semi-transparent black rectangle as background (radius 5.0f)
    ImGui::GetWindowDrawList()->AddRectFilled(rect_min, rect_max, IM_COL32(0, 0, 0, 150), 5.0f);
    
    // Render the time text on top of the background box
    ImGui::Text("%s", time_str);

    // The PWR Button (Fixed Bottom Right Corner)
    ImGui::SetCursorPos(ImVec2(ImGui::GetWindowWidth() - 70, ImGui::GetWindowHeight() - 50));

    // Push red color styling for this button 
    ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(200, 50, 50, 255));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(255, 80, 80, 255));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, IM_COL32(150, 30, 30, 255));

    //Render the button and check if it was clicked 
    if (ImGui::Button("PWR", ImVec2(50, 30))) {
        //Tell GLFW to break out of the while loop
        glfwSetWindowShouldClose(window, true);
    }

    ImGui::PopStyleColor(3); // Pop the 3 button colors we pushed
    ImGui::End();
}