#include "Bootloader.h"

void RenderBIOS(float elapsed_time) {
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings;

    // Force a pitch black background for the BIOS
    ImGui::PushStyleColor(ImGuiCol_WindowBg, IM_COL32(0, 0, 0, 255));
    ImGui::Begin("BIOS", nullptr, flags);

    // Set text color to a retro light gray
    ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(200, 200, 200, 255));

    ImGui::Text("CSOPESY Core BIOS v6.7");
    ImGui::Text("Copyright (C) 2026, Core Architect Systems");
    ImGui::Text(" "); // Spacing

    // Stagger the text appearance based on the stopwatch!
    if (elapsed_time > 0.8f) ImGui::Text("Initializing CPU Multiprocessing... OK");
    if (elapsed_time > 1.6f) ImGui::Text("Checking RAM... 16384 MB OK");
    if (elapsed_time > 2.4f) ImGui::Text("Mounting Virtual File System... OK");
    if (elapsed_time > 3.2f) ImGui::Text("Loading CSOPESY Kernel... OK");

    ImGui::PopStyleColor(2);
    ImGui::End();
}

void RenderSplash(float elapsed_time) {
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings;

    ImGui::PushStyleColor(ImGuiCol_WindowBg, IM_COL32(15, 15, 15, 255));
    ImGui::Begin("Splash", nullptr, flags);

    // Center the ASCII/Title text
    ImVec2 window_size = ImGui::GetWindowSize();

    // Placeholder ASCII / Title (You can replace this with real ASCII art later!)
    const char* title = R"(
       ____ ____   ___  ____  _____ ______   __
      / ___/ ___| / _ \|  _ \| ____/ ___\ \ / /
     | |   \___ \| | | | |_) |  _| \___ \\ V / 
     | |___ ___) | |_| |  __/| |___ ___) || |  
      \____|____/ \___/|_|   |_____|____/ |_|  
    )";
    const char* subtitle = "Desktop OS Emulator";
    const char* members = "KELVIN ALVIAR | ANGELO CALPOPORO | MIGUEL CARLOS | NIO TUJAN";

    // Calculate the width/height of the entire ASCII block
    ImVec2 title_size = ImGui::CalcTextSize(title);

    // Center it horizontally, and place it slightly higher vertically
    ImGui::SetCursorPos(ImVec2((window_size.x - title_size.x) * 0.5f, window_size.y * 0.3f));
    ImGui::Text("%s", title);

    ImGui::SetCursorPos(ImVec2((window_size.x - ImGui::CalcTextSize(subtitle).x) * 0.5f, window_size.y * 0.45f));
    ImGui::Text("%s", subtitle);

    // Set text to a subdued color for the member names
    ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(150, 150, 150, 255));
    ImGui::SetCursorPos(ImVec2((window_size.x - ImGui::CalcTextSize(members).x) * 0.5f, window_size.y * 0.55f));
    ImGui::Text("%s", members);
    ImGui::PopStyleColor();

    // Blinking "Loading..." Text at the bottom
    // Math trick: multiply time by 2, cast to int. If even, show text. If odd, hide text.
    if ((int)(elapsed_time * 2.0f) % 2 == 0) {
        const char* loading = "Loading System UI...";
        ImGui::SetCursorPos(ImVec2((window_size.x - ImGui::CalcTextSize(loading).x) * 0.5f, window_size.y * 0.8f));
        ImGui::Text("%s", loading);
    }

    ImGui::PopStyleColor();
    ImGui::End();
}