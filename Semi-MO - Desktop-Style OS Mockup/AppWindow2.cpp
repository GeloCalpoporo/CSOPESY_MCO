#include "AppWindow2.h"
#include <cmath>
#include <cstdio>

// ============================================================
// COMPONENT: SYSTEM SETTINGS (App Window 2)
// A settings panel with tabbed sections:
//   Display | Sound | Network | About
// ============================================================

// Persistent settings state
static float  ss_brightness      = 80.0f;
static float  ss_contrast        = 50.0f;
static int    ss_resolution_idx  = 2;       // 0=1280x720, 1=1366x768, 2=1920x1080 ...
static bool   ss_vsync           = true;
static bool   ss_fullscreen      = false;
static float  ss_accent[3]       = { 0.376f, 0.773f, 0.502f }; // default green
static ImVec4 ss_accent_preview  = ImVec4(0.376f, 0.773f, 0.502f, 1.0f);

static float  ss_master_vol      = 75.0f;
static float  ss_music_vol       = 60.0f;
static float  ss_sfx_vol         = 85.0f;
static bool   ss_mute            = false;

static bool   ss_wifi_enabled    = true;
static int    ss_wifi_idx        = 1;       // selected network
static bool   ss_airplane        = false;

static bool   ss_notify_sys      = true;
static bool   ss_notify_app      = true;
static bool   ss_notify_sound    = true;
static float  ss_notify_timeout  = 5.0f;

// Preset resolution options
static const char* resolutions[] = {
    "1280 x 720",
    "1366 x 768",
    "1920 x 1080",
    "2560 x 1440",
    "3840 x 2160",
};

// Mock Wi-Fi networks
static const char* networks[] = {
    "DLSU-WiFi",
    "Home_Network_5G",
    "AndroidAP_3421",
    "CoffeeShop_Free",
    "HIDDEN NETWORK",
};
static const int NETWORK_COUNT = (int)(sizeof(networks) / sizeof(networks[0]));
// Signal strength (bars out of 4)
static const int  net_signal[] = { 4, 4, 2, 3, 1 };
// Lock icon indicator
static const bool net_locked[] = { true, true, false, false, true };

// Draw N filled / empty signal bars
static void DrawSignalBars(int bars, float bar_w=4.f, float bar_h_max=12.f, float gap=2.f) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 p = ImGui::GetCursorScreenPos();
    for (int i = 0; i < 4; i++) {
        float h = bar_h_max * (0.35f + 0.22f * (float)i);
        float x = p.x + (float)i * (bar_w + gap);
        float y_bot = p.y + bar_h_max;
        ImU32 col = (i < bars)
            ? IM_COL32(74, 222, 128, 255)   // filled — green
            : IM_COL32(50, 55, 75, 255);    // empty — dim
        dl->AddRectFilled(
            ImVec2(x, y_bot - h),
            ImVec2(x + bar_w, y_bot),
            col, 1.0f
        );
    }
    ImGui::Dummy(ImVec2((bar_w + gap) * 4.0f, bar_h_max));
}

// A labelled toggle switch drawn with ImDrawList
static bool ToggleSwitch(const char* id, bool* value) {
    const float w  = 36.0f, h = 18.0f, r = h * 0.5f;
    ImVec2 p = ImGui::GetCursorScreenPos();
    ImGui::InvisibleButton(id, ImVec2(w, h));
    bool clicked = ImGui::IsItemClicked();
    if (clicked) *value = !*value;

    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImU32 bg = *value ? IM_COL32(34, 150, 80, 255) : IM_COL32(50, 55, 75, 255);
    float t = *value ? 1.0f : 0.0f;
    float knob_x = p.x + r + t * (w - h);

    dl->AddRectFilled(p, ImVec2(p.x + w, p.y + h), bg, r);
    dl->AddCircleFilled(ImVec2(knob_x, p.y + h * 0.5f), r - 2.0f,
                        IM_COL32(220, 225, 240, 255));
    return clicked;
}

