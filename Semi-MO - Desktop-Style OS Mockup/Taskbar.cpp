#include "Taskbar.h"
#include <cmath>
#include <cstdio>

#define ICON_FA_PLAY   "\xef\x81\x8b"
#define ICON_FA_STOP   "\xef\x81\x8d"
#define ICON_FA_MAGNIFY "\xef\x80\x82"
#define ICON_FA_VOLUME  "\xef\x80\xa8"
#define ICON_FA_WIFI    "\xef\x87\xab"
#define ICON_FA_POWER   "\xef\x80\x91"

// ============================================================
// INTERNAL: Fake process data
// ============================================================
struct ProcessEntry { const char* name; float cpu_base; float mem_mb; };

static ProcessEntry processes[] = {
    { "csopesy.exe",    3.2f,  128.0f },
    { "System",         0.4f,   64.0f },
    { "explorer.exe",   1.1f,  212.0f },
    { "chrome.exe",    12.5f,  890.0f },
    { "discord.exe",    4.7f,  320.0f },
    { "spotify.exe",    2.3f,  185.0f },
    { "svchost.exe",    0.8f,   96.0f },
    { "antivirus.exe",  1.9f,  144.0f },
    { "winlogon.exe",   0.1f,   32.0f },
    { "taskmgr.exe",    0.6f,   48.0f },
};
static const int PROCESS_COUNT = (int)(sizeof(processes) / sizeof(processes[0]));

static float Jitter(float base, float range, float time, float seed) {
    return base
        + std::sin(time * 1.7f + seed)        * range * 0.5f
        + std::sin(time * 3.1f + seed * 2.0f) * range * 0.3f;
}

// Plain text/FA icon coloured button
static bool IconButton(const char* label, ImVec2 size,
                       ImU32 bg_normal, ImU32 bg_hovered, ImU32 text_col)
{
    ImGui::PushStyleColor(ImGuiCol_Button,        ImGui::ColorConvertU32ToFloat4(bg_normal));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImGui::ColorConvertU32ToFloat4(bg_hovered));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImGui::ColorConvertU32ToFloat4(bg_normal));
    ImGui::PushStyleColor(ImGuiCol_Text,          ImGui::ColorConvertU32ToFloat4(text_col));
    bool clicked = ImGui::Button(label, size);
    ImGui::PopStyleColor(4);
    return clicked;
}

// PNG button — draws colored bg + PNG image + invisible click area
static bool PngButton(GLuint tex, ImVec2 btn_size, ImVec2 icon_size,
                      ImU32 bg_normal, ImU32 bg_hovered,
                      const char* tooltip = nullptr)
{
    ImVec2 pos = ImGui::GetCursorScreenPos();
    ImDrawList* dl = ImGui::GetWindowDrawList();

    bool hovered = ImGui::IsMouseHoveringRect(pos,
                       ImVec2(pos.x + btn_size.x, pos.y + btn_size.y));
    dl->AddRectFilled(pos,
                      ImVec2(pos.x + btn_size.x, pos.y + btn_size.y),
                      hovered ? bg_hovered : bg_normal,
                      7.0f);

    // Draw PNG image centered in the button
    if (tex != 0) {
        float off_x = (btn_size.x - icon_size.x) * 0.5f;
        float off_y = (btn_size.y - icon_size.y) * 0.5f;
        ImVec2 img_min = ImVec2(pos.x + off_x, pos.y + off_y);
        ImVec2 img_max = ImVec2(img_min.x + icon_size.x, img_min.y + icon_size.y);
        // ImTextureID is uint64 in this ImGui version — cast via ImTextureID constructor
        ImTextureID tid = (ImTextureID)(ImU64)tex;
        dl->AddImage(tid, img_min, img_max);
    }

    ImGui::InvisibleButton("##pb", btn_size);
    bool clicked = ImGui::IsItemClicked();

    if (tooltip && ImGui::IsItemHovered())
        ImGui::SetTooltip("%s", tooltip);

    return clicked;
}

