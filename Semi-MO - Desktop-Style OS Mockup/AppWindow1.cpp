#include "AppWindow1.h"
#include <cstring>

// ============================================================
// COMPONENT: FILE EXPLORER (App Window 1)
// A mockup file explorer with sidebar navigation and file list
// ============================================================

struct FileEntry {
    const char* name;
    const char* type;
    const char* size;
    const char* modified;
    bool        is_folder;
};

// Sidebar locations
static const char* sidebar_items[] = {
    "  Desktop",
    "  Documents",
    "  Downloads",
    "  Pictures",
    "  Music",
    "  Videos",
    "  Recycle Bin",
};
static const int SIDEBAR_COUNT = (int)(sizeof(sidebar_items) / sizeof(sidebar_items[0]));

// File entries per sidebar location (all dummy data)
static FileEntry desktop_files[] = {
    { "My Project",       "Folder",    "--",       "Jun 10, 2026", true  },
    { "notes.txt",        "Text File", "4 KB",     "Jun 9, 2026",  false },
    { "background.jpg",   "Image",     "2.1 MB",   "Jun 8, 2026",  false },
    { "resume_final.pdf", "PDF",       "312 KB",   "Jun 7, 2026",  false },
    { "Shortcut.lnk",     "Shortcut",  "1 KB",     "Jun 6, 2026",  false },
};

static FileEntry documents_files[] = {
    { "CSOPESY",          "Folder",    "--",       "Jun 10, 2026", true  },
    { "Lab Reports",      "Folder",    "--",       "Jun 5, 2026",  true  },
    { "thesis_draft.docx","Document",  "1.8 MB",   "Jun 9, 2026",  false },
    { "budget2026.xlsx",  "Spreadsheet","88 KB",   "Jun 3, 2026",  false },
    { "lecture_notes.pdf","PDF",       "540 KB",   "Jun 1, 2026",  false },
};

static FileEntry downloads_files[] = {
    { "vcpkg-master.zip", "ZIP",       "24.6 MB",  "Jun 8, 2026",  false },
    { "clion-installer.exe","Application","512 MB","Jun 7, 2026",  false },
    { "imgui-v1.91",      "Folder",    "--",       "Jun 6, 2026",  true  },
    { "wallpaper_pack.zip","ZIP",      "78 MB",    "Jun 5, 2026",  false },
};

static FileEntry pictures_files[] = {
    { "Screenshots",      "Folder",    "--",       "Jun 10, 2026", true  },
    { "profile_pic.png",  "Image",     "3.4 MB",   "Jun 4, 2026",  false },
    { "meme_stash",       "Folder",    "--",       "May 30, 2026", true  },
    { "vacation2025.jpg", "Image",     "5.1 MB",   "Dec 25, 2025", false },
};

static FileEntry music_files[] = {
    { "Lofi Study",       "Folder",    "--",       "Jun 1, 2026",  true  },
    { "playlist.m3u",     "Playlist",  "8 KB",     "May 28, 2026", false },
    { "track01.mp3",      "Audio",     "7.2 MB",   "May 20, 2026", false },
    { "track02.flac",     "Audio",     "32.1 MB",  "May 20, 2026", false },
};

static FileEntry videos_files[] = {
    { "Demo_Walkthrough.mp4","Video",  "680 MB",   "Jun 9, 2026",  false },
    { "lecture_rec.mp4",  "Video",     "1.2 GB",   "Jun 5, 2026",  false },
    { "clips",            "Folder",    "--",       "Jun 3, 2026",  true  },
};

static FileEntry recycle_files[] = {
    { "old_main.cpp",     "C++ Source","12 KB",    "Jun 8, 2026",  false },
    { "draft_v1.docx",    "Document",  "95 KB",    "Jun 7, 2026",  false },
};

struct LocationData { FileEntry* entries; int count; const char* path; };
static LocationData locations[] = {
    { desktop_files,   5, "C:\\Users\\User\\Desktop"   },
    { documents_files, 5, "C:\\Users\\User\\Documents" },
    { downloads_files, 4, "C:\\Users\\User\\Downloads" },
    { pictures_files,  4, "C:\\Users\\User\\Pictures"  },
    { music_files,     4, "C:\\Users\\User\\Music"     },
    { videos_files,    3, "C:\\Users\\User\\Videos"    },
    { recycle_files,   2, "C:\\$Recycle.Bin"           },
};