// Styled slider with colored track fill
static bool ColoredSliderFloat(const char* id, float* v, float vmin, float vmax,
                               ImU32 fill_color, float width = -1.0f) {
    ImGui::PushStyleColor(ImGuiCol_FrameBg,          IM_COL32(28, 32, 48, 255));
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered,   IM_COL32(35, 40, 60, 255));
    ImGui::PushStyleColor(ImGuiCol_SliderGrab,        ImGui::ColorConvertU32ToFloat4(fill_color));
    ImGui::PushStyleColor(ImGuiCol_SliderGrabActive,  ImGui::ColorConvertU32ToFloat4(fill_color));
    ImGui::PushStyleVar(ImGuiStyleVar_GrabRounding, 6.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);

    if (width > 0.0f) ImGui::SetNextItemWidth(width);
    bool changed = ImGui::SliderFloat(id, v, vmin, vmax, "%.0f");

    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(4);
    return changed;
}

void RenderAppWindow2(bool* p_open) {
    if (!p_open || !*p_open) return;

    ImGui::SetNextWindowSize(ImVec2(520, 440), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos(ImVec2(160, 120), ImGuiCond_FirstUseEver);

    ImGui::PushStyleColor(ImGuiCol_WindowBg,      IM_COL32(16, 18, 26, 252));
    ImGui::PushStyleColor(ImGuiCol_TitleBgActive,  IM_COL32(14, 40, 74, 255));
    ImGui::PushStyleColor(ImGuiCol_Tab,            IM_COL32(22, 25, 36, 255));
    ImGui::PushStyleColor(ImGuiCol_TabActive,      IM_COL32(18, 45, 80, 255));
    ImGui::PushStyleColor(ImGuiCol_TabHovered,     IM_COL32(28, 60, 105, 255));
    ImGui::PushStyleColor(ImGuiCol_Border,         IM_COL32(45, 52, 80, 255));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 6.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,  ImVec2(14, 10));

    bool open = ImGui::Begin("  System Settings", p_open,
                              ImGuiWindowFlags_NoCollapse);
    ImGui::PopStyleVar(2);

    if (!open) {
        ImGui::End();
        ImGui::PopStyleColor(6);
        return;
    }

    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 5.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,   ImVec2(8, 8));

    if (ImGui::BeginTabBar("SettingsTabs")) {

        // ── TAB 1: DISPLAY ───────────────────────────────────────────
        if (ImGui::BeginTabItem("  Display  ")) {
            ImGui::Spacing();

            // Section header helper lambda-style via macro
            auto SectionHeader = [](const char* label) {
                ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(96, 165, 250, 255));
                ImGui::TextUnformatted(label);
                ImGui::PopStyleColor();
                ImGui::Separator();
                ImGui::Spacing();
            };

            SectionHeader("Screen");

            // Resolution dropdown
            ImGui::Text("Resolution");
            ImGui::SameLine(160);
            ImGui::SetNextItemWidth(200);
            ImGui::PushStyleColor(ImGuiCol_FrameBg,        IM_COL32(28, 32, 48, 255));
            ImGui::PushStyleColor(ImGuiCol_PopupBg,        IM_COL32(22, 26, 40, 255));
            ImGui::PushStyleColor(ImGuiCol_Header,         IM_COL32(18, 45, 80, 255));
            ImGui::PushStyleColor(ImGuiCol_HeaderHovered,  IM_COL32(28, 60, 105, 255));
            ImGui::Combo("##res", &ss_resolution_idx, resolutions,
                         (int)(sizeof(resolutions)/sizeof(resolutions[0])));
            ImGui::PopStyleColor(4);

            ImGui::Spacing();

            // Brightness slider
            ImGui::Text("Brightness");
            ImGui::SameLine(160);
            ColoredSliderFloat("##bright", &ss_brightness, 0, 100,
                               IM_COL32(251, 191, 36, 255), 200);
            ImGui::SameLine();
            ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(251, 191, 36, 255));
            ImGui::Text("%.0f%%", ss_brightness);
            ImGui::PopStyleColor();

            // Contrast slider
            ImGui::Text("Contrast");
            ImGui::SameLine(160);
            ColoredSliderFloat("##contrast", &ss_contrast, 0, 100,
                               IM_COL32(167, 139, 250, 255), 200);
            ImGui::SameLine();
            ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(167, 139, 250, 255));
            ImGui::Text("%.0f%%", ss_contrast);
            ImGui::PopStyleColor();

            ImGui::Spacing();
            SectionHeader("Window");

            // VSync toggle
            ImGui::Text("V-Sync");
            ImGui::SameLine(160);
            ToggleSwitch("##vsync", &ss_vsync);
            ImGui::SameLine();
            ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(130, 138, 168, 255));
            ImGui::Text(ss_vsync ? "On" : "Off");
            ImGui::PopStyleColor();

            // Fullscreen toggle
            ImGui::Text("Fullscreen");
            ImGui::SameLine(160);
            ToggleSwitch("##fs", &ss_fullscreen);
            ImGui::SameLine();
            ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(130, 138, 168, 255));
            ImGui::Text(ss_fullscreen ? "On" : "Off");
            ImGui::PopStyleColor();

            ImGui::Spacing();
            SectionHeader("Accent Color");
            ImGui::Text("UI Accent");
            ImGui::SameLine(160);
            ImGui::PushStyleColor(ImGuiCol_FrameBg, IM_COL32(28, 32, 48, 255));
            if (ImGui::ColorEdit3("##accent", ss_accent,
                                  ImGuiColorEditFlags_NoInputs |
                                  ImGuiColorEditFlags_PickerHueBar)) {
                ss_accent_preview = ImVec4(ss_accent[0], ss_accent[1], ss_accent[2], 1.0f);
            }
            ImGui::PopStyleColor();

            ImGui::EndTabItem();
        }

        // ── TAB 2: SOUND ─────────────────────────────────────────────
        if (ImGui::BeginTabItem("  Sound  ")) {
            ImGui::Spacing();

            ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(96, 165, 250, 255));
            ImGui::TextUnformatted("Volume");
            ImGui::PopStyleColor();
            ImGui::Separator();
            ImGui::Spacing();

            // Mute toggle
            ImGui::Text("Mute All");
            ImGui::SameLine(160);
            ToggleSwitch("##mute", &ss_mute);
            ImGui::SameLine();
            ImGui::PushStyleColor(ImGuiCol_Text,
                ss_mute ? IM_COL32(248, 113, 113, 255) : IM_COL32(74, 222, 128, 255));
            ImGui::Text(ss_mute ? "Muted" : "Active");
            ImGui::PopStyleColor();

            ImGui::Spacing();

            // Volume sliders (greyed out when muted)
            ImU32 col_master = ss_mute ? IM_COL32(60,65,85,255) : IM_COL32(96,165,250,255);
            ImU32 col_music  = ss_mute ? IM_COL32(60,65,85,255) : IM_COL32(167,139,250,255);
            ImU32 col_sfx    = ss_mute ? IM_COL32(60,65,85,255) : IM_COL32(74,222,128,255);

            if (ss_mute) ImGui::BeginDisabled();

            ImGui::Text("Master");
            ImGui::SameLine(160);
            ColoredSliderFloat("##mvol", &ss_master_vol, 0, 100, col_master, 200);
            ImGui::SameLine();
            ImGui::PushStyleColor(ImGuiCol_Text, col_master);
            ImGui::Text("%.0f", ss_master_vol);
            ImGui::PopStyleColor();

            ImGui::Text("Music");
            ImGui::SameLine(160);
            ColoredSliderFloat("##musicvol", &ss_music_vol, 0, 100, col_music, 200);
            ImGui::SameLine();
            ImGui::PushStyleColor(ImGuiCol_Text, col_music);
            ImGui::Text("%.0f", ss_music_vol);
            ImGui::PopStyleColor();

            ImGui::Text("Effects");
            ImGui::SameLine(160);
            ColoredSliderFloat("##sfxvol", &ss_sfx_vol, 0, 100, col_sfx, 200);
            ImGui::SameLine();
            ImGui::PushStyleColor(ImGuiCol_Text, col_sfx);
            ImGui::Text("%.0f", ss_sfx_vol);
            ImGui::PopStyleColor();

            if (ss_mute) ImGui::EndDisabled();

            ImGui::Spacing();
            ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(96, 165, 250, 255));
            ImGui::TextUnformatted("Notifications");
            ImGui::PopStyleColor();
            ImGui::Separator();
            ImGui::Spacing();

            ImGui::Text("System Alerts");
            ImGui::SameLine(160);
            ToggleSwitch("##nsys", &ss_notify_sys);

            ImGui::Text("App Sounds");
            ImGui::SameLine(160);
            ToggleSwitch("##napp", &ss_notify_app);

            ImGui::Text("Alert Sound");
            ImGui::SameLine(160);
            ToggleSwitch("##nsnd", &ss_notify_sound);

            ImGui::Spacing();
            ImGui::Text("Alert Timeout");
            ImGui::SameLine(160);
            ColoredSliderFloat("##ntout", &ss_notify_timeout, 1.0f, 30.0f,
                               IM_COL32(251, 191, 36, 255), 180);
            ImGui::SameLine();
            ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(130, 138, 168, 255));
            ImGui::Text("%.0fs", ss_notify_timeout);
            ImGui::PopStyleColor();

            ImGui::EndTabItem();
        }

        // ── TAB 3: NETWORK ───────────────────────────────────────────
        if (ImGui::BeginTabItem("  Network  ")) {
            ImGui::Spacing();

            // Airplane mode row
            ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(96, 165, 250, 255));
            ImGui::TextUnformatted("Connectivity");
            ImGui::PopStyleColor();
            ImGui::Separator();
            ImGui::Spacing();

            ImGui::Text("Airplane Mode");
            ImGui::SameLine(160);
            ToggleSwitch("##airplane", &ss_airplane);
            ImGui::SameLine();
            ImGui::PushStyleColor(ImGuiCol_Text,
                ss_airplane ? IM_COL32(248,113,113,255) : IM_COL32(130,138,168,255));
            ImGui::Text(ss_airplane ? "On — radios off" : "Off");
            ImGui::PopStyleColor();

            ImGui::Spacing();

            ImGui::Text("Wi-Fi");
            ImGui::SameLine(160);
            ToggleSwitch("##wifi", &ss_wifi_enabled);
            ImGui::SameLine();
            ImGui::PushStyleColor(ImGuiCol_Text,
                ss_wifi_enabled ? IM_COL32(74,222,128,255) : IM_COL32(130,138,168,255));
            ImGui::Text(ss_wifi_enabled ? "Enabled" : "Disabled");
            ImGui::PopStyleColor();

            ImGui::Spacing();

            if (ss_airplane || !ss_wifi_enabled) {
                ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(100, 108, 140, 255));
                ImGui::TextUnformatted("  Wi-Fi is unavailable.");
                ImGui::PopStyleColor();
            } else {
                ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(96, 165, 250, 255));
                ImGui::TextUnformatted("Available Networks");
                ImGui::PopStyleColor();
                ImGui::Separator();
                ImGui::Spacing();

                ImGui::PushStyleColor(ImGuiCol_ChildBg,       IM_COL32(20, 22, 32, 200));
                ImGui::PushStyleColor(ImGuiCol_HeaderHovered,  IM_COL32(28, 55, 95, 255));
                ImGui::PushStyleColor(ImGuiCol_Header,         IM_COL32(18, 45, 80, 255));
                ImGui::BeginChild("##nets", ImVec2(-1, 200), false);

                ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 4));
                for (int i = 0; i < NETWORK_COUNT; i++) {
                    bool sel = (ss_wifi_idx == i);
                    char lbl[64];
                    snprintf(lbl, sizeof(lbl), "##net%d", i);

                    ImGui::SetCursorPosX(4);
                    if (ImGui::Selectable(lbl, sel,
                                          ImGuiSelectableFlags_SpanAvailWidth,
                                          ImVec2(0, 30)))
                        ss_wifi_idx = i;

                    // Overlay content on the selectable row
                    ImGui::SameLine();
                    ImGui::SetCursorPosX(10);
                    float row_top = ImGui::GetCursorPosY() - 28;
                    ImGui::SetCursorPosY(row_top + 9);

                    // Signal bars
                    DrawSignalBars(net_signal[i], 3.5f, 11.0f, 1.5f);
                    ImGui::SameLine(0, 8);

                    // Network name
                    ImGui::PushStyleColor(ImGuiCol_Text,
                        sel ? IM_COL32(147,197,253,255) : IM_COL32(200,208,230,255));
                    ImGui::TextUnformatted(networks[i]);
                    ImGui::PopStyleColor();

                    // Lock / connected indicator
                    ImGui::SameLine(340);
                    if (sel) {
                        ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(74, 222, 128, 255));
                        ImGui::TextUnformatted("Connected");
                        ImGui::PopStyleColor();
                    } else if (net_locked[i]) {
                        ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(130, 138, 168, 255));
                        ImGui::TextUnformatted("Secured");
                        ImGui::PopStyleColor();
                    } else {
                        ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(74, 222, 128, 200));
                        ImGui::TextUnformatted("Open");
                        ImGui::PopStyleColor();
                    }
                }
                ImGui::PopStyleVar();
                ImGui::EndChild();
                ImGui::PopStyleColor(3);
            }

            ImGui::EndTabItem();
        }

        // ── TAB 4: ABOUT ─────────────────────────────────────────────
        if (ImGui::BeginTabItem("  About  ")) {
            ImGui::Spacing();

            // Big OS name
            ImGui::SetCursorPosX((ImGui::GetContentRegionAvail().x -
                                  ImGui::CalcTextSize("CSOPESY OS").x) * 0.5f + 14);
            ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(147, 197, 253, 255));
            ImGui::TextUnformatted("CSOPESY OS");
            ImGui::PopStyleColor();

            ImGui::SetCursorPosX((ImGui::GetContentRegionAvail().x -
                                  ImGui::CalcTextSize("Desktop Emulator v1.0").x) * 0.5f + 14);
            ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(130, 138, 168, 255));
            ImGui::TextUnformatted("Desktop Emulator v1.0");
            ImGui::PopStyleColor();

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            auto InfoRow = [](const char* label, const char* value,
                              ImU32 val_col = IM_COL32(200,208,230,255)) {
                ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(100, 108, 140, 255));
                ImGui::Text("%-18s", label);
                ImGui::PopStyleColor();
                ImGui::SameLine(160);
                ImGui::PushStyleColor(ImGuiCol_Text, val_col);
                ImGui::TextUnformatted(value);
                ImGui::PopStyleColor();
            };

            InfoRow("OS Version",   "1.0.0 (Build 2026.06)");
            InfoRow("Kernel",       "CSOPESY Core 6.7");
            InfoRow("Architecture", "x86-64");
            InfoRow("CPU",          "Intel Core i7-12700H  (16 cores)");
            InfoRow("Memory",       "16384 MB DDR5");
            InfoRow("GPU",          "NVIDIA RTX 3060  (8 GB)");
            InfoRow("Display",      resolutions[ss_resolution_idx]);
            InfoRow("Uptime",       "2h 34m 17s");

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(100, 108, 140, 255));
            ImGui::TextUnformatted("Developed by:");
            ImGui::PopStyleColor();
            ImGui::Spacing();

            const char* members[] = {
                "Kelvin Alviar",
                "Angelo Calpoporo",
                "Miguel Carlos",
                "Nio Tujan",
            };
            for (const char* m : members) {
                ImGui::SetCursorPosX(20);
                ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(200, 208, 230, 255));
                ImGui::Text("  %s", m);
                ImGui::PopStyleColor();
            }

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();
            ImGui::SetCursorPosX((ImGui::GetContentRegionAvail().x -
                                  ImGui::CalcTextSize("CSOPESY 2026  |  De La Salle University").x) * 0.5f + 14);
            ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(70, 76, 105, 255));
            ImGui::TextUnformatted("CSOPESY 2026  |  De La Salle University");
            ImGui::PopStyleColor();

            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }

    ImGui::PopStyleVar(2);
    ImGui::End();
    ImGui::PopStyleColor(6);
}