// Slim vertical separator
static void VSep(float btn_h) {
    ImGui::SameLine();
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 4.0f);
    ImVec2 sp = ImGui::GetCursorScreenPos();
    ImGui::GetWindowDrawList()->AddLine(
        ImVec2(sp.x, sp.y),
        ImVec2(sp.x, sp.y + btn_h),
        IM_COL32(45, 52, 70, 255), 1.0f
    );
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 10.0f);
}

// ============================================================
// COMPONENT 2: THE TASKBAR
// ============================================================
void RenderTaskbar(AppState& state, GLFWwindow* window, const TaskbarIcons& icons) {
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    float bar_y = viewport->WorkPos.y + viewport->WorkSize.y - TASKBAR_HEIGHT;
    ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x, bar_y));
    ImGui::SetNextWindowSize(ImVec2(viewport->WorkSize.x, TASKBAR_HEIGHT));

    ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoDecoration          |
        ImGuiWindowFlags_NoMove                |
        ImGuiWindowFlags_NoSavedSettings       |
        ImGuiWindowFlags_NoBringToFrontOnFocus |
        ImGuiWindowFlags_NoScrollbar           |
        ImGuiWindowFlags_NoScrollWithMouse;

    ImGui::PushStyleColor(ImGuiCol_WindowBg, IM_COL32(40, 40, 40, 180));
    ImGui::Begin("##Taskbar", nullptr, flags);

    // Top accent line
    ImVec2 wp = ImGui::GetWindowPos();
    ImGui::GetWindowDrawList()->AddLine(
        wp, ImVec2(wp.x + ImGui::GetWindowWidth(), wp.y),
        IM_COL32(55, 62, 82, 255), 1.5f
    );

    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 7.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,   ImVec2(5.0f, 0.0f));

    float btn_h   = 36.0f;
    float btn_y   = (TASKBAR_HEIGHT - btn_h) * 0.5f;
    float icon_sz = 24.0f;  // rendered icon size inside each button
    ImVec2 btn_sq = ImVec2(btn_h, btn_h);
    ImVec2 icon_v = ImVec2(icon_sz, icon_sz);

    ImGui::SetCursorPos(ImVec2(8.0f, btn_y));

    // ── [FILES] PNG image button ──────────────────────────────────────
    ImGui::PushID(1);
    if (PngButton(icons.files, btn_sq, icon_v,
                  IM_COL32(38, 30, 8,  255),
                  IM_COL32(55, 44, 12, 255),
                  "File Explorer"))
        state.show_app1 = !state.show_app1;
    ImGui::PopID();

    VSep(btn_h);
    ImGui::SameLine(); ImGui::SetCursorPosY(btn_y);

    // ── [INIT] PNG image button ───────────────────────────────────────
    ImGui::PushID(2);
    if (PngButton(icons.init, btn_sq, icon_v,
                  IM_COL32(22, 25, 35, 255),
                  IM_COL32(34, 38, 54, 255),
                  "Initialize"))
    { /* INIT action */ }
    ImGui::PopID();

    ImGui::SameLine(); ImGui::SetCursorPosY(btn_y);

    // ── START — FA play icon, green ───────────────────────────────────
    IconButton(ICON_FA_PLAY "  START", ImVec2(90, btn_h),
               IM_COL32(10, 38, 18, 255),
               IM_COL32(14, 54, 26, 255),
               IM_COL32(74, 222, 128, 255));
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Start scheduler");
    ImGui::SameLine(); ImGui::SetCursorPosY(btn_y);

    // ── STOP — FA stop icon, red ──────────────────────────────────────
    IconButton(ICON_FA_STOP "  STOP", ImVec2(84, btn_h),
               IM_COL32(38, 10, 10, 255),
               IM_COL32(54, 14, 14, 255),
               IM_COL32(248, 113, 113, 255));
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Stop scheduler");

    VSep(btn_h);
    ImGui::SameLine(); ImGui::SetCursorPosY(btn_y);

    // ── [TASK MGR] PNG image button ───────────────────────────────────
    ImU32 tm_bg  = state.show_task_manager ? IM_COL32(10,30,58,255) : IM_COL32(22,25,35,255);
    ImU32 tm_hov = state.show_task_manager ? IM_COL32(14,44,78,255) : IM_COL32(34,38,54,255);
    ImGui::PushID(3);
    if (PngButton(icons.taskmgr, btn_sq, icon_v, tm_bg, tm_hov, "Task Manager"))
        state.show_task_manager = !state.show_task_manager;
    ImGui::PopID();

    ImGui::SameLine(); ImGui::SetCursorPosY(btn_y);

    // ── SRCH — FA magnifier ───────────────────────────────────────────
    if (IconButton(ICON_FA_MAGNIFY "  SRCH", ImVec2(84, btn_h),
                   IM_COL32(22, 25, 35, 255),
                   IM_COL32(34, 38, 54, 255),
                   IM_COL32(160, 165, 195, 255)))
        state.show_app2 = !state.show_app2;
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Search / Settings");

    // ── RIGHT SIDE — VOL  NET  |  PWR ────────────────────────────────
    // Calculate exact right-side position so PWR stays fully on screen
    // PWR=68, sep=15, NET=68, VOL=68, gap=8 from right edge
    float pwr_w   = 68.0f;
    float net_w   = 68.0f;
    float vol_w   = 68.0f;
    float sep_w   = 15.0f;
    float margin  = 8.0f;
    float right_x = ImGui::GetWindowWidth() - margin - pwr_w - sep_w - net_w - vol_w;

    ImGui::SetCursorPos(ImVec2(right_x, btn_y));

    IconButton(ICON_FA_VOLUME "  VOL", ImVec2(vol_w, btn_h),
               IM_COL32(22, 25, 35, 255),
               IM_COL32(34, 38, 54, 255),
               IM_COL32(140, 148, 175, 255));
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Volume");
    ImGui::SameLine(); ImGui::SetCursorPosY(btn_y);

    IconButton(ICON_FA_WIFI "  NET", ImVec2(net_w, btn_h),
               IM_COL32(22, 25, 35, 255),
               IM_COL32(34, 38, 54, 255),
               IM_COL32(140, 148, 175, 255));
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Network");

    VSep(btn_h);
    ImGui::SameLine(); ImGui::SetCursorPosY(btn_y);

    // PWR — always fully visible
    ImGui::PushStyleColor(ImGuiCol_Button,        IM_COL32(65, 12, 12, 255));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(95, 18, 18, 255));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,  IM_COL32(45, 8,  8,  255));
    ImGui::PushStyleColor(ImGuiCol_Text,          IM_COL32(248, 113, 113, 255));
    if (ImGui::Button(ICON_FA_POWER "  PWR", ImVec2(pwr_w, btn_h)))
        glfwSetWindowShouldClose(window, true);
    ImGui::PopStyleColor(4);
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Shut down");

    ImGui::PopStyleVar(2);
    ImGui::End();
    ImGui::PopStyleColor(); // WindowBg
}

