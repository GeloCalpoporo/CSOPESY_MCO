#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include <iostream>
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <GLFW/glfw3.h>
#include <chrono>

// Components Here
#include "Bootloader.h"
#include "Desktop.h"
#include "Taskbar.h"

// Helper: load image file → OpenGL texture
bool LoadTextureFromFile(const char* filename, GLuint* out_texture, int* out_width, int* out_height) {
    int w = 0, h = 0;
    unsigned char* data = stbi_load(filename, &w, &h, NULL, 4);
    if (data == NULL) return false;

    GLuint tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
    stbi_image_free(data);

    *out_texture = tex;
    *out_width   = w;
    *out_height  = h;
    return true;
}

int main() {
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW!" << std::endl;
        return -1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);

    GLFWwindow* window = glfwCreateWindow(1280, 720, "CSOPESY Desktop OS Emulator", NULL, NULL);
    if (!window) {
        std::cerr << "Failed to create GLFW window!" << std::endl;
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    ImGui::StyleColorsDark();

    // Font Awesome icon font (for START/STOP/SRCH/VOL/NET/PWR buttons)
    ImFontConfig default_cfg;
    default_cfg.SizePixels = 14.0f;
    io.Fonts->AddFontDefault(&default_cfg);

    static const ImWchar fa_ranges[] = { 0xe000, 0xf8ff, 0 };
    ImFontConfig fa_cfg;
    fa_cfg.MergeMode        = true;
    fa_cfg.PixelSnapH       = true;
    fa_cfg.GlyphMinAdvanceX = 14.0f;
    io.Fonts->AddFontFromFileTTF("assets/fa-solid-900.ttf", 14.0f, &fa_cfg, fa_ranges);
    // No Build() needed — backend handles it automatically

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 130");

    // Load wallpaper
    GLuint bg_texture = 0;
    int bg_w = 0, bg_h = 0;
    if (!LoadTextureFromFile("assets/background.jpg", &bg_texture, &bg_w, &bg_h))
        std::cerr << "Failed to load wallpaper!" << std::endl;

    // Load taskbar PNG icons
    TaskbarIcons icons;
    int dummy_w, dummy_h;
    if (!LoadTextureFromFile("assets/icon_files.png",   &icons.files,   &dummy_w, &dummy_h))
        std::cerr << "Failed to load icon_files.png" << std::endl;
    if (!LoadTextureFromFile("assets/icon_init.png",    &icons.init,    &dummy_w, &dummy_h))
        std::cerr << "Failed to load icon_init.png" << std::endl;
    if (!LoadTextureFromFile("assets/icon_taskmgr.png", &icons.taskmgr, &dummy_w, &dummy_h))
        std::cerr << "Failed to load icon_taskmgr.png" << std::endl;

    // Boot sequence state machine
    enum SystemState { BOOT_BIOS, BOOT_SPLASH, DESKTOP_UI };
    SystemState current_state = BOOT_BIOS;
    auto boot_start_time = std::chrono::system_clock::now();

    AppState app_state;

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        auto now = std::chrono::system_clock::now();
        float elapsed = std::chrono::duration<float>(now - boot_start_time).count();

        // SWITCH TO 1.0f FOR FAST TESTING
        if      (elapsed > 8.0f)  current_state = DESKTOP_UI;
        else if (elapsed > 4.5f)  current_state = BOOT_SPLASH;

        if (current_state == BOOT_BIOS) {
            RenderBIOS(elapsed);
        }
        else if (current_state == BOOT_SPLASH) {
            RenderSplash(elapsed);
        }
        else if (current_state == DESKTOP_UI) {
            RenderDesktop(window, bg_texture);
            RenderTaskbar(app_state, window, icons);

            if (app_state.show_task_manager)
                RenderTaskManager(&app_state.show_task_manager);

            // if (app_state.show_app1) RenderAppWindow1(&app_state.show_app1);
            // if (app_state.show_app2) RenderAppWindow2(&app_state.show_app2);
        }

        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window);
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}