static int  fe_selected_location = 0;
static int  fe_selected_file     = -1;
static char fe_search_buf[128]   = "";

// Folder / file icon glyphs (Font Awesome codepoints)
#define ICON_FOLDER  "\xef\x81\xbb"   // fa-folder  U+F07B
#define ICON_FILE    "\xef\x85\x9b"   // fa-file    U+F15B
#define ICON_IMG     "\xef\x80\xbe"   // fa-image   U+F03E
#define ICON_PDF     "\xef\x85\x9c"   // fa-file-pdf U+F1C1
#define ICON_ZIP     "\xef\x85\xa1"   // fa-file-archive U+F1C6
#define ICON_AUDIO   "\xef\x80\x81"   // fa-music   U+F001
#define ICON_VIDEO   "\xef\x81\x9d"   // fa-film    U+F008
#define ICON_DOC     "\xef\x85\x9d"   // fa-file-word U+F1C2
#define ICON_XLS     "\xef\x85\x9e"   // fa-file-excel U+F1C3
#define ICON_EXE     "\xef\x84\xa0"   // fa-cog     U+F120
#define ICON_LINK    "\xef\x81\x99"   // fa-external-link U+F059

static const char* FileIcon(const FileEntry& f) {
    if (f.is_folder)                         return ICON_FOLDER;
    const char* t = f.type;
    if (strstr(t, "Image"))                  return ICON_IMG;
    if (strstr(t, "PDF"))                    return ICON_PDF;
    if (strstr(t, "ZIP"))                    return ICON_ZIP;
    if (strstr(t, "Audio"))                  return ICON_AUDIO;
    if (strstr(t, "Video"))                  return ICON_VIDEO;
    if (strstr(t, "Document"))               return ICON_DOC;
    if (strstr(t, "Spreadsheet"))            return ICON_XLS;
    if (strstr(t, "Application"))            return ICON_EXE;
    if (strstr(t, "Shortcut"))               return ICON_LINK;
    return ICON_FILE;
}