// ============================================================
// COMPONENT 3: TASK MANAGER
// ============================================================
void RenderTaskManager(bool* p_open) {
    if (!p_open || !*p_open) return;

    float t = (float)ImGui::GetTime();

    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowSize(ImVec2(580, 440), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos(
        ImVec2(viewport->WorkPos.x + viewport->WorkSize.x * 0.5f - 290.0f,
               viewport->WorkPos.y + viewport->WorkSize.y * 0.5f - 220.0f),
        ImGuiCond_FirstUseEver
    );

    ImGui::PushStyleColor(ImGuiCol_WindowBg,      IM_COL32(14, 16, 22, 252));
    ImGui::PushStyleColor(ImGuiCol_TitleBgActive, IM_COL32(18, 45, 80, 255));
    ImGui::PushStyleColor(ImGuiCol_Tab,           IM_COL32(22, 25, 35, 255));
    ImGui::PushStyleColor(ImGuiCol_TabActive,     IM_COL32(18, 45, 80, 255));
    ImGui::PushStyleColor(ImGuiCol_TabHovered,    IM_COL32(30, 60, 100, 255));
    ImGui::PushStyleColor(ImGuiCol_TableHeaderBg, IM_COL32(22, 25, 38, 255));
    ImGui::PushStyleColor(ImGuiCol_TableRowBg,    IM_COL32(18, 20, 28, 255));
    ImGui::PushStyleColor(ImGuiCol_TableRowBgAlt, IM_COL32(22, 25, 35, 255));

    if (!ImGui::Begin("Task Manager", p_open, ImGuiWindowFlags_NoCollapse)) {
        ImGui::End();
        ImGui::PopStyleColor(8);
        return;
    }

    float total_cpu = 0.0f, total_mem = 0.0f;
    for (int i = 0; i < PROCESS_COUNT; i++) {
        float c = Jitter(processes[i].cpu_base, 2.0f,  t, (float)i * 7.3f);
        float m = Jitter(processes[i].mem_mb,   20.0f, t, (float)i * 3.7f);
        if (c < 0.0f) c = 0.0f;
        if (m < 0.0f) m = 0.0f;
        total_cpu += c;
        total_mem += m;
    }
    if (total_cpu > 100.0f) total_cpu = 100.0f;

    ImGui::PushStyleColor(ImGuiCol_ChildBg, IM_COL32(18, 22, 34, 255));
    ImGui::BeginChild("##summary", ImVec2(-1, 52), false, ImGuiWindowFlags_NoScrollbar);
    ImGui::SetCursorPos(ImVec2(10, 8));

    ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(96, 165, 250, 255));
    ImGui::Text("CPU");
    ImGui::PopStyleColor();
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(220, 225, 240, 255));
    ImGui::Text("%.1f%%", total_cpu);
    ImGui::PopStyleColor();

    ImGui::SameLine(160);
    ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(74, 222, 128, 255));
    ImGui::Text("MEM");
    ImGui::PopStyleColor();
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(220, 225, 240, 255));
    ImGui::Text("%.0f / 16384 MB", total_mem);
    ImGui::PopStyleColor();

    ImGui::SameLine(420);
    ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(248, 113, 113, 255));
    ImGui::Text("PROC");
    ImGui::PopStyleColor();
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(220, 225, 240, 255));
    ImGui::Text("%d running", PROCESS_COUNT);
    ImGui::PopStyleColor();

    ImGui::SetCursorPos(ImVec2(10, 34));
    ImGui::PushStyleColor(ImGuiCol_PlotHistogram, IM_COL32(96, 165, 250, 200));
    ImGui::PushStyleColor(ImGuiCol_FrameBg,       IM_COL32(25, 30, 48, 255));
    ImGui::ProgressBar(total_cpu / 100.0f, ImVec2(ImGui::GetContentRegionAvail().x - 10, 8), "");
    ImGui::PopStyleColor(2);
    ImGui::EndChild();
    ImGui::PopStyleColor();
    ImGui::Spacing();

    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 5.0f);
    if (ImGui::BeginTabBar("TM_Tabs")) {

        if (ImGui::BeginTabItem("  Processes  ")) {
            ImGui::Spacing();
            ImGuiTableFlags tflags =
                ImGuiTableFlags_Borders        |
                ImGuiTableFlags_RowBg          |
                ImGuiTableFlags_ScrollY        |
                ImGuiTableFlags_SizingFixedFit |
                ImGuiTableFlags_Resizable;

            if (ImGui::BeginTable("ProcTable", 4, tflags,
                                  ImVec2(0.0f, ImGui::GetContentRegionAvail().y - 6.0f)))
            {
                ImGui::TableSetupScrollFreeze(0, 1);
                ImGui::TableSetupColumn("Process Name", ImGuiTableColumnFlags_WidthStretch, 200.0f);
                ImGui::TableSetupColumn("CPU (%)",      ImGuiTableColumnFlags_WidthFixed,    75.0f);
                ImGui::TableSetupColumn("Mem (MB)",     ImGuiTableColumnFlags_WidthFixed,    90.0f);
                ImGui::TableSetupColumn("Status",       ImGuiTableColumnFlags_WidthFixed,    72.0f);
                ImGui::TableHeadersRow();

                for (int i = 0; i < PROCESS_COUNT; i++) {
                    float cpu_v = Jitter(processes[i].cpu_base, 2.0f,  t, (float)i * 7.3f);
                    float mem_v = Jitter(processes[i].mem_mb,   20.0f, t, (float)i * 3.7f);
                    if (cpu_v < 0.0f) cpu_v = 0.0f;
                    if (mem_v < 0.0f) mem_v = 0.0f;

                    ImGui::TableNextRow();

                    ImGui::TableSetColumnIndex(0);
                    ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(200, 205, 220, 255));
                    ImGui::TextUnformatted(processes[i].name);
                    ImGui::PopStyleColor();

                    ImGui::TableSetColumnIndex(1);
                    ImU32 cc = (cpu_v > 8.0f) ? IM_COL32(248,113,113,255)
                             : (cpu_v > 3.0f) ? IM_COL32(251,191, 36,255)
                                              : IM_COL32( 74,222,128,255);
                    ImGui::PushStyleColor(ImGuiCol_Text, cc);
                    ImGui::Text("%.1f", cpu_v);
                    ImGui::PopStyleColor();

                    ImGui::TableSetColumnIndex(2);
                    ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(160, 165, 185, 255));
                    ImGui::Text("%.0f", mem_v);
                    ImGui::PopStyleColor();

                    ImGui::TableSetColumnIndex(3);
                    ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(74, 222, 128, 255));
                    ImGui::TextUnformatted("Running");
                    ImGui::PopStyleColor();
                }
                ImGui::EndTable();
            }
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("  Performance  ")) {
            ImGui::Spacing();

            static float cpu_history[90] = {};
            static int   hist_offset = 0;
            static float hist_timer  = 0.0f;

            hist_timer += ImGui::GetIO().DeltaTime;
            if (hist_timer >= 0.1f) {
                hist_timer = 0.0f;
                float s = 0.0f;
                for (int i = 0; i < PROCESS_COUNT; i++)
                    s += Jitter(processes[i].cpu_base, 2.0f, t, (float)i * 7.3f);
                if (s > 100.0f) s = 100.0f;
                if (s < 0.0f)   s = 0.0f;
                cpu_history[hist_offset] = s;
                hist_offset = (hist_offset + 1) % 90;
            }

            ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(96, 165, 250, 255));
            ImGui::Text("CPU Usage  %.1f%%", total_cpu);
            ImGui::PopStyleColor();

            ImGui::PushStyleColor(ImGuiCol_PlotLines, IM_COL32(96, 165, 250, 220));
            ImGui::PushStyleColor(ImGuiCol_FrameBg,   IM_COL32(12, 16, 28, 255));
            ImGui::PlotLines("##cpugraph", cpu_history, 90, hist_offset,
                             nullptr, 0.0f, 100.0f, ImVec2(-1.0f, 115.0f));
            ImGui::PopStyleColor(2);

            ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();

            ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(74, 222, 128, 255));
            ImGui::Text("Memory Usage");
            ImGui::PopStyleColor();

            char overlay[48];
            snprintf(overlay, sizeof(overlay), "%.0f / 16384 MB", total_mem);
            ImGui::PushStyleColor(ImGuiCol_PlotHistogram, IM_COL32(74, 222, 128, 200));
            ImGui::PushStyleColor(ImGuiCol_FrameBg,       IM_COL32(10, 22, 14, 255));
            ImGui::ProgressBar(total_mem / 16384.0f, ImVec2(-1.0f, 26.0f), overlay);
            ImGui::PopStyleColor(2);

            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }
    ImGui::PopStyleVar();

    ImGui::End();
    ImGui::PopStyleColor(8);
}