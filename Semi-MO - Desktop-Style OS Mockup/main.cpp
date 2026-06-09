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



// Helper function to load an image file and convert it into an OpenGL Texture
bool LoadTextureFromFile(const char* filename, GLuint* out_texture, int* out_width, int* out_height) {
    int image_width = 0;
    int image_height = 0;

    // Load the image from the hard drive into RAM (4 = force RGBA channels)
    unsigned char* image_data = stbi_load(filename, &image_width, &image_height, NULL, 4);
    if (image_data == NULL) return false;

    // Generate a texture ID on the GPU
    GLuint image_texture;
    glGenTextures(1, &image_texture);
    glBindTexture(GL_TEXTURE_2D, image_texture);

    // Setup texture filtering (Makes it look smooth when resized)
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    // Upload the raw pixel data from RAM to the GPU
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, image_width, image_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, image_data);

    // Free the RAM memory (the GPU has it now)
    stbi_image_free(image_data);

    *out_texture = image_texture;
    *out_width = image_width;
    *out_height = image_height;

    return true;
}


int main() {
    // Initialize GLFW (The Window Manager)
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW!" << std::endl;
        return -1;
    }

    // Set OpenGL version (3.0 Core)
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);

    // 2. Create the Window
    GLFWwindow* window = glfwCreateWindow(1280, 720, "CSOPESY Desktop OS Emulator", NULL, NULL);
    if (!window) {
        std::cerr << "Failed to create GLFW window!" << std::endl;
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); // Enable VSync

    // 3. Initialize Dear ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    ImGui::StyleColorsDark(); // Set Dark Mode

    // Setup Platform/Renderer backends
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 130");

    // --- OS BOOT SEQUENCE INITIALIZATION ---
    GLuint bg_texture = 0;
    int bg_width = 0;
    int bg_height = 0;


    bool ret = LoadTextureFromFile("assets/background.jpg", &bg_texture, &bg_width, &bg_height);
    if (!ret) {
        std::cerr << "Failed to load wallpaper!" << std::endl;
    }

    // -We Define the States for the bootload sequence
    enum SystemState { BOOT_BIOS, BOOT_SPLASH, DESKTOP_UI };
    SystemState current_state = BOOT_BIOS;

    // -Start the Stopwatch
    auto boot_start_time = std::chrono::system_clock::now();

    // 4. THE MAIN RENDER LOOP (The "Compositor")
    while (!glfwWindowShouldClose(window)) {
        // Poll keyboard/mouse events
        glfwPollEvents();

        // Start the ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // -Update the Stopwatch
        auto current_time = std::chrono::system_clock::now();
        std::chrono::duration<float> elapsed = current_time - boot_start_time;
        float elapsed_seconds = elapsed.count();

        // -State Machine Logic
		// SWITCH 1.0 IF DOING TESTING 
        if (elapsed_seconds > 8.0f) { 
            current_state = DESKTOP_UI;
        }
        else if (elapsed_seconds > 4.5f) {
            current_state = BOOT_SPLASH;
        }

        // =====================================================================
        // OS UI CODE ROUTING GOES HERE INSIDE THE WHILE LOOP

        // =====================================================================
        // COMPONENT 1: THE DESKTOP BASE
        // =====================================================================

        if (current_state == BOOT_BIOS) {
            RenderBIOS(elapsed_seconds);
        }
        else if (current_state == BOOT_SPLASH) {
            RenderSplash(elapsed_seconds);
        }
        else if (current_state == DESKTOP_UI) {
            RenderDesktop(window, bg_texture);

            // Eventually, add here
            // RenderTaskbar();
            // RenderAppWindows();
        }
        // =====================================================================

        // Render everything
            ImGui::Render();
            int display_w, display_h;
            glfwGetFramebufferSize(window, &display_w, &display_h);
            glViewport(0, 0, display_w, display_h);

            // Background color (Desktop Wallpaper)
            glClearColor(0.0f, 0.0f, 0.0f, 1.00f);
            glClear(GL_COLOR_BUFFER_BIT);

            // Draw ImGui over the background
            ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

            // Swap buffers (Double Buffering)
            glfwSwapBuffers(window);
        }

        // 5. Cleanup & Shutdown
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        glfwDestroyWindow(window);
        glfwTerminate();

        return 0;
    }