void RenderAppWindow1(bool* p_open) {
    if (!p_open || !*p_open) return;

    ImGui::SetNextWindowSize(ImVec2(720, 480), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos(ImVec2(80, 80), ImGuiCond_FirstUseEver);

    // Dark navy window chrome
    ImGui::PushStyleColor(ImGuiCol_WindowBg,      IM_COL32(16, 18, 26, 252));
    ImGui::PushStyleColor(ImGuiCol_TitleBgActive,  IM_COL32(14, 40, 74, 255));
    ImGui::PushStyleColor(ImGuiCol_Border,         IM_COL32(45, 52, 80, 255));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 6.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,  ImVec2(0, 0));

    bool open = ImGui::Begin("  File Explorer", p_open,
                             ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar);
    ImGui::PopStyleVar(2);

    if (!open) {
        ImGui::End();
        ImGui::PopStyleColor(3);
        return;
    }

    // ── Address / search bar ──────────────────────────────────────────
    const LocationData& loc = locations[fe_selected_location];

    ImGui::PushStyleColor(ImGuiCol_ChildBg, IM_COL32(22, 25, 36, 255));
    ImGui::BeginChild("##topbar", ImVec2(-1, 38), false, ImGuiWindowFlags_NoScrollbar);

    ImGui::SetCursorPos(ImVec2(8, 7));
    ImGui::PushStyleColor(ImGuiCol_Text,        IM_COL32(160, 170, 200, 255));
    ImGui::PushStyleColor(ImGuiCol_FrameBg,     IM_COL32(30, 34, 50, 255));
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, IM_COL32(36, 40, 60, 255));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);

    // Path display (read-only label styled as a box)
    char path_display[256];
    snprintf(path_display, sizeof(path_display), "  %s", loc.path);
    ImGui::PushItemWidth(350);
    ImGui::InputText("##path", path_display, sizeof(path_display),
                     ImGuiInputTextFlags_ReadOnly);
    ImGui::PopItemWidth();

    ImGui::SameLine(0, 8);

    // Search box
    ImGui::PushItemWidth(160);
    ImGui::InputTextWithHint("##search", "  Search...", fe_search_buf, sizeof(fe_search_buf));
    ImGui::PopItemWidth();

    // Clear search button
    ImGui::SameLine(0, 4);
    if (fe_search_buf[0] != '\0') {
        ImGui::PushStyleColor(ImGuiCol_Button,        IM_COL32(60, 20, 20, 200));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(90, 30, 30, 255));
        ImGui::PushStyleColor(ImGuiCol_Text,          IM_COL32(248, 113, 113, 255));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
        if (ImGui::Button("x##clr", ImVec2(22, 22)))
            fe_search_buf[0] = '\0';
        ImGui::PopStyleVar();
        ImGui::PopStyleColor(3);
    }

    ImGui::PopStyleVar();
    ImGui::PopStyleColor(3);
    ImGui::EndChild();
    ImGui::PopStyleColor(); // ChildBg topbar

    // ── Main body: sidebar + file list ──────────────────────────────
    float sidebar_w = 150.0f;
    float body_h    = ImGui::GetContentRegionAvail().y;

    // Sidebar
    ImGui::PushStyleColor(ImGuiCol_ChildBg,      IM_COL32(20, 22, 32, 255));
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, IM_COL32(35, 55, 90, 255));
    ImGui::PushStyleColor(ImGuiCol_HeaderActive,  IM_COL32(20, 40, 75, 255));
    ImGui::PushStyleColor(ImGuiCol_Header,        IM_COL32(18, 45, 80, 255));

    ImGui::BeginChild("##sidebar", ImVec2(sidebar_w, body_h), false);
    ImGui::SetCursorPosY(8);

    ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(100, 108, 140, 255));
    ImGui::SetCursorPosX(10);
    ImGui::TextUnformatted("QUICK ACCESS");
    ImGui::PopStyleColor();
    ImGui::Spacing();

    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 2));
    for (int i = 0; i < SIDEBAR_COUNT; i++) {
        ImGui::SetCursorPosX(0);
        bool selected = (i == fe_selected_location);

        // Highlight color for selected
        if (selected)
            ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(147, 197, 253, 255));
        else
            ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(180, 188, 210, 255));

        char label[64];
        snprintf(label, sizeof(label), "  %s##sb%d", sidebar_items[i], i);
        if (ImGui::Selectable(label, selected,
                              ImGuiSelectableFlags_SpanAvailWidth,
                              ImVec2(0, 26))) {
            fe_selected_location = i;
            fe_selected_file     = -1;
            fe_search_buf[0]     = '\0';
        }
        ImGui::PopStyleColor();
    }
    ImGui::PopStyleVar();
    ImGui::EndChild();
    ImGui::PopStyleColor(4);

    // Vertical divider line
    ImVec2 div_p = ImGui::GetItemRectMin();
    div_p.x += sidebar_w;
    ImGui::GetWindowDrawList()->AddLine(
        ImVec2(div_p.x + sidebar_w, ImGui::GetWindowPos().y + 38),
        ImVec2(div_p.x + sidebar_w, ImGui::GetWindowPos().y + 38 + body_h),
        IM_COL32(38, 44, 65, 255), 1.0f
    );

    ImGui::SameLine(0, 0);

    // File list pane
    ImGui::PushStyleColor(ImGuiCol_ChildBg,       IM_COL32(16, 18, 26, 255));
    ImGui::PushStyleColor(ImGuiCol_TableHeaderBg,  IM_COL32(22, 26, 40, 255));
    ImGui::PushStyleColor(ImGuiCol_TableRowBg,     IM_COL32(18, 20, 30, 255));
    ImGui::PushStyleColor(ImGuiCol_TableRowBgAlt,  IM_COL32(22, 25, 36, 255));
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered,  IM_COL32(35, 55, 90, 255));
    ImGui::PushStyleColor(ImGuiCol_Header,         IM_COL32(18, 45, 80, 255));

    ImGui::BeginChild("##filelist", ImVec2(-1, body_h - 28), false);

    ImGuiTableFlags tflags =
        ImGuiTableFlags_Borders        |
        ImGuiTableFlags_RowBg          |
        ImGuiTableFlags_ScrollY        |
        ImGuiTableFlags_SizingFixedFit |
        ImGuiTableFlags_Resizable;

    if (ImGui::BeginTable("FileTable", 4, tflags,
                          ImVec2(0, ImGui::GetContentRegionAvail().y)))
    {
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableSetupColumn("Name",      ImGuiTableColumnFlags_WidthStretch, 220.0f);
        ImGui::TableSetupColumn("Type",      ImGuiTableColumnFlags_WidthFixed,    90.0f);
        ImGui::TableSetupColumn("Size",      ImGuiTableColumnFlags_WidthFixed,    72.0f);
        ImGui::TableSetupColumn("Modified",  ImGuiTableColumnFlags_WidthFixed,   110.0f);
        ImGui::TableHeadersRow();

        const LocationData& cur = locations[fe_selected_location];
        for (int i = 0; i < cur.count; i++) {
            const FileEntry& f = cur.entries[i];

            // Apply search filter
            if (fe_search_buf[0] != '\0') {
                // Simple case-insensitive substring check
                bool match = false;
                const char* h = f.name;
                const char* n = fe_search_buf;
                // basic search: scan for needle in haystack (case-aware enough)
                int nlen = (int)strlen(n);
                for (int k = 0; h[k]; k++) {
                    bool eq = true;
                    for (int j = 0; j < nlen; j++) {
                        char hc = h[k+j], nc = n[j];
                        // tolower
                        if (hc >= 'A' && hc <= 'Z') hc += 32;
                        if (nc >= 'A' && nc <= 'Z') nc += 32;
                        if (hc != nc) { eq = false; break; }
                    }
                    if (eq) { match = true; break; }
                }
                if (!match) continue;
            }

            ImGui::TableNextRow();
            bool row_selected = (fe_selected_file == i);

            // Name column with icon
            ImGui::TableSetColumnIndex(0);
            ImU32 name_col = f.is_folder
                ? IM_COL32(253, 196, 78, 255)
                : IM_COL32(200, 208, 230, 255);
            ImGui::PushStyleColor(ImGuiCol_Text, name_col);

            char row_label[256];
            snprintf(row_label, sizeof(row_label), " %s  %s##row%d",
                     FileIcon(f), f.name, i);
            if (ImGui::Selectable(row_label, row_selected,
                                  ImGuiSelectableFlags_SpanAllColumns,
                                  ImVec2(0, 0)))
                fe_selected_file = (row_selected ? -1 : i);
            ImGui::PopStyleColor();

            // Type
            ImGui::TableSetColumnIndex(1);
            ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(130, 138, 168, 255));
            ImGui::TextUnformatted(f.type);
            ImGui::PopStyleColor();

            // Size
            ImGui::TableSetColumnIndex(2);
            ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(130, 138, 168, 255));
            ImGui::TextUnformatted(f.size);
            ImGui::PopStyleColor();

            // Modified
            ImGui::TableSetColumnIndex(3);
            ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(130, 138, 168, 255));
            ImGui::TextUnformatted(f.modified);
            ImGui::PopStyleColor();
        }
        ImGui::EndTable();
    }
    ImGui::EndChild();
    ImGui::PopStyleColor(6);

    // ── Status bar ───────────────────────────────────────────────────
    ImGui::PushStyleColor(ImGuiCol_ChildBg, IM_COL32(20, 22, 34, 255));
    ImGui::BeginChild("##statusbar", ImVec2(-1, 24), false, ImGuiWindowFlags_NoScrollbar);
    ImGui::SetCursorPos(ImVec2(10, 4));
    ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(100, 108, 140, 255));
    if (fe_selected_file >= 0 && fe_selected_file < locations[fe_selected_location].count) {
        const FileEntry& sel = locations[fe_selected_location].entries[fe_selected_file];
        ImGui::Text("  Selected: %s  |  %s  |  %s", sel.name, sel.type, sel.size);
    } else {
        ImGui::Text("  %d item(s)  |  %s",
                    locations[fe_selected_location].count,
                    locations[fe_selected_location].path);
    }
    ImGui::PopStyleColor();
    ImGui::EndChild();
    ImGui::PopStyleColor(); // statusbar ChildBg

    ImGui::End();
    ImGui::PopStyleColor(3);
}